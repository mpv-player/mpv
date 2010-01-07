/*
 * OS/2 DART audio output driver
 *
 * Copyright (c) 2007-2009 by KO Myung-Hun (komh@chollian.net)
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

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <float.h>

#include <dart.h>

#include "config.h"
#include "libaf/af_format.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "mp_msg.h"
#include "libvo/fastmemcpy.h"
#include "subopt-helper.h"
#include "libavutil/fifo.h"

static const ao_info_t info = {
    "DART audio output",
    "dart",
    "KO Myung-Hun <komh@chollian.net>",
    ""
};

LIBAO_EXTERN(dart)

#define OUTBURST_SAMPLES        512
#define DEFAULT_DART_SAMPLES    (OUTBURST_SAMPLES << 2)

#define CHUNK_SIZE  ao_data.outburst

static AVFifoBuffer *m_audioBuf;

static int m_nBufSize = 0;

static volatile int m_fQuit = FALSE;

static int write_buffer(unsigned char *data, int len)
{
    int nFree = av_fifo_space(m_audioBuf);

    if (len > nFree)
        len = nFree;

    return av_fifo_generic_write(m_audioBuf, data, len, NULL);
}

static int read_buffer(unsigned char *data, int len)
{
    int nBuffered = av_fifo_size(m_audioBuf);

    if (len > nBuffered)
        len = nBuffered;

    av_fifo_generic_read(m_audioBuf, data, len, NULL);
    return len;
}

// end ring buffer stuff

static ULONG APIENTRY dart_audio_callback(PVOID pCBData, PVOID pBuffer,
                                          ULONG ulSize)
{
    int nReadLen;

    nReadLen = read_buffer(pBuffer, ulSize);
    if (nReadLen < ulSize && !m_fQuit) {
        memset((uint8_t *)pBuffer + nReadLen, DART.bSilence, ulSize - nReadLen);
        nReadLen = ulSize;
    }

    return nReadLen;
}

// to set/get/query special features/parameters
static int control(int cmd, void *arg)
{
    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        {
        ao_control_vol_t *vol = arg;

        vol->left = vol->right = LOUSHORT(dartGetVolume());

        return CONTROL_OK;
        }

    case AOCONTROL_SET_VOLUME:
        {
        int mid;
        ao_control_vol_t *vol = arg;

        mid = (vol->left + vol->right) / 2;
        dartSetVolume(MCI_SET_AUDIO_ALL, mid);

        return CONTROL_OK;
        }
    }

    return CONTROL_UNKNOWN;
}

static void print_help(void)
{
    mp_msg(MSGT_AO, MSGL_FATAL,
           "\n-ao dart commandline help:\n"
           "Example: mplayer -ao dart:noshare\n"
           "    open DART in exclusive mode\n"
           "\nOptions:\n"
           "    (no)share\n"
           "        Open DART in shareable or exclusive mode\n"
           "    bufsize=<size>\n"
           "        Set buffer size to <size> in samples(default: 2048)\n");
}

// open & set up audio device
// return: 1=success 0=fail
static int init(int rate, int channels, int format, int flags)
{
    int fShare = 1;
    int nDartSamples = DEFAULT_DART_SAMPLES;
    int nBytesPerSample;

    const opt_t subopts[] = {
        {"share", OPT_ARG_BOOL, &fShare, NULL},
        {"bufsize", OPT_ARG_INT, &nDartSamples, int_non_neg},
        {NULL}
    };

    if (subopt_parse(ao_subdevice, subopts) != 0) {
        print_help();
        return 0;
    }

    if (!nDartSamples)
        nDartSamples = DEFAULT_DART_SAMPLES;

    mp_msg(MSGT_AO, MSGL_V, "DART: opened in %s mode, buffer size = %d sample(s)\n",
           fShare ? "shareable" : "exclusive", nDartSamples);

    switch (format) {
    case AF_FORMAT_S16_LE:
    case AF_FORMAT_S8:
        break;

    default:
        format = AF_FORMAT_S16_LE;
        mp_msg(MSGT_AO, MSGL_V, "DART: format %s not supported defaulting to Signed 16-bit Little-Endian\n",
               af_fmt2str_short(format));
        break;
    }

    nBytesPerSample = (af_fmt2bits(format) >> 3) * channels;

    if (dartInit(0, af_fmt2bits(format), rate, MCI_WAVE_FORMAT_PCM, channels,
                 2, nBytesPerSample * nDartSamples, fShare,
                 dart_audio_callback, NULL))
        return 0;

    mp_msg(MSGT_AO, MSGL_V, "DART: obtained buffer size = %lu bytes\n",
           DART.ulBufferSize);

    m_fQuit = FALSE;

    ao_data.channels    = channels;
    ao_data.samplerate  = rate;
    ao_data.format      = format;
    ao_data.bps         = nBytesPerSample * rate;
    ao_data.outburst    = nBytesPerSample * OUTBURST_SAMPLES;
    ao_data.buffersize  = DART.ulBufferSize;

    // multiple of CHUNK_SIZE
    m_nBufSize = ((DART.ulBufferSize << 2) / CHUNK_SIZE) * CHUNK_SIZE;
    // and one more chunk plus round up
    m_nBufSize += 2 * CHUNK_SIZE;

    m_audioBuf = av_fifo_alloc(m_nBufSize);

    dartPlay();

    // might cause PM DLLs to be loaded which incorrectly enable SIG_FPE,
    // which AAC decoding might trigger.
    // so, mask off all floating-point exceptions.
    _control87(MCW_EM, MCW_EM);

    return 1;
}

// close audio device
static void uninit(int immed)
{
    m_fQuit = TRUE;

    if (!immed) {
        while (DART.fPlaying)
            DosSleep(1);
    }

    dartClose();

    av_fifo_free(m_audioBuf);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void)
{
    dartPause();

    // Reset ring-buffer state
    av_fifo_reset(m_audioBuf);

    dartResume();
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    dartPause();
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
    dartResume();
}

// return: how many bytes can be played without blocking
static int get_space(void)
{
    return av_fifo_space(m_audioBuf);
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void *data, int len, int flags)
{

    if (!(flags & AOPLAY_FINAL_CHUNK))
        len = (len / ao_data.outburst) * ao_data.outburst;

    return write_buffer(data, len);
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void)
{
    int nBuffered = av_fifo_size(m_audioBuf); // could be less

    return (float)nBuffered / (float)ao_data.bps;
}
