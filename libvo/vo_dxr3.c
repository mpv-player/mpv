#define PES_MAX_SIZE 2048
/* 
 * vo_dxr3.c - DXR3/H+ video out
 *
 * Copyright (C) 2001 David Holm <dholm@iname.com>
 *
 * libav - MPEG-PS multiplexer, part of ffmpeg
 * Copyright Gerard Lantau  (see http://ffmpeg.sf.net)
 *
 *
 *
 *
 *
 *	*** NOTICE ***
 * Further development of this device will be carried out using
 * the new libvo2 system, meanwhile I hope someone can find where
 * the lockup problem is located (caused by using subpics or audio
 * (video gets blocked))
 *
 */

#include "fastmemcpy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/kernel.h>

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
static unsigned char *spubuf=NULL;
static int outbuf_size = 0;
static int v_width,v_height;
static int s_width,s_height;
static int s_pos_x,s_pos_y;
static int d_pos_x,d_pos_y;
static int osd_w,osd_h;
static int img_format = 0;
static int palette[] = { 0x000000, 0x494949, 0xb5b5b5, 0xffffff };

static vo_info_t vo_info = 
{
	"DXR3/H+ video out",
	"dxr3",
	"David Holm <dholm@iname.com>",
	""
};

static uint32_t
init(uint32_t scr_width, uint32_t scr_height, uint32_t width, uint32_t height, uint32_t fullscreen, char *title, uint32_t format)
{
    if( dxr3_get_status() == DXR3_STATUS_CLOSED )
    {
	if( dxr3_open( "/dev/em8300" ) != 0 ) { printf( "Error opening /dev/em8300\n" ); return -1; }
	printf( "DXR3 status: %s\n",  dxr3_get_status() ? "opened":"closed" );
    }

    /* Subpic code isn't working yet, don't set to ON 
       unless you are really sure what you are doing */
    dxr3_subpic_set_mode( DXR3_SPU_MODE_ON );
    dxr3_subpic_set_palette( (char*)palette );
    spubuf = malloc(53220); //53220bytes is the standardized max size of a subpic
    
    if( dxr3_set_playmode( DXR3_PLAYMODE_PLAY ) !=0 ) printf( "Error setting playmode of DXR3\n" );

    img_format = format;
    v_width = width;
    v_height = height;
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
        codec_context.frame_rate=25*FRAME_RATE_BASE;
        codec_context.gop_size=0;
        codec_context.flags=CODEC_FLAG_QSCALE;
        codec_context.quality=1;
	codec_context.pix_fmt = PIX_FMT_YUV420P;
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
	s_width = codec_context.width;
	s_height = codec_context.height;;

    
        osd_w=scr_width;
        d_pos_x=(codec_context.width-(int)scr_width)/2;
        if(d_pos_x<0){
          s_pos_x=-d_pos_x;d_pos_x=0;
          osd_w=codec_context.width;
        } else s_pos_x=0;
    
        osd_h=scr_height;
        d_pos_y=(codec_context.height-(int)scr_height)/2;
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
        
        outbuf_size=10000+width*height;
        outbuf = malloc(outbuf_size);
    
        size = codec_context.width*codec_context.height;
        picture_buf = malloc((size * 3)/2); /* size for YUV 420 */
        
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
    else if(format==IMGFMT_BGR24)
    {
#ifdef USE_LIBAVCODEC
	int size = 0;
	printf("Format: BGR24\n");

        if(!avcodec_inited)
	{
	  avcodec_init();
          avcodec_register_all();
          avcodec_inited=1;
        }
    
        /* find the mpeg1 video encoder */
        codec = avcodec_find_encoder(CODEC_ID_MPEG1VIDEO);
        if (!codec) 
	{
            fprintf(stderr, "mpeg1 codec not found\n");
            return -1;
        }
            
        outbuf_size=10000+width*height;
        outbuf = malloc(outbuf_size);
	
        memset(&codec_context,0,sizeof(codec_context));
        codec_context.bit_rate=100000;
        codec_context.frame_rate=25*FRAME_RATE_BASE;
        codec_context.gop_size=0;
        codec_context.flags=CODEC_FLAG_QSCALE;
        codec_context.quality=1;
	codec_context.pix_fmt = PIX_FMT_YUV420P;

        /*if(width<=352 && height<=288){
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
        }*/
	s_width = codec_context.width = width;
	s_height = codec_context.height = height;

        osd_w=scr_width;
        d_pos_x=(codec_context.width-(int)scr_width)/2;
        if(d_pos_x<0){
          s_pos_x=-d_pos_x;d_pos_x=0;
          osd_w=codec_context.width;
        } else s_pos_x=0;
    
        osd_h=scr_height;
        d_pos_y=(codec_context.height-(int)scr_height)/2;
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

        size = 10000+codec_context.width*codec_context.height;
        picture_buf = malloc((size * 3)/2);
        
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
    else if(format==IMGFMT_YUY2)
    {
#ifdef USE_LIBAVCODEC
	int size = 0;
	printf("Format: YUY2\n");

        if(!avcodec_inited)
	{
	  avcodec_init();
          avcodec_register_all();
          avcodec_inited=1;
        }
    
        /* find the mpeg1 video encoder */
        codec = avcodec_find_encoder(CODEC_ID_MPEG1VIDEO);
        if (!codec) 
	{
            fprintf(stderr, "mpeg1 codec not found\n");
            return -1;
        }
            
        outbuf_size=10000+width*height;
        outbuf = malloc(outbuf_size);
	
        memset(&codec_context,0,sizeof(codec_context));
        codec_context.bit_rate=100000;
        codec_context.frame_rate=25*FRAME_RATE_BASE;
        codec_context.gop_size=0;
        codec_context.flags=CODEC_FLAG_QSCALE;
        codec_context.quality=1;
	codec_context.pix_fmt = PIX_FMT_YUV420P;

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
	s_width = codec_context.width;
	s_height = codec_context.height;;
	/* FOR DEBUGGING ONLY!! */
	codec_context.width = width;
	codec_context.height = height;
	
        osd_w=scr_width;
        d_pos_x=(codec_context.width-(int)scr_width)/2;
        if(d_pos_x<0){
          s_pos_x=-d_pos_x;d_pos_x=0;
          osd_w=codec_context.width;
        } else s_pos_x=0;
    
        osd_h=scr_height;
        d_pos_y=(codec_context.height-(int)scr_height)/2;
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

        size = 10000+codec_context.width*codec_context.height;
        picture_buf = malloc((size * 3)/2);
        
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

static const vo_info_t* get_info(void)
{
    return &vo_info;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char* src, unsigned char *srca, int srcstride)
{
    int x,y,index=0;
    int n_rles=0, prev_nibbled=0, nibbled=0;
    char prevcolor=0;
    unsigned char *dst = spubuf;
    unsigned short *subpic_size, *cs_table;
    subpic_size = dst+=2;
    cs_table = dst+=2;
    prevcolor = src[0];
    for( y = 0; y <= (h-1); y+=2 )
    {
	for( x = 0; x < w; x++ )
	{
	    if( prevcolor == src[x+(y*w)] ) index++;
	    else
	    {
		if( prevcolor < 64 )
		    prevcolor = 0x00;
		else if( prevcolor < 128 )
		    prevcolor = 0x01;
		else if( prevcolor < 192 )
		    prevcolor = 0x02;
		else
		    prevcolor = 0x03;
	    }
	}
    }

    dxr3_subpic_write(spubuf,(dst-spubuf));
}

static void draw_osd(void)
{
    vo_draw_text(osd_w,osd_h,draw_alpha);
}

static uint32_t draw_frame(uint8_t * src[])
{
    if( img_format == IMGFMT_MPEGPES )
    {
        int data_left;
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
    else if( img_format == IMGFMT_BGR24 )
    {
	int tmp_size, out_size;
	int wrap, wrap3, x, y;
	int r, g, b, R, G, B, h = v_height, w = v_width;
	unsigned char *s, *Y, *U, *V;

	printf( "dS: %dx%d dD: %dx%d S: %dx%d V: %dx%d\n",s_pos_x,s_pos_y,d_pos_x,d_pos_y,s_width,s_height,v_width,v_height);
        if(d_pos_x+w>picture.linesize[0]) w=picture.linesize[0]-d_pos_x;
        if(d_pos_y+h>codec_context.height) h=codec_context.height-d_pos_y;
	
	Y = picture.data[0]+d_pos_x+(d_pos_y*picture.linesize[0]);
	U = picture.data[1]+(d_pos_x/2)+((d_pos_y/2)*picture.linesize[1]);
	V = picture.data[2]+(d_pos_x/2)+((d_pos_y/2)*picture.linesize[2]);

	//BGR24->YUV420P from ffmpeg, see ffmpeg.sourceforge.net for terms of license
#define SCALEBITS	8
#define ONE_HALF	(1 << (SCALEBITS - 1))
#define FIX(x)		((int) ((x) * (1L<<SCALEBITS) + 0.5))
	wrap = s_width;
	wrap3 = w * 3;
        s = src[0]+s_pos_x+(s_pos_y*wrap3);
	for( y = 0; y < h; y+=2 )
	{
	    for( x = 0; x < w; x+=2 )
	    {
		b = s[0];
		g = s[1];
		r = s[2];
		R = r;
		G = g;
		B = b;
		Y[0] = (FIX(0.29900) * r + FIX(0.58700) * g + FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
		b = s[3];
		g = s[4];
		r = s[5];
		R += r;
		G += g;
		B += b;
		Y[1] = (FIX(0.29900) * r + FIX(0.58700) * g + FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
		s += wrap3;
		Y += wrap;

		b = s[0];
		g = s[1];
		r = s[2];
		R += r;
		G += g;
		B += b;
		Y[0] = (FIX(0.29900) * r + FIX(0.58700) * g + FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
		b = s[3];
		g = s[4];
		r = s[5];
		R += r;
		G += g;
		B += b;
		Y[1] = (FIX(0.29900) * r + FIX(0.58700) * g + FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
		U[0] = ((- FIX(0.16874) * R - FIX(0.33126) * G - FIX(0.50000) * B + 4 * ONE_HALF - 1) >> (SCALEBITS + 2)) + 128;
		V[0] = ((FIX(0.50000) * R - FIX(0.41869) * G - FIX(0.08131) * B + 4 * ONE_HALF - 1) >> (SCALEBITS + 2)) + 128;
		
		U++;
		V++;
		s -= (wrap3-6);
		Y -= (wrap-(3/2));
	    }
	    s += wrap3;
	    Y += wrap;
	}
#undef SCALEBITS
#undef ONE_HALF
#undef FIX(x)
	//End of ffmpeg code, see ffmpeg.sourceforge.net for terms of license
        tmp_size = out_size = avcodec_encode_video(&codec_context, outbuf, outbuf_size, &picture);
	while( out_size )
		out_size -= dxr3_video_write( &outbuf[tmp_size-out_size], out_size );
	return 0;
    }
    else if( img_format == IMGFMT_YUY2 )
    {
	int tmp_size, out_size;	
        tmp_size = out_size = avcodec_encode_video(&codec_context, outbuf, outbuf_size, &picture);
        while( out_size )
		out_size -= dxr3_video_write( &outbuf[tmp_size-out_size], out_size );

	return 0;
    }
#endif
    
    printf( "Error in draw_frame(...)\n" );
    return -1;
}

static void flip_page (void)
{
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
        int out_size, tmp_size;
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
	
        tmp_size = out_size = avcodec_encode_video(&codec_context, outbuf, outbuf_size, &picture);
        while( out_size )
		out_size -= dxr3_video_write( &outbuf[tmp_size-out_size], out_size );

	return 0;
#endif
	return -1;
    }
    else if( img_format == IMGFMT_BGR24 )
    {
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
    if(format==IMGFMT_MPEGPES) return 0x2|0x4;
#ifdef USE_LIBAVCODEC
    if(format==IMGFMT_YV12) return 0x1|0x4;
    if(format==IMGFMT_YUY2) return 0x1|0x4;
    if(format==IMGFMT_BGR24) return 0x1|0x4;
#else
    if(format==IMGFMT_YV12) {printf("You need to compile with libavcodec or ffmpeg.so to play this file!\n" ); return 0;}
    if(format==IMGFMT_YUY2) {printf("You need to compile with libavcodec or ffmpeg.so to play this file!\n" ); return 0;}
    if(format==IMGFMT_BGR24) {printf("You need to compile with libavcodec or ffmpeg.so to play this file!\n" ); return 0;}
#endif    
    return 0;
}

static void
uninit(void)
{
    free(outbuf);
    free(picture_buf);
    free(spubuf);
    dxr3_close( );
}


static void check_events(void)
{
}

