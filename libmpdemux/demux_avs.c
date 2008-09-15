/*
 * Demuxer for avisynth
 * Copyright (c) 2005 Gianluigi Tiesi <sherpya@netfarm.it>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "libvo/fastmemcpy.h"

#include "loader/wine/windef.h"

#ifdef WIN32_LOADER
#include "loader/ldt_keeper.h"
#endif

#include "demux_avs.h"

#define MAX_AVS_SIZE    16 * 1024 /* 16k should be enough */

HMODULE WINAPI LoadLibraryA(LPCSTR);
FARPROC WINAPI GetProcAddress(HMODULE,LPCSTR);
int     WINAPI FreeLibrary(HMODULE);

typedef WINAPI AVS_ScriptEnvironment* (*imp_avs_create_script_environment)(int version);
typedef WINAPI AVS_Value (*imp_avs_invoke)(AVS_ScriptEnvironment *, const char * name, AVS_Value args, const char** arg_names);
typedef WINAPI const AVS_VideoInfo *(*imp_avs_get_video_info)(AVS_Clip *);
typedef WINAPI AVS_Clip* (*imp_avs_take_clip)(AVS_Value, AVS_ScriptEnvironment *);
typedef WINAPI void (*imp_avs_release_clip)(AVS_Clip *);
typedef WINAPI AVS_VideoFrame* (*imp_avs_get_frame)(AVS_Clip *, int n);
typedef WINAPI void (*imp_avs_release_video_frame)(AVS_VideoFrame *);
typedef WINAPI int (*imp_avs_get_audio)(AVS_Clip *, void * buf, uint64_t start, uint64_t count); 

#define Q(string) # string
#define IMPORT_FUNC(x) \
    AVS->x = ( imp_##x ) GetProcAddress(AVS->dll, Q(x)); \
    if (!AVS->x) { mp_msg(MSGT_DEMUX,MSGL_V,"AVS: failed to load "Q(x)"()\n"); return 0; }

typedef struct tagAVS
{
    AVS_ScriptEnvironment *avs_env;
    AVS_Value handler;
    AVS_Clip *clip;
    const AVS_VideoInfo *video_info;
#ifdef WIN32_LOADER
    ldt_fs_t* ldt_fs;
#endif
    HMODULE dll;
    int frameno;
    uint64_t sampleno;
    int init;
    
    imp_avs_create_script_environment avs_create_script_environment;
    imp_avs_invoke avs_invoke;
    imp_avs_get_video_info avs_get_video_info;
    imp_avs_take_clip avs_take_clip;
    imp_avs_release_clip avs_release_clip;
    imp_avs_get_frame avs_get_frame;
    imp_avs_release_video_frame avs_release_video_frame;
    imp_avs_get_audio avs_get_audio;
} AVS_T;

AVS_T *initAVS(const char *filename)
{   
    AVS_T *AVS = malloc (sizeof(AVS_T));
    AVS_Value arg0 = avs_new_value_string(filename);
    AVS_Value args = avs_new_value_array(&arg0, 1);
    
    memset(AVS, 0, sizeof(AVS_T));

#ifdef WIN32_LOADER
    AVS->ldt_fs = Setup_LDT_Keeper();
#endif
    
    AVS->dll = LoadLibraryA("avisynth.dll");
    if(!AVS->dll)
    {
        mp_msg(MSGT_DEMUX ,MSGL_V, "AVS: failed to load avisynth.dll\n");
        goto avs_err;
    }
    
    /* Dynamic import of needed stuff from avisynth.dll */
    IMPORT_FUNC(avs_create_script_environment);
    IMPORT_FUNC(avs_invoke);
    IMPORT_FUNC(avs_get_video_info);
    IMPORT_FUNC(avs_take_clip);
    IMPORT_FUNC(avs_release_clip);
    IMPORT_FUNC(avs_get_frame);
    IMPORT_FUNC(avs_release_video_frame);
    IMPORT_FUNC(avs_get_audio);
    
    AVS->avs_env = AVS->avs_create_script_environment(AVISYNTH_INTERFACE_VERSION);
    if (!AVS->avs_env)
    {
        mp_msg(MSGT_DEMUX, MSGL_V, "AVS: avs_create_script_environment failed\n");
        goto avs_err;
    }
    

    AVS->handler = AVS->avs_invoke(AVS->avs_env, "Import", args, 0);
    
    if (avs_is_error(AVS->handler))
    {
        mp_msg(MSGT_DEMUX, MSGL_V, "AVS: Avisynth error: %s\n", avs_as_string(AVS->handler));
        goto avs_err;
    }

    if (!avs_is_clip(AVS->handler))
    {
        mp_msg(MSGT_DEMUX, MSGL_V, "AVS: Avisynth doesn't return a clip\n");
        goto avs_err;
    }
    
    return AVS;

avs_err:
    if (AVS->dll) FreeLibrary(AVS->dll);
#ifdef WIN32_LOADER
    Restore_LDT_Keeper(AVS->ldt_fs);
#endif
    free(AVS);
    return NULL;
}

/* Implement RGB MODES ?? */
#if 0
static __inline int get_mmioFOURCC(const AVS_VideoInfo *v)
{
    if (avs_is_rgb(v)) return mmioFOURCC(8, 'R', 'G', 'B');
    if (avs_is_rgb24(v)) return mmioFOURCC(24, 'R', 'G', 'B');
    if (avs_is_rgb32(v)) return mmioFOURCC(32, 'R', 'G', 'B');
    if (avs_is_yv12(v)) return mmioFOURCC('Y', 'V', '1', '2');
    if (avs_is_yuy(v)) return mmioFOURCC('Y', 'U', 'Y', ' ');
    if (avs_is_yuy2(v)) return mmioFOURCC('Y', 'U', 'Y', '2');    
    return 0;
}
#endif

static int demux_avs_fill_buffer(demuxer_t *demuxer, demux_stream_t *ds)
{
    AVS_VideoFrame *curr_frame;
    demux_packet_t *dp = NULL;
    AVS_T *AVS = demuxer->priv;

    if (ds == demuxer->video)
    {
        sh_video_t *sh_video = demuxer->video->sh;
        char *dst;
        int w, h;
        if (AVS->video_info->num_frames <= AVS->frameno) return 0; // EOF

        curr_frame = AVS->avs_get_frame(AVS->clip, AVS->frameno);
        if (!curr_frame)
        {
            mp_msg(MSGT_DEMUX, MSGL_V, "AVS: error getting frame -- EOF??\n");
            return 0;
        }
        w = curr_frame->row_size;
        h = curr_frame->height;

        dp = new_demux_packet(w * h + 2 * (w / 2) * (h / 2));

        dp->pts=AVS->frameno / sh_video->fps;

        dst = dp->buffer;
        memcpy_pic(dst, curr_frame->vfb->data + curr_frame->offset,
                   w, h, w, curr_frame->pitch);
        dst += w * h;
        w /= 2; h /= 2;
        memcpy_pic(dst, curr_frame->vfb->data + curr_frame->offsetV,
                   w, h, w, curr_frame->pitchUV);
        dst += w * h;
        memcpy_pic(dst, curr_frame->vfb->data + curr_frame->offsetU,
                   w, h, w, curr_frame->pitchUV);
        ds_add_packet(demuxer->video, dp);

        AVS->frameno++;
        AVS->avs_release_video_frame(curr_frame);
    }
    
    /* Audio */
    if (ds == demuxer->audio)
    {
        sh_audio_t *sh_audio = ds->sh;
        int samples = sh_audio->samplerate;
        uint64_t l;
        samples = FFMIN(samples, AVS->video_info->num_audio_samples - AVS->sampleno);
        if (!samples) return 0;
        l = samples * sh_audio->channels * sh_audio->samplesize;
        if (l > INT_MAX) {
            mp_msg(MSGT_DEMUX, MSGL_FATAL, "AVS: audio packet too big\n");
            return 0;
        }
        dp = new_demux_packet(l);
        dp->pts = AVS->sampleno / sh_audio->samplerate;
        
        if (AVS->avs_get_audio(AVS->clip, dp->buffer, AVS->sampleno, samples))
        {
            mp_msg(MSGT_DEMUX, MSGL_V, "AVS: avs_get_audio() failed\n");
            return 0;
        }
        ds_add_packet(demuxer->audio, dp);

        AVS->sampleno += samples;
    }
    
    return 1;
}

static demuxer_t* demux_open_avs(demuxer_t* demuxer)
{
    int found = 0;
    AVS_T *AVS = demuxer->priv;
    int audio_samplesize = 0;
    AVS->frameno = 0;
    AVS->sampleno = 0;

    mp_msg(MSGT_DEMUX, MSGL_V, "AVS: demux_open_avs()\n");
    demuxer->seekable = 1;

    AVS->clip = AVS->avs_take_clip(AVS->handler, AVS->avs_env);
    if(!AVS->clip)
    {
        mp_msg(MSGT_DEMUX, MSGL_V, "AVS: avs_take_clip() failed\n");
        return NULL;
    }

    AVS->video_info = AVS->avs_get_video_info(AVS->clip);
    if (!AVS->video_info)
    {
        mp_msg(MSGT_DEMUX, MSGL_V, "AVS: avs_get_video_info() call failed\n");
        return NULL;
    }
    
    if (!avs_is_yv12(AVS->video_info))
    {
        AVS->handler = AVS->avs_invoke(AVS->avs_env, "ConvertToYV12", avs_new_value_array(&AVS->handler, 1), 0);
        if (avs_is_error(AVS->handler))
        {
            mp_msg(MSGT_DEMUX, MSGL_V, "AVS: Cannot convert input video to YV12: %s\n", avs_as_string(AVS->handler));
            return NULL;
        }
        
        AVS->clip = AVS->avs_take_clip(AVS->handler, AVS->avs_env);
        
        if(!AVS->clip)
        {
            mp_msg(MSGT_DEMUX, MSGL_V, "AVS: avs_take_clip() failed\n");
            return NULL;
        }

        AVS->video_info = AVS->avs_get_video_info(AVS->clip);
        if (!AVS->video_info)
        {
            mp_msg(MSGT_DEMUX, MSGL_V, "AVS: avs_get_video_info() call failed\n");
            return NULL;
        }
    }
    
    // TODO check field-based ??

    /* Video */  
    if (avs_has_video(AVS->video_info))
    {
        sh_video_t *sh_video = new_sh_video(demuxer, 0);
        found = 1;
        
        if (demuxer->video->id == -1) demuxer->video->id = 0;
        if (demuxer->video->id == 0)
        demuxer->video->sh = sh_video;
        sh_video->ds = demuxer->video;
        
        sh_video->disp_w = AVS->video_info->width;
        sh_video->disp_h = AVS->video_info->height;
        
        //sh_video->format = get_mmioFOURCC(AVS->video_info);
        sh_video->format = mmioFOURCC('Y', 'V', '1', '2');
        sh_video->fps = (double) AVS->video_info->fps_numerator / (double) AVS->video_info->fps_denominator;
        sh_video->frametime = 1.0 / sh_video->fps;
        
        sh_video->bih = malloc(sizeof(BITMAPINFOHEADER) + (256 * 4));
        sh_video->bih->biCompression = sh_video->format;
        sh_video->bih->biBitCount = avs_bits_per_pixel(AVS->video_info);
        //sh_video->bih->biPlanes = 2;
        
        sh_video->bih->biWidth = AVS->video_info->width;
        sh_video->bih->biHeight = AVS->video_info->height;
        sh_video->num_frames = 0;
        sh_video->num_frames_decoded = 0;
    }
    
    /* Audio */
    if (avs_has_audio(AVS->video_info))
      switch (AVS->video_info->sample_type) {
        case AVS_SAMPLE_INT8:  audio_samplesize = 1; break;
        case AVS_SAMPLE_INT16: audio_samplesize = 2; break;
        case AVS_SAMPLE_INT24: audio_samplesize = 3; break;
        case AVS_SAMPLE_INT32:
        case AVS_SAMPLE_FLOAT: audio_samplesize = 4; break;
        default:
          mp_msg(MSGT_DEMUX, MSGL_ERR, "AVS: unknown audio type, disabling\n");
      }
    if (audio_samplesize)
    {
        sh_audio_t *sh_audio = new_sh_audio(demuxer, 0);
        found = 1;
        mp_msg(MSGT_DEMUX, MSGL_V, "AVS: Clip has audio -> Channels = %d - Freq = %d\n", AVS->video_info->nchannels, AVS->video_info->audio_samples_per_second);

        if (demuxer->audio->id == -1) demuxer->audio->id = 0;
        if (demuxer->audio->id == 0)
        demuxer->audio->sh = sh_audio;
        sh_audio->ds = demuxer->audio;
        
        sh_audio->wf = malloc(sizeof(WAVEFORMATEX));
        sh_audio->wf->wFormatTag = sh_audio->format =
            (AVS->video_info->sample_type == AVS_SAMPLE_FLOAT) ? 0x3 : 0x1;
        sh_audio->wf->nChannels = sh_audio->channels = AVS->video_info->nchannels;
        sh_audio->wf->nSamplesPerSec = sh_audio->samplerate = AVS->video_info->audio_samples_per_second;
        sh_audio->samplesize = audio_samplesize;
        sh_audio->wf->nAvgBytesPerSec = sh_audio->channels * sh_audio->samplesize * sh_audio->samplerate;
        sh_audio->wf->nBlockAlign = sh_audio->channels * sh_audio->samplesize;
        sh_audio->wf->wBitsPerSample = sh_audio->samplesize * 8;
        sh_audio->wf->cbSize = 0;
        sh_audio->i_bps = sh_audio->wf->nAvgBytesPerSec;
    }

    AVS->init = 1;
    if (found)
        return demuxer;
    else
        return NULL;
}

static int demux_avs_control(demuxer_t *demuxer, int cmd, void *arg)
{   
    sh_video_t *sh_video=demuxer->video->sh;
    sh_audio_t *sh_audio=demuxer->audio->sh;
    AVS_T *AVS = demuxer->priv;

    switch(cmd)
    {
        case DEMUXER_CTRL_GET_TIME_LENGTH:
        {
            double res = sh_video ? (double)AVS->video_info->num_frames / sh_video->fps : 0;
            if (sh_audio)
              res = FFMAX(res, (double)AVS->video_info->num_audio_samples / sh_audio->samplerate);
            *((double *)arg) = res;
            return DEMUXER_CTRL_OK;
        }
        case DEMUXER_CTRL_GET_PERCENT_POS:
        {
            if (sh_video)
            *((int *)arg) = AVS->frameno * 100 / AVS->video_info->num_frames;
            else
              *((int *)arg) = AVS->sampleno * 100 / AVS->video_info->num_audio_samples;
            return DEMUXER_CTRL_OK;
        }
    default:
        return DEMUXER_CTRL_NOTIMPL;
    }
}

static void demux_close_avs(demuxer_t* demuxer)
{
    AVS_T *AVS = demuxer->priv;

    if (AVS)
    {
        if (AVS->dll)
        {
            if (AVS->clip)
                AVS->avs_release_clip(AVS->clip);
            mp_msg(MSGT_DEMUX, MSGL_V, "AVS: Unloading avisynth.dll\n");
            FreeLibrary(AVS->dll);
        }
#ifdef WIN32_LOADER
        Restore_LDT_Keeper(AVS->ldt_fs);
#endif
        free(AVS);
    }
}

static void demux_seek_avs(demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags)
{
    sh_video_t *sh_video=demuxer->video->sh;
    sh_audio_t *sh_audio=demuxer->audio->sh;
    AVS_T *AVS = demuxer->priv;
    double video_pos = sh_video ?
                       (double)AVS->frameno / sh_video->fps :
                       (double)AVS->sampleno / sh_audio->samplerate;
    double duration = sh_video ?
                      (double)AVS->video_info->num_frames / sh_video->fps :
                      (double)AVS->video_info->num_audio_samples / sh_audio->samplerate;
    
    //mp_msg(MSGT_DEMUX, MSGL_V, "AVS: seek rel_seek_secs = %f - flags = %x\n", rel_seek_secs, flags);
    
    if (flags&SEEK_ABSOLUTE) video_pos=0;
    if (flags&SEEK_FACTOR) rel_seek_secs *= duration;

    video_pos += rel_seek_secs;
    if (video_pos < 0) video_pos = 0;
        
    if (sh_video) {
      AVS->frameno = FFMIN(video_pos * sh_video->fps,
                           AVS->video_info->num_frames);
      sh_video->num_frames_decoded = AVS->frameno;
      sh_video->num_frames = AVS->frameno;
    }
    video_pos += audio_delay;
    if (video_pos < 0) video_pos = 0;
    if (sh_audio)
      AVS->sampleno = FFMIN(video_pos * sh_audio->samplerate,
                            AVS->video_info->num_audio_samples);
}

static int avs_check_file(demuxer_t *demuxer)
{
    mp_msg(MSGT_DEMUX, MSGL_V, "AVS: avs_check_file - attempting to open file %s\n", demuxer->filename);

    if (!demuxer->filename) return 0;
    
    /* Avoid crazy memory eating when passing an mpg stream */
    if (demuxer->movi_end > MAX_AVS_SIZE)
    {
        mp_msg(MSGT_DEMUX,MSGL_V, "AVS: File is too big, aborting...\n");
        return 0;
    }
    
    demuxer->priv = initAVS(demuxer->filename);
    
    if (demuxer->priv)
    {
        mp_msg(MSGT_DEMUX,MSGL_V, "AVS: Init Ok\n");
        return DEMUXER_TYPE_AVS;
    }
    mp_msg(MSGT_DEMUX,MSGL_V, "AVS: Init failed\n");
    return 0;
}


const demuxer_desc_t demuxer_desc_avs = {
  "Avisynth demuxer",
  "avs",
  "AVS",
  "Gianluigi Tiesi",
  "Requires binary dll",
  DEMUXER_TYPE_AVS,
  0, // unsafe autodetect
  avs_check_file,
  demux_avs_fill_buffer,
  demux_open_avs,
  demux_close_avs,
  demux_seek_avs,
  demux_avs_control
};
