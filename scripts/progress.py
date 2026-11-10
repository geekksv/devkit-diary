"""Draw assets/progress-{dark,light}.svg from the state of chapters/.

Counts what is actually on disk rather than taking a number from a config file, because a
hand-maintained count is a number that goes stale the first busy week.

A chapter counts as:
    done        its folder has a src/ directory with something in it
    started     the folder exists with notes or a readme, but no working sketch yet
    planned     no folder at all

    python scripts/progress.py
"""

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from theme import THEMES, MONO, esc  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
CHAPTERS = ROOT / "chapters"
ASSETS = ROOT / "assets"
TOTAL = 24  # chapters in PROJECTS.md; raise this when the book grows


def survey():
    """Return (done, started) by looking at what is on disk."""
    done = started = 0
    if not CHAPTERS.exists():
        return 0, 0
    for folder in sorted(CHAPTERS.iterdir()):
        # Only count folders named like a chapter: 01-something. A stray directory should
        # not quietly inflate the count.
        if not folder.is_dir() or not re.match(r"^\d{2}-", folder.name):
            continue
        src = folder / "src"
        # Ignore dotfiles: a src/ holding only .gitkeep is an empty folder git can track,
        # not a finished chapter. Counting it as done marked chapter 1 complete on the day
        # the folder was created.
        sketches = [f for f in src.iterdir() if not f.name.startswith(".")] if src.is_dir() else []
        if sketches:
            done += 1
        else:
            started += 1
    return done, started


def draw(done, started, theme):
    t = THEMES[theme]
    w, h = 760, 96
    pad = 22
    bar_y, bar_h = 58, 10
    bar_w = w - pad * 2

    frac_done = done / TOTAL if TOTAL else 0
    frac_any = (done + started) / TOTAL if TOTAL else 0

    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
         f'viewBox="0 0 {w} {h}" role="img" aria-label="{done} of {TOTAL} chapters written">']
    o.append(f'<rect width="{w}" height="{h}" rx="10" fill="{t["bg"]}"/>')

    o.append(f'<text x="{pad}" y="30" fill="{t["ink"]}" font-family="{MONO}" '
             f'font-size="14" font-weight="600">{done} of {TOTAL} chapters</text>')

    right = esc(f"{started} in progress" if started else "one board, nothing attached")
    o.append(f'<text x="{w - pad}" y="30" fill="{t["mute"]}" font-family="{MONO}" '
             f'font-size="11" text-anchor="end">{right}</text>')

    # track, then everything started, then everything finished on top
    o.append(f'<rect x="{pad}" y="{bar_y}" width="{bar_w}" height="{bar_h}" rx="5" '
             f'fill="{t["rule"]}"/>')
    if frac_any > 0:
        o.append(f'<rect x="{pad}" y="{bar_y}" width="{bar_w * frac_any:.1f}" '
                 f'height="{bar_h}" rx="5" fill="{t["accent"]}" opacity="0.32"/>')
    if frac_done > 0:
        o.append(f'<rect x="{pad}" y="{bar_y}" width="{bar_w * frac_done:.1f}" '
                 f'height="{bar_h}" rx="5" fill="{t["accent"]}"/>')

    o.append("</svg>")
    return "\n".join(o)


def main():
    done, started = survey()
    ASSETS.mkdir(parents=True, exist_ok=True)
    for theme in THEMES:
        (ASSETS / f"progress-{theme}.svg").write_text(draw(done, started, theme),
                                                      encoding="utf-8")
    print(f"progress: {done} done, {started} in progress, {TOTAL} total")


if __name__ == "__main__":
    main()
