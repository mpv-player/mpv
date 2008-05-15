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

#include "config.h"
#include "af.h"
#include "help_mp.h"
#include "reorder_ch.h"

#include "libavcodec/avcodec.h"
#include "libavcodec/ac3.h"

// Data for specific instances of this filter
typedef struct af_ac3enc_s {
    struct AVCodec        *lavc_acodec;
    struct AVCodecContext *lavc_actx;
    int add_iec61937_header;
    int bit_rate;
    char *pending_data;
    int pending_len;
    int expect_len;
    int min_channel_num;
} af_ac3enc_t;

extern int  avcodec_initialized;

// Initialization and runtime control
static int control(struct af_instance_s *af, int cmd, void *arg)
{
    af_ac3enc_t *s  = (af_ac3enc_t *)af->setup;
    af_data_t *data = (af_data_t *)arg;
    int i, bit_rate, test_output_res;
    static const int default_bit_rate[AC3_MAX_CHANNELS+1] = \
        {0, 96000, 192000, 256000, 384000, 448000, 448000};

    switch (cmd){
    case AF_CONTROL_REINIT:
        if (data->format == AF_FORMAT_AC3 || data->nch < s->min_channel_num)
            return AF_DETACH;

        s->pending_len = 0;
        s->expect_len = AC3_FRAME_SIZE * data->nch * data->bps;
        if (s->add_iec61937_header)
            af->mul = (double)AC3_FRAME_SIZE * 2 * 2 / s->expect_len;
        else
            af->mul = (double)AC3_MAX_CODED_FRAME_SIZE / s->expect_len;

        af_msg(AF_MSG_DEBUG0, "af_lavcac3enc reinit: %d, %d, %f, %d.\n",
               data->nch, data->rate, af->mul, s->expect_len);

        af->data->format = AF_FORMAT_S16_NE;
        if (data->rate == 48000 || data->rate == 44100 || data->rate == 32000)
            af->data->rate = data->rate;
        else
            af->data->rate = 48000;
        if (data->nch > AC3_MAX_CHANNELS)
            af->data->nch = AC3_MAX_CHANNELS;
        else
            af->data->nch = data->nch;
        af->data->bps = 2;
        test_output_res = af_test_output(af, data);

        bit_rate = s->bit_rate ? s->bit_rate : default_bit_rate[af->data->nch];

        if (s->lavc_actx->channels != af->data->nch ||
                s->lavc_actx->sample_rate != af->data->rate ||
                s->lavc_actx->bit_rate != bit_rate) {

            if (s->lavc_actx->codec)
                avcodec_close(s->lavc_actx);

            // Put sample parameters
            s->lavc_actx->channels = af->data->nch;
            s->lavc_actx->sample_rate = af->data->rate;
            s->lavc_actx->bit_rate = bit_rate;

            if(avcodec_open(s->lavc_actx, s->lavc_acodec) < 0) {
                af_msg(AF_MSG_ERROR, MSGTR_CouldntOpenCodec, "ac3", bit_rate);
                return AF_ERROR;
            }
        }
        af->data->format = AF_FORMAT_AC3;
        af->data->nch = 2;
        return test_output_res;
    case AF_CONTROL_COMMAND_LINE:
        af_msg(AF_MSG_DEBUG0, "af_lavcac3enc cmdline: %s.\n", (char*)arg);
        s->bit_rate = 0;
        s->min_channel_num = 0;
        s->add_iec61937_header = 0;
        sscanf((char*)arg,"%d:%d:%d", &s->add_iec61937_header, &s->bit_rate,
               &s->min_channel_num);
        if (s->bit_rate < 1000)
            s->bit_rate *= 1000;
        if (s->bit_rate) {
            for (i = 0; i < 19; ++i)
                if (ff_ac3_bitrate_tab[i] * 1000 == s->bit_rate)
                    break;
            if (i >= 19) {
                af_msg(AF_MSG_WARN, "af_lavcac3enc unable set unsupported "
                       "bitrate %d, use default bitrate (check manpage to see "
                       "supported bitrates).\n", s->bit_rate);
                s->bit_rate = 0;
            }
        }
        if (s->min_channel_num == 0)
            s->min_channel_num = 5;
        af_msg(AF_MSG_VERBOSE, "af_lavcac3enc config spdif:%d, bitrate:%d, "
               "minchnum:%d.\n", s->add_iec61937_header, s->bit_rate,
               s->min_channel_num);
        return AF_OK;
    }
    return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance_s* af)
{
    if (af->data)
        free(af->data->audio);
    free(af->data);
    if (af->setup) {
        af_ac3enc_t *s = af->setup;
        af->setup = NULL;
        if(s->lavc_actx) {
            if (s->lavc_actx->codec)
                avcodec_close(s->lavc_actx);
            free(s->lavc_actx);
        }
        free(s->pending_data);
        free(s);
    }
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
    af_ac3enc_t *s = af->setup;
    af_data_t *c = data;    // Current working data
    af_data_t *l;
    int len, left, outsize = 0, destsize;
    char *buf, *src, *dest;
    int max_output_len;
    int frame_num = (data->len + s->pending_len) / s->expect_len;

    if (s->add_iec61937_header)
        max_output_len = AC3_FRAME_SIZE * 2 * 2 * frame_num;
    else
        max_output_len = AC3_MAX_CODED_FRAME_SIZE * frame_num;

    if (af->data->len < max_output_len) {
        af_msg(AF_MSG_VERBOSE,"[libaf] Reallocating memory in module %s, "
               "old len = %i, new len = %i\n", af->info->name, af->data->len,
                max_output_len);
        free(af->data->audio);
        af->data->audio = malloc(max_output_len);
        if (!af->data->audio) {
            af_msg(AF_MSG_FATAL,"[libaf] Could not allocate memory \n");
            return NULL;
        }
        af->data->len = max_output_len;
    }

    l = af->data;           // Local data
    buf = (char *)l->audio;
    src = (char *)c->audio;
    left = c->len;


    while (left > 0) {
        if (left + s->pending_len < s->expect_len) {
            memcpy(s->pending_data + s->pending_len, src, left);
            src += left;
            s->pending_len += left;
            left = 0;
            break;
        }

        dest = s->add_iec61937_header ? buf + 8 : buf;
        destsize = (char *)l->audio + l->len - buf;

        if (s->pending_len) {
            int needs = s->expect_len - s->pending_len;
            if (needs > 0) {
                memcpy(s->pending_data + s->pending_len, src, needs);
                src += needs;
                left -= needs;
            }

            if (c->nch >= 5)
                reorder_channel_nch(s->pending_data,
                                    AF_CHANNEL_LAYOUT_MPLAYER_DEFAULT,
                                    AF_CHANNEL_LAYOUT_LAVC_AC3_DEFAULT,
                                    c->nch,
                                    s->expect_len / 2, 2);

            len = avcodec_encode_audio(s->lavc_actx, dest, destsize,
                                       (void *)s->pending_data);
            s->pending_len = 0;
        }
        else {
            if (c->nch >= 5)
                reorder_channel_nch(src,
                                    AF_CHANNEL_LAYOUT_MPLAYER_DEFAULT,
                                    AF_CHANNEL_LAYOUT_LAVC_AC3_DEFAULT,
                                    c->nch,
                                    s->expect_len / 2, 2);
            len = avcodec_encode_audio(s->lavc_actx,dest,destsize,(void *)src);
            src += s->expect_len;
            left -= s->expect_len;
        }
        af_msg(AF_MSG_DEBUG0, "avcodec_encode_audio got %d, pending %d.\n",
               len, s->pending_len);

        if (s->add_iec61937_header) {
            int16_t *out = (int16_t *)buf;
            int bsmod = dest[5] & 0x7;

#ifndef WORDS_BIGENDIAN
            int i;
            char tmp;
            for (i = 0; i < len; i += 2) {
                tmp = dest[i];
                dest[i] = dest[i+1];
                dest[i+1] = tmp;
            }
            if (len & 1) {
                dest[len] = dest[len-1];
                dest[len-1] = 0;
                len++;
            }
#endif
            out[0] = 0xF872;   // iec 61937 syncword 1
            out[1] = 0x4E1F;   // iec 61937 syncword 2
            out[2] = 0x0001;   // data-type ac3
            out[2] |= bsmod << 8; // bsmod
            out[3] = len << 3; // number of bits in payload

            memset(buf + 8 + len, 0, AC3_FRAME_SIZE * 2 * 2 - 8 - len);
            len = AC3_FRAME_SIZE * 2 * 2;
        }

        outsize += len;
        buf += len;
    }
    c->audio = l->audio;
    c->nch   = 2;
    c->bps   = 2;
    c->len   = outsize;
    af_msg(AF_MSG_DEBUG0, "play return size %d, pending %d\n",
           outsize, s->pending_len);
    return c;
}

static int af_open(af_instance_t* af){

    af_ac3enc_t *s = calloc(1,sizeof(af_ac3enc_t));
    int pending_space = 2 * AC3_MAX_CHANNELS * AC3_FRAME_SIZE;
    s->pending_data = calloc(pending_space, sizeof(char));

    af->control=control;
    af->uninit=uninit;
    af->play=play;
    af->mul=1;
    af->data=calloc(1,sizeof(af_data_t));
    af->setup=s;

    if (!avcodec_initialized){
        avcodec_init();
        avcodec_register_all();
        avcodec_initialized=1;
    }

    s->lavc_acodec = avcodec_find_encoder_by_name("ac3");
    if (!s->lavc_acodec) {
        af_msg(AF_MSG_ERROR, MSGTR_LavcAudioCodecNotFound, "ac3");
        return AF_ERROR;
    }

    s->lavc_actx = avcodec_alloc_context();
    if (!s->lavc_actx) {
        af_msg(AF_MSG_ERROR, MSGTR_CouldntAllocateLavcContext);
        return AF_ERROR;
    }

    return AF_OK;
}

af_info_t af_info_lavcac3enc = {
    "runtime encode to ac3 using libavcodec",
    "lavcac3enc",
    "Ulion",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
