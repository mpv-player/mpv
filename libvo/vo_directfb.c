/*
   MPlayer video driver for DirectFramebuffer device
  
   (C) 2001
   
   Written by  Jiri Svoboda <Jiri.Svoboda@seznam.cz>

   Inspired by vo_sdl and vo_fbdev.    
  
   To get second head working delete line 120
   from fbdev.c (from DirectFB sources version 0.9.7)
   Line contains following:
        fbdev->fd = open( "/dev/fb0", O_RDWR );

   Parts of this code taken from DirectFB examples:
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

// directfb includes

#include <directfb.h>

// other things

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <linux/fb.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "sub.h"
#include "../postproc/rgb2rgb.h"

LIBVO_EXTERN(directfb)

static vo_info_t vo_info = {
	"Direct Framebuffer Device",
	"directfb",
	"Jiri Svoboda Jiri.Svoboda@seznam.cz",
	""
};

extern int verbose;

/******************************
*	   directfb 	      *
******************************/

 /*
 * (Globals)
 */
static IDirectFB *dfb = NULL;
static IDirectFBSurface *primary = NULL;
static IDirectFBInputDevice *keyboard = NULL;
static IDirectFBDisplayLayer       *videolayer = NULL;
static DFBDisplayLayerConfig        dlc;
static int screen_width  = 0;
static int screen_height = 0;
static DFBSurfacePixelFormat frame_format;
static unsigned int frame_pixel_size = 0;
static unsigned int source_pixel_size = 0;
static int xoffset=0,yoffset=0;
#define DFBCHECK(x...)                                         \
  {                                                            \
    DFBResult err = x;                                         \
                                                               \
    if (err != DFB_OK)                                         \
      {                                                        \
        fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
        DirectFBErrorFatal( #x, err );                         \
      }                                                        \
  }

/*
 * The frame is to be loaded into a surface that we can blit from.
 */

static IDirectFBSurface *frame = NULL;

/*
 * A buffer for input events.
 */

#ifdef HAVE_DIRECTFB099
static IDirectFBEventBuffer *buffer = NULL;
#else
static IDirectFBInputBuffer *buffer = NULL;
#endif

/******************************
*	    vo_directfb       *
******************************/

/* command line/config file options */
#ifdef HAVE_FBDEV
extern char *fb_dev_name;
#else
char *fb_dev_name;
#endif

static void (*draw_alpha_p)(int w, int h, unsigned char *src,
		unsigned char *srca, int stride, unsigned char *dst,
		int dstride);

static int in_width;
static int in_height;
static int out_width;
static int out_height;
static uint32_t pixel_format;
static int fs;
static int flip;
static int stretch=0;
struct modes_t {
        int valid;
        unsigned int width;
        unsigned int height;
        int overx,overy;
        } modes [4];
static unsigned int best_bpp=5;
static unsigned int preinit_done=0;
static int no_yuy2=1;
static int no_uyvy_support=1;


DFBEnumerationResult enum_modes_callback( unsigned int width,unsigned int height,unsigned int bpp, void *data)
{
int overx=0,overy=0;
unsigned int index=bpp/8-1;
int allow_under=0;

//printf("Validator entered %i %i %i\n",width,height,bpp);

overx=width-out_width;
overy=height-out_height;
if (!modes[index].valid) {
        modes[index].valid=1;
        modes[index].width=width;
        modes[index].height=height;
        modes[index].overx=overx;
        modes[index].overy=overy;
        }
if ((modes[index].overy<0)||(modes[index].overx<0)) allow_under=1;
if (abs(overx*overy)<abs(modes[index].overx * modes[index].overy)) {
        if (((overx>=0)&&(overy>=0)) || allow_under) {
                modes[index].valid=1;
                modes[index].width=width;
                modes[index].height=height;
                modes[index].overx=overx;
                modes[index].overy=overy;
//                printf("Better mode added %i %i %i\n",width,height,bpp);
                };
        };

return DFENUM_OK;
}


DFBEnumerationResult enum_layers_callback( unsigned int                 id,
                                           DFBDisplayLayerCapabilities  caps,
                                           void                        *data )
{
     IDirectFBDisplayLayer **layer = (IDirectFBDisplayLayer **)data;
/*
     printf( "\nLayer %d:\n", id );

     if (caps & DLCAPS_SURFACE)
          printf( "  - Has a surface.\n" );

     if (caps & DLCAPS_ALPHACHANNEL)
          printf( "  - Supports blending based on alpha channel.\n" );

     if (caps & DLCAPS_COLORKEYING)
          printf( "  - Supports color keying.\n" );

     if (caps & DLCAPS_FLICKER_FILTERING)
          printf( "  - Supports flicker filtering.\n" );

     if (caps & DLCAPS_INTERLACED_VIDEO)
          printf( "  - Can natively display interlaced video.\n" );

     if (caps & DLCAPS_OPACITY)
          printf( "  - Supports blending based on global alpha factor.\n" );

     if (caps & DLCAPS_SCREEN_LOCATION)
          printf( "  - Can be positioned on the screen.\n" );

     if (caps & DLCAPS_BRIGHTNESS)
          printf( "  - Brightness can be adjusted.\n" );

     if (caps & DLCAPS_CONTRAST)
          printf( "  - Contrast can be adjusted.\n" );

     if (caps & DLCAPS_HUE)
          printf( "  - Hue can be adjusted.\n" );

     if (caps & DLCAPS_SATURATION)
          printf( "  - Saturation can be adjusted.\n" );

     printf("\n");
*/
     /* We take the first layer not being the primary */
     if (id != DLID_PRIMARY) {
          DFBResult ret;

          ret = dfb->GetDisplayLayer( dfb, id, layer );
          if (ret)
               DirectFBError( "dfb->GetDisplayLayer failed", ret );
          else
               return DFENUM_CANCEL;
     }

     return DFENUM_OK;
}

static uint32_t preinit(const char *arg)
{
     DFBSurfaceDescription dsc;
     DFBResult             ret;
     DFBDisplayLayerConfigFlags   failed;


  if (preinit_done) return 1;

  /*
   * (Initialize)
   */

//  if (!dfb) {

        DFBCHECK (DirectFBInit (NULL,NULL));
	
	if ((directfb_major_version >= 0) &&
	    (directfb_minor_version >= 9) &&
	    (directfb_micro_version >= 7))
	    no_uyvy_support = 0;
	else
	{
	    no_uyvy_support = 1;
	    printf("vo_directfb: no UYVY support. Version: %d.%d.%d\n",
		directfb_major_version, directfb_minor_version,
		directfb_micro_version);
	}

	if ((directfb_major_version >= 0) &&
	    (directfb_minor_version >= 9) &&
	    (directfb_micro_version >= 7))
	{
    	    if (!fb_dev_name && !(fb_dev_name = getenv("FRAMEBUFFER"))) fb_dev_name = "/dev/fb0";
    	    DFBCHECK (DirectFBSetOption ("fbdev",fb_dev_name));
	}

//	uncomment this if you do not wish to create a new vt for DirectFB
//       DFBCHECK (DirectFBSetOption ("no-vt-switch",fb_dev_name));

//	uncomment this if you want to allow vt switching
//       DFBCHECK (DirectFBSetOption ("vt-switching",fb_dev_name));
        DFBCHECK (DirectFBSetOption ("bg-color","00000000"));

        DFBCHECK (DirectFBCreate (&dfb));
        DFBCHECK (dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN));

  // lets try to get YUY2 layer - borrowed from DirectFb examples

     /* Enumerate display layers */
        DFBCHECK (dfb->EnumDisplayLayers( dfb, enum_layers_callback, &videolayer ));

        if (!videolayer) {
          // no yuy2 layer -> fallback to RGB
//          printf( "\nNo additional layers have been found.\n" );
          no_yuy2 = 1;
//          preinit_done = 1;
        } else {

        // there is an additional layer so test it for YUV
                dlc.flags       = /*DLCONF_WIDTH | DLCONF_HEIGHT | */DLCONF_PIXELFORMAT; //| DLCONF_OPTIONS;
        /*     dlc.width       = dsc.width;
                dlc.height      = dsc.height;*/
                dlc.pixelformat = DSPF_YUY2;
        //     dlc.options     = DLOP_INTERLACED_VIDEO;

                /* Test the configuration, getting failed fields */
                ret = videolayer->TestConfiguration( videolayer, &dlc, &failed );
                if (ret == DFB_UNSUPPORTED && no_uyvy_support == 0) {
//    	       printf("Videolayer does not support YUY2");
                        dlc.pixelformat = DSPF_UYVY;
                        ret = videolayer->TestConfiguration( videolayer, &dlc, &failed );
                        if (ret != DFB_UNSUPPORTED) { no_yuy2 = 0;}
                } else { no_yuy2 = 0;}
        }


// just look at RGB things
        modes[0].valid=0;
        modes[1].valid=0;
        modes[2].valid=0;
        modes[3].valid=0;
        DFBCHECK (dfb->EnumVideoModes(dfb,enum_modes_callback,NULL));
//    printf("No YUY2 %i\n",no_yuy2);
//  }
  preinit_done=1;
  return 0;

}


static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,
		uint32_t d_height, uint32_t fullscreen, char *title,
		uint32_t format,const vo_tune_info_t *info)
{
  /*
   * (Locals)
   */
	DFBSurfaceDescription dsc;
        DFBResult             ret;


        int vm = fullscreen & 0x02;
	int zoom = fullscreen & 0x04;

	fs = fullscreen & 0x01;
	flip = fullscreen & 0x08;

	pixel_format=format;

	in_width = width;
	in_height = height;

        if (d_width) {
		out_width = d_width;
		out_height = d_height;
	} else {
		d_width = out_width = width;
		d_height = out_height = height;
	}


//        if (!preinit(NULL)) return 1;


  if (vm) {
        // need better algorithm just hack
        if (modes[source_pixel_size-1].valid) dfb->SetVideoMode(dfb,modes[source_pixel_size-1].width,modes[source_pixel_size-1].height,source_pixel_size);
        }


     if (!no_yuy2) {
        // try to set proper w a h values matching image size
        dlc.flags       = DLCONF_WIDTH | DLCONF_HEIGHT;// | DLCONF_OPTIONS;
        dlc.width       = in_width;
        dlc.height      = in_height;

        ret = videolayer->SetConfiguration( videolayer, &dlc );

/*		if (ret) {
	printf("Set layer size failed\n");
	};
*/
        dlc.flags       = DLCONF_PIXELFORMAT;// | DLCONF_OPTIONS;
        //dlc.pixelformat = DSPF_YUY2; should be set by preinit
        //dlc.options     = (vcaps & DVCAPS_INTERLACED) ? DLOP_INTERLACED_VIDEO : 0;
        no_yuy2=1;
/*	switch (dlc.pixelformat) {
                case DSPF_ARGB:  printf("Directfb frame format ARGB\n");
                                 frame_pixel_size = 4;
                                 break;
                case DSPF_RGB32: printf("Directfb frame format RGB32\n");
                                 frame_pixel_size = 4;
                                 break;
                case DSPF_RGB24: printf("Directfb frame format RGB24\n");
                                 frame_pixel_size = 3;
                                 break;
                case DSPF_RGB16: printf("Directfb frame format RGB16\n");
                                 frame_pixel_size = 2;
                                 break;
                case DSPF_RGB15: printf("Directfb frame format RGB15\n");
                                 frame_pixel_size = 2;
                                 break;
                case DSPF_YUY2:  printf("Directfb frame format YUY2\n");
                                 frame_pixel_size = 2;
                                 break;
                case DSPF_UYVY:  printf("Directfb frame format UYVY\n");
                                 frame_pixel_size = 2;
                                 break;
                default: printf("Directfb - unknown format ->exit\n"); return 1;
        }
*/        ret =videolayer->SetConfiguration( videolayer, &dlc );
        if (!ret) {
//             printf("SetConfiguration for layer OK\n");
	     ret = videolayer->GetSurface( videolayer, &primary );
	     if (!ret){
                no_yuy2=0;
//                printf("Get surface for layer OK\n");
/*              dsc.width=in_width;
              dsc.height=in_height;
              dsc.flags = DSDESC_CAPS | DSDESC_HEIGHT | DSDESC_WIDTH;
              dsc.caps  = DSCAPS_VIDEOONLY;//| DSCAPS_FLIPPING;
              primary->SetConfiguration(priamry,&dsc);*/
              };
        };

      }

// mam pro flip pouzit tenhle flip nebo bitblt
  dsc.flags = DSDESC_CAPS | DSDESC_PIXELFORMAT;
  dsc.caps  = DSCAPS_PRIMARY | DSCAPS_VIDEOONLY;//| DSCAPS_FLIPPING;

  switch (format) {
        case IMGFMT_RGB32: dsc.pixelformat =  DSPF_ARGB; source_pixel_size= 4; break;
        case IMGFMT_BGR32: dsc.pixelformat =  DSPF_ARGB; source_pixel_size= 4;  break;
        case IMGFMT_RGB24: dsc.pixelformat =  DSPF_RGB24; source_pixel_size= 3;  break;
        case IMGFMT_BGR24: dsc.pixelformat =  DSPF_RGB24; source_pixel_size= 3;  break;
        case IMGFMT_RGB16: dsc.pixelformat =  DSPF_RGB16; source_pixel_size= 2; break;
        case IMGFMT_BGR16: dsc.pixelformat =  DSPF_RGB16; source_pixel_size= 2; break;
        case IMGFMT_RGB15: dsc.pixelformat =  DSPF_RGB15; source_pixel_size= 2; break;
        case IMGFMT_BGR15: dsc.pixelformat =  DSPF_RGB15; source_pixel_size= 2; break;
        default: dsc.pixelformat =  DSPF_RGB24; source_pixel_size=2; break; //YUV formats
        };



  if (no_yuy2) {
  DFBCHECK (dfb->CreateSurface( dfb, &dsc, &primary ));
  }
  else {//  try to set pos for YUY2 layer and proper aspect ratio

    extern float monitor_aspect;

    float h=(float)out_height,w=(float)out_width/monitor_aspect;
    float aspect=h/w;
//    printf("in out d: %d %d, %d %d, %d %d\n",in_width,in_height,out_width,out_height,d_width,d_height);
//    printf ("Aspect y/x=%f/%f=%f\n",h,w,aspect);

//    if (fs) {
        // scale fullscreen
        if (aspect>1) {
                aspect=w/h;
                ret = videolayer->SetScreenLocation(videolayer,(1-aspect)/2,0,aspect,1);
        } else {
                ret = videolayer->SetScreenLocation(videolayer,0,(1-aspect)/2,1,aspect);
        }
//    } else {
        // beacase I can't get real screen size values I can't scale properly in other way then fullscreen
//    };
//    if (ret) printf("SetScreenLocation failed\n");
  }

  DFBCHECK (primary->GetSize (primary, &screen_width, &screen_height));

  DFBCHECK (primary->GetPixelFormat (primary, &frame_format));

// temporary buffer buffer
  dsc.flags = DSDESC_CAPS | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_WIDTH;
  dsc.caps  = DSCAPS_SYSTEMONLY;

  dsc.width = in_width;
  dsc.height = in_height;

  // at this time use pixel req format or format of main disp
/*
  switch (frame_format) {
                case DSPF_ARGB:  printf("Directfb frame format ARGB\n");
                                 break;
                case DSPF_RGB32: printf("Directfb frame format RGB32\n");
                                 break;
               case DSPF_RGB24: printf("Directfb frame format RGB24\n");
                                 break;
                case DSPF_RGB16:  printf("Directfb frame format RGB16\n");
                                 break;
                case DSPF_RGB15:  printf("Directfb frame format RGB15\n");
                                 break;
                case DSPF_YUY2:  printf("Directfb frame format YUY2\n");
                                 break;
                default: printf("Directfb - unknown format ->exit\n"); return 1;
        }
 */

  switch (format) {
        case IMGFMT_RGB32: dsc.pixelformat =  DSPF_ARGB; break;
        case IMGFMT_BGR32: dsc.pixelformat =  DSPF_ARGB; break;
        case IMGFMT_RGB24: dsc.pixelformat =  DSPF_RGB24; break;
        case IMGFMT_BGR24: dsc.pixelformat =  DSPF_RGB24; break;
        case IMGFMT_RGB16: dsc.pixelformat =  DSPF_RGB16; break;
        case IMGFMT_BGR16: dsc.pixelformat =  DSPF_RGB16; break;
        case IMGFMT_RGB15: dsc.pixelformat =  DSPF_RGB15; break;
        case IMGFMT_BGR15: dsc.pixelformat =  DSPF_RGB15; break;
        default: dsc.pixelformat =  frame_format; break;  // uknown or YUV ->  retain layer format eg. RGB or YUY2
        };


  /*
   * Create a surface based on the description of the source frame
   */
  DFBCHECK (dfb->CreateSurface( dfb, &dsc, &frame));

  DFBCHECK (frame->GetPixelFormat (frame, &frame_format));

  switch (frame_format) {
                case DSPF_ARGB:  //printf("Directfb frame format ARGB\n");
                                 frame_pixel_size = 4;
                                 break;
                case DSPF_RGB32: //printf("Directfb frame format RGB32\n");
                                 frame_pixel_size = 4;
                                 break;
                case DSPF_RGB24: //printf("Directfb frame format RGB24\n");
                                 frame_pixel_size = 3;
                                 break;
                case DSPF_RGB16: //printf("Directfb frame format RGB16\n");
                                 frame_pixel_size = 2;
                                 break;
                case DSPF_RGB15: //printf("Directfb frame format RGB15\n");
                                 frame_pixel_size = 2;
                                 break;
                case DSPF_YUY2:  //printf("Directfb frame format YUY2\n");
                                 frame_pixel_size = 2;
                                 break;
                case DSPF_UYVY:  //printf("Directfb frame format UYVY\n");
                                 frame_pixel_size = 2;
                                 break;
                default: printf("Directfb - unknown format ->exit\n"); return 1;
        }


	if (zoom) {
		printf("-zoom is not yet supported\n");
//		return 1;
	}
/*
        if (fs) {
		printf("-fs can degrade image and performance\n");
//		return 1;
	}
*/
	if ((out_width < in_width || out_height < in_height) && (!fs)) {
		printf("Screensize is smaller than video size !\n");
//		return 1;  // doesn't matter we will
	}

        if ((fs) && (no_yuy2)) {
                // aspect ratio correction for fullscreen
                out_height=d_height*screen_width/d_width;
                if (out_height <= screen_height) {out_width=screen_width;} else {out_width=screen_width*screen_height/d_height; out_height=screen_height;};
//                printf("Going fullscreen !\n");
	}


  /*
   * (Get keyboard)
   */
  DFBCHECK (dfb->GetInputDevice (dfb, DIDID_KEYBOARD, &keyboard));

  /*
   * Create an input buffer for the keyboard.
   */
#ifdef HAVE_DIRECTFB099
  DFBCHECK (keyboard->CreateEventBuffer (DICAPS_ALL, &buffer));
#else
  DFBCHECK (keyboard->CreateInputBuffer (keyboard, &buffer));
#endif

// yuv2rgb transform init

 if (((format == IMGFMT_YV12) || (format == IMGFMT_YUY2)) && no_yuy2){ yuv2rgb_init(frame_pixel_size * 8,MODE_RGB);};

 if (((out_width != in_width) || (out_height != in_height)) && (no_yuy2)) {stretch = 1;} else stretch=0; //yuy doesn't like strech and should not be needed

 if (no_yuy2) {
        xoffset = (screen_width - out_width) / 2;
        yoffset = (screen_height - out_height) / 2;
        } else {
        xoffset = 0;
        yoffset = 0;
        }
// printf("in out d: %d %d, %d %d, %d %d\n",in_width,in_height,out_width,out_height,d_width,d_height);

return 0;
}

static uint32_t query_format(uint32_t format)
{
	int ret = 0x4; /* osd/sub is supported on every bpp */

//        preinit(NULL);

//        printf("Format query: %s\n",vo_format_name(format));
	switch (format) {

// RGB mode works only if color depth is same as on screen and this driver doesn't know before init
// so we couldn't report supported formats well

// Just support those detected by preinit
                case IMGFMT_RGB32:
                case IMGFMT_BGR32: if (modes[3].valid) return ret|0x2;
                                   break;
                case IMGFMT_RGB24:
                case IMGFMT_BGR24: if (modes[2].valid) return ret|0x2;
                                   break;
                case IMGFMT_RGB16:
                case IMGFMT_BGR16:
                case IMGFMT_RGB15:
                case IMGFMT_BGR15: if (modes[1].valid) return ret|0x2;
                                   break;
                case IMGFMT_YUY2: if (!no_yuy2) return ret|0x2;
                                   break;
        	case IMGFMT_YV12: if (!no_yuy2) return ret|0x2; else return ret|0x1;
                                   break;
  // YV12 should work in all cases
 	}

	return 0;
}

static const vo_info_t *get_info(void)
{
	return &vo_info;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
        void *dst;
        int pitch;
	int len;

        DFBCHECK (frame->Lock(frame,DSLF_WRITE,&dst,&pitch));

	switch(frame_format) {
                case DSPF_RGB32:
                case DSPF_ARGB:
                        vo_draw_alpha_rgb32(w,h,src,srca,stride,((uint8_t *) dst)+pitch*y0 + frame_pixel_size*x0,pitch);
                        break;

                case DSPF_RGB24:
                        vo_draw_alpha_rgb24(w,h,src,srca,stride,((uint8_t *) dst)+pitch*y0 + frame_pixel_size*x0,pitch);
                        break;

                case DSPF_RGB16:
                        vo_draw_alpha_rgb16(w,h,src,srca,stride,((uint8_t *) dst)+pitch*y0 + frame_pixel_size*x0,pitch);
                        break;

                case DSPF_RGB15:
                        vo_draw_alpha_rgb15(w,h,src,srca,stride,((uint8_t *) dst)+pitch*y0 + frame_pixel_size*x0,pitch);
                        break;

		case DSPF_YUY2:
    			vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) dst) + pitch*y0 + frame_pixel_size*x0,pitch);
		break;

        	case DSPF_UYVY:
    			vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) dst) + pitch*y0 + frame_pixel_size*x0 + 1,pitch);
		break;
		}
        DFBCHECK (frame->Unlock(frame));
}

static uint32_t draw_frame(uint8_t *src[])
{
        void *dst;
        int pitch;
	int len;

        DFBCHECK (frame->Lock(frame,DSLF_WRITE,&dst,&pitch));

        switch (frame_format) {
                case DSPF_ARGB:
                case DSPF_RGB32:
                case DSPF_RGB24:
                case DSPF_RGB16:
                case DSPF_RGB15:switch (pixel_format) {
                                case IMGFMT_YV12:
                                        yuv2rgb(dst,src[0],src[1],src[2],in_width,in_height,pitch,in_width,in_width/2);
                                        break;
                                /* how to handle this? need conversion from YUY2 to RGB*/
/*                                case IMGFMT_YUY2:
                                        yuv2rgb(dst,src[0],src[0]+1,src[0]+3,1,in_height*in_width/2,frame_pixel_size*2,4,4); //odd pixels
                                        yuv2rgb(dst+1,src[0]+2,src[0]+1,src[0]+3,1,in_height*in_width/2,frame_pixel_size*2,4,4); //even pixels
                                        break;*/
				// RGB - just copy
                                default:    if (source_pixel_size==frame_pixel_size) {memcpy(dst,src[0],in_width * in_height * frame_pixel_size);};

                                }
                        break;
                case DSPF_YUY2:
                        switch (pixel_format) {
                        	case IMGFMT_YV12:   yv12toyuy2(src[0],src[1],src[2],dst,in_width,in_height,in_width,in_width >>1,pitch);
		                        	    break;
	                        case IMGFMT_YUY2: {
                                                int i;
                                                for (i=0;i<in_height;i++) {
                                                        memcpy(dst+i*pitch,src[0]+i*in_width*2,in_width*2);
                                                         }
                                                }
                                                /*len = in_width * in_height * pitch;
			                        memcpy(dst,src[0],len);*/
		        	                break;
                                // hopefully there will be no RGB in this case otherwise convert - not implemented
	                                }
                        break;
        }
        DFBCHECK (frame->Unlock(frame));
        return 0;
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h, int x, int y)
{

        int err;
        void *dst;
	uint8_t *s;
        int pitch;
	int i;

        err = frame->Lock(frame,DSLF_WRITE,&dst,&pitch);

//        printf("Drawslice\n");

        switch (frame_format) {
                case DSPF_ARGB:
                case DSPF_RGB32:
                case DSPF_RGB24:
                case DSPF_RGB16:
                case DSPF_RGB15:
                        switch (pixel_format) {
                                case IMGFMT_YV12:
                                        yuv2rgb(dst+ y * pitch + frame_pixel_size*x ,src[0],src[1],src[2],w,h,pitch,stride[0],stride[1]);
                                        break;
                                // how to handle this? need conversion
/*                                case IMGFMT_YUY2: {
                                        int i;
                                        for (i=0;i<=h;i++) {
                                                yuv2rgb(dst+ (i+y) * pitch + frame_pixel_size*x,src[0]+i*stride[0],src[0]+1+i*stride[0],src[0]+3+i*stride[0],1,(w+1)/2,frame_pixel_size*2,4,4); //odd pixels
                                                yuv2rgb(dst+ (i+y) * pitch + frame_pixel_size*x +1,src[0]+i*stride[0]+2,src[0]+1+i*stride[0],src[0]+3+i*stride[0],1,(w)/2,frame_pixel_size*2,4,4); //odd pixels
                                                };
                                        };
                                        break;
  */
                                default:    if (source_pixel_size==frame_pixel_size) {
                                                        dst += x * frame_pixel_size;
				                        s = src[0];
				                        for (i=y;i<=(y+h);i++) {
					                        memcpy(dst,s,w);
					                        dst += (pitch);
					                        s += stride[0];
					                        };
                                               }
				            break;

                                }
                        break;
                case DSPF_YUY2:
                	switch (pixel_format) {
	                        case IMGFMT_YV12:   yv12toyuy2(src[0],src[1],src[2],dst + pitch*y + frame_pixel_size*x ,w,h,stride[0],stride[1],pitch);
                 				break;
	                        case IMGFMT_YUY2:   {
				                        dst += x * frame_pixel_size;
				                        s = src[0];
				                        for (i=y;i<=(y+h);i++) {
					                        memcpy(dst,s,w);
					                        dst += (pitch);
					                        s += stride[0];
					                        };
				                        };
                        				break;
                                // hopefully there will be no RGB in this case otherwise convert - not implemented
                        	}
                         break;
        };

        frame->Unlock(frame);

	return 0;
}

extern void mplayer_put_key(int code);

#include "../linux/keycodes.h"

static void check_events(void)
{

      DFBInputEvent event;
#ifdef HAVE_DIRECTFB099
if (buffer->GetEvent (buffer, DFB_EVENT(&event)) == DFB_OK) {
#else
if (buffer->GetEvent (buffer, &event) == DFB_OK) {
#endif
     if (event.type == DIET_KEYPRESS) { 
    		switch (event.keycode) {
                                case DIKC_ESCAPE:
					mplayer_put_key('q');
				break;
                                case DIKC_KP_PLUS: mplayer_put_key('+');break;
                                case DIKC_KP_MINUS: mplayer_put_key('-');break;
				case DIKC_TAB: mplayer_put_key('\t');break;
				case DIKC_PAGEUP: mplayer_put_key(KEY_PAGE_UP);break;
				case DIKC_PAGEDOWN: mplayer_put_key(KEY_PAGE_DOWN);break;
                                case DIKC_UP: mplayer_put_key(KEY_UP);break;
                                case DIKC_DOWN: mplayer_put_key(KEY_DOWN);break;
                                case DIKC_LEFT: mplayer_put_key(KEY_LEFT);break;
                                case DIKC_RIGHT: mplayer_put_key(KEY_RIGHT);break;
                                case DIKC_ASTERISK:
				case DIKC_KP_MULT:mplayer_put_key('*');break;
                                case DIKC_KP_DIV: mplayer_put_key('/');break;
				case DIKC_INSERT: mplayer_put_key(KEY_INSERT);break;
				case DIKC_DELETE: mplayer_put_key(KEY_DELETE);break;
				case DIKC_HOME: mplayer_put_key(KEY_HOME);break;
				case DIKC_END: mplayer_put_key(KEY_END);break;

                default:mplayer_put_key(event.key_ascii);
                };
	};
    };
// empty buffer, because of repeating (keyboard repeat is faster than key handling
// and this causes problems during seek)
// temporary workabout should be solved in the future

buffer->Reset(buffer);

}

static void draw_osd(void)
{
	vo_draw_text(in_width, in_height, draw_alpha);
}

static void flip_page(void)
{
	DFBSurfaceBlittingFlags flags=DSBLIT_NOFX;

	DFBCHECK (primary->SetBlittingFlags(primary,flags));

        if (stretch) {
        	DFBRectangle rect;
        	rect.x=xoffset;
	        rect.y=yoffset;
	        rect.w=out_width;
	        rect.h=out_height;

                DFBCHECK (primary->StretchBlit(primary,frame,NULL,&rect));
                }
        else    {
                DFBCHECK (primary->Blit(primary,frame,NULL,xoffset,yoffset));
                };
//      DFBCHECK (primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC));
}

static void uninit(void)
{
	if (verbose > 0)
		printf("uninit\n");

  /*
   * (Release)
   */
//  printf("Release buffer\n");
  buffer->Release (buffer);
//  printf("Release keyb\n");
  keyboard->Release (keyboard);
//  printf("Release frame\n");
  frame->Release (frame);

// we will not release dfb and layer because there could be a new film

//  printf("Release primary\n");
  primary->Release (primary);
//  switch off BES
  if (videolayer) videolayer->SetOpacity(videolayer,0);
//  printf("Release videolayer\n");
//  if (videolayer) videolayer->Release(videolayer);
//  printf("Release dfb\n");
//  dfb->Release (dfb);
//  preinit_done=0;

}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
