# screenshot-folder test notes

Representative manual parse expectations for v1.

## Movies

1. `Inception.mkv`
   - classification: movie
   - folder: `Movies/Inception/`

2. `Inception.2010.1080p.BluRay.x264.mkv`
   - classification: movie
   - parsed: title `Inception`, year `2010`
   - folder: `Movies/Inception/`

3. `Dune Part Two (2024).mkv`
   - classification: movie
   - parsed: title `Dune Part Two`, year `2024`
   - folder: `Movies/Dune Part Two/`

## Shows

1. `Breaking.Bad.S03E05.mkv`
   - classification: show
   - parsed: show `Breaking Bad`, season `03`, episode `05`
   - folder: `Shows/Breaking Bad/Season 03/Episode 05/`

2. `breaking_bad_s3e5.mp4`
   - classification: show
   - parsed: show `Breaking Bad`, season `03`, episode `05`
   - folder: `Shows/Breaking Bad/Season 03/Episode 05/`

3. `Dark/Season 2/Episode 6.mkv`
   - classification: show
   - parsed from folders + filename: show `Dark`, season `02`, episode `06`
   - folder: `Shows/Dark/Season 02/Episode 06/`

4. `The Office/Season 01/1x03.mp4`
   - classification: show
   - parsed: show `The Office`, season `01`, episode `03`
   - folder: `Shows/The Office/Season 01/Episode 03/`

## Episode title parsing

`The Office (US) (2005) - S01E02 - Diversity Day (1080p BluRay x265 Silence).mkv`

- show: `The Office (US)`
- season: `01`
- episode: `02`
- episode title: `Diversity Day`
- folder: `Shows/The Office (US)/Season 01/Episode 02/`

## Year inside series name

`Invincible.2021.S04E02.1080p.WEB.h264-ETHEL.mkv`

- show: `Invincible`
- optional year metadata: `2021`
- season: `04`
- episode: `02`
- folder: `Shows/Invincible/Season 04/Episode 02/`

## Daily/date-based

`The.Daily.Show.2025.03.14.1080p.WEB.h264.mkv`

- show: `The Daily Show`
- date: `2025-03-14`
- if `support_daily_show_folders=yes`:
  - folder: `Shows/The Daily Show/By Date/2025-03-14/`
- otherwise:
  - safe fallback under `Unsorted/<Show Title>/`
  - date preserved in filename metadata

## x-style

`Treme.1x03.Right.Place.Wrong.Time.HDTV.XviD-NoTV.avi`

- show: `Treme`
- season: `01`
- episode: `03`
- episode title: `Right Place Wrong Time`

## Fallbacks

1. `random_video.mp4`
   - classification: movie or unknown (depending on evidence)
   - save must still succeed with deterministic folder

2. Stream URL with media title only
   - no local path required
   - route to fallback safely using media title if available

## Safety checks

- Invalid path chars are removed from folder/file names.
- Existing file collisions append ` - 1`, ` - 2`, etc.
- Folder creation failure attempts fallback root under `fallback_subdir`.
- OSD always reports save success/failure.
