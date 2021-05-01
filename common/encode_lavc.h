/*
 * muxing using libavformat
 *
 * Copyright (C) 2011 Rudolf Polzer <divVerent@xonotic.org>
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

#ifndef MPLAYER_ENCODE_LAVC_H
#define MPLAYER_ENCODE_LAVC_H

#include <pthread.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avstring.h>
#include <libavutil/pixfmt.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>

#include "common/common.h"
#include "encode.h"
#include "video/csputils.h"

struct encode_lavc_context {
    // --- Immutable after init
    struct mpv_global *global;
    struct encode_opts *options;
    struct mp_log *log;
    struct encode_priv *priv;
    const AVOutputFormat *oformat;
    const char *filename;

    // All entry points must be guarded with the lock. Functions called by
    // the playback core lock this automatically, but ao_lavc.c and vo_lavc.c
    // must lock manually before accessing state.
    pthread_mutex_t lock;

    // anti discontinuity mode
    double next_in_pts;
    double discontinuity_pts_offset;
};

// --- interface for vo/ao drivers

// Static information after encoder init. This never changes (even if there are
// dynamic runtime changes, they have to work over AVPacket side data).
// For use in encoder_context, most fields are copied from encoder_context.encoder
// by encoder_init_codec_and_muxer().
struct encoder_stream_info {
    AVRational timebase; // timebase used by the encoder (in frames/out packets)
    AVCodecParameters *codecpar;
};

// The encoder parts for each stream (no muxing parts included).
// This is private to each stream.
struct encoder_context {
    struct mpv_global *global;
    struct encode_opts *options;
    struct mp_log *log;
    const AVOutputFormat *oformat;

    // (avoid using this)
    struct encode_lavc_context *encode_lavc_ctx;

    enum stream_type type;

    // (different access restrictions before/after encoder init)
    struct encoder_stream_info info;
    AVCodecContext *encoder;
    struct mux_stream *mux_stream;

    struct stream *twopass_bytebuffer;
};

// Free with talloc_free(). (Keep in mind actual deinitialization requires
// sending a flush packet.)
// This can fail and return NULL.
struct encoder_context *encoder_context_alloc(struct encode_lavc_context *ctx,
                                              enum stream_type type,
                                              struct mp_log *log);

// After setting your codec parameters on p->encoder, you call this to "open"
// the encoder. This also initializes p->mux_stream. Returns false on failure.
// on_ready is called as soon as the muxer has been initialized. Then you are
// allowed to write packets with encoder_encode().
// Warning: the on_ready callback is called asynchronously, so you need to
// make sure to properly synchronize everything.
bool encoder_init_codec_and_muxer(struct encoder_context *p,
                                  void (*on_ready)(void *ctx), void *ctx);

// Encode the frame and write the packet. frame is ref'ed as need.
bool encoder_encode(struct encoder_context *p, AVFrame *frame);

// Return muxer timebase (only available after on_ready() has been called).
// Caller needs to acquire encode_lavc_context.lock (or call it from on_ready).
AVRational encoder_get_mux_timebase_unlocked(struct encoder_context *p);

double encoder_get_offset(struct encoder_context *p);

#endif
