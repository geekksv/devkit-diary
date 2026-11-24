"""Checks helper/clipcrypto.py: against the published AES vectors, against openssl, and against
the browser's WebCrypto running tools/clipcrypto.js, in both directions.

    python3 tools/crypto_test.py            (needs Google Chrome for the browser half)
"""
import base64, json, os, random, shutil, subprocess, sys, tempfile, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "helper"))
import clipcrypto as cc

passed = failed = 0


def check(ok, what):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
        print("FAIL", what)


# ── published vectors ─────────────────────────────────────────────────────────
# FIPS-197 appendix C.3 (AES-256)
k = bytes(range(32))
check(cc.aes256_block(k, bytes.fromhex("00112233445566778899aabbccddeeff")).hex() == "8ea2b7ca516745bfeafc49904b496089",
      "FIPS-197 C.3")
# SP 800-38A F.5.5 (CTR-AES256.Encrypt)
k = bytes.fromhex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4")
iv = bytes.fromhex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff")
pt = bytes.fromhex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
                   "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710")
ct = bytes.fromhex("601ec313775789a5b7a7f504bbf3d228f443e3ca4d62b59aca84e990cacaf5c5"
                   "2b0930daa23de94ce87017ba2d84988ddfc9c58db67aada613c2dd08457941a6")
check(cc.aes256_ctr(k, iv, pt) == ct, "SP 800-38A F.5.5")
check(cc.aes256_ctr(k, iv, ct) == pt, "CTR undoes itself")

# ── against openssl, random keys, ivs and lengths ─────────────────────────────
rnd = random.Random(7)
if shutil.which("openssl"):
    n = 0
    for _ in range(40):
        key, iv = bytes(rnd.getrandbits(8) for _ in range(32)), bytes(rnd.getrandbits(8) for _ in range(16))
        iv = iv[:8] + (b"\x00" * 7 + iv[15:]) if rnd.random() < .5 else iv   # no wrap in openssl's 128-bit counter
        data = bytes(rnd.getrandbits(8) for _ in range(rnd.choice([0, 1, 15, 16, 17, 100, 4096, 5000])))
        if not data:
            continue
        out = subprocess.run(["openssl", "enc", "-aes-256-ctr", "-K", key.hex(), "-iv", iv.hex(), "-nosalt"],
                             input=data, capture_output=True)
        if out.returncode:
            print("openssl failed:", out.stderr.decode()[:200]); break
        low = int.from_bytes(iv[8:], "big")
        if low + (len(data) + 15) // 16 >= 1 << 64:
            continue                                         # openssl carries into the high half; we don't
        check(out.stdout == cc.aes256_ctr(key, iv, data), "openssl %d bytes" % len(data)); n += 1
    print("openssl comparisons:", n)
else:
    print("openssl not found; skipped")

# ── seal / unseal ─────────────────────────────────────────────────────────────
keys = cc.derive("correct horse", b"\x01" * 16, 1000)
blob = cc.seal(keys, "नमस्ते 😀 hello".encode())
check(cc.unseal(keys, blob).decode() == "नमस्ते 😀 hello", "round trip")
for i in [0, 5, 20, len(blob) - 1]:
    bad = bytearray(blob); bad[i] ^= 1
    try:
        cc.unseal(keys, bytes(bad)); check(False, "tamper at %d not noticed" % i)
    except cc.Tampered:
        check(True, "")
try:
    cc.unseal(cc.derive("wrong horse", b"\x01" * 16, 1000), blob); check(False, "wrong passphrase not noticed")
except cc.Tampered:
    check(True, "")
check(cc.seal(keys, b"x") != cc.seal(keys, b"x"), "a fresh iv each time")
check(cc.derive("é", b"s" * 16, 10).enc == cc.derive("é", b"s" * 16, 10).enc, "passphrases compared after NFC")

t = time.time(); cc.seal(keys, os.urandom(300_000)); speed = 300 / (time.time() - t)
print("python AES-CTR + seal: %.0f KB/s" % speed)

# ── against the browser ───────────────────────────────────────────────────────
chrome = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
if os.path.exists(chrome):
    cases = []
    passes = ["correct horse battery staple", "नमस्ते दुनिया", "😀 emoji pass", "é decomposed", ""]
    for i in range(24):
        salt = bytes(rnd.getrandbits(8) for _ in range(16))
        iv = bytes(rnd.getrandbits(8) for _ in range(16))
        if i in (3, 4):
            iv = iv[:8] + b"\xff" * 7 + b"\xfe"              # the counter wraps inside the item
        size = [0, 1, 15, 16, 17, 33, 1000, 5000, 70000][i % 9]
        plain = bytes(rnd.getrandbits(8) for _ in range(size))
        it = 200000 if i == 0 else 1000
        keys = cc.derive(passes[i % len(passes)], salt, it)
        chal = bytes(rnd.getrandbits(8) for _ in range(32))
        cases.append(dict(p=passes[i % len(passes)], salt=salt.hex(), iv=iv.hex(), it=it,
                          plain=base64.b64encode(plain).decode(), chal=chal.hex(),
                          pyblob=base64.b64encode(cc.seal(keys, plain, iv)).decode(),
                          pyans=cc.answer(keys, chal)))
    js = open(os.path.join(HERE, "clipcrypto.js")).read()
    page = """<!doctype html><meta charset=utf-8><pre id=out>running</pre><script>%s
const C=%s;const hex=h=>new Uint8Array(h.match(/../g)||[]).map((_,i)=>parseInt(h.substr(i*2,2),16));
const b64=s=>Uint8Array.from(atob(s),c=>c.charCodeAt(0));const tob64=u=>btoa(String.fromCharCode(...u));
(async()=>{const R=[];for(const c of C){const k=await clipDerive(c.p,hex(c.salt),c.it);
 const mine=await clipSeal(k,b64(c.plain),hex(c.iv));let opened='',err='';
 try{opened=tob64(await clipUnseal(k,b64(c.pyblob)));}catch(e){err=e.message;}
 let wrong='';try{const kb=await clipDerive(c.p+'x',hex(c.salt),c.it);await clipUnseal(kb,mine);}catch(e){wrong=e.message;}
 const raw=await clipDeriveRaw(c.p,hex(c.salt),c.it);const k2=await clipImport(raw);
 const again=tob64(await clipSeal(k2,b64(c.plain),hex(c.iv)));
 R.push({blob:tob64(mine),opened,err,ans:await clipAnswer(k,hex(c.chal)),wrong,again,
  raw:[raw.enc,raw.mac,raw.auth].map(u=>[...u].map(b=>b.toString(16).padStart(2,'0')).join('')).join(':')});}
 fetch('/result',{method:'POST',body:JSON.stringify(R)});})().catch(e=>fetch('/result',{method:'POST',body:'ERR '+e}));
</script>""" % (js, json.dumps(cases))
    # Served from localhost, which browsers treat as secure (WebCrypto needs that), and the page
    # posts its answers back: WebCrypto runs off the page's thread, so a page dump comes too early.
    import http.server, threading
    got = {}
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a): pass
        def do_GET(self):
            b = page.encode(); self.send_response(200); self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
        def do_POST(self):
            got["raw"] = self.rfile.read(int(self.headers["Content-Length"])).decode(); self.send_response(200); self.end_headers()
    srv = http.server.HTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    d = tempfile.mkdtemp()
    ch = subprocess.Popen([chrome, "--headless=new", "--disable-gpu", "--user-data-dir=" + os.path.join(d, "p"),
                           "http://127.0.0.1:%d/" % srv.server_port], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    while "raw" not in got and time.time() - t0 < 90:
        time.sleep(0.2)
    ch.kill(); srv.shutdown(); shutil.rmtree(d, ignore_errors=True)
    raw = got.get("raw", "no answer from the browser")
    try:
        res = json.loads(raw.replace("&quot;", '"').replace("&amp;", "&"))
    except Exception:
        res = None; check(False, "browser said: " + raw[:300])
    if res:
        for c, r in zip(cases, res):
            check(r["blob"] == c["pyblob"], "browser and python seal the same bytes (%d bytes, iv %s)" % (len(base64.b64decode(c["plain"])), c["iv"][-4:]))
            check(r["opened"] == c["plain"] and not r["err"], "browser opens python's item: " + r["err"])
            keys = cc.derive(c["p"], bytes.fromhex(c["salt"]), c["it"])
            check(cc.unseal(keys, base64.b64decode(r["blob"])) == base64.b64decode(c["plain"]), "python opens the browser's item")
            check(r["ans"] == c["pyans"], "the same proof for the board")
            check(bool(r["wrong"]), "browser refuses the wrong passphrase")
            check(r["raw"] == ":".join(x.hex() for x in (keys.enc, keys.mac, keys.auth)), "the browser's raw keys are python's")
            check(r["again"] == c["pyblob"], "keys kept and loaded back seal the same")
        print("browser cases:", len(res))
else:
    print("Chrome not found; browser half skipped")

print("%d passed, %d failed" % (passed, failed))
sys.exit(1 if failed else 0)
