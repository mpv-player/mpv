/*
 * PCM audio output driver
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/common.h>

#include "talloc.h"

#include "core/subopt-helper.h"
#include "audio/format.h"
#include "audio/reorder_ch.h"
#include "ao.h"
#include "core/mp_msg.h"

#ifdef __MINGW32__
// for GetFileType to detect pipes
#include <windows.h>
#include <io.h>
#endif

struct priv {
    char *outputfilename;
    int waveheader;
    uint64_t data_length;
    FILE *fp;
};

#define WAV_ID_RIFF 0x46464952 /* "RIFF" */
#define WAV_ID_WAVE 0x45564157 /* "WAVE" */
#define WAV_ID_FMT  0x20746d66 /* "fmt " */
#define WAV_ID_DATA 0x61746164 /* "data" */
#define WAV_ID_PCM  0x0001
#define WAV_ID_FLOAT_PCM  0x0003
#define WAV_ID_FORMAT_EXTENSIBLE 0xfffe

static void fput16le(uint16_t val, FILE *fp)
{
    uint8_t bytes[2] = {val, val >> 8};
    fwrite(bytes, 1, 2, fp);
}

static void fput32le(uint32_t val, FILE *fp)
{
    uint8_t bytes[4] = {val, val >> 8, val >> 16, val >> 24};
    fwrite(bytes, 1, 4, fp);
}

static void write_wave_header(struct ao *ao, FILE *fp, uint64_t data_length)
{
    bool use_waveex = true;
    uint16_t fmt = ao->format == AF_FORMAT_FLOAT_LE ?
        WAV_ID_FLOAT_PCM : WAV_ID_PCM;
    uint32_t fmt_chunk_size = use_waveex ? 40 : 16;
    int bits = af_fmt2bits(ao->format);

    // Master RIFF chunk
    fput32le(WAV_ID_RIFF, fp);
    // RIFF chunk size: 'WAVE' + 'fmt ' + 4 + fmt_chunk_size +
    // data chunk hdr (8) + data length
    fput32le(12 + fmt_chunk_size + 8 + data_length, fp);
    fput32le(WAV_ID_WAVE, fp);

    // Format chunk
    fput32le(WAV_ID_FMT, fp);
    fput32le(fmt_chunk_size, fp);
    fput16le(use_waveex ? WAV_ID_FORMAT_EXTENSIBLE : fmt, fp);
    fput16le(ao->channels.num, fp);
    fput32le(ao->samplerate, fp);
    fput32le(ao->bps, fp);
    fput16le(ao->channels.num * (bits / 8), fp);
    fput16le(bits, fp);

    if (use_waveex) {
        // Extension chunk
        fput16le(22, fp);
        fput16le(bits, fp);
        fput32le(mp_chmap_to_waveext(&ao->channels), fp);
        // 2 bytes format + 14 bytes guid
        fput32le(fmt, fp);
        fput32le(0x00100000, fp);
        fput32le(0xAA000080, fp);
        fput32le(0x719B3800, fp);
    }

    // Data chunk
    fput32le(WAV_ID_DATA, fp);
    fput32le(data_length, fp);
}

static int init(struct ao *ao, char *params)
{
    struct priv *priv = talloc_zero(ao, struct priv);
    ao->priv = priv;

    const opt_t subopts[] = {
        {"waveheader", OPT_ARG_BOOL,  &priv->waveheader, NULL},
        {"file",       OPT_ARG_MSTRZ, &priv->outputfilename, NULL},
        {NULL}
    };
    // set defaults
    priv->waveheader = 1;

    if (subopt_parse(params, subopts) != 0)
        return -1;

    if (!priv->outputfilename)
        priv->outputfilename =
            strdup(priv->waveheader ? "audiodump.wav" : "audiodump.pcm");
    if (priv->waveheader) {
        // WAV files must have one of the following formats

        switch (ao->format) {
        case AF_FORMAT_U8:
        case AF_FORMAT_S16_LE:
        case AF_FORMAT_S24_LE:
        case AF_FORMAT_S32_LE:
        case AF_FORMAT_FLOAT_LE:
        case AF_FORMAT_AC3_BE:
        case AF_FORMAT_AC3_LE:
             break;
        default:
            ao->format = AF_FORMAT_S16_LE;
            break;
        }
    }

    mp_chmap_reorder_to_waveext(&ao->channels);

    ao->outburst = 65536;
    ao->bps = ao->channels.num * ao->samplerate * (af_fmt2bits(ao->format) / 8);

    mp_tmsg(MSGT_AO, MSGL_INFO, "[AO PCM] File: %s (%s)\n"
            "PCM: Samplerate: %d Hz   Channels: %d   Format: %s\n",
            priv->outputfilename,
            priv->waveheader ? "WAVE" : "RAW PCM", ao->samplerate,
            ao->channels.num, af_fmt2str_short(ao->format));
    mp_tmsg(MSGT_AO, MSGL_INFO,
            "[AO PCM] Info: Faster dumping is achieved with -no-video\n"
            "[AO PCM] Info: To write WAVE files use -ao pcm:waveheader (default).\n");

    priv->fp = fopen(priv->outputfilename, "wb");
    if (!priv->fp) {
        mp_tmsg(MSGT_AO, MSGL_ERR, "[AO PCM] Failed to open %s for writing!\n",
                priv->outputfilename);
        return -1;
    }
    if (priv->waveheader)  // Reserve space for wave header
        write_wave_header(ao, priv->fp, 0x7ffff000);
    ao->untimed = true;

    return 0;
}

// close audio device
static void uninit(struct ao *ao, bool cut_audio)
{
    struct priv *priv = ao->priv;

    if (priv->waveheader) {    // Rewrite wave header
        bool broken_seek = false;
#ifdef __MINGW32__
        // Windows, in its usual idiocy "emulates" seeks on pipes so it always
        // looks like they work. So we have to detect them brute-force.
        broken_seek = FILE_TYPE_DISK !=
            GetFileType((HANDLE)_get_osfhandle(_fileno(priv->fp)));
#endif
        if (broken_seek || fseek(priv->fp, 0, SEEK_SET) != 0)
            mp_msg(MSGT_AO, MSGL_ERR, "Could not seek to start, "
                   "WAV size headers not updated!\n");
        else {
            if (priv->data_length > 0xfffff000) {
                mp_msg(MSGT_AO, MSGL_ERR, "File larger than allowed for "
                       "WAV files, may play truncated!\n");
                priv->data_length = 0xfffff000;
            }
            write_wave_header(ao, priv->fp, priv->data_length);
        }
    }
    fclose(priv->fp);
    free(priv->outputfilename);
}

static int get_space(struct ao *ao)
{
    return ao->outburst;
}

static int play(struct ao *ao, void *data, int len, int flags)
{
    struct priv *priv = ao->priv;

    fwrite(data, len, 1, priv->fp);
    priv->data_length += len;
    return len;
}

const struct ao_driver audio_out_pcm = {
    .info = &(const struct ao_info) {
        "RAW PCM/WAVE file writer audio output",
        "pcm",
        "Atmosfear",
        "",
    },
    .init      = init,
    .uninit    = uninit,
    .get_space = get_space,
    .play      = play,
};
