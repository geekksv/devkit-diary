"""The pets, drawn as a rig of smooth shapes — and how each part moves.

Writes the rig as one JSON line into both places that draw it, so they can never drift apart:
the desktop helper (devkit_pet.py, drawn with QPainter) and the phone page (pet_page.h, drawn
on a canvas). Run after changing anything here:  python3 helper/art.py

Coordinates: a 200 x 140 box, ground at y = 130, the pet facing right.
A part is a path ("d", absolute M/L/C/Q/Z only) or an ellipse ("e": cx, cy, rx, ry), with a fill
(a colour, or a linear gradient "lg": x1, y1, x2, y2, [[stop, colour], ...]), an optional stroke,
and optional animations, applied in order around a pivot:
    rot   [amplitude deg, period s, phase rad]   swing (legs, tail, ears)
    ty    [amplitude, period, phase]             bob up and down
    sy    [amplitude, period, phase]             breathe (scale y)
    blink [period s, offset s]                   eyes: squeeze shut for a moment
A part in group "g" also gets that group's animations first (a head that bobs as one), and every
part gets the pose's "all" group.
"""
import json, math, pathlib, re

HERE = pathlib.Path(__file__).resolve().parent


def lg(x1, y1, x2, y2, *stops):
    return {"lg": [x1, y1, x2, y2, [[i / (len(stops) - 1), c] for i, c in enumerate(stops)]]}


def _r(v):
    return int(v) if float(v).is_integer() else round(v, 2)


def _part(fill, kw):
    return dict(kw, **({"fill": fill} if fill else {}))


def ell(cx, cy, rx, ry, fill, **kw):
    return dict(e=[_r(cx), _r(cy), _r(rx), _r(ry)], **_part(fill, kw))


def path(d, fill, **kw):
    d = re.sub(r"-?\d+\.\d+", lambda m: str(_r(float(m.group()))), " ".join(d.split()))
    return dict(d=d, **_part(fill, kw))


def leg(x, top, bottom, w, fill, stroke, paw=None, **kw):
    """A leg hanging from (x, top): a rounded column, slightly narrower at the paw."""
    h, b = w / 2, w / 2 - 1
    d = (f"M {x-h} {top} L {x+h} {top} L {x+b} {bottom-b} "
         f"Q {x+b} {bottom} {x} {bottom} Q {x-b} {bottom} {x-b} {bottom-b} Z")
    out = [path(d, fill, stroke=stroke, sw=1.4, pivot=[x, top + 2], **kw)]
    if paw:
        out.append(ell(x + 1, bottom - 2.5, b + 1.5, 3.4, paw, pivot=[x, top + 2], **kw))
    return out


def front(x, top, bottom, w, fill, stroke, paw, **kw):
    """A front leg: wide at the shoulder, tapering to the paw."""
    h, b, mid = w / 2, w * 0.36, top + (bottom - top) * 0.55
    d = (f"M {x-h} {top} C {x-h} {mid}, {x-b} {bottom-12}, {x-b} {bottom-4} Q {x-b} {bottom} {x} {bottom} "
         f"Q {x+b} {bottom} {x+b} {bottom-4} C {x+b} {bottom-12}, {x+h} {mid}, {x+h} {top} Z")
    return [path(d, fill, stroke=stroke, sw=1.4, pivot=[x, top + 2], **kw),
            ell(x + 1.5, bottom - 2.6, b + 2.2, 3.5, paw, stroke=stroke, sw=0.9, pivot=[x, top + 2], **kw)]


def back(x, top, bottom, w, fill, stroke, paw, **kw):
    """A hind leg: a round thigh, then a slim shin angled back to the hock and down to the paw."""
    b = w * 0.32
    hock = bottom - 13
    d = (f"M {x-b-1} {top+14} C {x-b-3} {top+24}, {x-b-6} {hock-4}, {x-b-5} {hock} "
         f"C {x-b-4} {hock+5}, {x-b-1} {bottom-4}, {x-b} {bottom-3} Q {x-b} {bottom} {x+1} {bottom} "
         f"Q {x+b+1} {bottom} {x+b+1} {bottom-4} C {x+b} {hock+2}, {x+b+1} {hock-6}, {x+b+3} {top+16} Z")
    return [path(d, fill, stroke=stroke, sw=1.4, pivot=[x + 2, top + 4], **kw),
            ell(x + 2, bottom - 2.6, b + 2.6, 3.5, paw, stroke=stroke, sw=0.9, pivot=[x + 2, top + 4], **kw),
            ell(x + 2, top + 9, w * 0.95, w * 1.15, fill, stroke=stroke, sw=1.4, pivot=[x + 2, top + 4], **kw)]


def walk(amp, per, phase):
    return [{"rot": [amp, per, phase]}]


# ── the cat: an orange tabby ─────────────────────────────────────────────────────────────────
C = dict(fur="#ec9a4e", furTop="#e0853a", dark="#c2672a", far="#cf7a36", light="#fbe3c4",
         ear="#f3a7a2", nose="#e9837f", iris="#8db63b", line="#8a4a1d")


def cat_head(cx, cy, g="head", closed=False):
    c = C
    parts = [
        path(f"M {cx-19} {cy-8} L {cx-21} {cy-31} L {cx-4} {cy-17} Z", c["fur"], stroke=c["line"], sw=1.4, g=g),
        path(f"M {cx+3} {cy-17} L {cx+19} {cy-32} L {cx+20} {cy-8} Z", c["fur"], stroke=c["line"], sw=1.4, g=g),
        path(f"M {cx-17} {cy-11} L {cx-18} {cy-25} L {cx-8} {cy-16} Z", c["ear"], g=g),
        path(f"M {cx+6} {cy-16} L {cx+16} {cy-26} L {cx+17} {cy-11} Z", c["ear"], g=g),
        ell(cx, cy, 24, 21, lg(cx, cy - 21, cx, cy + 21, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, g=g),
        # tabby stripes on the forehead
        path(f"M {cx-6} {cy-20} Q {cx-5} {cy-13} {cx-3} {cy-10}", None, stroke=c["dark"], sw=2.2, cap=1, g=g),
        path(f"M {cx} {cy-21} L {cx} {cy-11}", None, stroke=c["dark"], sw=2.2, cap=1, g=g),
        path(f"M {cx+6} {cy-20} Q {cx+5} {cy-13} {cx+3} {cy-10}", None, stroke=c["dark"], sw=2.2, cap=1, g=g),
        ell(cx - 7, cy + 8, 9, 7, c["light"], g=g),
        ell(cx + 7, cy + 8, 9, 7, c["light"], g=g),
    ]
    if closed:
        parts += [path(f"M {cx-14} {cy-1} Q {cx-9} {cy+3} {cx-4} {cy-1}", None, stroke=c["line"], sw=1.8, cap=1, g=g),
                  path(f"M {cx+4} {cy-1} Q {cx+9} {cy+3} {cx+14} {cy-1}", None, stroke=c["line"], sw=1.8, cap=1, g=g)]
    else:
        for ex in (cx - 9, cx + 9):
            parts += [ell(ex, cy - 1, 5.2, 5.8, c["iris"], stroke=c["line"], sw=1, g=g, pivot=[ex, cy - 1], anim=[{"blink": [4.6, 0]}]),
                      ell(ex + 0.4, cy - 1, 1.8, 4.6, "#1b1b1b", g=g, pivot=[ex, cy - 1], anim=[{"blink": [4.6, 0]}]),
                      ell(ex + 1.7, cy - 3.2, 1.3, 1.3, "#ffffff", g=g, pivot=[ex, cy - 1], anim=[{"blink": [4.6, 0]}])]
    parts += [
        path(f"M {cx-3} {cy+4} L {cx+3} {cy+4} L {cx} {cy+7.5} Z", c["nose"], g=g),
        path(f"M {cx} {cy+7.5} Q {cx-1} {cy+11} {cx-5} {cy+10.5} M {cx} {cy+7.5} Q {cx+1} {cy+11} {cx+5} {cy+10.5}",
             None, stroke=c["line"], sw=1.2, cap=1, g=g),
        path(f"M {cx-12} {cy+7} L {cx-30} {cy+3} M {cx-12} {cy+10} L {cx-30} {cy+11} "
             f"M {cx+12} {cy+7} L {cx+30} {cy+3} M {cx+12} {cy+10} L {cx+30} {cy+11}",
             None, stroke="#ffffff", sw=0.9, cap=1, g=g, alpha=0.85),
    ]
    return parts


def cat_tail(base, sway, per=1.8, curl=False):
    c = C
    if curl:
        d = "M 60 124 C 40 126, 34 112, 46 106 C 52 103, 56 108, 52 111 C 46 114, 50 120, 64 119 Z"
    else:
        d = "M 52 80 C 34 80, 20 66, 20 44 C 20 36, 30 34, 31 41 C 31 58, 40 70, 54 72 Z"
    t = [path(d, lg(20, 40, 54, 80, c["dark"], c["fur"]), stroke=c["line"], sw=1.4, pivot=base, anim=[{"rot": [sway, per, 0]}])]
    if not curl:
        t += [path("M 22 50 Q 27 50 30 46", None, stroke=c["dark"], sw=2.4, cap=1, pivot=base, anim=[{"rot": [sway, per, 0]}]),
              path("M 26 62 Q 31 61 33 56", None, stroke=c["dark"], sw=2.4, cap=1, pivot=base, anim=[{"rot": [sway, per, 0]}])]
    return t


def cat_walk():
    c, P = C, 0.62
    body = path("M 46 84 C 46 63, 72 57, 100 58 C 128 59, 150 63, 151 83 C 152 101, 135 108, 100 108 C 70 108, 46 104, 46 84 Z",
                lg(0, 58, 0, 108, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4)
    parts = cat_tail([52, 78], 9)
    parts += front(126, 90, 129, 12, c["far"], c["line"], c["light"], anim=walk(22, P, math.pi))
    parts += back(72, 82, 129, 13, c["far"], c["line"], c["light"], anim=walk(18, P, 0))
    parts += [body,
              path("M 72 104 C 90 110, 118 110, 136 101 C 124 106, 90 107, 72 104 Z", c["light"]),
              path("M 82 60 Q 86 67 83 73", None, stroke=c["dark"], sw=3.2, cap=1),
              path("M 98 59 Q 102 67 99 74", None, stroke=c["dark"], sw=3.2, cap=1),
              path("M 114 60 Q 118 67 115 73", None, stroke=c["dark"], sw=3.2, cap=1),
              ell(141, 86, 12, 17, c["light"])]
    parts += back(58, 80, 129, 14, c["fur"], c["line"], c["light"], anim=walk(18, P, math.pi))
    parts += front(138, 92, 129, 12.5, c["fur"], c["line"], c["light"], anim=walk(22, P, 0))
    parts += cat_head(160, 56)
    return {"groups": {"all": [{"ty": [1.2, P / 2, 0]}], "head": [{"ty": [1.0, P / 2, 1.2]}]}, "parts": parts}


def cat_sit():
    c = C
    parts = [path("M 72 129 C 58 110, 66 80, 94 68 C 116 60, 134 76, 133 98 C 133 118, 124 129, 110 129 Z",
                   lg(0, 66, 0, 129, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, g="body"),
              ell(118, 96, 13, 20, c["light"], g="body"),
              path("M 80 80 Q 86 86 84 93", None, stroke=c["dark"], sw=3.2, cap=1, g="body"),
              path("M 76 95 Q 83 100 81 107", None, stroke=c["dark"], sw=3.2, cap=1, g="body"),
              ell(86, 112, 25, 17, lg(0, 95, 0, 129, c["fur"], c["furTop"]), stroke=c["line"], sw=1.4, g="body")]
    parts += front(112, 98, 129, 11, c["far"], c["line"], c["light"])
    parts += front(124, 98, 129, 11.5, c["fur"], c["line"], c["light"])
    parts += [path("M 66 124 C 86 133, 126 134, 142 125 C 147 121, 144 115, 139 118 C 126 126, 92 127, 72 118 Z",
                   lg(66, 120, 146, 120, c["fur"], c["dark"]), stroke=c["line"], sw=1.3, pivot=[70, 122], anim=[{"rot": [1.5, 2.6, 0]}]),
              path("M 128 124 Q 131 128 129 131", None, stroke=c["dark"], sw=2.2, cap=1, pivot=[70, 122], anim=[{"rot": [1.5, 2.6, 0]}])]
    parts += cat_head(118, 50)
    return {"groups": {"all": [], "body": [{"sy": [0.012, 3.2, 0]}], "head": [{"ty": [0.8, 3.2, 0.6]}]}, "parts": parts}


def cat_sleep():
    c = C
    parts = [path("M 38 116 C 36 94, 62 84, 100 84 C 138 84, 160 96, 160 114 C 160 126, 140 130, 100 130 C 60 130, 38 128, 38 116 Z",
                  lg(0, 84, 0, 130, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, g="body"),
             path("M 70 88 Q 74 95 71 101", None, stroke=c["dark"], sw=3.2, cap=1, g="body"),
             path("M 88 85 Q 92 93 89 100", None, stroke=c["dark"], sw=3.2, cap=1, g="body"),
             path("M 106 86 Q 110 93 107 100", None, stroke=c["dark"], sw=3.2, cap=1, g="body"),
             path("M 46 124 C 70 132, 140 134, 166 124 C 168 120, 162 117, 158 120 C 136 128, 76 127, 52 119 Z",
                  lg(46, 120, 166, 120, c["fur"], c["dark"]), stroke=c["line"], sw=1.2, g="body"),
             ell(150, 124, 10, 5, c["light"], stroke=c["line"], sw=1)]
    parts += cat_head(150, 106, closed=True)
    return {"groups": {"all": [], "body": [{"sy": [0.022, 3.6, 0]}], "head": [{"ty": [0.9, 3.6, 0]}]}, "parts": parts}


# ── the dog: a golden pup with floppy ears ───────────────────────────────────────────────────
D = dict(fur="#dda25f", furTop="#c98a47", far="#bd8143", light="#f6e2c0", ear="#9a6233", earIn="#b57a45",
         nose="#2a201c", tongue="#ef7d8b", iris="#5a3a22", line="#7a4a22")


def dog_head(cx, cy, g="head", closed=False, tongue=True):
    c = D
    parts = [
        ell(cx, cy, 22, 20, lg(cx, cy - 20, cx, cy + 20, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, g=g),
        ell(cx + 2, cy + 10, 14, 10.5, c["light"], stroke=c["line"], sw=1.2, g=g),
        ell(cx + 2, cy + 4, 5.8, 4.2, c["nose"], g=g),
        ell(cx + 3.6, cy + 2.6, 1.6, 1, "#ffffff", g=g, alpha=0.6),
        path(f"M {cx+2} {cy+8} L {cx+2} {cy+12} M {cx+2} {cy+12} Q {cx-3} {cy+16} {cx-7} {cy+13} "
             f"M {cx+2} {cy+12} Q {cx+7} {cy+16} {cx+11} {cy+13}", None, stroke=c["line"], sw=1.3, cap=1, g=g),
    ]
    if tongue and not closed:
        parts.append(path(f"M {cx-2} {cy+14} Q {cx+2} {cy+24} {cx+6} {cy+14} Z", c["tongue"], stroke="#c45567", sw=1, g=g))
    if closed:
        parts += [path(f"M {cx-13} {cy-3} Q {cx-8} {cy+1} {cx-3} {cy-3}", None, stroke=c["line"], sw=1.8, cap=1, g=g),
                  path(f"M {cx+5} {cy-3} Q {cx+10} {cy+1} {cx+15} {cy-3}", None, stroke=c["line"], sw=1.8, cap=1, g=g)]
    else:
        for ex in (cx - 8, cx + 10):
            parts += [ell(ex, cy - 4, 4.4, 4.8, c["iris"], g=g, pivot=[ex, cy - 4], anim=[{"blink": [5.2, 1.3]}]),
                      ell(ex + 1.4, cy - 5.6, 1.4, 1.4, "#ffffff", g=g, pivot=[ex, cy - 4], anim=[{"blink": [5.2, 1.3]}])]
    # floppy ears, drawn over the head's sides, swinging a little
    parts += [path(f"M {cx-14} {cy-16} C {cx-30} {cy-14}, {cx-32} {cy+8}, {cx-24} {cy+16} C {cx-18} {cy+20}, {cx-14} {cy+6}, {cx-12} {cy-6} Z",
                   lg(cx - 30, cy - 16, cx - 14, cy + 18, c["ear"], c["earIn"]), stroke=c["line"], sw=1.3, g=g,
                   pivot=[cx - 14, cy - 14], anim=[{"rot": [5, 1.4, 0]}]),
              path(f"M {cx+16} {cy-16} C {cx+32} {cy-14}, {cx+34} {cy+8}, {cx+26} {cy+16} C {cx+20} {cy+20}, {cx+16} {cy+6}, {cx+14} {cy-6} Z",
                   lg(cx + 14, cy - 16, cx + 32, cy + 18, c["earIn"], c["ear"]), stroke=c["line"], sw=1.3, g=g,
                   pivot=[cx + 16, cy - 14], anim=[{"rot": [5, 1.4, 1.5]}])]
    return parts


def dog_tail(base, amp, per):
    c = D
    return [path("M 50 74 C 40 66, 32 50, 36 36 C 38 30, 46 31, 44 38 C 42 50, 48 62, 56 68 Z",
                 lg(36, 34, 56, 74, c["far"], c["fur"]), stroke=c["line"], sw=1.4, pivot=base, anim=[{"rot": [amp, per, 0]}])]


def dog_walk():
    c, P = D, 0.56
    parts = dog_tail([52, 72], 18, 0.42)
    parts += front(128, 88, 129, 14, c["far"], c["line"], c["light"], anim=walk(24, P, math.pi))
    parts += back(70, 80, 129, 15, c["far"], c["line"], c["light"], anim=walk(20, P, 0))
    parts += [path("M 44 82 C 44 62, 70 56, 100 57 C 130 58, 150 62, 152 82 C 153 100, 136 106, 100 106 C 68 106, 44 102, 44 82 Z",
                   lg(0, 57, 0, 106, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4),
              path("M 70 102 C 90 108, 118 108, 138 99 C 124 104, 90 105, 70 102 Z", c["light"]),
              ell(142, 84, 12, 16, c["light"])]
    parts += back(56, 78, 129, 16, c["fur"], c["line"], c["light"], anim=walk(20, P, math.pi))
    parts += front(140, 90, 129, 14.5, c["fur"], c["line"], c["light"], anim=walk(24, P, 0))
    parts += dog_head(160, 56)
    return {"groups": {"all": [{"ty": [1.4, P / 2, 0]}], "head": [{"ty": [1.2, P / 2, 1.2]}]}, "parts": parts}


def dog_sit():
    c = D
    parts = [path("M 60 126 C 44 122, 40 108, 50 104 C 56 101, 60 108, 56 112 C 54 117, 60 120, 68 120 Z",
                  lg(40, 100, 68, 126, c["far"], c["fur"]), stroke=c["line"], sw=1.3, pivot=[64, 122], anim=[{"rot": [10, 0.6, 0]}]),
             path("M 70 129 C 56 110, 64 78, 94 68 C 118 60, 136 76, 135 98 C 135 118, 126 129, 112 129 Z",
                  lg(0, 66, 0, 129, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, g="body"),
             ell(120, 94, 13, 21, c["light"], g="body"),
             ell(86, 112, 26, 17, lg(0, 95, 0, 129, c["fur"], c["furTop"]), stroke=c["line"], sw=1.4, g="body")]
    parts += front(113, 96, 129, 13, c["far"], c["line"], c["light"])
    parts += front(126, 96, 129, 13.5, c["fur"], c["line"], c["light"])
    parts += dog_head(120, 50)
    return {"groups": {"all": [], "body": [{"sy": [0.014, 2.4, 0]}], "head": [{"ty": [1.0, 2.4, 0.6]}]}, "parts": parts}


def dog_sleep():
    c = D
    parts = [path("M 36 116 C 34 94, 62 84, 100 84 C 138 84, 160 96, 160 114 C 160 126, 140 130, 100 130 C 60 130, 36 128, 36 116 Z",
                  lg(0, 84, 0, 130, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, g="body"),
             path("M 40 124 C 26 127, 16 121, 20 114 C 23 109, 30 115, 42 116 Z", lg(16, 112, 42, 124, c["far"], c["fur"]), stroke=c["line"], sw=1.2,
                  pivot=[40, 120], anim=[{"rot": [3, 3.6, 0]}]),
             ell(146, 125, 14, 5, c["light"], stroke=c["line"], sw=1.1),
             ell(124, 125, 12, 5, c["light"], stroke=c["line"], sw=1.1)]
    parts += dog_head(146, 104, closed=True)
    return {"groups": {"all": [], "body": [{"sy": [0.022, 3.6, 0]}], "head": [{"ty": [0.9, 3.6, 0]}]}, "parts": parts}


RIG = {"box": [200, 140], "pets": {
    "cat": {"walk": cat_walk(), "sit": cat_sit(), "sleep": cat_sleep()},
    "dog": {"walk": dog_walk(), "sit": dog_sit(), "sleep": dog_sleep()},
}}


def write():
    line = json.dumps(RIG, separators=(",", ":"))
    targets = [(HERE / "devkit_pet.py", r"^RIG = .*$", "RIG = json.loads(r\'\'\'" + line + "\'\'\')"),
               (HERE.parent / "src/pet/pet_page.h", r"^const RIG=.*;$", "const RIG=" + line + ";")]
    for f, pat, new in targets:
        t = f.read_text()
        t2, n = re.subn(pat, lambda _: new, t, flags=re.M)
        if n != 1:
            raise SystemExit("no RIG line in %s" % f)
        f.write_text(t2)
    print("rig: %d bytes, written to the helper and the page" % len(line))


if __name__ == "__main__":
    import sys
    if "--json" in sys.argv:
        print(json.dumps(RIG, separators=(",", ":")))
    else:
        write()
