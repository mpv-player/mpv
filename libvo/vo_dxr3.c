/* 
 * vo_dxr3.c - DXR3/H+ video out
 *
 * Copyright (C) 2001 David Holm <dholm@iname.com>
 *
 */

#include "fastmemcpy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/em8300.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#ifdef USE_MP1E
#include "../libmp1e/libmp1e.h"
#endif

#include "../postproc/rgb2rgb.h"
#ifdef HAVE_MMX
#include "mmx.h"
#endif

LIBVO_EXTERN (dxr3)

#ifdef USE_MP1E
rte_context *mp1e_context = NULL;
rte_codec *mp1e_codec = NULL;
rte_buffer mp1e_buffer;
#endif
struct { uint16_t Y,U,V; } YUV_s;
#define RGBTOY(R,G,B) (uint16_t)( (0.257 * R) + (0.504 * G) + (0.098 * B) + 16 )
#define RGBTOU(R,G,B) (uint16_t)( -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128 )
#define RGBTOV(R,G,B) (uint16_t)( (0.439 * R) - (0.368 * G) - (0.071 * B) + 128 )
#define RGBTOYUV(R,G,B) YUV_s.Y = RGBTOY(R,G,B); \
			YUV_s.U = RGBTOU(R,G,B); \
			YUV_s.V = RGBTOV(R,G,B);

static unsigned char *picture_data[3];
static unsigned int picture_linesize[3];

static int v_width,v_height;
static int s_width,s_height;
static int c_width,c_height;
static int s_pos_x,s_pos_y;
static int d_pos_x,d_pos_y;
static int osd_w,osd_h;

static int img_format = 0;

static int fd_control = -1;
static int fd_video = -1;
static int fd_spu = -1;
static int ioval = 0;

static vo_info_t vo_info = 
{
	"DXR3/H+ video out",
	"dxr3",
	"David Holm <dholm@iname.com>",
	""
};

#ifdef USE_MP1E
void write_dxr3( rte_context* context, void* data, size_t size, void* user_data )
{
    write( fd_video, data, size );
}
#endif

static uint32_t init(uint32_t scr_width, uint32_t scr_height, uint32_t width, uint32_t height, uint32_t fullscreen, char *title, uint32_t format)
{
    fd_control = open( "/dev/em8300", O_WRONLY );
    if( fd_control < 1 )
    {
	printf( "VO: [dxr3] Error opening /dev/em8300 for writing!\n" );
	return -1;
    }
    fd_video = open( "/dev/em8300_mv", O_WRONLY );
    if( fd_video < 0 )
    {
	printf( "VO: [dxr3] Error opening /dev/em8300_mv for writing!\n" );
	return -1;
    }
    else printf( "VO: [dxr3] Opened /dev/em8300_mv\n" );
    fd_spu = open( "/dev/em8300_sp", O_WRONLY );
    if( fd_spu < 0 )
    {
	printf( "VO: [dxr3] Error opening /dev/em8300_sp for writing!\n" );
	return -1;
    }

    /* Subpic code isn't working yet, don't set to ON 
       unless you are really sure what you are doing */
    ioval = EM8300_SPUMODE_OFF;
    if( ioctl( fd_control, EM8300_IOCTL_SET_SPUMODE, &ioval ) < 0 )
    {
	printf( "VO: [dxr3] Unable to set subpicture mode!\n" );
	return -1;
    }

    ioval = EM8300_PLAYMODE_PLAY;
    if( ioctl( fd_control, EM8300_IOCTL_SET_PLAYMODE, &ioval ) < 0 )
	printf( "VO: [dxr3] Unable to set playmode!\n" );
    
    close( fd_control );

    img_format = format;
    v_width = width;
    v_height = height;
    s_width = scr_width;
    s_height = scr_height;

    if( format == IMGFMT_YV12 || format == IMGFMT_YUY2 || format == IMGFMT_BGR24 )
    {
#ifdef USE_MP1E
	int size;
	enum rte_frame_rate frame_rate;

	if( !rte_init() )
	{
	    printf( "VO: [dxr3] Unable to initialize MP1E!\n" );
	    return -1;
	}
	
        if(width<=352 && height<=288){
          c_width=352;
          c_height=288;
        } else
        if(width<=352 && height<=576){
          c_width=352;
          c_height=576;
        } else
        if(width<=480 && height<=576){
          c_width=480;
          c_height=576;
        } else
        if(width<=544 && height<=576){
          c_width=544;
          c_height=576;
        } else {
          c_width=704;
          c_height=576;
        }
	
	mp1e_context = rte_context_new( c_width, c_height, NULL );
	rte_set_verbosity( mp1e_context, 0 );
	
	printf( "VO: [dxr3] %dx%d => %dx%d\n", v_width, v_height, c_width, c_height );

	if( !mp1e_context )
	{
	    printf( "VO: [dxr3] Unable to create context!\n" );
	    return -1;
	}
	
	if( !rte_set_format( mp1e_context, "mpeg1" ) )
	{
	    printf( "VO: [dxr3] Unable to set format\n" );
	    return -1;
	}

	rte_set_mode( mp1e_context, RTE_VIDEO );
	mp1e_codec = rte_codec_set( mp1e_context, RTE_STREAM_VIDEO, 0, "mpeg1-video" );

	if( vo_fps < 24.0 ) frame_rate = RTE_RATE_1;
	else if( vo_fps < 25.0 ) frame_rate = RTE_RATE_2;
	else if( vo_fps < 29.97 ) frame_rate = RTE_RATE_3;
	else if( vo_fps < 30.0 ) frame_rate = RTE_RATE_4;
	else if( vo_fps < 50.0 ) frame_rate = RTE_RATE_5;
	else if( vo_fps < 59.97 ) frame_rate = RTE_RATE_6;
	else if( vo_fps < 60.0 ) frame_rate = RTE_RATE_7;
	else if( vo_fps > 60.0 ) frame_rate = RTE_RATE_8;
	else frame_rate = RTE_RATE_NORATE;

        if( !rte_set_video_parameters( mp1e_context, RTE_YUV420, mp1e_context->width,
					mp1e_context->height, frame_rate,
					3e6, "I" ) )
        {
            printf( "VO: [dxr3] Unable to set mp1e context!\n" );
	    rte_context_destroy( mp1e_context );
	    return -1;
	}
	
	rte_set_input( mp1e_context, RTE_VIDEO, RTE_PUSH, TRUE, NULL, NULL, NULL );
	rte_set_output( mp1e_context, (void*)write_dxr3, NULL, NULL );
	
	if( !rte_init_context( mp1e_context ) )
	{
	    printf( "VO: [dxr3] Unable to init mp1e context!\n" );
	    rte_context_delete( mp1e_context );
	    return -1;
	}

        osd_w=scr_width;
        d_pos_x=(c_width-(int)scr_width)/2;
        if(d_pos_x<0)
	{
    	    s_pos_x=-d_pos_x;d_pos_x=0;
    	    osd_w=c_width;
        } else s_pos_x=0;
    
        osd_h=scr_height;
        d_pos_y=(c_height-(int)scr_height)/2;
        if(d_pos_y<0)
	{
    	    s_pos_y=-d_pos_y;d_pos_y=0;
    	    osd_h=c_height;
        } else s_pos_y=0;
    
        printf("VO: [dxr3] Position mapping: %d;%d => %d;%d\n",s_pos_x,s_pos_y,d_pos_x,d_pos_y);
                
        size = c_width*c_height;

        picture_data[0] = malloc((size * 3)/2);
	picture_data[1] = picture_data[0] + size;
	picture_data[2] = picture_data[1] + size / 4;
	picture_linesize[0] = c_width;
	picture_linesize[1] = c_width / 2;
	picture_linesize[2] = c_width / 2;

	// Set the border colorwou
	RGBTOYUV(0,0,0)
        memset( picture_data[0], YUV_s.Y, picture_linesize[0]*c_height );
        memset( picture_data[1], YUV_s.U, picture_linesize[1]*(c_height/2) );
        memset( picture_data[2], YUV_s.V, picture_linesize[2]*(c_height/2) );
	
	if( !rte_start_encoding( mp1e_context ) )
	{
	    printf( "VO: [dxr3] Unable to start mp1e encoding!\n" );
	    uninit();
	    return -1;
	}

	return 0;
#endif
	return -1;
    }
    else if(format==IMGFMT_MPEGPES)
    {
	printf( "VO: [dxr3] Format: MPEG-PES (no conversion needed)\n" );
	return 0;
    }

    printf( "VO: [dxr3] Format: Unsupported\n" );
    return -1;
}

static const vo_info_t* get_info(void)
{
    return &vo_info;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char* src, unsigned char *srca, int srcstride)
{
}

static void draw_osd(void)
{
}

static uint32_t draw_frame(uint8_t * src[])
{
    if( img_format == IMGFMT_MPEGPES )
    {
        int data_left;
	vo_mpegpes_t *p=(vo_mpegpes_t *)src[0];

	data_left = p->size;
	while( data_left )
	    data_left -= write( fd_video, &((unsigned char*)p->data)[p->size-data_left], data_left );

	return 0;
    }
#ifdef USE_MP1E
    else if( img_format == IMGFMT_YUY2 )
    {
	int w=v_width,h=v_height;
	unsigned char *s,*dY,*dU,*dV;
	
        if(d_pos_x+w>picture_linesize[0]) w=picture_linesize[0]-d_pos_x;
        if(d_pos_y+h>c_height) h=c_height-d_pos_y;

	s = src[0]+s_pos_x+s_pos_y*(w*2);
	dY = picture_data[0]+d_pos_x+d_pos_y*picture_linesize[0];
	dU = picture_data[1]+(d_pos_x/2)+(d_pos_y/2)*picture_linesize[1];
	dV = picture_data[2]+(d_pos_x/2)+(d_pos_y/2)*picture_linesize[2];
	
	yuy2toyv12( s, dY, dU, dV, w, h, picture_linesize[0], picture_linesize[1], w*2 );
	
	mp1e_buffer.data = picture_data[0];
	mp1e_buffer.time = vo_pts/90000.0;
	mp1e_buffer.user_data = NULL;
	rte_push_video_buffer( mp1e_context, &mp1e_buffer );
	return 0;
    }
    else if( img_format == IMGFMT_BGR24 )
    {
	int x,y,w=v_width,h=v_height;
	unsigned char *s,*dY,*dU,*dV;
	
        if(d_pos_x+w>picture_linesize[0]) w=picture_linesize[0]-d_pos_x;
        if(d_pos_y+h>c_height) h=c_height-d_pos_y;

	s = src[0]+s_pos_y*(w*3);

	dY = picture_data[0]+d_pos_y*picture_linesize[0];
	dU = picture_data[1]+(d_pos_y/2)*picture_linesize[1];
	dV = picture_data[2]+(d_pos_y/2)*picture_linesize[2];
	
	for(y=0;y<h;y++)
	{
	    dY+=d_pos_x;
	    dU+=d_pos_x/4;
	    dV+=d_pos_x/4;
	    s+=s_pos_x;
	    for(x=0;x<w;x+=4)
	    {
		RGBTOYUV(s[2],s[1],s[0]);
		s+=3;
		*dY = YUV_s.Y;dY++;
//	The chrominance is shifted, ppl will have to settle with b&w for now ;)
//		*dU = YUV_s.U;dU++;
//		*dV = YUV_s.V;dV++;
		
		*dY = RGBTOY(s[2],s[1],s[0]);dY++;
		s+=3;
		*dY = RGBTOY(s[2],s[1],s[0]);dY++;
		s+=3;
		*dY = RGBTOY(s[2],s[1],s[0]);dY++;
		s+=3;
	    }
	    dY+=d_pos_x;
	    dU+=d_pos_x/4;
	    dV+=d_pos_x/4;
	    s+=s_pos_x;
	}
	
	mp1e_buffer.data = picture_data[0];
	mp1e_buffer.time = vo_pts/90000.0;
	mp1e_buffer.user_data = NULL;
	rte_push_video_buffer( mp1e_context, &mp1e_buffer );
	
	return 0;
    }
#endif
    return -1;
}

static void flip_page (void)
{
#ifdef USE_MP1E
    if( img_format == IMGFMT_YV12 )
    {
	mp1e_buffer.data = picture_data[0];
	mp1e_buffer.time = vo_pts/90000.0;
	mp1e_buffer.user_data = NULL;
	rte_push_video_buffer( mp1e_context, &mp1e_buffer );
    }
#endif
}

static uint32_t draw_slice( uint8_t *srcimg[], int stride[], int w, int h, int x0, int y0 )
{
    if( img_format == IMGFMT_YV12 )
    {
	int y;
	unsigned char *s,*s1;
	unsigned char *d,*d1;

	x0+=d_pos_x;
	y0+=d_pos_y;

        if(x0+w>picture_linesize[0]) w=picture_linesize[0]-x0;
        if(y0+h>c_height) h=c_height-y0;

        s=srcimg[0]+s_pos_x+s_pos_y*stride[0];
        d=picture_data[0]+x0+y0*picture_linesize[0];
	for(y=0;y<h;y++)
	{
	    memcpy(d,s,w);
	    s+=stride[0];
	    d+=picture_linesize[0];
	}

	w/=2;h/=2;x0/=2;y0/=2;
	
	s=srcimg[1]+s_pos_x+s_pos_y*stride[1];
	d=picture_data[1]+x0+y0*picture_linesize[1];
	s1=srcimg[2]+s_pos_x+s_pos_y*stride[2];
	d1=picture_data[2]+x0+y0*picture_linesize[2];
	for(y=0;y<h;y++)
	{
	    memcpy(d,s,w);
	    memcpy(d1,s1,w);
	    s+=stride[1];s1+=stride[2];
	    d+=picture_linesize[1];d1+=picture_linesize[2];
	}

	return 0;
    }

    return -1;
}


static uint32_t
query_format(uint32_t format)
{
    if(format==IMGFMT_MPEGPES) return 1|256;
#ifdef USE_MP1E
    if(format==IMGFMT_YV12) return 0x1|0x4;
    if(format==IMGFMT_YUY2) return 0x1|0x4;
    if(format==IMGFMT_BGR24) { printf( "VO: [dxr3] WARNING\tExperimental output, black&white only and very slow\n\t(will be inproved later, this format is rarely used)\n" ); return 0x1|0x4; }
    else printf( "VO: [dxr3] Format unsupported, mail dholm@iname.com\n" );
#else
    else printf( "VO: [dxr3] You have disabled libmp1e support, you won't be able to play this format!\n" );
#endif
    return 0;
}

static void uninit(void)
{
    printf( "VO: [dxr3] Uninitializing\n" );
    if( picture_data[0] ) free(picture_data[0]);
    if( fd_video ) close(fd_video);
    if( fd_spu ) close(fd_spu);
}


static void check_events(void)
{
}

