/*
 * AI translator: lookahead speech-to-text + translation into live subtitles.
 *
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

#ifndef MP_AI_TRANSLATE_H
#define MP_AI_TRANSLATE_H

#include <stdbool.h>

#include "options/m_option.h"

// Option group; registered in options.c as OPT_SUBSTRUCT(ai_opts, ...).
struct ai_translate_opts {
    bool enabled;
    char *lang;         // target language (e.g. "en", "vi")
    char *api_key;      // optional Anthropic API key
    char *src;          // source language hint ("" = auto)
    char *stt_url;      // whisper-server base URL
    char *tr_url;       // translation API base URL (Anthropic-compatible)
    char *stt_key;      // optional bearer token for the STT proxy
    char *model;        // Claude model id
    bool translate;     // if false, show transcription only (no translation)
    double window;      // STT window length in seconds
    double lookahead;   // minimum translated seconds ahead of the playhead
};

extern const struct m_sub_options ai_translate_conf;

struct mpv_global;
struct mp_log;
struct ai_translate;

// Spawn the lookahead worker for `filename`. Returns NULL if disabled, if
// libcurl support is missing, or if the source cannot be reopened by ffmpeg
// (e.g. stdin, dvd://). The returned object owns a background thread.
struct ai_translate *ai_translate_create(struct mpv_global *global,
                                         struct mp_log *log,
                                         const struct ai_translate_opts *opts,
                                         const char *filename);

// Stop the worker thread and free all resources.
void ai_translate_destroy(struct ai_translate *ai);

// Report the current playback position (seconds). Used to pace decoding and to
// steer seeks. Safe to call every frame from the main thread.
void ai_translate_set_playhead(struct ai_translate *ai, double pts);

// Tell the worker the playhead jumped (seek). Cached results are kept; the
// decoder re-seeks only if `pts` is past the translated frontier.
void ai_translate_seek(struct ai_translate *ai, double pts);

// Contiguous translated coverage: subtitles exist for all speech up to this
// pts (seconds). Returns +inf when nothing more will ever be produced (whole
// file done, or the worker failed to open the source) so callers never stall.
double ai_translate_get_frontier(struct ai_translate *ai);

// Look up the subtitle line covering `pts`. On success stores a freshly
// allocated string in *out (free with talloc_free) and returns true.
bool ai_translate_get_line(struct ai_translate *ai, double pts, char **out);

#endif
