/*
	raw dv file parser for MPlayer
   by Alexander Neundorf <neundorf@kde.org>
   based on the fli demuxer

   LGPL
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

#ifdef HAVE_LIBDV095

#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include <libdv/dv.h>
#include <libdv/dv_types.h>

#define DV_PAL_FRAME_SIZE  144000
#define DV_NTSC_FRAME_SIZE 122000

typedef struct
{
   int current_frame;
   int frame_size;
   int current_filepos;
   int frame_number;
   dv_decoder_t *decoder;
} rawdv_frames_t;

void demux_seek_rawdv(demuxer_t *demuxer,float rel_seek_secs,int flags)
{
   rawdv_frames_t *frames = (rawdv_frames_t *)demuxer->priv;
   sh_video_t *sh_video = demuxer->video->sh;
   int newpos=(flags&1)?0:frames->current_frame;
   if(flags&2)
   {
      // float 0..1
      newpos+=rel_seek_secs*frames->frame_number;
   }
   else
   {
      // secs
      newpos+=rel_seek_secs*sh_video->fps;
   }
   if(newpos<0)
      newpos=0;
   else if(newpos>frames->frame_number)
      newpos=frames->frame_number;
   frames->current_frame=newpos;
   frames->current_filepos=newpos*frames->frame_size;
}

int check_file_rawdv(demuxer_t *demuxer)
{
   unsigned char tmp_buffer[DV_PAL_FRAME_SIZE];
   int bytes_read=0;
   int result=0;
   dv_decoder_t *td;
   stream_reset(demuxer->stream);
   stream_seek(demuxer->stream, 0);
   bytes_read=stream_read(demuxer->stream,tmp_buffer,DV_PAL_FRAME_SIZE);
   if ((bytes_read!=DV_PAL_FRAME_SIZE) && (bytes_read!=DV_NTSC_FRAME_SIZE))
      return 0;

   td=dv_decoder_new(TRUE,TRUE,FALSE);
   td->quality=DV_QUALITY_BEST;
   dv_parse_header(td, tmp_buffer);
   if ((( td->num_dif_seqs==10) || (td->num_dif_seqs==12))
       && (td->width==720)
       && ((td->height==576) || (td->height==480)))
      result=1;
   dv_decoder_free(td);
   return result;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_rawdv_fill_buffer(demuxer_t *demuxer)
{
   rawdv_frames_t *frames = (rawdv_frames_t *)demuxer->priv;
   demux_packet_t* dp_video=NULL;
   sh_video_t *sh_video = demuxer->video->sh;
   int bytes_read=0;
//   fprintf(stderr,"demux_rawdv_fill_buffer() seek to %d, size: %d\n",frames->current_filepos,frames->frame_size);
   // fetch the frame from the file
   // first, position the file properly since ds_read_packet() doesn't
   // seem to do it, even though it takes a file offset as a parameter
   stream_seek(demuxer->stream, frames->current_filepos);

   dp_video=new_demux_packet(frames->frame_size);
   bytes_read=stream_read(demuxer->stream,dp_video->buffer,frames->frame_size);
   if (bytes_read<frames->frame_size)
      return 0;
   dp_video->pts=frames->current_frame/sh_video->fps;
   dp_video->pos=frames->current_filepos;
   dp_video->flags=0;

   if (demuxer->audio)
	{
      demux_packet_t* dp_audio=clone_demux_packet(dp_video);
      ds_add_packet(demuxer->audio,dp_audio);
   }
   ds_add_packet(demuxer->video,dp_video);
   // get the next frame ready
   frames->current_filepos+=frames->frame_size;
   frames->current_frame++;
//   fprintf(stderr," audio->packs: %d , video->packs: %d \n",demuxer->audio->packs, demuxer->video->packs);
   return 1;
}

demuxer_t* demux_open_rawdv(demuxer_t* demuxer)
{
   unsigned char dv_frame[DV_PAL_FRAME_SIZE];
   sh_video_t *sh_video = NULL;
   rawdv_frames_t *frames = (rawdv_frames_t *)malloc(sizeof(rawdv_frames_t));
   dv_decoder_t *dv_decoder=NULL;

   mp_msg(MSGT_DEMUXER,MSGL_V,"demux_open_rawdv() end_pos %d\n",demuxer->stream->end_pos);

   // go back to the beginning
   stream_reset(demuxer->stream);
   stream_seek(demuxer->stream, 0);

   //get the first frame
   stream_read(demuxer->stream, dv_frame, DV_PAL_FRAME_SIZE);

   //read params from this frame
   dv_decoder=dv_decoder_new(TRUE,TRUE,FALSE);
   dv_decoder->quality=DV_QUALITY_BEST;

   dv_parse_header(dv_decoder, dv_frame);

   // create a new video stream header
   sh_video = new_sh_video(demuxer, 0);

   // make sure the demuxer knows about the new video stream header
   // (even though new_sh_video() ought to take care of it)
   demuxer->seekable = 1;
   demuxer->video->sh = sh_video;

   // make sure that the video demuxer stream header knows about its
   // parent video demuxer stream (this is getting wacky), or else
   // video_read_properties() will choke
   sh_video->ds = demuxer->video;

   // custom fourcc for internal MPlayer use
//   sh_video->format = mmioFOURCC('R', 'A', 'D', 'V');
   sh_video->format = mmioFOURCC('D', 'V', 'S', 'D');

   sh_video->disp_w = dv_decoder->width;
   sh_video->disp_h = dv_decoder->height;
   mp_msg(MSGT_DEMUXER,MSGL_V,"demux_open_rawdv() frame_size: %d w: %d h: %d dif_seq: %d system: %d\n",dv_decoder->frame_size,dv_decoder->width, dv_decoder->height,dv_decoder->num_dif_seqs,dv_decoder->system);

   sh_video->fps= (dv_decoder->system==e_dv_system_525_60?29.97:25);
   sh_video->frametime = 1.0/sh_video->fps;

  // emulate BITMAPINFOHEADER for win32 decoders:
  sh_video->bih=malloc(sizeof(BITMAPINFOHEADER));
  memset(sh_video->bih,0,sizeof(BITMAPINFOHEADER));
  sh_video->bih->biSize=40;
  sh_video->bih->biWidth = dv_decoder->width;
  sh_video->bih->biHeight = dv_decoder->height;
  sh_video->bih->biPlanes=1;
  sh_video->bih->biBitCount=24;
  sh_video->bih->biCompression=sh_video->format; // "DVSD"
  sh_video->bih->biSizeImage=sh_video->bih->biWidth*sh_video->bih->biHeight*3;


   frames->current_filepos=0;
   frames->current_frame=0;
   frames->frame_size=dv_decoder->frame_size;
   frames->frame_number=demuxer->stream->end_pos/frames->frame_size;

   mp_msg(MSGT_DEMUXER,MSGL_V,"demux_open_rawdv() seek to %d, size: %d, dv_dec->frame_size: %d\n",frames->current_filepos,frames->frame_size, dv_decoder->frame_size);
    if (dv_decoder->audio != NULL){
       sh_audio_t *sh_audio =  new_sh_audio(demuxer, 0);
	    demuxer->audio->sh = sh_audio;
	    sh_audio->ds = demuxer->audio;
       mp_msg(MSGT_DEMUXER,MSGL_V,"demux_open_rawdv() chan: %d samplerate: %d\n",dv_decoder->audio->num_channels,dv_decoder->audio->frequency );
       // custom fourcc for internal MPlayer use
       sh_audio->format = mmioFOURCC('R', 'A', 'D', 'V');

	sh_audio->wf = malloc(sizeof(WAVEFORMATEX));
	memset(sh_audio->wf, 0, sizeof(WAVEFORMATEX));
	sh_audio->wf->wFormatTag = sh_audio->format;
	sh_audio->wf->nChannels = dv_decoder->audio->num_channels;
	sh_audio->wf->wBitsPerSample = 16;
	sh_audio->wf->nSamplesPerSec = dv_decoder->audio->frequency;
	// info about the input stream:
	sh_audio->wf->nAvgBytesPerSec = sh_video->fps*dv_decoder->frame_size;
	sh_audio->wf->nBlockAlign = dv_decoder->frame_size;

//       sh_audio->context=(void*)dv_decoder;
    }
   stream_reset(demuxer->stream);
   stream_seek(demuxer->stream, 0);
   dv_decoder_free(dv_decoder);  //we keep this in the context of both stream headers
   demuxer->priv=frames;
   return demuxer;
}

void demux_close_rawdv(demuxer_t* demuxer)
{
   rawdv_frames_t *frames = (rawdv_frames_t *)demuxer->priv;

   if(frames==0)
      return;
  free(frames);
}

#endif
