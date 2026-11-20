"""The pets, drawn as naturalistic illustrations — and how each part moves.

Five animals drawn in side profile with real proportions: a domestic cat (orange tabby), a dog
(golden retriever), a rabbit (wild brown), a red fox and a giant panda. Each has jointed legs
(shoulder–elbow–wrist, hip–stifle–hock), shading rather than outlines, and its own markings.

Writes the rig as one JSON line into both places that draw it, so they can never drift apart:
the desktop helper (devkit_pet.py, drawn with QPainter) and the phone page (pet_page.h, drawn on
a canvas). Run after changing anything here:  python3 helper/art.py

Coordinates: a 200 x 140 box, ground at y = 130, the animal facing right.
A part is a path ("d", absolute M/L/C/Q/Z only) or an ellipse ("e": cx, cy, rx, ry), with a fill
(a colour, or a linear gradient "lg": x1, y1, x2, y2, [[stop, colour], ...]), an optional stroke,
an optional "alpha", and optional animation steps applied in order — each about its own joint
"at" (or the part's "pivot"), so a lower leg can swing with the upper leg and then bend:
    rot   [amplitude deg, period s, phase rad]
    ty    [amplitude, period, phase]             bob
    sy    [amplitude, period, phase]             breathe (scale y about the ground)
    blink [period s, offset s]                   eyes: squeeze shut for a moment
Parts in group "g" also get that group's steps first, and every part gets the pose's "all" group.
"""
import json, math, pathlib, re

HERE = pathlib.Path(__file__).resolve().parent
G = 130                                          # the ground


# ── drawing helpers ─────────────────────────────────────────────────────────────────────────

def _r(v):
    v = round(v, 2)
    return int(v) if float(v).is_integer() else v


def _num(m):
    return str(_r(float(m.group())))


def shift(d, dx, dy):
    """Move every coordinate pair of a path (all commands here take x,y pairs)."""
    out, k = [], 0
    for tok in re.findall(r"[MLCQZ]|-?\d*\.?\d+", d):
        if tok in "MLCQZ":
            out.append(tok)
        else:
            out.append(str(_r(float(tok) + (dx if k % 2 == 0 else dy))))
            k += 1
    return " ".join(out)


def scale(d, s):
    if s == 1:
        return d
    return " ".join(t if t in "MLCQZ" else str(_r(float(t) * s)) for t in re.findall(r"[MLCQZ]|-?\d*\.?\d+", d))


def lg(x1, y1, x2, y2, *stops):
    return {"lg": [_r(x1), _r(y1), _r(x2), _r(y2), [[_r(i / (len(stops) - 1)), c] for i, c in enumerate(stops)]]}


def part(fill=None, **kw):
    out = {k: v for k, v in kw.items() if v is not None}
    if fill:
        out["fill"] = fill
    return out


def P(d, fill=None, **kw):
    return part(fill, d=re.sub(r"-?\d+\.\d+", _num, " ".join(d.split())), **kw)


def E(cx, cy, rx, ry, fill=None, **kw):
    return part(fill, e=[_r(cx), _r(cy), _r(rx), _r(ry)], **kw)


def seg(a, b, wa, wb):
    """A limb segment from a to b, wa wide at a and wb at b, with rounded ends."""
    (x1, y1), (x2, y2) = a, b
    L = math.hypot(x2 - x1, y2 - y1) or 1
    ux, uy = (x2 - x1) / L, (y2 - y1) / L
    nx, ny = -uy, ux
    ha, hb, k = wa / 2, wb / 2, 1.33
    p = lambda x, y: "%s %s" % (_r(x), _r(y))
    return ("M %s L %s C %s, %s, %s L %s C %s, %s, %s Z" % (
        p(x1 + nx * ha, y1 + ny * ha), p(x2 + nx * hb, y2 + ny * hb),
        p(x2 + nx * hb + ux * hb * k, y2 + ny * hb + uy * hb * k),
        p(x2 - nx * hb + ux * hb * k, y2 - ny * hb + uy * hb * k), p(x2 - nx * hb, y2 - ny * hb),
        p(x1 - nx * ha, y1 - ny * ha),
        p(x1 - nx * ha - ux * ha * k, y1 - ny * ha - uy * ha * k),
        p(x1 + nx * ha - ux * ha * k, y1 + ny * ha - uy * ha * k), p(x1 + nx * ha, y1 + ny * ha)))


def swing(amp, per, ph, at):
    return {"rot": [amp, per, ph], "at": [_r(at[0]), _r(at[1])]}


def fore_leg(c, sh, el, wr, w, colour, gait=None, lower=None):
    """Shoulder -> elbow -> wrist -> paw. gait = (period, phase): a swing at the shoulder and a bend
    at the elbow, the bend lagging the swing like a real stride."""
    lower = lower or colour
    up = [swing(18, gait[0], gait[1], sh)] if gait else None
    lo = up + [swing(16, gait[0], gait[1] - 1.4, el)] if gait else None
    paw = (wr[0] + w[2] * 0.55, G - w[2] * 0.42)
    return [P(seg(sh, el, w[0], w[1]), colour, anim=up),
            P(seg(el, wr, w[1] * 0.9, w[2]), lower, anim=lo),
            E(paw[0], paw[1], w[2] * 0.95, w[2] * 0.48, lower, anim=lo)]


def hind_leg(c, hip, st, hock, w, colour, gait=None, lower=None, thigh=None):
    """Hip -> stifle -> hock -> paw: a heavy thigh, a slim gaskin angled back, the cannon to the
    ground. Swings at the hip, folds at the stifle."""
    lower = lower or colour
    up = [swing(16, gait[0], gait[1], hip)] if gait else None
    lo = up + [swing(-14, gait[0], gait[1] - 1.4, st)] if gait else None
    paw = (hock[0] + 2, G)
    return [P(seg(st, hock, w[1], w[2]), lower if thigh else colour, anim=lo),
            P(seg(hock, (paw[0] + 1, G - w[3] * 0.5), w[2] * 0.95, w[3]), lower, anim=lo),
            E(paw[0] + w[3] * 0.6, G - w[3] * 0.42, w[3] * 1.0, w[3] * 0.48, lower, anim=lo),
            P(seg(hip, st, w[0], w[1] * 1.1), thigh or colour, anim=up)]


def eye_profile(c, x, y, size=1.0, slit=False, closed=False, g="head", blink=(5.0, 0.0)):
    s = size
    if closed:
        return [P("M %s %s Q %s %s %s %s" % (x - 3.6 * s, y, x, y + 2.2 * s, x + 3.6 * s, y - 0.4 * s),
                  stroke=c["line"], sw=1.1, cap=1, g=g)]
    b = [{"blink": list(blink)}]
    almond = "M %s %s Q %s %s %s %s Q %s %s %s %s Z" % (x - 3.6 * s, y, x, y - 3.4 * s, x + 3.6 * s, y - 0.5 * s,
                                                       x, y + 2.6 * s, x - 3.6 * s, y)
    pupil = E(x + 0.5 * s, y - 0.4 * s, 0.75 * s if slit else 1.7 * s, 2.3 * s if slit else 1.7 * s, "#120d0a",
              g=g, pivot=[x, y], anim=b)
    return [P(almond, c["eye"], g=g, pivot=[x, y], anim=b), pupil,
            E(x + 1.3 * s, y - 1.4 * s, 0.6 * s, 0.6 * s, "#ffffff", g=g, pivot=[x, y], anim=b, alpha=0.9),
            P("M %s %s Q %s %s %s %s" % (x - 3.8 * s, y + 0.1, x, y - 3.7 * s, x + 3.8 * s, y - 0.6 * s),
              stroke=c["line"], sw=0.9, cap=1, g=g)]


def shaded_body(c, d, g=None, top=60, bottom=110):
    """A body: a fill that is lighter along the back and darker under the belly, a thin edge."""
    return [P(d, lg(0, top, 0, bottom, c["hi"], c["fur"], c["lo"]), stroke=c["line"], sw=0.7, g=g)]


def H(parts, hx, hy, g="head", s=1.0):
    """Head parts written about (0, 0), scaled by s and moved to (hx, hy)."""
    X, Y = (lambda v: _r(v * s + hx)), (lambda v: _r(v * s + hy))
    out = []
    for q in parts:
        q = dict(q)
        if "d" in q:
            q["d"] = shift(scale(q["d"], s), hx, hy)
        if "e" in q:
            q["e"] = [X(q["e"][0]), Y(q["e"][1]), _r(q["e"][2] * s), _r(q["e"][3] * s)]
        if "pivot" in q:
            q["pivot"] = [X(q["pivot"][0]), Y(q["pivot"][1])]
        if "sw" in q:
            q["sw"] = _r(q["sw"] * min(s, 1.15))
        if "anim" in q:
            q["anim"] = [dict(a, at=[X(a["at"][0]), Y(a["at"][1])]) if "at" in a else a for a in q["anim"]]
        if isinstance(q.get("fill"), dict):
            x1, y1, x2, y2, st = q["fill"]["lg"]
            q["fill"] = {"lg": [X(x1), Y(y1), X(x2), Y(y2), st]}
        q.setdefault("g", g)
        out.append(q)
    return out


def shadow(wide=False):
    return E(100, G + 1, 70 if wide else 58, 4.2, "#000000", alpha=0.18)


def walk_groups(per, bob=1.0):
    return {"all": [{"ty": [bob, per / 2, 0]}], "head": [{"ty": [bob * 0.6, per / 2, 1.0]}]}


SIT = {"all": [], "body": [{"sy": [0.01, 3.4, 0]}], "head": [{"ty": [0.7, 3.4, 0.6]}]}
SLEEP = {"all": [], "body": [{"sy": [0.02, 3.8, 0]}], "head": [{"ty": [0.8, 3.8, 0]}]}


# ── the cat: a domestic orange tabby ─────────────────────────────────────────────────────────
CAT = dict(fur="#d48a4c", hi="#e7a66b", lo="#ad6a36", cream="#f1dcbf", mark="#9a5426", line="#6d3c1a",
           eye="#b9b04e", nose="#cf8b84", ear="#dfa197")


def cat_head(closed=False):
    c = CAT
    return [P("M -8 -8 L -4 -24 L 2 -10 Z", c["lo"]),                                  # far ear
            P("M -14 2 C -15 -8, -8 -14, 0 -14 C 8 -14, 12 -9, 15 -4 C 18 -2, 19 1, 18 3 "
              "C 15 7, 10 9, 4 10 C -4 12, -12 10, -14 2 Z", lg(0, -14, 0, 11, c["hi"], c["fur"]), stroke=c["line"], sw=0.7),
            P("M -4 -11 L 1 -27 L 8 -12 Z", c["fur"], stroke=c["line"], sw=0.7),             # near ear
            P("M -1 -12.5 L 1.3 -23 L 5 -12.8 Z", c["ear"], alpha=0.85),
            P("M 7 2 C 11 0, 16 1, 18 3 C 15 8, 9 10, 3 9 Z", c["cream"], alpha=0.95),        # muzzle
            P("M -4 -13 Q -2 -9 -3 -5", stroke=c["mark"], sw=1.6, cap=1, alpha=0.7),          # tabby brow
            P("M 1 -14 Q 2 -10 1 -6.5", stroke=c["mark"], sw=1.6, cap=1, alpha=0.7),
            P("M -12 0 Q -8 -1 -5 2", stroke=c["mark"], sw=1.4, cap=1, alpha=0.6),
            P("M 15.5 -1.5 L 19 -0.8 L 18 1.6 Z", c["nose"]),
            P("M 17.5 3 Q 14 6.5 10.5 5.5", stroke=c["line"], sw=0.8, cap=1),
            P("M 14 3 L 31 -1 M 14 4 L 31 4 M 13.5 5 L 30 8.5", stroke="#ffffff", sw=0.5, cap=1, alpha=0.75),
            ] + eye_profile(c, 7, -4, 1.0, slit=True, closed=closed, g="head", blink=(4.6, 0))


def cat_tail_up():
    c = CAT
    sway = [{"rot": [6, 2.2, 0], "at": [54, 80]}]
    return [P("M 54 82 C 38 80, 27 68, 25 50 C 24 42, 29 35, 35 33 C 39 33, 40 37, 37 39 "
              "C 33 42, 32 48, 33 54 C 35 66, 44 73, 56 75 Z", lg(25, 35, 56, 82, c["fur"], c["lo"]),
              stroke=c["line"], sw=0.7, anim=sway),
            P("M 27 44 Q 31 45 34 43 M 27 54 Q 31 55 34 52 M 31 64 Q 35 64 38 61", stroke=c["mark"], sw=2.2,
              cap=1, alpha=0.7, anim=sway)]


def cat_walk():
    c, per = CAT, 0.7
    parts = [shadow()]
    parts += fore_leg(c, (128, 88), (126, 106), (127, 124), (11, 7.5, 5.5), c["lo"], gait=(per, math.pi))
    parts += hind_leg(c, (77, 86), (85, 104), (75, 118), (17, 8, 6, 5), c["lo"], gait=(per, 0))
    parts += cat_tail_up()
    parts += shaded_body(c, "M 50 90 C 47 76, 58 68, 76 68 C 96 68, 114 71, 128 68 C 137 66, 142 58, 147 50 "
                            "L 160 60 C 154 70, 150 84, 145 96 C 140 105, 126 106, 116 104 C 102 102, 88 104, 76 102 "
                            "C 63 100, 52 99, 50 90 Z", top=66, bottom=106)
    parts += [P("M 140 66 C 147 76, 148 90, 143 100 C 138 92, 137 78, 140 66 Z", c["cream"], alpha=0.85),
              P("M 84 69 Q 88 80 84 91 M 96 69 Q 100 81 96 93 M 108 70 Q 112 81 108 92 M 120 69 Q 123 78 120 86 "
                "M 60 80 Q 66 87 62 95", stroke=c["mark"], sw=2.6, cap=1, alpha=0.55)]
    parts += hind_leg(c, (66, 86), (74, 104), (64, 118), (18, 8.5, 6.5, 5.5), c["fur"], gait=(per, math.pi))
    parts += fore_leg(c, (138, 88), (136, 106), (137, 124), (12, 8, 6), c["fur"], gait=(per, 0))
    parts += H(cat_head(), 158, 54, s=1.2)
    return {"groups": walk_groups(per, 0.9), "parts": parts}


def cat_sit():
    c = CAT
    parts = [shadow()]
    parts += fore_leg(c, (129, 94), (129, 110), (130, 124), (10, 7, 5.5), c["lo"])
    parts += shaded_body(c, "M 66 129 C 56 118, 58 102, 74 92 C 88 82, 104 76, 118 70 C 125 67, 131 63, 134 58 "
                            "L 148 62 C 146 74, 144 88, 142 100 C 140 113, 138 124, 134 129 Z", g="body", top=58, bottom=128)
    parts += [P("M 136 68 C 142 80, 142 100, 138 118 C 132 104, 130 84, 136 68 Z", c["cream"], alpha=0.85, g="body"),
              P("M 92 80 Q 98 88 94 96 M 104 72 Q 110 80 106 88 M 116 66 Q 121 73 118 80", stroke=c["mark"], sw=2.6,
                cap=1, alpha=0.55, g="body"),
              P("M 62 128 C 56 112, 66 96, 84 96 C 100 96, 108 108, 106 120 C 104 128, 90 130, 62 128 Z",
                lg(0, 96, 0, 129, c["fur"], c["lo"]), stroke=c["line"], sw=0.7, g="body"),
              P("M 72 104 Q 80 106 84 114 M 66 112 Q 74 114 78 121", stroke=c["mark"], sw=2.4, cap=1, alpha=0.5, g="body"),
              E(110, 127.6, 8, 2.8, c["fur"], stroke=c["line"], sw=0.6)]
    parts += fore_leg(c, (137, 94), (137, 110), (138, 124), (11, 7.5, 6), c["fur"])
    parts += [P("M 64 126 C 80 133, 118 133, 141 127 C 147 125, 148 120, 143 121 C 124 126, 88 127, 70 120 Z",
                lg(64, 124, 148, 124, c["lo"], c["fur"]), stroke=c["line"], sw=0.7, pivot=[70, 124],
                anim=[{"rot": [1.2, 2.8, 0]}]),
              P("M 100 128 Q 103 125 102 122 M 120 128 Q 123 125 122 122", stroke=c["mark"], sw=2, cap=1, alpha=0.55,
                pivot=[70, 124], anim=[{"rot": [1.2, 2.8, 0]}])]
    parts += H(cat_head(), 145, 54, s=1.2)
    return {"groups": SIT, "parts": parts}


def cat_sleep():
    c = CAT
    parts = [shadow(True)]
    parts += shaded_body(c, "M 38 121 C 36 104, 58 94, 92 94 C 124 94, 146 102, 150 116 C 152 127, 138 130, 96 130 "
                            "C 60 130, 40 129, 38 121 Z", g="body", top=94, bottom=130)
    parts += [P("M 70 96 Q 74 104 70 112 M 86 94 Q 90 103 86 112 M 102 95 Q 106 103 102 111 M 118 97 Q 121 104 118 110",
                stroke=c["mark"], sw=2.6, cap=1, alpha=0.5, g="body"),
              P("M 44 124 C 66 133, 130 134, 158 126 C 162 123, 158 118, 153 121 C 130 128, 74 128, 50 117 Z",
                lg(44, 120, 160, 120, c["fur"], c["lo"]), stroke=c["line"], sw=0.7, g="body"),
              E(150, 127.4, 9, 3, c["fur"], stroke=c["line"], sw=0.6)]
    parts += H(cat_head(closed=True), 154, 110, s=1.15)
    return {"groups": SLEEP, "parts": parts}


# ── the dog: a golden retriever ──────────────────────────────────────────────────────────────
DOG = dict(fur="#c89a5c", hi="#dbb67a", lo="#9c6f38", cream="#ead2a6", line="#644220", eye="#3d2513",
           nose="#211a17")


def dog_head(closed=False):
    c = DOG
    ear = [{"rot": [4, 1.8, 0], "at": [-6, -9]}]
    return [P("M -16 4 C -17 -6, -10 -13, -1 -13 C 6 -13, 10 -10, 13 -6 C 18 -5, 24 -4, 27 -2 "
              "C 29 1, 28 4, 26 5 C 22 8, 16 10, 10 11 C 2 14, -10 14, -16 4 Z",
              lg(0, -13, 0, 13, c["hi"], c["fur"]), stroke=c["line"], sw=0.7),
            P("M 10 -4 C 18 -4, 26 -3, 27 1 C 24 8, 16 10, 10 9 Z", c["cream"], alpha=0.6),
            P("M 23.5 -4 C 27 -5.5, 30.5 -3, 30 0 C 28.8 1.8, 25.5 1.4, 24 -0.6 Z", c["nose"]),
            P("M 27 3 Q 20 7.5 12.5 6", stroke=c["line"], sw=0.9, cap=1),
            P("M 1 -10 Q 6 -12.5 10 -9", stroke=c["lo"], sw=1.2, cap=1, alpha=0.7),
            P("M -7 -10 C -14 -8, -18 4, -15 16 C -13 21, -6 19, -4 12 C -2 4, -2 -6, -7 -10 Z",
              lg(-16, -10, -2, 20, c["lo"], c["fur"]), stroke=c["line"], sw=0.7, pivot=[-6, -9], anim=ear),
            P("M -12 2 Q -13 9 -11 14 M -9 0 Q -9 8 -7 12", stroke=c["lo"], sw=0.8, cap=1, alpha=0.6,
              pivot=[-6, -9], anim=ear),
            ] + eye_profile(c, 5.5, -5, 0.95, slit=False, closed=closed, g="head", blink=(5.2, 1.3))


def dog_walk():
    c, per = DOG, 0.62
    parts = [shadow()]
    parts += fore_leg(c, (134, 80), (132, 102), (133, 123), (14, 9, 7), c["lo"], gait=(per, math.pi))
    parts += hind_leg(c, (68, 78), (78, 100), (66, 117), (21, 10.5, 8, 6.5), c["lo"], gait=(per, 0))
    wag = [{"rot": [10, 0.55, 0], "at": [46, 76]}]
    parts += [P("M 46 74 C 32 76, 22 88, 18 102 C 17 106, 21 107, 23 104 C 28 92, 36 86, 48 82 Z",
                lg(18, 74, 48, 106, c["fur"], c["lo"]), stroke=c["line"], sw=0.7, anim=wag),
              P("M 36 82 Q 30 92 26 100 M 32 80 Q 26 88 22 96", stroke=c["hi"], sw=1.6, cap=1, alpha=0.6, anim=wag)]
    parts += shaded_body(c, "M 44 82 C 42 66, 56 58, 76 58 C 98 58, 120 60, 136 58 C 146 56, 150 48, 152 42 "
                            "L 168 50 C 162 60, 158 74, 154 88 C 148 98, 132 98, 120 96 C 104 94, 90 96, 78 94 "
                            "C 62 92, 46 92, 44 82 Z", top=56, bottom=98)
    parts += [P("M 150 58 C 158 70, 158 84, 152 94 C 146 86, 145 72, 150 58 Z", c["cream"], alpha=0.7),
              P("M 80 94 Q 94 100 108 95 M 112 96 Q 124 101 136 96", stroke=c["hi"], sw=1.4, cap=1, alpha=0.6)]
    parts += hind_leg(c, (58, 78), (68, 100), (56, 117), (23, 11.5, 8.5, 7), c["fur"], gait=(per, math.pi))
    parts += fore_leg(c, (144, 80), (142, 102), (143, 123), (15, 9.5, 7.5), c["fur"], gait=(per, 0))
    parts += H(dog_head(), 164, 44, s=1.35)
    return {"groups": walk_groups(per, 1.1), "parts": parts}


def dog_sit():
    c = DOG
    parts = [shadow()]
    parts += [P("M 52 126 C 38 128, 26 126, 18 120 C 16 118, 18 116, 21 117 C 30 121, 42 121, 54 120 Z",
                lg(18, 118, 54, 126, c["lo"], c["fur"]), stroke=c["line"], sw=0.7, pivot=[52, 123],
                anim=[{"rot": [6, 0.9, 0]}])]
    parts += fore_leg(c, (134, 92), (134, 110), (135, 124), (12, 8, 6.5), c["lo"])
    parts += shaded_body(c, "M 56 129 C 46 116, 50 98, 68 88 C 84 78, 104 72, 120 66 C 128 62, 134 56, 138 50 "
                            "L 154 56 C 153 70, 150 86, 148 100 C 146 113, 144 124, 140 129 Z", g="body", top=50, bottom=128)
    parts += [P("M 140 64 C 148 78, 148 100, 142 120 C 136 106, 134 80, 140 64 Z", c["cream"], alpha=0.75, g="body"),
              P("M 50 128 C 44 110, 56 92, 76 92 C 96 92, 104 106, 102 120 C 100 128, 84 130, 50 128 Z",
                lg(0, 92, 0, 129, c["fur"], c["lo"]), stroke=c["line"], sw=0.7, g="body"),
              E(106, 127.5, 9, 3, c["fur"], stroke=c["line"], sw=0.6)]
    parts += fore_leg(c, (143, 92), (143, 110), (144, 124), (13, 8.5, 7), c["fur"])
    parts += H(dog_head(), 148, 46, s=1.35)
    return {"groups": SIT, "parts": parts}


def dog_sleep():
    c = DOG
    parts = [shadow(True)]
    parts += shaded_body(c, "M 34 121 C 32 102, 58 92, 94 92 C 128 92, 150 100, 154 116 C 156 127, 142 130, 98 130 "
                            "C 58 130, 36 129, 34 121 Z", g="body", top=92, bottom=130)
    parts += [P("M 38 124 C 26 126, 16 122, 14 116 C 13 113, 16 112, 18 114 C 22 118, 30 120, 40 119 Z",
                lg(14, 114, 40, 126, c["lo"], c["fur"]), stroke=c["line"], sw=0.7, pivot=[38, 121],
                anim=[{"rot": [2, 3.8, 0]}]),
              P("M 132 127 C 144 128, 160 128, 170 126 C 172 123, 168 121, 160 122 C 150 123, 140 123, 132 122 Z",
                c["fur"], stroke=c["line"], sw=0.6)]
    parts += H(dog_head(closed=True), 156, 110, s=1.3)
    return {"groups": SLEEP, "parts": parts}


# ── the rabbit: a wild brown rabbit, that hops ───────────────────────────────────────────────
RAB = dict(fur="#8e6d4f", hi="#a9886a", lo="#694e36", cream="#e3d4bf", line="#4b3423", eye="#25150c",
           nose="#c7958f", ear="#c9a69a", tail="#f5f1ea")


def rab_head(closed=False):
    c = RAB
    tw = [{"rot": [3, 2.6, 0], "at": [-4, -8]}]
    ears = [P("M -6 -8 C -12 -22, -17 -40, -13 -46 C -9 -50, -5 -40, -3 -24 C -2 -16, -2 -10, -6 -8 Z",
              lg(-16, -48, -2, -8, c["lo"], c["fur"]), stroke=c["line"], sw=0.6, anim=tw)]
    near = [P("M -2 -9 C -6 -24, -6 -44, -1 -50 C 3 -54, 7 -44, 6 -26 C 5 -17, 3 -11, -2 -9 Z",
              lg(-6, -52, 6, -9, c["fur"], c["hi"]), stroke=c["line"], sw=0.6, anim=tw),
            P("M 0 -12 C -2 -26, -2 -40, 1 -45 C 3 -42, 4 -28, 3.5 -14 Z", c["ear"], alpha=0.6, anim=tw),
            P("M -1 -49 C 2 -53, 5 -50, 6 -46", stroke="#3a2618", sw=1.6, cap=1, alpha=0.7, anim=tw)]
    face = [P("M -12 4 C -13 -6, -6 -11, 2 -11 C 10 -11, 16 -6, 18 0 C 19 4, 16 8, 10 9 C 2 11, -8 10, -12 4 Z",
              lg(0, -11, 0, 11, c["hi"], c["fur"]), stroke=c["line"], sw=0.7),
            P("M 4 1 C 10 0, 16 1, 18 2 C 16 7, 10 9, 4 8 Z", c["cream"], alpha=0.8)]
    details = [E(2.5, -2.5, 4.4, 4.2, c["cream"], alpha=0.8),
               P("M 16.5 0 L 18.8 1 L 17 2.6 Z", c["nose"]),
               P("M 17.5 3 Q 15 6 12 5.5", stroke=c["line"], sw=0.7, cap=1),
               P("M 15 3 L 28 1 M 15 4 L 28 5", stroke="#ffffff", sw=0.45, cap=1, alpha=0.7)]
    eye = ([P("M -1 -2.5 Q 2.5 -0.5 6 -2.8", stroke=c["line"], sw=1.1, cap=1)] if closed else
           [E(2.5, -2.5, 3.3, 3.3, c["eye"], pivot=[2.5, -2.5], anim=[{"blink": [5.6, 2.0]}]),
            E(3.7, -3.7, 0.9, 0.9, "#ffffff", pivot=[2.5, -2.5], anim=[{"blink": [5.6, 2.0]}], alpha=0.9)])
    if closed:                                           # asleep, the ears lie back (drawn by the body)
        return face + details + eye
    return ears + face + near + details + eye


def rab_body(hop=None):
    """The rabbit's crouch: a round haunch, a short front. hop = period for the hopping version."""
    c = RAB
    hind = [swing(-22, hop, 0, (70, 100))] if hop else None
    fore = [swing(24, hop, 0.6, (136, 102))] if hop else None
    parts = [shadow()]
    parts += [P(seg((128, 102), (131, 117), 7, 5), c["lo"], stroke=c["line"], sw=0.6, anim=fore),
              P(seg((131, 117), (134, 126), 5, 4), c["lo"], stroke=c["line"], sw=0.6, anim=fore)]
    parts += [E(52, 96, 7.5, 7, c["tail"], stroke="#d8d0c4", sw=0.6, pivot=[56, 96], anim=[{"rot": [6, 1.4, 0]}])]
    parts += shaded_body(c, "M 52 112 C 46 96, 52 78, 70 72 C 88 66, 108 70, 122 78 C 130 82, 136 80, 142 74 "
                            "L 150 82 C 146 92, 142 102, 136 110 C 128 118, 110 120, 96 120 C 80 120, 58 122, 52 112 Z",
                         top=68, bottom=120)
    parts += [P("M 120 96 C 132 96, 140 104, 136 112 C 128 114, 118 108, 120 96 Z", c["cream"], alpha=0.7),
              P("M 58 126 C 56 108, 64 92, 80 92 C 96 92, 102 106, 98 118 C 94 126, 76 128, 58 126 Z",
                lg(0, 92, 0, 128, c["fur"], c["lo"]), stroke=c["line"], sw=0.7, anim=hind),
              P("M 60 126 C 70 125, 86 126, 96 128 C 98 130, 94 131, 86 131 C 74 131, 62 130, 58 129 Z",
                c["lo"], stroke=c["line"], sw=0.6, anim=hind)]
    parts += [P(seg((136, 102), (139, 117), 8, 5.5), c["fur"], stroke=c["line"], sw=0.6, anim=fore),
              P(seg((139, 117), (142, 126), 5.5, 4.5), c["fur"], stroke=c["line"], sw=0.6, anim=fore),
              E(144.5, 127.6, 4.6, 2.4, c["fur"], stroke=c["line"], sw=0.6, anim=fore)]
    return parts


def rab_walk():
    per = 0.5
    parts = rab_body(hop=per) + H(rab_head(), 156, 72)
    return {"groups": {"all": [{"ty": [3.2, per, 0]}, {"rot": [2.5, per, 1.2], "at": [100, 128]}],
                       "head": [{"ty": [0.8, per, 1.0]}]}, "parts": parts}


def rab_sit():
    return {"groups": SIT, "parts": rab_body() + H(rab_head(), 156, 72)}


def rab_sleep():
    c = RAB
    parts = [shadow(True)]
    parts += [E(46, 112, 7, 6.5, c["tail"], stroke="#d8d0c4", sw=0.6, g="body")]
    parts += shaded_body(c, "M 44 122 C 42 104, 62 94, 96 94 C 126 94, 146 102, 148 116 C 150 127, 136 130, 96 130 "
                            "C 62 130, 46 129, 44 122 Z", g="body", top=94, bottom=130)
    parts += H(rab_head(closed=True), 156, 112)
    # ears laid back along the body
    parts += [P("M 150 104 C 134 100, 112 100, 104 104 C 101 107, 110 109, 124 107 C 136 106, 144 106, 150 106 Z",
                lg(104, 100, 150, 108, c["lo"], c["fur"]), stroke=c["line"], sw=0.6),
              P("M 152 106 C 136 106, 116 109, 110 113 C 110 116, 118 116, 130 113 C 140 111, 148 109, 153 108 Z",
                lg(110, 104, 153, 116, c["fur"], c["hi"]), stroke=c["line"], sw=0.6)]
    return {"groups": SLEEP, "parts": parts}


# ── the red fox ──────────────────────────────────────────────────────────────────────────────
FOX = dict(fur="#c8652a", hi="#dd8a4b", lo="#9a4a1b", cream="#f4e8d8", line="#5a2a10", eye="#d39a2b",
           nose="#1b1512", sock="#2c211d", sockLo="#221a17", earback="#2a1f1b")


def fox_head(closed=False):
    c = FOX
    return [P("M -9 -8 L -7 -28 L 1 -10 Z", c["lo"]),                                    # far ear
            P("M -8.5 -24 L -7 -28 L -5.5 -23.5 Z", c["earback"]),
            P("M -13 4 C -14 -6, -8 -12, 0 -12 C 8 -12, 13 -7, 17 -3 C 22 -1, 28 1, 31 3 "
              "C 30 5, 26 6, 20 7 C 12 9, 2 12, -6 11 C -11 10, -13 8, -13 4 Z",
              lg(0, -12, 0, 12, c["hi"], c["fur"]), stroke=c["line"], sw=0.7),
            P("M 2 2 C 10 2, 20 3, 30 4 C 24 7, 12 10, 2 11 C -4 10, -6 6, 2 2 Z", c["cream"]),
            P("M -5 -9 L -1 -31 L 7 -10 Z", c["fur"], stroke=c["line"], sw=0.7),             # near ear
            P("M -3 -24 L -1 -31 L 1.2 -24 Z", c["earback"]),
            P("M -2 -11 L -0.6 -24 L 4 -11 Z", c["cream"], alpha=0.85),
            P("M 28.5 0.5 C 31 0, 33 1.5, 32.5 3.2 C 31.5 4.5, 29.5 4, 28.5 2.8 Z", c["nose"]),
            P("M 30 4.5 Q 24 7 18 6.5", stroke=c["line"], sw=0.8, cap=1),
            P("M 9 -3 Q 13 -1 17 -2", stroke=c["line"], sw=0.7, cap=1, alpha=0.6),
            ] + eye_profile(c, 7, -4.5, 0.95, slit=True, closed=closed, g="head", blink=(4.8, 0.6))


def fox_walk():
    c, per = FOX, 0.66
    parts = [shadow()]
    parts += fore_leg(c, (128, 84), (127, 104), (128, 124), (10, 6.5, 5), c["lo"], gait=(per, math.pi), lower=c["sockLo"])
    parts += hind_leg(c, (72, 84), (81, 103), (70, 118), (16, 7.5, 5.5, 4.8), c["lo"], gait=(per, 0),
                      lower=c["sockLo"], thigh=c["lo"])
    sway = [{"rot": [5, 1.8, 0], "at": [48, 84]}]
    parts += [P("M 48 80 C 30 80, 14 86, 6 98 C 2 104, 4 110, 10 108 C 18 104, 30 94, 48 90 Z",
                lg(4, 80, 48, 110, c["fur"], c["lo"]), stroke=c["line"], sw=0.7, anim=sway),
              P("M 14 92 C 8 96, 3 102, 5 107 C 7 110, 12 107, 15 103 Z", c["cream"], anim=sway),
              P("M 40 84 Q 28 86 18 92", stroke=c["hi"], sw=1.6, cap=1, alpha=0.5, anim=sway)]
    parts += shaded_body(c, "M 46 86 C 44 74, 56 66, 74 66 C 94 66, 116 68, 130 66 C 139 64, 145 56, 150 48 "
                            "L 162 56 C 156 66, 152 80, 147 92 C 142 100, 128 100, 118 98 C 104 96, 90 98, 78 98 "
                            "C 64 96, 48 96, 46 86 Z", top=64, bottom=100)
    parts += [P("M 144 64 C 152 72, 152 86, 146 96 C 140 88, 140 74, 144 64 Z", c["cream"]),
              P("M 82 98 C 96 101, 116 100, 132 96 C 120 102, 96 103, 82 98 Z", c["cream"], alpha=0.6)]
    parts += hind_leg(c, (60, 84), (69, 103), (58, 118), (17, 8, 6, 5), c["fur"], gait=(per, math.pi),
                      lower=c["sock"], thigh=c["fur"])
    parts += fore_leg(c, (138, 84), (137, 104), (138, 124), (11, 7, 5.5), c["fur"], gait=(per, 0), lower=c["sock"])
    parts += H(fox_head(), 162, 48, s=1.12)
    return {"groups": walk_groups(per, 0.9), "parts": parts}


def fox_sit():
    c = FOX
    parts = [shadow()]
    parts += fore_leg(c, (129, 94), (129, 110), (130, 124), (9.5, 6.5, 5), c["lo"], lower=c["sockLo"])
    parts += shaded_body(c, "M 66 129 C 56 118, 58 102, 74 92 C 88 82, 104 76, 118 70 C 125 67, 131 63, 134 58 "
                            "L 148 62 C 146 74, 144 88, 142 100 C 140 113, 138 124, 134 129 Z", g="body", top=58, bottom=128)
    parts += [P("M 134 66 C 144 78, 144 100, 138 120 C 130 104, 128 82, 134 66 Z", c["cream"], g="body"),
              P("M 62 128 C 56 112, 66 96, 84 96 C 100 96, 108 108, 106 120 C 104 128, 90 130, 62 128 Z",
                lg(0, 96, 0, 129, c["fur"], c["lo"]), stroke=c["line"], sw=0.7, g="body"),
              E(110, 127.6, 7.5, 2.7, c["sock"], stroke=c["line"], sw=0.6)]
    parts += fore_leg(c, (137, 94), (137, 110), (138, 124), (10.5, 7, 5.5), c["fur"], lower=c["sock"])
    # the brush, curled round the front paws, white tip forward
    parts += [P("M 62 124 C 82 136, 124 136, 150 126 C 158 122, 156 114, 148 116 C 126 124, 90 124, 70 114 Z",
                lg(62, 120, 156, 120, c["lo"], c["fur"]), stroke=c["line"], sw=0.7, pivot=[68, 120],
                anim=[{"rot": [1.2, 2.8, 0]}]),
              P("M 146 116 C 154 113, 160 118, 156 124 C 152 128, 146 127, 144 124 Z", c["cream"], pivot=[68, 120],
                anim=[{"rot": [1.2, 2.8, 0]}])]
    parts += H(fox_head(), 144, 48, s=1.12)
    return {"groups": SIT, "parts": parts}


def fox_sleep():
    c = FOX
    parts = [shadow(True)]
    parts += shaded_body(c, "M 40 121 C 38 104, 60 94, 94 94 C 126 94, 146 102, 150 116 C 152 127, 138 130, 96 130 "
                            "C 60 130, 42 129, 40 121 Z", g="body", top=94, bottom=130)
    parts += H(fox_head(closed=True), 154, 110)
    # the brush laid over the nose, as foxes sleep
    parts += [P("M 44 122 C 64 136, 128 138, 170 122 C 176 118, 172 110, 164 114 C 132 128, 76 128, 50 110 Z",
                lg(44, 116, 176, 116, c["lo"], c["fur"]), stroke=c["line"], sw=0.7, g="body"),
              P("M 162 113 C 170 108, 178 112, 174 120 C 170 125, 164 124, 162 120 Z", c["cream"], g="body")]
    return {"groups": SLEEP, "parts": parts}


# ── the giant panda ──────────────────────────────────────────────────────────────────────────
PAN = dict(fur="#efebe3", hi="#faf8f3", lo="#cdc7bb", black="#1e1d20", blackHi="#36343a", line="#3a383d",
           eye="#2a2420")


def pan_head(closed=False):
    c = PAN
    return [E(6, -16, 5.2, 5, c["black"]),                                              # far ear
            P("M -14 2 C -15 -10, -6 -16, 4 -16 C 14 -16, 20 -8, 21 0 C 22 6, 18 10, 12 11 "
              "C 2 13, -10 12, -14 2 Z", lg(0, -16, 0, 12, c["hi"], c["fur"], c["lo"]), stroke=c["line"], sw=0.7),
            E(-6, -14.5, 6.6, 6.4, lg(-6, -21, -6, -8, c["blackHi"], c["black"])),           # near ear
            P("M 3 -8 C 9 -11, 14 -6, 12.5 -0.5 C 11 4, 4 3, 2.5 -2 C 1.8 -4.5, 1.6 -6.6, 3 -8 Z", c["black"]),
            P("M 14 2 C 17 0, 21 0, 22 2.5 C 21.5 4.5, 18.5 5.2, 15.5 4.3 Z", c["black"]),
            P("M 19 5 Q 15 8.5 10 7.5", stroke=c["line"], sw=0.9, cap=1),
            ] + ([P("M 5 -3 Q 8 -1 11 -3.5", stroke="#cfcac0", sw=1.1, cap=1)] if closed else
                 [E(8, -3.5, 1.9, 1.9, "#100e0d", pivot=[8, -3.5], anim=[{"blink": [5.4, 3.0]}]),
                  E(8.7, -4.2, 0.6, 0.6, "#ffffff", pivot=[8, -3.5], anim=[{"blink": [5.4, 3.0]}], alpha=0.9)])


def pan_walk():
    c, per = PAN, 0.8
    parts = [shadow(True)]
    parts += fore_leg(c, (130, 96), (130, 112), (131, 125), (16, 12.5, 10), c["black"], gait=(per, math.pi))
    parts += hind_leg(c, (72, 96), (80, 110), (72, 121), (20, 12.5, 10, 9), c["black"], gait=(per, 0))
    parts += [E(40, 86, 6, 5, c["fur"], stroke=c["lo"], sw=0.6)]
    parts += shaded_body(c, "M 40 92 C 36 72, 52 58, 76 58 C 100 58, 122 60, 138 62 C 148 64, 154 66, 158 70 "
                            "L 166 82 C 160 92, 156 100, 150 106 C 142 112, 124 112, 110 110 C 94 108, 76 110, 62 108 "
                            "C 46 106, 42 102, 40 92 Z", top=58, bottom=112)
    parts += [P("M 112 60 C 124 58, 140 60, 152 66 C 157 80, 153 96, 147 110 C 136 106, 122 88, 112 60 Z",
                lg(112, 60, 150, 110, c["blackHi"], c["black"]))]
    parts += hind_leg(c, (58, 96), (66, 110), (58, 121), (22, 13.5, 11, 10), c["black"], gait=(per, math.pi))
    parts += fore_leg(c, (142, 96), (142, 112), (143, 125), (18, 13.5, 11), c["black"], gait=(per, 0))
    parts += H(pan_head(), 164, 76, s=1.25)
    return {"groups": walk_groups(per, 1.3), "parts": parts}


def pan_sit():
    c = PAN
    parts = [shadow(True)]
    parts += shaded_body(c, "M 60 129 C 48 116, 52 90, 74 74 C 90 64, 112 62, 126 66 C 136 70, 140 82, 140 96 "
                            "C 140 112, 132 124, 120 129 Z", g="body", top=62, bottom=129)
    parts += [P("M 92 66 C 108 60, 126 64, 136 74 C 140 86, 138 100, 132 108 C 122 100, 106 84, 92 66 Z",
                lg(92, 60, 136, 108, c["blackHi"], c["black"]), g="body")]
    # hind legs stretched forward, as pandas sit; forelegs down in front
    parts += [P(seg((80, 118), (118, 124), 20, 14), c["black"]),
              E(124, 120, 8, 9.5, c["blackHi"]),
              P(seg((126, 96), (130, 122), 14, 11), c["black"]),
              E(132, 125, 8, 4.5, c["black"])]
    parts += H(pan_head(), 122, 66, s=1.3)
    return {"groups": SIT, "parts": parts}


def pan_sleep():
    c = PAN
    parts = [shadow(True)]
    parts += shaded_body(c, "M 36 120 C 34 100, 58 90, 94 90 C 128 90, 150 98, 154 114 C 156 126, 142 130, 98 130 "
                            "C 58 130, 38 129, 36 120 Z", g="body", top=90, bottom=130)
    parts += [P("M 108 92 C 122 90, 140 94, 150 104 C 150 116, 144 124, 136 128 C 126 120, 114 104, 108 92 Z",
                lg(108, 90, 150, 128, c["blackHi"], c["black"]), g="body"),
              P(seg((60, 124), (40, 126), 16, 13), c["black"]),
              P(seg((140, 126), (170, 127), 13, 11), c["black"])]
    parts += H(pan_head(closed=True), 162, 110, s=1.2)
    return {"groups": SLEEP, "parts": parts}


RIG = {"box": [200, 140], "pets": {
    "cat": {"walk": cat_walk(), "sit": cat_sit(), "sleep": cat_sleep()},
    "dog": {"walk": dog_walk(), "sit": dog_sit(), "sleep": dog_sleep()},
    "bunny": {"walk": rab_walk(), "sit": rab_sit(), "sleep": rab_sleep()},
    "fox": {"walk": fox_walk(), "sit": fox_sit(), "sleep": fox_sleep()},
    "panda": {"walk": pan_walk(), "sit": pan_sit(), "sleep": pan_sleep()},
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
