/*
 * NuppelVideo 0.05 file parser
 * for MPlayer
 * by Panagiotis Issaris <takis@lumumba.luc.ac.be>
 *
 * Reworked by alex
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
#include "nuppelvideo.h" 


struct nuv_signature
{ 
	char finfo[12];     /* "NuppelVideo" + \0 */
	char version[5];    /* "0.05" + \0 */
};

typedef struct _nuv_position_t nuv_position_t;

struct _nuv_position_t 
{
	off_t offset;
	float time;
	int frame;
	nuv_position_t* next;
};

typedef struct _nuv_info_t 
{
	int current_audio_frame;
	int current_video_frame;
	nuv_position_t *index_list;
	nuv_position_t *current_position;
} nuv_priv_t;


/**
 * Seek to a position relative to the current position, indicated in time.
 */
void demux_seek_nuv ( demuxer_t *demuxer, float rel_seek_secs, int flags )
{
#define MAX_TIME 1000000
	nuv_priv_t* priv = demuxer->priv;
	struct rtframeheader rtjpeg_frameheader;
	off_t orig_pos;
	off_t curr_pos;
	float current_time = 0;
	float start_time = MAX_TIME;
	float target_time = start_time + rel_seek_secs * 1000; /* target_time, start_time are ms, rel_seek_secs s */

	orig_pos = stream_tell ( demuxer->stream );

	if ( rel_seek_secs > 0 )
	{
		/* Seeking forward */


		while(current_time < target_time )
		{	
			if (stream_read ( demuxer->stream, (char*)& rtjpeg_frameheader, sizeof ( rtjpeg_frameheader ) ) < sizeof(rtjpeg_frameheader))
				return; /* EOF */

			if ( rtjpeg_frameheader.frametype == 'V' ) 
			{
				priv->current_position->next = (nuv_position_t*) malloc ( sizeof ( nuv_position_t ) );
				priv->current_position = priv->current_position->next;
				priv->current_position->frame = priv->current_video_frame++;
				priv->current_position->time = rtjpeg_frameheader.timecode;
				priv->current_position->offset = orig_pos;
				priv->current_position->next = NULL;

				if ( start_time == MAX_TIME )
				{
					start_time = rtjpeg_frameheader.timecode;
					/* Recalculate target time with real start time */
					target_time = start_time + rel_seek_secs*1000;
				}

				current_time = rtjpeg_frameheader.timecode;

				curr_pos = stream_tell ( demuxer->stream );
				stream_seek ( demuxer->stream, curr_pos + rtjpeg_frameheader.packetlength );

				/* Adjust current sequence pointer */
			}
			else if ( rtjpeg_frameheader.frametype == 'A' )
			{	
				if ( start_time == MAX_TIME )
				{
					start_time = rtjpeg_frameheader.timecode;
					/* Recalculate target time with real start time */
					target_time = start_time + rel_seek_secs * 1000;
				}
				current_time = rtjpeg_frameheader.timecode;


				curr_pos = stream_tell ( demuxer->stream );
				stream_seek ( demuxer->stream, curr_pos + rtjpeg_frameheader.packetlength );
			}
		}
	}
	else
	{
		/* Seeking backward */
		nuv_position_t* p;
		start_time = priv->current_position->time;

		/* Recalculate target time with real start time */
		target_time = start_time + rel_seek_secs * 1000;


		if(target_time < 0)
			target_time = 0;

		// Search the target time in the index list, get the offset
		// and go to that offset.
		p = priv->index_list;
		while ( ( p->next != NULL ) && ( p->time < target_time ) )
		{
			p = p->next;
		}
		stream_seek ( demuxer->stream, p->offset );
		priv->current_video_frame = p->frame;
	}
}


int demux_nuv_fill_buffer ( demuxer_t *demuxer )
{
	struct rtframeheader rtjpeg_frameheader;
	off_t orig_pos;
	nuv_priv_t* priv = demuxer->priv;

	orig_pos = stream_tell ( demuxer->stream );
	if (stream_read ( demuxer->stream, (char*)& rtjpeg_frameheader, sizeof ( rtjpeg_frameheader ) ) < sizeof(rtjpeg_frameheader))
	    return 0; /* EOF */

#if 0
	printf("NUV frame: frametype: %c, comptype: %c, packetlength: %d\n",
	    rtjpeg_frameheader.frametype, rtjpeg_frameheader.comptype,
	    rtjpeg_frameheader.packetlength);
#endif

	/* Skip Seekpoint, Text and Sync for now */
	if ((rtjpeg_frameheader.frametype == 'R') ||
	    (rtjpeg_frameheader.frametype == 'T') ||
	    (rtjpeg_frameheader.frametype == 'S'))
	    return 1;
	
	if (((rtjpeg_frameheader.frametype == 'D') &&
	    (rtjpeg_frameheader.comptype == 'R')) ||
	    (rtjpeg_frameheader.frametype == 'V'))
	{
	    if ( rtjpeg_frameheader.frametype == 'V' )
	    {
		priv->current_video_frame++;
		priv->current_position->next = (nuv_position_t*) malloc(sizeof(nuv_position_t));
		priv->current_position = priv->current_position->next;
		priv->current_position->frame = priv->current_video_frame;
		priv->current_position->time = rtjpeg_frameheader.timecode;
		priv->current_position->offset = orig_pos;
		priv->current_position->next = NULL;
	    }
	    /* put RTjpeg tables, Video info to video buffer */
	    stream_seek ( demuxer->stream, orig_pos );
	    ds_read_packet ( demuxer->video, demuxer->stream, rtjpeg_frameheader.packetlength + 12, 
		    rtjpeg_frameheader.timecode*0.001, orig_pos, 0 );


	} else
	/* copy PCM only */
	if (demuxer->audio && (rtjpeg_frameheader.frametype == 'A') &&
	    (rtjpeg_frameheader.comptype == '0'))
	{
	    priv->current_audio_frame++;
	    /* put Audio to audio buffer */
	    ds_read_packet ( demuxer->audio, demuxer->stream, rtjpeg_frameheader.packetlength, 
		rtjpeg_frameheader.timecode*0.001, orig_pos + 12, 0 );
	}

	return 1;
}


demuxer_t* demux_open_nuv ( demuxer_t* demuxer )
{
	sh_video_t *sh_video = NULL;
	sh_audio_t *sh_audio = NULL;
	struct rtfileheader rtjpeg_fileheader;
	nuv_priv_t* priv = (nuv_priv_t*) malloc ( sizeof ( nuv_priv_t) );
	demuxer->priv = priv;
	priv->current_audio_frame = 0;
	priv->current_video_frame = 0;


	/* Go to the start */
	stream_reset(demuxer->stream);
	stream_seek(demuxer->stream, 0);

	stream_read ( demuxer->stream, (char*)& rtjpeg_fileheader, sizeof(rtjpeg_fileheader) );

	/* no video */
	if (rtjpeg_fileheader.videoblocks == 0)
	{
	    printf("No video blocks in file\n");
	    return NULL;
	}

	/* Create a new video stream header */
	sh_video = new_sh_video ( demuxer, 0 );

	/* Make sure the demuxer knows about the new video stream header
	 * (even though new_sh_video() ought to take care of it)
	 */
	demuxer->video->sh = sh_video;

	/* Make sure that the video demuxer stream header knows about its
	 * parent video demuxer stream (this is getting wacky), or else
	 * video_read_properties() will choke
         */
	sh_video->ds = demuxer->video;

	/* Custom fourcc for internal MPlayer use */
	sh_video->format = mmioFOURCC('N', 'U', 'V', '1');

	sh_video->disp_w = rtjpeg_fileheader.width;
	sh_video->disp_h = rtjpeg_fileheader.height;

	/* NuppelVideo uses pixel aspect ratio
           here display aspect ratio is used.
	   For the moment NuppelVideo only supports 1.0 thus
	   1.33 == 4:3 aspect ratio.   
	*/
	if(rtjpeg_fileheader.aspect == 1.0)
		sh_video->aspect = (float) 4.0f/3.0f;

	/* Get the FPS */
	sh_video->fps = rtjpeg_fileheader.fps;
	sh_video->frametime = 1 / sh_video->fps;

	if (rtjpeg_fileheader.audioblocks != 0)
	{
	    sh_audio = new_sh_audio(demuxer, 0);
	    demuxer->audio->sh = sh_audio;
	    sh_audio->ds = demuxer->audio;
	    sh_audio->format = 0x1;
	    sh_audio->channels = 2;
	    sh_audio->samplerate = 44100;
	    
	    sh_audio->wf = malloc(sizeof(WAVEFORMATEX));
	    memset(sh_audio->wf, 0, sizeof(WAVEFORMATEX));
	    sh_audio->wf->wFormatTag = sh_audio->format;
	    sh_audio->wf->nChannels = sh_audio->channels;
	    sh_audio->wf->wBitsPerSample = 16;
	    sh_audio->wf->nSamplesPerSec = sh_audio->samplerate;
	    sh_audio->wf->nAvgBytesPerSec = sh_audio->wf->nChannels*
		sh_audio->wf->wBitsPerSample*sh_audio->wf->nSamplesPerSec/8;
	    sh_audio->wf->nBlockAlign = sh_audio->channels * 2;
	    sh_audio->wf->cbSize = 0;
	}

	priv->index_list = (nuv_position_t*) malloc(sizeof(nuv_position_t));
	priv->index_list->frame = 0;
	priv->index_list->time = 0;
	priv->index_list->offset = stream_tell ( demuxer->stream );
	priv->index_list->next = NULL;
	priv->current_position = priv->index_list;

	return demuxer;
}

int nuv_check_file ( demuxer_t* demuxer )
{
	struct nuv_signature ns;

	/* Store original position */
	off_t orig_pos = stream_tell(demuxer->stream);

	mp_msg ( MSGT_DEMUX, MSGL_V, "Checking for NuppelVideo\n" );

	stream_read(demuxer->stream,(char*)&ns,sizeof(ns));

	if ( strncmp ( ns.finfo, "NuppelVideo", 12 ) ) 
		return 0; /* Not a NuppelVideo file */
	if ( strncmp ( ns.version, "0.05", 5 ) ) 
		return 0; /* Wrong version NuppelVideo file */

	/* Return to original position */
	stream_seek ( demuxer->stream, orig_pos );
	return 1;
}

void demux_close_nuv(demuxer_t* demuxer) {
  nuv_priv_t* priv = demuxer->priv;
  nuv_position_t* pos;
  if(!priv)
    return;
  for(pos = priv->index_list ; pos != NULL ; ) {
    nuv_position_t* p = pos;
    pos = pos->next;
    free(p);
  }
  free(priv);
}
