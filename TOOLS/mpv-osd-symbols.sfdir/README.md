Steps to add new icons:

- Install [FontForge](https://fontforge.org/en-US/)
- Load a freely licensed font and mpv's glyph directory, for example:
  `fontforge Symbola.ttf TOOLS/mpv-osd-symbols.sfdir`
- Check the Unicode hex value of the desired character (`g-a` in vim)
- Scroll until that value in the font window and click it
- Copy the selected glyph (Ctrl+c)
- Focus the window with TOOLS/mpv-osd-symbols.sfdir
- Click an unused character slot
- Paste the glyph (Ctrl+v)
- Save (Ctrl+s)
- Edit the numbers in the glyph file to match the size and position of adjacent
  icons (TODO: find a better way)
- Run `sh TOOLS/gen-osd-font.sh`
- Add the icon to osc.lua following the instructions there
