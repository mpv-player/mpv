/*
 * MPlayer video driver for DirectFramebuffer device
 *
 * copyright (C) 2002 Jiri Svoboda <Jiri.Svoboda@seznam.cz>
 *
 * based on vo_directfb2.c
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

// directfb includes

#include <directfb.h>

#define DFB_VERSION(a,b,c) (((a)<<16)|((b)<<8)|(c))

// other things

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <sys/kd.h>
#else
#include <linux/kd.h>
#endif

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "sub.h"
#include "mp_msg.h"
#include "aspect.h"
#include "subopt-helper.h"
#include "mp_fifo.h"

#ifndef min
#define min(x,y) (((x)<(y))?(x):(y))
#endif

#if DIRECTFBVERSION > DFB_VERSION(0,9,17)
// triple buffering
#define TRIPLE 1
#endif

static const vo_info_t info = {
	"Direct Framebuffer Device",
	"directfb",
	"Jiri Svoboda Jiri.Svoboda@seznam.cz",
	"v 2.0 (for DirectFB version >=0.9.13)"
};

const LIBVO_EXTERN(directfb)

/******************************
*      vo_directfb globals    *
******************************/

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
 *  filled by preinit
 */

// main DirectFB handle
static IDirectFB *dfb = NULL;
// keyboard handle
static IDirectFBInputDevice *keyboard = NULL;
// A buffer for input events.
static IDirectFBEventBuffer *buffer = NULL;

 /*
 *  filled during config
 */

// handle of used layer
static IDirectFBDisplayLayer *layer = NULL;
// surface of used layer
static IDirectFBSurface *primary = NULL;
static int primarylocked = 0;
// handle of temporary surface (if used)
static IDirectFBSurface *frame = NULL;
static int framelocked = 0;
// flipping mode flag (layer/surface)
static int flipping = 0; 
// scaling flag
static int stretch = 0;
// picture position
static int xoffset=0,yoffset=0;
// picture size
static int out_width=0,out_height=0;
// frame/primary size
static int width=0,height=0;
// frame primary format
DFBSurfacePixelFormat pixel_format;
/*
static void (*draw_alpha_p)(int w, int h, unsigned char *src,
		unsigned char *srca, int stride, unsigned char *dst,
		int dstride);
*/

/******************************
* cmd line parameteres        *
******************************/

/* command line/config file options */
static int layer_id = -1;
static int buffer_mode = 1;
static int use_input = 1;
static int field_parity = -1;

/******************************
*	   implementation     *
******************************/

void unlock(void) {
if (frame && framelocked) frame->Unlock(frame);
if (primary && primarylocked) primary->Unlock(primary);
}

static int get_parity(strarg_t *arg) {
  if (strargcmp(arg, "top") == 0)
    return 0;
  if (strargcmp(arg, "bottom") == 0)
    return 1;
  return -1;
}

static int check_parity(void *arg) {
  return get_parity(arg) != -1;
}

static int get_mode(strarg_t *arg) {
  if (strargcmp(arg, "single") == 0)
    return 1;
  if (strargcmp(arg, "double") == 0)
    return 2;
  if (strargcmp(arg, "triple") == 0)
    return 3;
  return 0;
}

static int check_mode(void *arg) {
  return get_mode(arg) != 0;
}

static int preinit(const char *arg)
{
    DFBResult ret;
    strarg_t mode_str = {0, NULL};
    strarg_t par_str = {0, NULL};
    strarg_t dfb_params = {0, NULL};
    opt_t subopts[] = {
      {"input",       OPT_ARG_BOOL, &use_input,  NULL},
      {"buffermode",  OPT_ARG_STR,  &mode_str,   check_mode},
      {"fieldparity", OPT_ARG_STR,  &par_str,    check_parity},
      {"layer",       OPT_ARG_INT,  &layer_id,   NULL},
      {"dfbopts",     OPT_ARG_STR,  &dfb_params, NULL},
      {NULL}
    };

    //mp_msg(MSGT_VO, MSGL_INFO,"DirectFB: Preinit entered\n");

    if (dfb) return 0; // we are already initialized!

    // set defaults
    buffer_mode = 1 + vo_doublebuffering; // honor -double switch
    layer_id = -1;
    use_input = 1;
    field_parity = -1;
     if (subopt_parse(arg, subopts) != 0) {
               mp_msg( MSGT_VO, MSGL_ERR,
                       "\n-vo directfb command line help:\n"
                       "Example: mplayer -vo directfb:layer=1:buffermode=single\n"
                       "\nOptions (use 'no' prefix to disable):\n"
                       "  input  Use DirectFB for keyboard input\n"
                       "\nOther options:\n"
                       "  layer=n\n"
                       "    n=0..xx   Use layer with id n for output (0=primary)\n"
                       "  buffermode=(single|double|triple)\n"
                       "    single   Use single buffering\n"
                       "    double   Use double buffering\n"
                       "    triple   Use triple buffering\n"
                       "  fieldparity=(top|bottom)\n"
                       "    top      Top field first\n"
                       "    bottom   Bottom field first\n"
		       "  dfbopts=<str>\n"
		       "    Specify a parameter list for DirectFB\n"
                       "\n" );
               return -1;
     }
    if (mode_str.len)
      buffer_mode = get_mode(&mode_str);
    if (par_str.len)
      field_parity = get_parity(&par_str);


	if (dfb_params.len > 0)
	{
	    int argc = 2;
	    char arg0[10] = "mplayer";
	    char *arg1 = malloc(dfb_params.len + 7);
	    char* argv[3];
	    char ** a;
	    
	    a = &argv[0];
	    
	    strcpy(arg1, "--dfb:");
	    strncat(arg1, dfb_params.str, dfb_params.len);

	    argv[0]=arg0;
	    argv[1]=arg1;
	    argv[2]=NULL;
	    
    	    DFBCHECK (DirectFBInit (&argc,&a));

	    free(arg1);
	} else {
	
        DFBCHECK (DirectFBInit (NULL,NULL));
	}

	if (((directfb_major_version <= 0) &&
	    (directfb_minor_version <= 9) &&
	    (directfb_micro_version < 13)))
	{
	    mp_msg(MSGT_VO, MSGL_ERR,"DirectFB: Unsupported DirectFB version\n");	
	    return 1;
	}

  /*
   * (set options)
   */
	
//	uncomment this if you do not wish to create a new VT for DirectFB
//        DFBCHECK (DirectFBSetOption ("no-vt-switch",""));

//	uncomment this if you want to allow VT switching
//       DFBCHECK (DirectFBSetOption ("vt-switching",""));

//	uncomment this if you want to hide gfx cursor (req dfb >=0.9.9)
        DFBCHECK (DirectFBSetOption ("no-cursor",""));

// bg color fix
        DFBCHECK (DirectFBSetOption ("bg-color","00000000"));

  /*
   * (Initialize)
   */

        DFBCHECK (DirectFBCreate (&dfb));

#if DIRECTFBVERSION < DFB_VERSION(0,9,17)
        if (DFB_OK != dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN)) {
            mp_msg(MSGT_VO, MSGL_WARN,"DirectFB: Warning - cannot switch to fullscreen mode");
        };
#endif
	
  /*
   * (Get keyboard)
   */
    
  if (use_input) {
    ret = dfb->GetInputDevice (dfb, DIDID_KEYBOARD, &keyboard);
    if (ret==DFB_OK) {
	mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Keyboard init OK\n");
    } else {
	keyboard = NULL;
	mp_msg(MSGT_VO, MSGL_ERR,"DirectFB: Keyboard init FAILED\n");
    }
  } 


  /*
   * Create an input buffer for the keyboard.
   */
  if (keyboard) DFBCHECK (keyboard->CreateEventBuffer (keyboard, &buffer));

  // just to start clean ...
  if (buffer) buffer->Reset(buffer);

  //mp_msg(MSGT_VO, MSGL_INFO,"DirectFB: Preinit OK\n");

  return 0;

}

DFBSurfacePixelFormat convformat(uint32_t format)
{
// add more formats !!!
	switch (format) {
            case IMGFMT_RGB32: return  DSPF_RGB32; break;
	    case IMGFMT_BGR32: return  DSPF_RGB32; break;
    	    case IMGFMT_RGB24: return  DSPF_RGB24; break;
	    case IMGFMT_BGR24: return  DSPF_RGB24; break;
            case IMGFMT_RGB16: return  DSPF_RGB16; break;
            case IMGFMT_BGR16: return  DSPF_RGB16; break;
#if DIRECTFBVERSION > DFB_VERSION(0,9,15)
            case IMGFMT_RGB15: return  DSPF_ARGB1555; break;
            case IMGFMT_BGR15: return  DSPF_ARGB1555; break;
#else
            case IMGFMT_RGB15: return  DSPF_RGB15; break;
            case IMGFMT_BGR15: return  DSPF_RGB15; break;
#endif
            case IMGFMT_YUY2:  return  DSPF_YUY2; break;
            case IMGFMT_UYVY:  return  DSPF_UYVY; break;
    	    case IMGFMT_YV12:  return  DSPF_YV12; break;
    	    case IMGFMT_I420:  return  DSPF_I420; break;
//    	    case IMGFMT_IYUV:  return  DSPF_IYUV; break;
    	    case IMGFMT_RGB8:  return  DSPF_RGB332; break;
    	    case IMGFMT_BGR8:  return  DSPF_RGB332; break;
	
	    default: return 0;
	}
return 0;	
}

typedef struct enum1_s {
uint32_t format;
int scale;
int result;
unsigned int id;
unsigned int width;
unsigned int height;
int setsize;
} enum1_t;

DFBEnumerationResult test_format_callback( unsigned int                 id,
                                           DFBDisplayLayerDescription  desc,
                                           void *data)
{
     enum1_t *params =(enum1_t *)data;
     IDirectFBDisplayLayer *layer;
     DFBResult ret;
    
     if ((layer_id == -1 )||(layer_id == id)) {
    
     ret = dfb->GetDisplayLayer( dfb, id, &layer);
     if (ret) {
               DirectFBError( "dfb->GetDisplayLayer failed", ret );
	       return DFENUM_OK;
     } else {
        DFBDisplayLayerConfig        dlc;

	if (params->setsize) {
    	    dlc.flags	= DLCONF_WIDTH |DLCONF_HEIGHT;
	    dlc.width	= params->width;
	    dlc.height	= params->height;
	    layer->SetConfiguration(layer,&dlc);
	}


        dlc.flags       = DLCONF_PIXELFORMAT;
	dlc.pixelformat = convformat(params->format);

	layer->SetOpacity(layer,0);
             
	ret = layer->TestConfiguration(layer,&dlc,NULL);
 
        layer->Release(layer);

	mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Test format - layer %i scale/pos %i\n",id,(desc.caps & DLCAPS_SCREEN_LOCATION));
     
	if (ret==DFB_OK) {
//	    printf("Test OK\n");     
	    if (params->result) {
	        if  ((!params->scale) && (desc.caps & DLCAPS_SCREEN_LOCATION)) {
		    params->scale=1;
		    params->id=id;
		    mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Test format - added layer %i scale/pos %i\n",id,(desc.caps & DLCAPS_SCREEN_LOCATION));
		}
	    } else {
		params->result=1;
		params->id=id;
		if (desc.caps & DLCAPS_SCREEN_LOCATION) params->scale=1;
		mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Test format - added layer %i scale/pos %i\n",id,(desc.caps & DLCAPS_SCREEN_LOCATION));
	   };
	};
     };

    };

    return DFENUM_OK;
}

static int query_format(uint32_t format)
{
	int ret = VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_OSD; // osd should be removed the in future -> will be handled outside...
	enum1_t params;


	if (!convformat(format)) return 0;
// temporarily disable YV12
//	if (format == IMGFMT_YV12) return 0;
//	if (format == IMGFMT_I420) return 0;
	if (format == IMGFMT_IYUV) return 0;
	
	mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Format query: %s\n",vo_format_name(format));

	params.format=format;
	params.scale=0;
	params.result=0;
	params.setsize=0;

        DFBCHECK (dfb->EnumDisplayLayers(dfb,test_format_callback,&params));

	if (params.result) {
	    if (params.scale) ret |=VFCAP_HWSCALE_UP|VFCAP_HWSCALE_DOWN;
	    return ret;
	}

	return 0;
}

typedef struct videomode_s {
int width;
int height;
int out_width;
int out_height;
int overx;
int overy;
int bpp;
} videomode_t;


DFBEnumerationResult video_modes_callback( int width,int height,int bpp, void *data)
{
     videomode_t *params =(videomode_t *)data;

int overx=0,overy=0,closer=0,over=0;
int we_are_under=0;

//mp_msg(MSGT_VO, MSGL_INFO,"DirectFB: Validator entered %i %i %i\n",width,height,bpp);

overx=width-params->out_width;
overy=height-params->out_height;

if (!params->width) {
        params->width=width;
        params->height=height;
	params->overx=overx;
	params->overy=overy;
	mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Mode added %i %i %i\n",width,height,bpp);
}

if ((params->overy<0)||(params->overx<0)) we_are_under=1; // stored mode is smaller than req mode
if (abs(overx*overy)<abs(params->overx * params->overy)) closer=1; // current mode is closer to desired res
if ((overx>=0)&&(overy>=0)) over=1; // current mode is bigger or equaul to desired res
if ((closer && (over || we_are_under)) || (we_are_under && over)) {
                params->width=width;
                params->height=height;
                params->overx=overx;
                params->overy=overy;
		mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Better mode added %i %i %i\n",width,height,bpp);
                };

return DFENUM_OK;
}

#define CONFIG_ERROR -1

static int config(uint32_t s_width, uint32_t s_height, uint32_t d_width,
		uint32_t d_height, uint32_t flags, char *title,
		uint32_t format)
{
  /*
   * (Locals)
   */

// decode flags

	int fs = flags & VOFLAG_FULLSCREEN;
        int vm = flags & VOFLAG_MODESWITCHING;

	DFBSurfaceDescription dsc;
        DFBResult             ret;
        DFBDisplayLayerConfig dlc;
	DFBSurfaceCapabilities caps;

	enum1_t params;

	mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Config entered [%ix%i]\n",s_width,s_height);
	mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: With requested format: %s\n",vo_format_name(format));
	
// initial cleanup
	if (frame) {
	    frame->Release(frame);
	    frame=NULL;
	}

	if (primary) {
	    primary->Release(primary);
	    primary=NULL;
	}

	if (layer) {
	    layer->Release(layer);
	    layer=NULL;
	}


// vm things

	if (vm) {
	    videomode_t params;
	    params.out_width=d_width;
	    params.out_height=d_height;
	    params.width=0;
	    params.height=0;
	    switch (format) {
		    case IMGFMT_RGB32:
    		    case IMGFMT_BGR32:
					params.bpp=32;
					break;
        	    case IMGFMT_RGB24:
		    case IMGFMT_BGR24:
					params.bpp=24;
					break;
		    case IMGFMT_RGB16:
    		    case IMGFMT_BGR16:
            	    case IMGFMT_RGB15:
	    	    case IMGFMT_BGR15:
					params.bpp=16;
					break;
		    default:		params.bpp=0;

	    }
	    mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Config - trying to change videomode\n");
            DFBCHECK (dfb->EnumVideoModes(dfb,video_modes_callback,&params));
	    ret=dfb->SetVideoMode(dfb,params.width,params.height,params.bpp);
	    if (ret) {
		ret=dfb->SetVideoMode(dfb,params.width,params.height,24);
		    if (ret) {
			ret=dfb->SetVideoMode(dfb,params.width,params.height,32);
			    if (ret) {
				ret=dfb->SetVideoMode(dfb,params.width,params.height,16);
				    if (ret) {
					ret=dfb->SetVideoMode(dfb,params.width,params.height,8);
				    }
			    }
		    }
	    }
	} // vm end

// just to be sure clear primary layer
#if DIRECTFBVERSION > DFB_VERSION(0,9,13)
        ret = dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer);
	if (ret==DFB_OK) {
	    ret = layer->GetSurface(layer,&primary);
	    if (ret==DFB_OK) {
		primary->Clear(primary,0,0,0,0xff);
		ret = primary->Flip(primary,NULL,0);
		if (ret==DFB_OK) { 
		    primary->Clear(primary,0,0,0,0xff);
		}
    	    primary->Release(primary);
	    }
	primary=NULL;
        layer->Release(layer);
	}
	layer=NULL;
#endif

// find best layer

        mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Config - looking for suitable layer\n");
	params.format=format;
	params.scale=0;
	params.result=0;
	params.width=s_width;
	params.height=s_height;
	params.setsize=1;

        DFBCHECK (dfb->EnumDisplayLayers(dfb,test_format_callback,&params));

	if (!params.result) {
	    mp_msg(MSGT_VO, MSGL_ERR,"DirectFB: ConfigError - no suitable layer found\n");
	    params.id = DLID_PRIMARY;
	}

	mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Config - layer %i\n",params.id);

	// setup layer

        DFBCHECK (dfb->GetDisplayLayer( dfb, params.id, &layer));
	
#if DIRECTFBVERSION > DFB_VERSION(0,9,16)
        mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Config - switching layer to exclusive mode\n");
	ret = layer->SetCooperativeLevel (layer, DLSCL_EXCLUSIVE);

        if (DFB_OK != ret) {
	    mp_msg(MSGT_VO, MSGL_WARN,"DirectFB: Warning - cannot switch layer to exclusive mode. This could cause\nproblems. You may need to select correct pixel format manually!\n");
	    DirectFBError("MPlayer - Switch layer to exlusive mode.",ret);
	};
#endif
	if (params.scale) {
            mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Config - changing layer configuration (size)\n");
            dlc.flags       = DLCONF_WIDTH | DLCONF_HEIGHT;
	    dlc.width       = s_width;
    	    dlc.height      = s_height;

	    ret = layer->SetConfiguration(layer,&dlc);

	    if (ret) {
		mp_msg(MSGT_VO, MSGL_ERR,"DirectFB: ConfigError in layer configuration (size)\n");
		DirectFBError("MPlayer - Layer size change.",ret);
	    };
	}

        // look if we need to change the pixel format of the layer
	// and just to be sure also fetch all layer properties
	dlc.flags       = DLCONF_PIXELFORMAT | DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_OPTIONS | DLCONF_BUFFERMODE;

	ret = layer->GetConfiguration(layer,&dlc);

	dlc.flags       = DLCONF_PIXELFORMAT | DLCONF_WIDTH | DLCONF_HEIGHT;

	if (ret) {
	    mp_msg(MSGT_VO, MSGL_WARN,"DirectFB: Warning - could not get layer properties!\n");
	} else {
	    mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Layer reports format:%x\n",dlc.pixelformat);
	}

	if ((dlc.pixelformat != convformat(params.format)) || (ret != DFB_OK)) {

    	    dlc.flags       = DLCONF_PIXELFORMAT;
	    dlc.pixelformat = convformat(params.format);

	    mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Desired pixelformat: %x\n",dlc.pixelformat);

    	    mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Config - changing layer configuration (format)\n");
	    ret = layer->SetConfiguration(layer,&dlc);

	    if (ret) {
		unsigned int bpp;
		mp_msg(MSGT_VO, MSGL_ERR,"DirectFB: ConfigError in layer configuration (format, flags=%x)\n",dlc.flags);
		DirectFBError("MPlayer - layer pixelformat change",ret);

		// ugly fbdev workaround - try to switch pixelformat via videomode change
		switch (dlc.pixelformat) {
		    case DSPF_ARGB: 
		    case DSPF_RGB32: bpp=32;break;
    		    case DSPF_RGB24: bpp=24;break;
	            case DSPF_RGB16: bpp=16;break;
#if DIRECTFBVERSION > DFB_VERSION(0,9,15)
    		    case DSPF_ARGB1555: bpp=15;break;
#else
        	    case DSPF_RGB15: bpp=15;break;
#endif
		    case DSPF_RGB332 : bpp=8;break;
		}
		
		switch (dlc.pixelformat) {
		    case DSPF_ARGB:
		    case DSPF_RGB32:
    		    case DSPF_RGB24:
	            case DSPF_RGB16:
#if DIRECTFBVERSION > DFB_VERSION(0,9,15)
    		    case DSPF_ARGB1555:
#else
        	    case DSPF_RGB15:
#endif
		    case DSPF_RGB332:
				    mp_msg(MSGT_VO, MSGL_V,"DirectFB: Trying to recover via videomode change (VM).\n");
				    // get size
		        	    dlc.flags = DLCONF_WIDTH | DLCONF_HEIGHT;
				    if (DFB_OK==layer->GetConfiguration(layer,&dlc)) {
					// try to set videomode
				        mp_msg(MSGT_VO, MSGL_V,"DirectFB: Videomode  %ix%i BPP %i\n",dlc.width,dlc.height,bpp);
				    	ret = dfb->SetVideoMode(dfb,dlc.width,dlc.height,bpp);
					if (ret) DirectFBError("MPlayer - VM - pixelformat change",ret);

				    };

				    //get current pixel format
				    dlc.flags       = DLCONF_PIXELFORMAT;
	    			    ret = layer->GetConfiguration(layer,&dlc);
				    if (ret) {
					DirectFBError("MPlayer - VM - Layer->GetConfiguration",ret);
					} else {
				        mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Layer now has pixelformat [%x]\n",dlc.pixelformat);
					};

				    // check if we were succesful
				    if ((dlc.pixelformat != convformat(params.format)) || (ret != DFB_OK)) {
				        mp_msg(MSGT_VO, MSGL_INFO,"DirectFB: Recovery failed!.\n");
					return CONFIG_ERROR;
					}

				    break;
				    
		    default: return CONFIG_ERROR;	
			
		};
	    };
	};

// flipping of layer
// try triple, double... buffering

	dlc.flags = DLCONF_BUFFERMODE;
#ifdef TRIPLE
	if (buffer_mode > 2) {
	    dlc.buffermode = DLBM_TRIPLE;
	    ret = layer->SetConfiguration( layer, &dlc );
	} else { 
	    ret=!DFB_OK;
	}
	
	if (ret!=DFB_OK) {
#endif
	    if (buffer_mode > 1) {
		    dlc.buffermode = DLBM_BACKVIDEO;
    		    ret = layer->SetConfiguration( layer, &dlc );
    		if (ret!=DFB_OK) {
    		    dlc.buffermode = DLBM_BACKSYSTEM;
		    ret = layer->SetConfiguration( layer, &dlc );
		}
	    }
	    if (ret == DFB_OK) {
		mp_msg(MSGT_VO, MSGL_V,"DirectFB: Double buffering is active\n");
	    }
#ifdef TRIPLE
	} else { 
	    mp_msg(MSGT_VO, MSGL_V,"DirectFB: Triple buffering is active\n");
	}
#endif

#if DIRECTFBVERSION > DFB_VERSION(0,9,16)
        if (field_parity != -1) {
	    dlc.flags = DLCONF_OPTIONS;
	    ret = layer->GetConfiguration( layer, &dlc );
	    if (ret==DFB_OK) {
               dlc.options |= DLOP_FIELD_PARITY;
	       ret = layer->SetConfiguration( layer, &dlc );
		if (ret==DFB_OK) {
            	    layer->SetFieldParity( layer, field_parity );
		}
	    }
        }
	mp_msg( MSGT_VO, MSGL_DBG2, "DirectFB: Requested field parity: ");
          switch (field_parity) {
            case -1:
                mp_msg( MSGT_VO, MSGL_DBG2, "Don't care\n");
                break;
            case 0:
                mp_msg( MSGT_VO, MSGL_DBG2, "Top field first\n");
                break;
            case 1:
                mp_msg( MSGT_VO, MSGL_DBG2, "Bottom field first\n");
                break;
          }
	
#endif


// get layer surface
	
	ret = layer->GetSurface(layer,&primary);
	
	if (ret) {
	    mp_msg(MSGT_VO, MSGL_ERR,"DirectFB: ConfigError - could not get surface\n");
	    return CONFIG_ERROR; // what shall we report on failure?
	}

// test surface for flipping	
	DFBCHECK(primary->GetCapabilities(primary,&caps));
#if DIRECTFBVERSION > DFB_VERSION(0,9,13)
	primary->Clear(primary,0,0,0,0xff);
#endif	
        flipping = 0;
	if (caps & (DSCAPS_FLIPPING
#ifdef TRIPLE
	| DSCAPS_TRIPLE
#endif
	)) {
	    ret = primary->Flip(primary,NULL,0);
	    if (ret==DFB_OK) { 
		flipping = 1; 
#if DIRECTFBVERSION > DFB_VERSION(0,9,13)
		primary->Clear(primary,0,0,0,0xff);
#ifdef TRIPLE
// if we have 3 buffers clean once more
	if (caps & DSCAPS_TRIPLE) {
		primary->Flip(primary,NULL,0);
		primary->Clear(primary,0,0,0,0xff);
		flipping = 2; 
	}
#endif
#endif	
	    } 
	};

        mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Config - flipping = %i\n",flipping);

// is scale needed ? Aspect ratio and layer pos/size
	

	// get surface size
	DFBCHECK(primary->GetSize(primary,&width,&height));

        mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Config - surface size = %ix%i\n",width,height);

	aspect_save_orig(s_width,s_height);
	aspect_save_prescale(d_width,d_height);
	if (params.scale) {
		aspect_save_screenres(10000,10000);
		aspect(&out_width,&out_height,A_ZOOM);

                ret = layer->SetScreenLocation(layer,(1-(float)out_width/10000)/2,(1-(float)out_height/10000)/2,((float)out_width/10000),((float)out_height/10000));

		if (ret) mp_msg(MSGT_VO, MSGL_ERR,"DirectFB: ConfigError in layer configuration (position)\n");

		xoffset = 0;
		yoffset = 0;

	} else {

		aspect_save_screenres(width,height);
	
		if(fs) /* -fs */
			aspect(&out_width,&out_height,A_ZOOM);
		else
			aspect(&out_width,&out_height,A_NOZOOM);


    		xoffset = (width - out_width) / 2;
	        yoffset = (height - out_height) / 2;
	}

	if (((s_width==out_width)&&(s_height==out_height)) || (params.scale)) {
	    stretch = 0;
	} else {
	    stretch = 1;
	}

	
// temporary buffer in case of not flipping or scaling
	if ((!flipping) || stretch) {

	    DFBCHECK (primary->GetPixelFormat (primary, &dsc.pixelformat));

	    dsc.flags = DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_WIDTH;

	    dsc.width = s_width;
	    dsc.height = s_height;
	    
	    DFBCHECK (dfb->CreateSurface( dfb, &dsc, &frame));
	    DFBCHECK(frame->GetSize(frame,&width,&height));
	    mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Frame is active.\n");
	} 

// get format for draw_alpha - should be removed soon - osd will be rendered outside vo driver
	if (frame) {
		DFBCHECK (frame->GetPixelFormat(frame,&pixel_format));
        } else {
		DFBCHECK (primary->GetPixelFormat(primary,&pixel_format));
        };

 // finally turn on layer
 layer->SetOpacity(layer,255);

 //mp_msg(MSGT_VO, MSGL_INFO,"DirectFB: Config finished [%ix%i] - [%ix%i]\n",out_width,out_height,width,height);

return 0;
}

#include "osdep/keycodes.h"

static void check_events(void)
{

if (buffer) {

      DFBInputEvent event;

//if ( mp_msg_test(MSGT_VO,MSGL_V) ) printf ("DirectFB: Check events entered\n");
     if (buffer->GetEvent(buffer, DFB_EVENT (&event)) == DFB_OK) {

     if (event.type == DIET_KEYPRESS) { 
    		switch (event.key_symbol) {
                                case DIKS_ESCAPE:
					mplayer_put_key(KEY_ESC);
				break;
				case DIKS_PAGE_UP: mplayer_put_key(KEY_PAGE_UP);break;
				case DIKS_PAGE_DOWN: mplayer_put_key(KEY_PAGE_DOWN);break;
                                case DIKS_CURSOR_UP: mplayer_put_key(KEY_UP);break;
                                case DIKS_CURSOR_DOWN: mplayer_put_key(KEY_DOWN);break;
                                case DIKS_CURSOR_LEFT: mplayer_put_key(KEY_LEFT);break;
                                case DIKS_CURSOR_RIGHT: mplayer_put_key(KEY_RIGHT);break;
				case DIKS_INSERT: mplayer_put_key(KEY_INSERT);break;
				case DIKS_DELETE: mplayer_put_key(KEY_DELETE);break;
				case DIKS_HOME: mplayer_put_key(KEY_HOME);break;
				case DIKS_END: mplayer_put_key(KEY_END);break;

                default:mplayer_put_key(event.key_symbol);
                };
	};
    };
// empty buffer, because of repeating (keyboard repeat is faster than key handling
// and this causes problems during seek)
// temporary workaround should be solved in the future
    buffer->Reset(buffer);

}
//if ( mp_msg_test(MSGT_VO,MSGL_V) ) printf ("DirectFB: Check events finished\n");
}

static void flip_page(void)
{
	DFBSurfaceBlittingFlags flags=DSBLIT_NOFX;

	unlock(); // unlock frame & primary

//	if ( mp_msg_test(MSGT_VO,MSGL_V) ) printf("DirectFB: Flip page entered");
	
	DFBCHECK (primary->SetBlittingFlags(primary,flags));

	if (frame) {
	    if (stretch) {
        	DFBRectangle rect;
        	rect.x=xoffset;
	        rect.y=yoffset;
	        rect.w=out_width;
	        rect.h=out_height;

                DFBCHECK (primary->StretchBlit(primary,frame,NULL,&rect));
	    
	    } else {

		DFBCHECK (primary->Blit(primary,frame,NULL,xoffset,yoffset));
	    
	    };
	};


#ifdef TRIPLE
    switch (flipping) { 
	    case 1: DFBCHECK (primary->Flip (primary, NULL, DSFLIP_WAIT));
		    break;
	    case 2: DFBCHECK (primary->Flip (primary, NULL, DSFLIP_ONSYNC));
		    break;
	    default:; // should never be reached
	}
#else
    if (flipping) { 
	    DFBCHECK (primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC));
	}
#endif

}



static void uninit(void)
{

  //mp_msg(MSGT_VO, MSGL_INFO,"DirectFB: Uninit entered\n");

  unlock();
  
  /*
   * (Release)
   */
/*
  mp_msg(MSGT_VO, MSGL_INFO,"DirectFB: Releasing buffer\n");
  if (buffer) buffer->Release (buffer);
  mp_msg(MSGT_VO, MSGL_INFO,"DirectFB: Releasing keyboard\n");
  if (keyboard) keyboard->Release (keyboard);
*/
  if (frame) {
    mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Releasing frame\n");
    frame->Release (frame);
    frame = NULL;
  };

//  switch off BES
//  if (layer) layer->SetOpacity(layer,0);

  if (layer) {
   mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Releasing layer\n");
   layer->Release(layer);
   layer = NULL;
  }

  if (primary) {
   mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: Releasing primary\n");
   primary->Release (primary);
   primary = NULL;
  }


/*  mp_msg(MSGT_VO, MSGL_INFO,"DirectFB: Releasing DirectFB library\n");

  dfb->Release (dfb);
*/
  //mp_msg(MSGT_VO, MSGL_INFO,"DirectFB: Uninit done.\n");
}


static uint32_t directfb_set_video_eq(char *data, int value) //data==name
{
	
	DFBColorAdjustment ca;
	float factor =  (float)0xffff / 200.0;

        DFBDisplayLayerDescription  desc;
	
	unlock();
	
if (layer) {	
	
	layer->GetDescription(layer,&desc);
	
	ca.flags=DCAF_NONE;
	
	if (! strcmp( data,"brightness" )) {
	    if (desc.caps & DLCAPS_BRIGHTNESS) {
		ca.brightness = value * factor +0x8000;
		ca.flags |= DCAF_BRIGHTNESS;
		mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: SetVEq Brightness 0x%X %i\n",ca.brightness,value);
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"contrast" )) {
	    if ((desc.caps & DLCAPS_CONTRAST)) {
	        ca.contrast = value * factor + 0x8000;
		ca.flags |= DCAF_CONTRAST;
		mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: SetVEq Contrast 0x%X %i\n",ca.contrast,value);
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"hue" )) {
    	    if ((desc.caps & DLCAPS_HUE)) {
		ca.hue = value * factor + 0x8000;
		ca.flags |= DCAF_HUE;
		mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: SetVEq Hue 0x%X %i\n",ca.hue,value);
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"saturation" )) {
		if ((desc.caps & DLCAPS_SATURATION)) {
	        ca.saturation = value * factor + 0x8000;
		ca.flags |= DCAF_SATURATION;
		mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: SetVEq Saturation 0x%X %i\n",ca.saturation,value);
	    } else return VO_FALSE;
	}

	if (ca.flags != DCAF_NONE) {
	    layer->SetColorAdjustment(layer,&ca);
	    return VO_TRUE;
	}
}	

    return VO_FALSE;

}

static uint32_t directfb_get_video_eq(char *data, int *value) // data==name
{
	
	DFBColorAdjustment ca;
	float factor = 200.0 / (float)0xffff;
	
        DFBDisplayLayerDescription desc;
	 
if (layer) {	
	
	unlock();
	
	layer->GetDescription(layer,&desc);

	layer->GetColorAdjustment(layer,&ca);
	
	if (! strcmp( data,"brightness" )) {
	    if (desc.caps & DLCAPS_BRIGHTNESS) {
		*value = (int) ((ca.brightness-0x8000) * factor);
		mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: GetVEq Brightness 0x%X %i\n",ca.brightness,*value);
    		return VO_TRUE;
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"contrast" )) {
	    if ((desc.caps & DLCAPS_CONTRAST)) {
		*value = (int) ((ca.contrast-0x8000) * factor);
		mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: GetVEq Contrast 0x%X %i\n",ca.contrast,*value);
    		return VO_TRUE;
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"hue" )) {
    	    if ((desc.caps & DLCAPS_HUE)) {
		*value = (int) ((ca.hue-0x8000) * factor);
    		mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: GetVEq Hue 0x%X %i\n",ca.hue,*value);
    		return VO_TRUE;
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"saturation" )) {
    	    if ((desc.caps & DLCAPS_SATURATION)) {
		*value = (int) ((ca.saturation-0x8000) * factor);
    		mp_msg(MSGT_VO, MSGL_DBG2,"DirectFB: GetVEq Saturation 0x%X %i\n",ca.saturation,*value);
    		return VO_TRUE;
	    } else return VO_FALSE;
	}
}
    return VO_FALSE;
}

static uint32_t get_image(mp_image_t *mpi)
{

        int err;
        uint8_t *dst;
        int pitch;

//    if ( mp_msg_test(MSGT_VO,MSGL_V) ) printf("DirectFB: get_image() called\n");
    if(mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE; // slow video ram 
    if(mpi->type==MP_IMGTYPE_STATIC) return VO_FALSE; // it is not static

//    printf("width=%d vs. pitch=%d, flags=0x%X  \n",mpi->width,pitch,mpi->flags);

    if((mpi->width==pitch) ||
       (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH))){
       // we're lucky or codec accepts stride => ok, let's go!

	    if (frame) {
		err = frame->Lock(frame,DSLF_WRITE|DSLF_READ,(void *)&dst,&pitch);
		framelocked=1;
	    } else {
 		err = primary->Lock(primary,DSLF_WRITE,(void *)&dst,&pitch);
		primarylocked=1;
	    }

	    if (err) {
		mp_msg(MSGT_VO, MSGL_ERR,"DirectFB: DR lock failed!");
		return VO_FALSE;
	    };

       if(mpi->flags&MP_IMGFLAG_PLANAR){
    	   //YV12 format
	   mpi->planes[0]=dst;
	   if(mpi->flags&MP_IMGFLAG_SWAPPED){
	   mpi->planes[1]=dst + pitch*height;
	   mpi->planes[2]=mpi->planes[1] + pitch*height/4;
	   } else {
	   mpi->planes[2]=dst + pitch*height;
	   mpi->planes[1]=mpi->planes[2] + pitch*height/4;
	   }
	   mpi->width=width;
	   mpi->stride[0]=pitch;
	   mpi->stride[1]=mpi->stride[2]=pitch/2;
       } else {
    	   //YUY2 and RGB formats
           mpi->planes[0]=dst;
	   mpi->width=width;
	   mpi->stride[0]=pitch;
       }

       // center image
       
       if (!frame) {
            if(mpi->flags&MP_IMGFLAG_PLANAR){
		mpi->planes[0]= dst + yoffset * pitch + xoffset;
		mpi->planes[1]+= ((yoffset * pitch) >> 2) + (xoffset >> 1);
		mpi->planes[2]+= ((yoffset * pitch) >> 2) + (xoffset >> 1);
	    } else {
		mpi->planes[0]=dst + yoffset * pitch + xoffset * (mpi->bpp >> 3);
	    }		   
       }
       
       mpi->flags|=MP_IMGFLAG_DIRECT;
//       if ( mp_msg_test(MSGT_VO,MSGL_V) ) printf("DirectFB: get_image() SUCCESS -> Direct Rendering ENABLED\n");
       return VO_TRUE;

    } 
    return VO_FALSE;
}

static int draw_slice(uint8_t *src[], int stride[], int w, int h, int x, int y)
{
        int i;
	unsigned int pitch;
        uint8_t *dst;
        uint8_t *dst2;
        uint8_t *srcp;
	unsigned int p;

//        if ( mp_msg_test(MSGT_VO,MSGL_V) ) printf("DirectFB: draw_slice entered\n");

	unlock();

	if (frame) {
		DFBCHECK (frame->Lock(frame,DSLF_WRITE|DSLF_READ,(void *)&dst,&pitch));
		framelocked = 1;
        } else {
		DFBCHECK (primary->Lock(primary,DSLF_WRITE,(void *)&dst,&pitch));
		primarylocked = 1;
        };
	
	p=min(w,pitch);

	dst += y*pitch + x;
	dst2 = dst + pitch*height - y*pitch + y*pitch/4 - x/2;
	srcp = src[0];
	
	for (i=0;i<h;i++) {
            fast_memcpy(dst,srcp,p);
	    dst += pitch;
	    srcp += stride[0];
        }

	if (pixel_format == DSPF_YV12) { 

            dst = dst2;
	    srcp = src[2];
    	    p = p/2;

            for (i=0;i<h/2;i++) {
                fast_memcpy(dst,srcp,p);
		dst += pitch/2;
	        srcp += stride[2];
    	    }
	
    	    dst = dst2 + pitch*height/4;
	    srcp = src[1];
	
    	    for (i=0;i<h/2;i++) {
                fast_memcpy(dst,srcp,p);
		dst += pitch/2;
	        srcp += stride[1];
    	    }

	} else {

            dst = dst2;
	    srcp = src[1];
	    p = p/2;

    	    for (i=0;i<h/2;i++) {
                fast_memcpy(dst,srcp,p);
		dst += pitch/2;
	        srcp += stride[1];
    	    }
	
    	    dst = dst2 + pitch*height/4;
	    srcp = src[2];
	
    	    for (i=0;i<h/2;i++) {
                fast_memcpy(dst,srcp,p);
		dst += pitch/2;
	        srcp += stride[2];
    	    }
	
	}

	unlock();

    return 0;
}


static uint32_t put_image(mp_image_t *mpi){


//    static IDirectFBSurface *tmp = NULL;
//    DFBSurfaceDescription dsc;
//    DFBRectangle rect;
    
//    if ( mp_msg_test(MSGT_VO,MSGL_V) ) printf("DirectFB: Put_image entered %i %i %i %i %i %i\n",mpi->x,mpi->y,mpi->w,mpi->h,mpi->width,mpi->height);

    unlock();

    // already out?
    if((mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK))) {
//        if ( mp_msg_test(MSGT_VO,MSGL_V) ) printf("DirectFB: Put_image - nothing to do\n");
	return VO_TRUE;
    }

    if (mpi->flags&MP_IMGFLAG_PLANAR) {
    // memcpy all planes - sad but necessary
        int i;
	unsigned int pitch;
        uint8_t *dst;
	uint8_t *src;
	unsigned int p;

//        if ( mp_msg_test(MSGT_VO,MSGL_V) ) printf("DirectFB: Put_image - planar branch\n");
	if (frame) {
		DFBCHECK (frame->Lock(frame,DSLF_WRITE|DSLF_READ,(void *)&dst,&pitch));
		framelocked = 1;
        } else {
		DFBCHECK (primary->Lock(primary,DSLF_WRITE,(void *)&dst,&pitch));
		primarylocked = 1;
        };
	
	p=min(mpi->w,pitch);

	src = mpi->planes[0]+mpi->y*mpi->stride[0]+mpi->x;
	
	for (i=0;i<mpi->h;i++) {
            fast_memcpy(dst+i*pitch,src+i*mpi->stride[0],p);
        }

	
	if (pixel_format == DSPF_YV12) {

            dst += pitch*height;
    	    p = p/2;
	    src = mpi->planes[2]+mpi->y*mpi->stride[2]+mpi->x/2;

            for (i=0;i<mpi->h/2;i++) {
	        fast_memcpy(dst+i*pitch/2,src+i*mpi->stride[2],p);
    	    }
	
    	    dst += pitch*height/4;
	    src = mpi->planes[1]+mpi->y*mpi->stride[1]+mpi->x/2;
	
    	    for (i=0;i<mpi->h/2;i++) {
        	fast_memcpy(dst+i*pitch/2,src+i*mpi->stride[1],p);
    	    }

	} else {

    	    dst += pitch*height;
	    p = p/2;
	    src = mpi->planes[1]+mpi->y*mpi->stride[1]+mpi->x/2;

    	    for (i=0;i<mpi->h/2;i++) {
        	fast_memcpy(dst+i*pitch/2,src+i*mpi->stride[1],p);
    	    }
	
    	    dst += pitch*height/4;
	    src = mpi->planes[2]+mpi->y*mpi->stride[2]+mpi->x/2;
	
    	    for (i=0;i<mpi->h/2;i++) {
        	fast_memcpy(dst+i*pitch/2,src+i*mpi->stride[2],p);
    	    }
	
	}
	unlock();

    } else {
// I had to disable native directfb blit because it wasn't working under some conditions :-(

/*
	dsc.flags = DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_WIDTH | DSDESC_PREALLOCATED;
	dsc.preallocated[0].data = mpi->planes[0];
	dsc.preallocated[0].pitch = mpi->stride[0];
        dsc.width = mpi->width;
        dsc.height = mpi->height;
        dsc.pixelformat = convformat(mpi->imgfmt);
	    
	DFBCHECK (dfb->CreateSurface( dfb, &dsc, &tmp));
    
        rect.x=mpi->x;
        rect.y=mpi->y;
        rect.w=mpi->w;
        rect.h=mpi->h;

	if (frame) {
		DFBCHECK (tmp->Blit(tmp,frame,&rect,0,0));
        } else {
		DFBCHECK (tmp->Blit(tmp,primary,&rect,xoffset,yoffset));
        };
        tmp->Release(tmp);
*/

	unsigned int pitch;
        uint8_t *dst;

//        if ( mp_msg_test(MSGT_VO,MSGL_V) ) printf("DirectFB: Put_image - non-planar branch\n");
	if (frame) {
		DFBCHECK (frame->Lock(frame,DSLF_WRITE,(void *)&dst,&pitch));
		framelocked = 1;
		mem2agpcpy_pic(dst,mpi->planes[0] + mpi->y * mpi->stride[0] + mpi->x * (mpi->bpp >> 3)  ,mpi->w * (mpi->bpp >> 3),mpi->h,pitch,mpi->stride[0]);
        } else {
		DFBCHECK (primary->Lock(primary,DSLF_WRITE,(void *)&dst,&pitch));
		primarylocked = 1;
		mem2agpcpy_pic(dst + yoffset * pitch + xoffset * (mpi->bpp >> 3),mpi->planes[0] + mpi->y * mpi->stride[0] + mpi->x * (mpi->bpp >> 3)  ,mpi->w * (mpi->bpp >> 3),mpi->h,pitch,mpi->stride[0]);
        };
	unlock();

    }
    return VO_TRUE;
}



static int control(uint32_t request, void *data, ...)
{
  switch (request) {
    case VOCTRL_QUERY_FORMAT:
	return query_format(*((uint32_t*)data));
    case VOCTRL_GET_IMAGE:
	return get_image(data);
    case VOCTRL_DRAW_IMAGE:
	return put_image(data);    
    case VOCTRL_SET_EQUALIZER:
      {
        va_list ap;
	int value;
    
        va_start(ap, data);
	value = va_arg(ap, int);
        va_end(ap);
    
	return directfb_set_video_eq(data, value);
      }
    case VOCTRL_GET_EQUALIZER:
      {
	va_list ap;
        int *value;
    
        va_start(ap, data);
        value = va_arg(ap, int*);
        va_end(ap);
    
	return directfb_get_video_eq(data, value);
      }
  };
  return VO_NOTIMPL;
}

// unused function

static int draw_frame(uint8_t *src[])
{
	return -1;
}

// hopefully will be removed soon

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
        void *dst;
        int pitch;
	
	unlock(); // isn't it silly I have to unlock surface and then lock it again :-)
	
	if (frame) {
		DFBCHECK (frame->Lock(frame,DSLF_WRITE|DSLF_READ,&dst,&pitch));
		framelocked = 1;
        } else {
		DFBCHECK (primary->Lock(primary,DSLF_WRITE,&dst,&pitch));
		primarylocked = 1;
        };
    
	switch(pixel_format) {
                case DSPF_RGB32:
                case DSPF_ARGB:
                        vo_draw_alpha_rgb32(w,h,src,srca,stride,((uint8_t *) dst)+pitch*y0 + 4*x0,pitch);
                        break;

                case DSPF_RGB24:
                        vo_draw_alpha_rgb24(w,h,src,srca,stride,((uint8_t *) dst)+pitch*y0 + 3*x0,pitch);
                        break;

                case DSPF_RGB16:
                        vo_draw_alpha_rgb16(w,h,src,srca,stride,((uint8_t *) dst)+pitch*y0 + 2*x0,pitch);
                        break;
#if DIRECTFBVERSION > DFB_VERSION(0,9,15)
                case DSPF_ARGB1555:
#else
                case DSPF_RGB15:
#endif
                        vo_draw_alpha_rgb15(w,h,src,srca,stride,((uint8_t *) dst)+pitch*y0 + 2*x0,pitch);
                        break;

		case DSPF_YUY2:
    			vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) dst) + pitch*y0 + 2*x0,pitch);
		break;

        	case DSPF_UYVY:
    			vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) dst) + pitch*y0 + 2*x0 + 1,pitch);
		break;

        	case DSPF_I420:
		case DSPF_YV12:
    			vo_draw_alpha_yv12(w,h,src,srca,stride,((uint8_t *) dst) + pitch*y0 + 1*x0,pitch);
		break;
		}

	    unlock();
}

static void draw_osd(void)
{
    vo_draw_text(width,height,draw_alpha);
}
