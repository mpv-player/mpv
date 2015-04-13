/*
 * audio encoding using libavformat
 * Copyright (C) 2011-2012 Rudolf Polzer <divVerent@xonotic.org>
 * NOTE: this file is partially based on ao_pcm.c by Atmosfear
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <libavutil/common.h>
#include <libavutil/audioconvert.h>

#include "config.h"
#include "options/options.h"
#include "common/common.h"
#include "audio/format.h"
#include "audio/fmt-conversion.h"
#include "talloc.h"
#include "ao.h"
#include "internal.h"
#include "common/msg.h"

#include "common/encode_lavc.h"

struct priv {
    uint8_t *buffer;
    size_t buffer_size;
    AVStream *stream;
    int pcmhack;
    int aframesize;
    int aframecount;
    int64_t savepts;
    int framecount;
    int64_t lastpts;
    int sample_size;
    const void *sample_padding;
    double expected_next_pts;

    AVRational worst_time_base;
    int worst_time_base_is_stream;

    bool shutdown;
};

static void select_format(struct ao *ao, AVCodec *codec)
{
    int best_score = INT_MIN;
    int best_format = 0;

    // Check the encoder's list of supported formats.
    for (const enum AVSampleFormat *sampleformat = codec->sample_fmts;
         sampleformat && *sampleformat != AV_SAMPLE_FMT_NONE;
         ++sampleformat)
    {
        int fmt = af_from_avformat(*sampleformat);
        if (!fmt) {
            MP_WARN(ao, "unsupported lavc format %s\n",
                    av_get_sample_fmt_name(*sampleformat));
            continue;
        }
        int score = af_format_conversion_score(fmt, ao->format);
        if (score > best_score) {
            best_score = score;
            best_format = fmt;
        }
    }

    if (best_format) {
        ao->format = best_format;
    } else {
        MP_ERR(ao, "sample format not found\n"); // shouldn't happen
    }
}

// open & setup audio device
static int init(struct ao *ao)
{
    struct priv *ac = talloc_zero(ao, struct priv);
    AVCodec *codec;

    ao->priv = ac;

    if (!encode_lavc_available(ao->encode_lavc_ctx)) {
        MP_ERR(ao, "the option --o (output file) must be specified\n");
        return -1;
    }

    pthread_mutex_lock(&ao->encode_lavc_ctx->lock);

    ac->stream = encode_lavc_alloc_stream(ao->encode_lavc_ctx,
                                          AVMEDIA_TYPE_AUDIO);

    if (!ac->stream) {
        MP_ERR(ao, "could not get a new audio stream\n");
        goto fail;
    }

    codec = encode_lavc_get_codec(ao->encode_lavc_ctx, ac->stream);

    // TODO: Remove this redundancy with encode_lavc_alloc_stream also
    // setting the time base.
    // Using codec->time_bvase is deprecated, but needed for older lavf.
    ac->stream->time_base.num = 1;
    ac->stream->time_base.den = ao->samplerate;
    ac->stream->codec->time_base.num = 1;
    ac->stream->codec->time_base.den = ao->samplerate;

    ac->stream->codec->sample_rate = ao->samplerate;

    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_any(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        goto fail;
    mp_chmap_reorder_to_lavc(&ao->channels);
    ac->stream->codec->channels = ao->channels.num;
    ac->stream->codec->channel_layout = mp_chmap_to_lavc(&ao->channels);

    ac->stream->codec->sample_fmt = AV_SAMPLE_FMT_NONE;

    select_format(ao, codec);

    ac->sample_size = af_fmt2bps(ao->format);
    ac->stream->codec->sample_fmt = af_to_avformat(ao->format);
    ac->stream->codec->bits_per_raw_sample = ac->sample_size * 8;

    if (encode_lavc_open_codec(ao->encode_lavc_ctx, ac->stream) < 0)
        goto fail;

    ac->pcmhack = 0;
    if (ac->stream->codec->frame_size <= 1)
        ac->pcmhack = av_get_bits_per_sample(ac->stream->codec->codec_id) / 8;

    if (ac->pcmhack) {
        ac->aframesize = 16384; // "enough"
        ac->buffer_size =
            ac->aframesize * ac->pcmhack * ao->channels.num * 2 + 200;
    } else {
        ac->aframesize = ac->stream->codec->frame_size;
        ac->buffer_size =
            ac->aframesize * ac->sample_size * ao->channels.num * 2 + 200;
    }
    if (ac->buffer_size < FF_MIN_BUFFER_SIZE)
        ac->buffer_size = FF_MIN_BUFFER_SIZE;
    ac->buffer = talloc_size(ac, ac->buffer_size);

    // enough frames for at least 0.25 seconds
    ac->framecount = ceil(ao->samplerate * 0.25 / ac->aframesize);
    // but at least one!
    ac->framecount = FFMAX(ac->framecount, 1);

    ac->savepts = AV_NOPTS_VALUE;
    ac->lastpts = AV_NOPTS_VALUE;

    ao->untimed = true;

    pthread_mutex_unlock(&ao->encode_lavc_ctx->lock);
    return 0;

fail:
    pthread_mutex_unlock(&ao->encode_lavc_ctx->lock);
    ac->shutdown = true;
    return -1;
}

// close audio device
static int encode(struct ao *ao, double apts, void **data);
static void uninit(struct ao *ao)
{
    struct priv *ac = ao->priv;
    struct encode_lavc_context *ectx = ao->encode_lavc_ctx;

    if (!ac || ac->shutdown)
        return;

    pthread_mutex_lock(&ectx->lock);

    if (!encode_lavc_start(ectx)) {
        MP_WARN(ao, "not even ready to encode audio at end -> dropped\n");
        pthread_mutex_unlock(&ectx->lock);
        return;
    }

    if (ac->buffer) {
        double outpts = ac->expected_next_pts;
        if (!ectx->options->rawts && ectx->options->copyts)
            outpts += ectx->discontinuity_pts_offset;
        outpts += encode_lavc_getoffset(ectx, ac->stream);
        while (encode(ao, outpts, NULL) > 0) ;
    }

    pthread_mutex_unlock(&ectx->lock);

    ac->shutdown = true;
}

// return: how many bytes can be played without blocking
static int get_space(struct ao *ao)
{
    struct priv *ac = ao->priv;

    return ac->aframesize * ac->framecount;
}

// must get exactly ac->aframesize amount of data
static int encode(struct ao *ao, double apts, void **data)
{
    AVPacket packet;
    struct priv *ac = ao->priv;
    struct encode_lavc_context *ectx = ao->encode_lavc_ctx;
    double realapts = ac->aframecount * (double) ac->aframesize /
                      ao->samplerate;
    int status, gotpacket;

    ac->aframecount++;

    if (data)
        ectx->audio_pts_offset = realapts - apts;

    av_init_packet(&packet);
    packet.data = ac->buffer;
    packet.size = ac->buffer_size;
    if(data) {
        AVFrame *frame = av_frame_alloc();
        frame->format = af_to_avformat(ao->format);
        frame->nb_samples = ac->aframesize;

        size_t num_planes = AF_FORMAT_IS_PLANAR(ao->format) ? ao->channels.num : 1;
        assert(num_planes <= AV_NUM_DATA_POINTERS);
        for (int n = 0; n < num_planes; n++)
            frame->extended_data[n] = data[n];

        frame->linesize[0] = frame->nb_samples * ao->sstride;

        if (ectx->options->rawts || ectx->options->copyts) {
            // real audio pts
            frame->pts = floor(apts * ac->stream->codec->time_base.den / ac->stream->codec->time_base.num + 0.5);
        } else {
            // audio playback time
            frame->pts = floor(realapts * ac->stream->codec->time_base.den / ac->stream->codec->time_base.num + 0.5);
        }

        int64_t frame_pts = av_rescale_q(frame->pts, ac->stream->codec->time_base, ac->worst_time_base);
        if (ac->lastpts != AV_NOPTS_VALUE && frame_pts <= ac->lastpts) {
            // this indicates broken video
            // (video pts failing to increase fast enough to match audio)
            MP_WARN(ao, "audio frame pts went backwards (%d <- %d), autofixed\n",
                    (int)frame->pts, (int)ac->lastpts);
            frame_pts = ac->lastpts + 1;
            frame->pts = av_rescale_q(frame_pts, ac->worst_time_base, ac->stream->codec->time_base);
        }
        ac->lastpts = frame_pts;

        frame->quality = ac->stream->codec->global_quality;
        status = avcodec_encode_audio2(ac->stream->codec, &packet, frame, &gotpacket);

        if (!status) {
            if (ac->savepts == AV_NOPTS_VALUE)
                ac->savepts = frame->pts;
        }

        av_frame_free(&frame);
    }
    else
    {
        status = avcodec_encode_audio2(ac->stream->codec, &packet, NULL, &gotpacket);
    }

    if(status) {
        MP_ERR(ao, "error encoding\n");
        return -1;
    }

    if(!gotpacket)
        return 0;

    MP_DBG(ao, "got pts %f (playback time: %f); out size: %d\n",
           apts, realapts, packet.size);

    encode_lavc_write_stats(ao->encode_lavc_ctx, ac->stream);

    packet.stream_index = ac->stream->index;

    // Do we need this at all? Better be safe than sorry...
    if (packet.pts == AV_NOPTS_VALUE) {
        MP_WARN(ao, "encoder lost pts, why?\n");
        if (ac->savepts != MP_NOPTS_VALUE)
            packet.pts = ac->savepts;
    }

    if (packet.pts != AV_NOPTS_VALUE)
        packet.pts = av_rescale_q(packet.pts, ac->stream->codec->time_base,
                ac->stream->time_base);

    if (packet.dts != AV_NOPTS_VALUE)
        packet.dts = av_rescale_q(packet.dts, ac->stream->codec->time_base,
                ac->stream->time_base);

    if(packet.duration > 0)
        packet.duration = av_rescale_q(packet.duration, ac->stream->codec->time_base,
                ac->stream->time_base);

    ac->savepts = AV_NOPTS_VALUE;

    if (encode_lavc_write_frame(ao->encode_lavc_ctx, &packet) < 0) {
        MP_ERR(ao, "error writing at %f %f/%f\n",
               realapts, (double) ac->stream->time_base.num,
               (double) ac->stream->time_base.den);
        return -1;
    }

    return packet.size;
}

// this should round samples down to frame sizes
// return: number of samples played
static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *ac = ao->priv;
    struct encode_lavc_context *ectx = ao->encode_lavc_ctx;
    int bufpos = 0;
    double nextpts;
    double outpts;
    int orig_samples = samples;

    pthread_mutex_lock(&ectx->lock);

    if (!encode_lavc_start(ectx)) {
        MP_WARN(ao, "not ready yet for encoding audio\n");
        pthread_mutex_unlock(&ectx->lock);
        return 0;
    }

    double pts = ectx->last_audio_in_pts;
    pts += ectx->samples_since_last_pts / (double)ao->samplerate;

    size_t num_planes = AF_FORMAT_IS_PLANAR(ao->format) ? ao->channels.num : 1;

    void *tempdata = NULL;
    void *padded[MP_NUM_CHANNELS];

    if ((flags & AOPLAY_FINAL_CHUNK) && (samples % ac->aframesize)) {
       tempdata = talloc_new(NULL);
       size_t bytelen = samples * ao->sstride;
       size_t extralen = (ac->aframesize - 1) * ao->sstride;
       for (int n = 0; n < num_planes; n++) {
           padded[n] = talloc_size(tempdata, bytelen + extralen);
           memcpy(padded[n], data[n], bytelen);
           af_fill_silence((char *)padded[n] + bytelen, extralen, ao->format);
       }
       data = padded;
       samples = (bytelen + extralen) / ao->sstride;
    }

    if (pts == MP_NOPTS_VALUE) {
        MP_WARN(ao, "frame without pts, please report; synthesizing pts instead\n");
        // synthesize pts from previous expected next pts
        pts = ac->expected_next_pts;
    }

    if (ac->worst_time_base.den == 0) {
        //if (ac->stream->codec->time_base.num / ac->stream->codec->time_base.den >= ac->stream->time_base.num / ac->stream->time_base.den)
        if (ac->stream->codec->time_base.num * (double) ac->stream->time_base.den >=
                ac->stream->time_base.num * (double) ac->stream->codec->time_base.den) {
            MP_VERBOSE(ao, "NOTE: using codec time base (%d/%d) for pts "
                       "adjustment; the stream base (%d/%d) is not worse.\n",
                       (int)ac->stream->codec->time_base.num,
                       (int)ac->stream->codec->time_base.den,
                       (int)ac->stream->time_base.num,
                       (int)ac->stream->time_base.den);
            ac->worst_time_base = ac->stream->codec->time_base;
            ac->worst_time_base_is_stream = 0;
        } else {
            MP_WARN(ao, "NOTE: not using codec time base (%d/%d) for pts "
                    "adjustment; the stream base (%d/%d) is worse.\n",
                    (int)ac->stream->codec->time_base.num,
                    (int)ac->stream->codec->time_base.den,
                    (int)ac->stream->time_base.num,
                    (int)ac->stream->time_base.den);
            ac->worst_time_base = ac->stream->time_base;
            ac->worst_time_base_is_stream = 1;
        }

        // NOTE: we use the following "axiom" of av_rescale_q:
        // if time base A is worse than time base B, then
        //   av_rescale_q(av_rescale_q(x, A, B), B, A) == x
        // this can be proven as long as av_rescale_q rounds to nearest, which
        // it currently does

        // av_rescale_q(x, A, B) * B = "round x*A to nearest multiple of B"
        // and:
        //    av_rescale_q(av_rescale_q(x, A, B), B, A) * A
        // == "round av_rescale_q(x, A, B)*B to nearest multiple of A"
        // == "round 'round x*A to nearest multiple of B' to nearest multiple of A"
        //
        // assume this fails. Then there is a value of x*A, for which the
        // nearest multiple of B is outside the range [(x-0.5)*A, (x+0.5)*A[.
        // Absurd, as this range MUST contain at least one multiple of B.
    }

    // Fix and apply the discontinuity pts offset.
    if (!ectx->options->rawts && ectx->options->copyts) {
        // fix the discontinuity pts offset
        nextpts = pts;
        if (ectx->discontinuity_pts_offset == MP_NOPTS_VALUE) {
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        }
        else if (fabs(nextpts + ectx->discontinuity_pts_offset - ectx->next_in_pts) > 30) {
            MP_WARN(ao, "detected an unexpected discontinuity (pts jumped by "
                    "%f seconds)\n",
                    nextpts + ectx->discontinuity_pts_offset - ectx->next_in_pts);
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        }

        outpts = pts + ectx->discontinuity_pts_offset;
    }
    else {
        outpts = pts;
    }

    // Shift pts by the pts offset first.
    outpts += encode_lavc_getoffset(ectx, ac->stream);

    while (samples - bufpos >= ac->aframesize) {
        void *start[MP_NUM_CHANNELS] = {0};
        for (int n = 0; n < num_planes; n++)
            start[n] = (char *)data[n] + bufpos * ao->sstride;
        encode(ao, outpts + bufpos / (double) ao->samplerate, start);
        bufpos += ac->aframesize;
    }

    // Calculate expected pts of next audio frame (input side).
    ac->expected_next_pts = pts + bufpos / (double) ao->samplerate;

    // Set next allowed input pts value (input side).
    if (!ectx->options->rawts && ectx->options->copyts) {
        nextpts = ac->expected_next_pts + ectx->discontinuity_pts_offset;
        if (nextpts > ectx->next_in_pts)
            ectx->next_in_pts = nextpts;
    }

    talloc_free(tempdata);

    int taken = FFMIN(bufpos, orig_samples);
    ectx->samples_since_last_pts += taken;

    pthread_mutex_unlock(&ectx->lock);

    if (flags & AOPLAY_FINAL_CHUNK) {
        if (bufpos < orig_samples) {
            MP_ERR(ao, "did not write enough data at the end\n");
        }
    } else {
        if (bufpos > orig_samples) {
            MP_ERR(ao, "audio buffer overflow (should never happen)\n");
        }
    }

    return taken;
}

static void drain(struct ao *ao)
{
    // pretend we support it, so generic code doesn't force a wait
}

const struct ao_driver audio_out_lavc = {
    .encode = true,
    .description = "audio encoding using libavcodec",
    .name      = "lavc",
    .init      = init,
    .uninit    = uninit,
    .get_space = get_space,
    .play      = play,
    .drain     = drain,
};
