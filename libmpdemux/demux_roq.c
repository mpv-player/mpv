/*
        RoQ file demuxer for the MPlayer program
        by Mike Melanson
	based on Dr. Tim Ferguson's RoQ document found at:
	  http://www.csse.monash.edu.au/~timf/videocodec.html
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

#define RoQ_INFO           0x1001
#define RoQ_QUAD_CODEBOOK  0x1002
#define RoQ_QUAD_VQ        0x1011
#define RoQ_SOUND_MONO     0x1020
#define RoQ_SOUND_STEREO   0x1021

#define CHUNK_TYPE_AUDIO 0
#define CHUNK_TYPE_VIDEO 1

#define RoQ_FPS 24

typedef struct roq_chunk_t
{
  int chunk_type;
  off_t chunk_offset;
  int chunk_size;
} roq_chunk_t;

typedef struct roq_data_t
{
  int total_chunks;
  int current_chunk;
  roq_chunk_t *chunks;
} roq_data_t;

// Check if a stream qualifies as a RoQ file based on the magic numbers
// at the start of the file:
//  84 10 FF FF FF FF 1E 00
int roq_check_file(demuxer_t *demuxer)
{
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, 0);

  if ((stream_read_dword(demuxer->stream) == 0x8410FFFF) &&
      (stream_read_dword(demuxer->stream) == 0xFFFF1E00))
    return 1;
  else
    return 0;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_roq_fill_buffer(demuxer_t *demuxer)
{
  roq_data_t *roq_data = (roq_data_t *)demuxer->priv;
  roq_chunk_t roq_chunk;
  demux_stream_t *ds;

  if (roq_data->current_chunk >= roq_data->total_chunks)
    return 0;

  roq_chunk = roq_data->chunks[roq_data->current_chunk];
  if (roq_chunk.chunk_type == CHUNK_TYPE_AUDIO)
    ds = demuxer->audio;
  else
    ds = demuxer->video;

  // make sure we're at the right place in the stream and fetch the chunk
  stream_seek(demuxer->stream, roq_chunk.chunk_offset);
  ds_read_packet(
    ds,
    demuxer->stream,
    roq_chunk.chunk_size,
//    roq_data->current_frame/sh_video->fps,
    0,
    roq_chunk.chunk_offset,
    0
  );

  roq_data->current_chunk++;
  return 1;
}

demuxer_t* demux_open_roq(demuxer_t* demuxer)
{
  sh_video_t *sh_video = NULL;
  sh_audio_t *sh_audio = NULL;

  roq_data_t *roq_data = (roq_data_t *)malloc(sizeof(roq_data_t));
  int chunk_id;
  int chunk_size;
  int chunk_arg1;
  int chunk_arg2;
  int chunk_counter = 0;
  int last_chunk_id = 0;

  roq_data->chunks = NULL;

  // position the stream and start traversing
  stream_seek(demuxer->stream, 8);
  while (!stream_eof(demuxer->stream))
  {
    chunk_id = stream_read_word_le(demuxer->stream);
    chunk_size = stream_read_word_le(demuxer->stream);
    chunk_arg1 = stream_read_word_le(demuxer->stream);
    chunk_arg2 = stream_read_word_le(demuxer->stream);

    // this is the only useful header info in the file
    if (chunk_id == RoQ_INFO)
    {
      // there should only be one RoQ_INFO chunk per file
      if (sh_video)
      {
        mp_msg(MSGT_DECVIDEO, MSGL_WARN, "Found more than one RoQ_INFO chunk\n");
        stream_skip(demuxer->stream, 8);
      }
      else
      {
        // make the header first
        sh_video = new_sh_video(demuxer, 0);
        // make sure the demuxer knows about the new stream header
        demuxer->video->sh = sh_video;
        // make sure that the video demuxer stream header knows about its
        // parent video demuxer stream
        sh_video->ds = demuxer->video;

        // this is a good opportunity to create a video stream header
        sh_video->disp_w = stream_read_word_le(demuxer->stream);
        sh_video->disp_h = stream_read_word_le(demuxer->stream);
        stream_skip(demuxer->stream, 4);

        // custom fourcc for internal MPlayer use
        sh_video->format = mmioFOURCC('R', 'o', 'Q', 'V');

        // constant frame rate
        sh_video->fps = RoQ_FPS;
        sh_video->frametime = 1 / RoQ_FPS;
      }
    }
    else if ((chunk_id == RoQ_SOUND_MONO) ||
      (chunk_id == RoQ_SOUND_STEREO))
    {
      // create the audio stream header if it hasn't been created it
      if (sh_audio == NULL)
      {
        // make the header first
        sh_audio = new_sh_audio(demuxer, 0);
        // make sure the demuxer knows about the new stream header
        demuxer->audio->sh = sh_audio;
        // make sure that the audio demuxer stream header knows about its
        // parent audio demuxer stream
        sh_audio->ds = demuxer->audio;

        // custom fourcc for internal MPlayer use
        sh_audio->format = mmioFOURCC('R', 'o', 'Q', 'A');
        // assume it's mono until there's reason to believe otherwise
        sh_audio->channels = 1;
        // always 22KHz, 16-bit
        sh_audio->samplerate = 22050;
        sh_audio->samplesize = 2;
      }

      // if it's stereo, promote the channel number
      if (chunk_id == RoQ_SOUND_STEREO)
        sh_audio->channels = 2;

      // index the chunk
      roq_data->chunks = (roq_chunk_t *)realloc(roq_data->chunks,
        (chunk_counter + 1) * sizeof (roq_chunk_t));
      roq_data->chunks[chunk_counter].chunk_type = CHUNK_TYPE_AUDIO;
      roq_data->chunks[chunk_counter].chunk_offset = 
        stream_tell(demuxer->stream) - 8;
      roq_data->chunks[chunk_counter].chunk_size = chunk_size + 8;

      stream_skip(demuxer->stream, chunk_size);
      chunk_counter++;
    }
    else if ((chunk_id == RoQ_QUAD_CODEBOOK) ||
      ((chunk_id == RoQ_QUAD_VQ) && (last_chunk_id != RoQ_QUAD_CODEBOOK)))
    {
      // index a new chunk if it's a codebook or quad VQ not following a
      // codebook
      roq_data->chunks = (roq_chunk_t *)realloc(roq_data->chunks,
        (chunk_counter + 1) * sizeof (roq_chunk_t));
      roq_data->chunks[chunk_counter].chunk_type = CHUNK_TYPE_VIDEO;
      roq_data->chunks[chunk_counter].chunk_offset = 
        stream_tell(demuxer->stream) - 8;
      roq_data->chunks[chunk_counter].chunk_size = chunk_size + 8;

      stream_skip(demuxer->stream, chunk_size);
      chunk_counter++;
    }
    else if ((chunk_id == RoQ_QUAD_VQ) && (last_chunk_id == RoQ_QUAD_CODEBOOK))
    {
      // if it's a quad VQ chunk following a codebook chunk, extend the last
      // chunk
      roq_data->chunks[chunk_counter - 1].chunk_size += (chunk_size + 8);
      stream_skip(demuxer->stream, chunk_size);
    }
    else
    {
        mp_msg(MSGT_DECVIDEO, MSGL_WARN, "Unknown RoQ chunk ID: %04X\n", chunk_id);
    }

    last_chunk_id = chunk_id;
  }

  roq_data->total_chunks = chunk_counter;
  roq_data->current_chunk = 0;

  demuxer->priv = roq_data;

  return demuxer;
}
