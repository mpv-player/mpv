/*
    Copyright (C) 2004 Michael Niedermayer <michaelni@gmx.at>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// #include <stdio.h>
#include <stdlib.h>
// #include <unistd.h>

#include "config.h"
#include "mp_msg.h"
// #include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#ifdef USE_LIBAVFORMAT

#include "avformat.h"
#include "avi.h"

#define PROBE_BUF_SIZE 2048

typedef struct lavf_priv_t{
    AVInputFormat *avif;
    AVFormatContext *avfc;
    ByteIOContext pb;
    int audio_streams;
    int video_streams;
    int64_t last_pts;
}lavf_priv_t;

extern void print_wave_header(WAVEFORMATEX *h);
extern void print_video_header(BITMAPINFOHEADER *h);

static int mp_open(URLContext *h, const char *filename, int flags){
    return 0;
}

static int mp_read(URLContext *h, unsigned char *buf, int size){
    stream_t *stream = (stream_t*)h->priv_data;
    int ret;

    if(stream_eof(stream)) //needed?
        return -1;
    ret=stream_read(stream, buf, size);

    mp_msg(MSGT_HEADER,MSGL_DBG2,"%d=mp_read(%p, %p, %d), eof:%d\n", ret, h, buf, size, stream->eof);
    return ret;
}

static int mp_write(URLContext *h, unsigned char *buf, int size){
    return -1;
}

static offset_t mp_seek(URLContext *h, offset_t pos, int whence){
    stream_t *stream = (stream_t*)h->priv_data;
    
    mp_msg(MSGT_HEADER,MSGL_DBG2,"mp_seek(%p, %d, %d)\n", h, (int)pos, whence);
    if(whence == SEEK_CUR)
        pos +=stream_tell(stream);
    else if(whence == SEEK_END)
        pos += stream->end_pos;
    else if(whence != SEEK_SET)
        return -1;

    if(pos<stream->end_pos && stream->eof)
        stream_reset(stream);
    if(stream_seek(stream, pos)==0)
        return -1;

    return pos;
}

static int mp_close(URLContext *h){
    return 0;
}

static URLProtocol mp_protocol = {
    "mp",
    mp_open,
    mp_read,
    mp_write,
    mp_seek,
    mp_close,
};

int lavf_check_file(demuxer_t *demuxer){
    AVProbeData avpd;
    uint8_t buf[PROBE_BUF_SIZE];
    lavf_priv_t *priv;
    
    if(!demuxer->priv) 
        demuxer->priv=calloc(sizeof(lavf_priv_t),1);
    priv= demuxer->priv;

    av_register_all();

    stream_read(demuxer->stream, buf, PROBE_BUF_SIZE);
    avpd.filename= demuxer->stream->url;
    avpd.buf= buf;
    avpd.buf_size= PROBE_BUF_SIZE;

    priv->avif= av_probe_input_format(&avpd, 1);
    if(!priv->avif){
        mp_msg(MSGT_HEADER,MSGL_V,"LAVF_check: no clue about this gibberish!\n");
        return 0;
    }else
        mp_msg(MSGT_HEADER,MSGL_V,"LAVF_check: %s\n", priv->avif->long_name);

    return 1;
}
    
int demux_open_lavf(demuxer_t *demuxer){
    AVFormatContext *avfc;
    AVFormatParameters ap;
    lavf_priv_t *priv= demuxer->priv;
    int i;
    char mp_filename[256]="mp:";

    memset(&ap, 0, sizeof(AVFormatParameters));

    stream_seek(demuxer->stream, 0);

    register_protocol(&mp_protocol);

    strncpy(mp_filename + 3, demuxer->stream->url, sizeof(mp_filename)-3);
    
    url_fopen(&priv->pb, mp_filename, URL_RDONLY);
    
    ((URLContext*)(priv->pb.opaque))->priv_data= demuxer->stream;
        
    if(av_open_input_stream(&avfc, &priv->pb, mp_filename, priv->avif, &ap)<0){
        mp_msg(MSGT_HEADER,MSGL_ERR,"LAVF_header: av_open_input_stream() failed\n");
        return 0;
    }

    priv->avfc= avfc;

    if(av_find_stream_info(avfc) < 0){
        mp_msg(MSGT_HEADER,MSGL_ERR,"LAVF_header: av_find_stream_info() failed\n");
        return 0;
    }

    if(avfc->title    [0]) demux_info_add(demuxer, "name"     , avfc->title    );
    if(avfc->author   [0]) demux_info_add(demuxer, "author"   , avfc->author   );
    if(avfc->copyright[0]) demux_info_add(demuxer, "copyright", avfc->copyright);
    if(avfc->comment  [0]) demux_info_add(demuxer, "comments" , avfc->comment  );
    if(avfc->album    [0]) demux_info_add(demuxer, "album"    , avfc->album    );
//    if(avfc->year        ) demux_info_add(demuxer, "year"     , avfc->year     );
//    if(avfc->track       ) demux_info_add(demuxer, "track"    , avfc->track    );
    if(avfc->genre    [0]) demux_info_add(demuxer, "genre"    , avfc->genre    );

    for(i=0; i<avfc->nb_streams; i++){
        AVStream *st= avfc->streams[i];
        AVCodecContext *codec= &st->codec;
        
        switch(codec->codec_type){
        case CODEC_TYPE_AUDIO:{
            WAVEFORMATEX *wf= calloc(sizeof(WAVEFORMATEX) + codec->extradata_size, 1);
            sh_audio_t* sh_audio=new_sh_audio(demuxer, i);
            priv->audio_streams++;
            if(!codec->codec_tag)
                codec->codec_tag= codec_get_wav_tag(codec->codec_id);
            wf->wFormatTag= codec->codec_tag;
            wf->nChannels= codec->channels;
            wf->nSamplesPerSec= codec->sample_rate;
            wf->nAvgBytesPerSec= codec->bit_rate/8;
            wf->nBlockAlign= codec->block_align;
            wf->wBitsPerSample= codec->bits_per_sample;
            wf->cbSize= codec->extradata_size;
            if(codec->extradata_size){
                memcpy(
                    wf + 1, 
                    codec->extradata,
                    codec->extradata_size);
            }
            sh_audio->wf= wf;
            sh_audio->ds= demuxer->audio;
            sh_audio->format= codec->codec_tag;
            sh_audio->channels= codec->channels;
            sh_audio->samplerate= codec->sample_rate;
            if(verbose>=1) print_wave_header(sh_audio->wf);
            demuxer->audio->id=i;
            demuxer->audio->sh= demuxer->a_streams[i];
            break;}
        case CODEC_TYPE_VIDEO:{
            BITMAPINFOHEADER *bih=calloc(sizeof(BITMAPINFOHEADER) + codec->extradata_size,1);
            sh_video_t* sh_video=new_sh_video(demuxer, i);

	    priv->video_streams++;
            if(!codec->codec_tag)
                codec->codec_tag= codec_get_bmp_tag(codec->codec_id);
            bih->biSize= sizeof(BITMAPINFOHEADER) + codec->extradata_size;
            bih->biWidth= codec->width;
            bih->biHeight= codec->height;
            bih->biBitCount= codec->bits_per_sample;
            bih->biSizeImage = bih->biWidth * bih->biHeight * bih->biBitCount/8;
            bih->biCompression= codec->codec_tag;
            sh_video->bih= bih;
            sh_video->disp_w= codec->width;
            sh_video->disp_h= codec->height;
            sh_video->video.dwRate= codec->frame_rate;
            sh_video->video.dwScale= codec->frame_rate_base;
            sh_video->fps=(float)sh_video->video.dwRate/(float)sh_video->video.dwScale;
            sh_video->frametime=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
            sh_video->format = bih->biCompression;
            sh_video->aspect=   codec->width * codec->sample_aspect_ratio.num 
                              / (float)(codec->height * codec->sample_aspect_ratio.den);
            mp_msg(MSGT_DEMUX,MSGL_DBG2,"aspect= %d*%d/(%d*%d)\n", 
                codec->width, codec->sample_aspect_ratio.num,
                codec->height, codec->sample_aspect_ratio.den);

            sh_video->ds= demuxer->video;
            if(codec->extradata_size)
                memcpy(sh_video->bih + 1, codec->extradata, codec->extradata_size);
            if(verbose>=1) print_video_header(sh_video->bih);
/*    short 	biPlanes;
    int  	biXPelsPerMeter;
    int  	biYPelsPerMeter;
    int 	biClrUsed;
    int 	biClrImportant;*/
            demuxer->video->id=i;
            demuxer->video->sh= demuxer->v_streams[i];            
            break;}
        }
    }
    
    mp_msg(MSGT_HEADER,MSGL_V,"LAVF: %d audio and %d video streams found\n",priv->audio_streams,priv->video_streams);
    if(!priv->audio_streams) demuxer->audio->id=-2;  // nosound
//    else if(best_audio > 0 && demuxer->audio->id == -1) demuxer->audio->id=best_audio;
    if(!priv->video_streams){
        if(!priv->audio_streams){
	    mp_msg(MSGT_HEADER,MSGL_ERR,"LAVF: no audio or video headers found - broken file?\n");
            return 0; 
        }
        demuxer->video->id=-2; // audio-only
    } //else if (best_video > 0 && demuxer->video->id == -1) demuxer->video->id = best_video;

    return 1;
}

int demux_lavf_fill_buffer(demuxer_t *demux){
    lavf_priv_t *priv= demux->priv;
    AVPacket pkt;
    demux_packet_t *dp;
    demux_stream_t *ds;
    int id;
    mp_msg(MSGT_DEMUX,MSGL_DBG2,"demux_lavf_fill_buffer()\n");

    demux->filepos=stream_tell(demux->stream);

    if(stream_eof(demux->stream)){
//        demuxre->stream->eof=1;
        return 0;
    }

    if(av_read_frame(priv->avfc, &pkt) < 0)
        return 0;
        
    id= pkt.stream_index;

    if(id==demux->audio->id){
        // audio
        ds=demux->audio;
        if(!ds->sh){
            ds->sh=demux->a_streams[id];
            mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected LAVF audio ID = %d\n",ds->id);
        }
    } else if(id==demux->video->id){
        // video
        ds=demux->video;
        if(!ds->sh){
            ds->sh=demux->v_streams[id];
            mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected LAVF video ID = %d\n",ds->id);
        }
    } else
        ds= NULL;
        
    if(0/*pkt.destruct == av_destruct_packet*/){
        //ok kids, dont try this at home :)
        dp=(demux_packet_t*)malloc(sizeof(demux_packet_t));
        dp->len=pkt.size;
        dp->next=NULL;
        dp->refcount=1;
        dp->master=NULL;
        dp->buffer=pkt.data;
        pkt.destruct= NULL;
    }else{
        dp=new_demux_packet(pkt.size);
        memcpy(dp->buffer, pkt.data, pkt.size);
        av_free_packet(&pkt);
    }

    priv->last_pts= pkt.pts;
    
    dp->pts=pkt.pts / (float)AV_TIME_BASE;
    dp->pos=demux->filepos;
    dp->flags= !!(pkt.flags&PKT_FLAG_KEY);
    // append packet to DS stream:
    ds_add_packet(ds,dp);
    return 1;
}

void demux_seek_lavf(demuxer_t *demuxer, float rel_seek_secs, int flags){
    lavf_priv_t *priv = demuxer->priv;
    mp_msg(MSGT_DEMUX,MSGL_DBG2,"demux_seek_lavf(%p, %f, %d)\n", demuxer, rel_seek_secs, flags);
    
    av_seek_frame(priv->avfc, -1, priv->last_pts + rel_seek_secs*AV_TIME_BASE);
}

int demux_lavf_control(demuxer_t *demuxer, int cmd, void *arg)
{
    lavf_priv_t *priv = demuxer->priv;
    
    switch (cmd) {
        case DEMUXER_CTRL_GET_TIME_LENGTH:
	    if (priv->avfc->duration == 0)
	        return DEMUXER_CTRL_DONTKNOW;
	    
	    *((unsigned long *)arg) = priv->avfc->duration / AV_TIME_BASE;
	    return DEMUXER_CTRL_OK;

	case DEMUXER_CTRL_GET_PERCENT_POS:
	    if (priv->avfc->duration == 0)
	        return DEMUXER_CTRL_DONTKNOW;
	    
	    *((int *)arg) = (int)(priv->last_pts*100 / priv->avfc->duration);
	    return DEMUXER_CTRL_OK;
	
	default:
	    return DEMUXER_CTRL_NOTIMPL;
    }
}

void demux_close_lavf(demuxer_t *demuxer)
{
    lavf_priv_t* priv = demuxer->priv;
    if (priv){
        if(priv->avfc)
       {
         av_close_input_file(priv->avfc); priv->avfc= NULL;
        }
        free(priv); demuxer->priv= NULL;
    }
}

#endif // USE_LIBAVFORMAT
