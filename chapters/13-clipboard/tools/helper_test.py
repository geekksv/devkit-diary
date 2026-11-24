"""Two real helpers against the laptop build of the board, with files standing in for their
clipboards (so this test never touches this computer's real clipboard).

    c++ -std=c++17 -I../src/clip devserver.cpp -o devserver
    python3 helper_test.py ./devserver
"""
import base64, json, os, shutil, subprocess, sys, tempfile, time, urllib.request, urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
HELPER = os.path.join(HERE, "..", "helper", "devkit_clip.py")
sys.path.insert(0, os.path.join(HERE, "..", "helper"))
import clipcrypto as cc

PORT = 8094
HOST = "localhost:%d" % PORT
PASS = "our shared passphrase ✨"
passed = failed = 0


def check(ok, what):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
        print("FAIL", what)


def call(path, q=None, body=None):
    url = "http://%s%s%s" % (HOST, path, ("?" + urllib.parse.urlencode(q)) if q else "")
    req = urllib.request.Request(url, data=body, method="POST" if body is not None else "GET")
    with urllib.request.urlopen(req, timeout=10) as r:
        data = r.read()
    try:
        return json.loads(data)
    except ValueError:
        return data


def until(f, what, secs=12):
    end = time.time() + secs
    while time.time() < end:
        try:
            v = f()
            if v:
                return v
        except Exception:
            pass
        time.sleep(0.2)
    check(False, "timed out: " + what)
    return None


class Helper:
    def __init__(self, name, create=False, passphrase=PASS):
        self.home = tempfile.mkdtemp(prefix="clip-" + name + "-")
        self.clip = os.path.join(self.home, "clipboard")
        self.env = dict(os.environ, HOME=self.home, DEVKIT_CLIP_FAKE=self.clip, DEVKIT_CLIP_HOSTS=HOST)
        code = ("import sys; sys.path.insert(0, %r); import devkit_clip as d\n"
                "try:\n  cfg = d.join_group(d.Board(%r.split()), %r, %r, %r); d.save_config(cfg); print('ok')\n"
                "except ValueError as e:\n  print('err', e)\n") % (os.path.dirname(HELPER), HOST, name, passphrase, create)
        self.joined = subprocess.run([sys.executable, "-c", code], env=self.env, capture_output=True, text=True).stdout.strip()
        self.proc = None

    @property
    def cfg(self):
        with open(os.path.join(self.home, ".devkit-clip", "config.json")) as f:
            return json.load(f)

    def start(self):
        self.proc = subprocess.Popen([sys.executable, HELPER], env=self.env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def cli(self, *args):
        return subprocess.run([sys.executable, HELPER] + list(args), env=self.env, capture_output=True, text=True).stdout

    def copy(self, data, kind=0):
        with open(self.clip + ".tmp", "wb") as f:
            f.write(bytes([kind]) + data)
        os.replace(self.clip + ".tmp", self.clip)

    def paste(self):
        try:
            with open(self.clip, "rb") as f:
                b = f.read()
            return (b[0], b[1:]) if b else None
        except OSError:
            return None

    def log(self):
        try:
            return open(os.path.join(self.home, ".devkit-clip", "clip.log"), encoding="utf-8").read()
        except OSError:
            return ""

    def stop(self):
        if self.proc:
            self.proc.terminate()
            self.proc.wait()
        shutil.rmtree(self.home, ignore_errors=True)


def board_items(cfg):
    r = call("/state", {"t": cfg["t"], "v": 0, "epoch": 0})
    return r["ids"], r


def main():
    server = subprocess.Popen([sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "devserver"), str(PORT)], stdout=subprocess.PIPE, text=True)
    server.stdout.readline()
    helpers = []
    try:
        a = Helper("Laptop", create=True); helpers.append(a)
        check(a.joined == "ok", "the first computer starts the group: " + a.joined)
        b = Helper("Desktop"); helpers.append(b)
        check(b.joined == "ok", "the second joins with the passphrase: " + b.joined)
        x = Helper("Intruder", passphrase="a wrong passphrase"); helpers.append(x)
        check(x.joined == "err proof", "a wrong passphrase is refused: " + x.joined)
        a.start(); b.start()
        time.sleep(2.5)

        # Copy on A, paste on B.
        a.copy("नमस्ते from the laptop 😀\nline two".encode())
        got = until(lambda: b.paste(), "text reaches B")
        check(got == (0, "नमस्ते from the laptop 😀\nline two".encode()), "B has exactly what A copied: %r" % (got,))
        # And back.
        b.copy(b"reply from the desktop")
        check(until(lambda: a.paste() == (0, b"reply from the desktop"), "text reaches A"), "A has B's copy")
        time.sleep(3)
        ids, _ = board_items(a.cfg)
        check(len(ids) == 2, "no loop: two copies made two items, not more (%d)" % len(ids))

        # The board holds only sealed bytes.
        _, st = board_items(a.cfg)
        sealed = b"".join(base64.b64decode(it["b"]) for it in st["items"] if it.get("b"))
        check(b"laptop" not in sealed and b"desktop" not in sealed and len(sealed) > 40, "the board never sees the words")

        # A big text goes by itself (not inline), an image too.
        big = ("long text " * 15000).encode()
        a.copy(big)
        check(until(lambda: b.paste() == (0, big), "big text"), "a 150 KB text arrives whole")
        img = os.urandom(60000)
        b.copy(img, kind=1)
        check(until(lambda: a.paste() == (1, img), "image"), "an image arrives whole")

        # Paused: nothing is sent.
        a.cli("--pause")
        time.sleep(1.5)
        before, _ = board_items(a.cfg)
        a.copy(b"secret while paused")
        time.sleep(3)
        after, _ = board_items(a.cfg)
        check(len(after) == len(before), "a paused computer sends nothing")
        a.cli("--resume")

        # A third device (like the page) sends a one-time code to B only, and a link for A to open.
        keys = cc.Keys(*[bytes.fromhex(a.cfg[k]) for k in ("enc", "mac", "auth")])
        bid, aid = b.cfg["id"], a.cfg["id"]
        r = call("/put", {"t": a.cfg["t"], "f": 2, "to": bid}, cc.seal(keys, b"\x00" + b"482913"))
        check(r.get("ok"), "a one-time item for B")
        check(until(lambda: b.paste() == (0, b"482913"), "one-time code"), "B gets the one-time code")
        check(until(lambda: r["id"] not in board_items(a.cfg)[0], "one-time gone"), "and it is gone from the board after")
        r = call("/put", {"t": b.cfg["t"], "f": 8, "to": aid}, cc.seal(keys, b"\x00https://example.com/page"))
        check(until(lambda: "would open: https://example.com/page" in a.log(), "open on A"), "A opens a link sent to it")

        # The picker's list: decrypted history, newest first.
        code = "import sys; sys.path.insert(0, %r); import devkit_clip as d; items, _ = d.fetch_history(d.load_config()); print(len(items), items[0]['data'][:20])" % os.path.dirname(HELPER)
        out = subprocess.run([sys.executable, "-c", code], env=a.env, capture_output=True, text=True).stdout
        n = int(out.split()[0]) if out.split() and out.split()[0].isdigit() else 0
        check(n >= 4 and "b'" in out and "secret while paused" not in out, "the picker gets the decrypted history: " + out.strip()[:80])

        # Status, and leaving.
        st = b.cli("--status")
        check("Laptop" in st and "Desktop" in st, "status lists the devices")
        b.cli("--leave")
        names = [d["n"] for d in board_items(a.cfg)[1]["devices"]]
        check("Desktop" not in names, "leaving removes the device from the group: %s" % names)
        check(not os.path.exists(os.path.join(b.home, ".devkit-clip", "config.json")), "and its keys from the computer")
    finally:
        for h in helpers:
            h.stop()
        server.terminate()
    print("%d passed, %d failed" % (passed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
