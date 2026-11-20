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


def back(x, top, bottom, w, fill, stroke, paw, thigh=None, **kw):
    """A hind leg: a round thigh, then a slim shin angled back to the hock and down to the paw."""
    b = w * 0.32
    hock = bottom - 13
    d = (f"M {x-b-1} {top+14} C {x-b-3} {top+24}, {x-b-6} {hock-4}, {x-b-5} {hock} "
         f"C {x-b-4} {hock+5}, {x-b-1} {bottom-4}, {x-b} {bottom-3} Q {x-b} {bottom} {x+1} {bottom} "
         f"Q {x+b+1} {bottom} {x+b+1} {bottom-4} C {x+b} {hock+2}, {x+b+1} {hock-6}, {x+b+3} {top+16} Z")
    return [path(d, fill, stroke=stroke, sw=1.4, pivot=[x + 2, top + 4], **kw),
            ell(x + 2, bottom - 2.6, b + 2.6, 3.5, paw, stroke=stroke, sw=0.9, pivot=[x + 2, top + 4], **kw),
            ell(x + 2, top + 9, w * 0.95, w * 1.15, thigh or fill, stroke=stroke, sw=1.4, pivot=[x + 2, top + 4], **kw)]


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



# ── the other three share one four-legged body; each has its own head, ears, tail and colours ──

def body_walk(c, P, legs, tail, head, amp=22):
    """A walking four-legged body. legs: (far, near) colour for the legs; tail/head: part lists."""
    far, near = legs
    paw = c.get("paw", c["light"])
    parts = list(tail)
    parts += front(126, 90, 129, 12, far, c["line"], paw, anim=walk(amp, P, math.pi))
    thighs = c.get("thighs", (None, None))
    parts += back(72, 82, 129, 13, far, c["line"], paw, thigh=thighs[0], anim=walk(amp * 0.8, P, 0))
    parts += [path("M 46 84 C 46 63, 72 57, 100 58 C 128 59, 150 63, 151 83 C 152 101, 135 108, 100 108 C 70 108, 46 104, 46 84 Z",
                   lg(0, 58, 0, 108, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4),
              path("M 72 104 C 90 110, 118 110, 136 101 C 124 106, 90 107, 72 104 Z", c["light"]),
              ell(141, 86, 12, 17, c["light"])]
    parts += c.get("bodyMarks", [])
    parts += back(58, 80, 129, 14, near, c["line"], paw, thigh=thighs[1], anim=walk(amp * 0.8, P, math.pi))
    parts += front(138, 92, 129, 12.5, near, c["line"], paw, anim=walk(amp, P, 0))
    parts += head
    return {"groups": {"all": [{"ty": [1.2, P / 2, 0]}], "head": [{"ty": [1.0, P / 2, 1.2]}]}, "parts": parts}


def body_sit(c, legs, tail, head, tail_front=False):
    far, near = legs
    paw = c.get("paw", c["light"])
    parts = [] if tail_front else list(tail)
    parts += [path("M 72 129 C 58 110, 66 80, 94 68 C 116 60, 134 76, 133 98 C 133 118, 124 129, 110 129 Z",
                   lg(0, 66, 0, 129, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, g="body"),
              ell(118, 96, 13, 20, c["light"], g="body")]
    parts += c.get("sitMarks", [])
    parts += [ell(86, 112, 25, 17, lg(0, 95, 0, 129, c.get("haunch", c["fur"]), c.get("haunch2", c["furTop"])),
                  stroke=c["line"], sw=1.4, g="body")]
    parts += front(112, 98, 129, 11, far, c["line"], paw)
    parts += front(124, 98, 129, 11.5, near, c["line"], paw)
    if tail_front:
        parts += list(tail)
    parts += head
    return {"groups": {"all": [], "body": [{"sy": [0.012, 3.2, 0]}], "head": [{"ty": [0.8, 3.2, 0.6]}]}, "parts": parts}


def body_sleep(c, tail, head):
    parts = [path("M 38 116 C 36 94, 62 84, 100 84 C 138 84, 160 96, 160 114 C 160 126, 140 130, 100 130 C 60 130, 38 128, 38 116 Z",
                  lg(0, 84, 0, 130, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, g="body")]
    parts += c.get("sleepMarks", [])
    parts += list(tail)
    parts += [ell(150, 125, 11, 5, c.get("paw", c["light"]), stroke=c["line"], sw=1)]
    parts += head
    return {"groups": {"all": [], "body": [{"sy": [0.022, 3.6, 0]}], "head": [{"ty": [0.9, 3.6, 0]}]}, "parts": parts}


def eyes(cx, cy, dx, iris, rx, ry, closed, line, g, blink):
    if closed:
        return [path(f"M {cx-dx-5} {cy} Q {cx-dx} {cy+4} {cx-dx+5} {cy}", None, stroke=line, sw=1.8, cap=1, g=g),
                path(f"M {cx+dx-5} {cy} Q {cx+dx} {cy+4} {cx+dx+5} {cy}", None, stroke=line, sw=1.8, cap=1, g=g)]
    out = []
    for ex in (cx - dx, cx + dx):
        out += [ell(ex, cy, rx, ry, iris, g=g, pivot=[ex, cy], anim=[{"blink": blink}]),
                ell(ex + rx * 0.35, cy - ry * 0.35, rx * 0.32, rx * 0.32, "#ffffff", g=g, pivot=[ex, cy], anim=[{"blink": blink}])]
    return out


# the bunny: soft grey, long ears that twitch, a white pom-pom tail
B = dict(fur="#cfc6bd", furTop="#bdb2a7", far="#b3a89c", light="#f7f2ec", line="#6e6158", ear="#f1b3b6")


def bunny_head(cx, cy, g="head", closed=False):
    c = B
    ears = []
    for side, ph in ((-1, 0), (1, 1.7)):
        x = cx + side * 7
        ears += [path(f"M {x-5} {cy-12} C {x-9} {cy-34}, {x-5} {cy-50}, {x} {cy-50} C {x+5} {cy-50}, {x+8} {cy-34}, {x+5} {cy-12} Z",
                      lg(x, cy - 50, x, cy - 12, c["furTop"], c["fur"]), stroke=c["line"], sw=1.3, g=g,
                      pivot=[x, cy - 14], anim=[{"rot": [4, 2.2, ph]}]),
                 path(f"M {x-2.5} {cy-15} C {x-5} {cy-32}, {x-2} {cy-45}, {x} {cy-45} C {x+2} {cy-45}, {x+5} {cy-32}, {x+2.5} {cy-15} Z",
                      c["ear"], g=g, pivot=[x, cy - 14], anim=[{"rot": [4, 2.2, ph]}])]
    parts = ears + [ell(cx, cy, 21, 19, lg(cx, cy - 19, cx, cy + 19, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, g=g),
                    ell(cx - 6, cy + 8, 8, 6.5, c["light"], g=g), ell(cx + 6, cy + 8, 8, 6.5, c["light"], g=g)]
    parts += eyes(cx, cy - 1, 9, "#2a2220", 4.4, 5, closed, c["line"], g, [5.0, 2.1])
    parts += [path(f"M {cx-2.6} {cy+4} L {cx+2.6} {cy+4} L {cx} {cy+6.5} Z", "#e98d98", g=g),
              path(f"M {cx} {cy+6.5} L {cx} {cy+9} M {cx} {cy+9} Q {cx-2} {cy+11} {cx-4} {cy+10} M {cx} {cy+9} Q {cx+2} {cy+11} {cx+4} {cy+10}",
                   None, stroke=c["line"], sw=1.1, cap=1, g=g),
              path(f"M {cx-10} {cy+7} L {cx-26} {cy+4} M {cx-10} {cy+10} L {cx-26} {cy+11} "
                   f"M {cx+10} {cy+7} L {cx+26} {cy+4} M {cx+10} {cy+10} L {cx+26} {cy+11}",
                   None, stroke="#ffffff", sw=0.8, cap=1, g=g, alpha=0.9)]
    return parts


def bunny_tail(x, y, per=1.6):
    return [ell(x, y, 9, 8.5, B["light"], stroke=B["line"], sw=1.2, pivot=[x + 6, y], anim=[{"rot": [6, per, 0]}])]


# the fox: russet with a white chest, dark socks and ear tips, a big bushy white-tipped tail
F = dict(fur="#e2702f", furTop="#d05d1f", far="#c9611f", light="#fff3e6", line="#7a3412", paw="#3a2a24",
         sock="#4a3328", sockFar="#3b2920", thighs=("#c9611f", "#e2702f"))


def fox_head(cx, cy, g="head", closed=False):
    c = F
    parts = []
    for x0, x1, tip in ((cx - 20, cx - 4, cx - 18), (cx + 4, cx + 20, cx + 18)):
        top = cy - 36
        parts += [path(f"M {x0} {cy-8} L {tip} {top} L {x1} {cy-14} Z", c["fur"], stroke=c["line"], sw=1.3, g=g),
                  path(f"M {(x0+tip)/2+1} {(cy-8+top)/2} L {tip} {top} L {(x1+tip)/2-1} {(cy-14+top)/2} Z", "#3a2a24", g=g),
                  path(f"M {x0+4} {cy-10} L {tip+0.5*(1 if tip < cx else -1)} {top+12} L {x1-3} {cy-14} Z", c["light"], g=g, alpha=0.9)]
    parts += [ell(cx, cy, 22, 19, lg(cx, cy - 19, cx, cy + 19, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, g=g),
              path(f"M {cx-21} {cy+2} C {cx-14} {cy+20}, {cx+14} {cy+20}, {cx+21} {cy+2} C {cx+12} {cy+8}, {cx+4} {cy+6}, {cx} {cy+3} "
                   f"C {cx-4} {cy+6}, {cx-12} {cy+8}, {cx-21} {cy+2} Z", c["light"], g=g)]
    parts += eyes(cx, cy - 2, 9, "#b8731d", 3.8, 4.4, closed, c["line"], g, [4.8, 0.6])
    if not closed:
        parts += [ell(cx - 9, cy - 2, 1.2, 3.4, "#1b1b1b", g=g, pivot=[cx - 9, cy - 2], anim=[{"blink": [4.8, 0.6]}]),
                  ell(cx + 9, cy - 2, 1.2, 3.4, "#1b1b1b", g=g, pivot=[cx + 9, cy - 2], anim=[{"blink": [4.8, 0.6]}])]
    parts += [ell(cx, cy + 6, 3.6, 2.8, "#231a16", g=g),
              path(f"M {cx} {cy+8.8} Q {cx-3} {cy+12} {cx-6} {cy+11} M {cx} {cy+8.8} Q {cx+3} {cy+12} {cx+6} {cy+11}",
                   None, stroke=c["line"], sw=1.1, cap=1, g=g)]
    return parts


def fox_tail(pose):
    c = F
    if pose == "sit":
        d = "M 70 122 C 92 136, 132 136, 150 124 C 156 118, 150 110, 142 114 C 126 124, 94 124, 76 112 Z"
        tip = ell(147, 119, 8, 6.5, c["light"], pivot=[72, 118], anim=[{"rot": [1.5, 2.6, 0]}])
        return [path(d, lg(70, 118, 150, 118, c["fur"], c["furTop"]), stroke=c["line"], sw=1.3, pivot=[72, 118],
                     anim=[{"rot": [1.5, 2.6, 0]}]), tip]
    if pose == "sleep":
        d = "M 44 120 C 60 136, 130 138, 160 124 C 164 118, 158 112, 150 116 C 126 126, 70 126, 50 110 Z"
        return [path(d, lg(44, 118, 160, 118, c["fur"], c["furTop"]), stroke=c["line"], sw=1.3, g="body"),
                ell(156, 119, 8, 6, c["light"], g="body")]
    d = "M 50 80 C 30 84, 8 78, 4 62 C 2 52, 12 46, 22 52 C 30 58, 38 66, 52 70 Z"
    return [path(d, lg(4, 56, 52, 80, c["furTop"], c["fur"]), stroke=c["line"], sw=1.4, pivot=[50, 74], anim=[{"rot": [7, 1.6, 0]}]),
            path("M 13 50 C 6 50, 2 56, 4 62 C 6 66, 10 60, 14 56 Z", c["light"], pivot=[50, 74], anim=[{"rot": [7, 1.6, 0]}])]


# the panda: white with black legs, a black band over the shoulders, black ears and eye patches
N = dict(fur="#f6f4ef", furTop="#e9e6df", far="#2b2b2e", light="#ffffff", line="#3a3a3f", paw="#1f1f22",
         haunch="#2f2f33", haunch2="#242427")
N["bodyMarks"] = [path("M 118 60 C 128 58, 142 62, 148 72 C 150 82, 146 96, 140 104 C 132 100, 126 86, 118 60 Z", "#2b2b2e")]
N["sitMarks"] = [path("M 98 70 C 112 64, 130 72, 132 90 C 132 98, 128 102, 122 100 C 118 88, 108 78, 98 74 Z", "#2b2b2e", g="body")]


def panda_head(cx, cy, g="head", closed=False):
    c = N
    parts = [ell(cx - 16, cy - 15, 7.5, 7, "#1f1f22", g=g), ell(cx + 16, cy - 15, 7.5, 7, "#1f1f22", g=g),
             ell(cx, cy, 23, 20, lg(cx, cy - 20, cx, cy + 20, "#ffffff", c["furTop"]), stroke=c["line"], sw=1.4, g=g),
             path(f"M {cx-14} {cy-8} C {cx-4} {cy-10}, {cx-2} {cy+2}, {cx-6} {cy+5} C {cx-12} {cy+8}, {cx-18} {cy+2}, {cx-14} {cy-8} Z",
                  "#26262a", g=g),
             path(f"M {cx+14} {cy-8} C {cx+4} {cy-10}, {cx+2} {cy+2}, {cx+6} {cy+5} C {cx+12} {cy+8}, {cx+18} {cy+2}, {cx+14} {cy-8} Z",
                  "#26262a", g=g)]
    if closed:
        parts += [path(f"M {cx-12} {cy-1} Q {cx-8} {cy+2} {cx-5} {cy-1}", None, stroke="#ffffff", sw=1.5, cap=1, g=g),
                  path(f"M {cx+5} {cy-1} Q {cx+8} {cy+2} {cx+12} {cy-1}", None, stroke="#ffffff", sw=1.5, cap=1, g=g)]
    else:
        for ex in (cx - 8.5, cx + 8.5):
            parts += [ell(ex, cy - 1, 3.2, 3.4, "#ffffff", g=g, pivot=[ex, cy - 1], anim=[{"blink": [5.4, 3.0]}]),
                      ell(ex + 0.4, cy - 0.8, 1.8, 2.2, "#121214", g=g, pivot=[ex, cy - 1], anim=[{"blink": [5.4, 3.0]}])]
    parts += [ell(cx, cy + 7, 4, 2.8, "#1f1f22", g=g),
              path(f"M {cx} {cy+9.6} L {cx} {cy+12} M {cx} {cy+12} Q {cx-3} {cy+14} {cx-5} {cy+12.5} "
                   f"M {cx} {cy+12} Q {cx+3} {cy+14} {cx+5} {cy+12.5}", None, stroke=c["line"], sw=1.1, cap=1, g=g)]
    return parts


def panda_tail(x, y):
    return [ell(x, y, 7, 6, "#ffffff", stroke=N["line"], sw=1.1, pivot=[x + 4, y], anim=[{"rot": [5, 2.0, 0]}])]


def shadowed(pose, wide=False):
    """A soft shadow under the pet, so it stands on the screen rather than floating over it."""
    pose["parts"].insert(0, ell(100, 131, 66 if wide else 56, 4.5, "#000000", alpha=0.16))
    return pose


B["paw"] = B["light"]

RIG = {"box": [200, 140], "pets": {
    "cat": {"walk": cat_walk(), "sit": cat_sit(), "sleep": cat_sleep()},
    "dog": {"walk": dog_walk(), "sit": dog_sit(), "sleep": dog_sleep()},
    "bunny": {"walk": body_walk(B, 0.5, (B["far"], B["fur"]), bunny_tail(48, 80), bunny_head(158, 60), amp=26),
              "sit": body_sit(B, (B["far"], B["fur"]), bunny_tail(72, 120), bunny_head(118, 54)),
              "sleep": body_sleep(B, bunny_tail(42, 116), bunny_head(150, 108, closed=True))},
    "fox": {"walk": body_walk(F, 0.6, (F["sockFar"], F["sock"]), fox_tail("walk"), fox_head(160, 58)),
            "sit": body_sit(F, (F["sockFar"], F["sock"]), fox_tail("sit"), fox_head(118, 52), tail_front=True),
            "sleep": body_sleep(F, fox_tail("sleep"), fox_head(150, 108, closed=True))},
    "panda": {"walk": body_walk(N, 0.7, (N["far"], "#2f2f33"), panda_tail(48, 78), panda_head(160, 58), amp=18),
              "sit": body_sit(N, (N["far"], "#2f2f33"), panda_tail(66, 122), panda_head(118, 52)),
              "sleep": body_sleep(N, panda_tail(40, 116), panda_head(150, 108, closed=True))},
}}
for _kind in RIG["pets"].values():
    for _name, _pose in _kind.items():
        shadowed(_pose, wide=_name == "sleep")


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
