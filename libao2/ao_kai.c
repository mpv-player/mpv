/*
 * OS/2 KAI audio output driver
 *
 * Copyright (c) 2010 by KO Myung-Hun (komh@chollian.net)
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

#include <kai.h>

#include "config.h"
#include "libaf/af_format.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "mp_msg.h"
#include "libvo/fastmemcpy.h"
#include "subopt-helper.h"
#include "libavutil/avutil.h"
#include "libavutil/fifo.h"

static const ao_info_t info = {
    "KAI audio output",
    "kai",
    "KO Myung-Hun <komh@chollian.net>",
    ""
};

LIBAO_EXTERN(kai)

#define OUTBURST_SAMPLES        512
#define DEFAULT_SAMPLES         (OUTBURST_SAMPLES << 2)

#define CHUNK_SIZE  ao_data.outburst

static AVFifoBuffer *m_audioBuf;

static int m_nBufSize = 0;

static volatile int m_fQuit = FALSE;

static KAISPEC m_kaiSpec;

static HKAI m_hkai;

static int write_buffer(unsigned char *data, int len)
{
    int nFree = av_fifo_space(m_audioBuf);

    len = FFMIN(len, nFree);

    return av_fifo_generic_write(m_audioBuf, data, len, NULL);
}

static int read_buffer(unsigned char *data, int len)
{
    int nBuffered = av_fifo_size(m_audioBuf);

    len = FFMIN(len, nBuffered);

    av_fifo_generic_read(m_audioBuf, data, len, NULL);
    return len;
}

// end ring buffer stuff

static ULONG APIENTRY kai_audio_callback(PVOID pCBData, PVOID pBuffer,
                                         ULONG ulSize)
{
    int nReadLen;

    nReadLen = read_buffer(pBuffer, ulSize);
    if (nReadLen < ulSize && !m_fQuit) {
        memset((uint8_t *)pBuffer + nReadLen, m_kaiSpec.bSilence, ulSize - nReadLen);
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

        vol->left = vol->right = kaiGetVolume(m_hkai, MCI_STATUS_AUDIO_ALL);

        return CONTROL_OK;
        }

    case AOCONTROL_SET_VOLUME:
        {
        int mid;
        ao_control_vol_t *vol = arg;

        mid = (vol->left + vol->right) / 2;
        kaiSetVolume(m_hkai, MCI_SET_AUDIO_ALL, mid);

        return CONTROL_OK;
        }
    }

    return CONTROL_UNKNOWN;
}

static void print_help(void)
{
    mp_msg(MSGT_AO, MSGL_FATAL,
           "\n-ao kai commandline help:\n"
           "Example: mplayer -ao kai:noshare\n"
           "    open audio in exclusive mode\n"
           "\nOptions:\n"
           "    uniaud\n"
           "        Use UNIAUD audio driver\n"
           "    dart\n"
           "        Use DART audio driver\n"
           "    (no)share\n"
           "        Open audio in shareable or exclusive mode\n"
           "    bufsize=<size>\n"
           "        Set buffer size to <size> in samples(default: 2048)\n");
}

// open & set up audio device
// return: 1=success 0=fail
static int init(int rate, int channels, int format, int flags)
{
    int     fUseUniaud = 0;
    int     fUseDart = 0;
    int     fShare = 1;
    ULONG   kaiMode;
    KAICAPS kc;
    int     nSamples = DEFAULT_SAMPLES;
    int     nBytesPerSample;
    KAISPEC ksWanted;

    const opt_t subopts[] = {
        {"uniaud",  OPT_ARG_BOOL, &fUseUniaud, NULL},
        {"dart",    OPT_ARG_BOOL, &fUseDart,   NULL},
        {"share",   OPT_ARG_BOOL, &fShare,     NULL},
        {"bufsize", OPT_ARG_INT,  &nSamples,   int_non_neg},
        {NULL}
    };

    const char *audioDriver[] = {"DART", "UNIAUD",};

    if (subopt_parse(ao_subdevice, subopts) != 0) {
        print_help();
        return 0;
    }

    if (fUseUniaud && fUseDart)
        mp_msg(MSGT_VO, MSGL_WARN,"KAI: Multiple mode specified!!!\n");

    if (fUseUniaud)
        kaiMode = KAIM_UNIAUD;
    else if (fUseDart)
        kaiMode = KAIM_DART;
    else
        kaiMode = KAIM_AUTO;

    if (kaiInit(kaiMode)) {
        mp_msg(MSGT_VO, MSGL_ERR, "KAI: Init failed!!!\n");
        return 0;
    }

    kaiCaps(&kc);
    mp_msg(MSGT_AO, MSGL_V, "KAI: selected audio driver = %s\n",
           audioDriver[kc.ulMode - 1]);
    mp_msg(MSGT_AO, MSGL_V, "KAI: PDD name = %s, maximum channels = %lu\n",
           kc.szPDDName, kc.ulMaxChannels);

    if (!nSamples)
        nSamples = DEFAULT_SAMPLES;

    mp_msg(MSGT_AO, MSGL_V, "KAI: open in %s mode, buffer size = %d sample(s)\n",
           fShare ? "shareable" : "exclusive", nSamples);

    switch (format) {
    case AF_FORMAT_S16_LE:
    case AF_FORMAT_S8:
        break;

    default:
        format = AF_FORMAT_S16_LE;
        mp_msg(MSGT_AO, MSGL_V, "KAI: format %s not supported defaulting to Signed 16-bit Little-Endian\n",
               af_fmt2str_short(format));
        break;
    }

    nBytesPerSample = (af_fmt2bits(format) >> 3) * channels;

    ksWanted.usDeviceIndex      = 0;
    ksWanted.ulType             = KAIT_PLAY;
    ksWanted.ulBitsPerSample    = af_fmt2bits(format);
    ksWanted.ulSamplingRate     = rate;
    ksWanted.ulDataFormat       = MCI_WAVE_FORMAT_PCM;
    ksWanted.ulChannels         = channels;
    ksWanted.ulNumBuffers       = 2;
    ksWanted.ulBufferSize       = nBytesPerSample * nSamples;
    ksWanted.fShareable         = fShare;
    ksWanted.pfnCallBack        = kai_audio_callback;
    ksWanted.pCallBackData      = NULL;

    if (kaiOpen(&ksWanted, &m_kaiSpec, &m_hkai)) {
        mp_msg(MSGT_VO, MSGL_ERR, "KAI: Open failed!!!\n");
        return 0;
    }

    mp_msg(MSGT_AO, MSGL_V, "KAI: obtained buffer count = %lu, size = %lu bytes\n",
           m_kaiSpec.ulNumBuffers, m_kaiSpec.ulBufferSize);

    m_fQuit = FALSE;

    ao_data.channels    = channels;
    ao_data.samplerate  = rate;
    ao_data.format      = format;
    ao_data.bps         = nBytesPerSample * rate;
    ao_data.outburst    = nBytesPerSample * OUTBURST_SAMPLES;
    ao_data.buffersize  = m_kaiSpec.ulBufferSize;

    m_nBufSize = (m_kaiSpec.ulBufferSize * m_kaiSpec.ulNumBuffers) << 2;

    // multiple of CHUNK_SIZE
    m_nBufSize = (m_nBufSize / CHUNK_SIZE) * CHUNK_SIZE;

    // and one more chunk plus round up
    m_nBufSize += 2 * CHUNK_SIZE;

    mp_msg(MSGT_AO, MSGL_V, "KAI: internal audio buffer size = %d bytes\n",
           m_nBufSize);

    m_audioBuf = av_fifo_alloc(m_nBufSize);

    kaiPlay(m_hkai);

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

    if (!immed)
        while (kaiStatus(m_hkai) & KAIS_PLAYING)
            DosSleep(1);

    kaiClose(m_hkai);

    kaiDone();

    av_fifo_free(m_audioBuf);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void)
{
    kaiPause(m_hkai);

    // Reset ring-buffer state
    av_fifo_reset(m_audioBuf);

    kaiResume(m_hkai);
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    kaiPause(m_hkai);
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
    kaiResume(m_hkai);
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
