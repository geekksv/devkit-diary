#!/usr/bin/env python3
"""devkit-diary, chapter 13: one clipboard for every device. The helper for a computer.

Copy something here and it appears on your other computers' clipboards a moment later; copy
there and it appears here. Everything is sealed with your passphrase before it leaves this
computer (clipcrypto.py, next to this file): the board in the middle only ever holds sealed bytes.

    devkit_clip.py            run in the background (the installer starts it at login)
    devkit_clip.py --setup    join your clipboard group (asks for a name and the passphrase)
    devkit_clip.py --pick     the quick picker: your history, search, Enter to paste
    devkit_clip.py --status   what it is doing
    devkit_clip.py --pause / --resume
    devkit_clip.py --leave    leave the group and forget everything on this computer

Standard library only, Python 3.8+. It never listens on a port: it asks the board.
"""

import base64
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import clipcrypto as cc  # noqa: E402

SYSTEM = platform.system()
if SYSTEM == "Windows":
    HOME = os.path.join(os.environ.get("LOCALAPPDATA", os.path.expanduser("~")), "devkit-clip")
else:
    HOME = os.path.expanduser("~/.devkit-clip")
CONFIG = os.path.join(HOME, "config.json")
LOG = os.path.join(HOME, "clip.log")
T_TEXT, T_IMAGE = 0, 1                     # first byte of every sealed plaintext
F_PIN, F_ONCE, F_IMAGE, F_OPEN = 1, 2, 4, 8
MAX_TEXT = 300_000                          # characters; a clipboard bigger than this stays here
MAX_IMAGE = 300_000                         # bytes of PNG


def log(*a):
    line = time.strftime("%H:%M:%S ") + " ".join(str(x) for x in a)
    try:
        os.makedirs(HOME, exist_ok=True)
        with open(LOG, "a", encoding="utf-8") as f:
            f.write(line + "\n")
        if os.path.getsize(LOG) > 500_000:
            os.replace(LOG, LOG + ".old")
    except OSError:
        pass
    if sys.stdout and sys.stdout.isatty():
        print(line)


# ── settings on this computer ────────────────────────────────────────────────────────────────

def load_config():
    try:
        with open(CONFIG, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def save_config(cfg):
    os.makedirs(HOME, exist_ok=True)
    tmp = CONFIG + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=1)
    try:
        os.chmod(tmp, 0o600)                 # the keys are in here; only this user may read them
    except OSError:
        pass
    os.replace(tmp, CONFIG)


def board_hosts(cfg):
    if os.environ.get("DEVKIT_CLIP_HOSTS"):            # for trying it against tools/devserver.cpp
        return os.environ["DEVKIT_CLIP_HOSTS"].split()
    hosts = cfg.get("hosts")
    if hosts:
        return hosts
    try:
        with open(os.path.join(HOME, "board"), encoding="utf-8") as f:
            return f.read().split()
    except OSError:
        return ["clip.local"]


# ── talking to the board ─────────────────────────────────────────────────────────────────────

class Board:
    """The board over plain http: what travels is sealed already, and https would cost the board
    ~37 KB of memory per connection. One request at a time."""

    def __init__(self, hosts):
        self.hosts = list(hosts)
        self.lock = threading.Lock()

    def _url(self, host, path, query):
        q = ("?" + urllib.parse.urlencode(query)) if query else ""
        return "http://%s%s%s" % (host, path, q)

    def call(self, path, query=None, body=None, raw=False, timeout=10):
        with self.lock:
            last = None
            for i, host in enumerate(self.hosts):
                try:
                    req = urllib.request.Request(self._url(host, path, query), data=body, method="POST" if body is not None else "GET")
                    if body is not None:
                        req.add_header("Content-Type", "application/octet-stream")
                    with urllib.request.urlopen(req, timeout=timeout) as r:
                        data = r.read()
                    if i:                                     # this address worked: try it first next time
                        self.hosts.insert(0, self.hosts.pop(i))
                    return data if raw else json.loads(data.decode("utf-8"))
                except (urllib.error.URLError, OSError, ValueError) as e:
                    last = e
            raise ConnectionError("can't reach the board (%s)" % last)


def keys_from(cfg):
    return cc.Keys(bytes.fromhex(cfg["enc"]), bytes.fromhex(cfg["mac"]), bytes.fromhex(cfg["auth"]))


def join_group(board, name, passphrase, create):
    """Sets up the group (first device) or joins it. Returns the settings to keep."""
    hello = board.call("/hello")
    salt = bytes.fromhex(hello["salt"])
    keys = cc.derive(passphrase, salt)
    if not hello["set"]:
        if not create:
            raise ValueError("nogroup")
        r = board.call("/setup", {"salt": salt.hex(), "auth": keys.auth.hex()})
        if not r.get("ok"):
            raise ValueError(r.get("err", "setup failed"))
    ch = board.call("/challenge")
    r = board.call("/join", {"n": ch["n"], "a": cc.answer(keys, bytes.fromhex(ch["n"])), "name": name, "kind": 0})
    if not r.get("ok"):
        raise ValueError(r.get("err", "join failed"))
    return {"hosts": board.hosts, "t": r["t"], "id": r["id"], "name": name, "salt": salt.hex(), "epoch": hello["epoch"],
            "enc": keys.enc.hex(), "mac": keys.mac.hex(), "auth": keys.auth.hex()}


# ── the clipboard on each system ─────────────────────────────────────────────────────────────

class Clipboard:
    """change_token(): something that changes when the clipboard does (cheap, called often).
    read(): (kind, bytes) or None; None also for items a password manager marked private.
    write(kind, bytes)."""

    def change_token(self):
        r = self.read()
        return hashlib.sha256(r[1]).hexdigest() if r else None


class MacClipboard(Clipboard):
    ENV = dict(os.environ, LANG="en_US.UTF-8", LC_ALL="en_US.UTF-8")
    # Password managers mark what they copy as concealed or transient (nspasteboard.org).
    JXA = ("ObjC.import('AppKit');var p=$.NSPasteboard.generalPasteboard;var t=ObjC.deepUnwrap(p.types)||[];"
           "JSON.stringify({c:p.changeCount,private:t.indexOf('org.nspasteboard.ConcealedType')>=0||"
           "t.indexOf('org.nspasteboard.TransientType')>=0,png:t.indexOf('public.png')>=0,text:t.indexOf('public.utf8-plain-text')>=0})")

    def _info(self):
        out = subprocess.run(["osascript", "-l", "JavaScript", "-e", self.JXA], capture_output=True, timeout=5).stdout
        return json.loads(out.decode() or "{}")

    def change_token(self):
        i = self._info()
        return str(i.get("c"))

    def read(self):
        i = self._info()
        if i.get("private"):
            return None
        if i.get("text"):
            t = subprocess.run(["pbpaste"], capture_output=True, env=self.ENV, timeout=5).stdout
            return (T_TEXT, t) if t else None
        if i.get("png"):
            js = ("ObjC.import('AppKit');var d=$.NSPasteboard.generalPasteboard.dataForType('public.png');"
                  "d.isNil()?'':ObjC.unwrap(d.base64EncodedStringWithOptions(0))")
            b = subprocess.run(["osascript", "-l", "JavaScript", "-e", js], capture_output=True, timeout=10).stdout.strip()
            return (T_IMAGE, base64.b64decode(b)) if b else None
        return None

    def write(self, kind, data):
        if kind == T_TEXT:
            subprocess.run(["pbcopy"], input=data, env=self.ENV, timeout=5)
        else:
            js = ("ObjC.import('AppKit');var p=$.NSPasteboard.generalPasteboard;p.clearContents;"
                  "var d=$.NSData.alloc.initWithBase64EncodedStringOptions('%s',0);p.setDataForType(d,'public.png')"
                  % base64.b64encode(data).decode())
            subprocess.run(["osascript", "-l", "JavaScript", "-e", js], capture_output=True, timeout=10)


class LinuxClipboard(Clipboard):
    def __init__(self):
        self.wayland = bool(os.environ.get("WAYLAND_DISPLAY")) and shutil.which("wl-paste")
        if not self.wayland and not shutil.which("xclip"):
            raise RuntimeError("install wl-clipboard (Wayland) or xclip (X11) for the clipboard")

    def _types(self):
        cmd = ["wl-paste", "--list-types"] if self.wayland else ["xclip", "-selection", "clipboard", "-t", "TARGETS", "-o"]
        return subprocess.run(cmd, capture_output=True, timeout=5).stdout.decode(errors="replace").split()

    def _get(self, mime):
        cmd = ["wl-paste", "--no-newline", "--type", mime] if self.wayland else ["xclip", "-selection", "clipboard", "-t", mime, "-o"]
        return subprocess.run(cmd, capture_output=True, timeout=5).stdout

    def read(self):
        types = self._types()
        if "x-kde-passwordManagerHint" in types:
            return None
        for m in ("text/plain;charset=utf-8", "UTF8_STRING", "text/plain"):
            if m in types:
                t = self._get(m)
                return (T_TEXT, t) if t else None
        if "image/png" in types:
            return (T_IMAGE, self._get("image/png"))
        return None

    def write(self, kind, data):
        mime = "text/plain;charset=utf-8" if kind == T_TEXT else "image/png"
        if self.wayland:
            subprocess.Popen(["wl-copy", "--type", mime], stdin=subprocess.PIPE).communicate(data, timeout=5)
        else:
            subprocess.Popen(["xclip", "-selection", "clipboard", "-t", "UTF8_STRING" if kind == T_TEXT else mime, "-i"],
                             stdin=subprocess.PIPE).communicate(data, timeout=5)


class WindowsClipboard(Clipboard):
    def __init__(self):
        import ctypes
        from ctypes import wintypes as w
        self.ct = ctypes
        u, k = ctypes.WinDLL("user32", use_last_error=True), ctypes.WinDLL("kernel32", use_last_error=True)
        u.OpenClipboard.argtypes = [w.HWND]; u.OpenClipboard.restype = w.BOOL
        u.CloseClipboard.restype = w.BOOL
        u.EmptyClipboard.restype = w.BOOL
        u.GetClipboardData.argtypes = [w.UINT]; u.GetClipboardData.restype = w.HANDLE
        u.SetClipboardData.argtypes = [w.UINT, w.HANDLE]; u.SetClipboardData.restype = w.HANDLE
        u.IsClipboardFormatAvailable.argtypes = [w.UINT]; u.IsClipboardFormatAvailable.restype = w.BOOL
        u.GetClipboardSequenceNumber.restype = w.DWORD
        u.RegisterClipboardFormatW.argtypes = [w.LPCWSTR]; u.RegisterClipboardFormatW.restype = w.UINT
        k.GlobalAlloc.argtypes = [w.UINT, ctypes.c_size_t]; k.GlobalAlloc.restype = w.HGLOBAL
        k.GlobalLock.argtypes = [w.HGLOBAL]; k.GlobalLock.restype = w.LPVOID
        k.GlobalUnlock.argtypes = [w.HGLOBAL]; k.GlobalUnlock.restype = w.BOOL
        k.GlobalSize.argtypes = [w.HGLOBAL]; k.GlobalSize.restype = ctypes.c_size_t
        k.GlobalFree.argtypes = [w.HGLOBAL]; k.GlobalFree.restype = w.HGLOBAL
        self.u, self.k = u, k
        self.CF_UNICODETEXT = 13
        self.CF_PNG = u.RegisterClipboardFormatW("PNG")
        # How password managers (and Windows' own history) say "don't keep this".
        self.CF_EXCLUDE = u.RegisterClipboardFormatW("ExcludeClipboardContentFromMonitorProcessing")
        self.CF_NOHISTORY = u.RegisterClipboardFormatW("CanIncludeInClipboardHistory")
        self.CF_NOCLOUD = u.RegisterClipboardFormatW("CanUploadToCloudClipboard")

    def change_token(self):
        return str(self.u.GetClipboardSequenceNumber())

    def _open(self):
        for _ in range(20):                       # another program may hold it for a moment
            if self.u.OpenClipboard(None):
                return True
            time.sleep(0.05)
        return False

    def _bytes(self, fmt):
        h = self.u.GetClipboardData(fmt)
        if not h:
            return None
        p = self.k.GlobalLock(h)
        if not p:
            return None
        try:
            return self.ct.string_at(p, self.k.GlobalSize(h))
        finally:
            self.k.GlobalUnlock(h)

    def _dword_zero(self, fmt):
        b = self._bytes(fmt) if self.u.IsClipboardFormatAvailable(fmt) else None
        return b is not None and len(b) >= 4 and int.from_bytes(b[:4], "little") == 0

    def read(self):
        if not self._open():
            return None
        try:
            if self.u.IsClipboardFormatAvailable(self.CF_EXCLUDE) or self._dword_zero(self.CF_NOHISTORY) or self._dword_zero(self.CF_NOCLOUD):
                return None
            if self.u.IsClipboardFormatAvailable(self.CF_UNICODETEXT):
                b = self._bytes(self.CF_UNICODETEXT)
                if b is None:
                    return None
                t = b.decode("utf-16-le", errors="replace").split("\x00", 1)[0]
                return (T_TEXT, t.replace("\r\n", "\n").encode("utf-8")) if t else None
            if self.u.IsClipboardFormatAvailable(self.CF_PNG):
                b = self._bytes(self.CF_PNG)
                return (T_IMAGE, b) if b else None
            return None
        finally:
            self.u.CloseClipboard()

    def _put(self, fmt, data):
        h = self.k.GlobalAlloc(0x0002, len(data))          # GMEM_MOVEABLE
        p = self.k.GlobalLock(h)
        self.ct.memmove(p, data, len(data))
        self.k.GlobalUnlock(h)
        if not self.u.SetClipboardData(fmt, h):
            self.k.GlobalFree(h)

    def write(self, kind, data):
        if not self._open():
            raise OSError("the clipboard is busy")
        try:
            self.u.EmptyClipboard()
            if kind == T_TEXT:
                t = data.decode("utf-8", errors="replace").replace("\r\n", "\n").replace("\n", "\r\n")
                self._put(self.CF_UNICODETEXT, (t + "\x00").encode("utf-16-le"))
            else:
                self._put(self.CF_PNG, data)               # browsers, Office and most editors take PNG
        finally:
            self.u.CloseClipboard()


class FakeClipboard(Clipboard):
    """A file standing in for the clipboard: for the tests (DEVKIT_CLIP_FAKE=path)."""

    def __init__(self, path):
        self.path = path

    def change_token(self):
        try:
            return str(os.stat(self.path).st_mtime_ns)
        except OSError:
            return None

    def read(self):
        try:
            with open(self.path, "rb") as f:
                b = f.read()
        except OSError:
            return None
        if not b:
            return None
        return (b[0], b[1:])

    def write(self, kind, data):
        tmp = self.path + ".tmp"
        with open(tmp, "wb") as f:
            f.write(bytes([kind]) + data)
        os.replace(tmp, self.path)


def make_clipboard():
    if os.environ.get("DEVKIT_CLIP_FAKE"):
        return FakeClipboard(os.environ["DEVKIT_CLIP_FAKE"])
    if SYSTEM == "Darwin":
        return MacClipboard()
    if SYSTEM == "Windows":
        return WindowsClipboard()
    return LinuxClipboard()


def notify(title, text):
    text = text[:180]
    try:
        if os.environ.get("DEVKIT_CLIP_FAKE"):
            log("notify:", title, "-", text)
        elif SYSTEM == "Darwin":
            esc = lambda s: s.replace("\\", "\\\\").replace('"', '\\"')
            subprocess.run(["osascript", "-e", 'display notification "%s" with title "%s"' % (esc(text), esc(title))], timeout=5)
        elif SYSTEM == "Linux" and shutil.which("notify-send"):
            subprocess.run(["notify-send", title, text], timeout=5)
    except (OSError, subprocess.SubprocessError):
        pass


# ── keeping in step ──────────────────────────────────────────────────────────────────────────

def summary(kind, data):
    if kind == T_IMAGE:
        return "an image (%d KB)" % (len(data) // 1024)
    t = data.decode("utf-8", errors="replace").strip().replace("\n", " ")
    return t[:60] + ("…" if len(t) > 60 else "")


class Sync:
    def __init__(self, cfg, board, clip):
        self.cfg, self.board, self.clip = cfg, board, clip
        self.keys = keys_from(cfg)
        self.v, self.epoch = 0, int(cfg.get("epoch", 0))
        self.items = {}                      # id -> item from /state
        self.seen = set()                    # ids already dealt with
        self.ours = None                     # sha256 of what we last put on (or sent from) the clipboard
        self.token = None                    # clipboard change token last looked at
        self.started = False

    def tok(self):
        return {"t": self.cfg["t"]}

    def blob(self, it):
        b = it.get("b")
        raw = base64.b64decode(b) if b else self.board.call("/blob", dict(self.tok(), id=it["id"]), raw=True, timeout=30)
        plain = cc.unseal(self.keys, raw)
        return plain[0], plain[1:]

    def send(self, kind, data, flags=0, to=0):
        sealed = cc.seal(self.keys, bytes([kind]) + data)
        q = dict(self.tok(), f=flags | (F_IMAGE if kind == T_IMAGE else 0))
        if to:
            q["to"] = to
        r = self.board.call("/put", q, body=sealed, timeout=30)
        if not r.get("ok"):
            log("not sent:", r.get("err"))
        return r

    def check_clipboard(self):
        t = self.clip.change_token()
        if t == self.token:
            return
        self.token = t
        if self.cfg.get("paused"):           # skipped for good: resuming doesn't send what was copied meanwhile
            return
        got = self.clip.read()
        if not got:
            return
        kind, data = got
        h = hashlib.sha256(bytes([kind]) + data).hexdigest()
        if h == self.ours:                   # what we just put there ourselves: don't send it back
            return
        if (kind == T_TEXT and len(data) > MAX_TEXT * 4) or (kind == T_IMAGE and len(data) > MAX_IMAGE):
            log("too big to share:", summary(kind, data))
            return
        self.ours = h
        r = self.send(kind, data)
        if r.get("ok"):
            self.seen.add(r["id"])
            log("sent", summary(kind, data))

    def poll(self):
        r = self.board.call("/state", dict(self.tok(), v=self.v, epoch=self.epoch))
        if not r.get("ok"):
            raise PermissionError(r.get("err"))
        if r.get("same"):
            return
        if r["epoch"] != self.epoch:         # the group was reset on the board
            raise PermissionError("nogroup")
        ids = set(r["ids"])
        for it in r["items"]:
            self.items[it["id"]] = it
        for i in list(self.items):
            if i not in ids:
                del self.items[i]
        self.v = r["v"]
        me = self.cfg["id"]
        fresh = sorted(i for i, it in self.items.items() if i not in self.seen and it["from"] != me)
        if not self.started:                 # what was there before this helper started is history
            self.seen.update(self.items)
            self.started = not r.get("more")
            return
        # One item goes on the clipboard: the newest sent to this computer by name, else the newest of
        # all. The rest is history, and isn't even fetched (fetching a one-time item uses it up).
        mine = [i for i in fresh if self.items[i]["to"] == me and not self.items[i]["f"] & F_OPEN]
        general = [i for i in fresh if not self.items[i]["f"] & F_OPEN and not self.items[i]["f"] & F_ONCE]
        chosen = max(mine) if mine else (max(general) if general else None)
        for i in fresh:
            it = self.items.get(i)
            self.seen.add(i)
            if not it or (i != chosen and not it["f"] & F_OPEN):
                continue
            try:
                kind, data = self.blob(it)
            except (cc.Tampered, ConnectionError, ValueError) as e:
                log("couldn't open an item:", e)
                continue
            who = next((d["n"] for d in r["devices"] if d["i"] == it["from"]), "another device")
            if it["f"] & F_OPEN:
                url = data.decode("utf-8", errors="replace").strip()
                if url.startswith(("http://", "https://")):
                    if os.environ.get("DEVKIT_CLIP_FAKE"):
                        log("would open:", url)
                    else:
                        webbrowser.open(url)
                    notify("Opened from " + who, url)
                continue
            self.ours = hashlib.sha256(bytes([kind]) + data).hexdigest()
            self.clip.write(kind, data)
            self.token = self.clip.change_token()
            log("from", who + ":", summary(kind, data))
            if it["to"] == me or it["f"] & F_ONCE:
                notify("From " + who, ("Ready to paste: " if kind == T_TEXT else "") + summary(kind, data))
        if r.get("more"):
            self.poll()


def run():
    cfg = load_config()
    if not cfg.get("t"):
        if not setup_interactive():
            return 1
        cfg = load_config()
    board = Board(board_hosts(cfg))
    clip = make_clipboard()
    sync = Sync(cfg, board, clip)
    if SYSTEM == "Windows" and not os.environ.get("DEVKIT_CLIP_FAKE"):
        threading.Thread(target=windows_hotkey, daemon=True).start()
    log("running as", cfg.get("name"), "on", " ".join(board.hosts))
    backoff = 1
    while True:
        try:
            cfg2 = load_config()             # --pause/--resume from another process
            sync.cfg["paused"] = cfg2.get("paused", False)
            sync.check_clipboard()
            sync.poll()
            backoff = 1
            time.sleep(1.0)
        except PermissionError as e:
            log("the board doesn't know this computer any more (%s); run --setup again" % e)
            notify("Clipboard", "This computer left the clipboard group. Run the setup again to rejoin.")
            return 2
        except (ConnectionError, OSError, ValueError, subprocess.SubprocessError) as e:
            log("waiting for the board:", e)
            time.sleep(min(backoff, 30))
            backoff *= 2
        except KeyboardInterrupt:
            return 0


# ── joining, the first time ──────────────────────────────────────────────────────────────────

JOIN_ERRORS = {"proof": "That passphrase doesn't match your group's.", "wait": "Too many wrong tries. Wait a minute.",
               "full": "The group already has 12 devices.", "nogroup": "No group yet."}


def setup_interactive():
    hosts = board_hosts(load_config())
    board = Board(hosts)
    try:
        hello = board.call("/hello")
    except ConnectionError as e:
        print("Can't reach the board:", e)
        return False
    name = platform.node().split(".")[0] or "Computer"
    try:
        import tkinter  # noqa: F401
        return setup_window(board, hello, name)
    except Exception as e:                   # no display, or no tkinter
        log("no setup window (%s); asking here" % e)
    import getpass
    print("Clipboard setup for this computer.")
    name = input("This computer's name [%s]: " % name).strip() or name
    while True:
        p = getpass.getpass("Passphrase: " if hello["set"] else "Choose a passphrase for your clipboard group: ")
        if not hello["set"] and getpass.getpass("The same again: ") != p:
            print("They don't match."); continue
        if len(p) < 6:
            print("At least 6 characters."); continue
        try:
            cfg = join_group(board, name, p, not hello["set"])
        except ValueError as e:
            print(JOIN_ERRORS.get(str(e), str(e))); continue
        save_config(cfg)
        print("Joined as", name + ".")
        return True


def setup_window(board, hello, name):
    import tkinter as tk
    from tkinter import ttk
    root = tk.Tk()
    root.title("Clipboard: join")
    root.resizable(False, False)
    f = ttk.Frame(root, padding=18)
    f.grid()
    create = not hello["set"]
    ttk.Label(f, text="Start your clipboard group" if create else "Join your clipboard group", font=("", 14, "bold")).grid(columnspan=2, sticky="w")
    ttk.Label(f, text=("Choose a passphrase. Every device types it once. The board never sees it,\nso it can't read your clipboard. Keep it somewhere safe: it can't be recovered."
                       if create else "Type the passphrase you chose on your first device."), foreground="#666").grid(columnspan=2, sticky="w", pady=(4, 12))
    v_name, v_p1, v_p2 = tk.StringVar(value=name), tk.StringVar(), tk.StringVar()
    ttk.Label(f, text="This computer").grid(row=2, column=0, sticky="w")
    ttk.Entry(f, textvariable=v_name, width=32).grid(row=2, column=1, pady=3)
    ttk.Label(f, text="Passphrase").grid(row=3, column=0, sticky="w")
    e1 = ttk.Entry(f, textvariable=v_p1, show="•", width=32)
    e1.grid(row=3, column=1, pady=3)
    if create:
        ttk.Label(f, text="Again").grid(row=4, column=0, sticky="w")
        ttk.Entry(f, textvariable=v_p2, show="•", width=32).grid(row=4, column=1, pady=3)
    msg = ttk.Label(f, text="", foreground="#c93b3b")
    msg.grid(row=5, columnspan=2, sticky="w", pady=(6, 0))
    done = {"ok": False}

    def go(*_):
        p = v_p1.get()
        if len(p) < 6:
            msg.config(text="At least 6 characters."); return
        if create and v_p2.get() != p:
            msg.config(text="The two don't match."); return
        msg.config(text="Working…", foreground="#666"); root.update()
        try:
            cfg = join_group(board, v_name.get().strip() or name, p, create)
        except (ValueError, ConnectionError) as e:
            msg.config(text=JOIN_ERRORS.get(str(e), str(e)), foreground="#c93b3b"); return
        save_config(cfg)
        done["ok"] = True
        root.destroy()

    ttk.Button(f, text="Start" if create else "Join", command=go).grid(row=6, column=1, sticky="e", pady=(10, 0))
    root.bind("<Return>", go)
    e1.focus_set()
    root.mainloop()
    return done["ok"]


# ── the quick picker ─────────────────────────────────────────────────────────────────────────

def fetch_history(cfg):
    board = Board(board_hosts(cfg))
    keys = keys_from(cfg)
    r = board.call("/state", {"t": cfg["t"], "v": 0, "epoch": 0})
    if not r.get("ok"):
        raise PermissionError(r.get("err"))
    items = list(r["items"])
    while r.get("more"):
        r = board.call("/state", {"t": cfg["t"], "v": r["v"], "epoch": r["epoch"]})
        items += r["items"]
    names = {d["i"]: d["n"] for d in r["devices"]}
    out = []
    for it in items:
        if it["f"] & (F_ONCE | F_OPEN):
            continue
        try:
            raw = base64.b64decode(it["b"]) if it.get("b") else board.call("/blob", {"t": cfg["t"], "id": it["id"]}, raw=True, timeout=30)
            plain = cc.unseal(keys, raw)
        except (cc.Tampered, ConnectionError, ValueError):
            continue
        out.append(dict(id=it["id"], kind=plain[0], data=plain[1:], pin=bool(it["f"] & F_PIN), who=names.get(it["from"], "?"), at=it["at"]))
    out.sort(key=lambda x: (not x["pin"], -x["id"]))
    return out, board


def ago(t):
    if not t:
        return ""
    s = max(0, int(time.time()) - t)
    return "now" if s < 60 else "%dm" % (s // 60) if s < 3600 else "%dh" % (s // 3600) if s < 86400 else "%dd" % (s // 86400)


def paste_keystroke():
    """Presses paste in whatever app was in front before the picker opened."""
    time.sleep(0.15)
    try:
        if SYSTEM == "Windows":
            import ctypes
            u = ctypes.WinDLL("user32")
            for vk, up in ((0x11, 0), (0x56, 0), (0x56, 2), (0x11, 2)):    # Ctrl down, V down, V up, Ctrl up
                u.keybd_event(vk, 0, up, 0)
        elif SYSTEM == "Darwin":
            r = subprocess.run(["osascript", "-e", 'tell application "System Events" to keystroke "v" using command down'],
                               capture_output=True, timeout=5)
            if r.returncode:
                notify("Clipboard", "Copied. Press ⌘V to paste (or allow Accessibility for Python to paste for you).")
        elif shutil.which("xdotool"):
            subprocess.run(["xdotool", "key", "--clearmodifiers", "ctrl+v"], timeout=5)
    except (OSError, subprocess.SubprocessError):
        pass


def pick():
    cfg = load_config()
    if not cfg.get("t"):
        print("Not set up yet: run with --setup first.")
        return 1
    import tkinter as tk
    try:
        items, board = fetch_history(cfg)
    except (ConnectionError, PermissionError) as e:
        notify("Clipboard", "Can't reach the board: %s" % e)
        return 1
    root = tk.Tk()
    root.title("Clipboard")
    root.attributes("-topmost", True)
    w, h = 560, 420
    try:
        x, y = root.winfo_pointerxy()
    except tk.TclError:
        x, y = 200, 200
    sw, sh = root.winfo_screenwidth(), root.winfo_screenheight()
    root.geometry("%dx%d+%d+%d" % (w, h, min(max(0, x - w // 2), sw - w), min(max(0, y - 40), sh - h - 40)))
    bg, fg, dim, sel = "#ffffff", "#151922", "#667085", "#e9eefc"
    root.configure(bg=bg)
    q = tk.StringVar()
    e = tk.Entry(root, textvariable=q, font=("", 14), relief="flat", bg="#f0f2f5", fg=fg, insertbackground=fg)
    e.pack(fill="x", padx=10, pady=(10, 6), ipady=6)
    lb = tk.Listbox(root, font=("", 12), activestyle="none", relief="flat", bg=bg, fg=fg, selectbackground=sel,
                    selectforeground=fg, highlightthickness=0, borderwidth=0)
    lb.pack(fill="both", expand=True, padx=10)
    tip = tk.Label(root, text="Enter paste · ⌘/Ctrl+P pin · Delete remove · Esc close", fg=dim, bg=bg, font=("", 10))
    tip.pack(fill="x", pady=(4, 8))
    shown = []

    def label(it):
        text = "[image, %d KB]" % (len(it["data"]) // 1024) if it["kind"] == T_IMAGE else it["data"].decode("utf-8", errors="replace")
        text = " ".join(text.split())
        text = text[:70] + ("…" if len(text) > 70 else "")
        return ("★ " if it["pin"] else "   ") + text + "   · " + it["who"] + (" " + ago(it["at"]) if it["at"] else "")

    def refresh(*_):
        want = q.get().lower()
        shown[:] = [it for it in items if not want or (it["kind"] == T_TEXT and want in it["data"].decode("utf-8", "replace").lower())]
        lb.delete(0, "end")
        for it in shown:
            lb.insert("end", label(it))
        if shown:
            lb.selection_set(0); lb.activate(0)

    def current():
        s = lb.curselection()
        return shown[s[0]] if s and s[0] < len(shown) else None

    def choose(*_):
        it = current()
        if not it:
            return
        make_clipboard().write(it["kind"], it["data"])
        root.destroy()
        if cfg.get("autopaste", True):
            paste_keystroke()

    def move(d):
        s = lb.curselection()
        i = (s[0] if s else -1) + d
        if 0 <= i < len(shown):
            lb.selection_clear(0, "end"); lb.selection_set(i); lb.activate(i); lb.see(i)
        return "break"

    def toggle_pin(*_):
        it = current()
        if it:
            board.call("/pin", {"t": cfg["t"], "id": it["id"], "on": 0 if it["pin"] else 1})
            it["pin"] = not it["pin"]
            items.sort(key=lambda x: (not x["pin"], -x["id"]))
            refresh()
        return "break"

    def remove(*_):
        it = current()
        if it:
            board.call("/del", {"t": cfg["t"], "id": it["id"]})
            items.remove(it)
            refresh()
        return "break"

    q.trace_add("write", refresh)
    e.bind("<Down>", lambda _: move(1)); e.bind("<Up>", lambda _: move(-1))
    root.bind("<Return>", choose); lb.bind("<Double-Button-1>", choose)
    root.bind("<Escape>", lambda _: root.destroy())
    root.bind("<Control-p>", toggle_pin); root.bind("<Command-p>", toggle_pin)
    e.bind("<Delete>", lambda ev: remove() if not q.get() else None)
    lb.bind("<Delete>", remove)
    refresh()
    if not items:
        lb.insert("end", "   Nothing yet. Copy something on any of your devices.")
    root.after(50, lambda: (root.lift(), root.focus_force(), e.focus_set()))
    root.mainloop()
    return 0


# Windows: a hotkey that opens the picker, registered by the helper itself. Win+V is Windows' own
# clipboard history, so it is left alone; the first of these that Windows lets us have is used.
HOTKEYS = [(0x0008 | 0x0001, "Win+Alt+V"), (0x0002 | 0x0001 | 0x0004, "Ctrl+Alt+Shift+V"), (0x0008 | 0x0004, "Win+Shift+V")]


def windows_hotkey():
    import ctypes
    from ctypes import wintypes as w
    u = ctypes.WinDLL("user32", use_last_error=True)
    got = None
    for i, (mods, name) in enumerate(HOTKEYS):
        if u.RegisterHotKey(None, 1 + i, mods | 0x4000, 0x56):   # MOD_NOREPEAT, 'V'
            got = name
            break
    cfg = load_config()
    cfg["hotkey"] = got or "none"
    save_config(cfg)
    log("picker hotkey:", got or "none free")
    msg = w.MSG()
    while u.GetMessageW(ctypes.byref(msg), None, 0, 0) > 0:
        if msg.message == 0x0312:                               # WM_HOTKEY
            exe = sys.executable.replace("python.exe", "pythonw.exe")
            subprocess.Popen([exe if os.path.exists(exe) else sys.executable, os.path.abspath(__file__), "--pick"])


# ── the rest of the command line ─────────────────────────────────────────────────────────────

def status():
    cfg = load_config()
    if not cfg.get("t"):
        print("Not set up. Run: devkit_clip.py --setup")
        return 1
    print("This computer:", cfg.get("name"), "(device %s)" % cfg.get("id"))
    print("Board:", " ".join(board_hosts(cfg)))
    print("Sharing:", "paused" if cfg.get("paused") else "on")
    if cfg.get("hotkey"):
        print("Picker hotkey:", cfg["hotkey"])
    try:
        r = Board(board_hosts(cfg)).call("/state", {"t": cfg["t"], "v": 0, "epoch": 0})
        if r.get("ok"):
            print("Devices:", ", ".join(d["n"] + ("" if d["on"] else " (offline)") for d in r["devices"]))
            print("Items on the board:", len(r["ids"]), "· %d KB of %d KB" % (r["used"] // 1024, r["budget"] // 1024))
        else:
            print("The board says:", r.get("err"))
    except ConnectionError as e:
        print(e)
    return 0


def main(argv):
    a = argv[1] if len(argv) > 1 else ""
    if a == "--setup":
        return 0 if setup_interactive() else 1
    if a == "--pick":
        return pick()
    if a == "--status":
        return status()
    if a in ("--pause", "--resume"):
        cfg = load_config(); cfg["paused"] = a == "--pause"; save_config(cfg)
        print("Sharing from this computer is", "paused." if cfg["paused"] else "on.")
        return 0
    if a == "--leave":
        cfg = load_config()
        if cfg.get("t"):
            try:
                Board(board_hosts(cfg)).call("/forget", {"t": cfg["t"], "id": cfg["id"]})
            except ConnectionError:
                pass
        for f in (CONFIG, LOG, LOG + ".old"):            # the keys go; the helper itself stays
            try:
                os.remove(f)
            except OSError:
                pass
        print("Left the group: its keys are gone from this computer. (To remove the helper too, use the uninstall line.)")
        return 0
    return run()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
