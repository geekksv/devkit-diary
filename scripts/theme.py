"""Shared palette and helpers for the drawing scripts.

One place for the colours so the cover and the progress bar cannot drift apart, which is
exactly what happened on the last repo that defined them twice.

The accent is amber rather than the GitHub green used on the profile repo. That is deliberate:
this is a book, not an extension of a profile page, and it should not read as one.
"""

THEMES = {
    "dark": {
        "bg": "#0d1117",
        "panel": "#11161d",
        "rule": "#232b36",
        "ink": "#e8ecf2",
        "mute": "#8b949e",
        "faint": "#5a6472",
        "accent": "#e8a33d",
    },
    "light": {
        "bg": "#ffffff",
        "panel": "#fbfaf8",
        "rule": "#e2e0db",
        "ink": "#14171d",
        "mute": "#5c6470",
        "faint": "#9aa1ab",
        "accent": "#9a5b12",
    },
}

# One stack, declared once. Monospace throughout: this is a book about a dev board, and the
# cover should not pretend otherwise.
MONO = ("ui-monospace,SFMono-Regular,'SF Mono',Menlo,Consolas,"
        "'JetBrains Mono','Liberation Mono',monospace")


def esc(text):
    """XML-escape text bound for an SVG.

    A bare & makes the whole file invalid XML, and the browser renders nothing at all — no
    warning, no partial draw, just a broken image icon. This has cost an afternoon before.
    """
    return (str(text)
            .replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
            .replace('"', "&quot;"))
