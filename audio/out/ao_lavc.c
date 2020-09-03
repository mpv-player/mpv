/*
 * audio encoding using libavformat
 *
 * Copyright (C) 2011-2012 Rudolf Polzer <divVerent@xonotic.org>
 * NOTE: this file is partially based on ao_pcm.c by Atmosfear
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <libavutil/common.h>

#include "config.h"
#include "options/options.h"
#include "common/common.h"
#include "audio/aframe.h"
#include "audio/format.h"
#include "audio/fmt-conversion.h"
#include "filters/filter_internal.h"
#include "filters/f_utils.h"
#include "mpv_talloc.h"
#include "ao.h"
#include "internal.h"
#include "common/msg.h"

#include "common/encode_lavc.h"

struct priv {
    struct encoder_context *enc;

    int pcmhack;
    int aframesize;
    int framecount;
    int64_t lastpts;
    int sample_size;
    double expected_next_pts;
    struct mp_filter *filter_root;
    struct mp_filter *fix_frame_size;

    AVRational worst_time_base;

    bool shutdown;
};

static bool write_frame(struct ao *ao, struct mp_frame frame);

static bool supports_format(const AVCodec *codec, int format)
{
    for (const enum AVSampleFormat *sampleformat = codec->sample_fmts;
         sampleformat && *sampleformat != AV_SAMPLE_FMT_NONE;
         sampleformat++)
    {
        if (af_from_avformat(*sampleformat) == format)
            return true;
    }
    return false;
}

static void select_format(struct ao *ao, const AVCodec *codec)
{
    int formats[AF_FORMAT_COUNT + 1];
    af_get_best_sample_formats(ao->format, formats);

    for (int n = 0; formats[n]; n++) {
        if (supports_format(codec, formats[n])) {
            ao->format = formats[n];
            break;
        }
    }
}

static void on_ready(void *ptr)
{
    struct ao *ao = ptr;
    struct priv *ac = ao->priv;

    ac->worst_time_base = encoder_get_mux_timebase_unlocked(ac->enc);

    ao_add_events(ao, AO_EVENT_INITIAL_UNBLOCK);
}

// open & setup audio device
static int init(struct ao *ao)
{
    struct priv *ac = ao->priv;

    ac->enc = encoder_context_alloc(ao->encode_lavc_ctx, STREAM_AUDIO, ao->log);
    if (!ac->enc)
        return -1;
    talloc_steal(ac, ac->enc);

    AVCodecContext *encoder = ac->enc->encoder;
    const AVCodec *codec = encoder->codec;

    int samplerate = af_select_best_samplerate(ao->samplerate,
                                               codec->supported_samplerates);
    if (samplerate > 0)
        ao->samplerate = samplerate;

    encoder->time_base.num = 1;
    encoder->time_base.den = ao->samplerate;

    encoder->sample_rate = ao->samplerate;

    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_any(&sel);
    if (!ao_chmap_sel_adjust2(ao, &sel, &ao->channels, false))
        goto fail;
    mp_chmap_reorder_to_lavc(&ao->channels);
    encoder->channels = ao->channels.num;
    encoder->channel_layout = mp_chmap_to_lavc(&ao->channels);

    encoder->sample_fmt = AV_SAMPLE_FMT_NONE;

    select_format(ao, codec);

    ac->sample_size = af_fmt_to_bytes(ao->format);
    encoder->sample_fmt = af_to_avformat(ao->format);
    encoder->bits_per_raw_sample = ac->sample_size * 8;

    if (!encoder_init_codec_and_muxer(ac->enc, on_ready, ao))
        goto fail;

    ac->pcmhack = 0;
    if (encoder->frame_size <= 1)
        ac->pcmhack = av_get_bits_per_sample(encoder->codec_id) / 8;

    if (ac->pcmhack) {
        ac->aframesize = 16384; // "enough"
    } else {
        ac->aframesize = encoder->frame_size;
    }

    // enough frames for at least 0.25 seconds
    ac->framecount = ceil(ao->samplerate * 0.25 / ac->aframesize);
    // but at least one!
    ac->framecount = MPMAX(ac->framecount, 1);

    ac->lastpts = AV_NOPTS_VALUE;

    ao->untimed = true;

    ao->device_buffer = ac->aframesize * ac->framecount;

    ac->filter_root = mp_filter_create_root(ao->global);
    ac->fix_frame_size = mp_fixed_aframe_size_create(ac->filter_root,
                                                     ac->aframesize, true);
    MP_HANDLE_OOM(ac->fix_frame_size);

    return 0;

fail:
    pthread_mutex_unlock(&ao->encode_lavc_ctx->lock);
    ac->shutdown = true;
    return -1;
}

// close audio device
static void uninit(struct ao *ao)
{
    struct priv *ac = ao->priv;
    struct encode_lavc_context *ectx = ao->encode_lavc_ctx;

    if (!ac->shutdown) {
        double outpts = ac->expected_next_pts;

        pthread_mutex_lock(&ectx->lock);
        if (!ac->enc->options->rawts)
            outpts += ectx->discontinuity_pts_offset;
        pthread_mutex_unlock(&ectx->lock);

        outpts += encoder_get_offset(ac->enc);

        if (!write_frame(ao, MP_EOF_FRAME))
            MP_WARN(ao, "could not flush last frame\n");
        encoder_encode(ac->enc, NULL);
    }

    talloc_free(ac->filter_root);
}

// must get exactly ac->aframesize amount of data
static void encode(struct ao *ao, struct mp_aframe *af)
{
    struct priv *ac = ao->priv;
    AVCodecContext *encoder = ac->enc->encoder;
    double outpts = mp_aframe_get_pts(af);

    AVFrame *frame = mp_aframe_to_avframe(af);
    if (!frame)
        abort();

    frame->pts = rint(outpts * av_q2d(av_inv_q(encoder->time_base)));

    int64_t frame_pts = av_rescale_q(frame->pts, encoder->time_base,
                                     ac->worst_time_base);
    if (ac->lastpts != AV_NOPTS_VALUE && frame_pts <= ac->lastpts) {
        // whatever the fuck this code does?
        MP_WARN(ao, "audio frame pts went backwards (%d <- %d), autofixed\n",
                (int)frame->pts, (int)ac->lastpts);
        frame_pts = ac->lastpts + 1;
        ac->lastpts = frame_pts;
        frame->pts = av_rescale_q(frame_pts, ac->worst_time_base,
                                  encoder->time_base);
        frame_pts = av_rescale_q(frame->pts, encoder->time_base,
                                 ac->worst_time_base);
    }
    ac->lastpts = frame_pts;

    frame->quality = encoder->global_quality;
    encoder_encode(ac->enc, frame);
    av_frame_free(&frame);
}

static bool write_frame(struct ao *ao, struct mp_frame frame)
{
    struct priv *ac = ao->priv;

    // Can't push in frame if it doesn't want it output one.
    mp_pin_out_request_data(ac->fix_frame_size->pins[1]);

    if (!mp_pin_in_write(ac->fix_frame_size->pins[0], frame))
        return false; // shouldn't happen™

    while (1) {
        struct mp_frame fr = mp_pin_out_read(ac->fix_frame_size->pins[1]);
        if (!fr.type)
            break;
        if (fr.type != MP_FRAME_AUDIO)
            continue;
        struct mp_aframe *af = fr.data;
        encode(ao, af);
        mp_frame_unref(&fr);
    }

    return true;
}

static bool audio_write(struct ao *ao, void **data, int samples)
{
    struct priv *ac = ao->priv;
    struct encode_lavc_context *ectx = ao->encode_lavc_ctx;

    // See ao_driver.write_frames.
    struct mp_aframe *af = mp_aframe_new_ref(*(struct mp_aframe **)data);

    double nextpts;
    double pts = mp_aframe_get_pts(af);
    double outpts = pts;

    // for ectx PTS fields
    pthread_mutex_lock(&ectx->lock);

    if (!ectx->options->rawts) {
        // Fix and apply the discontinuity pts offset.
        nextpts = pts;
        if (ectx->discontinuity_pts_offset == MP_NOPTS_VALUE) {
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        } else if (fabs(nextpts + ectx->discontinuity_pts_offset -
                        ectx->next_in_pts) > 30)
        {
            MP_WARN(ao, "detected an unexpected discontinuity (pts jumped by "
                    "%f seconds)\n",
                    nextpts + ectx->discontinuity_pts_offset - ectx->next_in_pts);
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        }

        outpts = pts + ectx->discontinuity_pts_offset;
    }

    // Shift pts by the pts offset first.
    outpts += encoder_get_offset(ac->enc);

    // Calculate expected pts of next audio frame (input side).
    ac->expected_next_pts = pts + mp_aframe_get_size(af) / (double) ao->samplerate;

    // Set next allowed input pts value (input side).
    if (!ectx->options->rawts) {
        nextpts = ac->expected_next_pts + ectx->discontinuity_pts_offset;
        if (nextpts > ectx->next_in_pts)
            ectx->next_in_pts = nextpts;
    }

    pthread_mutex_unlock(&ectx->lock);

    mp_aframe_set_pts(af, outpts);

    return write_frame(ao, MAKE_FRAME(MP_FRAME_AUDIO, af));
}

static void get_state(struct ao *ao, struct mp_pcm_state *state)
{
    state->free_samples = 1;
    state->queued_samples = 0;
    state->delay = 0;
}

static bool set_pause(struct ao *ao, bool paused)
{
    return true; // signal support so common code doesn't write silence
}

static void start(struct ao *ao)
{
    // we use data immediately
}

static void reset(struct ao *ao)
{
}

const struct ao_driver audio_out_lavc = {
    .encode = true,
    .description = "audio encoding using libavcodec",
    .name      = "lavc",
    .initially_blocked = true,
    .write_frames = true,
    .priv_size = sizeof(struct priv),
    .init      = init,
    .uninit    = uninit,
    .get_state = get_state,
    .set_pause = set_pause,
    .write     = audio_write,
    .start     = start,
    .reset     = reset,
};

// vim: sw=4 ts=4 et tw=80
