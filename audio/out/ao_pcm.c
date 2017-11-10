/*
 * PCM audio output driver
 *
 * Original author: Atmosfear
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/common.h>

#include "mpv_talloc.h"

#include "options/m_option.h"
#include "audio/format.h"
#include "ao.h"
#include "internal.h"
#include "common/msg.h"
#include "osdep/endian.h"

#ifdef __MINGW32__
// for GetFileType to detect pipes
#include <windows.h>
#include <io.h>
#endif

struct priv {
    char *outputfilename;
    int waveheader;
    int append;
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
    uint16_t fmt = ao->format == AF_FORMAT_FLOAT ? WAV_ID_FLOAT_PCM : WAV_ID_PCM;
    int bits = af_fmt_to_bytes(ao->format) * 8;

    // Master RIFF chunk
    fput32le(WAV_ID_RIFF, fp);
    // RIFF chunk size: 'WAVE' + 'fmt ' + 4 + 40 +
    // data chunk hdr (8) + data length
    fput32le(12 + 40 + 8 + data_length, fp);
    fput32le(WAV_ID_WAVE, fp);

    // Format chunk
    fput32le(WAV_ID_FMT, fp);
    fput32le(40, fp);
    fput16le(WAV_ID_FORMAT_EXTENSIBLE, fp);
    fput16le(ao->channels.num, fp);
    fput32le(ao->samplerate, fp);
    fput32le(ao->bps, fp);
    fput16le(ao->channels.num * (bits / 8), fp);
    fput16le(bits, fp);

    // Extension chunk
    fput16le(22, fp);
    fput16le(bits, fp);
    fput32le(mp_chmap_to_waveext(&ao->channels), fp);
    // 2 bytes format + 14 bytes guid
    fput32le(fmt, fp);
    fput32le(0x00100000, fp);
    fput32le(0xAA000080, fp);
    fput32le(0x719B3800, fp);

    // Data chunk
    fput32le(WAV_ID_DATA, fp);
    fput32le(data_length, fp);
}

static int init(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (!priv->outputfilename)
        priv->outputfilename =
            talloc_strdup(priv, priv->waveheader ? "audiodump.wav" : "audiodump.pcm");

    ao->format = af_fmt_from_planar(ao->format);

    if (priv->waveheader) {
        // WAV files must have one of the following formats

        // And they don't work in big endian; fixing it would be simple, but
        // nobody cares.
        if (BYTE_ORDER == BIG_ENDIAN) {
            MP_FATAL(ao, "Not supported on big endian.\n");
            return -1;
        }

        switch (ao->format) {
        case AF_FORMAT_U8:
        case AF_FORMAT_S16:
        case AF_FORMAT_S32:
        case AF_FORMAT_FLOAT:
             break;
        default:
            if (!af_fmt_is_spdif(ao->format))
                ao->format = AF_FORMAT_S16;
            break;
        }
    }

    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        return -1;

    ao->bps = ao->channels.num * ao->samplerate * af_fmt_to_bytes(ao->format);

    MP_INFO(ao, "File: %s (%s)\nPCM: Samplerate: %d Hz Channels: %d Format: %s\n",
            priv->outputfilename,
            priv->waveheader ? "WAVE" : "RAW PCM", ao->samplerate,
            ao->channels.num, af_fmt_to_str(ao->format));

    priv->fp = fopen(priv->outputfilename, priv->append ? "ab" : "wb");
    if (!priv->fp) {
        MP_ERR(ao, "Failed to open %s for writing!\n", priv->outputfilename);
        return -1;
    }
    if (priv->waveheader)  // Reserve space for wave header
        write_wave_header(ao, priv->fp, 0x7ffff000);
    ao->untimed = true;

    return 0;
}

// close audio device
static void uninit(struct ao *ao)
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
            MP_ERR(ao, "Could not seek to start, WAV size headers not updated!\n");
        else {
            if (priv->data_length > 0xfffff000) {
                MP_ERR(ao, "File larger than allowed for "
                       "WAV files, may play truncated!\n");
                priv->data_length = 0xfffff000;
            }
            write_wave_header(ao, priv->fp, priv->data_length);
        }
    }
    fclose(priv->fp);
}

static int get_space(struct ao *ao)
{
    return 65536;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *priv = ao->priv;
    int len = samples * ao->sstride;

    fwrite(data[0], len, 1, priv->fp);
    priv->data_length += len;
    return samples;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_pcm = {
    .description = "RAW PCM/WAVE file writer audio output",
    .name      = "pcm",
    .init      = init,
    .uninit    = uninit,
    .get_space = get_space,
    .play      = play,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) { .waveheader = 1 },
    .options = (const struct m_option[]) {
        OPT_STRING("file", outputfilename, M_OPT_FILE),
        OPT_FLAG("waveheader", waveheader, 0),
        OPT_FLAG("append", append, 0),
        {0}
    },
    .options_prefix = "ao-pcm",
};
