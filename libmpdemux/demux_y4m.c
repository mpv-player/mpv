//  Y4M file parser by Rik Snel (using yuv4mpeg*.[ch] from
//  mjpeg.sourceforge.net) (derived from demux_viv.c)
//  older YUV4MPEG (used by xawtv) support by Alex Beregszaszi

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> /* strtok */

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "yuv4mpeg.h"

//#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

typedef struct {
    int framenum; 
    y4m_stream_info_t* si;
    int is_older;
} y4m_priv_t;

static int y4m_check_file(demuxer_t* demuxer){
    int orig_pos = stream_tell(demuxer->stream);
    char buf[10];
    y4m_priv_t* priv;
    
    mp_msg(MSGT_DEMUX, MSGL_V, "Checking for YUV4MPEG2\n");
    
    if(stream_read(demuxer->stream, buf, 9)!=9)
        return 0;

    buf[9] = 0;
    
    if (strncmp("YUV4MPEG2", buf, 9) && strncmp("YUV4MPEG ", buf, 9)) {
	    return 0;
    }

    demuxer->priv = malloc(sizeof(y4m_priv_t));
    priv = demuxer->priv;

    priv->is_older = 0;

    if (!strncmp("YUV4MPEG ", buf, 9))
    {
	mp_msg(MSGT_DEMUX, MSGL_V, "Found older YUV4MPEG format (used by xawtv)\n");
	priv->is_older = 1;
    }

    mp_msg(MSGT_DEMUX,MSGL_DBG2,"Success: YUV4MPEG2\n");

    stream_seek(demuxer->stream, orig_pos);

    return DEMUXER_TYPE_Y4M;
}


// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int demux_y4m_fill_buffer(demuxer_t *demux, demux_stream_t *dsds) {
  demux_stream_t *ds=demux->video;
  demux_packet_t *dp;
  y4m_priv_t *priv=demux->priv;
  y4m_frame_info_t fi;
  unsigned char *buf[3];
  int err, size;

  y4m_init_frame_info(&fi);

  demux->filepos=stream_tell(demux->stream);

  size = ((sh_video_t*)ds->sh)->disp_w*((sh_video_t*)ds->sh)->disp_h;

  dp = new_demux_packet(3*size/2);

  /* swap U and V components */
  buf[0] = dp->buffer;
  buf[1] = dp->buffer + 5*size/4;
  buf[2] = dp->buffer + size;

  if (priv->is_older)
  {
    int c;
    
    c = stream_read_char(demux->stream); /* F */
    if (c == -256)
	return 0; /* EOF */
    if (c != 'F')
    {
	mp_msg(MSGT_DEMUX, MSGL_V, "Bad frame at %d\n", (int)stream_tell(demux->stream)-1);
	return 0;
    }
    stream_skip(demux->stream, 5); /* RAME\n */
    stream_read(demux->stream, buf[0], size);
    stream_read(demux->stream, buf[1], size/4);
    stream_read(demux->stream, buf[2], size/4);
  }
  else
  {
    if ((err=y4m_read_frame(demux->stream, priv->si, &fi, buf)) != Y4M_OK) {
      mp_msg(MSGT_DEMUX, MSGL_V, "error reading frame %s\n", y4m_strerr(err));
      return 0;
    }
  }

  /* This seems to be the right way to calculate the presentation time stamp */
  dp->pts=(float)priv->framenum/((sh_video_t*)ds->sh)->fps;
  priv->framenum++;
  dp->pos=demux->filepos;
  dp->flags=0;
  ds_add_packet(ds, dp);

  return 1;
}

static demuxer_t* demux_open_y4m(demuxer_t* demuxer){
    y4m_priv_t* priv = demuxer->priv;
    y4m_ratio_t ratio;
    sh_video_t* sh=new_sh_video(demuxer,0);
    int err;

    priv->framenum = 0;
    priv->si = malloc(sizeof(y4m_stream_info_t));

    if (priv->is_older)
    {
	char buf[4];
	int frame_rate_code;

	stream_skip(demuxer->stream, 8); /* YUV4MPEG */
	stream_skip(demuxer->stream, 1); /* space */
	stream_read(demuxer->stream, (char *)&buf[0], 3);
	buf[3] = 0;
	sh->disp_w = atoi(buf);
	stream_skip(demuxer->stream, 1); /* space */
	stream_read(demuxer->stream, (char *)&buf[0], 3);
	buf[3] = 0;
	sh->disp_h = atoi(buf);
	stream_skip(demuxer->stream, 1); /* space */
	stream_read(demuxer->stream, (char *)&buf[0], 1);
	buf[1] = 0;
	frame_rate_code = atoi(buf);
	stream_skip(demuxer->stream, 1); /* new-line */
	
	if (!sh->fps)
	{
	    /* values from xawtv */
	    switch(frame_rate_code)
	    {
		case 1:
		    sh->fps = 23.976f;
		    break;
		case 2:
		    sh->fps = 24.0f;
		    break;
		case 3:
		    sh->fps = 25.0f;
		    break;
		case 4:
		    sh->fps = 29.97f;
		    break;
		case 5:
		    sh->fps = 30.0f;
		    break;
		case 6:
		    sh->fps = 50.0f;
		    break;
		case 7:
		    sh->fps = 59.94f;
		    break;
		case 8:
		    sh->fps = 60.0f;
		    break;
		default:
		    sh->fps = 25.0f;
	    }
	}
	sh->frametime = 1.0f/sh->fps;
    }
    else
    {
	y4m_init_stream_info(priv->si);
	if ((err=y4m_read_stream_header(demuxer->stream, priv->si)) != Y4M_OK) 
	    mp_msg(MSGT_DEMUXER, MSGL_FATAL, "error parsing YUV4MPEG header: %s\n", y4m_strerr(err));
	
	if(!sh->fps) {
    	    ratio = y4m_si_get_framerate(priv->si);
    	    if (ratio.d != 0)
        	sh->fps=(float)ratio.n/(float)ratio.d;
    	    else
        	sh->fps=15.0f;
	}
	sh->frametime=1.0f/sh->fps;
	
	ratio = y4m_si_get_sampleaspect(priv->si);

	sh->disp_w = y4m_si_get_width(priv->si);
	sh->disp_h = y4m_si_get_height(priv->si);

	if (ratio.d != 0 && ratio.n != 0)
	    sh->aspect = (float)(sh->disp_w*ratio.n)/(float)(sh->disp_h*ratio.d);

    	demuxer->seekable = 0;
    }

    sh->format = mmioFOURCC('Y', 'V', '1', '2');

    sh->bih=malloc(sizeof(BITMAPINFOHEADER));
    memset(sh->bih,0,sizeof(BITMAPINFOHEADER));
    sh->bih->biSize=40;
    sh->bih->biWidth = sh->disp_w;
    sh->bih->biHeight = sh->disp_h;
    sh->bih->biPlanes=3;
    sh->bih->biBitCount=12;
    sh->bih->biCompression=sh->format;
    sh->bih->biSizeImage=sh->bih->biWidth*sh->bih->biHeight*3/2; /* YV12 */

    demuxer->video->sh=sh;
    sh->ds=demuxer->video;
    demuxer->video->id=0;
		

    mp_msg(MSGT_DEMUX, MSGL_INFO, "YUV4MPEG2 Video stream %d size: display: %dx%d, codec: %ux%u\n",
            demuxer->video->id, sh->disp_w, sh->disp_h, sh->bih->biWidth,
            sh->bih->biHeight);

    return demuxer;
}

static void demux_seek_y4m(demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags) {
    sh_video_t* sh = demuxer->video->sh;
    y4m_priv_t* priv = demuxer->priv;
    int rel_seek_frames = sh->fps*rel_seek_secs;
    int size = 3*sh->disp_w*sh->disp_h/2;
    off_t curr_pos = stream_tell(demuxer->stream);

    if (priv->framenum + rel_seek_frames < 0) rel_seek_frames = -priv->framenum;

    //printf("seektoframe=%d rel_seek_secs=%f seektooffset=%ld\n", priv->framenum + rel_seek_frames, rel_seek_secs, curr_pos + rel_seek_frames*(size+6));
    //printf("framenum=%d, curr_pos=%ld, currpos/(size+6)=%f\n", priv->framenum, curr_pos, (float)curr_pos/(float)(size+6));
    priv->framenum += rel_seek_frames;

    if (priv->is_older) {
        /* Well this is easy: every frame takes up size+6 bytes
         * in the stream and we may assume that the stream pointer
         * is always at the beginning of a frame.
         * framenum is the number of the frame that is about to be
         * demuxed (counting from ONE (see demux_open_y4m)) */
        stream_seek(demuxer->stream, curr_pos + rel_seek_frames*(size+6));
    } else {
	    /* should never come here, because seeking for YUV4MPEG2 
	     * is disabled. */
	    mp_msg(MSGT_DEMUX, MSGL_WARN, "Seeking for YUV4MPEG2 not yet implemented!\n");
    }
}

static void demux_close_y4m(demuxer_t *demuxer)
{
    y4m_priv_t* priv = demuxer->priv;

    if(!priv)
      return;
    if (!priv->is_older)
	y4m_fini_stream_info(((y4m_priv_t*)demuxer->priv)->si);
    free(((y4m_priv_t*)demuxer->priv)->si);
    free(demuxer->priv);
    return;
}


const demuxer_desc_t demuxer_desc_y4m = {
  "YUV4MPEG2 demuxer",
  "y4m",
  "YUV4MPEG2",
  "Rik snel",
  "",
  DEMUXER_TYPE_Y4M,
  1, // safe autodetect
  y4m_check_file,
  demux_y4m_fill_buffer,
  demux_open_y4m,
  demux_close_y4m,
  demux_seek_y4m,
  NULL
};
