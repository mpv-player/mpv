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

#include "libavutil/common.h"
#include "mpbswap.h"
#include "subopt-helper.h"
#include "libaf/af_format.h"
#include "libaf/reorder_ch.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef __MINGW32__
// for GetFileType to detect pipes
#include <windows.h>
#endif

static const ao_info_t info =
{
    "RAW PCM/WAVE file writer audio output",
    "pcm",
    "Atmosfear",
    ""
};

LIBAO_EXTERN(pcm)

extern int vo_pts;

static char *ao_outputfilename = NULL;
static int ao_pcm_waveheader = 1;
static int fast = 0;

#define WAV_ID_RIFF 0x46464952 /* "RIFF" */
#define WAV_ID_WAVE 0x45564157 /* "WAVE" */
#define WAV_ID_FMT  0x20746d66 /* "fmt " */
#define WAV_ID_DATA 0x61746164 /* "data" */
#define WAV_ID_PCM  0x0001
#define WAV_ID_FLOAT_PCM  0x0003
#define WAV_ID_FORMAT_EXTENSIBLE 0xfffe

/* init with default values */
static uint64_t data_length;
static FILE *fp = NULL;


static void fput16le(uint16_t val, FILE *fp) {
    uint8_t bytes[2] = {val, val >> 8};
    fwrite(bytes, 1, 2, fp);
}

static void fput32le(uint32_t val, FILE *fp) {
    uint8_t bytes[4] = {val, val >> 8, val >> 16, val >> 24};
    fwrite(bytes, 1, 4, fp);
}

static void write_wave_header(FILE *fp, uint64_t data_length) {
    int use_waveex = (ao_data.channels >= 5 && ao_data.channels <= 8);
    uint16_t fmt = (ao_data.format == AF_FORMAT_FLOAT_LE) ? WAV_ID_FLOAT_PCM : WAV_ID_PCM;
    uint32_t fmt_chunk_size = use_waveex ? 40 : 16;
    int bits = af_fmt2bits(ao_data.format);

    // Master RIFF chunk
    fput32le(WAV_ID_RIFF, fp);
    // RIFF chunk size: 'WAVE' + 'fmt ' + 4 + fmt_chunk_size + data chunk hdr (8) + data length
    fput32le(12 + fmt_chunk_size + 8 + data_length, fp);
    fput32le(WAV_ID_WAVE, fp);

    // Format chunk
    fput32le(WAV_ID_FMT, fp);
    fput32le(fmt_chunk_size, fp);
    fput16le(use_waveex ? WAV_ID_FORMAT_EXTENSIBLE : fmt, fp);
    fput16le(ao_data.channels, fp);
    fput32le(ao_data.samplerate, fp);
    fput32le(ao_data.bps, fp);
    fput16le(ao_data.channels * (bits / 8), fp);
    fput16le(bits, fp);

    if (use_waveex) {
        // Extension chunk
        fput16le(22, fp);
        fput16le(bits, fp);
        switch (ao_data.channels) {
            case 5:
                fput32le(0x0607, fp); // L R C Lb Rb
                break;
            case 6:
                fput32le(0x060f, fp); // L R C Lb Rb LFE
                break;
            case 7:
                fput32le(0x0727, fp); // L R C Cb Ls Rs LFE
                break;
            case 8:
                fput32le(0x063f, fp); // L R C Lb Rb Ls Rs LFE
                break;
        }
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

// to set/get/query special features/parameters
static int control(int cmd,void *arg){
    return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){
    const opt_t subopts[] = {
        {"waveheader", OPT_ARG_BOOL, &ao_pcm_waveheader, NULL},
        {"file",       OPT_ARG_MSTRZ, &ao_outputfilename, NULL},
        {"fast",       OPT_ARG_BOOL, &fast, NULL},
        {NULL}
    };
    // set defaults
    ao_pcm_waveheader = 1;

    if (subopt_parse(ao_subdevice, subopts) != 0) {
        return 0;
    }
    if (!ao_outputfilename){
        ao_outputfilename =
            strdup(ao_pcm_waveheader?"audiodump.wav":"audiodump.pcm");
    }

    if (ao_pcm_waveheader)
    {
        // WAV files must have one of the following formats

        switch(format){
        case AF_FORMAT_U8:
        case AF_FORMAT_S16_LE:
        case AF_FORMAT_S24_LE:
        case AF_FORMAT_S32_LE:
        case AF_FORMAT_FLOAT_LE:
        case AF_FORMAT_AC3_BE:
        case AF_FORMAT_AC3_LE:
             break;
        default:
            format = AF_FORMAT_S16_LE;
            break;
        }
    }

    ao_data.outburst = 65536;
    ao_data.buffersize= 2*65536;
    ao_data.channels=channels;
    ao_data.samplerate=rate;
    ao_data.format=format;
    ao_data.bps=channels*rate*(af_fmt2bits(format)/8);

    mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_PCM_FileInfo, ao_outputfilename,
           (ao_pcm_waveheader?"WAVE":"RAW PCM"), rate,
           (channels > 1) ? "Stereo" : "Mono", af_fmt2str_short(format));
    mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_PCM_HintInfo);

    fp = fopen(ao_outputfilename, "wb");
    if(fp) {
        if(ao_pcm_waveheader){ /* Reserve space for wave header */
            write_wave_header(fp, 0x7ffff000);
        }
        return 1;
    }
    mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_PCM_CantOpenOutputFile,
               ao_outputfilename);
    return 0;
}

// close audio device
static void uninit(int immed){

    if(ao_pcm_waveheader){ /* Rewrite wave header */
        int broken_seek = 0;
#ifdef __MINGW32__
        // Windows, in its usual idiocy "emulates" seeks on pipes so it always looks
        // like they work. So we have to detect them brute-force.
        broken_seek = GetFileType((HANDLE)_get_osfhandle(_fileno(fp))) != FILE_TYPE_DISK;
#endif
        if (broken_seek || fseek(fp, 0, SEEK_SET) != 0)
            mp_msg(MSGT_AO, MSGL_ERR, "Could not seek to start, WAV size headers not updated!\n");
        else {
            if (data_length > 0xfffff000) {
                mp_msg(MSGT_AO, MSGL_ERR, "File larger than allowed for WAV files, may play truncated!\n");
                data_length = 0xfffff000;
            }
            write_wave_header(fp, data_length);
        }
    }
    fclose(fp);
    if (ao_outputfilename)
        free(ao_outputfilename);
    ao_outputfilename = NULL;
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void){

}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    // for now, just call reset();
    reset();
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
}

// return: how many bytes can be played without blocking
static int get_space(void){

    if(vo_pts)
        return ao_data.pts < vo_pts + fast * 30000 ? ao_data.outburst : 0;
    return ao_data.outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){

// let libaf to do the conversion...
#if 0
//#if HAVE_BIGENDIAN
    if (ao_data.format == AFMT_S16_LE) {
      unsigned short *buffer = (unsigned short *) data;
      register int i;
      for(i = 0; i < len/2; ++i) {
        buffer[i] = le2me_16(buffer[i]);
      }
    }
#endif

    if (ao_data.channels == 5 || ao_data.channels == 6 || ao_data.channels == 8) {
        int frame_size = af_fmt2bits(ao_data.format) / 8;
        len -= len % (frame_size * ao_data.channels);
        reorder_channel_nch(data, AF_CHANNEL_LAYOUT_MPLAYER_DEFAULT,
                            AF_CHANNEL_LAYOUT_WAVEEX_DEFAULT,
                            ao_data.channels,
                            len / frame_size, frame_size);
    }

    //printf("PCM: Writing chunk!\n");
    fwrite(data,len,1,fp);

    if(ao_pcm_waveheader)
        data_length += len;

    return len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void){

    return 0.0;
}
