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

static vo_info_t info = {
     "DirectFB / Matrox G400",
     "dfbmga",
     "Ville Syrjala <syrjala@sci.fi>",
     ""
};

LIBVO_EXTERN(dfbmga)

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

static int num_bufs;
static int current_buf;
static int current_ip_buf;
static IDirectFBSurface *bufs[3];

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

static int osd_changed;

/******************************
*	    vo_directfb       *
******************************/

/* command line/config file options */
#ifdef HAVE_FBDEV
 extern char *fb_dev_name;
#else
 char *fb_dev_name;
#endif

static uint32_t in_width;
static uint32_t in_height;
static uint32_t screen_width;
static uint32_t screen_height;
static uint32_t sub_width;
static uint32_t sub_height;

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
          if (DirectFBInit( NULL, NULL ) != DFB_OK)
               return -1;

          if (!fb_dev_name && !(fb_dev_name = getenv( "FRAMEBUFFER" )))
               fb_dev_name = "/dev/fb0";
          DirectFBSetOption( "fbdev", fb_dev_name );
          DirectFBSetOption( "no-cursor", "" );
          DirectFBSetOption( "bg-color", "00000000" );

          if (DirectFBCreate( &dfb ) != DFB_OK)
               return -1;

          inited = 1;
     }

     if (use_bes) {
          if (dfb->GetDisplayLayer( dfb, 1, &bes ) != DFB_OK)
               return -1;
          bes->SetCooperativeLevel( bes, DLSCL_EXCLUSIVE );
          bes->SetOpacity( bes, 0 );
     }

     if (use_crtc2) {
          if (dfb->GetDisplayLayer( dfb, 2, &crtc2 ) != DFB_OK)
               return -1;
          crtc2->SetCooperativeLevel( crtc2, DLSCL_EXCLUSIVE );
          crtc2->SetOpacity( crtc2, 0 );
     }

     if (dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard ) != DFB_OK)
          return -1;
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
          bufs[0] = frame;
          num_bufs = 1;
     } else {
          /* Draw to a temporary surface */
          DFBSurfaceDescription dsc;

          dsc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT |
                            DSDESC_PIXELFORMAT;
          dsc.width       = in_width;
          dsc.height      = in_height;
          dsc.pixelformat = dlc.pixelformat;

          for (num_bufs = 0; num_bufs < 3; num_bufs++) {
               if (dfb->CreateSurface( dfb, &dsc, &bufs[num_bufs] ) != DFB_OK) {
                    if (num_bufs == 0)
                         return -1;
               }
          }
          frame = bufs[0];
          current_buf = 0;
          current_ip_buf = 0;
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
     } else {
          screen_width  = in_width;
          screen_height = in_height;
          use_spic      = 0;
     }

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

          if (dfb->GetDisplayLayer( dfb, 3, &spic ) != DFB_OK)
               return -1;
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
          subframe = frame;
     }

     subframe->GetSize( subframe, &sub_width, &sub_height );
     subframe->GetPixelFormat( subframe, &subframe_format );
     subframe_pixel_size = DFB_BYTES_PER_PIXEL( subframe_format );
     printf( "vo_dfbmga: Sub-picture surface %dx%d %s (%s)\n",
             sub_width, sub_height,
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

static void
vo_draw_alpha_lut8( int w, int h,
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
                    dst[x] = ((255 - srca[x]) & 0xF0) | (src[x] >> 4);
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

     if (use_spic && !osd_changed)
          return;

     if (subframe->Lock( subframe, DSLF_WRITE, &dst, &pitch ) != DFB_OK)
          return;

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
     default:
     }

     subframe->Unlock( subframe );
}

static uint32_t
draw_frame( uint8_t * src[] )
{
     return -1;
}

static uint32_t
draw_slice( uint8_t * src[], int stride[], int w, int h, int x, int y )
{
     void *dst;
     int pitch;

     if (vo_directrendering)
          frame->Unlock( frame );

     if (frame->Lock( frame, DSLF_WRITE, &dst, &pitch ) != DFB_OK)
          return VO_FALSE;

     memcpy_pic( dst + pitch * y + x, src[0],
                 w, h, pitch, stride[0] );

     dst += pitch * in_height;

     x /= 2; y /= 2;
     w /= 2; h /= 2;
     pitch /= 2;

     if (frame_format == DSPF_I420 )
          memcpy_pic( dst + pitch * y + x, src[1],
                      w, h, pitch, stride[1] );
     else 
          memcpy_pic( dst + pitch * y + x, src[2],
                      w, h, pitch, stride[2] );

     dst += pitch * in_height / 2;

     if (frame_format == DSPF_I420 )
          memcpy_pic( dst + pitch * y + x, src[2],
                      w, h, pitch, stride[2] );
     else
          memcpy_pic( dst + pitch * y + x, src[1],
                      w, h, pitch, stride[1] );

     frame->Unlock( frame );

     return VO_TRUE;
}

static void
draw_osd( void )
{
     if (vo_directrendering)
          frame->Unlock( frame );

     osd_changed = vo_osd_changed( 0 );

     if (osd_changed) {
          if (use_spic) {
               subframe->Clear( subframe, 0, 0, 0, 0 );
          } else if (use_crtc2) {
               /* Clear black bars around the picture */
               subframe->SetColor( subframe, 0, 0, 0, 0 );
               subframe->FillRectangle( subframe,
                                        0, 0,
                                        screen_width, drect.y );
               subframe->FillRectangle( subframe,
                                        0, drect.y + drect.h,
                                        screen_width, drect.y );
               subframe->FillRectangle( subframe,
                                        0, drect.y,
                                        drect.x, drect.h );
               subframe->FillRectangle( subframe,
                                        drect.x + drect.w, drect.y,
                                        drect.x, drect.h );
          }
     }

     if (use_crtc2) {
          /* Flip BES */
          if (use_bes)
               frame->Flip( frame, NULL, 0 );

          /* Blit from BES/temp to CRTC2 */
          c2frame->SetBlittingFlags( c2frame, DSBLIT_NOFX );
          if (stretch)
               c2frame->StretchBlit( c2frame, frame, NULL, &drect );
          else
               c2frame->Blit( c2frame, frame, NULL, drect.x, drect.y );
     }

     vo_draw_text( sub_width, sub_height, draw_alpha );

     if (use_spic)
          subframe->Flip( subframe, NULL, 0 );
}

static void
flip_page( void )
{
     if (!use_crtc2) {
          /* Flip BES */
          frame->Flip( frame, NULL, vo_vsync ? DSFLIP_WAITFORSYNC : 0 );
     } else {
          /* Flip CRTC2 */
          c2frame->Flip( c2frame, NULL, vo_vsync ? DSFLIP_WAITFORSYNC : 0 );
     }

     current_buf = 0;
}

static void
uninit( void )
{
     buffer->Release( buffer );
     keyboard->Release( keyboard );

     while (num_bufs--) {
          frame = bufs[num_bufs];
          frame->Release( frame );
     }
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

static uint32_t
get_image( mp_image_t *mpi )
{
     void *dst;
     int pitch;

     if (use_bes &&
         (mpi->type == MP_IMGTYPE_STATIC ||
          mpi->flags & MP_IMGFLAG_READABLE))
          return VO_FALSE;

     if (mpi->flags & MP_IMGFLAG_READABLE &&
         (mpi->type == MP_IMGTYPE_IPB || mpi->type == MP_IMGTYPE_IP)) {
          int buf = current_buf;

          if (num_bufs < 2)
               return VO_FALSE;

          current_ip_buf ^= 1;

          if (mpi->type == MP_IMGTYPE_IPB && num_bufs < 3 && current_ip_buf)
               return VO_FALSE;

          buf = current_ip_buf;

          if (mpi->type == MP_IMGTYPE_IPB)
               buf++;

          current_buf = buf;
          frame = bufs[current_buf];
     }

     if (frame->Lock( frame, DSLF_WRITE | (mpi->flags & MP_IMGFLAG_READABLE ? DSLF_READ : 0),
                      &dst, &pitch ) != DFB_OK)
          return VO_FALSE;

     if ((mpi->width == pitch) ||
         (mpi->flags & (MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_ACCEPT_WIDTH))) {

          mpi->planes[0] = dst;
          mpi->width     = in_width;
          mpi->stride[0] = pitch;

          if (mpi->flags & MP_IMGFLAG_PLANAR) {
               mpi->stride[1] = mpi->stride[2] = pitch / 2;

               if (mpi->flags & MP_IMGFLAG_SWAPPED) {
                    mpi->planes[1] = dst + in_height * pitch;
                    mpi->planes[2] = mpi->planes[1] + in_height * pitch / 4;
               } else {
                    mpi->planes[2] = dst + in_height * pitch;
                    mpi->planes[1] = mpi->planes[2] + in_height * pitch / 4;
               }
          }

          mpi->flags |= MP_IMGFLAG_DIRECT;

          return VO_TRUE;
     }

     frame->Unlock( frame );

     return VO_FALSE;
}



static uint32_t
draw_image( mp_image_t *mpi )
{
     if (mpi->flags & (MP_IMGFLAG_DIRECT | MP_IMGFLAG_DRAW_CALLBACK))
          return VO_TRUE;

     if (mpi->flags & MP_IMGFLAG_PLANAR)
          return draw_slice( mpi->planes, mpi->stride,
                             mpi->w, mpi->h, 0, 0 );
     else {
          void *dst;
          int pitch;

          if (frame->Lock( frame, DSLF_WRITE, &dst, &pitch ) != DFB_OK)
               return VO_FALSE;
          memcpy_pic( dst, mpi->planes[0],
                      mpi->w * (mpi->bpp / 8), mpi->h,
                      pitch, mpi->stride[0] );
          frame->Unlock( frame );

          return VO_TRUE;
     }
}

static int
set_equalizer( char *data, int value )
{
     DFBResult res;
     DFBColorAdjustment ca;
     float factor = (float) 0xffff / 200.0;

     ca.flags = DCAF_NONE;

     if (!strcasecmp( data, "brightness" )) {
          ca.flags      |= DCAF_BRIGHTNESS;
          ca.brightness  = value * factor + 0x8000;
     }
     if (!strcasecmp( data, "contrast" )) {
          ca.flags    |= DCAF_CONTRAST;
          ca.contrast  = value * factor + 0x8000;
     }
     if (!strcasecmp( data, "hue" )) {
          ca.flags |= DCAF_HUE;
          ca.hue    = value * factor + 0x8000;
     }
     if (!strcasecmp( data, "saturation" )) {
          ca.flags      |= DCAF_SATURATION;
          ca.saturation  = value * factor + 0x8000;
     }

     /* Prefer CRTC2 over BES */
     if (use_crtc2)
          res = crtc2->SetColorAdjustment( crtc2, &ca );
     else
          res = bes->SetColorAdjustment( bes, &ca );

     if (res != DFB_OK)
          return VO_FALSE;

     return VO_TRUE;
}

static int
get_equalizer( char *data, int *value )
{
     DFBResult res;
     DFBColorAdjustment ca;
     float factor = 200.0 / (float) 0xffff;

     /* Prefer CRTC2 over BES */
     if (use_crtc2)
          res = crtc2->GetColorAdjustment( crtc2, &ca );
     else
          res = bes->GetColorAdjustment( bes, &ca );

     if (res != DFB_OK)
          return VO_FALSE;
     
     if (!strcasecmp( data, "brightness" ) &&
         (ca.flags & DCAF_BRIGHTNESS))
          *value = (ca.brightness - 0x8000) * factor;
     if (!strcasecmp( data, "contrast" ) &&
         (ca.flags & DCAF_CONTRAST))
          *value = (ca.contrast - 0x8000) * factor;
     if (!strcasecmp( data, "hue" ) &&
         (ca.flags & DCAF_HUE))
          *value = (ca.hue - 0x8000) * factor;
     if (!strcasecmp( data, "saturation" ) &&
         (ca.flags & DCAF_SATURATION))
          *value = (ca.saturation - 0x8000) * factor;

     return VO_TRUE;
}

static uint32_t
control( uint32_t request, void *data, ... )
{
     switch (request) {
     case VOCTRL_QUERY_FORMAT:
	  return query_format( *((uint32_t *) data) );

     case VOCTRL_GET_IMAGE:
          return get_image( data );

     case VOCTRL_DRAW_IMAGE:
          return draw_image( data );

     case VOCTRL_SET_EQUALIZER:
          {
               va_list ap;
               int value;

               va_start( ap, data );
               value = va_arg( ap, int );
               va_end( ap );

               return set_equalizer( data, value );
          }
     case VOCTRL_GET_EQUALIZER:
          {
               va_list ap;
               int *value;

               va_start( ap, data );
               value = va_arg( ap, int* );
               va_end( ap );

               return get_equalizer( data, value );
          }
     }

     return VO_NOTIMPL;
}

extern void mplayer_put_key( int code );

#include "../linux/keycodes.h"

static void
check_events( void )
{
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
