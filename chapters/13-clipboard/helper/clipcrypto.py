"""End-to-end encryption for the shared clipboard, in plain Python (3.8+, standard library only).

The same format is made by the page with WebCrypto, so a phone and a computer read each other's
items byte for byte. The board only ever holds sealed blobs.

    keys    master = PBKDF2-HMAC-SHA256(passphrase (NFC, UTF-8), b"devkit-clip/1" + group salt,
                                        iterations, 32 bytes)
            enc  = HMAC-SHA256(master, b"enc")    encrypts
            mac  = HMAC-SHA256(master, b"mac")    seals
            auth = HMAC-SHA256(master, b"auth")   proves to the board that a device is one of yours

    a blob  0x01 | iv (16) | AES-256-CTR(enc, iv, plaintext) | HMAC-SHA256(mac, 0x01 | iv | ciphertext)

CTR counts in the low 64 bits of the iv and wraps there, without carrying into the high half:
that is what WebCrypto does with AES-CTR and `length: 64`.

Python's standard library has hashlib and hmac but no AES, so AES-256 is here, table-driven. It
is checked against FIPS-197 and SP 800-38A and against the browser (tools/crypto_test.py). It
only ever encrypts (CTR needs no decryption), and it is not constant-time: fine for a clipboard
on your own computer, which is where it runs.
"""

import hashlib
import hmac
import os
import unicodedata

VERSION = 1
ITERATIONS = 200000
SALT_PREFIX = b"devkit-clip/1"


# ── AES-256, encryption only ────────────────────────────────────────────────────────────────

def _tables():
    sbox = [0] * 256
    p = q = 1
    while True:                              # walk the multiplicative group of GF(2^8)
        p = p ^ ((p << 1) & 0xFF) ^ (0x1B if p & 0x80 else 0)
        q ^= q << 1
        q ^= q << 2
        q ^= q << 4
        q &= 0xFF
        if q & 0x80:
            q ^= 0x09
        x = q ^ ((q << 1) | (q >> 7)) ^ ((q << 2) | (q >> 6)) ^ ((q << 3) | (q >> 5)) ^ ((q << 4) | (q >> 4))
        sbox[p] = (x ^ 0x63) & 0xFF
        if p == 1:
            break
    sbox[0] = 0x63

    def xt(a):
        return ((a << 1) ^ 0x1B) & 0xFF if a & 0x80 else a << 1

    t0, t1, t2, t3 = [0] * 256, [0] * 256, [0] * 256, [0] * 256
    for i in range(256):
        s = sbox[i]
        s2 = xt(s)
        s3 = s2 ^ s
        w = (s2 << 24) | (s << 16) | (s << 8) | s3
        t0[i] = w
        t1[i] = ((w >> 8) | (w << 24)) & 0xFFFFFFFF
        t2[i] = ((w >> 16) | (w << 16)) & 0xFFFFFFFF
        t3[i] = ((w >> 24) | (w << 8)) & 0xFFFFFFFF
    return sbox, t0, t1, t2, t3


_SBOX, _T0, _T1, _T2, _T3 = _tables()


def _expand(key):
    """The 60-word AES-256 key schedule."""
    if len(key) != 32:
        raise ValueError("AES-256 needs a 32-byte key")
    w = [int.from_bytes(key[i:i + 4], "big") for i in range(0, 32, 4)]
    rcon = 1
    s = _SBOX
    for i in range(8, 60):
        t = w[i - 1]
        if i % 8 == 0:
            t = ((t << 8) | (t >> 24)) & 0xFFFFFFFF
            t = (s[t >> 24] << 24) | (s[(t >> 16) & 255] << 16) | (s[(t >> 8) & 255] << 8) | s[t & 255]
            t ^= rcon << 24
            rcon = ((rcon << 1) ^ 0x1B) & 0xFF if rcon & 0x80 else rcon << 1
        elif i % 8 == 4:
            t = (s[t >> 24] << 24) | (s[(t >> 16) & 255] << 16) | (s[(t >> 8) & 255] << 8) | s[t & 255]
        w.append(w[i - 8] ^ t)
    return w


def _encrypt_block(rk, block):
    """One 16-byte block, as a 128-bit integer in and out."""
    t0, t1, t2, t3, s = _T0, _T1, _T2, _T3, _SBOX
    a = (block >> 96) ^ rk[0]
    b = ((block >> 64) & 0xFFFFFFFF) ^ rk[1]
    c = ((block >> 32) & 0xFFFFFFFF) ^ rk[2]
    d = (block & 0xFFFFFFFF) ^ rk[3]
    k = 4
    for _ in range(13):
        a, b, c, d = (
            t0[a >> 24] ^ t1[(b >> 16) & 255] ^ t2[(c >> 8) & 255] ^ t3[d & 255] ^ rk[k],
            t0[b >> 24] ^ t1[(c >> 16) & 255] ^ t2[(d >> 8) & 255] ^ t3[a & 255] ^ rk[k + 1],
            t0[c >> 24] ^ t1[(d >> 16) & 255] ^ t2[(a >> 8) & 255] ^ t3[b & 255] ^ rk[k + 2],
            t0[d >> 24] ^ t1[(a >> 16) & 255] ^ t2[(b >> 8) & 255] ^ t3[c & 255] ^ rk[k + 3],
        )
        k += 4
    a, b, c, d = (
        ((s[a >> 24] << 24) | (s[(b >> 16) & 255] << 16) | (s[(c >> 8) & 255] << 8) | s[d & 255]) ^ rk[56],
        ((s[b >> 24] << 24) | (s[(c >> 16) & 255] << 16) | (s[(d >> 8) & 255] << 8) | s[a & 255]) ^ rk[57],
        ((s[c >> 24] << 24) | (s[(d >> 16) & 255] << 16) | (s[(a >> 8) & 255] << 8) | s[b & 255]) ^ rk[58],
        ((s[d >> 24] << 24) | (s[(a >> 16) & 255] << 16) | (s[(b >> 8) & 255] << 8) | s[c & 255]) ^ rk[59],
    )
    return (a << 96) | (b << 64) | (c << 32) | d


def aes256_block(key, block16):
    """Encrypts one block (bytes). For the test vectors."""
    out = _encrypt_block(_expand(key), int.from_bytes(block16, "big"))
    return out.to_bytes(16, "big")


def aes256_ctr(key, iv, data):
    """AES-256-CTR with a 64-bit counter in the low half of the iv (WebCrypto's `length: 64`)."""
    if len(iv) != 16:
        raise ValueError("the iv is 16 bytes")
    rk = _expand(key)
    high = int.from_bytes(iv[:8], "big") << 64
    low = int.from_bytes(iv[8:], "big")
    n = len(data)
    blocks = (n + 15) // 16
    stream = bytearray(blocks * 16)
    for i in range(blocks):
        ks = _encrypt_block(rk, high | ((low + i) & 0xFFFFFFFFFFFFFFFF))
        stream[i * 16:i * 16 + 16] = ks.to_bytes(16, "big")
    x = int.from_bytes(data, "big") ^ int.from_bytes(stream[:n], "big") if n else 0
    return x.to_bytes(n, "big") if n else b""


# ── keys, sealing, opening ──────────────────────────────────────────────────────────────────

class Keys:
    def __init__(self, enc, mac, auth):
        self.enc, self.mac, self.auth = enc, mac, auth


def derive(passphrase, group_salt, iterations=ITERATIONS):
    """The three keys from the passphrase and the board's group salt (bytes)."""
    pw = unicodedata.normalize("NFC", passphrase).encode("utf-8")
    master = hashlib.pbkdf2_hmac("sha256", pw, SALT_PREFIX + group_salt, iterations, 32)
    sub = lambda label: hmac.new(master, label, hashlib.sha256).digest()
    return Keys(sub(b"enc"), sub(b"mac"), sub(b"auth"))


def seal(keys, plaintext, iv=None):
    iv = iv if iv is not None else os.urandom(16)
    head = bytes([VERSION]) + iv
    body = head + aes256_ctr(keys.enc, iv, plaintext)
    return body + hmac.new(keys.mac, body, hashlib.sha256).digest()


class Tampered(Exception):
    """The blob was changed, or was sealed with another passphrase."""


def unseal(keys, blob):
    if len(blob) < 1 + 16 + 32 or blob[0] != VERSION:
        raise Tampered("not a clipboard item")
    body, tag = blob[:-32], blob[-32:]
    if not hmac.compare_digest(tag, hmac.new(keys.mac, body, hashlib.sha256).digest()):
        raise Tampered("wrong passphrase, or the item was changed")
    return aes256_ctr(keys.enc, blob[1:17], blob[17:-32])


def answer(keys, challenge):
    """Proof for the board that this device knows the passphrase, without sending it."""
    return hmac.new(keys.auth, challenge, hashlib.sha256).hexdigest()
