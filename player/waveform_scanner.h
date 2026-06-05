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

#ifndef MPLAYER_WAVEFORM_SCANNER_H
#define MPLAYER_WAVEFORM_SCANNER_H

#include <stdbool.h>
#include "waveform_cache.h"

struct mp_log;
struct mpv_global;
struct mp_dispatch_queue;

// Priority levels (higher = processed first)
#define WAVEFORM_PRIORITY_USER       100  // user-triggered FPS switch
#define WAVEFORM_PRIORITY_FILE_LOAD   50  // automatic scan on file open
#define WAVEFORM_PRIORITY_BACKGROUND   1  // preload scans

// Callback invoked on the main thread when a scan completes.
// On success: result is a talloc-owned waveform_data (caller must free).
// On failure: result is NULL and error is a static error string.
typedef void (*waveform_scan_cb)(void *ctx,
                                 struct waveform_data *result,
                                 const char *error);

struct waveform_scanner;

// Create a scanner with up to max_threads worker threads.
// global is used for cache path resolution.
// dispatch is the main-thread queue for callback delivery.
struct waveform_scanner *waveform_scanner_create(struct mp_log *log,
                                                 struct mpv_global *global,
                                                 struct mp_dispatch_queue *dispatch,
                                                 int max_threads);

// Enqueue a scan of file_path at target_fps with the given priority.
// If a cache entry already exists and is valid, the callback fires
// immediately (next main-thread dispatch cycle) with the cached data.
void waveform_scanner_enqueue(struct waveform_scanner *scanner,
                              const char *file_path,
                              int target_fps,
                              int priority,
                              waveform_scan_cb callback,
                              void *callback_ctx);

// Cancel all pending/running scans for file_path.
// In-progress scans check the cancel flag and stop at the next sample.
void waveform_scanner_cancel_file(struct waveform_scanner *scanner,
                                  const char *file_path);

// Return scan progress in [0.0, 1.0], or -1 if no active scan for this file.
double waveform_scanner_get_progress(struct waveform_scanner *scanner,
                                     const char *file_path,
                                     int fps);

// Destroy the scanner and join all worker threads.
// Blocks until all threads have exited.
void waveform_scanner_destroy(struct waveform_scanner *scanner);

#endif /* MPLAYER_WAVEFORM_SCANNER_H */
