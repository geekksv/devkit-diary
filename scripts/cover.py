"""Draw assets/cover-{dark,light}.svg — the book cover.

Drawn rather than photographed, for the same reason the rest of this repo draws its images:
it is a file in the repo that renders identically for everyone, with no service to go down and
no account to expire.

The board on the cover is schematic, not a picture: a module outline, the antenna's serpentine
trace, and the two rows of pins. Enough to be recognised by anyone who owns one.

    python scripts/cover.py
"""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from theme import THEMES, MONO, esc  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"

TITLE = "devkit-diary"
SUBTITLE = "What one ESP32 can do with nothing attached to it"
AUTHOR = "Keshav Kumar"


def board(x, y, t):
    """A schematic DevKit V1: pin rows, module can, antenna trace."""
    o = []
    bw, bh = 196, 92          # the PCB
    mw, mh = 118, 58          # the shielded module sitting on it

    o.append(f'<rect x="{x}" y="{y}" width="{bw}" height="{bh}" rx="5" '
             f'fill="none" stroke="{t["rule"]}" stroke-width="1.5"/>')

    # pin headers down both long edges
    for row_y in (y + 6, y + bh - 6):
        for i in range(15):
            px = x + 14 + i * 12
            o.append(f'<circle cx="{px}" cy="{row_y}" r="2" fill="{t["faint"]}"/>')

    mx, my = x + (bw - mw) / 2, y + (bh - mh) / 2
    o.append(f'<rect x="{mx}" y="{my}" width="{mw}" height="{mh}" rx="3" '
             f'fill="{t["panel"]}" stroke="{t["rule"]}" stroke-width="1.2"/>')

    # The serpentine antenna, top-right of the module — the one feature that makes the
    # outline unmistakably this board.
    ax, ay = mx + mw - 34, my + 7
    seg = []
    for i in range(6):
        sx = ax + i * 5
        seg.append(f"M{sx} {ay} L{sx} {ay + 11}" if i % 2 == 0
                   else f"M{sx} {ay + 11} L{sx} {ay}")
    o.append(f'<path d="{" ".join(seg)}" stroke="{t["accent"]}" stroke-width="1.4" '
             f'fill="none" stroke-linecap="round"/>')
    o.append(f'<path d="M{ax} {ay} L{ax + 25} {ay}" stroke="{t["accent"]}" '
             f'stroke-width="1.4" fill="none" opacity="0.5"/>')

    # a few traces leaving the module, so it does not float
    for i, dy in enumerate((14, 26, 38)):
        o.append(f'<path d="M{mx} {my + dy} L{x + 10} {my + dy}" stroke="{t["rule"]}" '
                 f'stroke-width="1"/>')
    return "\n".join(o)


def draw(theme):
    t = THEMES[theme]
    w, h = 900, 420
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
         f'viewBox="0 0 {w} {h}" role="img" '
         f'aria-label="{esc(TITLE)} — {esc(SUBTITLE)}">']
    o.append(f'<rect width="{w}" height="{h}" fill="{t["bg"]}"/>')

    # a thin accent spine down the left edge, like a bound book
    o.append(f'<rect x="0" y="0" width="5" height="{h}" fill="{t["accent"]}"/>')

    left = 64
    o.append(f'<text x="{left}" y="92" fill="{t["mute"]}" font-family="{MONO}" '
             f'font-size="12" letter-spacing="3">A FIELD NOTEBOOK</text>')

    o.append(f'<text x="{left}" y="168" fill="{t["ink"]}" font-family="{MONO}" '
             f'font-size="54" font-weight="700" letter-spacing="-1">{esc(TITLE)}</text>')

    o.append(f'<path d="M{left} 192 L{left + 150} 192" stroke="{t["accent"]}" '
             f'stroke-width="2.5"/>')

    o.append(f'<text x="{left}" y="232" fill="{t["mute"]}" font-family="{MONO}" '
             f'font-size="16">{esc(SUBTITLE)}</text>')

    o.append(f'<text x="{left}" y="286" fill="{t["faint"]}" font-family="{MONO}" '
             f'font-size="13">No modules. No breadboard. No shopping list.</text>')

    o.append(f'<text x="{left}" y="356" fill="{t["ink"]}" font-family="{MONO}" '
             f'font-size="14" font-weight="600">{esc(AUTHOR)}</text>')
    o.append(f'<text x="{left}" y="376" fill="{t["faint"]}" font-family="{MONO}" '
             f'font-size="11">written in the open, one chapter at a time</text>')

    o.append(board(w - 268, 164, t))
    o.append("</svg>")
    return "\n".join(o)


def main():
    ASSETS.mkdir(parents=True, exist_ok=True)
    for theme in THEMES:
        (ASSETS / f"cover-{theme}.svg").write_text(draw(theme), encoding="utf-8")
    print("drew cover-dark.svg and cover-light.svg")


if __name__ == "__main__":
    main()
