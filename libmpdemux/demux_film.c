/*
	FILM file parser for the MPlayer program
	by Mike Melanson

        Details of the FILM file format can be found at:
          http://www.pcisys.net/~melanson/codecs/
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

// chunk types found in a FILM file
#define CHUNK_FILM mmioFOURCC('F', 'I', 'L', 'M')
#define CHUNK_FDSC mmioFOURCC('F', 'D', 'S', 'C')
#define CHUNK_STAB mmioFOURCC('S', 'T', 'A', 'B')

typedef struct _film_chunk_t
{
  off_t chunk_offset;
  int chunk_size;

  unsigned int syncinfo1;
  unsigned int syncinfo2;

  unsigned int video_chunk_number;  // in the case of a video chunk
  unsigned int running_audio_sample_count;  // for an audio chunk
} film_chunk_t;

typedef struct _film_data_t
{
  int total_chunks;
  int current_chunk;
  int total_video_chunks;
  int total_audio_sample_count;
  film_chunk_t *chunks;
  unsigned int ticks;
  unsigned int film_version;
} film_data_t;

#if 0
void demux_seek_film(demuxer_t *demuxer,float rel_seek_secs,int flags)
{
  film_frames_t *frames = (film_frames_t *)demuxer->priv;
  sh_video_t *sh_video = demuxer->video->sh;
  int newpos=(flags&1)?0:frames->current_frame;
  if(flags&2){
      // float 0..1
      newpos+=rel_seek_secs*frames->num_frames;
  } else {
      // secs
      newpos+=rel_seek_secs*sh_video->fps;
  }
  if(newpos<0) newpos=0; else
  if(newpos>frames->num_frames) newpos=frames->num_frames;
  frames->current_frame=newpos;
}
#endif

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_film_fill_buffer(demuxer_t *demuxer)
{
  int i;
  unsigned char byte_swap;
  int cvid_size;
  sh_video_t *sh_video = demuxer->video->sh;
  sh_audio_t *sh_audio = demuxer->audio->sh;
  film_data_t *film_data = (film_data_t *)demuxer->priv;
  film_chunk_t film_chunk;

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
    demux_packet_t* dp=new_demux_packet(film_chunk.chunk_size);
    stream_read(demuxer->stream, dp->buffer, film_chunk.chunk_size);
    dp->pts = 0;
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

    // append packet to DS stream
    ds_add_packet(demuxer->audio, dp);
    film_data->current_chunk++;
  }
  else
  {
    // if the demuxer is dealing with CVID data, deal with it a special way
    if (sh_video->format == mmioFOURCC('c', 'v', 'i', 'd'))
    {
      // account for 2 extra bytes
      demux_packet_t* dp=new_demux_packet(film_chunk.chunk_size - 2);

      // these CVID data chunks appear to have 2 extra bytes; skip them
      stream_read(demuxer->stream, dp->buffer, 10);
      stream_skip(demuxer->stream, 2);
      stream_read(demuxer->stream, dp->buffer + 10, film_chunk.chunk_size - 12);
      dp->pts = (film_chunk.syncinfo1 & 0x7FFFFFFF) / sh_video->fps;
      dp->pos = film_chunk.chunk_offset;
      dp->flags = (film_chunk.syncinfo1 & 0x80000000) ? 1 : 0;

      // fix the CVID chunk size by adding 6
      cvid_size = (dp->buffer[1] << 16) | (dp->buffer[2] << 8) | dp->buffer[3];
      cvid_size += 6;
      dp->buffer[1] = (cvid_size >> 16) & 0xFF;
      dp->buffer[2] = (cvid_size >>  8) & 0xFF;
      dp->buffer[3] = (cvid_size >>  0) & 0xFF;

      // append packet to DS stream
      ds_add_packet(demuxer->video, dp);
      film_data->current_chunk++;
    }
    else
    {
      ds_read_packet(demuxer->video, demuxer->stream, film_chunk.chunk_size,
        (film_chunk.syncinfo1 & 0x7FFFFFFF) / sh_video->fps,
        film_chunk.chunk_offset, (film_chunk.syncinfo1 & 0x80000000) ? 1 : 0);
      film_data->current_chunk++;
    }
  }

  return 1;
}

demuxer_t* demux_open_film(demuxer_t* demuxer)
{
  sh_video_t *sh_video = NULL;
  sh_audio_t *sh_audio = NULL;
  film_data_t *film_data;
  film_chunk_t film_chunk;
  int header_size;
  unsigned int chunk_type;
  unsigned int chunk_size;
  int i;
  unsigned int video_format;

  int largest_audio_chunk = 0;
  int audio_channels;

  film_data = (film_data_t *)malloc(sizeof(film_data_t));
  film_data->total_chunks = 0;
  film_data->current_chunk = 0;
  film_data->total_video_chunks = 0;
  film_data->chunks = NULL;
  film_data->ticks = 0;

  // go back to the beginning
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, 0);

  // read the master chunk type
  chunk_type = stream_read_fourcc(demuxer->stream);
  // validate the chunk type
  if (chunk_type != CHUNK_FILM)
  {
    mp_msg(MSGT_DEMUX, MSGL_ERR, "Not a FILM file\n");
    return(NULL);    
  }

  // get the header size, which implicitly points past the header and
  // to the start of the data
  header_size = stream_read_dword(demuxer->stream);
  film_data->film_version = stream_read_fourcc(demuxer->stream);
  demuxer->movi_start = header_size;
  demuxer->movi_end = demuxer->stream->end_pos;
  header_size -= 16;

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
        stream_skip(demuxer->stream, 1);  // unknown byte
        mp_msg(MSGT_DECVIDEO, MSGL_V,
          "  FILM video: %d x %d\n", sh_video->disp_w,
          sh_video->disp_h);
      }
      else
        stream_skip(demuxer->stream, 9);

      // fetch the audio channels to see if there's any audio
      audio_channels = stream_read_char(demuxer->stream);
      if (audio_channels > 0)
      {
        // create and initialize the audio stream header
        sh_audio = new_sh_audio(demuxer, 0);
        demuxer->audio->sh = sh_audio;
        sh_audio->ds = demuxer->audio;

        sh_audio->wf = (WAVEFORMATEX *)malloc(sizeof(WAVEFORMATEX));

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
      break;

    case CHUNK_STAB:
      mp_msg(MSGT_DECVIDEO, MSGL_V, "parsing STAB chunk\n");

      // FPS hack based on empirical observation
      if (sh_video)
      {
        sh_video->fps = stream_read_dword(demuxer->stream);
//        if (film_data->film_version != VERSION_1_01)
//          sh_video->fps = MAGIC_FPS_CONSTANT;
        sh_video->frametime = 1.0 / sh_video->fps;
      }

      // fetch the number of chunks
      film_data->total_chunks = stream_read_dword(demuxer->stream);
      film_data->current_chunk = 0;
      mp_msg(MSGT_DECVIDEO, MSGL_V,
        "  STAB chunk contains %d chunks\n", film_data->total_chunks);

      // allocate enough entries for the chunk
      film_data->chunks = 
        (film_chunk_t *)malloc(film_data->total_chunks * sizeof(film_chunk_t));

      // build the chunk index
      for (i = 0; i < film_data->total_chunks; i++)
      {
        film_chunk = film_data->chunks[i];
        film_chunk.chunk_offset = 
          demuxer->movi_start + stream_read_dword(demuxer->stream);
        film_chunk.chunk_size = stream_read_dword(demuxer->stream);
        film_chunk.syncinfo1 = stream_read_dword(demuxer->stream);
        film_chunk.syncinfo2 = stream_read_dword(demuxer->stream);
        film_data->chunks[i] = film_chunk;

        // audio housekeeping
        if (sh_audio)
        {
          if ((film_chunk.syncinfo1 == 0xFFFFFFFF) && 
            (film_chunk.chunk_size > largest_audio_chunk))
            largest_audio_chunk = film_chunk.chunk_size;
          film_data->total_audio_sample_count +=
            (chunk_size / sh_audio->wf->nChannels);
        }

        // video housekeeping
        if (sh_video)
        {
          if (film_chunk.syncinfo1 != 0xFFFFFFFF)
            film_chunk.video_chunk_number =
              film_data->total_video_chunks++;
        }
      }
      break;

    default:
      mp_msg(MSGT_DEMUX, MSGL_ERR, "Unrecognized FILM header chunk: %08X\n",
        chunk_type);
      return(NULL);    
      break;
    }
  }

  demuxer->priv = film_data;

  return demuxer;
}
