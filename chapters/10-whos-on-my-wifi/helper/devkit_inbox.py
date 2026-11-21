"""devkit-diary, chapter 10 — the inbox: lets the WiFi scanner send this computer a message.

It asks the board every two seconds "anything for me?". When there is, it shows the message in a
small window on top of everything, with a box to type a reply. Nothing listens on this computer:
it only ever talks *to* the board, so no firewall door is opened and nothing on the WiFi can reach it.

Python's standard library only. Installed and started at login by:
    curl -fsSL http://scan.local/i | sh          (macOS, Linux)
    irm http://scan.local/i.ps1 | iex            (Windows)

For testing: DEVKIT_INBOX_BOARD=host:port picks the board, DEVKIT_INBOX_SHOW=print prints messages
instead of showing them (and DEVKIT_INBOX_REPLY, if set, is sent back as the reply).
"""
import json
import os
import platform
import shutil
import socket
import subprocess
import sys
import threading
import time
import urllib.parse
import urllib.request

HOME = os.path.dirname(os.path.abspath(__file__))
POLL = 2.0
SYSTEM = platform.system()          # "Darwin", "Windows", "Linux"
OS = {"Darwin": "mac", "Windows": "windows"}.get(SYSTEM, "linux")


def log(*a):
    line = time.strftime("%H:%M:%S ") + " ".join(str(x) for x in a)
    try:
        print(line, flush=True)
    except Exception:               # pythonw on Windows has no console
        try:
            with open(os.path.join(HOME, "inbox.log"), "a", encoding="utf-8") as f:
                f.write(line + "\n")
        except Exception:
            pass


def boards():
    env = os.environ.get("DEVKIT_INBOX_BOARD")
    if env:
        return env.split()
    try:
        with open(os.path.join(HOME, "board"), encoding="utf-8") as f:
            return f.read().split() or ["scan.local"]
    except OSError:
        return ["scan.local"]


def my_name():
    if SYSTEM == "Darwin":
        try:
            n = subprocess.run(["scutil", "--get", "ComputerName"], capture_output=True, text=True, timeout=3).stdout.strip()
            if n:
                return n
        except Exception:
            pass
    return socket.gethostname().split(".")[0]


# ── showing a message, and getting a reply ───────────────────────────────────────────────────

MAC_DIALOG = [
    "on run argv",
    'set r to display dialog (item 1 of argv) with title (item 2 of argv) default answer "" '
    'buttons {"Close", "Reply"} default button "Reply" cancel button "Close" giving up after 900',
    "if gave up of r then return \"\"",
    'return "R:" & (text returned of r)',
    "end run",
]


def show_mac(text, title):
    args = ["osascript"]
    for line in MAC_DIALOG:
        args += ["-e", line]
    out = subprocess.run(args + [text, title], capture_output=True, text=True).stdout.strip()
    return out[2:] if out.startswith("R:") else None


def show_tk(text, title):
    import tkinter as tk
    reply = {"v": None}
    root = tk.Tk()
    root.title(title)
    root.attributes("-topmost", True)
    root.resizable(False, False)
    pad = {"padx": 16, "pady": 6}
    tk.Label(root, text=title, font=("Segoe UI", 11, "bold")).pack(anchor="w", **pad)
    tk.Label(root, text=text, wraplength=360, justify="left", font=("Segoe UI", 11)).pack(anchor="w", **pad)
    entry = tk.Entry(root, width=44, font=("Segoe UI", 10))
    entry.pack(fill="x", **pad)
    row = tk.Frame(root)
    row.pack(fill="x", padx=16, pady=(4, 14))

    def send(*_):
        reply["v"] = entry.get().strip() or None
        root.destroy()
    tk.Button(row, text="Reply", width=10, command=send).pack(side="right")
    tk.Button(row, text="Close", width=10, command=root.destroy).pack(side="right", padx=8)
    entry.bind("<Return>", send)
    root.after(200, lambda: (root.lift(), root.focus_force(), entry.focus_set()))
    root.mainloop()
    return reply["v"]


def show_windows_plain(text, title):
    import ctypes
    ctypes.windll.user32.MessageBoxW(0, text, title, 0x40 | 0x40000 | 0x10000)   # info, topmost, foreground
    return None


def show_linux(text, title):
    if shutil.which("zenity"):
        r = subprocess.run(["zenity", "--entry", "--title", title, "--text", text, "--ok-label", "Reply",
                            "--cancel-label", "Close"], capture_output=True, text=True)
        return (r.stdout.strip() or None) if r.returncode == 0 else None
    if shutil.which("kdialog"):
        r = subprocess.run(["kdialog", "--title", title, "--inputbox", text], capture_output=True, text=True)
        return (r.stdout.strip() or None) if r.returncode == 0 else None
    try:
        return show_tk(text, title)
    except Exception:
        if shutil.which("notify-send"):
            subprocess.run(["notify-send", title, text])
        return None


def show(text, sender):
    title = "Message from %s" % (sender or "the scanner")
    mode = os.environ.get("DEVKIT_INBOX_SHOW", "")
    if mode == "print":
        log("MESSAGE from %s: %s" % (sender, text))
        return os.environ.get("DEVKIT_INBOX_REPLY") or None
    if SYSTEM == "Darwin":
        return show_mac(text, title)
    if SYSTEM == "Windows":
        try:
            return show_tk(text, title)
        except Exception:
            return show_windows_plain(text, title)
    return show_linux(text, title)


# ── talking to the board ─────────────────────────────────────────────────────────────────────

class Board:
    def __init__(self):
        self.hosts = boards()
        self.at = 0

    def url(self, path):
        return "http://%s%s" % (self.hosts[self.at % len(self.hosts)], path)

    def get(self, path):
        with urllib.request.urlopen(self.url(path), timeout=5) as r:
            return json.loads(r.read().decode("utf-8"))

    def post(self, path, **form):
        data = urllib.parse.urlencode(form).encode()
        with urllib.request.urlopen(urllib.request.Request(self.url(path), data=data), timeout=5) as r:
            return r.read()

    def next_host(self):
        self.at += 1


def answer(board, msg):
    try:
        reply = show(msg.get("t", ""), msg.get("f", ""))
    except Exception as e:
        log("couldn't show it:", e)
        return
    if reply:
        try:
            board.post("/reply", id=msg.get("id", 0), t=reply[:190])
            log("replied")
        except Exception as e:
            log("couldn't send the reply:", e)


def main():
    try:                            # one copy at a time
        guard = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        guard.bind(("127.0.0.1", int(os.environ.get("DEVKIT_INBOX_PORT", "47332"))))
        guard.listen(1)
    except OSError:
        log("already running")
        return
    board = Board()
    name = my_name()
    log("inbox started as %r, board %s" % (name, " / ".join(board.hosts)))
    q = "/inbox?" + urllib.parse.urlencode({"n": name, "o": OS})
    failing = 0
    while True:
        try:
            got = board.get(q)
            if failing:
                log("reached the board again")
            failing = 0
            for m in got.get("m", []):
                threading.Thread(target=answer, args=(board, m), daemon=True).start()
            time.sleep(POLL)
        except Exception as e:
            if failing == 0:
                log("can't reach the board (%s), will keep trying" % e)
            failing += 1
            board.next_host()
            time.sleep(min(30, POLL * (1 + failing)))


if __name__ == "__main__":
    main()
