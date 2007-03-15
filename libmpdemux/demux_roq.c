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

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#define RoQ_INFO           0x1001
#define RoQ_QUAD_CODEBOOK  0x1002
#define RoQ_QUAD_VQ        0x1011
#define RoQ_SOUND_MONO     0x1020
#define RoQ_SOUND_STEREO   0x1021

#define CHUNK_TYPE_AUDIO 0
#define CHUNK_TYPE_VIDEO 1

typedef struct roq_chunk_t
{
  int chunk_type;
  off_t chunk_offset;
  int chunk_size;

  float video_chunk_number;  // in the case of a video chunk
  int running_audio_sample_count;  // for an audio chunk
} roq_chunk_t;

typedef struct roq_data_t
{
  int total_chunks;
  int current_chunk;
  int total_video_chunks;
  int total_audio_sample_count;
  roq_chunk_t *chunks;
} roq_data_t;

// Check if a stream qualifies as a RoQ file based on the magic numbers
// at the start of the file:
//  84 10 FF FF FF FF xx xx
static int roq_check_file(demuxer_t *demuxer)
{
  if ((stream_read_dword(demuxer->stream) == 0x8410FFFF) &&
      ((stream_read_dword(demuxer->stream) & 0xFFFF0000) == 0xFFFF0000))
    return DEMUXER_TYPE_ROQ;
  else
    return 0;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int demux_roq_fill_buffer(demuxer_t *demuxer, demux_stream_t *ds)
{
  sh_video_t *sh_video = demuxer->video->sh;
  roq_data_t *roq_data = (roq_data_t *)demuxer->priv;
  roq_chunk_t roq_chunk;

  if (roq_data->current_chunk >= roq_data->total_chunks)
    return 0;

  roq_chunk = roq_data->chunks[roq_data->current_chunk];

  // make sure we're at the right place in the stream and fetch the chunk
  stream_seek(demuxer->stream, roq_chunk.chunk_offset);

  if (roq_chunk.chunk_type == CHUNK_TYPE_AUDIO)
    ds_read_packet(demuxer->audio, demuxer->stream, roq_chunk.chunk_size,
      0,
      roq_chunk.chunk_offset, 0);
  else
    ds_read_packet(demuxer->video, demuxer->stream, roq_chunk.chunk_size,
      roq_chunk.video_chunk_number / sh_video->fps,
      roq_chunk.chunk_offset, 0);

  roq_data->current_chunk++;
  return 1;
}

static demuxer_t* demux_open_roq(demuxer_t* demuxer)
{
  sh_video_t *sh_video = NULL;
  sh_audio_t *sh_audio = NULL;

  roq_data_t *roq_data = malloc(sizeof(roq_data_t));
  int chunk_id;
  int chunk_size;
  int chunk_arg;
  int last_chunk_id = 0;
  int largest_audio_chunk = 0;
  int fps;

  roq_data->total_chunks = 0;
  roq_data->current_chunk = 0;
  roq_data->total_video_chunks = 0;
  roq_data->chunks = NULL;

  // position the stream and start traversing
  stream_seek(demuxer->stream, 6);
  fps = stream_read_word_le(demuxer->stream);
  while (!stream_eof(demuxer->stream))
  {
    chunk_id = stream_read_word_le(demuxer->stream);
    chunk_size = stream_read_dword_le(demuxer->stream);
    chunk_arg = stream_read_word_le(demuxer->stream);

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
        // this is a good opportunity to create a video stream header
        sh_video = new_sh_video(demuxer, 0);
        // make sure the demuxer knows about the new stream header
        demuxer->video->sh = sh_video;
        // make sure that the video demuxer stream header knows about its
        // parent video demuxer stream
        sh_video->ds = demuxer->video;

        sh_video->disp_w = stream_read_word_le(demuxer->stream);
        sh_video->disp_h = stream_read_word_le(demuxer->stream);
        stream_skip(demuxer->stream, 4);

        // custom fourcc for internal MPlayer use
        sh_video->format = mmioFOURCC('R', 'o', 'Q', 'V');

        // constant frame rate
        sh_video->fps = fps;
        sh_video->frametime = 1 / sh_video->fps;
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

        // go through the bother of making a WAVEFORMATEX structure
        sh_audio->wf = malloc(sizeof(WAVEFORMATEX));

        // custom fourcc for internal MPlayer use
        sh_audio->format = mmioFOURCC('R', 'o', 'Q', 'A');
        if (chunk_id == RoQ_SOUND_STEREO)
          sh_audio->wf->nChannels = 2;
        else
          sh_audio->wf->nChannels = 1;
        // always 22KHz, 16-bit
        sh_audio->wf->nSamplesPerSec = 22050;
        sh_audio->wf->wBitsPerSample = 16;
      }

      // index the chunk
      roq_data->chunks = (roq_chunk_t *)realloc(roq_data->chunks,
        (roq_data->total_chunks + 1) * sizeof (roq_chunk_t));
      roq_data->chunks[roq_data->total_chunks].chunk_type = CHUNK_TYPE_AUDIO;
      roq_data->chunks[roq_data->total_chunks].chunk_offset = 
        stream_tell(demuxer->stream) - 8;
      roq_data->chunks[roq_data->total_chunks].chunk_size = chunk_size + 8;
      roq_data->chunks[roq_data->total_chunks].running_audio_sample_count =
        roq_data->total_audio_sample_count;

      // audio housekeeping
      if (chunk_size > largest_audio_chunk)
        largest_audio_chunk = chunk_size;
      roq_data->total_audio_sample_count += 
        (chunk_size / sh_audio->wf->nChannels);

      stream_skip(demuxer->stream, chunk_size);
      roq_data->total_chunks++;
    }
    else if ((chunk_id == RoQ_QUAD_CODEBOOK) ||
      ((chunk_id == RoQ_QUAD_VQ) && (last_chunk_id != RoQ_QUAD_CODEBOOK)))
    {
      // index a new chunk if it's a codebook or quad VQ not following a
      // codebook
      roq_data->chunks = (roq_chunk_t *)realloc(roq_data->chunks,
        (roq_data->total_chunks + 1) * sizeof (roq_chunk_t));
      roq_data->chunks[roq_data->total_chunks].chunk_type = CHUNK_TYPE_VIDEO;
      roq_data->chunks[roq_data->total_chunks].chunk_offset = 
        stream_tell(demuxer->stream) - 8;
      roq_data->chunks[roq_data->total_chunks].chunk_size = chunk_size + 8;
      roq_data->chunks[roq_data->total_chunks].video_chunk_number = 
        roq_data->total_video_chunks++;

      stream_skip(demuxer->stream, chunk_size);
      roq_data->total_chunks++;
    }
    else if ((chunk_id == RoQ_QUAD_VQ) && (last_chunk_id == RoQ_QUAD_CODEBOOK))
    {
      // if it's a quad VQ chunk following a codebook chunk, extend the last
      // chunk
      roq_data->chunks[roq_data->total_chunks - 1].chunk_size += (chunk_size + 8);
      stream_skip(demuxer->stream, chunk_size);
    }
    else if (!stream_eof(demuxer->stream))
    {
        mp_msg(MSGT_DECVIDEO, MSGL_WARN, "Unknown RoQ chunk ID: %04X\n", chunk_id);
    }

    last_chunk_id = chunk_id;
  }

  // minimum output buffer size = largest audio chunk * 2, since each byte
  // in the DPCM encoding effectively represents 1 16-bit sample
  // (store it in wf->nBlockAlign for the time being since init_audio() will
  // step on it anyway)
  if (sh_audio)
    sh_audio->wf->nBlockAlign = largest_audio_chunk * 2;

  roq_data->current_chunk = 0;

  demuxer->priv = roq_data;

  stream_reset(demuxer->stream);

  return demuxer;
}

static void demux_close_roq(demuxer_t* demuxer) {
  roq_data_t *roq_data = demuxer->priv;

  if(!roq_data)
    return;
  free(roq_data);
}
  

demuxer_desc_t demuxer_desc_roq = {
  "RoQ demuxer",
  "roq",
  "ROQ",
  "Mike Melanson",
  "",
  DEMUXER_TYPE_ROQ,
  0, // unsafe autodetect
  roq_check_file,
  demux_roq_fill_buffer,
  demux_open_roq,
  demux_close_roq,
  NULL,
  NULL
};
