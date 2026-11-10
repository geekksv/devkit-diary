"""Draw assets/progress-{dark,light}.svg from the state of chapters/.

Counts what is actually on disk rather than taking a number from a config file, because a
hand-maintained count is a number that goes stale the first busy week.

No total, and no bar filling toward one. How many chapters there will be is not decided, and
a progress bar implies it is.

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
    w, h = 760, 54

    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
         f'viewBox="0 0 {w} {h}" role="img" aria-label="{done} chapters written">']
    o.append(f'<rect width="{w}" height="{h}" rx="8" fill="{t["bg"]}"/>')
    o.append(f'<rect x="0" y="0" width="3" height="{h}" rx="1.5" fill="{t["accent"]}"/>')

    written = "nothing written yet" if done == 0 else (
        "1 chapter written" if done == 1 else f"{done} chapters written")
    o.append(f'<text x="22" y="32" fill="{t["ink"]}" font-family="{MONO}" '
             f'font-size="14" font-weight="600">{esc(written)}</text>')

    note = f"{started} in progress" if started else "one board, nothing attached"
    o.append(f'<text x="{w - 22}" y="32" fill="{t["mute"]}" font-family="{MONO}" '
             f'font-size="11" text-anchor="end">{esc(note)}</text>')

    o.append("</svg>")
    return "\n".join(o)


def main():
    done, started = survey()
    ASSETS.mkdir(parents=True, exist_ok=True)
    for theme in THEMES:
        (ASSETS / f"progress-{theme}.svg").write_text(draw(done, started, theme),
                                                      encoding="utf-8")
    print(f"progress: {done} written, {started} in progress")


if __name__ == "__main__":
    main()
