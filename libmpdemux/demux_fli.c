/*
	FLI file parser for the MPlayer program
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

typedef struct _fli_frames_t {
  int num_frames;
  int current_frame;
  off_t *filepos;
  unsigned int *frame_size;
} fli_frames_t;

void demux_seek_fli(demuxer_t *demuxer,float rel_seek_secs,int flags){
  fli_frames_t *frames = (fli_frames_t *)demuxer->priv;
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
int demux_fli_fill_buffer(demuxer_t *demuxer){
  fli_frames_t *frames = (fli_frames_t *)demuxer->priv;
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

demuxer_t* demux_open_fli(demuxer_t* demuxer){
  sh_video_t *sh_video = NULL;
  fli_frames_t *frames = (fli_frames_t *)malloc(sizeof(fli_frames_t));
  int frame_number;
  int speed;
  unsigned int frame_size;
  int magic_number;

  // go back to the beginning
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, 0);
  demuxer->movi_start = 128;
  demuxer->movi_end = stream_read_dword_le(demuxer->stream);

  magic_number = stream_read_word_le(demuxer->stream);
  
  if ((magic_number != 0xAF11) && (magic_number != 0xAF12))
  {
    mp_msg(MSGT_DEMUX, MSGL_ERR, "Bad/unknown magic number (%04x)\n",
	magic_number);
    return(NULL);    
  }

  // fetch the number of frames
  frames->num_frames = stream_read_word_le(demuxer->stream);
  frames->current_frame = 0;

  // allocate enough entries for the indices
  frames->filepos = (off_t *)malloc(frames->num_frames * sizeof(off_t));
  frames->frame_size = (int *)malloc(frames->num_frames * sizeof(int));

  // create a new video stream header
  sh_video = new_sh_video(demuxer, 0);

  // make sure the demuxer knows about the new video stream header
  // (even though new_sh_video() ought to take care of it)
  demuxer->video->sh = sh_video;

  // make sure that the video demuxer stream header knows about its
  // parent video demuxer stream (this is getting wacky), or else
  // video_read_properties() will choke
  sh_video->ds = demuxer->video;

  // custom fourcc for internal MPlayer use
  sh_video->format = mmioFOURCC('F', 'L', 'I', 'C');

  sh_video->disp_w = stream_read_word_le(demuxer->stream);
  sh_video->disp_h = stream_read_word_le(demuxer->stream);

  // skip the video depth and flags
  stream_skip(demuxer->stream, 4);

  // get the speed
  speed = stream_read_word_le(demuxer->stream);
  if (speed == 0)
    speed = 1;
  if (magic_number == 0xAF11)
    speed *= 1000/70;
  sh_video->fps = 1000 / speed;
  sh_video->frametime = 1/sh_video->fps;

  // build the frame index
  stream_seek(demuxer->stream, demuxer->movi_start);
  frame_number = 0;
  while ((!stream_eof(demuxer->stream)) && (frame_number < frames->num_frames))
  {
    frames->filepos[frame_number] = stream_tell(demuxer->stream);
    frame_size = stream_read_dword_le(demuxer->stream);
    magic_number = stream_read_word_le(demuxer->stream);
    stream_skip(demuxer->stream, frame_size - 6);

    // if this chunk has the right magic number, index it
    if ((magic_number == 0xF1FA) || (magic_number == 0xF5FA))
    {
      frames->frame_size[frame_number] = frame_size;
      frame_number++;
    }
  }

  // save the actual number of frames indexed
  frames->num_frames = frame_number;

  demuxer->priv = frames;

  return demuxer;
}

void demux_close_fli(demuxer_t* demuxer) {
  fli_frames_t *frames = demuxer->priv;

  if(!frames)
    return;

  if(frames->filepos)
    free(frames->filepos);
  if(frames->frame_size)
    free(frames->frame_size);

  free(frames);

}
