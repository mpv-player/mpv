#define PES_MAX_SIZE 2048
/* 
 * vo_dxr3.c - DXR3/H+ video out
 *
 * Copyright (C) 2001 David Holm <dholm@iname.com>
 *
 * libav - MPEG-PS multiplexer, part of ffmpeg
 * Copyright Gerard Lantau  (see http://ffmpeg.sf.net)
 *
 */

#include "fastmemcpy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <libdxr3/api.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "../postproc/rgb2rgb.h"
#ifdef HAVE_MMX
#include "mmx.h"
#endif

LIBVO_EXTERN (dxr3)

#ifdef USE_LIBAVCODEC

#ifdef USE_LIBAVCODEC_SO
#include <libffmpeg/avcodec.h>
#else
#include "../libavcodec/avcodec.h"
#endif
static AVPicture picture;
static AVCodec *codec=NULL;
static AVCodecContext codec_context;
extern int avcodec_inited;
#endif

static unsigned char *picture_buf=NULL;
static unsigned char *outbuf=NULL;
static int outbuf_size = 100000;
static int s_pos_x,s_pos_y;
static int d_pos_x,d_pos_y;
static int osd_w,osd_h;
static uint32_t img_format = 0;

static vo_info_t vo_info = 
{
	"DXR3/H+ video out",
	"dxr3",
	"David Holm <dholm@iname.com>",
	""
};

static uint32_t
init(uint32_t s_width, uint32_t s_height, uint32_t width, uint32_t height, uint32_t fullscreen, char *title, uint32_t format)
{
    if( dxr3_get_status() == DXR3_STATUS_CLOSED )
    {
	if( dxr3_open( "/dev/em8300", "/etc/dxr3.ux" ) != 0 ) printf( "Error loading /dev/em8300 with /etc/dxr3.ux microcode file\n" );
	printf( "DXR3 status: %s\n",  dxr3_get_status() ? "opened":"closed" );
    }
    else
	printf( "DXR3 already open\n" );

    if( dxr3_set_playmode( DXR3_PLAYMODE_PLAY ) !=0 ) printf( "Error setting playmode of DXR3\n" );

    img_format = format;
    picture_buf=NULL;
    if( format == IMGFMT_YV12 )
    {
#ifdef USE_LIBAVCODEC

	int size;

	printf("Format: YV12\n");

        if(!avcodec_inited){
	  avcodec_init();
          avcodec_register_all();
          avcodec_inited=1;
        }
    
        /* find the mpeg1 video encoder */
        codec = avcodec_find_encoder(CODEC_ID_MPEG1VIDEO);
        if (!codec) {
            fprintf(stderr, "mpeg1 codec not found\n");
            return -1;
        }
            
        memset(&codec_context,0,sizeof(codec_context));
        codec_context.bit_rate=100000; // not used
        codec_context.frame_rate=25*FRAME_RATE_BASE; // !!!!!
        codec_context.gop_size=0; // I frames only
        codec_context.flags=CODEC_FLAG_QSCALE;
        codec_context.quality=1; // quality!  1..31  (1=best,slowest)
	codec_context.pix_fmt = PIX_FMT_RGB24;
        if(width<=352 && height<=288){
          codec_context.width=352;
          codec_context.height=288;
        } else
        if(width<=352 && height<=576){
          codec_context.width=352;
          codec_context.height=576;
        } else
        if(width<=480 && height<=576){
          codec_context.width=480;
          codec_context.height=576;
        } else
        if(width<=544 && height<=576){
          codec_context.width=544;
          codec_context.height=576;
        } else {
          codec_context.width=704;
          codec_context.height=576;
        }
    
        osd_w=s_width;
        d_pos_x=(codec_context.width-(int)s_width)/2;
        if(d_pos_x<0){
          s_pos_x=-d_pos_x;d_pos_x=0;
          osd_w=codec_context.width;
        } else s_pos_x=0;
    
        osd_h=s_height;
        d_pos_y=(codec_context.height-(int)s_height)/2;
        if(d_pos_y<0){
          s_pos_y=-d_pos_y;d_pos_y=0;
          osd_h=codec_context.height;
        } else s_pos_y=0;
    
        printf("[vo] position mapping: %d;%d => %d;%d\n",s_pos_x,s_pos_y,d_pos_x,d_pos_y);
    
        /* open it */
        if (avcodec_open(&codec_context, codec) < 0) {
            fprintf(stderr, "could not open codec\n");
            return -1;
        }
        
        outbuf_size=10000+width*height;  // must be enough!
        outbuf = malloc(outbuf_size);
    
        size = codec_context.width*codec_context.height;
        picture_buf = malloc((size * 3) / 2); /* size for YUV 420 */
        
        picture.data[0] = picture_buf;
        picture.data[1] = picture.data[0] + size;
        picture.data[2] = picture.data[1] + size / 4;
        picture.linesize[0] = codec_context.width;
        picture.linesize[1] = codec_context.width / 2;
        picture.linesize[2] = codec_context.width / 2;
	return 0;
#endif
	return -1;    
    }
    else if(format==IMGFMT_MPEGPES)
    {
	printf( "Format: MPEG-PES\n" );
	return 0;
    }

    printf( "Format: Unsupported\n" );
    return -1;
}

static const vo_info_t*
get_info(void)
{
    return &vo_info;
}

#ifdef USE_LIBAVCODEC
static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
    int x,y;
    if( img_format == IMGFMT_YV12 )
	vo_draw_alpha_yv12(w,h,src,srca,stride,picture.data[0]+(x0+d_pos_x)+(y0+d_pos_y)*picture.linesize[0],picture.linesize[0]);
}
#endif

static void draw_osd(void)
{
  if( img_format == IMGFMT_YV12 )
  {
#ifdef USE_LIBAVCODEC
    vo_draw_text(osd_w,osd_h,draw_alpha);
#endif
  }
}

static uint32_t draw_frame(uint8_t * src[])
{
    int data_left;
    if( img_format == IMGFMT_MPEGPES )
    {
	vo_mpegpes_t *p=(vo_mpegpes_t *)src[0];
    
	data_left = p->size;
	while( data_left )
	    data_left -= dxr3_video_write( &((unsigned char*)p->data)[p->size-data_left], data_left );
	
	return 0;
    }
#ifdef USE_LIBAVCODEC
    else if( img_format == IMGFMT_YV12 )
    {
	printf("ERROR: Uninplemented\n");
    }
#endif
    
    printf( "Error in draw_frame(...)" );
    return -1;
}

static void flip_page (void)
{
#ifdef USE_LIBAVCODEC
    if( img_format == IMGFMT_YV12 )
    {
        int out_size, tmp_size;
        /* encode the image */
        tmp_size = out_size = avcodec_encode_video(&codec_context, outbuf, outbuf_size, &picture);
        while( out_size )
		out_size -= dxr3_video_write( &outbuf[tmp_size-out_size], out_size );
    }
#endif
}

static uint32_t draw_slice( uint8_t *srcimg[], int stride[], int w, int h, int x0, int y0 )
{
    int y;
    unsigned char* s;
    unsigned char* d;
    int data_left;
    vo_mpegpes_t *p = (vo_mpegpes_t *)srcimg[0];

    if( img_format == IMGFMT_YV12 )
    {    
#ifdef USE_LIBAVCODEC
	x0+=d_pos_x;
        y0+=d_pos_y;
        if(x0+w>picture.linesize[0]) w=picture.linesize[0]-x0; // !!
        if(y0+h>codec_context.height) h=codec_context.height-y0;

	// Y
        s=srcimg[0]+s_pos_x+s_pos_y*stride[0];
        d=picture.data[0]+x0+y0*picture.linesize[0];
	for( y = 0; y < h; y++)
	{
		memcpy(d,s,w);
		s+=stride[0];
		d+=picture.linesize[0];
        }
    
	w/=2;h/=2;x0/=2;y0/=2;

        // U
        s=srcimg[1]+(s_pos_x/2)+(s_pos_y/2)*stride[1];
        d=picture.data[1]+x0+y0*picture.linesize[1];
        for( y = 0; y < h; y++)
	{
	    memcpy(d,s,w);
    	    s+=stride[1];
    	    d+=picture.linesize[1];
        }
    
        // V
        s=srcimg[2]+(s_pos_x/2)+(s_pos_y/2)*stride[2];
        d=picture.data[2]+x0+y0*picture.linesize[2];
        for(y=0;y<h;y++)
	{
	    memcpy(d,s,w);
	    s+=stride[2];
	    d+=picture.linesize[2];
	}

	yuv2rgb_init( 24, MODE_RGB );
	yuv2rgb( outbuf, srcimg[0], srcimg[1], srcimg[2], w, h, h*3, stride[0], stride[1] );
	return 0;
#endif
	printf( "You will need libavcodec of ffmpeg.so yo play this video\n" );
	return -1;
    }
    else if( img_format == IMGFMT_MPEGPES )
    {
        data_left = p->size;
        while( data_left )
		data_left -= dxr3_video_write( &((unsigned char*)p->data)[p->size-data_left], data_left );
	return 0;
    }

    return -1;
}


static uint32_t
query_format(uint32_t format)
{
    if(format==IMGFMT_MPEGPES) return 1;
#ifdef USE_LIBAVCODEC
    if(format==IMGFMT_YV12) return 1;
#endif
    return 0;
}

static void
uninit(void)
{
#ifdef USE_LIBAVCODEC
    if( img_format == IMGFMT_YV12 )
    {
	free(outbuf);
	free(picture_buf);
    }
#endif
    dxr3_close( );
}


static void check_events(void)
{
}

