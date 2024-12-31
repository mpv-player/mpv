Steps to add new icons:

- Install `fontforge`
- Install the last freely licensed version of symbola (`ttf-symbola-free` in the
  AUR)
- `fontforge /usr/share/fonts/TTF/Symbola.ttf TOOLS/mpv-osd-symbols.sfdir`
- Check the Unicode hex value of the desired character (`g-a` in vim)
- Scroll until that value in the Symbola window and click it
- Press Ctrl+c
- Focus the window with TOOLS/mpv-osd-symbols.sfdir
- Click an unused character slot
- Press Ctrl+v
- Press Ctrl+s
- Close fontforge
- Figure out the Lua sequence of the new character by copying that of the
  previous character and incrementing the last 3 digits
