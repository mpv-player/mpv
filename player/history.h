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

// --video-history

#ifndef MPLAYER_HISTORY_H
#define MPLAYER_HISTORY_H

#include <stdbool.h>

#include "filters/frame.h"

struct MPContext;
struct mp_pin;

bool history_active(struct MPContext *mpctx);
// Drop all frames.
void history_reset(struct MPContext *mpctx);
// Next frame from the filter chain does not continue the current range.
void history_end_range(struct MPContext *mpctx);
// Like history_reset(), for changes that make the cached frames unusable
// without seek.
void history_invalidate(struct MPContext *mpctx);
// Keep a new reference to a frame read from the filter chain.
void history_add(struct MPContext *mpctx, struct mp_frame frame);
// Whether the next frame of this type comes from history.
bool history_replaying(struct MPContext *mpctx, enum mp_frame_type type);
// Replacements for mp_pin_out_read()/mp_pin_out_unread() on the filter chain
// output. *replay indicates whether the frame comes from the history.
struct mp_frame history_read(struct MPContext *mpctx, enum mp_frame_type type,
                             struct mp_pin *pin, bool *replay);
void history_unread(struct MPContext *mpctx, struct mp_frame frame,
                    struct mp_pin *pin, bool replay);
// Try to seek to the video frame an exact seek to *pts would show. If step is
// not 0, *pts is the current video frame, and the target is step frames away
// from it. On success, start the replay, set *pts to the pts of the target
// frame, and return true.
// If the frames after the target range are not what the decoders output next,
// *resume_pts is set to the end of the range. The caller must seek the demuxer
// there and reset the decoders. Otherwise it is set to MP_NOPTS_VALUE, and the
// decoders must be left alone.
bool history_seek(struct MPContext *mpctx, double *pts, int step,
                  double *resume_pts);
// After a seek with resume_pts, the decoders output some frames that are
// already cached. Read and drop them. Call this before reading from the pin.
void history_skip(struct MPContext *mpctx, enum mp_frame_type type,
                  struct mp_pin *pin);
// for video-history-state property
void history_get_state(struct MPContext *mpctx, struct mpv_node *node);

#endif
