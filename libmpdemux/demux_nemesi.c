/*
 * Copyright (C) 2007 Alessandro Molina <amol.wrk@gmail.com>
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
#include <stdlib.h>
#include <stdio.h>
#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#define HAVE_STRUCT_SOCKADDR_STORAGE
#include "nemesi/rtsp.h"
#include "nemesi/rtp.h"
#include <sched.h>

int rtsp_transport_http = 0;
int rtsp_transport_tcp = 0;
int rtsp_transport_sctp = 0;
int rtsp_port = 0;

typedef struct {
    char * mime;
    unsigned int fourcc;
} MIMEto4CC;

#define NMS_MAX_FORMATS 16

MIMEto4CC supported_audio[NMS_MAX_FORMATS] = {
    {"MPA", 0x55},
    {"vorbis", mmioFOURCC('v','r','b','s')},
    {"mpeg4-generic", mmioFOURCC('M','P','4','A')},
    {NULL, 0},
};

MIMEto4CC supported_video[NMS_MAX_FORMATS] = {
    {"MPV", mmioFOURCC('M','P','E','G')},
    {"theora",mmioFOURCC('t','h','e','o')},
    {"H264", mmioFOURCC('H','2','6','4')},
    {"H263-1998", mmioFOURCC('H','2','6','3')},
    {"MP4V-ES", mmioFOURCC('M','P','4','V')},
    {NULL, 0},
};

typedef enum { NEMESI_SESSION_VIDEO,
               NEMESI_SESSION_AUDIO } Nemesi_SessionType;

typedef struct {
    rtsp_ctrl * rtsp;
    rtp_session * session[2];
    rtp_frame first_pkt[2];
    double time[2];
    double seek;
} Nemesi_DemuxerStreamData;


#define STYPE_TO_DS(demuxer, stype) \
    ((stype) == NEMESI_SESSION_VIDEO ? (demuxer)->video : (demuxer)->audio)

#define DS_TO_STYPE(demuxer, ds) \
    ((ds) == (demuxer)->video ? NEMESI_SESSION_VIDEO : NEMESI_SESSION_AUDIO)

#define INVERT_STYPE(stype) ((stype + 1) % 2)

static unsigned int get4CC(MIMEto4CC * supported_formats, char const * format)
{
    unsigned i;

    for(i = 0; i < NMS_MAX_FORMATS; ++i) {
        if (!supported_formats[i].mime)
            return 0;
        else if ( strcmp(supported_formats[i].mime, format) == 0 )
            return supported_formats[i].fourcc;
    }

    return 0;
}

static rtp_ssrc *wait_for_packets(Nemesi_DemuxerStreamData * ndsd, Nemesi_SessionType stype)
{
    rtp_ssrc *ssrc = NULL;

    /* Wait for prebuffering (prebuffering must be enabled in nemesi) */
    int terminated = rtp_fill_buffers(rtsp_get_rtp_th(ndsd->rtsp));

    /* Wait for the ssrc to be registered, if prebuffering is on in nemesi
       this will just get immediatly the correct ssrc */
    if (!terminated) {
        while ( !(ssrc = rtp_session_get_ssrc(ndsd->session[stype], ndsd->rtsp)) )
            sched_yield();
    }

    return ssrc;
}

static void link_session_and_fetch_conf(Nemesi_DemuxerStreamData * ndsd,
                                        Nemesi_SessionType stype,
                                        rtp_session * sess,
                                        rtp_buff * buff, unsigned int * fps)
{
    extern double force_fps;
    rtp_ssrc *ssrc = NULL;
    rtp_frame * fr = &ndsd->first_pkt[stype];
    rtp_buff trash_buff;
    int must_prefetch = ((fps != NULL) || (buff != NULL)) ? 1 : 0;

    ndsd->session[stype] = sess;

    ssrc = wait_for_packets(ndsd, stype);

    if ( ((ssrc) && (must_prefetch)) ) {
        if (buff == NULL)
            buff = &trash_buff;

        rtp_fill_buffer(ssrc, fr, buff); //Prefetch the first packet

        /* Packet prefecthing must be done anyway or we won't be
           able to get the metadata, but fps calculation happens
           only if the user didn't specify the FPS */
        if ( ((!force_fps) && (fps != NULL)) ) {
            while ( *fps <= 0 ) {
                //Wait more pkts to calculate FPS and try again
                sched_yield();
                *fps = rtp_get_fps(ssrc);
            }
        }
    }
}

static demuxer_t* demux_open_rtp(demuxer_t* demuxer)
{
    nms_rtsp_hints hints;
    char * url = demuxer->stream->streaming_ctrl->url->url;
    rtsp_ctrl * ctl;
    RTSP_Error reply;
    rtsp_medium * media;
    Nemesi_DemuxerStreamData * ndsd = calloc(1, sizeof(Nemesi_DemuxerStreamData));

    memset(&hints,0,sizeof(hints));
    if (rtsp_port) hints.first_rtp_port = rtsp_port;
    if (rtsp_transport_tcp) {
        hints.pref_rtsp_proto = TCP;
        hints.pref_rtp_proto = TCP;
    }
    if (rtsp_transport_sctp) {
        hints.pref_rtsp_proto = SCTP;
        hints.pref_rtp_proto = SCTP;
    }

    mp_msg(MSGT_DEMUX, MSGL_INFO, "Initializing libNemesi\n");
    if ((ctl = rtsp_init(&hints)) == NULL) {
        free(ndsd);
        return STREAM_ERROR;
    }

    ndsd->rtsp = ctl;
    demuxer->priv = ndsd;
    //nms_verbosity_set(1);

    mp_msg(MSGT_DEMUX, MSGL_INFO, "Opening: %s\n", url);
    if (rtsp_open(ctl, url)) {
        mp_msg(MSGT_DEMUX, MSGL_ERR, "rtsp_open failed.\n");
        return demuxer;
    }

    reply = rtsp_wait(ctl);
    if (reply.got_error) {
        mp_msg(MSGT_DEMUX, MSGL_ERR,
               "OPEN Error from the server: %s\n",
               reply.message.reply_str);
        return demuxer;
    }

    rtsp_play(ctl, 0, 0);
    reply = rtsp_wait(ctl);
    if (reply.got_error) {
        mp_msg(MSGT_DEMUX, MSGL_ERR,
               "PLAY Error from the server: %s\n",
               reply.message.reply_str);
        return demuxer;
    }

    if (!ctl->rtsp_queue)
        return demuxer;

    media = ctl->rtsp_queue->media_queue;
    for (; media; media=media->next) {
        sdp_medium_info * info = media->medium_info;
        rtp_session * sess = media->rtp_sess;
        rtp_buff buff;

        int media_format = atoi(info->fmts);
        rtp_pt * ptinfo = rtp_get_pt_info(sess, media_format);
        char const * format_name = ptinfo ? ptinfo->name : NULL;

        memset(&buff, 0, sizeof(rtp_buff));

        if (sess->parsers[media_format] == NULL) {
            mp_msg(MSGT_DEMUX, MSGL_ERR,
                   "libNemesi unsupported media format: %s\n",
                   format_name ? format_name : info->fmts);
            continue;
        }
        else {
            mp_msg(MSGT_DEMUX, MSGL_INFO,
                   "libNemesi supported media: %s\n",
                   format_name);
        }

        if (ptinfo->type == AU) {
            if (ndsd->session[NEMESI_SESSION_AUDIO] == NULL) {
                sh_audio_t* sh_audio = new_sh_audio(demuxer,0);
                WAVEFORMATEX* wf;
                demux_stream_t* d_audio = demuxer->audio;
                demuxer->audio->id = 0;

                mp_msg(MSGT_DEMUX, MSGL_INFO, "Detected as AUDIO stream...\n");

                link_session_and_fetch_conf(ndsd, NEMESI_SESSION_AUDIO,
                                            sess, &buff, NULL);

                if (buff.len) {
                    wf = calloc(1,sizeof(WAVEFORMATEX)+buff.len);
                    wf->cbSize = buff.len;
                    memcpy(wf+1, buff.data, buff.len);
                } else {
                    wf = calloc(1,sizeof(WAVEFORMATEX));
                }

                sh_audio->wf = wf;
                d_audio->sh = sh_audio;
                sh_audio->ds = d_audio;
                wf->nSamplesPerSec = 0;

                wf->wFormatTag =
                sh_audio->format = get4CC(supported_audio, format_name);
                if ( !(wf->wFormatTag) )
                    mp_msg(MSGT_DEMUX, MSGL_WARN,
                           "Unknown MPlayer format code for MIME"
                           " type \"audio/%s\"\n", format_name);
            } else {
                mp_msg(MSGT_DEMUX, MSGL_ERR,
                       "There is already an audio session registered,"
                       " ignoring...\n");
            }
        } else if (ptinfo->type == VI) {
            if (ndsd->session[NEMESI_SESSION_VIDEO] == NULL) {
                sh_video_t* sh_video;
                BITMAPINFOHEADER* bih;
                demux_stream_t* d_video;
                int fps = 0;

                mp_msg(MSGT_DEMUX, MSGL_INFO, "Detected as VIDEO stream...\n");

                link_session_and_fetch_conf(ndsd, NEMESI_SESSION_VIDEO,
                                            sess, &buff, &fps);

                if (buff.len) {
                    bih = calloc(1,sizeof(BITMAPINFOHEADER)+buff.len);
                    bih->biSize = sizeof(BITMAPINFOHEADER)+buff.len;
                    memcpy(bih+1, buff.data, buff.len);
                } else {
                    bih = calloc(1,sizeof(BITMAPINFOHEADER));
                    bih->biSize = sizeof(BITMAPINFOHEADER);
                }

                sh_video = new_sh_video(demuxer,0);
                sh_video->bih = bih;
                d_video = demuxer->video;
                d_video->sh = sh_video;
                sh_video->ds = d_video;

                if (fps) {
                    sh_video->fps = fps;
                    sh_video->frametime = 1.0/fps;
                }

                bih->biCompression =
                sh_video->format = get4CC(supported_video, format_name);
                if ( !(bih->biCompression) ) {
                    mp_msg(MSGT_DEMUX, MSGL_WARN,
                        "Unknown MPlayer format code for MIME"
                        " type \"video/%s\"\n", format_name);
                }
            } else {
                mp_msg(MSGT_DEMUX, MSGL_ERR,
                       "There is already a video session registered,"
                       " ignoring...\n");
            }
        } else {
            mp_msg(MSGT_DEMUX, MSGL_ERR, "Unsupported media type\n");
        }
    }

    demuxer->stream->eof = 0;

    return demuxer;
}

static int get_data_for_session(Nemesi_DemuxerStreamData * ndsd,
                                Nemesi_SessionType stype, rtp_ssrc * ssrc,
                                rtp_frame * fr)
{
    if (ndsd->first_pkt[stype].len != 0) {
        fr->data = ndsd->first_pkt[stype].data;
        fr->time_sec = ndsd->first_pkt[stype].time_sec;
        fr->len = ndsd->first_pkt[stype].len;
        ndsd->first_pkt[stype].len = 0;
        return RTP_FILL_OK;
    } else {
        rtp_buff buff;
        return rtp_fill_buffer(ssrc, fr, &buff);
    }
}

static void stream_add_packet(Nemesi_DemuxerStreamData * ndsd,
                              Nemesi_SessionType stype,
                              demux_stream_t* ds, rtp_frame * fr)
{
    demux_packet_t* dp = new_demux_packet(fr->len);
    memcpy(dp->buffer, fr->data, fr->len);

    fr->time_sec += ndsd->seek;
    ndsd->time[stype] = dp->pts = fr->time_sec;

    ds_add_packet(ds, dp);
}

static int demux_rtp_fill_buffer(demuxer_t* demuxer, demux_stream_t* ds)
{
    Nemesi_DemuxerStreamData * ndsd = demuxer->priv;
    Nemesi_SessionType stype;
    rtp_ssrc * ssrc;
    rtp_frame fr;

    if ( (!ndsd->rtsp->rtsp_queue) || (demuxer->stream->eof) ) {
        mp_msg(MSGT_DEMUX, MSGL_INFO, "End of Stream...\n");
        demuxer->stream->eof = 1;
        return 0;
    }

    memset(&fr, 0, sizeof(fr));

    stype = DS_TO_STYPE(demuxer, ds);
    if ( (ssrc = wait_for_packets(ndsd, stype)) == NULL ) {
        mp_msg(MSGT_DEMUX, MSGL_INFO, "Bye...\n");
        demuxer->stream->eof = 1;
        return 0;
    }

    if(!get_data_for_session(ndsd, stype, ssrc, &fr))
        stream_add_packet(ndsd, stype, ds, &fr);
    else {
        stype = INVERT_STYPE(stype);

        //Must check if we actually have a stream of the other type
        if (!ndsd->session[stype])
            return 1;

        ds = STYPE_TO_DS(demuxer, stype);
        ssrc = wait_for_packets(ndsd, stype);

        if(!get_data_for_session(ndsd, stype, ssrc, &fr))
            stream_add_packet(ndsd, stype, ds, &fr);
    }

    return 1;
}


static void demux_close_rtp(demuxer_t* demuxer)
{
    Nemesi_DemuxerStreamData * ndsd = demuxer->priv;
    rtsp_ctrl * ctl = ndsd->rtsp;
    RTSP_Error err;

    mp_msg(MSGT_DEMUX, MSGL_INFO, "Closing libNemesi RTSP Stream...\n");

    if (ndsd == NULL)
        return;

    free(ndsd);

    if (rtsp_close(ctl)) {
        err = rtsp_wait(ctl);
        if (err.got_error)
            mp_msg(MSGT_DEMUX, MSGL_ERR,
                   "Error Closing Stream: %s\n",
                   err.message.reply_str);
    }

    rtsp_uninit(ctl);
}

static void demux_seek_rtp(demuxer_t *demuxer, float rel_seek_secs,
                           float audio_delay, int flags)
{
    Nemesi_DemuxerStreamData * ndsd = demuxer->priv;
    rtsp_ctrl * ctl = ndsd->rtsp;
    sdp_attr * r_attr = NULL;
    sdp_range r = {0, 0};
    double time = ndsd->time[NEMESI_SESSION_VIDEO] ?
                  ndsd->time[NEMESI_SESSION_VIDEO] :
                  ndsd->time[NEMESI_SESSION_AUDIO];

    if (!ctl->rtsp_queue)
        return;

    r_attr = sdp_get_attr(ctl->rtsp_queue->info->attr_list, "range");
    if (r_attr)
        r = sdp_parse_range(r_attr->value);

    //flags & 1 -> absolute seek
    //flags & 2 -> percent seek
    if (flags == 0) {
        time += rel_seek_secs;
        if (time < r.begin)
            time = r.begin;
        else if (time > r.end)
            time = r.end;
        ndsd->seek = time;

        mp_msg(MSGT_DEMUX,MSGL_WARN,"libNemesi SEEK %f on %f - %f)\n",
           time, r.begin, r.end);

        if (!rtsp_seek(ctl, time, 0)) {
            RTSP_Error err = rtsp_wait(ctl);
            if (err.got_error) {
                mp_msg(MSGT_DEMUX, MSGL_ERR,
                       "Error Performing Seek: %s\n",
                       err.message.reply_str);
                demuxer->stream->eof = 1;
            }
            else
                mp_msg(MSGT_DEMUX, MSGL_INFO, "Seek, performed\n");
        }
        else {
            mp_msg(MSGT_DEMUX, MSGL_ERR, "Unable to pause stream to perform seek\n");
            demuxer->stream->eof = 1;
        }
    }
    else
        mp_msg(MSGT_DEMUX, MSGL_ERR, "Unsupported seek type\n");
}

static int demux_rtp_control(struct demuxer *demuxer, int cmd, void *arg)
{
    Nemesi_DemuxerStreamData * ndsd = demuxer->priv;
    rtsp_ctrl * ctl = ndsd->rtsp;
    sdp_attr * r_attr = NULL;
    sdp_range r = {0, 0};
    double time = ndsd->time[NEMESI_SESSION_VIDEO] ?
                  ndsd->time[NEMESI_SESSION_VIDEO] :
                  ndsd->time[NEMESI_SESSION_AUDIO];

    if (!ctl->rtsp_queue)
        return DEMUXER_CTRL_DONTKNOW;

    r_attr = sdp_get_attr(ctl->rtsp_queue->info->attr_list, "range");
    if (r_attr)
        r = sdp_parse_range(r_attr->value);

    switch (cmd) {
        case DEMUXER_CTRL_GET_TIME_LENGTH:
            if (r.end == 0)
                return DEMUXER_CTRL_DONTKNOW;

            *((double *)arg) = ((double)r.end) - ((double)r.begin);
            return DEMUXER_CTRL_OK;

        case DEMUXER_CTRL_GET_PERCENT_POS:
            if (r.end == 0)
                return DEMUXER_CTRL_DONTKNOW;

            *((int *)arg) = (int)( time * 100 / (r.end - r.begin) );
            return DEMUXER_CTRL_OK;
        default:
           return DEMUXER_CTRL_DONTKNOW;
    }
}

const demuxer_desc_t demuxer_desc_rtp_nemesi = {
  "libnemesi RTP demuxer",
  "nemesi",
  "",
  "Alessandro Molina",
  "requires libnemesi",
  DEMUXER_TYPE_RTP_NEMESI,
  0, // no autodetect
  NULL,
  demux_rtp_fill_buffer,
  demux_open_rtp,
  demux_close_rtp,
  demux_seek_rtp,
  demux_rtp_control
};
