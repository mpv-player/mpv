/*
   MPlayer video driver for DirectFB / Matrox G400

   Copyright (C) 2002 Ville Syrjala <syrjala@sci.fi>

   Originally based on vo_directfb.c by
   Jiri Svoboda <Jiri.Svoboda@seznam.cz>

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

/* directfb includes */
#include <directfb.h>

/* other things */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
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

#include "aspect.h"

#ifndef min
#define min(x,y) (((x)<(y))?(x):(y))
#endif

LIBVO_EXTERN(dfbmga)

static vo_info_t vo_info = {
     "DirectFB / Matrox G400",
     "dfbmga",
     "Ville Syrjala <syrjala@sci.fi>",
     ""
};

extern int verbose;

/******************************
*	   directfb 	      *
******************************/

/*
 * (Globals)
 */
static IDirectFB *dfb;

static IDirectFBDisplayLayer *bes;
static IDirectFBDisplayLayer *crtc2;
static IDirectFBDisplayLayer *spic;

static IDirectFBSurface *frame;
static IDirectFBSurface *c2frame;
static IDirectFBSurface *subframe;

static DFBSurfacePixelFormat frame_format;
static DFBSurfacePixelFormat subframe_format;

static DFBRectangle drect;

static IDirectFBInputDevice  *keyboard;
static IDirectFBEventBuffer  *buffer;

static unsigned int frame_pixel_size;
static unsigned int subframe_pixel_size;

static int inited = 0;

static int stretch = 0;

static int use_bes   = 0;
static int use_crtc2 = 1;
static int use_spic  = 1;

/******************************
*	    vo_directfb       *
******************************/

/* command line/config file options */
#ifdef HAVE_FBDEV
 extern char *fb_dev_name;
#else
 char *fb_dev_name;
#endif

static void ( *draw_alpha_p ) ( int w, int h, unsigned char *src,
                                unsigned char *srca, int stride,
                                unsigned char *dst, int dstride);

static uint32_t in_width;
static uint32_t in_height;
static uint32_t screen_width;
static uint32_t screen_height;

static char *
pixelformat_name( DFBSurfacePixelFormat format )
{
     switch(format) {
     case DSPF_ARGB:
          return "ARGB";
     case DSPF_RGB32:
          return "RGB32";
     case DSPF_RGB24:
	  return "RGB24";
     case DSPF_RGB16:
	  return "RGB16";
     case DSPF_RGB15:
	  return "RGB15";
     case DSPF_YUY2:
	  return "YUY2";
     case DSPF_UYVY:
	  return "UYVY";
     case DSPF_YV12:
	  return "YV12";
     case DSPF_I420:
	  return "I420";
     case DSPF_LUT8:
	  return "LUT8";
     default:
	  return "Unknown pixel format";
     }
}

static DFBSurfacePixelFormat
imgfmt_to_pixelformat( uint32_t format )
{
     switch (format) {
     case IMGFMT_RGB32:
     case IMGFMT_BGR32:
	  return DSPF_ARGB;
     case IMGFMT_RGB24:
     case IMGFMT_BGR24:
	  return DSPF_RGB24;
     case IMGFMT_RGB16:
     case IMGFMT_BGR16:
	  return DSPF_RGB16;
     case IMGFMT_RGB15:
     case IMGFMT_BGR15:
	  return DSPF_RGB15;
     case IMGFMT_YUY2:
	  return DSPF_YUY2;
     case IMGFMT_UYVY:
	  return DSPF_UYVY;
     case IMGFMT_YV12:
	  return DSPF_YV12;
     case IMGFMT_I420:
     case IMGFMT_IYUV:
          return DSPF_I420;
     default:
	  return DSPF_UNKNOWN;
     }
}

static uint32_t
preinit( const char *arg )
{
     DFBSurfaceDescription dsc;

     if (vo_subdevice) {
          while (*vo_subdevice != '\0') {
               if (!strncmp(vo_subdevice, "bes", 3)) {
                    use_bes = 1;
                    vo_subdevice += 3;
               } else if (!strncmp(vo_subdevice, "nocrtc2", 7)) {
                    use_crtc2 = 0;
                    vo_subdevice += 7;
               } else if (!strncmp(vo_subdevice, "nospic", 6)) {
                    use_spic = 0;
                    vo_subdevice += 6;
               } else
                    vo_subdevice++;
          }
     }
     if (!use_bes && !use_crtc2) {
	  fprintf( stderr, "vo_dfbmga: No output selected\n" );
          return -1;
     }

     if (!inited) {
          DirectFBInit( NULL, NULL );

          if (!fb_dev_name && !(fb_dev_name = getenv( "FRAMEBUFFER" )))
               fb_dev_name = "/dev/fb0";
          DirectFBSetOption( "fbdev", fb_dev_name );
          DirectFBSetOption( "no-cursor", "" );
          DirectFBSetOption( "bg-color", "00000000" );

          DirectFBCreate( &dfb );

          inited = 1;
     }

     if (use_bes) {
          dfb->GetDisplayLayer( dfb, 1, &bes );
          bes->SetCooperativeLevel( bes, DLSCL_EXCLUSIVE );
          bes->SetOpacity( bes, 0 );
     }

     if (use_crtc2) {
          dfb->GetDisplayLayer( dfb, 2, &crtc2 );
          crtc2->SetCooperativeLevel( crtc2, DLSCL_EXCLUSIVE );
          crtc2->SetOpacity( crtc2, 0 );
     }

     dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard );
     keyboard->CreateEventBuffer( keyboard, &buffer );
     buffer->Reset( buffer );

     return 0;
}

static uint32_t
config( uint32_t width, uint32_t height,
        uint32_t d_width, uint32_t d_height,
        uint32_t fullscreen,
        char *title,
	uint32_t format )
{
     DFBDisplayLayerConfig      dlc;
     DFBDisplayLayerConfigFlags failed;
     DFBSurfaceDescription      dsc;
     DFBResult                  ret;

     uint32_t out_width;
     uint32_t out_height;

     in_width  = width;
     in_height = height;

     aspect_save_orig(width, height);
     aspect_save_prescale(d_width, d_height);

     dlc.pixelformat   = imgfmt_to_pixelformat( format );

     if (use_bes) {
          /* Draw to BES surface */
          dlc.flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
          dlc.width       = in_width;
          dlc.height      = in_height;
          dlc.buffermode  = DLBM_BACKVIDEO;

          if (bes->TestConfiguration( bes, &dlc, &failed ) != DFB_OK)
               return -1;
          bes->SetConfiguration( bes, &dlc );
          bes->GetSurface( bes, &frame );

          aspect_save_screenres( 10000, 10000 );
          aspect( &out_width, &out_height, A_ZOOM );
          bes->SetScreenLocation( bes,
                                  (1.0f - (float) out_width  / 10000.0f) / 2.0f,
                                  (1.0f - (float) out_height / 10000.0f) / 2.0f,
                                  (float) out_width  / 10000.0f,
                                  (float) out_height / 10000.0f );
     } else {
          /* Draw to a temporary surface */
          DFBSurfaceDescription dsc;

          dsc.flags       = DSDESC_CAPS |
                            DSDESC_WIDTH | DSDESC_HEIGHT |
                            DSDESC_PIXELFORMAT;
          dsc.caps        = DSCAPS_VIDEOONLY;
          dsc.width       = in_width;
          dsc.height      = in_height;
          dsc.pixelformat = dlc.pixelformat;

          dfb->CreateSurface( dfb, &dsc, &frame );
     }

     if (use_crtc2) {
          dlc.flags      = DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
          dlc.buffermode = DLBM_BACKVIDEO;

          switch (dlc.pixelformat) {
          case DSPF_I420:
          case DSPF_YV12:
               /* sub-picture supported */
               break;

          case DSPF_YUY2:
          case DSPF_UYVY:
               /* Blit to YUY2/UYVY not supported */
               dlc.pixelformat = DSPF_ARGB;

               /* fall through */
          default:
               /* sub-picture not supported */
               use_spic = 0;
          }

          if (crtc2->TestConfiguration( crtc2, &dlc, &failed ) != DFB_OK)
               return -1;
          crtc2->SetConfiguration( crtc2, &dlc );
          crtc2->GetSurface( crtc2, &c2frame );

          c2frame->GetSize( c2frame, &screen_width, &screen_height );

          /* Don't stretch only slightly smaller videos */
          if ((in_width > (0.95 * screen_width)) &&
              (in_width < screen_width))
               out_width = in_width;
          else
               out_width = screen_width;
          if ((in_height > (0.95 * screen_height)) &&
              (in_height < screen_height))
               out_height = in_height;
          else
               out_height = screen_height;

          aspect_save_screenres( out_width, out_height );
          aspect( &out_width, &out_height, (fullscreen & 0x01) ? A_ZOOM : A_NOZOOM );

          if (in_width != out_width ||
              in_height != out_height)
               stretch = 1;
          else
               stretch = 0;

          drect.x = (screen_width  - out_width)  / 2;
          drect.y = (screen_height - out_height) / 2;
          drect.w = out_width;
          drect.h = out_height;

          c2frame->Clear( c2frame, 0, 0, 0, 0 );
          c2frame->Flip( c2frame, NULL, 0 );
          c2frame->Clear( c2frame, 0, 0, 0, 0 );

          printf( "vo_dfbmga: CRTC2 surface %dx%d %s\n", dlc.width, dlc.height, pixelformat_name( dlc.pixelformat ) );
     } else
          use_spic = 0;

     frame->GetPixelFormat( frame, &frame_format );
     frame_pixel_size = DFB_BYTES_PER_PIXEL( frame_format );
     printf( "vo_dfbmga: Video surface %dx%d %s (%s)\n",
             in_width, in_height,
             pixelformat_name( frame_format ),
             use_bes ? "BES" : "offscreen" );

     if (use_spic) {
          /* Draw OSD to sub-picture surface */
          IDirectFBPalette *palette;
          DFBColor          color;
          int               i;

          dfb->GetDisplayLayer( dfb, 3, &spic );
          spic->SetCooperativeLevel( spic, DLSCL_EXCLUSIVE );
          spic->SetOpacity( spic, 0 );

          dlc.flags       = DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
          dlc.pixelformat = DSPF_LUT8;
          dlc.buffermode  = DLBM_BACKVIDEO;
          if (spic->TestConfiguration( spic, &dlc, &failed ) != DFB_OK)
               return -1;
          spic->SetConfiguration( spic, &dlc );

          spic->GetSurface( spic, &subframe );

          subframe->GetPalette( subframe, &palette );
          color.a = 0;
          for (i = 0; i < 16; i++) {
               color.r = i * 17;
               color.g = i * 17;
               color.b = i * 17;
               palette->SetEntries( palette, &color, 1, i );
          }
          palette->Release( palette );

          subframe->Clear( subframe, 0, 0, 0, 0 );
          subframe->Flip( subframe, NULL, 0 );
          subframe->Clear( subframe, 0, 0, 0, 0 );
     } else if (use_crtc2) {
          /* Draw OSD to CRTC2 surface */
          subframe = c2frame;
     } else {
          /* Draw OSD to BES surface */
          subframe            = frame;
          screen_width        = in_width;
          screen_height       = in_height;
     }

     subframe->GetPixelFormat( subframe, &subframe_format );
     subframe_pixel_size = DFB_BYTES_PER_PIXEL( subframe_format );
     printf( "vo_dfbmga: Sub-picture surface %dx%d %s (%s)\n",
             screen_width, screen_height,
             pixelformat_name( subframe_format ),
             use_crtc2 ? (use_spic ? "Sub-picture layer" : "CRTC2") : "BES" );

     /* Display all needed layers */
     if (use_bes)
          bes->SetOpacity( bes, 0xFF );
     if (use_crtc2)
          crtc2->SetOpacity( crtc2, 0xFF );
     if (use_spic)
          spic->SetOpacity( spic, 0xFF );

     return 0;
}

static uint32_t
query_format( uint32_t format )
{
     switch (format) {
          case IMGFMT_RGB32:
          case IMGFMT_BGR32:
          case IMGFMT_RGB24:
          case IMGFMT_BGR24:
          case IMGFMT_RGB16:
          case IMGFMT_BGR16:
          case IMGFMT_RGB15:
          case IMGFMT_BGR15:
          case IMGFMT_YUY2:
          case IMGFMT_UYVY:
          case IMGFMT_YV12:
          case IMGFMT_I420:
          case IMGFMT_IYUV:
               return (VFCAP_HWSCALE_UP |
                       VFCAP_HWSCALE_DOWN |
                       VFCAP_CSP_SUPPORTED_BY_HW |
                       VFCAP_CSP_SUPPORTED |
                       VFCAP_OSD);
     }

     return 0;
}

static const vo_info_t *get_info( void )
{
	return &vo_info;
}

static void vo_draw_alpha_lut8( int w, int h,
                                unsigned char* src,
                                unsigned char *srca,
                                int srcstride,
                                unsigned char* dst,
                                int dststride )
{
     int x;

     while (h--) {
          for (x = 0; x < w; x++) {
               if (srca[x])
                    dst[x] |= ((255 - srca[x]) & 0xF0) | (src[x] >> 4);
          }
          src  += srcstride;
          srca += srcstride;
          dst  += dststride;
     }
}

static void
draw_alpha( int x0, int y0,
            int w, int h,
            unsigned char *src,
	    unsigned char *srca,
            int stride )
{
     void *dst;
     int pitch;

     subframe->Lock( subframe, DSLF_WRITE, &dst, &pitch );

     switch (subframe_format) {
     case DSPF_LUT8:
          vo_draw_alpha_lut8( w, h, src, srca, stride,
                              ((uint8_t *) dst) + pitch * y0 + subframe_pixel_size * x0,
                              pitch );
          break;
     case DSPF_RGB32:
     case DSPF_ARGB:
	  vo_draw_alpha_rgb32( w, h, src, srca, stride,
			       (( uint8_t *) dst) + pitch * y0 + subframe_pixel_size * x0,
                               pitch );
	  break;
     case DSPF_RGB24:
	  vo_draw_alpha_rgb24( w, h, src, srca, stride,
			       ((uint8_t *) dst) + pitch * y0 + subframe_pixel_size * x0,
                               pitch );
	  break;
     case DSPF_RGB16:
	  vo_draw_alpha_rgb16( w, h, src, srca, stride,
			       ((uint8_t *) dst) + pitch * y0 + subframe_pixel_size * x0,
                               pitch );
	  break;
     case DSPF_RGB15:
	  vo_draw_alpha_rgb15( w, h, src, srca, stride,
			       ((uint8_t *) dst) + pitch * y0 + subframe_pixel_size * x0,
                               pitch );
	  break;
     case DSPF_YUY2:
	  vo_draw_alpha_yuy2( w, h, src, srca, stride,
			      ((uint8_t *) dst) + pitch * y0 + subframe_pixel_size * x0,
                              pitch );
	  break;
     case DSPF_UYVY:
	  vo_draw_alpha_yuy2( w, h, src, srca, stride,
			      ((uint8_t *) dst) + pitch * y0 + subframe_pixel_size * x0 + 1,
                              pitch );
	  break;
     case DSPF_I420:
     case DSPF_YV12:
	  vo_draw_alpha_yv12( w, h, src, srca, stride,
			      ((uint8_t *) dst) + pitch * y0 + subframe_pixel_size * x0,
                              pitch );
	  break;
     }

     subframe->Unlock( subframe );
}

static uint32_t
draw_frame( uint8_t * src[] )
{
     void *dst;
     int pitch;

     frame->Lock( frame, DSLF_WRITE, &dst, &pitch );

     switch (frame_format) {
     case DSPF_ARGB:
     case DSPF_RGB32:
     case DSPF_RGB24:
     case DSPF_RGB16:
     case DSPF_RGB15:
     case DSPF_YUY2:
     case DSPF_UYVY:
          {
               int i;
               for (i = 0; i < in_height; i++) {
                    memcpy( dst + i * pitch,
                            src[0] + i * in_width * frame_pixel_size,
                            in_width * frame_pixel_size );
               }
          }
          break;
     case DSPF_YV12:
          {
               int i;
               for (i = 0; i < in_height; i++) {
                    memcpy( dst + i * pitch,
                            src[0] + i * in_width,
                            in_width );
               }
               dst += pitch * in_height;
               for (i = 0; i < in_height / 2; i++) {
                    memcpy( dst + i * pitch / 2,
                            src[2] + i * in_width / 2,
                            in_width / 2 );
               }
               dst += pitch * in_height / 4;
               for (i = 0; i < in_height / 2; i++) {
                    memcpy( dst + i * pitch / 2,
                            src[1] + i * in_width / 2,
                            in_width / 2 );
               }
          }
          break;
     case DSPF_I420:
          {
               int i;
               for (i = 0; i < in_height; i++) {
                    memcpy( dst + i * pitch,
                            src[0] + i * in_width,
                            in_width );
               }
               dst += pitch * in_height;
               for (i = 0; i < in_height / 2; i++) {
                    memcpy( dst + i * pitch / 2,
                            src[1] + i * in_width / 2,
                            in_width / 2 );
               }
               dst += pitch * in_height / 4;
               for (i = 0; i < in_height / 2; i++) {
                    memcpy( dst + i * pitch / 2,
                            src[2] + i * in_width / 2,
                            in_width / 2 );
               }
          }
          break;
     }

     frame->Unlock( frame );

     return 0;
}

static uint32_t
draw_slice( uint8_t * src[], int stride[], int w, int h, int x, int y )
{
     void *dst;
     int pitch;

     frame->Lock( frame, DSLF_WRITE, &dst, &pitch );

     switch (frame_format) {
     case DSPF_ARGB:
     case DSPF_RGB32:
     case DSPF_RGB24:
     case DSPF_RGB16:
     case DSPF_RGB15:
     case DSPF_YUY2:
     case DSPF_UYVY:
          {
               void *s;
               int i;

               dst += y * pitch + x * frame_pixel_size;
               s = src[0];
               for (i = 0; i < h; i++) {
                    memcpy( dst, s, w );
                    dst += pitch;
                    s += stride[0];
               }
          }
          break;
     case DSPF_YV12:
          {
               void *d, *s;
               int i;
               d = dst + pitch * y + x;
               s = src[0];
               for (i = 0; i < h; i++) {
                    memcpy( d, s, w );
                    d += pitch;
                    s += stride[0];
               }
               d = dst + pitch * in_height + pitch * y / 4 + x / 2;
               s = src[2];
               for (i = 0; i < h / 2; i++) {
                    memcpy( d, s, w / 2 );
                    d += pitch / 2;
                    s += stride[2];
               }
               d = dst + pitch * in_height + pitch * in_height / 4 +
                    pitch * y / 4 + x / 2;
               s = src[1];
               for (i = 0; i < h / 2; i++) {
                    memcpy( d, s, w / 2 );
                    d += pitch / 2;
                    s += stride[1];
               }
          }
	  break;
     case DSPF_I420:
          {
               void *d, *s;
               int i;
               d = dst + pitch * y + x;
               s = src[0];
               for (i = 0; i < h; i++) {
                    memcpy( d, s, w );
                    d += pitch;
                    s += stride[0];
               }
               d = dst + pitch * in_height + pitch * y / 4 + x / 2;
               s = src[1];
               for (i = 0; i < h / 2; i++) {
                    memcpy( d, s, w / 2 );
                    d += pitch / 2;
                    s += stride[1];
               }
               d = dst + pitch * in_height + pitch * in_height / 4 +
                    pitch * y / 4 + x / 2;
               s = src[2];
               for (i = 0; i < h / 2; i++) {
                    memcpy( d, s, w / 2 );
                    d += pitch / 2;
                    s += stride[2];
               }
          }
	  break;
     }

     frame->Unlock( frame );

     return 0;
}

static void
draw_osd( void )
{
     if (use_spic)
          subframe->Clear( subframe, 0, 0, 0, 0 );
     else if (!use_crtc2) {
          /* Clear black bars around the picture */
          c2frame->SetColor( c2frame, 0, 0, 0, 0 );
          c2frame->FillRectangle( c2frame,
                                  0, 0,
                                  drect.x, drect.y + drect.h );
          c2frame->FillRectangle( c2frame,
                                  0, drect.y + drect.h,
                                  drect.x + drect.w, drect.y );
          c2frame->FillRectangle( c2frame,
                                  drect.x, 0,
                                  drect.x + drect.w, drect.y );
          c2frame->FillRectangle( c2frame,
                                  drect.x + drect.w, drect.y,
                                  drect.x, drect.y + drect.h );
     }

     vo_draw_text( screen_width, screen_height, draw_alpha );

     subframe->Flip( subframe, NULL, DSFLIP_WAITFORSYNC );
}

static void
flip_page( void )
{
     /* Flip is done by draw_osd() when only BES is used */
     if (!use_crtc2)
          return;

     if (use_bes)
          /* Flip BES */
          frame->Flip( frame, NULL, 0 );

     /* Blit from BES/temp to CRTC2 */
     c2frame->SetBlittingFlags( c2frame, DSBLIT_NOFX );
     if (stretch)
          c2frame->StretchBlit( c2frame, frame, NULL, &drect );
     else
          c2frame->Blit( c2frame, frame, NULL, drect.x, drect.y );

     if (use_spic)
          /* Flip CRTC2 */
          c2frame->Flip( c2frame, NULL, DSFLIP_WAITFORSYNC );
}

static void
uninit( void )
{
     buffer->Release( buffer );
     keyboard->Release( keyboard );

     frame->Release( frame );
     if (use_bes) {
          bes->SetOpacity( bes, 0 );
          bes->Release( bes );
     }
     if (use_crtc2) {
          c2frame->Release( c2frame );
          crtc2->SetOpacity( crtc2, 0 );
          crtc2->Release( crtc2 );
     }
     if (use_spic) {
          subframe->Release( subframe );
          spic->SetOpacity( spic, 0 );
          spic->Release( spic );
     }

     /*
      * Don't release. Segfault in preinit() if
      * DirectFBCreate() called more than once.
      *
      * dfb->Release( dfb );
      */
}

#if 0
static int
directfb_set_video_eq( const vidix_video_eq_t * info )
{
     DFBColorAdjustment ca;
     float factor = (float) 0xffff / 2000.0;

     ca.flags = DCAF_NONE;

     if (info->cap & VEQ_CAP_BRIGHTNESS) {
          ca.flags      |= DCAF_BRIGHTNESS;
          ca.brightness  = info->brightness * factor + 0x8000;
     }
     if (info->cap & VEQ_CAP_CONTRAST) {
          ca.flags    |= DCAF_CONTRAST;
          ca.contrast  = info->contrast * factor + 0x8000;
     }
     if (info->cap & VEQ_CAP_HUE) {
          ca.flags |= DCAF_HUE;
          ca.hue    = info->hue * factor + 0x8000;
     }
     if (info->cap & VEQ_CAP_SATURATION) {
          ca.flags      |= DCAF_SATURATION;
          ca.saturation  = info->saturation * factor + 0x8000;
     }

     /* Prefer CRTC2 over BES */
     if (use_crtc2)
          crtc2->SetColorAdjustment( crtc2, &ca );
     else if (use_bes)
          bes->SetColorAdjustment( bes, &ca );

     return 0;
}

static int
directfb_get_video_eq( vidix_video_eq_t * info )
{
     DFBColorAdjustment ca;
     float factor = 2000.0 / (float) 0xffff;

     /* Prefer CRTC2 over BES */
     if (use_crtc2)
          crtc2->GetColorAdjustment( crtc2, &ca );
     else if (use_bes)
          bes->GetColorAdjustment( bes, &ca );
     else
          return 0;

     if (ca.flags & DCAF_BRIGHTNESS) {
          info->cap        |= VEQ_CAP_BRIGHTNESS;
          info->brightness  = (ca.brightness - 0x8000) * factor;
     }
     if (ca.flags & DCAF_CONTRAST) {
          info->cap      |= VEQ_CAP_CONTRAST;
          info->contrast  = (ca.contrast - 0x8000) * factor;
     }
     if (ca.flags & DCAF_HUE) {
          info->cap |= VEQ_CAP_HUE;
          info->hue  = (ca.hue - 0x8000) * factor;
     }
     if (ca.flags & DCAF_SATURATION) {
          info->cap        |= VEQ_CAP_SATURATION;
          info->saturation  = (ca.saturation - 0x8000) * factor;
     }

     return 0;
}
#endif

static uint32_t
control( uint32_t request, void *data, ... )
{
     switch (request) {
     case VOCTRL_QUERY_FORMAT:
	  return query_format( *((uint32_t *) data) );
#if 0
     case VOCTRL_SET_EQUALIZER:
          {
               va_list ap;
               int value;
               vidix_video_eq_t info;

               va_start( ap, data );
               value = va_arg( ap, int );
               va_end( ap );

               if (!strcasecmp( data, "brightness" )) {
                    info.cap = VEQ_CAP_BRIGHTNESS;
                    info.brightness = value * 10;
               }
               if (!strcasecmp( data, "contrast" )) {
                    info.cap = VEQ_CAP_CONTRAST;
                    info.contrast = value * 10;
               }
               if (!strcasecmp( data, "saturation" )) {
                    info.cap = VEQ_CAP_SATURATION;
                    info.saturation = value * 10;
               }
               if (!strcasecmp( data, "hue" )) {
                    info.cap = VEQ_CAP_HUE;
                    info.hue = value * 10;
               }
               if (directfb_set_video_eq( &info ))
                    return VO_FALSE;

               return VO_TRUE;
          }
     case VOCTRL_GET_EQUALIZER:
          {
               va_list ap;
               int *value;
               vidix_video_eq_t info;

               if (directfb_get_video_eq( &info ))
                    return VO_FALSE;

               va_start( ap, data );
               value = va_arg( ap, int* );
               va_end( ap );

               if (!strcasecmp( data, "brightness" ))
                    if (info.cap & VEQ_CAP_BRIGHTNESS)
                         *value = info.brightness / 10;
               if (!strcasecmp( data, "contrast" ))
                    if (info.cap & VEQ_CAP_CONTRAST)
                         *value = info.contrast / 10;
               if (!strcasecmp( data, "saturation" ))
                    if (info.cap & VEQ_CAP_SATURATION)
                         *value = info.saturation / 10;
               if (!strcasecmp( data, "hue" ))
                    if (info.cap & VEQ_CAP_HUE)
                         *value = info.hue / 10;

               return VO_TRUE;
          }
#endif
     }
     return VO_NOTIMPL;
}

extern void mplayer_put_key( int code );

#include "../linux/keycodes.h"

static void
check_events( void )
{
     static int opa = 255;
     DFBInputEvent event;

     if (buffer->GetEvent( buffer, DFB_EVENT( &event )) == DFB_OK) {
          if (event.type == DIET_KEYPRESS) {
               switch (event.key_symbol) {
               case DIKS_ESCAPE:
                    mplayer_put_key( 'q' );
                    break;
               case DIKS_PAGE_UP:
                    mplayer_put_key( KEY_PAGE_UP );
                    break;
               case DIKS_PAGE_DOWN:
                    mplayer_put_key( KEY_PAGE_DOWN );
                    break;
               case DIKS_CURSOR_UP:
                    mplayer_put_key( KEY_UP );
                    break;
               case DIKS_CURSOR_DOWN:
                    mplayer_put_key( KEY_DOWN );
                    break;
               case DIKS_CURSOR_LEFT:
                    mplayer_put_key( KEY_LEFT );
                    break;
               case DIKS_CURSOR_RIGHT:
                    mplayer_put_key( KEY_RIGHT );
                    break;
               case DIKS_INSERT:
                    mplayer_put_key( KEY_INSERT );
                    break;
               case DIKS_DELETE:
                    mplayer_put_key( KEY_DELETE );
                    break;
               case DIKS_HOME:
                    mplayer_put_key( KEY_HOME );
                    break;
               case DIKS_END:
                    mplayer_put_key( KEY_END );
                    break;
               default:
                    mplayer_put_key( event.key_symbol );
               }
          }
     }

     /*
      * empty buffer, because of repeating
      * keyboard repeat is faster than key handling and this causes problems during seek
      * temporary workabout. should be solved in the future
      */
     buffer->Reset( buffer );
}
