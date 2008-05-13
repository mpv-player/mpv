/*
 * DTS Coherent Acoustics stream decoder using libdca
 * This file is partially based on dtsdec.c r9036 from FFmpeg and ad_liba52.c
 *
 * Copyright (C) 2007 Roberto Togni
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
#include <unistd.h>
#include <assert.h>
#include "config.h"

#include "mp_msg.h"
#include "ad_internal.h"

#include <dts.h>

static ad_info_t info =
{
    "DTS decoding with libdca",
    "libdca",
    "Roberto Togni",
    "",
    ""
};

LIBAD_EXTERN(libdca)

#define DTSBUFFER_SIZE 18726
#define HEADER_SIZE 14

#define CONVERT_LEVEL 1
#define CONVERT_BIAS 0

static const char ch2flags[6] = {
    DTS_MONO,
    DTS_STEREO,
    DTS_3F,
    DTS_2F2R,
    DTS_3F2R,
    DTS_3F2R | DTS_LFE
};

static inline int16_t convert(sample_t s)
{
    int i = s * 0x7fff;

    return (i > 32767) ? 32767 : ((i < -32768) ? -32768 : i);
}

static void convert2s16_multi(sample_t *f, int16_t *s16, int flags, int ch_out)
{
    int i;

    switch(flags & (DTS_CHANNEL_MASK | DTS_LFE)){
    case DTS_MONO:
        if (ch_out == 1)
            for(i = 0; i < 256; i++)
                s16[i] = convert(f[i]);
        else
            for(i = 0; i < 256; i++){
                s16[5*i] = s16[5*i+1] = s16[5*i+2] = s16[5*i+3] = 0;
                s16[5*i+4] = convert(f[i]);
            }
        break;
    case DTS_CHANNEL:
    case DTS_STEREO:
    case DTS_DOLBY:
        for(i = 0; i < 256; i++){
            s16[2*i] = convert(f[i]);
            s16[2*i+1] = convert(f[i+256]);
        }
        break;
    case DTS_3F:
        for(i = 0; i < 256; i++){
            s16[3*i] = convert(f[i+256]);
            s16[3*i+1] = convert(f[i+512]);
            s16[3*i+2] = convert(f[i]);
        }
        break;
    case DTS_2F2R:
        for(i = 0; i < 256; i++){
            s16[4*i] = convert(f[i]);
            s16[4*i+1] = convert(f[i+256]);
            s16[4*i+2] = convert(f[i+512]);
            s16[4*i+3] = convert(f[i+768]);
        }
        break;
    case DTS_3F2R:
        for(i = 0; i < 256; i++){
            s16[5*i] = convert(f[i+256]);
            s16[5*i+1] = convert(f[i+512]);
            s16[5*i+2] = convert(f[i+768]);
            s16[5*i+3] = convert(f[i+1024]);
            s16[5*i+4] = convert(f[i]);
        }
        break;
    case DTS_MONO | DTS_LFE:
        for(i = 0; i < 256; i++){
            s16[6*i] = s16[6*i+1] = s16[6*i+2] = s16[6*i+3] = 0;
            s16[6*i+4] = convert(f[i]);
            s16[6*i+5] = convert(f[i+256]);
        }
        break;
    case DTS_CHANNEL | DTS_LFE:
    case DTS_STEREO | DTS_LFE:
    case DTS_DOLBY | DTS_LFE:
        for(i = 0; i < 256; i++){
            s16[6*i] = convert(f[i]);
            s16[6*i+1] = convert(f[i+256]);
            s16[6*i+2] = s16[6*i+3] = s16[6*i+4] = 0;
            s16[6*i+5] = convert(f[i+512]);
        }
        break;
    case DTS_3F | DTS_LFE:
        for(i = 0; i < 256; i++){
            s16[6*i] = convert(f[i+256]);
            s16[6*i+1] = convert(f[i+512]);
            s16[6*i+2] = s16[6*i+3] = 0;
            s16[6*i+4] = convert(f[i]);
            s16[6*i+5] = convert(f[i+768]);
        }
        break;
    case DTS_2F2R | DTS_LFE:
        for(i = 0; i < 256; i++){
            s16[6*i] = convert(f[i]);
            s16[6*i+1] = convert(f[i+256]);
            s16[6*i+2] = convert(f[i+512]);
            s16[6*i+3] = convert(f[i+768]);
            s16[6*i+4] = 0;
            s16[6*i+5] = convert(f[1024]);
        }
        break;
    case DTS_3F2R | DTS_LFE:
        for(i = 0; i < 256; i++){
            s16[6*i] = convert(f[i+256]);
            s16[6*i+1] = convert(f[i+512]);
            s16[6*i+2] = convert(f[i+768]);
            s16[6*i+3] = convert(f[i+1024]);
            s16[6*i+4] = convert(f[i]);
            s16[6*i+5] = convert(f[i+1280]);
        }
        break;
    }
}

static void channels_info(int flags)
{
    int lfe = 0;
    char lfestr[5] = "";

    if (flags & DTS_LFE) {
        lfe = 1;
        strcpy(lfestr, "+lfe");
    }
    mp_msg(MSGT_DECAUDIO, MSGL_V, "DTS: ");
    switch(flags & DTS_CHANNEL_MASK){
    case DTS_MONO:
        mp_msg(MSGT_DECAUDIO, MSGL_V, "1.%d (mono%s)", lfe, lfestr);
        break;
    case DTS_CHANNEL:
        mp_msg(MSGT_DECAUDIO, MSGL_V, "2.%d (channel%s)", lfe, lfestr);
        break;
    case DTS_STEREO:
        mp_msg(MSGT_DECAUDIO, MSGL_V, "2.%d (stereo%s)", lfe, lfestr);
        break;
    case DTS_3F:
        mp_msg(MSGT_DECAUDIO, MSGL_V, "3.%d (3f%s)", lfe, lfestr);
        break;
    case DTS_2F2R:
        mp_msg(MSGT_DECAUDIO, MSGL_V, "4.%d (2f+2r%s)", lfe, lfestr);
        break;
    case DTS_3F2R:
        mp_msg(MSGT_DECAUDIO, MSGL_V, "5.%d (3f+2r%s)", lfe, lfestr);
        break;
    default:
        mp_msg(MSGT_DECAUDIO, MSGL_V, "x.%d (unknown%s)", lfe, lfestr);
    }
    mp_msg(MSGT_DECAUDIO, MSGL_V, "\n");
}

static int dts_sync(sh_audio_t *sh, int *flags)
{
    dts_state_t *s = sh->context;
    int length;
    int sample_rate;
    int frame_length;
    int bit_rate;

    sh->a_in_buffer_len=0;

    while(1) {
        while(sh->a_in_buffer_len < HEADER_SIZE) {
            int c = demux_getc(sh->ds);

            if(c < 0)
                return -1;
            sh->a_in_buffer[sh->a_in_buffer_len++] = c;
        }

        length = dts_syncinfo(s, sh->a_in_buffer, flags, &sample_rate,
                              &bit_rate, &frame_length);

        if(length >= HEADER_SIZE)
            break;

//        mp_msg(MSGT_DECAUDIO, MSGL_V, "skip\n");
        memmove(sh->a_in_buffer, sh->a_in_buffer+1, HEADER_SIZE-1);
        --sh->a_in_buffer_len;
    }

    demux_read_data(sh->ds, sh->a_in_buffer + HEADER_SIZE, length - HEADER_SIZE);

    sh->samplerate = sample_rate;
    sh->i_bps = bit_rate/8;

    return length;
}

static int decode_audio(sh_audio_t *sh, unsigned char *buf, int minlen, int maxlen)
{
    dts_state_t *s = sh->context;
    int16_t *out_samples = (int16_t*)buf;
    int flags;
    level_t level;
    sample_t bias;
    int nblocks;
    int i;
    int data_size = 0;

    if(!sh->a_in_buffer_len)
        if(dts_sync(sh, &flags) < 0) return -1; /* EOF */
    sh->a_in_buffer_len=0;

    flags &= ~(DTS_CHANNEL_MASK | DTS_LFE);
    flags |= ch2flags[sh->channels - 1];

    level = CONVERT_LEVEL;
    bias = CONVERT_BIAS;
    flags |= DTS_ADJUST_LEVEL;
    if(dts_frame(s, sh->a_in_buffer, &flags, &level, bias)) {
        mp_msg(MSGT_DECAUDIO, MSGL_ERR, "dts_frame() failed\n");
        goto end;
    }

    nblocks = dts_blocks_num(s);

    for(i = 0; i < nblocks; i++) {
        if(dts_block(s)) {
            mp_msg(MSGT_DECAUDIO, MSGL_ERR, "dts_block() failed\n");
            goto end;
        }

        convert2s16_multi(dts_samples(s), out_samples, flags, sh->channels);

        out_samples += 256 * sh->channels;
        data_size += 256 * sizeof(int16_t) * sh->channels;
    }

end:
    return data_size;
}

static int preinit(sh_audio_t *sh)
{
    /* 256 = samples per block, 16 = max number of blocks */
    sh->audio_out_minsize = audio_output_channels * sizeof(int16_t) * 256 * 16;
    sh->audio_in_minsize = DTSBUFFER_SIZE;
    sh->samplesize=2;

    return 1;
}

static int init(sh_audio_t *sh)
{
    dts_state_t *s;
    int flags;
    int decoded_bytes;

    s = dts_init(0);
    if(s == NULL) {
        mp_msg(MSGT_DECAUDIO, MSGL_ERR, "dts_init() failed\n");
        return 0;
    }
    sh->context = s;

    if(dts_sync(sh, &flags) < 0) {
        mp_msg(MSGT_DECAUDIO, MSGL_ERR, "dts sync failed\n");
        dts_free(s);
        return 0;
    }
    channels_info(flags);

    assert(audio_output_channels >= 1 && audio_output_channels <= 6);
    sh->channels = audio_output_channels;

    decoded_bytes = decode_audio(sh, sh->a_buffer, 1, sh->a_buffer_size);
    if(decoded_bytes > 0)
        sh->a_buffer_len = decoded_bytes;
    else {
        mp_msg(MSGT_DECAUDIO, MSGL_ERR, "dts decode failed on first frame (up/downmix problem?)\n");
        dts_free(s);
        return 0;
    }

    return 1;
}

static void uninit(sh_audio_t *sh)
{
    dts_state_t *s = sh->context;

    dts_free(s);
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    int flags;

    switch(cmd){
        case ADCTRL_RESYNC_STREAM:
            dts_sync(sh, &flags);
            return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}
