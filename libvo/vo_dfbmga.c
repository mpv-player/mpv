/*
 * MPlayer video driver for DirectFB / Matrox G200/G400/G450/G550
 *
 * copyright (C) 2002-2008 Ville Syrjala <syrjala@sci.fi>
 * Originally based on vo_directfb.c by Jiri Svoboda <Jiri.Svoboda@seznam.cz>.
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* directfb includes */
#include <directfb.h>

#define DFB_VERSION(a,b,c) (((a)<<16)|((b)<<8)|(c))

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
#include "mp_fifo.h"

static const vo_info_t info = {
     "DirectFB / Matrox G200/G400/G450/G550",
     "dfbmga",
     "Ville Syrjala <syrjala@sci.fi>",
     ""
};

const LIBVO_EXTERN(dfbmga)

static IDirectFB *dfb;

static IDirectFBDisplayLayer *crtc1;
static IDirectFBDisplayLayer *bes;
static IDirectFBDisplayLayer *crtc2;
static IDirectFBDisplayLayer *spic;

static int num_bufs;
static int current_buf;
static int current_ip_buf;
static IDirectFBSurface *bufs[3];

static IDirectFBSurface *frame;
static IDirectFBSurface *subframe;

static IDirectFBSurface *besframe;
static IDirectFBSurface *c1frame;
static IDirectFBSurface *c2frame;
static IDirectFBSurface *spicframe;

static DFBSurfacePixelFormat frame_format;
static DFBSurfacePixelFormat subframe_format;

static DFBRectangle besrect;
static DFBRectangle c1rect;
static DFBRectangle c2rect;
static DFBRectangle *subrect;

static IDirectFBInputDevice  *keyboard;
static IDirectFBInputDevice  *remote;
static IDirectFBEventBuffer  *buffer;

static int blit_done;
static int c1stretch;
static int c2stretch;

static int use_bes;
static int use_crtc1;
static int use_crtc2;
static int use_spic;
static int use_input;
static int use_remote;
static int field_parity;
static int flipping;
static DFBDisplayLayerBufferMode buffermode;
static int tvnorm;

static int osd_changed;
static int osd_dirty;
static int osd_current;
static int osd_max;

static int is_g200;

#if DIRECTFBVERSION < DFB_VERSION(0,9,18)
 #define DSPF_ALUT44 DSPF_LUT8
 #define DLBM_TRIPLE ~0
 #define DSFLIP_ONSYNC 0
#endif

#if DIRECTFBVERSION < DFB_VERSION(0,9,16)
 #define DSPF_ARGB1555 DSPF_RGB15
#endif

static uint32_t in_width;
static uint32_t in_height;
static uint32_t buf_height;
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
#if DIRECTFBVERSION > DFB_VERSION(0,9,21)
     case DSPF_NV12:
          return "NV12";
     case DSPF_NV21:
          return "NV21";
#endif
     default:
          return "Unknown pixel format";
     }
}

static DFBSurfacePixelFormat
imgfmt_to_pixelformat( uint32_t format )
{
     switch (format) {
     case IMGFMT_BGR32:
          return DSPF_RGB32;
     case IMGFMT_BGR16:
          return DSPF_RGB16;
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
#if DIRECTFBVERSION > DFB_VERSION(0,9,21)
     case IMGFMT_NV12:
          return DSPF_NV12;
     case IMGFMT_NV21:
          return DSPF_NV21;
#endif
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

#if DIRECTFBVERSION > DFB_VERSION(0,9,15)
     /* We have desc.name so use it */
     if (!strcmp( l->name, desc.name ))
          if ((l->res = dfb->GetDisplayLayer( dfb, id, l->layer )) == DFB_OK)
               return DFENUM_CANCEL;
#else
     /* Fake it according to id */
     if ((id == 0 && !strcmp( l->name, "FBDev Primary Layer" )) ||
         (id == 1 && !strcmp( l->name, "Matrox Backend Scaler" )) ||
         (id == 2 && !strcmp( l->name, "Matrox CRTC2" )) ||
         (id == 3 && !strcmp( l->name, "Matrox CRTC2 Sub-Picture" )))
          if ((l->res = dfb->GetDisplayLayer( dfb, id, l->layer )) == DFB_OK)
               return DFENUM_CANCEL;
#endif

     return DFENUM_OK;
}

static int
preinit( const char *arg )
{
     DFBResult res;
     int force_input = -1;

     /* Some defaults */
     use_bes = 0;
     use_crtc1 = 0;
     use_crtc2 = 1;
     use_spic = 1;
     field_parity = -1;
#if DIRECTFBVERSION > DFB_VERSION(0,9,17)
     buffermode = DLBM_TRIPLE;
     osd_max = 4;
#else
     buffermode = DLBM_BACKVIDEO;
     osd_max = 2;
#endif
     flipping = 1;
     tvnorm = -1;

     use_input = !getenv( "DISPLAY" );

     if (vo_subdevice) {
          int show_help = 0;
          int opt_no = 0;
          while (*vo_subdevice != '\0') {
               if (!strncmp(vo_subdevice, "bes", 3)) {
                    use_bes = !opt_no;
                    vo_subdevice += 3;
                    opt_no = 0;
               } else if (!strncmp(vo_subdevice, "crtc1", 5)) {
                    use_crtc1 = !opt_no;
                    vo_subdevice += 5;
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
                    force_input = !opt_no;
                    vo_subdevice += 5;
                    opt_no = 0;
               } else if (!strncmp(vo_subdevice, "remote", 6)) {
                    use_remote = !opt_no;
                    vo_subdevice += 6;
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
               } else if (!strncmp(vo_subdevice, "tvnorm=", 7)) {
                    if (opt_no) {
                         show_help = 1;
                         break;
                    }
                    vo_subdevice += 7;
                    if (!strncmp(vo_subdevice, "pal", 3)) {
                         tvnorm = 0;
                         vo_subdevice += 3;
                    } else if (!strncmp(vo_subdevice, "ntsc" , 4)) {
                         tvnorm = 1;
                         vo_subdevice += 4;
                    } else if (!strncmp(vo_subdevice, "auto" , 4)) {
                         tvnorm = 2;
                         vo_subdevice += 4;
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
                       "  crtc1  Use CRTC1\n"
                       "  crtc2  Use CRTC2\n"
                       "  spic   Use hardware sub-picture for OSD\n"
                       "  input  Use DirectFB for keyboard input\n"
                       "  remote Use DirectFB for remote control input\n"
                       "\nOther options:\n"
                       "  buffermode=(single|double|triple)\n"
                       "    single   Use single buffering\n"
                       "    double   Use double buffering\n"
                       "    triple   Use triple buffering\n"
                       "  fieldparity=(top|bottom)\n"
                       "    top      Top field first\n"
                       "    bottom   Bottom field first\n"
                       "  tvnorm=(pal|ntsc|auto)\n"
                       "    pal      Force PAL\n"
                       "    ntsc     Force NTSC\n"
                       "    auto     Select according to FPS\n"
                       "\n" );
               return -1;
          }
     }
     if (!use_bes && !use_crtc1 && !use_crtc2) {
          mp_msg( MSGT_VO, MSGL_ERR, "vo_dfbmga: No output selected\n" );
          return -1;
     }
     if (use_bes && use_crtc1) {
          mp_msg( MSGT_VO, MSGL_ERR, "vo_dfbmga: Both BES and CRTC1 outputs selected\n" );
          return -1;
     }

     if ((res = DirectFBInit( NULL, NULL )) != DFB_OK) {
          mp_msg( MSGT_VO, MSGL_ERR,
                  "vo_dfbmga: DirectFBInit() failed - %s\n",
                  DirectFBErrorString( res ) );
          return -1;
     }

     switch (tvnorm) {
     case 0:
          DirectFBSetOption( "matrox-tv-standard", "pal" );
          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: Forced TV standard to PAL\n" );
          break;
     case 1:
          DirectFBSetOption( "matrox-tv-standard", "ntsc" );
          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: Forced TV standard to NTSC\n" );
          break;
     case 2:
          if (vo_fps > 27) {
               DirectFBSetOption( "matrox-tv-standard", "ntsc" );
               mp_msg( MSGT_VO, MSGL_INFO,
                       "vo_dfbmga: Selected TV standard based upon FPS: NTSC\n" );
          } else {
               DirectFBSetOption( "matrox-tv-standard", "pal" );
               mp_msg( MSGT_VO, MSGL_INFO,
                       "vo_dfbmga: Selected TV standard based upon FPS: PAL\n" );
          }
          break;
     }

     if ((res = DirectFBCreate( &dfb )) != DFB_OK) {
          mp_msg( MSGT_VO, MSGL_ERR,
                  "vo_dfbmga: DirectFBCreate() failed - %s\n",
                  DirectFBErrorString( res ) );
          return -1;
     }

     if (use_crtc1 || use_bes) {
          struct layer_enum l = {
               "FBDev Primary Layer",
               &crtc1,
               DFB_UNSUPPORTED
          };
          dfb->EnumDisplayLayers( dfb, get_layer_by_name, &l );
          if (l.res != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "vo_dfbmga: Can't get CRTC1 layer - %s\n",
                       DirectFBErrorString( l.res ) );
               uninit();
               return -1;
          }
          if ((res = crtc1->SetCooperativeLevel( crtc1, DLSCL_EXCLUSIVE )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "Can't get exclusive access to CRTC1 layer - %s\n",
                       DirectFBErrorString( res ) );
               uninit();
               return -1;
          }
          use_input = 1;
     }

     if (force_input != -1)
          use_input = force_input;

     if (use_bes) {
          DFBDisplayLayerConfig      dlc;
          DFBDisplayLayerConfigFlags failed;
          struct layer_enum l = {
               "Matrox Backend Scaler",
               &bes,
               DFB_UNSUPPORTED
          };

          dfb->EnumDisplayLayers( dfb, get_layer_by_name, &l );
          if (l.res != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "Can't get BES layer - %s\n",
                       DirectFBErrorString( l.res ) );
               uninit();
               return -1;
          }
          if ((res = bes->SetCooperativeLevel( bes, DLSCL_EXCLUSIVE )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "Can't get exclusive access to BES - %s\n",
                       DirectFBErrorString( res ) );
               uninit();
               return -1;
          }
          dlc.flags = DLCONF_PIXELFORMAT;
          dlc.pixelformat = DSPF_RGB16;
          if (bes->TestConfiguration( bes, &dlc, &failed ) != DFB_OK) {
               is_g200 = 1;
               use_crtc2 = 0;
          }
     }

     if (use_crtc2) {
          struct layer_enum l = {
#if DIRECTFBVERSION > DFB_VERSION(0,9,20)
               "Matrox CRTC2 Layer",
#else
               "Matrox CRTC2",
#endif
               &crtc2,
               DFB_UNSUPPORTED
          };

          dfb->EnumDisplayLayers( dfb, get_layer_by_name, &l );
          if (l.res != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "Can't get CRTC2 layer - %s\n",
                       DirectFBErrorString( l.res ) );
               uninit();
               return -1;
          }
          if ((res = crtc2->SetCooperativeLevel( crtc2, DLSCL_EXCLUSIVE )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR, "Can't get exclusive access to CRTC2 - %s\n",
                       DirectFBErrorString( res ) );
               uninit();
               return -1;
          }
     }

     if (use_input || use_remote) {
          if ((res = dfb->CreateEventBuffer( dfb, &buffer )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Can't create event buffer - %s\n",
                       DirectFBErrorString( res ) );
               uninit();
               return -1;
          }
     }

     if (use_input) {
          if ((res = dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Can't get keyboard - %s\n",
                       DirectFBErrorString( res ) );
               uninit();
               return -1;
          }
          if ((res = keyboard->AttachEventBuffer( keyboard, buffer )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Can't attach event buffer to keyboard - %s\n",
                       DirectFBErrorString( res ) );
               uninit();
               return -1;
          }
     }
     if (use_remote) {
          if ((res = dfb->GetInputDevice( dfb, DIDID_REMOTE, &remote )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Can't get remote control - %s\n",
                       DirectFBErrorString( res ) );
               uninit();
               return -1;
          }
          if ((res = remote->AttachEventBuffer( remote, buffer )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Can't attach event buffer to remote control - %s\n",
                       DirectFBErrorString( res ) );
               uninit();
               return -1;
          }
     }

     return 0;
}

static void release_config( void )
{
     if (spicframe)
          spicframe->Release( spicframe );
     if (spic)
          spic->Release( spic );
     if (c2frame)
          c2frame->Release( c2frame );
     if (c1frame)
          c1frame->Release( c1frame );
     if (besframe)
          besframe->Release( besframe );
     if (bufs[0])
          bufs[0]->Release( bufs[0] );
     if (bufs[1])
          bufs[1]->Release( bufs[1] );
     if (bufs[2])
          bufs[2]->Release( bufs[2] );

     spicframe = NULL;
     spic = NULL;
     c2frame = NULL;
     c1frame = NULL;
     besframe = NULL;
     bufs[0] = NULL;
     bufs[1] = NULL;
     bufs[2] = NULL;
}

static int
config( uint32_t width, uint32_t height,
        uint32_t d_width, uint32_t d_height,
        uint32_t flags,
        char *title,
        uint32_t format )
{
     DFBResult res;

     DFBDisplayLayerConfig      dlc;
     DFBDisplayLayerConfigFlags failed;

     uint32_t out_width;
     uint32_t out_height;

     release_config();

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
          dsc.width       = (in_width + 15) & ~15;
          dsc.height      = (in_height + 15) & ~15;
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
          buf_height = dsc.height;
     }
     frame->GetPixelFormat( frame, &frame_format );
     mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: Video surface %dx%d %s\n",
             in_width, in_height,
             pixelformat_name( frame_format ) );


     /*
      * BES
      */
     if (use_bes) {
          aspect_save_screenres( 0x10000, 0x10000 );
          aspect( &out_width, &out_height, A_ZOOM );
          besrect.x = (0x10000 - out_width) * in_width / out_width / 2;
          besrect.y = (0x10000 - out_height) * in_height / out_height / 2;
          besrect.w = in_width;
          besrect.h = in_height;

          dlc.flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
          dlc.width       = besrect.w + besrect.x * 2;
          dlc.height      = besrect.h + besrect.y * 2;
          dlc.buffermode  = buffermode;

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

          bes->SetScreenLocation( bes, 0.0, 0.0, 1.0, 1.0 );

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
      * CRTC1
      */
     if (use_crtc1) {
          dlc.flags      = DLCONF_BUFFERMODE;
          dlc.buffermode = buffermode;

          if ((res = crtc1->TestConfiguration( crtc1, &dlc, &failed )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Invalid CRTC1 configuration - %s!\n",
                       DirectFBErrorString( res ) );
               return -1;
          }
          if ((res = crtc1->SetConfiguration( crtc1, &dlc )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: CRTC1 configuration failed - %s!\n",
                       DirectFBErrorString( res ) );
               return -1;
          }
          if ((res = crtc1->GetConfiguration( crtc1, &dlc )) != DFB_OK) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "vo_dfbmga: Getting CRTC1 configuration failed - %s!\n",
                       DirectFBErrorString( res ) );
               return -1;
          }

          crtc1->GetSurface( crtc1, &c1frame );
          c1frame->SetBlittingFlags( c1frame, DSBLIT_NOFX );
          c1frame->SetColor( c1frame, 0, 0, 0, 0xff );

          c1frame->GetSize( c1frame, &screen_width, &screen_height );

          aspect_save_screenres( screen_width, screen_height );
          aspect( &out_width, &out_height, (flags & VOFLAG_FULLSCREEN) ? A_ZOOM : A_NOZOOM );

          if (in_width != out_width || in_height != out_height)
               c1stretch = 1;
          else
               c1stretch = 0;

          c1rect.x = (screen_width  - out_width)  / 2;
          c1rect.y = (screen_height - out_height) / 2;
          c1rect.w = out_width;
          c1rect.h = out_height;

          c1frame->Clear( c1frame, 0, 0, 0, 0xff );
          c1frame->Flip( c1frame, NULL, 0 );
          c1frame->Clear( c1frame, 0, 0, 0, 0xff );
          c1frame->Flip( c1frame, NULL, 0 );
          c1frame->Clear( c1frame, 0, 0, 0, 0xff );

          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: CRTC1 using %s buffering\n",
                  dlc.buffermode == DLBM_TRIPLE ? "triple" :
                  dlc.buffermode == DLBM_BACKVIDEO ? "double" : "single" );
          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: CRTC1 surface %dx%d %s\n", screen_width, screen_height, pixelformat_name( dlc.pixelformat ) );
     }

     /*
      * CRTC2
      */
     if (use_crtc2) {
          dlc.flags      = DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;
          dlc.buffermode = buffermode;
          dlc.options    = DLOP_NONE;

#if DIRECTFBVERSION > DFB_VERSION(0,9,16)
          if (field_parity != -1) {
               dlc.options |= DLOP_FIELD_PARITY;
          }
#endif
          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: Field parity set to: ");
          switch (field_parity) {
          case -1:
               mp_msg( MSGT_VO, MSGL_INFO, "Don't care\n");
               break;
          case 0:
               mp_msg( MSGT_VO, MSGL_INFO, "Top field first\n");
               break;
          case 1:
               mp_msg( MSGT_VO, MSGL_INFO, "Bottom field first\n");
               break;
          }

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

#if DIRECTFBVERSION > DFB_VERSION(0,9,16)
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
          aspect( &out_width, &out_height, (flags & VOFLAG_FULLSCREEN) ? A_ZOOM : A_NOZOOM );

          if (in_width != out_width ||
              in_height != out_height)
               c2stretch = 1;
          else
               c2stretch = 0;

          c2rect.x = (screen_width  - out_width)  / 2;
          c2rect.y = (screen_height - out_height) / 2;
          c2rect.w = out_width;
          c2rect.h = out_height;

          c2frame->Clear( c2frame, 0, 0, 0, 0xff );
          c2frame->Flip( c2frame, NULL, 0 );
          c2frame->Clear( c2frame, 0, 0, 0, 0xff );
          c2frame->Flip( c2frame, NULL, 0 );
          c2frame->Clear( c2frame, 0, 0, 0, 0xff );

          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: CRTC2 using %s buffering\n",
                  dlc.buffermode == DLBM_TRIPLE ? "triple" :
                  dlc.buffermode == DLBM_BACKVIDEO ? "double" : "single" );
          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: CRTC2 surface %dx%d %s\n", screen_width, screen_height, pixelformat_name( dlc.pixelformat ) );
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

          dlc.flags       = DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
          dlc.pixelformat = DSPF_ALUT44;
          dlc.buffermode  = buffermode;

#if DIRECTFBVERSION > DFB_VERSION(0,9,16)
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

          spic->GetSurface( spic, &spicframe );

          spicframe->GetPalette( spicframe, &palette );
          color.a = 0xff;
          for (i = 0; i < 16; i++) {
               color.r = i * 17;
               color.g = i * 17;
               color.b = i * 17;
               palette->SetEntries( palette, &color, 1, i );
          }
          palette->Release( palette );

          spicframe->Clear( spicframe, 0, 0, 0, 0 );
          spicframe->Flip( spicframe, NULL, 0 );
          spicframe->Clear( spicframe, 0, 0, 0, 0 );
          spicframe->Flip( spicframe, NULL, 0 );
          spicframe->Clear( spicframe, 0, 0, 0, 0 );

          mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: Sub-picture layer using %s buffering\n",
                  dlc.buffermode == DLBM_TRIPLE ? "triple" :
                  dlc.buffermode == DLBM_BACKVIDEO ? "double" : "single" );

          subframe = spicframe;
          subrect = NULL;
     } else if (use_crtc2) {
          /* Draw OSD to CRTC2 surface */
          subframe = c2frame;
          subrect = &c2rect;
     } else if (use_crtc1) {
          /* Draw OSD to CRTC1 surface */
          subframe = c1frame;
          subrect = &c1rect;
     } else {
          /* Draw OSD to BES surface */
          subframe = besframe;
          subrect = &besrect;
     }

     subframe->GetSize( subframe, &sub_width, &sub_height );
     subframe->GetPixelFormat( subframe, &subframe_format );
     mp_msg( MSGT_VO, MSGL_INFO, "vo_dfbmga: Sub-picture surface %dx%d %s (%s)\n",
             sub_width, sub_height,
             pixelformat_name( subframe_format ),
             use_crtc2 ? (use_spic ? "Sub-picture layer" : "CRTC2") :
             use_crtc1 ? "CRTC1" : "BES" );

     osd_dirty = 0;
     osd_current = 1;
     blit_done = 0;

     return 0;
}

static int
query_format( uint32_t format )
{
     switch (format) {
     case IMGFMT_YV12:
     case IMGFMT_I420:
     case IMGFMT_IYUV:
          if (is_g200 || use_crtc1)
               return 0;
          break;
     case IMGFMT_BGR32:
     case IMGFMT_BGR16:
     case IMGFMT_BGR15:
          if (is_g200 && use_bes)
               return 0;
          break;
     case IMGFMT_UYVY:
          if (is_g200)
               return 0;
          break;
     case IMGFMT_YUY2:
          break;
#if DIRECTFBVERSION > DFB_VERSION(0,9,21)
     case IMGFMT_NV12:
     case IMGFMT_NV21:
          if (use_crtc1 || use_crtc2)
               return 0;
          break;
#endif
     default:
          return 0;
     }

     return  VFCAP_HWSCALE_UP |
             VFCAP_HWSCALE_DOWN |
             VFCAP_CSP_SUPPORTED_BY_HW |
             VFCAP_CSP_SUPPORTED |
             VFCAP_OSD;
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
     if (use_spic && !flipping && vo_osd_changed_flag)
          subframe->FillRectangle( subframe, x0, y0, w, h );
}

static void
draw_alpha( int x0, int y0,
            int w, int h,
            unsigned char *src,
            unsigned char *srca,
            int stride )
{
     uint8_t *dst;
     void *ptr;
     int pitch;

     if (use_spic) {
          if (!osd_changed || (!flipping && !vo_osd_changed_flag))
               return;
          osd_dirty |= osd_current;
     } else {
          if (x0 < subrect->x ||
              y0 < subrect->y ||
              x0 + w > subrect->x + subrect->w ||
              y0 + h > subrect->y + subrect->h)
               osd_dirty |= osd_current;
     }

     if (subframe->Lock( subframe, DSLF_READ | DSLF_WRITE, &ptr, &pitch ) != DFB_OK)
          return;
     dst = ptr;

     switch (subframe_format) {
     case DSPF_ALUT44:
          vo_draw_alpha_alut44( w, h, src, srca, stride,
                                dst + pitch * y0 + x0,
                                pitch );
          break;
     case DSPF_RGB32:
     case DSPF_ARGB:
          vo_draw_alpha_rgb32( w, h, src, srca, stride,
                               dst + pitch * y0 + 4 * x0,
                               pitch );
          break;
     case DSPF_RGB16:
          vo_draw_alpha_rgb16( w, h, src, srca, stride,
                               dst + pitch * y0 + 2 * x0,
                               pitch );
          break;
     case DSPF_ARGB1555:
          vo_draw_alpha_rgb15( w, h, src, srca, stride,
                               dst + pitch * y0 + 2 * x0,
                               pitch );
          break;
     case DSPF_YUY2:
          vo_draw_alpha_yuy2( w, h, src, srca, stride,
                              dst + pitch * y0 + 2 * x0,
                              pitch );
          break;
     case DSPF_UYVY:
          vo_draw_alpha_yuy2( w, h, src, srca, stride,
                              dst + pitch * y0 + 2 * x0 + 1,
                              pitch );
          break;
#if DIRECTFBVERSION > DFB_VERSION(0,9,21)
     case DSPF_NV12:
     case DSPF_NV21:
#endif
     case DSPF_I420:
     case DSPF_YV12:
          vo_draw_alpha_yv12( w, h, src, srca, stride,
                              dst + pitch * y0 + x0,
                              pitch );
          break;
     }

     subframe->Unlock( subframe );
}

static int
draw_frame( uint8_t * src[] )
{
     return -1;
}

static int
draw_slice( uint8_t * src[], int stride[], int w, int h, int x, int y )
{
     uint8_t *dst;
     void *ptr;
     int pitch;

     if (frame->Lock( frame, DSLF_WRITE, &ptr, &pitch ) != DFB_OK)
          return VO_FALSE;
     dst = ptr;

     memcpy_pic( dst + pitch * y + x, src[0],
                 w, h, pitch, stride[0] );

     dst += pitch * buf_height;

     y /= 2;
     h /= 2;

#if DIRECTFBVERSION > DFB_VERSION(0,9,21)
     if (frame_format == DSPF_NV12 || frame_format == DSPF_NV21) {
          memcpy_pic( dst + pitch * y + x, src[1],
                      w, h, pitch, stride[1] );
     } else
#endif
     {
          x /= 2;
          w /= 2;
          pitch /= 2;

          if (frame_format == DSPF_I420 )
               memcpy_pic( dst + pitch * y + x, src[1],
                           w, h, pitch, stride[1] );
          else
               memcpy_pic( dst + pitch * y + x, src[2],
                           w, h, pitch, stride[2] );

          dst += pitch * buf_height / 2;

          if (frame_format == DSPF_I420 )
               memcpy_pic( dst + pitch * y + x, src[2],
                           w, h, pitch, stride[2] );
          else
               memcpy_pic( dst + pitch * y + x, src[1],
                           w, h, pitch, stride[1] );
     }

     frame->Unlock( frame );

     return VO_TRUE;
}

static void
blit_to_screen( void )
{
     IDirectFBSurface *blitsrc = frame;
     DFBRectangle *srect = NULL;

     if (use_bes) {
#if DIRECTFBVERSION > DFB_VERSION(0,9,15)
          if (vo_vsync && !flipping)
               bes->WaitForSync( bes );
#endif

          besframe->Blit( besframe, blitsrc, NULL, besrect.x, besrect.y );
          blitsrc = besframe;
          srect = &besrect;
     }

     if (use_crtc1) {
#if DIRECTFBVERSION > DFB_VERSION(0,9,15)
          if (vo_vsync && !flipping)
               crtc1->WaitForSync( crtc1 );
#endif

          if (c1stretch)
               c1frame->StretchBlit( c1frame, blitsrc, srect, &c1rect );
          else
               c1frame->Blit( c1frame, blitsrc, srect, c1rect.x, c1rect.y );
     }

     if (use_crtc2) {
#if DIRECTFBVERSION > DFB_VERSION(0,9,15)
          if (vo_vsync && !flipping)
               crtc2->WaitForSync( crtc2 );
#endif

          if (c2stretch)
               c2frame->StretchBlit( c2frame, blitsrc, srect, &c2rect );
          else
               c2frame->Blit( c2frame, blitsrc, srect, c2rect.x, c2rect.y );
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
          } else {
               /* Clear black bars around the picture */
               subframe->FillRectangle( subframe,
                                        0, 0,
                                        sub_width, subrect->y );
               subframe->FillRectangle( subframe,
                                        0, subrect->y + subrect->h,
                                        sub_width, subrect->y );
               subframe->FillRectangle( subframe,
                                        0, subrect->y,
                                        subrect->x, subrect->h );
               subframe->FillRectangle( subframe,
                                        subrect->x + subrect->w, subrect->y,
                                        subrect->x, subrect->h );
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
          if (use_crtc1)
               c1frame->Flip( c1frame, NULL, vo_vsync ? DSFLIP_WAITFORSYNC : DSFLIP_ONSYNC );
          if (use_bes)
               besframe->Flip( besframe, NULL, vo_vsync ? DSFLIP_WAITFORSYNC : DSFLIP_ONSYNC );

          if (!use_spic) {
               osd_current <<= 1;
               if (osd_current > osd_max)
                    osd_current = 1;
          }
     }

     blit_done = 0;
     current_buf = 0;
}

static void
uninit( void )
{
     release_config();

     if (buffer)
          buffer->Release( buffer );
     if (remote)
          remote->Release( remote );
     if (keyboard)
          keyboard->Release( keyboard );
     if (crtc2)
          crtc2->Release( crtc2 );
     if (bes)
          bes->Release( bes );
     if (crtc1)
          crtc1->Release( crtc1 );
     if (dfb)
          dfb->Release( dfb );

     buffer = NULL;
     remote = NULL;
     keyboard = NULL;
     crtc2 = NULL;
     bes = NULL;
     crtc1 = NULL;
     dfb = NULL;
}

static uint32_t
get_image( mp_image_t *mpi )
{
     int buf = current_buf;
     uint8_t *dst;
     void *ptr;
     int pitch;

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
                      &ptr, &pitch ) != DFB_OK)
          return VO_FALSE;
     dst = ptr;

     if ((mpi->width == pitch) ||
         (mpi->flags & (MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_ACCEPT_WIDTH))) {

          mpi->planes[0] = dst;
          mpi->width     = in_width;
          mpi->stride[0] = pitch;

          if (mpi->flags & MP_IMGFLAG_PLANAR) {
               if (mpi->num_planes > 2) {
                    mpi->stride[1] = mpi->stride[2] = pitch / 2;

                    if (mpi->flags & MP_IMGFLAG_SWAPPED) {
                         /* I420 */
                         mpi->planes[1] = dst + buf_height * pitch;
                         mpi->planes[2] = mpi->planes[1] + buf_height * pitch / 4;
                    } else {
                         /* YV12 */
                         mpi->planes[2] = dst + buf_height * pitch;
                         mpi->planes[1] = mpi->planes[2] + buf_height * pitch / 4;
                    }
               } else {
                    /* NV12/NV21 */
                    mpi->stride[1] = pitch;
                    mpi->planes[1] = dst + buf_height * pitch;
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
     else if (use_crtc1)
          res = crtc1->SetColorAdjustment( crtc1, &ca );
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
     else if (use_crtc1)
          res = crtc1->GetColorAdjustment( crtc1, &ca );
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

static int
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

#include "osdep/keycodes.h"

static void
check_events( void )
{
     DFBInputEvent event;

     if (!buffer)
          return;

     if (buffer->GetEvent( buffer, DFB_EVENT( &event )) == DFB_OK) {
          if (event.type == DIET_KEYPRESS) {
               switch (event.key_symbol) {
               case DIKS_ESCAPE:
                    mplayer_put_key( KEY_ESC );
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

               case DIKS_POWER:
                    mplayer_put_key( KEY_POWER );
                    break;
               case DIKS_MENU:
                    mplayer_put_key( KEY_MENU );
                    break;
               case DIKS_PLAY:
                    mplayer_put_key( KEY_PLAY );
                    break;
               case DIKS_STOP:
                    mplayer_put_key( KEY_STOP );
                    break;
               case DIKS_PAUSE:
                    mplayer_put_key( KEY_PAUSE );
                    break;
               case DIKS_PLAYPAUSE:
                    mplayer_put_key( KEY_PLAYPAUSE );
                    break;
               case DIKS_FORWARD:
                    mplayer_put_key( KEY_FORWARD );
                    break;
               case DIKS_NEXT:
                    mplayer_put_key( KEY_NEXT );
                    break;
               case DIKS_REWIND:
                    mplayer_put_key( KEY_REWIND );
                    break;
               case DIKS_PREVIOUS:
                    mplayer_put_key( KEY_PREV );
                    break;
               case DIKS_VOLUME_UP:
                    mplayer_put_key( KEY_VOLUME_UP );
                    break;
               case DIKS_VOLUME_DOWN:
                    mplayer_put_key( KEY_VOLUME_DOWN );
                    break;
               case DIKS_MUTE:
                    mplayer_put_key( KEY_MUTE );
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
