/*
   MPlayer video driver for DirectFramebuffer device
  
   (C) 2002
   
   Written by  Jiri Svoboda <Jiri.Svoboda@seznam.cz>

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

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "sub.h"

#include "aspect.h"

#ifndef min
#define min(x,y) (((x)<(y))?(x):(y))
#endif


LIBVO_EXTERN(directfb)

static vo_info_t vo_info = {
	"Direct Framebuffer Device",
	"directfb",
	"Jiri Svoboda Jiri.Svoboda@seznam.cz",
	"version 2.0beta"
};

extern int verbose;

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
// pictrure position
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
#ifdef HAVE_FBDEV
extern char *fb_dev_name;
#else
char *fb_dev_name;
#endif

/******************************
*	   implementation     *
******************************/

void unlock() {
if (frame && framelocked) frame->Unlock(frame);
if (primary && primarylocked) primary->Unlock(primary);
}


static uint32_t preinit(const char *arg)
{

DFBResult ret;
	
if (verbose) printf("DirectFB: Preinit entered\n");

        DFBCHECK (DirectFBInit (NULL,NULL));

	if (((directfb_major_version <= 0) &&
	    (directfb_minor_version <= 9) &&
	    (directfb_micro_version < 13)))
	{
	    printf("DirectFB: Unsupported DirectFB version\n");	
	    return 1;
	}

  /*
   * (set options)
   */
	
	if (!fb_dev_name && !(fb_dev_name = getenv("FRAMEBUFFER"))) fb_dev_name = "/dev/fb0";
    	DFBCHECK (DirectFBSetOption ("fbdev",fb_dev_name));
	
//	uncomment this if you do not wish to create a new vt for DirectFB
//        DFBCHECK (DirectFBSetOption ("no-vt-switch",""));

//	uncomment this if you want to allow vt switching
//       DFBCHECK (DirectFBSetOption ("vt-switching",""));

//	uncomment this if you want to hide gfx cursor (req dfb >=0.9.9)
       DFBCHECK (DirectFBSetOption ("no-cursor",""));

// bg color fix
        DFBCHECK (DirectFBSetOption ("bg-color","00000000"));

  /*
   * (Initialize)
   */

        DFBCHECK (DirectFBCreate (&dfb));
        DFBCHECK (dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN));

  /*
   * (Get keyboard)
   */

  ret = dfb->GetInputDevice (dfb, DIDID_KEYBOARD, &keyboard);

  if (ret==DFB_OK) {
    if (verbose) {
    printf("DirectFB: Keyboard init OK\n");
    }
  } else {
    keyboard = NULL;
    printf("DirectFB: Keyboard init FAILED\n");
  }

  /*
   * Create an input buffer for the keyboard.
   */
  if (keyboard) DFBCHECK (keyboard->CreateEventBuffer (keyboard, &buffer));

  // just to start with clean ...
  if (buffer) buffer->Reset(buffer);

  if (verbose) {
    printf("DirectFB: Preinit OK\n");
   }

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
            case IMGFMT_RGB15: return  DSPF_RGB15; break;
            case IMGFMT_BGR15: return  DSPF_RGB15; break;
            case IMGFMT_YUY2:  return  DSPF_YUY2; break;
            case IMGFMT_UYVY:  return  DSPF_UYVY; break;
    	    case IMGFMT_YV12:  return  DSPF_YV12; break;
    	    case IMGFMT_I420:  return  DSPF_I420; break;
//    	    case IMGFMT_IYUV:  return  DSPF_IYUV; break;
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

	if (verbose) printf("DirectFB: Test format - layer %i scale/pos %i\n",id,(desc.caps & DLCAPS_SCREEN_LOCATION));
     
	if (!ret) {
//	    printf("Test OK\n");     
	    if (params->result) {
	        if  ((!params->scale) && (desc.caps & DLCAPS_SCREEN_LOCATION)) {
		    params->scale=1;
		    params->id=id;
		    if (verbose) printf("DirectFB: Test format - added layer %i scale/pos %i\n",id,(desc.caps & DLCAPS_SCREEN_LOCATION));
		    
		}
	    } else {
		params->result=1;
		params->id=id;
		if (desc.caps & DLCAPS_SCREEN_LOCATION) params->scale=1;
		if (verbose) printf("DirectFB: Test format - added layer %i scale/pos %i\n",id,(desc.caps & DLCAPS_SCREEN_LOCATION));
	   };
	};
     };

    return DFENUM_OK;
}

static uint32_t query_format(uint32_t format)
{
	int ret = VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_OSD; // osd should be removed in future -> will be handled outside...
	enum1_t params;


	if (!convformat(format)) return 0;
// temporary disable YV12
//	if (format == IMGFMT_YV12) return 0;
//	if (format == IMGFMT_I420) return 0;
	if (format == IMGFMT_IYUV) return 0;
	
	if (verbose) printf("DirectFB: Format query: %s\n",vo_format_name(format));

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


DFBEnumerationResult video_modes_callback( unsigned int width,unsigned int height,unsigned int bpp, void *data)
{
     videomode_t *params =(videomode_t *)data;

int overx=0,overy=0,closer=0,over=0;
int we_are_under=0;

if (verbose) printf("DirectFB: Validator entered %i %i %i\n",width,height,bpp);

overx=width-params->out_width;
overy=height-params->out_height;

if (!params->width) {
        params->width=width;
        params->height=height;
	params->overx=overx;
	params->overy=overy;
	if (verbose) printf("DirectFB: Mode added %i %i %i\n",width,height,bpp);
}

if ((params->overy<0)||(params->overx<0)) we_are_under=1; // stored mode is smaller than req mode
if (abs(overx*overy)<abs(params->overx * params->overy)) closer=1; // current mode is closer to desired res
if ((overx>=0)&&(overy>=0)) over=1; // current mode is bigger or equaul to desired res
if ((closer && (over || we_are_under)) || (we_are_under && over)) {
                params->width=width;
                params->height=height;
                params->overx=overx;
                params->overy=overy;
		if (verbose) printf("DirectFB: Better mode added %i %i %i\n",width,height,bpp);
                };

return DFENUM_OK;
}

#define CONFIG_ERROR -1

static uint32_t config(uint32_t s_width, uint32_t s_height, uint32_t d_width,
		uint32_t d_height, uint32_t fullscreen, char *title,
		uint32_t format,const vo_tune_info_t *info)
{

/*
1) pokud je vm zmen videomode 
    - HOTOVO

2) nejprve najit nejvhodnejsi vrstvu - tj. podporujici dany format a zmenu polohy/velikosti
->enum vyplni strukturu: - pokud neni nic tak tam placne prvni co alespon podporuje format
			 - jinak posledni podporujici vse (prepise klidne strukturu nekolikrat)
			 struktura - format + layer + caps 
    - HOTOVO
			 
3) nakonfigurovat vrstvu (postupne po jedne vlastnosti: - format 		HOTOVO
							- velikost obrazu	HOTOVO
							- buffermode		HOTOVO
							- pozici/velikost	HOTOVO
3) ziskat surface na vrstve - HOTOVO
4) pokud je zoom vytvori pomocny buffer
5) v pripade potreby zapnout a otestovat flip nebo vytvorit pomocny buffer (pokud jeste neni)

*/
  /*
   * (Locals)
   */

// decode flags

	int fs = fullscreen & 0x01;
        int vm = fullscreen & 0x02;
	int zoom = fullscreen & 0x04;
	int flip = fullscreen & 0x08;

	DFBSurfaceDescription dsc;
        DFBResult             ret;
        DFBDisplayLayerConfig dlc;
	DFBSurfaceCapabilities caps;

	enum1_t params;

	if (verbose) {
	    printf("DirectFB: Config entered [%ix%i]\n",s_width,s_height);
	    printf("DirectFB: With requested format: %s\n",vo_format_name(format));
	}
// initial clean-up
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
	    if (verbose) printf("DirectFB: Config - videomode change\n");
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

// find best layer

        if (verbose) printf("DirectFB: Config - find suitable layer\n");
	params.format=format;
	params.scale=0;
	params.result=0;
	params.width=s_width;
	params.height=s_height;
	params.setsize=1;

        DFBCHECK (dfb->EnumDisplayLayers(dfb,test_format_callback,&params));

	if (!params.result) {
	    printf("DirectFB: ConfigError - no suitable layer found\n");
	    params.id = DLID_PRIMARY;
	}	
	
	if (verbose) printf("DirectFB: Config - layer %i\n",params.id);


// try to setp-up proper configuration


        DFBCHECK (dfb->GetDisplayLayer( dfb, params.id, &layer));

	if (params.scale) {
            if (verbose) printf("DirectFB: Config - set layer config (size)\n");
            dlc.flags       = DLCONF_WIDTH | DLCONF_HEIGHT;
	    dlc.width       = s_width;
    	    dlc.height      = s_height;
	    
	    ret = layer->SetConfiguration(layer,&dlc);
	    
	    if (ret && (params.scale || verbose)) printf("DirectFB: ConfigError in layer configuration (size)\n");
	
	}

        dlc.flags       = DLCONF_PIXELFORMAT;
	dlc.pixelformat = convformat(params.format);

//	printf("DirectFB: Format [%x]\n",dlc.pixelformat);
             
        if (verbose) printf("DirectFB: Config - set layer config (format)\n");
	ret = layer->SetConfiguration(layer,&dlc);
	
	if (ret) {
	    printf("DirectFB: ConfigError in layer configuration (format)\n");
	    return CONFIG_ERROR;
	};
	

// flipping of layer

	dlc.flags = DLCONF_BUFFERMODE;
	dlc.buffermode = DLBM_BACKVIDEO;
        ret = layer->SetConfiguration( layer, &dlc );
        if (ret!=DFB_OK) {
    	    dlc.buffermode = DLBM_BACKSYSTEM;
	    ret = layer->SetConfiguration( layer, &dlc );
/*                if (ret==DFB_OK) {
		// nastav vse pro flip
		    flipping = 1;
		}
        } else  {
		// nastav vse pro flip
		    flipping = 1;
*/	}

// get layer surface
	
	ret = layer->GetSurface(layer,&primary);
	
	if (ret) {
	    printf("DirectFB: ConfigError in obtaining surface\n");
	    return CONFIG_ERROR; // what shall we report on fail?
	}

// test surface for flipping	
	DFBCHECK(primary->GetCapabilities(primary,&caps));

        flipping = 0;
	if (caps & DSCAPS_FLIPPING) {
	    ret = primary->Flip(primary,NULL,0);
	    if (ret==DFB_OK) { 
		flipping = 1; 
	    } 
	};

        if (verbose) printf("DirectFB: Config - flipping = %i\n",flipping);

// is scale needed ? Aspect ratio and layer pos/size
	

	// get surface size
	DFBCHECK(primary->GetSize(primary,&width,&height));

        if (verbose) printf("DirectFB: Config - surface size = %ix%i\n",width,height);

	aspect_save_orig(s_width,s_height);
	aspect_save_prescale(d_width,d_height);
	if (params.scale) {
		aspect_save_screenres(10000,10000);
		aspect(&out_width,&out_height,A_ZOOM);

                ret = layer->SetScreenLocation(layer,(1-(float)out_width/10000)/2,(1-(float)out_height/10000)/2,((float)out_width/10000),((float)out_height/10000));

		if (ret) printf("DirectFB: ConfigError in layer configuration (position)\n");

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
	    
	} 

// get format for draw_alpha - should be removed soon - osd will be rendered outside vo driver
	if (frame) {
		DFBCHECK (frame->GetPixelFormat(frame,&pixel_format));
        } else {
		DFBCHECK (primary->GetPixelFormat(primary,&pixel_format));
        };

 // finally turn on layer
 layer->SetOpacity(layer,255);

 if (verbose) printf("DirectFB: Config finished [%ix%i] - [%ix%i]\n",out_width,out_height,width,height);

return 0;
}

static const vo_info_t *get_info(void)
{
	return &vo_info;
}

extern void mplayer_put_key(int code);

#include "../linux/keycodes.h"

static void check_events(void)
{

if (buffer) {

      DFBInputEvent event;

//if (verbose) printf ("DirectFB: Check events entered\n");
     if (buffer->GetEvent(buffer, DFB_EVENT (&event)) == DFB_OK) {

     if (event.type == DIET_KEYPRESS) { 
    		switch (event.key_symbol) {
                                case DIKS_ESCAPE:
					mplayer_put_key('q');
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
// temporary workabout should be solved in the future
    buffer->Reset(buffer);

}
//if (verbose) printf ("DirectFB: Check events finished\n");
}

static void flip_page(void)
{
	DFBSurfaceBlittingFlags flags=DSBLIT_NOFX;

	unlock(); // unlock frame & primary

//	if (verbose) printf("DirectFB: Flip page entered");
	
	DFBCHECK (primary->SetBlittingFlags(primary,flags));

// tady jsete pridat odemknuti frame a primary v pripade potreby

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


    if (flipping) { 
	    DFBCHECK (primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC));
	}

}



static void uninit(void)
{

  if (verbose ) printf("DirectFB: Uninit entered\n");

  unlock();
  
  /*
   * (Release)
   */
  if (verbose ) printf("DirectFB: Release buffer\n");
  if (buffer) buffer->Release (buffer);
  if (verbose ) printf("DirectFB: Release keyboard\n");
  if (keyboard) keyboard->Release (keyboard);
  if (frame) {
    if (verbose ) printf("DirectFB: Release frame\n");
    frame->Release (frame);
  };

  if (verbose ) printf("DirectFB: Release primary\n");
  if (primary) primary->Release (primary);

//  switch off BES
//  if (layer) layer->SetOpacity(layer,0);

  if (layer) layer->Release(layer);

  if (verbose ) printf("DirectFB: Release DirectFB library\n");

  dfb->Release (dfb);

  if (verbose ) printf("DirectFB: Uninit done.\n");
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
		if (verbose) printf("DirectFB: SetVEq Brightness 0x%X %i\n",ca.brightness,value);
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"contrast" )) {
	    if ((desc.caps & DLCAPS_CONTRAST)) {
	        ca.contrast = value * factor + 0x8000;
		ca.flags |= DCAF_CONTRAST;
		if (verbose) printf("DirectFB: SetVEq Contrast 0x%X %i\n",ca.contrast,value);
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"hue" )) {
    	    if ((desc.caps & DLCAPS_HUE)) {
		ca.hue = value * factor + 0x8000;
		ca.flags |= DCAF_HUE;
		if (verbose) printf("DirectFB: SetVEq Hue 0x%X %i\n",ca.hue,value);
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"saturation" )) {
		if ((desc.caps & DLCAPS_SATURATION)) {
	        ca.saturation = value * factor + 0x8000;
		ca.flags |= DCAF_SATURATION;
		if (verbose) printf("DirectFB: SetVEq Saturation 0x%X %i\n",ca.saturation,value);
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
		if (verbose) printf("DirectFB: GetVEq Brightness 0x%X %i\n",ca.brightness,*value);
    		return VO_TRUE;
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"contrast" )) {
	    if ((desc.caps & DLCAPS_CONTRAST)) {
		*value = (int) ((ca.contrast-0x8000) * factor);
		if (verbose) printf("DirectFB: GetVEq Contrast 0x%X %i\n",ca.contrast,*value);
    		return VO_TRUE;
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"hue" )) {
    	    if ((desc.caps & DLCAPS_HUE)) {
		*value = (int) ((ca.hue-0x8000) * factor);
    		if (verbose) printf("DirectFB: GetVEq Hue 0x%X %i\n",ca.hue,*value);
    		return VO_TRUE;
	    } else return VO_FALSE;
	}

	if (! strcmp( data,"saturation" )) {
    	    if ((desc.caps & DLCAPS_SATURATION)) {
		*value = (int) ((ca.saturation-0x8000) * factor);
    		if (verbose) printf("DirectFB: GetVEq Saturation 0x%X %i\n",ca.saturation,*value);
    		return VO_TRUE;
	    } else return VO_FALSE;
	}
}
    return VO_FALSE;
}

static uint32_t get_image(mp_image_t *mpi)
{

        int err;
        void *dst;
        int pitch;

//    if (verbose) printf("DirectFB: get_image() called\n");

// tohle overit - mozna pokud mam frame pak by to nemuselo byt
    if(mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE; // slow video ram
    if(mpi->type==MP_IMGTYPE_STATIC) return VO_FALSE; // it is not static

//    printf("width=%d vs. pitch=%d, flags=0x%X  \n",mpi->width,pitch,mpi->flags);

    if((mpi->width==width) ||
       (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH))){
       // we're lucky or codec accepts stride => ok, let's go!

	    if (frame) {
		err = frame->Lock(frame,DSLF_WRITE,&dst,&pitch);
		framelocked=1;
	    } else {
 		err = primary->Lock(primary,DSLF_WRITE,&dst,&pitch);
		primarylocked=1;
	    }

	    if (err) {
		if (verbose) printf("DirectFB: DR lock failed!");
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
	   mpi->width= mpi->stride[0]=pitch;
	   mpi->stride[1]=mpi->stride[2]=pitch/2;
       } else {
    	   //YUY2 and RGB formats
           mpi->planes[0]=dst;
	   mpi->width=width;
	   mpi->stride[0]=pitch;
       }
       mpi->flags|=MP_IMGFLAG_DIRECT;
//       if (verbose) printf("DirectFB: get_image() SUCCESS -> Direct Rendering ENABLED\n");
       return VO_TRUE;

    } 
    return VO_FALSE;
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h, int x, int y)
{
        int i;
	unsigned int pitch;
        void *dst;
        void *dst2;
        void *srcp;
	unsigned int p;

	unlock();

	if (frame) {
		DFBCHECK (frame->Lock(frame,DSLF_WRITE,&dst,&pitch));
		framelocked = 1;
        } else {
		DFBCHECK (primary->Lock(primary,DSLF_WRITE,&dst,&pitch));
		primarylocked = 1;
        };
	
	p=min(w,pitch);

	dst += y*pitch + x;
	dst2 = dst + pitch*height - y*pitch + y*pitch/4 - x/2;
	srcp = src[0];
	
	for (i=0;i<h;i++) {
            memcpy(dst,srcp,p);
	    dst += pitch;
	    srcp += stride[0];
        }

	if (pixel_format == DSPF_YV12) { 

            dst = dst2;
	    srcp = src[2];
    	    p = p/2;

            for (i=0;i<h/2;i++) {
                memcpy(dst,srcp,p);
		dst += pitch/2;
	        srcp += stride[2];
    	    }
	
    	    dst = dst2 + pitch*height/4;
	    srcp = src[1];
	
    	    for (i=0;i<h/2;i++) {
                memcpy(dst,srcp,p);
		dst += pitch/2;
	        srcp += stride[1];
    	    }

	} else {

            dst = dst2;
	    srcp = src[1];
	    p = p/2;

    	    for (i=0;i<h/2;i++) {
                memcpy(dst,srcp,p);
		dst += pitch/2;
	        srcp += stride[1];
    	    }
	
    	    dst = dst2 + pitch*height/4;
	    srcp = src[2];
	
    	    for (i=0;i<h/2;i++) {
                memcpy(dst,srcp,p);
		dst += pitch/2;
	        srcp += stride[2];
    	    }
	
	}

	unlock();

    return 0;
}


static uint32_t put_image(mp_image_t *mpi){


    static IDirectFBSurface *tmp = NULL;
    DFBSurfaceDescription dsc;
    DFBRectangle rect;
    
//    if (verbose) printf("DirectFB: Put_image entered %i %i %i %i %i %i\n",mpi->x,mpi->y,mpi->w,mpi->h,mpi->width,mpi->height);

    unlock();

    // already out?
    if((mpi->flags&(MP_IMGFLAG_DIRECT))) {
//        if (verbose) printf("DirectFB: Put_image - nothing todo\n");
	return VO_TRUE;
    }

    //|MP_IMGFLAG_DRAW_CALLBACK
    
    if (mpi->flags&MP_IMGFLAG_PLANAR) {
    // memcpy all planes - sad but necessary
        int i;
	unsigned int pitch;
        void *dst;
	void *src;
	unsigned int p;

//        if (verbose) printf("DirectFB: Put_image - planar branch\n");
	if (frame) {
		DFBCHECK (frame->Lock(frame,DSLF_WRITE,&dst,&pitch));
		framelocked = 1;
        } else {
		DFBCHECK (primary->Lock(primary,DSLF_WRITE,&dst,&pitch));
		primarylocked = 1;
        };
	
	p=min(mpi->w,pitch);

	src = mpi->planes[0]+mpi->y*mpi->stride[0]+mpi->x;
	
	for (i=0;i<mpi->h;i++) {
            memcpy(dst+i*pitch,src+i*mpi->stride[0],p);
        }

	if (pixel_format == DSPF_YV12) {

            dst += pitch*height;
    	    p = p/2;
	    src = mpi->planes[2]+mpi->y*mpi->stride[2]+mpi->x/2;

            for (i=0;i<mpi->h/2;i++) {
	        memcpy(dst+i*pitch/2,src+i*mpi->stride[2],p);
    	    }
	
    	    dst += pitch*height/4;
	    src = mpi->planes[1]+mpi->y*mpi->stride[1]+mpi->x/2;
	
    	    for (i=0;i<mpi->h/2;i++) {
        	memcpy(dst+i*pitch/2,src+i*mpi->stride[1],p);
    	    }

	} else {

    	    dst += pitch*height;
	    p = p/2;
	    src = mpi->planes[1]+mpi->y*mpi->stride[1]+mpi->x/2;

    	    for (i=0;i<mpi->h/2;i++) {
        	memcpy(dst+i*pitch/2,src+i*mpi->stride[1],p);
    	    }
	
    	    dst += pitch*height/4;
	    src = mpi->planes[2]+mpi->y*mpi->stride[2]+mpi->x/2;
	
    	    for (i=0;i<mpi->h/2;i++) {
        	memcpy(dst+i*pitch/2,src+i*mpi->stride[2],p);
    	    }
	
	}
	unlock();

    } else {

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
    }
    return VO_TRUE;
}



static uint32_t control(uint32_t request, void *data, ...)
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
    
	return(directfb_set_video_eq(data, value));
      }
    case VOCTRL_GET_EQUALIZER:
      {
	va_list ap;
        int *value;
    
        va_start(ap, data);
        value = va_arg(ap, int*);
        va_end(ap);
    
	return(directfb_get_video_eq(data, value));
      }
  };
  return VO_NOTIMPL;
}

// unused function

static uint32_t draw_frame(uint8_t *src[])
{
	return -1;
}

// hopefully will be removed soon

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
        void *dst;
        int pitch;
	
	unlock(); // isnt it silly I have to unlock surface and than lock again :-)
	
	if (frame) {
		DFBCHECK (frame->Lock(frame,DSLF_WRITE,&dst,&pitch));
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

                case DSPF_RGB15:
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
