# screenshot-folder.lua

Windows-first mpv Lua script that saves screenshots into deterministic media
folders without modifying mpv core.

## What it does

- Registers a configurable hotkey (`Ctrl+Shift+s` by default).
- Reads current media path, filename, media title, and playback timestamp.
- Classifies media as `movie`, `show`, or `unknown` via layered rules.
- Parses show title/season/episode/episode title/date from filename + folders.
- Creates target folders automatically.
- Saves screenshot via `screenshot-to-file` with collision-safe filenames.
- Sanitizes Windows-invalid path characters.
- Shows OSD success/failure messages.

## Install

Use one of these approaches:

1. One-off run:
   - `mpv --script=/path/to/screenshot-folder.lua <media-file>`
2. Persistent:
   - copy `screenshot-folder.lua` to your mpv `scripts` directory.

## Configuration

Script options are loaded with identifier `screenshot-folder`.

Put values in `script-opts/screenshot-folder.conf` (or use
`script-opts-append=screenshot-folder-...=` in `mpv.conf`).

Default options:

```ini
base_output_dir=D:/MediaShots
movies_subdir=Movies
shows_subdir=Shows
fallback_subdir=Unsorted
image_format=png
include_year_in_movie_folder=no
zero_pad_season_episode=yes
screenshot_key=Ctrl+Shift+s
show_osd_confirmation=yes
save_debug_log=no
support_daily_show_folders=no
```

## Output structure

- Movie: `<base_output_dir>/Movies/<Movie Title>/`
- Show: `<base_output_dir>/Shows/<Show Title>/Season <NN>/Episode <NN>/`
- Show partial season: `<Show>/Season <NN>/Episode Unknown/`
- Date show (if enabled): `<Show>/By Date/<YYYY-MM-DD>/`
- Fallback: `<base_output_dir>/Unsorted/<Clean Name>/`

## Filename format

- Movie: `<Movie Title> - <HH-MM-SS-ms>.png`
- Show episodic: `<Show Title> - S<NN>E<NN> - <HH-MM-SS-ms>.png`
- Show date-only: `<Show Title> - <YYYY-MM-DD> - <HH-MM-SS-ms>.png`
- Collision handling: appends ` - 1`, ` - 2`, etc.

## Supported parser patterns (v1)

- `S01E01`, `s01e01`, `S1E1`
- `1x03`
- `Season 1 Episode 2`
- Folder-derived season/episode (`Season 02`, `Episode 06`)
- Date-based (`YYYY-MM-DD`, `YYYY.MM.DD`, `YYYY_MM_DD`)

## Notes

- No AI/OCR/subtitle extraction/external companion app.
- Behavior is deterministic and offline.
- Debug logs are emitted to mpv log output when `save_debug_log=yes`.
