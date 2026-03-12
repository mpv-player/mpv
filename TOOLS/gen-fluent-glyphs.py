#!/usr/bin/env python3
"""Import Fluent UI System Icons into mpv-osd-symbols font.

Downloads SVG icons from microsoft/fluentui-system-icons and uses FontForge
to import them into the mpv-osd-symbols font.

Usage:
    ./TOOLS/gen-fluent-glyphs.py
    ./TOOLS/gen-osd-font.sh

Requirements: fontforge
"""

import os
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SFDIR = os.path.join(SCRIPT_DIR, "mpv-osd-symbols.sfdir")
REPO_SHA = "9a4a2db2df7f0067b4ef43c5ae5bfcae3759a5a0"  # v1.1.320
REPO_BASE = ("https://raw.githubusercontent.com/"
             f"microsoft/fluentui-system-icons/{REPO_SHA}/assets")
GLYPH_WIDTH = 880

# Mapping: codepoint -> (fluent_icon_name, size, style, transform)
#
# See https://github.com/microsoft/fluentui-system-icons/tree/main/assets
# for available icons.

ICONS = {
    0xE200: ("Line Horizontal 3",    20, "filled", None),     # menu
    0xE201: ("Play",                 24, "filled", "flip_x"), # prev / play_backward
    0xE202: ("Play",                 24, "filled", None),     # play / next
    0xE203: ("Pause",                24, "filled", None),     # pause
    0xE204: ("Clock",                24, "regular", None),    # clock (buffering)
    0xE205: ("Rewind",               24, "filled", None),     # skip_backward
    0xE206: ("Fast Forward",         24, "filled", None),     # skip_forward
    0xE207: ("Previous",             24, "filled", None),     # chapter_prev
    0xE208: ("Next",                 24, "filled", None),     # chapter_next
    0xE209: ("Chat",                 24, "regular", None),    # audio
    0xE20A: ("Closed Caption",       24, "regular", None),    # subtitle
    0xE20B: ("Speaker Mute",         24, "filled", None),     # mute
    0xE20C: ("Speaker 0",            24, "filled", None),     # volume[1]
    0xE20D: ("Speaker 1",            24, "filled", None),     # volume[2]
    0xE20E: ("Speaker 2",            24, "filled", None),     # volume[3]
    0xE20F: ("Speaker 2",            24, "filled", "add_alert"),# volume[4] (>100%)
    0xE210: ("Full Screen Maximize", 24, "filled", None),     # fullscreen
    0xE211: ("Full Screen Minimize", 24, "filled", None),     # exit_fullscreen
    0xE212: ("Dismiss",              24, "filled", None),     # close
    0xE213: ("Subtract",             24, "filled", None),     # minimize
    0xE214: ("Maximize",             24, "filled", None),     # maximize
    0xE215: ("Square Multiple",      24, "filled", None),     # unmaximize
}


def icon_url(name, size, style):
    snake = name.lower().replace(" ", "_")
    filename = f"ic_fluent_{snake}_{size}_{style}.svg"
    dir_encoded = urllib.parse.quote(name)
    return f"{REPO_BASE}/{dir_encoded}/SVG/{filename}"


def download_icons(dest_dir):
    paths = {}
    seen: dict[str, str] = {}

    for cp, (name, size, style, _transform) in ICONS.items():
        url = icon_url(name, size, style)
        svg_path = os.path.join(dest_dir, f"uni{cp:04X}.svg")

        if url in seen:
            # Reuse previously downloaded file.
            import shutil
            shutil.copy2(seen[url], svg_path)
            paths[cp] = svg_path
            print(f"  U+{cp:04X}  {name} (cached)")
            continue

        print(f"  U+{cp:04X}  {name} <- {url}")
        try:
            urllib.request.urlretrieve(url, svg_path)
            seen[url] = svg_path
            paths[cp] = svg_path
        except urllib.error.HTTPError as e:
            print(f"  ERROR: {e.code} {e.reason} - check icon name/size")

    return paths


def import_into_font(svg_paths):
    transforms = {}
    for cp, (_name, _size, _style, transform) in ICONS.items():
        if transform:
            transforms[cp] = transform

    ff_script = f"""\
import fontforge
import psMat
import os

font = fontforge.open({SFDIR!r})
WIDTH = {GLYPH_WIDTH}

svg_paths = {svg_paths!r}
transforms = {transforms!r}

for cp, svg_path in sorted(svg_paths.items()):
    if not os.path.exists(svg_path):
        continue

    glyph = font.createChar(cp)
    glyph.clear()
    glyph.importOutlines(svg_path)

    # Scale to fit ascent (800 units) with padding
    bb = glyph.boundingBox()  # (xmin, ymin, xmax, ymax)
    bw = bb[2] - bb[0]
    bh = bb[3] - bb[1]
    if bw == 0 or bh == 0:
        print(f"  SKIP U+{{cp:04X}} (empty)")
        continue

    target = 700  # leave some padding within 800-unit ascent
    scale = min(target / bh, target / bw)
    glyph.transform(psMat.scale(scale))

    # Apply transform (e.g. horizontal flip for prev)
    if cp in transforms and transforms[cp] == "flip_x":
        glyph.transform(psMat.scale(-1, 1))

    # Center in glyph
    bb = glyph.boundingBox()
    x_off = (WIDTH - (bb[2] + bb[0])) / 2
    y_off = (800 - (bb[3] + bb[1])) / 2
    glyph.transform(psMat.translate(x_off, y_off))

    # Add alert indicator
    if cp in transforms and transforms[cp] == "add_alert":
        bb = glyph.boundingBox()
        cx = (bb[0] + bb[2]) / 2
        cy = (bb[1] + bb[3]) / 2
        # Shrink icon to make room
        glyph.transform(psMat.compose(
            psMat.translate(-cx, -cy),
            psMat.compose(psMat.scale(0.75),
                          psMat.translate(cx - 80, cy))))
        # Draw exclamation mark (centered vertically at y=400)
        pen = glyph.glyphPen(replace=False)
        pen.moveTo((720, 630))
        pen.lineTo((760, 630))
        pen.lineTo((760, 300))
        pen.lineTo((720, 300))
        pen.closePath()
        pen.moveTo((720, 240))
        pen.lineTo((760, 240))
        pen.lineTo((760, 170))
        pen.lineTo((720, 170))
        pen.closePath()
        pen = None

    glyph.width = WIDTH
    print(f"  U+{{cp:04X}} imported")

font.save({SFDIR!r})
print("Saved to", {SFDIR!r})
"""

    with tempfile.NamedTemporaryFile(mode="w", suffix=".py", delete=False) as f:
        f.write(ff_script)
        script_path = f.name

    try:
        subprocess.run(["fontforge", "-lang=py", "-script", script_path],check=True)
    finally:
        os.unlink(script_path)


def main():
    print("Downloading Fluent UI System Icons...")
    with tempfile.TemporaryDirectory() as tmpdir:
        svg_paths = download_icons(tmpdir)
        if not svg_paths:
            print("No icons downloaded.")
            sys.exit(1)

        print(f"\nImporting {len(svg_paths)} icons into {SFDIR}...")
        import_into_font(svg_paths)

    print("\nDone. Run './TOOLS/gen-osd-font.sh' to rebuild the mpv font.")


if __name__ == "__main__":
    main()
