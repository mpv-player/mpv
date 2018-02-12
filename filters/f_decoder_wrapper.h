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

#pragma once

#include <stdbool.h>

#include "filter.h"

struct sh_stream;
struct mp_codec_params;
struct mp_image_params;
struct mp_decoder_list;
struct demux_packet;

// (free with talloc_free(mp_decoder_wrapper.f)
struct mp_decoder_wrapper {
    // Filter with no input and 1 output, which returns the decoded data.
    struct mp_filter *f;

    // For informational purposes.
    char *decoder_desc;

    // Can be set by user.
    struct mp_recorder_sink *recorder_sink;

    // --- for STREAM_VIDEO

    // FPS from demuxer or from user override
    float fps;

    // Framedrop control for playback (not used for hr seek etc.)
    int attempt_framedrops; // try dropping this many frames
    int dropped_frames; // total frames _probably_ dropped

    // --- for STREAM_AUDIO

    // Prefer spdif wrapper over real decoders.
    bool try_spdif;

    // A pts reset was observed (audio only, heuristic).
    bool pts_reset;
};

// Create the decoder wrapper for the given stream, plus underlying decoder.
// The src stream must be selected, and remain valid and selected until the
// wrapper is destroyed.
struct mp_decoder_wrapper *mp_decoder_wrapper_create(struct mp_filter *parent,
                                                     struct sh_stream *src);

struct mp_decoder_list *video_decoder_list(void);
struct mp_decoder_list *audio_decoder_list(void);

// For precise seeking: if possible, try to drop frames up until the given PTS.
// This is automatically unset if the target is reached, or on reset.
void mp_decoder_wrapper_set_start_pts(struct mp_decoder_wrapper *d, double pts);

enum dec_ctrl {
    VDCTRL_FORCE_HWDEC_FALLBACK, // force software decoding fallback
    VDCTRL_GET_HWDEC,
    VDCTRL_REINIT,
    VDCTRL_GET_BFRAMES,
    // framedrop mode: 0=none, 1=standard, 2=hrseek
    VDCTRL_SET_FRAMEDROP,
};

int mp_decoder_wrapper_control(struct mp_decoder_wrapper *d,
                               enum dec_ctrl cmd, void *arg);

// Force it to reevaluate output parameters (for overrides like aspect).
void mp_decoder_wrapper_reset_params(struct mp_decoder_wrapper *d);

void mp_decoder_wrapper_get_video_dec_params(struct mp_decoder_wrapper *d,
                                             struct mp_image_params *p);

bool mp_decoder_wrapper_reinit(struct mp_decoder_wrapper *d);

struct mp_decoder {
    // Bidirectional filter; takes MP_FRAME_PACKET for input.
    struct mp_filter *f;

    // Can be set by decoder impl. on init for "special" functionality.
    int (*control)(struct mp_filter *f, enum dec_ctrl cmd, void *arg);
};

struct mp_decoder_fns {
    struct mp_decoder *(*create)(struct mp_filter *parent,
                                 struct mp_codec_params *codec,
                                 const char *decoder);
    void (*add_decoders)(struct mp_decoder_list *list);
};

extern const struct mp_decoder_fns vd_lavc;
extern const struct mp_decoder_fns ad_lavc;
extern const struct mp_decoder_fns ad_spdif;

// Convenience wrapper for lavc based decoders. eof_flag must be set to false
// on init and resets.
void lavc_process(struct mp_filter *f, bool *eof_flag,
                  bool (*send)(struct mp_filter *f, struct demux_packet *pkt),
                  bool (*receive)(struct mp_filter *f, struct mp_frame *res));

// ad_spdif.c
struct mp_decoder_list *select_spdif_codec(const char *codec, const char *pref);
