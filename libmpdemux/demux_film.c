/*
 * FILM file parser
 * Copyright (C) 2002 Mike Melanson
 *
 * This demuxer handles FILM (a.k.a. CPK) files commonly found on Sega
 * Saturn CD-ROM games. FILM files have also been found on 3DO games.
 *
 * details of the FILM file format can be found at:
 * http://www.pcisys.net/~melanson/codecs/
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

// chunk types found in a FILM file
#define CHUNK_FILM mmioFOURCC('F', 'I', 'L', 'M')
#define CHUNK_FDSC mmioFOURCC('F', 'D', 'S', 'C')
#define CHUNK_STAB mmioFOURCC('S', 'T', 'A', 'B')

typedef struct film_chunk_t
{
  off_t chunk_offset;
  int chunk_size;
  unsigned int syncinfo1;
  unsigned int syncinfo2;

  float pts;
} film_chunk_t;

typedef struct film_data_t
{
  unsigned int total_chunks;
  unsigned int current_chunk;
  film_chunk_t *chunks;
  unsigned int chunks_per_second;
  unsigned int film_version;
} film_data_t;

static void demux_seek_film(demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags)
{
  film_data_t *film_data = (film_data_t *)demuxer->priv;
  int new_current_chunk=(flags&SEEK_ABSOLUTE)?0:film_data->current_chunk;

  if(flags&SEEK_FACTOR)
      new_current_chunk += rel_seek_secs * film_data->total_chunks; // 0..1
  else
      new_current_chunk += rel_seek_secs * film_data->chunks_per_second; // secs


mp_msg(MSGT_DECVIDEO, MSGL_INFO,"current, total chunks = %d, %d; seek %5.3f sec, new chunk guess = %d\n",
  film_data->current_chunk, film_data->total_chunks,
  rel_seek_secs, new_current_chunk);

  // check if the new chunk number is valid
  if (new_current_chunk < 0)
    new_current_chunk = 0;
  if ((unsigned int)new_current_chunk > film_data->total_chunks)
    new_current_chunk = film_data->total_chunks - 1;

  while (((film_data->chunks[new_current_chunk].syncinfo1 == 0xFFFFFFFF) ||
    (film_data->chunks[new_current_chunk].syncinfo1 & 0x80000000)) &&
    (new_current_chunk > 0))
    new_current_chunk--;

  film_data->current_chunk = new_current_chunk;

mp_msg(MSGT_DECVIDEO, MSGL_INFO,"  (flags = %X)  actual new chunk = %d (syncinfo1 = %08X)\n",
  flags, film_data->current_chunk, film_data->chunks[film_data->current_chunk].syncinfo1);
  demuxer->video->pts=film_data->chunks[film_data->current_chunk].pts;

}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int demux_film_fill_buffer(demuxer_t *demuxer, demux_stream_t *ds)
{
  int i;
  unsigned char byte_swap;
  int cvid_size;
  sh_video_t *sh_video = demuxer->video->sh;
  sh_audio_t *sh_audio = demuxer->audio->sh;
  film_data_t *film_data = (film_data_t *)demuxer->priv;
  film_chunk_t film_chunk;
  int length_fix_bytes;
  demux_packet_t* dp;

  // see if the end has been reached
  if (film_data->current_chunk >= film_data->total_chunks)
    return 0;

  film_chunk = film_data->chunks[film_data->current_chunk];

  // position stream and fetch chunk
  stream_seek(demuxer->stream, film_chunk.chunk_offset);

  // load the chunks manually (instead of using ds_read_packet()), since
  // they require some adjustment
  // (all ones in syncinfo1 indicates an audio chunk)
  if (film_chunk.syncinfo1 == 0xFFFFFFFF)
  {
   if(demuxer->audio->id>=-1){   // audio not disabled
    dp = new_demux_packet(film_chunk.chunk_size);
    if (stream_read(demuxer->stream, dp->buffer, film_chunk.chunk_size) !=
      film_chunk.chunk_size)
      return 0;
    dp->pts = film_chunk.pts;
    dp->pos = film_chunk.chunk_offset;
    dp->flags = 0;

    // adjust the data before queuing it:
    //   8-bit: signed -> unsigned
    //  16-bit: big-endian -> little-endian
    if (sh_audio->wf->wBitsPerSample == 8)
      for (i = 0; i < film_chunk.chunk_size; i++)
        dp->buffer[i] += 128;
    else
      for (i = 0; i < film_chunk.chunk_size; i += 2)
      {
        byte_swap = dp->buffer[i];
        dp->buffer[i] = dp->buffer[i + 1];
        dp->buffer[i + 1] = byte_swap;
      }

    /* for SegaSaturn .cpk file, translate audio data if stereo */
    if (sh_audio->wf->nChannels == 2) {
      if (sh_audio->wf->wBitsPerSample == 8) {
        unsigned char* tmp = dp->buffer;
        unsigned char  buf[film_chunk.chunk_size];
        for(i = 0; i < film_chunk.chunk_size/2; i++) {
          buf[i*2] = tmp[i];
          buf[i*2+1] = tmp[film_chunk.chunk_size/2+i];
        }
        memcpy( tmp, buf, film_chunk.chunk_size );
      }
      else {/* for 16bit */
        unsigned short* tmp = dp->buffer;
        unsigned short  buf[film_chunk.chunk_size/2];
        for(i = 0; i < film_chunk.chunk_size/4; i++) {
          buf[i*2] = tmp[i];
          buf[i*2+1] = tmp[film_chunk.chunk_size/4+i];
        }
        memcpy( tmp, buf, film_chunk.chunk_size );
      }
    }

    // append packet to DS stream
    ds_add_packet(demuxer->audio, dp);
   }
  }
  else
  {
    // if the demuxer is dealing with CVID data, deal with it a special way
    if (sh_video->format == mmioFOURCC('c', 'v', 'i', 'd'))
    {
      if (film_data->film_version)
        length_fix_bytes = 2;
      else
        length_fix_bytes = 6;

      // account for the fix bytes when allocating the buffer
      dp = new_demux_packet(film_chunk.chunk_size - length_fix_bytes);

      // these CVID data chunks have a few extra bytes; skip them
      if (stream_read(demuxer->stream, dp->buffer, 10) != 10)
        return 0;
      stream_skip(demuxer->stream, length_fix_bytes);

      if (stream_read(demuxer->stream, dp->buffer + 10,
        film_chunk.chunk_size - (10 + length_fix_bytes)) !=
        (film_chunk.chunk_size - (10 + length_fix_bytes)))
        return 0;

      dp->pts = film_chunk.pts;
      dp->pos = film_chunk.chunk_offset;
      dp->flags = (film_chunk.syncinfo1 & 0x80000000) ? 1 : 0;

      // fix the CVID chunk size
      cvid_size = film_chunk.chunk_size - length_fix_bytes;
      dp->buffer[1] = (cvid_size >> 16) & 0xFF;
      dp->buffer[2] = (cvid_size >>  8) & 0xFF;
      dp->buffer[3] = (cvid_size >>  0) & 0xFF;

      // append packet to DS stream
      ds_add_packet(demuxer->video, dp);
    }
    else
    {
      ds_read_packet(demuxer->video, demuxer->stream, film_chunk.chunk_size,
        film_chunk.pts,
        film_chunk.chunk_offset, (film_chunk.syncinfo1 & 0x80000000) ? 1 : 0);
    }
  }
  film_data->current_chunk++;

  return 1;
}

static demuxer_t* demux_open_film(demuxer_t* demuxer)
{
  sh_video_t *sh_video = NULL;
  sh_audio_t *sh_audio = NULL;
  film_data_t *film_data;
  film_chunk_t film_chunk;
  int header_size;
  unsigned int chunk_type;
  unsigned int chunk_size;
  unsigned int i;
  unsigned int video_format;
  int audio_channels;
  int counting_chunks;
  unsigned int total_audio_bytes = 0;

  film_data = malloc(sizeof(film_data_t));
  film_data->total_chunks = 0;
  film_data->current_chunk = 0;
  film_data->chunks = NULL;
  film_data->chunks_per_second = 0;

  // go back to the beginning
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, 0);

  // read the master chunk type
  chunk_type = stream_read_fourcc(demuxer->stream);
  // validate the chunk type
  if (chunk_type != CHUNK_FILM)
  {
    mp_msg(MSGT_DEMUX, MSGL_ERR, "Not a FILM file\n");
    free(film_data);
    return NULL;
  }

  // get the header size, which implicitly points past the header and
  // to the start of the data
  header_size = stream_read_dword(demuxer->stream);
  film_data->film_version = stream_read_fourcc(demuxer->stream);
  demuxer->movi_start = header_size;
  demuxer->movi_end = demuxer->stream->end_pos;
  header_size -= 16;

  mp_msg(MSGT_DEMUX, MSGL_HINT, "FILM version %.4s\n",
    (char *)&film_data->film_version);

  // skip to where the next chunk should be
  stream_skip(demuxer->stream, 4);

  // traverse through the header
  while (header_size > 0)
  {
    // fetch the chunk type and size
    chunk_type = stream_read_fourcc(demuxer->stream);
    chunk_size = stream_read_dword(demuxer->stream);
    header_size -= chunk_size;

    switch (chunk_type)
    {
    case CHUNK_FDSC:
      mp_msg(MSGT_DECVIDEO, MSGL_V, "parsing FDSC chunk\n");

      // fetch the video codec fourcc to see if there's any video
      video_format = stream_read_fourcc(demuxer->stream);
      if (video_format)
      {
        // create and initialize the video stream header
        sh_video = new_sh_video(demuxer, 0);
        demuxer->video->sh = sh_video;
        sh_video->ds = demuxer->video;

        sh_video->format = video_format;
        sh_video->disp_h = stream_read_dword(demuxer->stream);
        sh_video->disp_w = stream_read_dword(demuxer->stream);
        mp_msg(MSGT_DECVIDEO, MSGL_V,
          "  FILM video: %d x %d\n", sh_video->disp_w,
          sh_video->disp_h);
      }
      else
        // skip height and width if no video
        stream_skip(demuxer->stream, 8);

      if(demuxer->audio->id<-1){
          mp_msg(MSGT_DECVIDEO, MSGL_INFO,"chunk size = 0x%X \n",chunk_size);
	stream_skip(demuxer->stream, chunk_size-12-8);
	break; // audio disabled (or no soundcard)
      }

      // skip over unknown byte, but only if file had non-NULL version
      if (film_data->film_version)
        stream_skip(demuxer->stream, 1);

      // fetch the audio channels to see if there's any audio
      // don't do this if the file is a quirky file with NULL version
      if (film_data->film_version)
      {
        audio_channels = stream_read_char(demuxer->stream);
        if (audio_channels > 0)
        {
          // create and initialize the audio stream header
          sh_audio = new_sh_audio(demuxer, 0);
          demuxer->audio->id = 0;
          demuxer->audio->sh = sh_audio;
          sh_audio->ds = demuxer->audio;

          sh_audio->wf = malloc(sizeof(WAVEFORMATEX));

          // uncompressed PCM format
          sh_audio->wf->wFormatTag = 1;
          sh_audio->format = 1;
          sh_audio->wf->nChannels = audio_channels;
          sh_audio->wf->wBitsPerSample = stream_read_char(demuxer->stream);
          stream_skip(demuxer->stream, 1);  // skip unknown byte
          sh_audio->wf->nSamplesPerSec = stream_read_word(demuxer->stream);
          sh_audio->wf->nAvgBytesPerSec =
            sh_audio->wf->nSamplesPerSec * sh_audio->wf->wBitsPerSample
            * sh_audio->wf->nChannels / 8;
          stream_skip(demuxer->stream, 6);  // skip the rest of the unknown

          mp_msg(MSGT_DECVIDEO, MSGL_V,
            "  FILM audio: %d channels, %d bits, %d Hz\n",
            sh_audio->wf->nChannels, 8 * sh_audio->wf->wBitsPerSample,
            sh_audio->wf->nSamplesPerSec);
        }
        else
          stream_skip(demuxer->stream, 10);
      }
      else
      {
        // otherwise, make some assumptions about the audio

        // create and initialize the audio stream header
        sh_audio = new_sh_audio(demuxer, 0);
        demuxer->audio->sh = sh_audio;
        sh_audio->ds = demuxer->audio;

        sh_audio->wf = malloc(sizeof(WAVEFORMATEX));

        // uncompressed PCM format
        sh_audio->wf->wFormatTag = 1;
        sh_audio->format = 1;
        sh_audio->wf->nChannels = 1;
        sh_audio->wf->wBitsPerSample = 8;
        sh_audio->wf->nSamplesPerSec = 22050;
        sh_audio->wf->nAvgBytesPerSec =
          sh_audio->wf->nSamplesPerSec * sh_audio->wf->wBitsPerSample
          * sh_audio->wf->nChannels / 8;

        mp_msg(MSGT_DECVIDEO, MSGL_V,
          "  FILM audio: %d channels, %d bits, %d Hz\n",
          sh_audio->wf->nChannels, sh_audio->wf->wBitsPerSample,
          sh_audio->wf->nSamplesPerSec);
      }
      break;

    case CHUNK_STAB:
      mp_msg(MSGT_DECVIDEO, MSGL_V, "parsing STAB chunk\n");

      if (sh_video)
      {
        sh_video->fps = stream_read_dword(demuxer->stream);
        sh_video->frametime = 1.0 / sh_video->fps;
      }

      // fetch the number of chunks
      film_data->total_chunks = stream_read_dword(demuxer->stream);
      film_data->current_chunk = 0;
      mp_msg(MSGT_DECVIDEO, MSGL_V,
        "  STAB chunk contains %d chunks\n", film_data->total_chunks);

      // allocate enough entries for the chunk
      film_data->chunks =
        calloc(film_data->total_chunks, sizeof(film_chunk_t));

      // build the chunk index
      counting_chunks = 1;
      for (i = 0; i < film_data->total_chunks; i++)
      {
        film_chunk = film_data->chunks[i];
        film_chunk.chunk_offset =
          demuxer->movi_start + stream_read_dword(demuxer->stream);
        film_chunk.chunk_size = stream_read_dword(demuxer->stream);
        film_chunk.syncinfo1 = stream_read_dword(demuxer->stream);
        film_chunk.syncinfo2 = stream_read_dword(demuxer->stream);

        // count chunks for the purposes of seeking
        if (counting_chunks)
        {
          // if we're counting chunks, always count an audio chunk
          if (film_chunk.syncinfo1 == 0xFFFFFFFF)
            film_data->chunks_per_second++;
          // if it's a video chunk, check if it's time to stop counting
          else if ((film_chunk.syncinfo1 & 0x7FFFFFFF) >= sh_video->fps)
            counting_chunks = 0;
          else
            film_data->chunks_per_second++;
        }

        // precalculate PTS
        if (film_chunk.syncinfo1 == 0xFFFFFFFF)
        {
	  if(demuxer->audio->id>=-1)
          film_chunk.pts =
            (float)total_audio_bytes / (float)sh_audio->wf->nAvgBytesPerSec;
          total_audio_bytes += film_chunk.chunk_size;
        }
        else
          film_chunk.pts =
            (film_chunk.syncinfo1 & 0x7FFFFFFF) / sh_video->fps;

        film_data->chunks[i] = film_chunk;
      }

      // in some FILM files (notably '1.09'), the length of the FDSC chunk
      // follows different rules
      if (chunk_size == (film_data->total_chunks * 16))
        header_size -= 16;
      break;

    default:
      mp_msg(MSGT_DEMUX, MSGL_ERR, "Unrecognized FILM header chunk: %08X\n",
        chunk_type);
      return NULL;
      break;
    }
  }

  demuxer->priv = film_data;

  return demuxer;
}

static void demux_close_film(demuxer_t* demuxer) {
  film_data_t *film_data = demuxer->priv;

  if(!film_data)
    return;
  if(film_data->chunks)
    free(film_data->chunks);
  free(film_data);

}

static int film_check_file(demuxer_t* demuxer)
{
  int signature=stream_read_fourcc(demuxer->stream);

  // check for the FILM file magic number
  if(signature==mmioFOURCC('F', 'I', 'L', 'M'))
    return DEMUXER_TYPE_FILM;

  return 0;
}


const demuxer_desc_t demuxer_desc_film = {
  "FILM/CPK demuxer for Sega Saturn CD-ROM games",
  "film",
  "FILM",
  "Mike Melanson",
  "",
  DEMUXER_TYPE_FILM,
  0, // unsafe autodetect (short signature)
  film_check_file,
  demux_film_fill_buffer,
  demux_open_film,
  demux_close_film,
  demux_seek_film,
  NULL
};
