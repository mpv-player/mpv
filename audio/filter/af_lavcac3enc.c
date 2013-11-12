/*
 * audio filter for runtime AC-3 encoding with libavcodec.
 *
 * Copyright (C) 2007 Ulion <ulion A gmail P com>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/audioconvert.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/common.h>
#include <libavutil/mem.h>

#include "config.h"
#include "af.h"
#include "audio/audio_buffer.h"
#include "audio/fmt-conversion.h"


#define AC3_MAX_CHANNELS 6
#define AC3_MAX_CODED_FRAME_SIZE 3840
#define AC3_FRAME_SIZE (6  * 256)
const uint16_t ac3_bitrate_tab[19] = {
    32, 40, 48, 56, 64, 80, 96, 112, 128,
    160, 192, 224, 256, 320, 384, 448, 512, 576, 640
};

// Data for specific instances of this filter
typedef struct af_ac3enc_s {
    struct AVCodec        *lavc_acodec;
    struct AVCodecContext *lavc_actx;
    AVPacket pkt;
    bool planarize;
    int add_iec61937_header;
    int bit_rate;
    struct mp_audio_buffer *pending;
    int in_samples;     // samples of input per AC3 frame
    int out_samples;    // upper bound on encoded output per AC3 frame
    int min_channel_num;
    int in_sampleformat;
} af_ac3enc_t;

// Initialization and runtime control
static int control(struct af_instance *af, int cmd, void *arg)
{
    af_ac3enc_t *s  = af->setup;
    int i, bit_rate;
    static const int default_bit_rate[AC3_MAX_CHANNELS+1] = \
        {0, 96000, 192000, 256000, 384000, 448000, 448000};

    switch (cmd){
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;
        struct mp_audio orig_in = *in;

        if (AF_FORMAT_IS_AC3(in->format) || in->nch < s->min_channel_num)
            return AF_DETACH;

        mp_audio_set_format(in, s->in_sampleformat);

        if (in->rate != 48000 && in->rate != 44100 && in->rate != 32000)
            in->rate = 48000;
        af->data->rate = in->rate;

        mp_chmap_reorder_to_lavc(&in->channels);
        if (in->nch > AC3_MAX_CHANNELS)
            mp_audio_set_num_channels(in, AC3_MAX_CHANNELS);

        mp_audio_set_format(af->data, AF_FORMAT_AC3_BE);
        mp_audio_set_num_channels(af->data, 2);

        if (!mp_audio_config_equals(in, &orig_in))
            return AF_FALSE;

        s->in_samples = AC3_FRAME_SIZE;
        if (s->add_iec61937_header) {
            s->out_samples = AC3_FRAME_SIZE;
        } else {
            s->out_samples = AC3_MAX_CODED_FRAME_SIZE / af->data->sstride;
        }
        af->mul = s->out_samples / (double)s->in_samples;

        mp_audio_buffer_reinit(s->pending, in);

        mp_msg(MSGT_AFILTER, MSGL_DBG2, "af_lavcac3enc reinit: %d, %d, %f, %d.\n",
               in->nch, in->rate, af->mul, s->in_samples);

        bit_rate = s->bit_rate ? s->bit_rate : default_bit_rate[in->nch];

        if (s->lavc_actx->channels != in->nch ||
            s->lavc_actx->sample_rate != in->rate ||
            s->lavc_actx->bit_rate != bit_rate)
        {
            avcodec_close(s->lavc_actx);

            // Put sample parameters
            s->lavc_actx->channels = in->nch;
            s->lavc_actx->channel_layout = mp_chmap_to_lavc(&in->channels);
            s->lavc_actx->sample_rate = in->rate;
            s->lavc_actx->bit_rate = bit_rate;

            if (avcodec_open2(s->lavc_actx, s->lavc_acodec, NULL) < 0) {
                mp_tmsg(MSGT_AFILTER, MSGL_ERR, "Couldn't open codec %s, br=%d.\n", "ac3", bit_rate);
                return AF_ERROR;
            }
        }
        if (s->lavc_actx->frame_size != AC3_FRAME_SIZE) {
            mp_msg(MSGT_AFILTER, MSGL_ERR, "lavcac3enc: unexpected ac3 "
                   "encoder frame size %d\n", s->lavc_actx->frame_size);
            return AF_ERROR;
        }
        return AF_OK;
    }
    case AF_CONTROL_COMMAND_LINE:
        mp_msg(MSGT_AFILTER, MSGL_DBG2, "af_lavcac3enc cmdline: %s.\n", (char*)arg);
        s->bit_rate = 0;
        s->min_channel_num = 0;
        s->add_iec61937_header = 0;
        sscanf(arg,"%d:%d:%d", &s->add_iec61937_header, &s->bit_rate,
               &s->min_channel_num);
        if (s->bit_rate < 1000)
            s->bit_rate *= 1000;
        if (s->bit_rate) {
            for (i = 0; i < 19; ++i)
                if (ac3_bitrate_tab[i] * 1000 == s->bit_rate)
                    break;
            if (i >= 19) {
                mp_msg(MSGT_AFILTER, MSGL_WARN, "af_lavcac3enc unable set unsupported "
                       "bitrate %d, use default bitrate (check manpage to see "
                       "supported bitrates).\n", s->bit_rate);
                s->bit_rate = 0;
            }
        }
        if (s->min_channel_num == 0)
            s->min_channel_num = 5;
        mp_msg(MSGT_AFILTER, MSGL_V, "af_lavcac3enc config spdif:%d, bitrate:%d, "
               "minchnum:%d.\n", s->add_iec61937_header, s->bit_rate,
               s->min_channel_num);
        return AF_OK;
    }
    return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance* af)
{
    af_ac3enc_t *s = af->setup;

    if (s) {
        av_free_packet(&s->pkt);
        if(s->lavc_actx) {
            avcodec_close(s->lavc_actx);
            av_free(s->lavc_actx);
        }
    }
}

// Filter data through filter
static struct mp_audio* play(struct af_instance* af, struct mp_audio* audio)
{
    struct mp_audio *out = af->data;
    af_ac3enc_t *s = af->setup;
    int num_frames = (audio->samples + mp_audio_buffer_samples(s->pending))
                     / s->in_samples;

    int max_out_samples = s->out_samples * num_frames;
    mp_audio_realloc_min(out, max_out_samples);
    out->samples = 0;

    while (audio->samples > 0) {
        int ret;

        int consumed_pending = 0;
        struct mp_audio in_frame;
        int pending = mp_audio_buffer_samples(s->pending);
        if (pending == 0 && audio->samples >= s->in_samples) {
            in_frame = *audio;
            mp_audio_skip_samples(audio, s->in_samples);
        } else {
            if (pending > 0 && pending < s->in_samples) {
                struct mp_audio tmp = *audio;
                tmp.samples = MPMIN(tmp.samples, s->in_samples);
                mp_audio_buffer_append(s->pending, &tmp);
                mp_audio_skip_samples(audio, tmp.samples);
            }
            mp_audio_buffer_peek(s->pending, &in_frame);
            if (in_frame.samples < s->in_samples)
                break;
            consumed_pending = s->in_samples;
        }
        in_frame.samples = s->in_samples;

        AVFrame *frame = avcodec_alloc_frame();
        if (!frame) {
            mp_msg(MSGT_AFILTER, MSGL_FATAL, "[libaf] Could not allocate memory \n");
            return NULL;
        }
        frame->nb_samples = s->in_samples;
        frame->format = s->lavc_actx->sample_fmt;
        frame->channel_layout = s->lavc_actx->channel_layout;
        assert(in_frame.num_planes <= AV_NUM_DATA_POINTERS);
        for (int n = 0; n < in_frame.num_planes; n++)
            frame->data[n] = in_frame.planes[n];
        frame->linesize[0] = s->in_samples * audio->sstride;

        int ok;
        ret = avcodec_encode_audio2(s->lavc_actx, &s->pkt, frame, &ok);
        if (ret < 0 || !ok) {
            mp_msg(MSGT_AFILTER, MSGL_FATAL, "[lavac3enc] Encode failed.\n");
            return NULL;
        }

        avcodec_free_frame(&frame);

        mp_audio_buffer_skip(s->pending, consumed_pending);

        mp_msg(MSGT_AFILTER, MSGL_DBG2, "avcodec_encode_audio got %d, pending %d.\n",
               s->pkt.size, mp_audio_buffer_samples(s->pending));

        int frame_size = s->pkt.size;
        int header_len = 0;
        char hdr[8];

        if (s->add_iec61937_header && s->pkt.size > 5) {
            int bsmod = s->pkt.data[5] & 0x7;
            int len = frame_size;

            frame_size = AC3_FRAME_SIZE * 2 * 2;
            header_len = 8;

            AV_WB16(hdr,     0xF872);   // iec 61937 syncword 1
            AV_WB16(hdr + 2, 0x4E1F);   // iec 61937 syncword 2
            hdr[4] = bsmod;             // bsmod
            hdr[5] = 0x01;              // data-type ac3
            AV_WB16(hdr + 6, len << 3); // number of bits in payload
        }

        size_t max_size = (max_out_samples - out->samples) * out->sstride;
        if (frame_size > max_size)
            abort();

        char *buf = (char *)out->planes[0] + out->samples * out->sstride;
        memcpy(buf, hdr, header_len);
        memcpy(buf + header_len, s->pkt.data, s->pkt.size);
        memset(buf + header_len + s->pkt.size, 0,
               frame_size - (header_len + s->pkt.size));
        out->samples += frame_size / out->sstride;
    }

    mp_audio_buffer_append(s->pending, audio);

    *audio = *out;
    return audio;
}

static int af_open(struct af_instance* af){

    af_ac3enc_t *s = talloc_zero(af, af_ac3enc_t);
    af->control=control;
    af->uninit=uninit;
    af->play=play;
    af->setup=s;

    s->lavc_acodec = avcodec_find_encoder_by_name("ac3");
    if (!s->lavc_acodec) {
        mp_tmsg(MSGT_AFILTER, MSGL_ERR, "Audio LAVC, couldn't find encoder for codec %s.\n", "ac3");
        return AF_ERROR;
    }

    s->lavc_actx = avcodec_alloc_context3(s->lavc_acodec);
    if (!s->lavc_actx) {
        mp_tmsg(MSGT_AFILTER, MSGL_ERR, "Audio LAVC, couldn't allocate context!\n");
        return AF_ERROR;
    }
    const enum AVSampleFormat *fmts = s->lavc_acodec->sample_fmts;
    for (int i = 0; fmts[i] != AV_SAMPLE_FMT_NONE; i++) {
        s->in_sampleformat = af_from_avformat(fmts[i]);
        if (s->in_sampleformat) {
            s->lavc_actx->sample_fmt = fmts[i];
            break;
        }
    }
    if (!s->in_sampleformat) {
        mp_msg(MSGT_AFILTER, MSGL_ERR, "Audio LAVC, encoder doesn't "
               "support expected sample formats!\n");
        return AF_ERROR;
    }
    mp_msg(MSGT_AFILTER, MSGL_V, "[af_lavcac3enc]: in sample format: %s\n",
           af_fmt_to_str(s->in_sampleformat));

    av_init_packet(&s->pkt);

    s->pending = mp_audio_buffer_create(af);

    return AF_OK;
}

struct af_info af_info_lavcac3enc = {
    .info = "runtime encode to ac3 using libavcodec",
    .name = "lavcac3enc",
    .open = af_open,
};
