/*
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "osdep/io.h"

#include "talloc.h"
#include "config.h"
#include "core/mp_msg.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "mf.h"

#define MF_MAX_FILE_SIZE (1024*1024*256)

static void free_mf(mf_t *mf)
{
    if (mf) {
        for (int n = 0; n < mf->nr_of_files; n++)
            free(mf->names[n]);
        free(mf->names);
        free(mf->streams);
        free(mf);
    }
}

static void demux_seek_mf(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags){
  mf_t * mf = (mf_t *)demuxer->priv;
  int newpos = (flags & SEEK_ABSOLUTE)?0:mf->curr_frame - 1;

  if ( flags & SEEK_FACTOR ) newpos+=rel_seek_secs*(mf->nr_of_files - 1);
   else newpos+=rel_seek_secs * mf->sh->fps;
  if ( newpos < 0 ) newpos=0;
  if( newpos >= mf->nr_of_files) newpos=mf->nr_of_files - 1;
  mf->curr_frame=newpos;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int demux_mf_fill_buffer(demuxer_t *demuxer, demux_stream_t *ds){
    mf_t *mf = demuxer->priv;
    if (mf->curr_frame >= mf->nr_of_files)
        return 0;

    struct stream *entry_stream = NULL;
    if (mf->streams)
        entry_stream = mf->streams[mf->curr_frame];
    struct stream *stream = entry_stream;
    if (!stream)
        stream = open_stream(mf->names[mf->curr_frame], demuxer->opts, NULL);

    if (stream) {
        stream_seek(stream, 0);
        bstr data = stream_read_complete(stream, NULL, MF_MAX_FILE_SIZE, 0);
        if (data.len) {
            demux_packet_t *dp = new_demux_packet(data.len);
            memcpy(dp->buffer, data.start, data.len);
            dp->pts = mf->curr_frame / mf->sh->fps;
            dp->pos = mf->curr_frame;
            dp->keyframe = true;
            ds_add_packet(demuxer->video, dp);
        }
        talloc_free(data.start);
    }

    if (stream != entry_stream)
        free_stream(stream);

    mf->curr_frame++;
    return 1;
}

// force extension/type to have a fourcc

static const struct {
  const char *type;
  uint32_t format;
} type2format[] = {
  { "bmp",  mmioFOURCC('b', 'm', 'p', ' ') },
  { "dpx",  mmioFOURCC('d', 'p', 'x', ' ') },
  { "j2c",  mmioFOURCC('M', 'J', '2', 'C') },
  { "j2k",  mmioFOURCC('M', 'J', '2', 'C') },
  { "jp2",  mmioFOURCC('M', 'J', '2', 'C') },
  { "jpc",  mmioFOURCC('M', 'J', '2', 'C') },
  { "jpeg", mmioFOURCC('I', 'J', 'P', 'G') },
  { "jpg",  mmioFOURCC('I', 'J', 'P', 'G') },
  { "jps",  mmioFOURCC('I', 'J', 'P', 'G') },
  { "jls",  mmioFOURCC('I', 'J', 'P', 'G') },
  { "thm",  mmioFOURCC('I', 'J', 'P', 'G') },
  { "db",   mmioFOURCC('I', 'J', 'P', 'G') },
  { "pcx",  mmioFOURCC('p', 'c', 'x', ' ') },
  { "png",  mmioFOURCC('M', 'P', 'N', 'G') },
  { "pns",  mmioFOURCC('M', 'P', 'N', 'G') },
  { "ptx",  mmioFOURCC('p', 't', 'x', ' ') },
  { "tga",  mmioFOURCC('M', 'T', 'G', 'A') },
  { "tif",  mmioFOURCC('t', 'i', 'f', 'f') },
  { "tiff",  mmioFOURCC('t', 'i', 'f', 'f') },
  { "sgi",  mmioFOURCC('S', 'G', 'I', '1') },
  { "sun",  mmioFOURCC('s', 'u', 'n', ' ') },
  { "ras",  mmioFOURCC('s', 'u', 'n', ' ') },
  { "ra",  mmioFOURCC('s', 'u', 'n', ' ') },
  { "im1",  mmioFOURCC('s', 'u', 'n', ' ') },
  { "im8",  mmioFOURCC('s', 'u', 'n', ' ') },
  { "im24",  mmioFOURCC('s', 'u', 'n', ' ') },
  { "sunras",  mmioFOURCC('s', 'u', 'n', ' ') },
  { NULL,   0 }
};

static uint32_t probe_format(mf_t *mf)
{
    if (mf->nr_of_files < 1)
        return 0;
    char *type = mf_type;
    if (!type || !type[0]) {
        char *p = strrchr(mf->names[0], '.');
        if (p)
            type = p + 1;
    }
    if (!type || !type[0])
        return 0;
    int i;
    for (i = 0; type2format[i].type; i++) {
        if (strcasecmp(type, type2format[i].type) == 0)
            break;
    }
    return type2format[i].format;
}

static mf_t *open_mf(demuxer_t *demuxer)
{
    if (!demuxer->stream->url)
        return NULL;

    if (strncmp(demuxer->stream->url, "mf://", 5) == 0) {
        return open_mf_pattern(demuxer->stream->url + 5);
    } else {
        mf_t *mf = open_mf_single(demuxer->stream->url);
        mf->streams = calloc(1, sizeof(struct stream *));
        mf->streams[0] = demuxer->stream;
        return mf;
    }
}

static int demux_check_file(demuxer_t *demuxer)
{
    if (demuxer->stream->type == STREAMTYPE_MF)
        return DEMUXER_TYPE_MF;
    mf_t *mf = open_mf(demuxer);
    bool ok = mf && probe_format(mf);
    free_mf(mf);
    return ok ? DEMUXER_TYPE_MF : 0;
}

static demuxer_t* demux_open_mf(demuxer_t* demuxer){
  sh_video_t   *sh_video = NULL;

  mf_t *mf = open_mf(demuxer);
  if (!mf)
    goto error;

  mf->curr_frame = 0;

  demuxer->movi_start = 0;
  demuxer->movi_end = mf->nr_of_files - 1;

  // create a new video stream header
  sh_video = new_sh_video(demuxer, 0);
  // make sure the demuxer knows about the new video stream header
  // (even though new_sh_video() ought to take care of it)
  demuxer->video->sh = sh_video;

  sh_video->format = probe_format(mf);
  if (!sh_video->format) {
    mp_msg(MSGT_DEMUX, MSGL_INFO, "[demux_mf] file type was not set! (try -mf type=ext)\n" );
    goto error;
  }

  // make sure that the video demuxer stream header knows about its
  // parent video demuxer stream (this is getting wacky), or else
  // video_read_properties() will choke
  sh_video->ds = demuxer->video;

  sh_video->disp_w = 0;
  sh_video->disp_h = 0;
  sh_video->fps = mf_fps;
  sh_video->frametime = 1 / sh_video->fps;

  mf->sh = sh_video;
  demuxer->priv=(void*)mf;

  return demuxer;

error:
  free_mf(mf);
  return NULL;
}

static void demux_close_mf(demuxer_t* demuxer) {
  mf_t *mf = demuxer->priv;

  free_mf(mf);
}

static int demux_control_mf(demuxer_t *demuxer, int cmd, void *arg) {
  mf_t *mf = (mf_t *)demuxer->priv;

  switch(cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH:
      *((double *)arg) = (double)mf->nr_of_files / mf->sh->fps;
      return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_GET_PERCENT_POS:
      if (mf->nr_of_files < 1)
        return DEMUXER_CTRL_DONTKNOW;
      *((int *)arg) = 100 * mf->curr_frame / mf->nr_of_files;
      return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_CORRECT_PTS:
      return DEMUXER_CTRL_OK;

    default:
      return DEMUXER_CTRL_NOTIMPL;
  }
}

const demuxer_desc_t demuxer_desc_mf = {
  "mf demuxer",
  "mf",
  "MF",
  "?",
  "multiframe?, pictures demuxer",
  DEMUXER_TYPE_MF,
  1,
  demux_check_file,
  demux_mf_fill_buffer,
  demux_open_mf,
  demux_close_mf,
  demux_seek_mf,
  demux_control_mf
};
