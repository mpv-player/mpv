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

#include "aspect.h"

LIBVO_EXTERN (dxr3)

#ifdef USE_MP1E
rte_context *mp1e_context = NULL;
rte_codec *mp1e_codec = NULL;
rte_buffer mp1e_buffer;
#endif

static unsigned char *picture_data[3];
static unsigned int picture_linesize[3];

static int v_width,v_height;
static int s_width,s_height;
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
    size_t data_left = size;
    if(ioctl(fd_video,EM8300_IOCTL_VIDEO_SETPTS,&vo_pts) < 0)
	    printf( "VO: [dxr3] Unable to set pts\n" );
    while( data_left )
	data_left -= write( fd_video, (void*) data+(size-data_left), data_left );
}
#endif

static uint32_t init(uint32_t scr_width, uint32_t scr_height, uint32_t width, uint32_t height, uint32_t fullscreen, char *title, uint32_t format)
{
    int tmp1,tmp2;
    
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
    
    img_format = format;
    v_width = scr_width;
    v_height = scr_height;

    s_width = (v_width+15)/16; s_width*=16;
    s_height = (v_height+15)/16; s_height*=16;
    
    /* Try to figure out whether to use ws output or not */
    tmp1 = abs(height - ((width/4)*3));
    tmp2 = abs(height - (int)(width/2.35));
    if(tmp1 < tmp2)
    {
	tmp1 = EM8300_ASPECTRATIO_4_3;
	printf( "VO: [dxr3] Setting aspect ratio to 4:3\n" );
    }
    else
    {
	tmp1 = EM8300_ASPECTRATIO_16_9;
	printf( "VO: [dxr3] Setting aspect ratio to 16:9\n" );
    }
    ioctl(fd_control,EM8300_IOCTL_SET_ASPECTRATIO,&tmp1);
    close(fd_control);
    
    if( format == IMGFMT_YV12 || format == IMGFMT_YUY2 || format == IMGFMT_BGR24 )
    {
#ifdef USE_MP1E
	int size;
	enum rte_frame_rate frame_rate;
	enum rte_pixformat pixel_format;

	if( !rte_init() )
	{
	    printf( "VO: [dxr3] Unable to initialize MP1E!\n" );
	    return -1;
	}
	
	mp1e_context = rte_context_new( s_width, s_height, NULL );
	rte_set_verbosity( mp1e_context, 0 );
	
	printf( "VO: [dxr3] %dx%d => %dx%d\n", v_width, v_height, s_width, s_height );

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

	if( format == IMGFMT_YUY2 )
	    pixel_format = RTE_YUYV;
	else
	    pixel_format = RTE_YUV420;
        if( !rte_set_video_parameters( mp1e_context, pixel_format, mp1e_context->width,
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

        osd_w=s_width;
        d_pos_x=(s_width-v_width)/2;
        if(d_pos_x<0)
	{
    	    s_pos_x=-d_pos_x;d_pos_x=0;
    	    osd_w=s_width;
        } else s_pos_x=0;
    
        osd_h=s_height;
        d_pos_y=(s_height-v_height)/2;
        if(d_pos_y<0)
	{
    	    s_pos_y=-d_pos_y;d_pos_y=0;
    	    osd_h=s_height;
        } else s_pos_y=0;
    
        printf("VO: [dxr3] Position mapping: %d;%d => %d;%d\n",s_pos_x,s_pos_y,d_pos_x,d_pos_y);
                
        size = s_width*s_height;

	if( format == IMGFMT_YUY2 )
	{
	    picture_data[0] = NULL;
	    picture_linesize[0] = s_width * 2;
	}
	else
	{
    	    picture_data[0] = malloc((size * 3)/2);
	    picture_data[1] = picture_data[0] + size;
	    picture_data[2] = picture_data[1] + size / 4;
	    picture_linesize[0] = s_width;
	    picture_linesize[1] = s_width / 2;
	    picture_linesize[2] = s_width / 2;
	}

	
	if( !rte_start_encoding( mp1e_context ) )
	{
	    printf( "VO: [dxr3] Unable to start mp1e encoding!\n" );
	    uninit();
	    return -1;
	}

	if(format == IMGFMT_BGR24) yuv2rgb_init(24, MODE_BGR);
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
    switch(img_format)
    {
    case IMGFMT_BGR24:
    case IMGFMT_YV12:
	vo_draw_alpha_yv12(w,h,src,srca,srcstride,picture_data[0]+(x0+d_pos_x)+(y0+d_pos_y)*picture_linesize[0],picture_linesize[0]);
	break;
    case IMGFMT_YUY2:
	vo_draw_alpha_yuy2(w,h,src,srca,srcstride,picture_data[0]+(x0+d_pos_x)*2+(y0+d_pos_y)*picture_linesize[0],picture_linesize[0]);
	break;
    }
}

static void draw_osd(void)
{
    vo_draw_text(osd_w,osd_h,draw_alpha);
}

static uint32_t draw_frame(uint8_t * src[])
{
    if( img_format == IMGFMT_MPEGPES )
    {
	vo_mpegpes_t *p=(vo_mpegpes_t *)src[0];
        size_t data_left = p->size;

	if(ioctl(fd_video,EM8300_IOCTL_VIDEO_SETPTS,&p->timestamp) < 0)
	    printf( "VO: [dxr3] Unable to set pts\n" );

	while( data_left )
	    data_left -= write( fd_video, (void*) p->data+(p->size-data_left), data_left );

	return 0;
    }
#ifdef USE_MP1E
    else if( img_format == IMGFMT_YUY2 )
    {
        picture_data[0] = src[0];
	return 0;
    }
    else if( img_format == IMGFMT_BGR24 )
    {
	int x,y,w=v_width,h=v_height;
	unsigned char *s,*dY,*dU,*dV;
	
        if(d_pos_x+w>picture_linesize[0]) w=picture_linesize[0]-d_pos_x;
        if(d_pos_y+h>s_height) h=s_height-d_pos_y;

	s = src[0]+s_pos_y*(w*3);

	dY = picture_data[0]+d_pos_y*picture_linesize[0];
	dU = picture_data[1]+(d_pos_y/2)*picture_linesize[1];
	dV = picture_data[2]+(d_pos_y/2)*picture_linesize[2];
	
	rgb24toyv12(s,dY,dU,dV,w,h,picture_linesize[0],picture_linesize[1],v_width*3);
	
	mp1e_buffer.data = picture_data[0];
	mp1e_buffer.time = vo_pts/90000.0;
	mp1e_buffer.user_data = NULL;
	vo_draw_text(osd_w,osd_h,draw_alpha);
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
    else if( img_format == IMGFMT_YUY2 )
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
        if(y0+h>s_height) h=s_height-y0;

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
    uint32_t flag = 0;
    if(format==IMGFMT_MPEGPES) flag = 0x2|0x4;
#ifdef USE_MP1E
    if(format==IMGFMT_YV12) flag = 0x1|0x4;
    if(format==IMGFMT_YUY2) flag = 0x1|0x4;
    if(format==IMGFMT_BGR24) flag = 0x1|0x4;
    else printf( "VO: [dxr3] Format unsupported, mail dholm@iname.com\n" );
#else
    else printf( "VO: [dxr3] You have disabled libmp1e support, you won't be able to play this format!\n" );
#endif
    return flag;
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

