/*
	FILM file parser for the MPlayer program
	by Mike Melanson
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

typedef struct _film_frames_t {
  int num_frames;
  int current_frame;
  off_t *filepos;
  unsigned int *frame_size;
  unsigned int *flags1;
  unsigned int *flags2;
} film_frames_t;

void demux_seek_film(demuxer_t *demuxer,float rel_seek_secs,int flags){
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

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_film_fill_buffer(demuxer_t *demuxer){
  film_frames_t *frames = (film_frames_t *)demuxer->priv;
  sh_video_t *sh_video = demuxer->video->sh;

  // see if the end has been reached
  if (frames->current_frame >= frames->num_frames)
    return 0;

  // fetch the frame from the file
  // first, position the file properly since ds_read_packet() doesn't
  // seem to do it, even though it takes a file offset as a parameter
  stream_seek(demuxer->stream, frames->filepos[frames->current_frame]);
  ds_read_packet(demuxer->video,
    demuxer->stream, 
    frames->frame_size[frames->current_frame],
    frames->current_frame/sh_video->fps,
    frames->filepos[frames->current_frame],
    0 /* what flags? -> demuxer.h (alex) */
  );

  // get the next frame ready
  frames->current_frame++;

  return 1;
}

demuxer_t* demux_open_film(demuxer_t* demuxer){
  sh_video_t *sh_video = NULL;
  film_frames_t *frames = (film_frames_t *)malloc(sizeof(film_frames_t));
  int header_size;
  unsigned int chunk_type;
  unsigned int chunk_size;
  int i;

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
  demuxer->movi_start = header_size;
  demuxer->movi_end = demuxer->stream->end_pos;
  header_size -= 16;

  // skip to where the next chunk should be
  stream_skip(demuxer->stream, 8);

  // create a new video stream header
  sh_video = new_sh_video(demuxer, 0);

  // make sure the demuxer knows about the new video stream header
  demuxer->video->sh = sh_video;

  // make sure that the video demuxer stream header knows about its
  // parent video demuxer stream, or else
  // video_read_properties() will choke
  sh_video->ds = demuxer->video;

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
printf ("parsing FDSC chunk\n");
      // fetch the video codec fourcc, height, then width
      sh_video->format = stream_read_fourcc(demuxer->stream);
      sh_video->disp_h = stream_read_dword(demuxer->stream);
      sh_video->disp_w = stream_read_dword(demuxer->stream);
// temporary; one of these fields has to specify FPS
stream_skip(demuxer->stream, 12);
      break;

    case CHUNK_STAB:
printf ("parsing STAB chunk\n");
      // skip unknown dword
      stream_skip(demuxer->stream, 4);

      // fetch the number of frames
      frames->num_frames = stream_read_dword(demuxer->stream);
      frames->current_frame = 1;
//      frames->current_frame = 0;

      // allocate enough entries for the indices
      frames->filepos = (off_t *)malloc(frames->num_frames * sizeof(off_t));
      frames->frame_size = (int *)malloc(frames->num_frames * sizeof(int));
      frames->flags1 = (int *)malloc(frames->num_frames * sizeof(int));
      frames->flags2 = (int *)malloc(frames->num_frames * sizeof(int));

      // build the frame index
      for (i = 0; i < frames->num_frames; i++)
      {
        frames->filepos[i] = demuxer->movi_start + stream_read_dword(demuxer->stream);
        frames->frame_size[i] = stream_read_dword(demuxer->stream);
        frames->flags1[i] = stream_read_dword(demuxer->stream);
        frames->flags2[i] = stream_read_dword(demuxer->stream);
      }
      break;

    default:
      mp_msg(MSGT_DEMUX, MSGL_ERR, "Unrecognized FILM header chunk: %08X\n",
        chunk_type);
      return(NULL);    
      break;
    }
  }

  // hard code the speed for now
  sh_video->fps = 1;
  sh_video->frametime = 1;

  demuxer->priv = frames;

  return demuxer;
}
