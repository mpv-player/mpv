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
//#include "RTjpegN.h"
//#include "minilzo.h"


struct nuv_signature
{ 
	char finfo[12];     /* "NuppelVideo" + \0 */
	char version[5];    /* "0.05" + \0 */
};


void demux_seek_nuv ( demuxer_t *demuxer, float rel_seek_secs, int flags )
{
}


int demux_nuv_fill_buffer ( demuxer_t *demuxer )
{
	struct rtframeheader rtjpeg_frameheader;
	int orig_pos;

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
	    /* put RTjpeg tables, Video info to video buffer */
	    stream_seek ( demuxer->stream, orig_pos );
	    ds_read_packet ( demuxer->video, demuxer->stream, rtjpeg_frameheader.packetlength + 12, 
		rtjpeg_frameheader.timecode / 1000, orig_pos, 0 );
	}
	
	/* copy PCM only */
	if (demuxer->audio && (rtjpeg_frameheader.frametype == 'A') &&
	    (rtjpeg_frameheader.comptype == '0'))
	{
	    /* put Audio to audio buffer */
	    ds_read_packet ( demuxer->audio, demuxer->stream, rtjpeg_frameheader.packetlength, 
		rtjpeg_frameheader.timecode / 1000, orig_pos + 12, 0 );
	}

	return 1;
}


demuxer_t* demux_open_nuv ( demuxer_t* demuxer )
{
	sh_video_t *sh_video = NULL;
	sh_audio_t *sh_audio = NULL;
	struct rtfileheader rtjpeg_fileheader;
	struct rtframeheader rtjpeg_frameheader;
	unsigned long int tbls[128];
	int bytes_read;

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
		sh_video->aspect = (float) 1.33;

	/* Get the FPS */
	sh_video->fps = rtjpeg_fileheader.fps;
	sh_video->frametime = 1 / sh_video->fps;

#if 1
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
#endif

	return demuxer;
}

int nuv_check_file ( demuxer_t* demuxer )
{
	struct nuv_signature ns;

	/* Store original position */
	int orig_pos = stream_tell(demuxer->stream);

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
