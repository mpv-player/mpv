/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_WAVEFORM_CACHE_H
#define MPLAYER_WAVEFORM_CACHE_H

#include <stdbool.h>
#include <stdint.h>

struct mp_log;
struct mpv_global;

// One amplitude sample: time position + RMS value
struct waveform_sample {
    float time;   // PTS in seconds
    float rms;    // Normalised RMS amplitude [0.0, 1.0]
};

// A complete waveform dataset (talloc'd when returned by waveform_cache_load)
struct waveform_data {
    struct waveform_sample *samples;
    int    count;
    int    fps;
    double duration;
};

// Generate a 64-bit file identity hash from path + size + mtime.
// Used as the cache key so stale entries are detected automatically.
uint64_t waveform_file_hash(const char *path, int64_t size, int64_t mtime);

// Persist waveform data to the user cache directory.
// Cache path: <mpv-cache>/waveform/<hash>-<fps>fps.mpvwfc
// Returns true on success.  Rejects invalid / silent data.
bool waveform_cache_save(struct mp_log *log, struct mpv_global *global,
                         const char *video_path, struct waveform_data *data,
                         int fps);

// Load waveform data from cache.
// Returns a talloc-allocated waveform_data, or NULL on miss / corruption.
// Corrupt files are automatically deleted so they will be regenerated.
struct waveform_data *waveform_cache_load(struct mp_log *log,
                                          struct mpv_global *global,
                                          const char *video_path,
                                          int fps, uint64_t file_hash);

// Delete the cache file for this video + fps combination.
void waveform_cache_invalidate(struct mp_log *log, struct mpv_global *global,
                               const char *video_path, int fps);

#endif /* MPLAYER_WAVEFORM_CACHE_H */
