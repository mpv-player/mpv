/*
   MPlayer video driver for DirectFB / Matrox G400/G450/G550

   Copyright (C) 2002,2003 Ville Syrjala <syrjala@sci.fi>

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

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "sub.h"
#include "mp_msg.h"
#include "aspect.h"

static vo_info_t info = {
     "DirectFB / Matrox G400/G450/G550",
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
static IDirectFBSurface *besframe;
static IDirectFBSurface *c2frame;
static IDirectFBSurface *subframe;

static DFBSurfacePixelFormat frame_format;
static DFBSurfacePixelFormat subframe_format;

static DFBRectangle drect;

static IDirectFBInputDevice  *keyboard;
static IDirectFBEventBuffer  *buffer;

static int inited = 0;

static int blit_done;
static int stretch;

static int use_bes;
static int use_crtc2;
static int use_spic;
static int use_input;
static int field_parity;
static int flipping;
static DFBDisplayLayerBufferMode buffermode;

static int osd_changed;
static int osd_dirty;
static int osd_current;
static int osd_max;

/******************************
*	    vo_dfbmga         *
******************************/

#if DIRECTFBVERSION < 918
 #define DSPF_ALUT44 DSPF_LUT8
 #define DLBM_TRIPLE ~0
 #define DSFLIP_ONSYNC 0
#endif

#if DIRECTFBVERSION < 916
 #define DSPF_ARGB1555 DSPF_RGB15
#endif

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
     case DSPF_ARGB1555:
	  return "ARGB1555";
     case DSPF_YUY2:
	  return "YUY2";
     case DSPF_UYVY:
	  return "UYVY";
     case DSPF_YV12:
	  return "YV12";
     case DSPF_I420:
	  return "I420";
     case DSPF_ALUT44:
	  return "ALUT44";
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
	  return DSPF_ARGB1555;
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

struct layer_enum
{
     const char *name;
     IDirectFBDisplayLayer **layer;
     DFBResult res;
};

static DFBEnumerationResult
get_layer_by_name( DFBDisplayLayerID id,
                   DFBDisplayLayerDescription desc,
                   void *data )
{
     struct layer_enum *l = (struct layer_enum *) data;

#if DIRECTFBVERSION > 915
     /* We have desc.name so use it */
     if (!strcmp( l->name, desc.name ))
          if ((l->res = dfb->GetDisplayLayer( dfb, id, l->layer )) == DFB_OK)
               return DFENUM_CANCEL;
#else
     /* Fake it according to id */
     if ((id == 1 && !strcmp( l->name, "Matrox Backend Scaler" )) ||
         (id == 2 && !strcmp( l->name, "Matrox CRTC2" )) ||
         (id == 3 && !strcmp( l->name, "Matrox CRTC2 Sub-Picture" )))
          if ((l->res = dfb->GetDisplayLayer( dfb, id, l->layer )) == DFB_OK)
               return DFENUM_CANCEL;
#endif

     return DFENUM_OK;
}

static uint32_t
preinit( const char *arg )
{
     DFBResult res;

     bes = NULL;
     crtc2 = NULL;
     keyboard = NULL;
     buffer = NULL;

     /* Some defaults */
     use_bes = 0;
     use_crtc2 = 1;
     use_spic = 1;
     field_parity = -1;
#if DIRECTFBVERSION > 917
     buffermode = DLBM_TRIPLE;
     osd_max = 4;
#else
     buffermode = DLBM_BACKVIDEO;
     osd_max = 2;
#endif
     flipping = 1;

     use_input = !getenv( "DISPLAY" );

     if (vo_subdevice) {
          int show_help = 0;
          int opt_no = 0;
          while (*vo_subdevice != '\0') {
               if (!strncmp(vo_subdevice, "bes", 3)) {
                    use_bes = !opt_no;
                    vo_subdevice += 3;
                    opt_no = 0;
               } else if (!strncmp(vo_subdevice, "crtc2", 5)) {
                    use_crtc2 = !opt_no;
                    vo_subdevice += 5;
                    opt_no = 0;
               } else if (!strncmp(vo_subdevice, "spic", 4)) {
                    use_spic = !opt_no;
                    vo_subdevice += 4;
                    opt_no = 0;
               } else if (!strncmp(vo_subdevice, "input", 5)) {
                    use_input = !opt_no;
                    vo_subdevice += 5;
                    opt_no = 0;
               } else if (!strncmp(vo_subdevice, "buffermode=", 11)) {
                    if (opt_no) {
                         show_help = 1;
                         break;
                    }
                    vo_subdevice += 11;
                    if (!strncmp(vo_subdevice, "single", 6)) {
                         buffermode = DLBM_FRONTONLY;
                         osd_max = 1;
                         flipping = 0;
                         vo_subdevice += 6;
                    } else if (!strncmp(vo_subdevice, "double", 6)) {
                         buffermode = DLBM_BACKVIDEO;
                         osd_max = 2;
                         flipping = 1;
                         vo_subdevice += 6;
                    } else if (!strncmp(vo_subdevice, "triple", 6)) {
                         buffermode = DLBM_TRIPLE;
                         osd_max = 4;
                         flipping = 1;
                         vo_subdevice += 6;
                    } else {
                         show_help = 1;
                         break;
                    }
                    opt_no = 0;
               } else if (!strncmp(vo_subdevice, "fieldparity=", 12)) {
                    if (opt_no) {
                         show_help = 1;
                         break;
                    }
                    vo_subdevice += 12;
                    if (!strncmp(vo_subdevice, "top", 3)) {
                         field_parity = 0;
                         vo_subdevice += 3;
                    } else if (!strncmp(vo_subdevice, "bottom", 6)) {
                         field_parity = 1;
                         vo_subdevice += 6;
                    } else {
                         show_help = 1;
                         break;
                    }
                    opt_no = 0;
               } else if (!strncmp(vo_subdevice, "no", 2)) {
                    if (opt_no) {
                         show_help = 1;
                         break;
                    }
                    vo_subdevice += 2;
                    opt_no = 1;
               } else if (*vo_subdevice == ':') {
                    if (opt_no) {
                         show_help = 1;
                         break;
                    }
                    vo_subdevice++;
                    opt_no = 0;
               } else {
                    show_help = 1;
                    break;
               }
               }
          if (show_help) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "\nvo_dfbmga command line help:\n"
                       "Example: mplayer -vo dfbmga:nocrtc2:bes:buffermode=single\n"
                       "\nOptions (use 'no' prefix to disable):\n"
                       "  bes    Use Backend Scaler\n"
                       "  crtc2  Use CRTC2\n"
                       "  spic   Use hardware sub-picture for OSD\n"
                       "  input  Use DirectFB for keyboard input\n"
                       "\nOther options:\n"
                       "  buffermode=(single|double|triple)\n"
                       "    single   Use single buffering\n"
                       "    double   Use double buffering\n"
                       "    triple   Use triple buffering\n"
                       "  fieldparity=(top|bottom)\n"
                       "    top      Top field first\n"
                       "    bottom   Bottom field first\n"
                       "\n" );
               return -1;
          }
     }
     if (!use_bes && !use_crtc2) {
	  mp_msg( MSGT_VO, MSGL_ERR, "vo_dfbmga: No output selected\n" );
          return -1;
     }

     if (!inited) {
          if ((res = DirectFBInit( NULL, NULL )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: DirectFBInit() failed - %s\n",
                       DirectFBErrorString( res ) );
               return -1;
          }

          if (!fb_dev_name && !(fb_dev_name = getenv( "FRAMEBUFFER" )))
               fb_dev_name = "/dev/fb0";
          DirectFBSetOption( "fbdev", fb_dev_name );
          DirectFBSetOption( "no-cursor", "" );
          DirectFBSetOption( "bg-color", "00000000" );

          if ((res = DirectFBCreate( &dfb )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: DirectFBCreate() failed - %s\n",
                       DirectFBErrorString( res ) );
               return -1;
          }

          inited = 1;
     }

     if (use_bes) {
          struct layer_enum l = {
               "Matrox Backend Scaler",
               &bes,
               DFB_UNSUPPORTED
          };

          dfb->EnumDisplayLayers( dfb, get_layer_by_name, &l );
          if (l.res != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "Can't get BES layer - %s\n",
                       DirectFBErrorString( l.res ) );
               return -1;
          }
          if ((res = bes->SetCooperativeLevel( bes, DLSCL_EXCLUSIVE )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "Can't get exclusive access to BES - %s\n",
                       DirectFBErrorString( res ) );
               return -1;
          }
          bes->SetOpacity( bes, 0 );
     }

     if (use_crtc2) {
          struct layer_enum l = {
               "Matrox CRTC2",
               &crtc2,
               DFB_UNSUPPORTED
          };

          dfb->EnumDisplayLayers( dfb, get_layer_by_name, &l );
          if (l.res != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "Can't get CRTC2 layer - %s\n",
                       DirectFBErrorString( l.res ) );
               return -1;
          }
          if ((res = crtc2->SetCooperativeLevel( crtc2, DLSCL_EXCLUSIVE )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "Can't get exclusive access to CRTC2 - %s\n",
                       DirectFBErrorString( res ) );
               return -1;
          }
          crtc2->SetOpacity( crtc2, 0 );
     }

     if (use_input) {
          if ((res = dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Can't get keyboard - %s\n",
                       DirectFBErrorString( res ) );
               return -1;
          }
          keyboard->CreateEventBuffer( keyboard, &buffer );
          buffer->Reset( buffer );
     }

     return 0;
}

static uint32_t
config( uint32_t width, uint32_t height,
        uint32_t d_width, uint32_t d_height,
        uint32_t fullscreen,
        char *title,
	uint32_t format )
{
     DFBResult res;

     DFBDisplayLayerConfig      dlc;
     DFBDisplayLayerConfigFlags failed;

     uint32_t out_width;
     uint32_t out_height;

     besframe = NULL;
     c2frame = NULL;
     spic = NULL;
     subframe = NULL;
     bufs[0] = bufs[1] = bufs[2] = NULL;
     
     in_width  = width;
     in_height = height;

     aspect_save_orig(width, height);
     aspect_save_prescale(d_width, d_height);

     dlc.pixelformat   = imgfmt_to_pixelformat( format );

     {
          /* Draw to a temporary surface */
          DFBSurfaceDescription dsc;

          dsc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT |
                            DSDESC_PIXELFORMAT;
          dsc.width       = in_width;
          dsc.height      = in_height;
          dsc.pixelformat = dlc.pixelformat;

          /* Don't waste video memory since we don't need direct stretchblit */
          if (use_bes) {
               dsc.flags |= DSDESC_CAPS;
               dsc.caps   = DSCAPS_SYSTEMONLY;
          }

          for (num_bufs = 0; num_bufs < 3; num_bufs++) {
               if ((res = dfb->CreateSurface( dfb, &dsc, &bufs[num_bufs] )) != DFB_OK) {
                    if (num_bufs == 0) {
                         mp_msg( MSGT_VO, MSGL_ERR,
                                 "vo_dfbmga: Can't create surfaces - %s!\n",
                                 DirectFBErrorString( res ) );
                         return -1;
                    }
                    break;
               }
          }
          frame = bufs[0];
          current_buf = 0;
          current_ip_buf = 0;
     }
     frame->GetPixelFormat( frame, &frame_format );
     mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: Video surface %dx%d %s\n",
             in_width, in_height,
             pixelformat_name( frame_format ) );


     /*
      * BES
      */
     if (use_bes) {
          dlc.flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
          dlc.width       = in_width;
          dlc.height      = in_height;

          if (use_crtc2)
               dlc.buffermode = DLBM_FRONTONLY;
          else
               dlc.buffermode = buffermode;

          if ((res = bes->TestConfiguration( bes, &dlc, &failed )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Invalid BES configuration - %s!\n",
                       DirectFBErrorString( res ) );
               return -1;
          }
          if ((res = bes->SetConfiguration( bes, &dlc )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: BES configuration failed - %s!\n",
                       DirectFBErrorString( res ) );
               return -1;
          }
          bes->GetSurface( bes, &besframe );
          besframe->SetBlittingFlags( besframe, DSBLIT_NOFX );

          aspect_save_screenres( 10000, 10000 );
          aspect( &out_width, &out_height, A_ZOOM );
          bes->SetScreenLocation( bes,
                                  (1.0f - (float) out_width  / 10000.0f) / 2.0f,
                                  (1.0f - (float) out_height / 10000.0f) / 2.0f,
                                  (float) out_width  / 10000.0f,
                                  (float) out_height / 10000.0f );

          besframe->Clear( besframe, 0, 0, 0, 0xff );
          besframe->Flip( besframe, NULL, 0 );
          besframe->Clear( besframe, 0, 0, 0, 0xff );
          besframe->Flip( besframe, NULL, 0 );
          besframe->Clear( besframe, 0, 0, 0, 0xff );

          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: BES using %s buffering\n",
                  dlc.buffermode == DLBM_TRIPLE ? "triple" :
                  dlc.buffermode == DLBM_BACKVIDEO ? "double" : "single" );
          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: BES surface %dx%d %s\n", dlc.width, dlc.height, pixelformat_name( dlc.pixelformat ) );
     }

     /*
      * CRTC2
      */
     if (use_crtc2) {
          dlc.flags      = DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;
          dlc.buffermode = buffermode;
          dlc.options    = DLOP_FLICKER_FILTERING;

#if DIRECTFBVERSION > 916
          if (field_parity != -1) {
               dlc.options  = DLOP_FIELD_PARITY;
          }
#endif

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

          if ((res = crtc2->TestConfiguration( crtc2, &dlc, &failed )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Invalid CRTC2 configuration - %s!\n",
                       DirectFBErrorString( res ) );
               return -1;
          }
          if ((res = crtc2->SetConfiguration( crtc2, &dlc )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: CRTC2 configuration failed - %s!\n",
                       DirectFBErrorString( res ) );
               return -1;
          }

#if DIRECTFBVERSION > 916
          if (field_parity != -1)
               crtc2->SetFieldParity( crtc2, field_parity );
#endif

          crtc2->GetSurface( crtc2, &c2frame );
          c2frame->SetBlittingFlags( c2frame, DSBLIT_NOFX );
          c2frame->SetColor( c2frame, 0, 0, 0, 0xff );

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

          c2frame->Clear( c2frame, 0, 0, 0, 0xff );
          c2frame->Flip( c2frame, NULL, 0 );
          c2frame->Clear( c2frame, 0, 0, 0, 0xff );
          c2frame->Flip( c2frame, NULL, 0 );
          c2frame->Clear( c2frame, 0, 0, 0, 0xff );

          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: CRTC2 using %s buffering\n",
                  dlc.buffermode == DLBM_TRIPLE ? "triple" :
                  dlc.buffermode == DLBM_BACKVIDEO ? "double" : "single" );
          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: CRTC2 surface %dx%d %s\n", dlc.width, dlc.height, pixelformat_name( dlc.pixelformat ) );
     } else {
          use_spic      = 0;
     }

     /*
      * Sub-picture
      */
     if (use_spic) {
          /* Draw OSD to sub-picture surface */
          IDirectFBPalette *palette;
          DFBColor          color;
          int               i;
          struct layer_enum l = {
               "Matrox CRTC2 Sub-Picture",
               &spic,
               DFB_UNSUPPORTED
          };
          dfb->EnumDisplayLayers( dfb, get_layer_by_name, &l );
          if (l.res != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "vo_dfbmga: Can't get sub-picture layer - %s\n",
                       DirectFBErrorString( l.res ) );
               return -1;
          }
          if ((res = spic->SetCooperativeLevel( spic, DLSCL_EXCLUSIVE )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "Can't get exclusive access to sub-picture - %s\n",
                       DirectFBErrorString( res ) );
               return -1;
          }
          spic->SetOpacity( spic, 0 );

          dlc.flags       = DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
          dlc.pixelformat = DSPF_ALUT44;
          dlc.buffermode  = buffermode;

#if DIRECTFBVERSION > 916
          dlc.flags      |= DLCONF_OPTIONS;
          dlc.options     = DLOP_ALPHACHANNEL;
#endif
          if ((res = spic->TestConfiguration( spic, &dlc, &failed )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Invalid sub-picture configuration - %s!\n",
                       DirectFBErrorString( res ) );
               return -1;
          }
          if ((res = spic->SetConfiguration( spic, &dlc )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Sub-picture configuration failed - %s!\n",
                       DirectFBErrorString( res ) );
               return -1;
          }

          spic->GetSurface( spic, &subframe );

          subframe->GetPalette( subframe, &palette );
          color.a = 0xff;
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
          subframe->Flip( subframe, NULL, 0 );
          subframe->Clear( subframe, 0, 0, 0, 0 );

          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: Sub-picture layer using %s buffering\n",
                  dlc.buffermode == DLBM_TRIPLE ? "triple" :
                  dlc.buffermode == DLBM_BACKVIDEO ? "double" : "single" );
     } else if (use_crtc2) {
          /* Draw OSD to CRTC2 surface */
          subframe = c2frame;
     } else {
          /* Draw OSD to BES surface */
          subframe = besframe;
     }

     subframe->GetSize( subframe, &sub_width, &sub_height );
     subframe->GetPixelFormat( subframe, &subframe_format );
     mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: Sub-picture surface %dx%d %s (%s)\n",
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

     osd_dirty = 0;
     osd_current = 1;
     blit_done = 0;

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
vo_draw_alpha_alut44( int w, int h,
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
clear_alpha( int x0, int y0,
             int w, int h )
{
     if (use_spic && !flipping && vo_osd_changed)
          subframe->FillRectangle( subframe, x0, y0, w, h );
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

     if (use_spic) {
          if (!osd_changed || (!flipping && !vo_osd_changed))
               return;
          osd_dirty |= osd_current;
     } else if (use_crtc2) {
          if (x0 < drect.x ||
              y0 < drect.y ||
              x0 + w > drect.x + drect.w ||
              y0 + h > drect.y + drect.h)
               osd_dirty |= osd_current;
     }

     if (subframe->Lock( subframe, DSLF_READ | DSLF_WRITE, &dst, &pitch ) != DFB_OK)
          return;

     switch (subframe_format) {
     case DSPF_ALUT44:
          vo_draw_alpha_alut44( w, h, src, srca, stride,
                                ((uint8_t *) dst) + pitch * y0 + x0,
                              pitch );
          break;
     case DSPF_RGB32:
     case DSPF_ARGB:
	  vo_draw_alpha_rgb32( w, h, src, srca, stride,
			       (( uint8_t *) dst) + pitch * y0 + 4 * x0,
                               pitch );
	  break;
     case DSPF_RGB24:
	  vo_draw_alpha_rgb24( w, h, src, srca, stride,
			       ((uint8_t *) dst) + pitch * y0 + 3 * x0,
                               pitch );
	  break;
     case DSPF_RGB16:
	  vo_draw_alpha_rgb16( w, h, src, srca, stride,
			       ((uint8_t *) dst) + pitch * y0 + 2 * x0,
                               pitch );
	  break;
     case DSPF_ARGB1555:
	  vo_draw_alpha_rgb15( w, h, src, srca, stride,
			       ((uint8_t *) dst) + pitch * y0 + 2 * x0,
                               pitch );
	  break;
     case DSPF_YUY2:
	  vo_draw_alpha_yuy2( w, h, src, srca, stride,
			      ((uint8_t *) dst) + pitch * y0 + 2 * x0,
                              pitch );
	  break;
     case DSPF_UYVY:
	  vo_draw_alpha_yuy2( w, h, src, srca, stride,
			      ((uint8_t *) dst) + pitch * y0 + 2 * x0 + 1,
                              pitch );
	  break;
     case DSPF_I420:
     case DSPF_YV12:
	  vo_draw_alpha_yv12( w, h, src, srca, stride,
			      ((uint8_t *) dst) + pitch * y0 + x0,
                              pitch );
	  break;
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
blit_to_screen( void )
{
     IDirectFBSurface *blitsrc = frame;

     if (use_bes) {
          if (vo_vsync && !flipping && !use_crtc2)
               bes->WaitForSync( bes );

          besframe->Blit( besframe, blitsrc, NULL, 0, 0 );
          blitsrc = besframe;
     }

     if (use_crtc2) {
          if (vo_vsync && !flipping)
               crtc2->WaitForSync( crtc2 );

     if (stretch)
               c2frame->StretchBlit( c2frame, blitsrc, NULL, &drect );
     else
               c2frame->Blit( c2frame, blitsrc, NULL, drect.x, drect.y );
     }
}

static void
draw_osd( void )
{
     frame = bufs[current_buf];
     frame->Unlock( frame );

     osd_changed = vo_osd_changed( 0 );
     if (osd_dirty & osd_current) {
          if (use_spic) {
               if (flipping)
                    subframe->Clear( subframe, 0, 0, 0, 0 );
          } else if (use_crtc2) {
               /* Clear black bars around the picture */
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
          osd_dirty &= ~osd_current;
     }

          blit_to_screen();
          blit_done = 1;

     vo_remove_text( sub_width, sub_height, clear_alpha );
     vo_draw_text( sub_width, sub_height, draw_alpha );

     if (use_spic && flipping && osd_changed) {
          subframe->Flip( subframe, NULL, 0 );
          osd_current <<= 1;
          if (osd_current > osd_max)
               osd_current = 1;
     }
}

static void
flip_page( void )
{
          if (!blit_done)
               blit_to_screen();

     if (flipping) {
          if (use_crtc2)
               c2frame->Flip( c2frame, NULL, vo_vsync ? DSFLIP_WAITFORSYNC : DSFLIP_ONSYNC );
          else
               besframe->Flip( besframe, NULL, vo_vsync ? DSFLIP_WAITFORSYNC : DSFLIP_ONSYNC );

          if (!use_spic) {
               osd_current <<= 1;
               if (osd_current > osd_max)
                    osd_current = 1;
          }
     }

     blit_done = 0;
     current_buf = 0;//vo_directrendering ? 0 : (current_buf + 1) % num_bufs;
}

static void
uninit( void )
{
     if (buffer)
          buffer->Release( buffer );
     if (keyboard)
          keyboard->Release( keyboard );
     buffer = NULL;
     keyboard = NULL;

     while (num_bufs--) {
          frame = bufs[num_bufs];
          if (frame)
               frame->Release( frame );
          bufs[num_bufs] = NULL;
     }
     if (bes) {
          if (besframe)
               besframe->Release( besframe );
          bes->SetOpacity( bes, 0 );
          bes->Release( bes );
          besframe = NULL;
          bes = NULL;
     }
     if (crtc2) {
          if (c2frame)
               c2frame->Release( c2frame );
          crtc2->SetOpacity( crtc2, 0 );
          crtc2->Release( crtc2 );
          c2frame = NULL;
          crtc2 = NULL;
     }
     if (spic) {
          if (subframe)
               subframe->Release( subframe );
          spic->SetOpacity( spic, 0 );
          spic->Release( spic );
          subframe = NULL;
          spic = NULL;
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
     int buf = current_buf;
     void *dst;
     int pitch;

     if (use_bes &&
         (mpi->type == MP_IMGTYPE_STATIC ||
          mpi->flags & MP_IMGFLAG_READABLE))
          return VO_FALSE;

     if (mpi->flags & MP_IMGFLAG_READABLE &&
         (mpi->type == MP_IMGTYPE_IPB || mpi->type == MP_IMGTYPE_IP)) {
          if (num_bufs < 2)
               return VO_FALSE;

          current_ip_buf ^= 1;

          if (mpi->type == MP_IMGTYPE_IPB && num_bufs < 3 && current_ip_buf)
               return VO_FALSE;

          buf = current_ip_buf;

          if (mpi->type == MP_IMGTYPE_IPB)
               buf++;
     }
     frame = bufs[buf];
     frame->Unlock( frame );

     /* Always use DSLF_READ to preserve system memory copy */
     if (frame->Lock( frame, DSLF_WRITE | DSLF_READ,
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
          mpi->priv = (void *) buf;
          current_buf = buf;

          return VO_TRUE;
     }

     frame->Unlock( frame );

     return VO_FALSE;
}



static uint32_t
draw_image( mp_image_t *mpi )
{
     if (mpi->flags & MP_IMGFLAG_DIRECT) {
          current_buf = (int) mpi->priv;
          return VO_TRUE;
     }
     if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)
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
     case VOCTRL_GUISUPPORT:
     case VOCTRL_GUI_NOWINDOW:
          return VO_TRUE;

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

#include "../osdep/keycodes.h"

static void
check_events( void )
{
     DFBInputEvent event;

     if (!use_input)
          return;

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
