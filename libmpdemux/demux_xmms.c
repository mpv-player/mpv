/*
 * Copyright (C) 2002-2004 Balatoni Denes and A'rpi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

// This is not reentrant because of global static variables, but most of
// the plugins are not reentrant either perhaps
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <dirent.h>
#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>

#include "m_option.h"
#include "libaf/af_format.h"
#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "mp_msg.h"
#include "help_mp.h"

#define XMMS_PACKETSIZE 65536  // some plugins won't play if this is too small

#include "demux_xmms_plugin.h"

typedef struct {
    uint64_t spos;   // stream position in number of output bytes from 00:00:00
    InputPlugin* ip;
}  xmms_priv_t;

static pthread_mutex_t xmms_mutex;
static int format = 0x1; // Raw PCM
static char xmms_audiobuffer[XMMS_PACKETSIZE];
static uint32_t xmms_channels;
static uint32_t xmms_samplerate;
static uint32_t xmms_afmt;
static int xmms_length;
static char *xmms_title=NULL;
static uint32_t xmms_audiopos=0;
static int xmms_playing=0;
static xmms_priv_t *xmms_priv=NULL;
static uint32_t xmms_byterate;
static int64_t xmms_flushto=-1;

// =========== mplayer xmms outputplugin stuff  ==============

static void disk_close(void) {}
static void disk_pause(short p) {}
static void disk_init(void) {}

static void disk_flush(int time) {
    if (xmms_priv) xmms_flushto=time*((long long) xmms_byterate)/1000LL;
}

static int disk_free(void) { // vqf plugin sends more than it should
    return (XMMS_PACKETSIZE-xmms_audiopos<XMMS_PACKETSIZE/4 ?
                            0:XMMS_PACKETSIZE-xmms_audiopos-XMMS_PACKETSIZE/4);
}

static int disk_playing(void) {
    return 0; //?? maybe plugins wait on exit until oplugin is not playing?
}

static int disk_get_output_time(void) {
    if (xmms_byterate)
        return xmms_priv->spos*1000LL/((long long)xmms_byterate);
    else return 0;
}

static int disk_open(AFormat fmt, int rate, int nch) {
    switch (fmt) {
        case FMT_U8:
            xmms_afmt=AF_FORMAT_U8;
            break;
        case FMT_S8:
            xmms_afmt=AF_FORMAT_S8;
            break;
        case FMT_U16_LE:
            xmms_afmt=AF_FORMAT_U16_LE;
            break;
        case FMT_U16_NE:
#if WORDS_BIGENDIAN
            xmms_afmt=AF_FORMAT_U16_BE;
#else
            xmms_afmt=AF_FORMAT_U16_LE;
#endif
            break;
        case FMT_U16_BE:
            xmms_afmt=AF_FORMAT_U16_BE;
            break;
        case FMT_S16_NE:
            xmms_afmt=AF_FORMAT_S16_NE;
            break;
        case FMT_S16_LE:
            xmms_afmt=AF_FORMAT_S16_LE;
            break;
        case FMT_S16_BE:
            xmms_afmt=AF_FORMAT_S16_BE;
            break;
    }
    xmms_samplerate=rate;
    xmms_channels=nch;
    return 1;
}

static void disk_write(void *ptr, int length) {
    if (!xmms_playing) return;
    pthread_mutex_lock(&xmms_mutex);
    if (xmms_flushto!=-1) {
        xmms_priv->spos=xmms_flushto;
        xmms_flushto=-1;
        xmms_audiopos=0;
    }
    xmms_priv->spos+= length;
    memcpy(&xmms_audiobuffer[xmms_audiopos],ptr,length);
    xmms_audiopos+=length;
    pthread_mutex_unlock(&xmms_mutex);
}

static OutputPlugin xmms_output_plugin =
{
    NULL,
    NULL,
    "MPlayer output interface plugin ", /* Description */
    disk_init,
    NULL,                   /* about */
    NULL,                   /* configure */
    NULL,                   /* get_volume */
    NULL,                   /* set_volume */
    disk_open,
    disk_write,
    disk_close,
    disk_flush,
    disk_pause,
    disk_free,
    disk_playing,
    disk_get_output_time,
    disk_get_output_time //we pretend that everything written is played at once
};

// ==================== mplayer xmms inputplugin helper stuff =================

static InputPlugin* input_plugins[100];
static int no_plugins=0;

/* Dummy functions  */
static InputVisType input_get_vis_type(){return 0;}
static void input_add_vis_pcm(int time, AFormat fmt, int nch, int length,
                                                                void *ptr){}
static void input_set_info_text(char * text){}
char *xmms_get_gentitle_format(){ return ""; }
/* Dummy functions  END*/

static void input_set_info(char* title,int length, int rate, int freq, int nch)
{
    xmms_length=length;
}

static void init_plugins_from_dir(const char *plugin_dir){
    DIR *dir;
    struct dirent *ent;

    dir = opendir(plugin_dir);
    if (!dir) return;

    while ((ent = readdir(dir)) != NULL){
        char filename[strlen(plugin_dir)+strlen(ent->d_name)+4];
        void* handle;
        sprintf(filename, "%s/%s", plugin_dir, ent->d_name);
        handle=dlopen(filename, RTLD_NOW);
        if(handle){
            void *(*gpi) (void);
            gpi=dlsym(handle, "get_iplugin_info");
            if(gpi){
                InputPlugin *p=gpi();
                mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_MPDEMUX_XMMS_FoundPlugin,
                                                ent->d_name,p->description);
                p->handle = handle;
                p->filename = strdup(filename);
                p->get_vis_type = input_get_vis_type;
                p->add_vis_pcm = input_add_vis_pcm;
                p->set_info = input_set_info;
                p->set_info_text = input_set_info_text;
                if(p->init) p->init();
                input_plugins[no_plugins++]=p;
            } else
                dlclose(handle);
        }
    }
    closedir(dir);
}

static void init_plugins(){
    char *home;

    no_plugins=0;

    home = getenv("HOME");
    if(home != NULL) {
        char xmms_home[strlen(home) + 15];
        sprintf(xmms_home, "%s/.xmms/Plugins", home);
        init_plugins_from_dir(xmms_home);
    }

    init_plugins_from_dir(XMMS_INPUT_PLUGIN_DIR);
}

static void cleanup_plugins(){
    while(no_plugins>0){
        --no_plugins;
        mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_MPDEMUX_XMMS_ClosingPlugin,
                                        input_plugins[no_plugins]->filename);
        if(input_plugins[no_plugins]->cleanup)
            input_plugins[no_plugins]->cleanup();
        dlclose(input_plugins[no_plugins]->handle);
    }
}

// ============================ mplayer demuxer stuff ===============

static int demux_xmms_open(demuxer_t* demuxer) {
    InputPlugin* ip = NULL;
    sh_audio_t* sh_audio;
    WAVEFORMATEX* w;
    xmms_priv_t *priv;
    int i;

    if (xmms_priv) return 0; // as I said, it's not reentrant :)
    init_plugins();
    for(i=0;i<no_plugins;i++){
        if (input_plugins[i]->is_our_file(demuxer->stream->url)){
            ip=input_plugins[i]; break;
        }
    }
    if(!ip) return 0; // no plugin to handle this...

    pthread_mutex_init(&xmms_mutex,NULL);

    xmms_priv=priv=malloc(sizeof(xmms_priv_t));
    memset(priv,0,sizeof(xmms_priv_t));
    priv->ip=ip;

    memset(xmms_audiobuffer,0,XMMS_PACKETSIZE);

    xmms_channels=0;
    sh_audio = new_sh_audio(demuxer,0);
    sh_audio->wf = w = malloc(sizeof(WAVEFORMATEX));
    w->wFormatTag = sh_audio->format = format;

    demuxer->movi_start = 0;
    demuxer->movi_end = 100;
    demuxer->audio->id = 0;
    demuxer->audio->sh = sh_audio;
    demuxer->priv=priv;
    sh_audio->ds = demuxer->audio;

    xmms_output_plugin.init();
    ip->output = &xmms_output_plugin;
    xmms_playing=1;
    ip->play_file(demuxer->stream->url);
    if (ip->get_song_info)
        ip->get_song_info(demuxer->stream->url,&xmms_title,&xmms_length);
    if (xmms_length<=0) demuxer->seekable=0;

    mp_msg(MSGT_DEMUX,MSGL_INFO,MSGTR_MPDEMUX_XMMS_WaitForStart,
                                                        demuxer->stream->url);
    while (xmms_channels==0) {
        usleep(10000);
        if(ip->get_time()<0) return 0;
    }
    sh_audio->sample_format= xmms_afmt;
    switch (xmms_afmt) {
        case AF_FORMAT_S16_LE:
        case AF_FORMAT_S16_BE:
        case AF_FORMAT_U16_LE:
        case AF_FORMAT_U16_BE:
            sh_audio->samplesize = 2;
            break;
        default:
            sh_audio->samplesize = 1;
    }
    w->wBitsPerSample = sh_audio->samplesize*8;
    w->nChannels = sh_audio->channels = xmms_channels;
    w->nSamplesPerSec = sh_audio->samplerate = xmms_samplerate;
    xmms_byterate = w->nAvgBytesPerSec =
                    xmms_samplerate*sh_audio->channels*sh_audio->samplesize;
    w->nBlockAlign = sh_audio->samplesize*sh_audio->channels;
    w->cbSize = 0;

    return DEMUXER_TYPE_XMMS;
}

static int demux_xmms_fill_buffer(demuxer_t* demuxer, demux_stream_t *ds) {
    sh_audio_t *sh_audio = demuxer->audio->sh;
    xmms_priv_t *priv=demuxer->priv;
    demux_packet_t*  dp;

    if (xmms_length<=0) demuxer->seekable=0;
    else demuxer->seekable=1;

    while (xmms_audiopos<XMMS_PACKETSIZE/2) {
        if((priv->ip->get_time()<0) || !xmms_playing)
            return 0;
        usleep(1000);
    }

    pthread_mutex_lock(&xmms_mutex);
    dp = new_demux_packet(XMMS_PACKETSIZE/2);
    dp->pts = priv->spos / sh_audio->wf->nAvgBytesPerSec;
    ds->pos = priv->spos;

    memcpy(dp->buffer,xmms_audiobuffer,XMMS_PACKETSIZE/2);
    memcpy(xmms_audiobuffer,&xmms_audiobuffer[XMMS_PACKETSIZE/2],
                                            xmms_audiopos-XMMS_PACKETSIZE/2);
    xmms_audiopos-=XMMS_PACKETSIZE/2;
    pthread_mutex_unlock(&xmms_mutex);

    ds_add_packet(ds,dp);

    return 1;
}

static void demux_xmms_seek(demuxer_t *demuxer,float rel_seek_secs,
                                               float audio_delay,int flags){
    stream_t* s = demuxer->stream;
    sh_audio_t* sh_audio = demuxer->audio->sh;
    xmms_priv_t *priv=demuxer->priv;
    int32_t pos;

    if(priv->ip->get_time()<0) return;

    pos = (flags & 1) ? 0 : priv->spos / sh_audio->wf->nAvgBytesPerSec;
    if (flags & 2)
        pos+= rel_seek_secs*xmms_length;
    else
        pos+= rel_seek_secs;

    if (pos<0) pos=0;
    if (pos>=xmms_length) pos=xmms_length-1;

    priv->ip->seek((pos<0)?0:pos);
    priv->spos=pos * sh_audio->wf->nAvgBytesPerSec;
}

static void demux_close_xmms(demuxer_t* demuxer) {
    xmms_priv_t *priv=demuxer->priv;
    xmms_playing=0;
    xmms_audiopos=0; // xmp on exit waits until buffer is free enough
    if (priv != NULL) {
        if (priv->ip != NULL)
            priv->ip->stop();
        free(priv); xmms_priv=demuxer->priv=NULL;
    }
    cleanup_plugins();
}

static int demux_xmms_control(demuxer_t *demuxer,int cmd, void *arg){
    demux_stream_t *d_video=demuxer->video;
    sh_audio_t *sh_audio=demuxer->audio->sh;
    xmms_priv_t *priv=demuxer->priv;

    switch(cmd) {
        case DEMUXER_CTRL_GET_TIME_LENGTH:
            if (xmms_length<=0) return DEMUXER_CTRL_DONTKNOW;
            *((double *)arg)=(double)xmms_length/1000;
            return DEMUXER_CTRL_GUESS;

        case DEMUXER_CTRL_GET_PERCENT_POS:
            if (xmms_length<=0)
                return DEMUXER_CTRL_DONTKNOW;
            *((int *)arg)=(int)( priv->spos /
                        (float)(sh_audio->wf->nAvgBytesPerSec) / xmms_length );
            return DEMUXER_CTRL_OK;

        default:
            return DEMUXER_CTRL_NOTIMPL;
    }
}


demuxer_desc_t demuxer_desc_xmms = {
    "XMMS demuxer",
    "xmms",
    "XMMS",
    "Balatoni Denes, A'rpi",
    "requires XMMS plugins",
    DEMUXER_TYPE_XMMS,
    0, // safe autodetect
    demux_xmms_open,
    demux_xmms_fill_buffer,
    NULL,
    demux_close_xmms,
    demux_xmms_seek,
    demux_xmms_control
};
