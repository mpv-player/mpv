/*
 *  vo_sdl.c
 *
 *  (was video_out_sdl.c from OMS project/mpeg2dec -> http://linuxvideo.org)
 *
 *  Copyright (C) Ryan C. Gordon <icculus@lokigames.com> - April 22, 2000.
 *
 *  Copyright (C) Felix Buenemann <atmosfear@users.sourceforge.net> - 2001
 *
 *  (for extensive code enhancements)
 *
 *  Current maintainer for MPlayer project (report bugs to that address):
 *    Felix Buenemann <atmosfear@users.sourceforge.net>
 *
 *  This file is a video out driver using the SDL library (http://libsdl.org/),
 *  to be used with MPlayer [The Movie Player for Linux] project, further info
 *  from http://www.mplayerhq.hu
 *
 *  -- old disclaimer --
 *
 *  A mpeg2dec display driver that does output through the
 *  Simple DirectMedia Layer (SDL) library. This effectively gives us all
 *  sorts of output options: X11, SVGAlib, fbcon, AAlib, GGI. Win32, MacOS
 *  and BeOS support, too. Yay. SDL info, source, and binaries can be found
 *  at http://slouken.devolution.com/SDL/
 *
 *  This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *	
 *  mpeg2dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation.
 *
 *  -- end old disclaimer -- 
 *
 *  Changes:
 *    Dominik Schnitzer <dominik@schnitzer.at> - November 08, 2000.
 *    - Added resizing support, fullscreen: changed the sdlmodes selection
 *       routine.
 *    - SDL bugfixes: removed the atexit(SLD_Quit), SDL_Quit now resides in
 *       the plugin_exit routine.
 *    - Commented the source :)
 *    - Shortcuts: for switching between Fullscreen/Windowed mode and for
 *       cycling between the different Fullscreen modes.
 *    - Small bugfixes: proper width/height of movie
 *    Dominik Schnitzer <dominik@schnitzer.at> - November 11, 2000.
 *    - Cleanup code, more comments
 *    - Better error handling
 *    Bruno Barreyra <barreyra@ufl.edu> - December 10, 2000.
 *    - Eliminated memcpy's for entire frames
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - March 11, 2001
 *    - Added aspect-ratio awareness for fullscreen
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - March 11, 2001
 *    - Fixed aspect-ratio awareness, did only vertical scaling (black bars above
 *       and below), now also does horizontal scaling (black bars left and right),
 *       so you get the biggest possible picture with correct aspect-ratio.
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - March 12, 2001
 *    - Minor bugfix to aspect-ratio for non-4:3-resolutions (like 1280x1024)
 *    - Bugfix to check_events() to reveal mouse cursor after 'q'-quit in
 *       fullscreen-mode
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - April 10, 2001
 *    - Changed keypress-detection from keydown to keyup, seems to fix keyrepeat
 *       bug (key had to be pressed twice to be detected)
 *    - Changed key-handling: 'f' cycles fullscreen/windowed, ESC/RETURN/'q' quits
 *    - Bugfix which avoids exit, because return is passed to sdl-output on startup,
 *       which caused the player to exit (keyboard-buffer problem? better solution
 *       recommed)
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - April 11, 2001
 *    - OSD and subtitle support added
 *    - some minor code-changes
 *    - added code to comply with new fullscreen meaning
 *    - changed fullscreen-mode-cycling from '+' to 'c' (interferred with audiosync
 *       adjustment)
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - April 13, 2001
 *    - added keymapping to toggle OSD ('o' key) 
 *    - added some defines to modify some sdl-out internas (see comments)
 *
 *    Felix Buenemann: further changes will be visible through cvs log, don't want
 *     to update this all the time (CVS info on http://mplayer.sourceforge.net)
 *
 */

/* define to force software-surface (video surface stored in system memory)*/
#undef SDL_NOHWSURFACE

/* define to enable surface locks, this might be needed on SMP machines */
#undef SDL_ENABLE_LOCKS

//#define BUGGY_SDL //defined by configure

/* MONITOR_ASPECT MUST BE FLOAT */
#define MONITOR_ASPECT 4.0/3.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "mp_msg.h"
#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "fastmemcpy.h"
#include "sub.h"
#include "aspect.h"
#include "libmpcodecs/vfcap.h"

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include "x11_common.h"
#endif

#include "input/input.h"
#include "input/mouse.h"
#include "subopt-helper.h"
#include "mp_fifo.h"

static vo_info_t info = 
{
	"SDL YUV/RGB/BGR renderer (SDL v1.1.7+ only!)",
	"sdl",
	"Ryan C. Gordon <icculus@lokigames.com>, Felix Buenemann <atmosfear@users.sourceforge.net>",
	""
};

LIBVO_EXTERN(sdl)

#include <SDL.h>
//#include <SDL/SDL_syswm.h>


#ifdef SDL_ENABLE_LOCKS
#define	SDL_OVR_LOCK(x)        if (SDL_LockYUVOverlay (priv->overlay)) { \
				if( mp_msg_test(MSGT_VO,MSGL_V) ) { \
 				  mp_msg(MSGT_VO,MSGL_V, "SDL: Couldn't lock YUV overlay\n"); } \
				return x; \
	    		    }
#define SDL_OVR_UNLOCK      SDL_UnlockYUVOverlay (priv->overlay);

#define SDL_SRF_LOCK(srf, x)   if(SDL_MUSTLOCK(srf)) { \
				if(SDL_LockSurface (srf)) { \
					if( mp_msg_test(MSGT_VO,MSGL_V) ) { \
 					  mp_msg(MSGT_VO,MSGL_V, "SDL: Couldn't lock RGB surface\n"); } \
					return x; \
				} \
			    }

#define SDL_SRF_UNLOCK(srf) if(SDL_MUSTLOCK(srf)) \
				SDL_UnlockSurface (srf);
#else
#define SDL_OVR_LOCK(x)
#define SDL_OVR_UNLOCK
#define SDL_SRF_LOCK(srf, x)
#define SDL_SRF_UNLOCK(srf)
#endif

/** Private SDL Data structure **/

static struct sdl_priv_s {

	/* output driver used by sdl */
	char driver[8];
	
	/* SDL display surface */
	SDL_Surface *surface;
	
	/* SDL RGB surface */
	SDL_Surface *rgbsurface;
	
	/* SDL YUV overlay */
	SDL_Overlay *overlay;

	/* available fullscreen modes */
	SDL_Rect **fullmodes;

	/* surface attributes for fullscreen and windowed mode */
	Uint32 sdlflags, sdlfullflags;

	/* save the windowed output extents */
	SDL_Rect windowsize;
	
	/* Bits per Pixel */
	Uint8 bpp;

	/* RGB or YUV? */
	Uint8 mode;
	#define YUV 0
	#define RGB 1
	#define BGR 2

	/* use direct blitting to surface */
	int dblit;

	/* current fullscreen mode, 0 = highest available fullscreen mode */
	int fullmode;

	/* YUV ints */
	int framePlaneY, framePlaneUV, framePlaneYUY;
	int stridePlaneY, stridePlaneUV, stridePlaneYUY;
	
	/* RGB ints */
	int framePlaneRGB;
	int stridePlaneRGB;

	/* Flip image */
	int flip;

	/* fullscreen behaviour; see init */
	int fulltype;

	/* is X running (0/1) */
	int X;

	/* X11 Resolution */
	int XWidth, XHeight;
	
        /* original image dimensions */
	int width, height;

	/* destination dimensions */
	int dstwidth, dstheight;

    /* Draw image at coordinate y on the SDL surfaces */
    int y;

    /* The image is displayed between those y coordinates in priv->surface */
    int y_screen_top, y_screen_bottom;

    /* 1 if the OSD has changed otherwise 0 */
    int osd_has_changed;
    
	/* source image format (YUV/RGB/...) */
    uint32_t format;

    /* dirty_off_frame[0] contains a bounding box around the osd contents drawn above the image
       dirty_off_frame[1] is the corresponding thing for OSD contents drawn below the image
    */
    SDL_Rect dirty_off_frame[2];
} sdl_priv;

static void erase_area_4(int x_start, int width, int height, int pitch, uint32_t color, uint8_t* pixels);
static void erase_area_1(int x_start, int width, int height, int pitch, uint8_t color, uint8_t* pixels);
static int setup_surfaces(void);
static void set_video_mode(int width, int height, int bpp, uint32_t sdlflags);
static void erase_rectangle(int x, int y, int w, int h);

/* Expand 'rect' to contain the rectangle specified by x, y, w and h */
static void expand_rect(SDL_Rect* rect, int x, int y, int w, int h)
{
    if(rect->x < 0 || rect->y < 0) {
        rect->x = x;
        rect->y = y;
        rect->w = w;
        rect->h = h;
        return;
    }
    
    if(rect->x > x)
        rect->x = x;

    if(rect->y > y)
        rect->y = y;

    if(rect->x + rect->w < x + w)
        rect->w = x + w - rect->x;

    if(rect->y + rect->h < y + h)
        rect->h = y + h - rect->y;
}

/** libvo Plugin functions **/

/**
 * draw_alpha is used for osd and subtitle display.
 *
 **/

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
	struct sdl_priv_s *priv = &sdl_priv;
	
    if(priv->osd_has_changed) {
        /* OSD did change. Store a bounding box of everything drawn into the OSD */
        if(priv->y >= y0) {
            /* Make sure we don't mark part of the frame area dirty */
            if(h + y0 > priv->y) 
                expand_rect(&priv->dirty_off_frame[0], x0, y0, w, priv->y - y0);
            else
                expand_rect(&priv->dirty_off_frame[0], x0, y0, w, h);
        }
        else if(priv->y + priv->height <= y0 + h) {
            /* Make sure we don't mark part of the frame area dirty */
            if(y0 < priv->y + priv->height) 
                expand_rect(&priv->dirty_off_frame[1], x0,
                            priv->y + priv->height,
                            w, h - ((priv->y + priv->height) - y0));
            else
                expand_rect(&priv->dirty_off_frame[1], x0, y0, w, h);
        }
    }
    else { /* OSD contents didn't change only draw parts that was erased by the frame */
        if(priv->y >= y0) {
           src = src + (priv->y - y0) * stride;
           srca = srca + (priv->y - y0) * stride;
           h -= priv->y - y0;
           y0 = priv->y;
        }

        if(priv->y + priv->height <= y0 + h)
            h = priv->y + priv->height - y0;

        if(h <= 0)
            return;
    }

	switch(priv->format) {
		case IMGFMT_YV12:  
		case IMGFMT_I420:
        	case IMGFMT_IYUV:
            vo_draw_alpha_yv12(w,h,src,srca,stride,((uint8_t *) *(priv->overlay->pixels))+priv->overlay->pitches[0]*y0+x0,priv->overlay->pitches[0]);
		break;
		case IMGFMT_YUY2:
        	case IMGFMT_YVYU:
                x0 *= 2;
    			vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) *(priv->overlay->pixels))+priv->overlay->pitches[0]*y0+x0,priv->overlay->pitches[0]);
		break;	
        	case IMGFMT_UYVY:
                x0 *= 2;
    			vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) *(priv->overlay->pixels))+priv->overlay->pitches[0]*y0+x0,priv->overlay->pitches[0]);
		break;

		default:
        if(priv->dblit) {
            x0 *= priv->surface->format->BytesPerPixel;
		switch(priv->format) {
		case IMGFMT_RGB15:
		case IMGFMT_BGR15:
    			vo_draw_alpha_rgb15(w,h,src,srca,stride,((uint8_t *) priv->surface->pixels)+y0*priv->surface->pitch+x0,priv->surface->pitch);
		break;
		case IMGFMT_RGB16:
		case IMGFMT_BGR16:
    			vo_draw_alpha_rgb16(w,h,src,srca,stride,((uint8_t *) priv->surface->pixels)+y0*priv->surface->pitch+x0,priv->surface->pitch);
		break;
		case IMGFMT_RGB24:
		case IMGFMT_BGR24:
    			vo_draw_alpha_rgb24(w,h,src,srca,stride,((uint8_t *) priv->surface->pixels)+y0*priv->surface->pitch+x0,priv->surface->pitch);
		break;
		case IMGFMT_RGB32:
		case IMGFMT_BGR32:
    			vo_draw_alpha_rgb32(w,h,src,srca,stride,((uint8_t *) priv->surface->pixels)+y0*priv->surface->pitch+x0,priv->surface->pitch);
		break;
		}
        }
		else {
            x0 *= priv->rgbsurface->format->BytesPerPixel;
		switch(priv->format) {
		case IMGFMT_RGB15:
		case IMGFMT_BGR15:
    			vo_draw_alpha_rgb15(w,h,src,srca,stride,((uint8_t *) priv->rgbsurface->pixels)+y0*priv->rgbsurface->pitch+x0,priv->rgbsurface->pitch);
		break;
		case IMGFMT_RGB16:
		case IMGFMT_BGR16:
    			vo_draw_alpha_rgb16(w,h,src,srca,stride,((uint8_t *) priv->rgbsurface->pixels)+y0*priv->rgbsurface->pitch+x0,priv->rgbsurface->pitch);
		break;
		case IMGFMT_RGB24:
		case IMGFMT_BGR24:
    			vo_draw_alpha_rgb24(w,h,src,srca,stride,((uint8_t *) priv->rgbsurface->pixels)+y0*priv->rgbsurface->pitch+x0,priv->rgbsurface->pitch);
		break;
		case IMGFMT_RGB32:
		case IMGFMT_BGR32:
    			vo_draw_alpha_rgb32(w,h,src,srca,stride,((uint8_t *) priv->rgbsurface->pixels)+y0*priv->rgbsurface->pitch+x0,priv->rgbsurface->pitch);
		break;
		}
        }
        
  	}	
}


/**
 * Take a null-terminated array of pointers, and find the last element.
 *
 *    params : array == array of which we want to find the last element.
 *   returns : index of last NON-NULL element.
 **/

static inline int findArrayEnd (SDL_Rect **array)
{
	int i = 0;
	while ( array[i++] );	/* keep loopin' ... */
	
	/* return the index of the last array element */
	return i - 1;
}


/**
 * Open and prepare SDL output.
 *
 *    params : *plugin ==
 *             *name == 
 *   returns : 0 on success, -1 on failure
 **/
  
static int sdl_open (void *plugin, void *name)
{
	struct sdl_priv_s *priv = &sdl_priv;
	const SDL_VideoInfo *vidInfo = NULL;
	/*static int opened = 0;
	
	if (opened)
	    return 0;
	opened = 1;*/


	/* other default values */
	#ifdef SDL_NOHWSURFACE
		if( mp_msg_test(MSGT_VO,MSGL_V) ) {
			mp_msg(MSGT_VO,MSGL_V, "SDL: using software-surface\n"); }
		priv->sdlflags = SDL_SWSURFACE|SDL_RESIZABLE|SDL_ANYFORMAT;
		priv->sdlfullflags = SDL_SWSURFACE|SDL_FULLSCREEN|SDL_ANYFORMAT;
		// XXX:FIXME: ASYNCBLIT should be enabled for SMP systems
	#else	
		/*if((strcmp(priv->driver, "dga") == 0) && (priv->mode)) {
			if( mp_msg_test(MSGT_VO,MSGL_V) ) {
				printf("SDL: using software-surface\n"); }
			priv->sdlflags = SDL_SWSURFACE|SDL_FULLSCREEN|SDL_ASYNCBLIT|SDL_HWACCEL|SDL_ANYFORMAT;
			priv->sdlfullflags = SDL_SWSURFACE|SDL_FULLSCREEN|SDL_ASYNCBLIT|SDL_HWACCEL|SDL_ANYFORMAT;
		}	
		else {	*/
			if( mp_msg_test(MSGT_VO,MSGL_V) ) {
	 			mp_msg(MSGT_VO,MSGL_V, "SDL: using hardware-surface\n"); }
			priv->sdlflags = SDL_HWSURFACE|SDL_RESIZABLE/*|SDL_ANYFORMAT*/;
			priv->sdlfullflags = SDL_HWSURFACE|SDL_FULLSCREEN/*|SDL_ANYFORMAT*/;
			// XXX:FIXME: ASYNCBLIT should be enabled for SMP systems
		//}	
	#endif	

#if !defined( AMIGA ) && !defined( MACOSX ) 
	priv->sdlfullflags |= SDL_DOUBLEBUF;	
	if (vo_doublebuffering)
	    priv->sdlflags |= SDL_DOUBLEBUF;
#endif
	
	/* Setup Keyrepeats (500/30 are defaults) */
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100 /*SDL_DEFAULT_REPEAT_INTERVAL*/);

	/* get information about the graphics adapter */
	vidInfo = SDL_GetVideoInfo ();
	
	/* collect all fullscreen & hardware modes available */
	if (!(priv->fullmodes = SDL_ListModes (vidInfo->vfmt, priv->sdlfullflags))) {

		/* non hardware accelerated fullscreen modes */
		priv->sdlfullflags &= ~SDL_HWSURFACE;
 		priv->fullmodes = SDL_ListModes (vidInfo->vfmt, priv->sdlfullflags);
	}
	
	/* test for normal resizeable & windowed hardware accellerated surfaces */
	if (!SDL_ListModes (vidInfo->vfmt, priv->sdlflags)) {
		
		/* test for NON hardware accelerated resizeable surfaces - poor you. 
		 * That's all we have. If this fails there's nothing left.
		 * Theoretically there could be Fullscreenmodes left - we ignore this for now.
		 */
		priv->sdlflags &= ~SDL_HWSURFACE;
		if ((!SDL_ListModes (vidInfo->vfmt, priv->sdlflags)) && (!priv->fullmodes)) {
			mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SDL_CouldntGetAnyAcceptableSDLModeForOutput);
			return -1;
		}
	}
													
		
   /* YUV overlays need at least 16-bit color depth, but the
    * display might less. The SDL AAlib target says it can only do
    * 8-bits, for example. So, if the display is less than 16-bits,
    * we'll force the BPP to 16, and pray that SDL can emulate for us.
    */
	priv->bpp = vidInfo->vfmt->BitsPerPixel;
	if (priv->mode == YUV && priv->bpp < 16) {

		if( mp_msg_test(MSGT_VO,MSGL_V) )
 		    mp_msg(MSGT_VO,MSGL_V, "SDL: Your SDL display target wants to be at a color "
                           "depth of (%d), but we need it to be at least 16 "
                           "bits, so we need to emulate 16-bit color. This is "
                           "going to slow things down; you might want to "
                           "increase your display's color depth, if possible.\n",
                           priv->bpp);

		priv->bpp = 16;  
	}
	
	/* We don't want those in our event queue. 
	 * We use SDL_KEYUP cause SDL_KEYDOWN seems to cause problems
	 * with keys need to be pressed twice, to be recognized.
	 */
#ifndef BUGGY_SDL
	SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
//	SDL_EventState(SDL_QUIT, SDL_IGNORE);
	SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
	SDL_EventState(SDL_USEREVENT, SDL_IGNORE);
#endif
	
	/* Success! */
	return 0;
}


/**
 * Close SDL, Cleanups, Free Memory
 *
 *    params : *plugin
 *   returns : non-zero on success, zero on error.
 **/

static int sdl_close (void)
{
	struct sdl_priv_s *priv = &sdl_priv;

	if (priv->fullmode)
	    SDL_ShowCursor(1);

	/* Cleanup YUV Overlay structure */
	if (priv->overlay) {
		SDL_FreeYUVOverlay(priv->overlay);
		priv->overlay=NULL;
	}

	/* Free RGB Surface */
	if (priv->rgbsurface) {
		SDL_FreeSurface(priv->rgbsurface);
		priv->rgbsurface=NULL;
	}

	/* Free our blitting surface */
	if (priv->surface) {
		SDL_FreeSurface(priv->surface);
		priv->surface=NULL;
	}
	
	/* DON'T attempt to free the fullscreen modes array. SDL_Quit* does this for us */
	
	return 0;
}

/**
 * Do aspect ratio calculations
 *
 *   params : srcw == sourcewidth
 *            srch == sourceheight
 *            dstw == destinationwidth
 *            dsth == destinationheight
 *
 *  returns : SDL_Rect structure with new x and y, w and h
 **/

#if 0
static SDL_Rect aspect(int srcw, int srch, int dstw, int dsth) {
	SDL_Rect newres;
	if( mp_msg_test(MSGT_VO,MSGL_V) ) {
	 	mp_msg(MSGT_VO,MSGL_V, "SDL Aspect-Destinationres: %ix%i (x: %i, y: %i)\n", newres.w, newres.h, newres.x, newres.y); }
	newres.h = ((float)dstw / (float)srcw * (float)srch) * ((float)dsth/((float)dstw/(MONITOR_ASPECT)));
	if(newres.h > dsth) {
		newres.w = ((float)dsth / (float)newres.h) * dstw;
		newres.h = dsth;
		newres.x = (dstw - newres.w) / 2;
		newres.y = 0;
	}
	else {
		newres.w = dstw;
		newres.x = 0;
		newres.y = (dsth - newres.h) / 2;
	}
	
	if( mp_msg_test(MSGT_VO,MSGL_V) ) {
		mp_msg(MSGT_VO,MSGL_V, "SDL Mode: %d:  %d x %d\n", i, priv->fullmodes[i]->w, priv->fullmodes[i]->h); }

	return newres;
}
#endif

/**
 * Sets the specified fullscreen mode.
 *
 *   params : mode == index of the desired fullscreen mode
 *  returns : doesn't return
 **/

#if 0
static void set_fullmode (int mode)
{
	struct sdl_priv_s *priv = &sdl_priv;
	SDL_Surface *newsurface = NULL;
	int haspect, waspect = 0;
	
	/* if we haven't set a fullmode yet, default to the lowest res fullmode first */
	if (mode < 0) 
		mode = priv->fullmode = findArrayEnd(priv->fullmodes) - 1;

	/* Calculate proper aspect ratio for fullscreen
	 * Height smaller than expected: add horizontal black bars (haspect)*/
	haspect = (priv->width * (float) ((float) priv->fullmodes[mode]->h / (float) priv->fullmodes[mode]->w) - priv->height) * (float) ((float) priv->fullmodes[mode]->w / (float) priv->width);
	/* Height bigger than expected: add vertical black bars (waspect)*/
	if (haspect < 0) {
		haspect = 0; /* set haspect to zero because image will be scaled horizontal instead of vertical */
		waspect = priv->fullmodes[mode]->w - ((float) ((float) priv->fullmodes[mode]->h / (float) priv->height) * (float) priv->width);
	}	
//	printf ("W-Aspect: %i  H-Aspect: %i\n", waspect, haspect);
	
	/* change to given fullscreen mode and hide the mouse cursor */
	newsurface = SDL_SetVideoMode(priv->fullmodes[mode]->w - waspect, priv->fullmodes[mode]->h - haspect, priv->bpp, priv->sdlfullflags);
	
	/* if we were successfull hide the mouse cursor and save the mode */
	if (newsurface) {
		if (priv->surface)
	    	    SDL_FreeSurface(priv->surface);
		priv->surface = newsurface;
		SDL_ShowCursor(0);
	}
}
#endif

/* Set video mode. Not fullscreen */
static void set_video_mode(int width, int height, int bpp, uint32_t sdlflags)
{
	struct sdl_priv_s *priv = &sdl_priv;
    SDL_Surface* newsurface;    

    if(priv->rgbsurface)
	SDL_FreeSurface(priv->rgbsurface);
    else if(priv->overlay)
	SDL_FreeYUVOverlay(priv->overlay);
 
    priv->rgbsurface = NULL;
    priv->overlay = NULL;
 
    newsurface = SDL_SetVideoMode(width, height, bpp, sdlflags);

    if(newsurface) {

        /* priv->surface will be NULL the first time this function is called. */
        if(priv->surface)
            SDL_FreeSurface(priv->surface);

        priv->surface = newsurface;
        priv->dstwidth = width;
        priv->dstheight = height;

        setup_surfaces();
    }
    else
        mp_msg(MSGT_VO,MSGL_WARN, "set_video_mode: SDL_SetVideoMode failed: %s\n", SDL_GetError());
}

static void set_fullmode (int mode) {
	struct sdl_priv_s *priv = &sdl_priv;
	SDL_Surface *newsurface = NULL;
 	int screen_surface_w, screen_surface_h;
	
 	if(priv->rgbsurface)
	        SDL_FreeSurface(priv->rgbsurface);
 	else if(priv->overlay)
	        SDL_FreeYUVOverlay(priv->overlay);
 
 	priv->rgbsurface = NULL;
 	priv->overlay = NULL;
 
	/* if we haven't set a fullmode yet, default to the lowest res fullmode first */
	/* But select a mode where the full video enter */
	if(priv->X && priv->fulltype & VOFLAG_FULLSCREEN) {
		screen_surface_w = priv->XWidth;
		screen_surface_h = priv->XHeight;
	}
	else if (mode < 0) {
        int i,j,imax;
		mode = 0; // Default to the biggest mode avaible
		if ( mp_msg_test(MSGT_VO,MSGL_V) ) for(i=0;priv->fullmodes[i];++i)
 	           mp_msg(MSGT_VO,MSGL_V, "SDL Mode: %d:  %d x %d\n", i, priv->fullmodes[i]->w, priv->fullmodes[i]->h);
		for(i = findArrayEnd(priv->fullmodes) - 1; i >=0; i--) {
		  if( (priv->fullmodes[i]->w >= priv->dstwidth) && 
		      (priv->fullmodes[i]->h >= priv->dstheight) ) {
		      imax = i;
		      for (j = findArrayEnd(priv->fullmodes) - 1; j >=0; j--) {
			  if (priv->fullmodes[j]->w > priv->fullmodes[imax]->w
			      && priv->fullmodes[j]->h == priv->fullmodes[imax]->h)
			      imax = j;
		      }
		      mode = imax;
		      break;
		    }
		  }
		if ( mp_msg_test(MSGT_VO,MSGL_V) ) {
			mp_msg(MSGT_VO,MSGL_V, "SET SDL Mode: %d:  %d x %d\n", mode, priv->fullmodes[mode]->w, priv->fullmodes[mode]->h); }
		priv->fullmode = mode;
        screen_surface_h = priv->fullmodes[mode]->h;
        screen_surface_w = priv->fullmodes[mode]->w;
	}
    else {
       screen_surface_h = priv->fullmodes[mode]->h;
       screen_surface_w = priv->fullmodes[mode]->w;
	}
	
	aspect_save_screenres(screen_surface_w, screen_surface_h);

	/* calculate new video size/aspect */
	if(priv->mode == YUV) {
        if(priv->fulltype&VOFLAG_FULLSCREEN)
		aspect_save_screenres(priv->XWidth, priv->XHeight);

        aspect(&priv->dstwidth, &priv->dstheight, A_ZOOM);
	}

	/* try to change to given fullscreenmode */
	newsurface = SDL_SetVideoMode(priv->dstwidth, screen_surface_h, priv->bpp,
                                  priv->sdlfullflags);

	/*
	 * In Mac OS X (and possibly others?) SDL_SetVideoMode() appears to 
	 * destroy the datastructure previously retrived, so we need to 
	 * re-assign it.  The comment in sdl_close() seems to imply that we 
	 * should not free() anything.
	 */
	#ifdef SYS_DARWIN
	{
	const SDL_VideoInfo *vidInfo = NULL;
	vidInfo = SDL_GetVideoInfo ();

	/* collect all fullscreen & hardware modes available */
	if (!(priv->fullmodes = SDL_ListModes (vidInfo->vfmt, priv->sdlfullflags))) {

	    /* non hardware accelerated fullscreen modes */
	    priv->sdlfullflags &= ~SDL_HWSURFACE;
	    priv->fullmodes = SDL_ListModes (vidInfo->vfmt, priv->sdlfullflags);
	}
	}
	#endif


	
	/* if creation of new surface was successfull, save it and hide mouse cursor */
	if(newsurface) {
		if (priv->surface)
	    	    SDL_FreeSurface(priv->surface);
		priv->surface = newsurface;
		SDL_ShowCursor(0);
        SDL_SRF_LOCK(priv->surface, -1)
        SDL_FillRect(priv->surface, NULL, 0);
        SDL_SRF_UNLOCK(priv->surface)
        setup_surfaces();
	}		
    else
        mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SDL_SetVideoModeFailedFull, SDL_GetError());
}


/**
 * Initialize an SDL surface and an SDL YUV overlay.
 *
 *    params : width  == width of video we'll be displaying.
 *             height == height of video we'll be displaying.
 *             fullscreen == want to be fullscreen?
 *             title == Title for window titlebar.
 *   returns : non-zero on success, zero on error.
 **/

static int
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
//static int sdl_setup (int width, int height)
{
	struct sdl_priv_s *priv = &sdl_priv;

    switch(format){
        case IMGFMT_I420:
            mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SDL_MappingI420ToIYUV);
            format = SDL_IYUV_OVERLAY;
		case IMGFMT_YV12:
		case IMGFMT_IYUV:
		case IMGFMT_YUY2:
		case IMGFMT_UYVY:
		case IMGFMT_YVYU:
            priv->mode = YUV;
            break;
		case IMGFMT_BGR15:	
		case IMGFMT_BGR16:	
		case IMGFMT_BGR24:	
		case IMGFMT_BGR32:	
			priv->mode = BGR;
			break;
        case IMGFMT_RGB15:	
        case IMGFMT_RGB16:	
        case IMGFMT_RGB24:	
		case IMGFMT_RGB32:	
			priv->mode = RGB;
			break;
		default:
 			mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_SDL_UnsupportedImageFormat,format);
			return -1;
	}

    if ( vo_config_count ) sdl_close();

    if( mp_msg_test(MSGT_VO,MSGL_V) ) {
      mp_msg(MSGT_VO,MSGL_V, "SDL: Using 0x%X (%s) image format\n", format, vo_format_name(format)); }
    
    if(priv->mode != YUV) {
		priv->sdlflags |= SDL_ANYFORMAT;
		priv->sdlfullflags |= SDL_ANYFORMAT;
	}

    /* SDL can only scale YUV data */
    if(priv->mode == RGB || priv->mode == BGR) {
        d_width = width;
        d_height = height;
    }

    aspect_save_orig(width,height);
	aspect_save_prescale(d_width ? d_width : width, d_height ? d_height : height);

	/* Save the original Image size */
    priv->width  = width;
    priv->height = height;
    priv->dstwidth  = d_width ? d_width : width;
    priv->dstheight = d_height ? d_height : height;

    priv->format = format;
    
	if (sdl_open(NULL, NULL) != 0)
	    return -1;

	/* Set output window title */
	SDL_WM_SetCaption (".: MPlayer : F = Fullscreen/Windowed : C = Cycle Fullscreen Resolutions :.", title);
	//SDL_WM_SetCaption (title, title);

    if(priv->X) {
	aspect_save_screenres(priv->XWidth,priv->XHeight);
	aspect(&priv->dstwidth,&priv->dstheight,A_NOZOOM);
    }
    
	priv->windowsize.w = priv->dstwidth;
  	priv->windowsize.h = priv->dstheight;
        
	/* bit 0 (0x01) means fullscreen (-fs)
	 * bit 1 (0x02) means mode switching (-vm)
	 * bit 2 (0x04) enables software scaling (-zoom)
	 * bit 3 (0x08) enables flipping (-flip)
	 */
//      printf("SDL: flags are set to: %i\n", flags);
//	printf("SDL: Width: %i Height: %i D_Width %i D_Height: %i\n", width, height, d_width, d_height);
	if(flags&VOFLAG_FLIPPING) {
		if( mp_msg_test(MSGT_VO,MSGL_V) ) {
			mp_msg(MSGT_VO,MSGL_V, "SDL: using flipped video (only with RGB/BGR/packed YUV)\n"); }
		priv->flip = 1; 
	}
	if(flags&VOFLAG_FULLSCREEN) {
	  	if( mp_msg_test(MSGT_VO,MSGL_V) ) {
 	  	    mp_msg(MSGT_VO,MSGL_V, "SDL: setting zoomed fullscreen without modeswitching\n");}
 		    mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SDL_InfoPleaseUseVmOrZoom);
		priv->fulltype = VOFLAG_FULLSCREEN;
		set_fullmode(priv->fullmode);
          	/*if((priv->surface = SDL_SetVideoMode (d_width, d_height, priv->bpp, priv->sdlfullflags)))
			SDL_ShowCursor(0);*/
	} else	
	if(flags&VOFLAG_MODESWITCHING) {
	 	if( mp_msg_test(MSGT_VO,MSGL_V) ) {
 	 		mp_msg(MSGT_VO,MSGL_V, "SDL: setting zoomed fullscreen with modeswitching\n"); }
		priv->fulltype = VOFLAG_MODESWITCHING;
		set_fullmode(priv->fullmode);
          	/*if((priv->surface = SDL_SetVideoMode (d_width ? d_width : width, d_height ? d_height : height, priv->bpp, priv->sdlfullflags)))
			SDL_ShowCursor(0);*/
	} else
	if(flags&VOFLAG_SWSCALE) {
	 	if( mp_msg_test(MSGT_VO,MSGL_V) ) {
			mp_msg(MSGT_VO,MSGL_V, "SDL: setting zoomed fullscreen with modeswitching\n"); }
		priv->fulltype = VOFLAG_SWSCALE;
		set_fullmode(priv->fullmode);
	} 
        else {
		if((strcmp(priv->driver, "x11") == 0)
		||(strcmp(priv->driver, "windib") == 0)
		||(strcmp(priv->driver, "directx") == 0)
		||(strcmp(priv->driver, "Quartz") == 0)
		||(strcmp(priv->driver, "cgx") == 0)
		||(strcmp(priv->driver, "os4video") == 0)
		||((strcmp(priv->driver, "aalib") == 0) && priv->X)){
			if( mp_msg_test(MSGT_VO,MSGL_V) ) {
 				mp_msg(MSGT_VO,MSGL_V, "SDL: setting windowed mode\n"); }
            set_video_mode(priv->dstwidth, priv->dstheight, priv->bpp, priv->sdlflags);
		}
		else {
			if( mp_msg_test(MSGT_VO,MSGL_V) ) {
 				mp_msg(MSGT_VO,MSGL_V, "SDL: setting zoomed fullscreen with modeswitching\n"); }
			priv->fulltype = VOFLAG_SWSCALE;
			set_fullmode(priv->fullmode);
		}	
	}

        if(!priv->surface) { // cannot SetVideoMode
 		mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_SDL_FailedToSetVideoMode, SDL_GetError());
		return -1;
	}	

	return 0;
}

/* Free priv->rgbsurface or priv->overlay if they are != NULL.
 * Setup priv->rgbsurface or priv->overlay depending on source format.
 * The size of the created surface or overlay depends on the size of
 * priv->surface, priv->width, priv->height, priv->dstwidth and priv->dstheight.
 */
static int setup_surfaces(void)
{
    struct sdl_priv_s *priv = &sdl_priv;
    float v_scale = ((float) priv->dstheight) / priv->height;
    int surfwidth, surfheight;

    surfwidth = priv->width;
    surfheight = priv->height + (priv->surface->h - priv->dstheight) / v_scale;
    surfheight&= ~1;
    /* Place the image in the middle of the screen */
    priv->y = (surfheight - priv->height) / 2;
    priv->y_screen_top = priv->y * v_scale;
    priv->y_screen_bottom = priv->y_screen_top + priv->dstheight;

    priv->dirty_off_frame[0].x = -1;
    priv->dirty_off_frame[0].y = -1;
    priv->dirty_off_frame[1].x = -1;
    priv->dirty_off_frame[1].y = -1;

    /* Make sure the entire screen is updated */
    vo_osd_changed(1);

    if(priv->rgbsurface)
        SDL_FreeSurface(priv->rgbsurface);
    else if(priv->overlay)
        SDL_FreeYUVOverlay(priv->overlay);

    priv->rgbsurface = NULL;
    priv->overlay = NULL;
    
    if(priv->mode != YUV && (priv->format&0xFF) == priv->bpp) {
		if(strcmp(priv->driver, "x11") == 0) {
            priv->dblit = 1;
            priv->framePlaneRGB = priv->width * priv->height * priv->surface->format->BytesPerPixel;
            priv->stridePlaneRGB = priv->width * priv->surface->format->BytesPerPixel;
            erase_rectangle(0, 0, priv->surface->w, priv->surface->h);
            return 0;
        }
	}

	switch(priv->format) {
	    	/* Initialize and create the RGB Surface used for video out in BGR/RGB mode */
//SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);	
		//	SDL_SWSURFACE,SDL_HWSURFACE,SDL_SRCCOLORKEY, priv->flags?	guess: exchange Rmask and Bmask for BGR<->RGB
		// 32 bit: a:ff000000 r:ff000 g:ff00 b:ff
		// 24 bit: r:ff0000 g:ff00 b:ff
		// 16 bit: r:1111100000000000b g:0000011111100000b b:0000000000011111b
		// 15 bit: r:111110000000000b g:000001111100000b b:000000000011111b
		// FIXME: colorkey detect based on bpp, FIXME static bpp value, FIXME alpha value correct?
	    case IMGFMT_RGB15:
            priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 15, 31, 992, 31744, 0);
	    break;	
	    case IMGFMT_BGR15:
            priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 15, 31744, 992, 31, 0);
	    break;	
	    case IMGFMT_RGB16:
            priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 16, 31, 2016, 63488, 0);
	    break;	
	    case IMGFMT_BGR16:
            priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 16, 63488, 2016, 31, 0);
	    break;	
	    case IMGFMT_RGB24:
            priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
	    break;	
	    case IMGFMT_BGR24:
            priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 24, 0xFF0000, 0x00FF00, 0x0000FF, 0);
	    break;	
	    case IMGFMT_RGB32:
            priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0/*0xFF000000*/);
	    break;	
	    case IMGFMT_BGR32:
            priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0/*0xFF000000*/);
	    break;	
	    default:
		/* Initialize and create the YUV Overlay used for video out */
		if (!(priv->overlay = SDL_CreateYUVOverlay (surfwidth, surfheight, priv->format, priv->surface))) {
			mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_SDL_CouldntCreateAYUVOverlay, SDL_GetError());
			return -1;
		}
		priv->framePlaneY = priv->width * priv->height;
		priv->framePlaneUV = (priv->width * priv->height) >> 2;
		priv->framePlaneYUY = priv->width * priv->height * 2;
		priv->stridePlaneY = priv->width;
		priv->stridePlaneUV = priv->width/2;
		priv->stridePlaneYUY = priv->width * 2;
	}
	
    if(priv->mode != YUV) {
        if(!priv->rgbsurface) {
            mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_SDL_CouldntCreateARGBSurface, SDL_GetError());
            return -1;
        }

        priv->dblit = 0;

        if((priv->format&0xFF) != priv->bpp)
            mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SDL_UsingDepthColorspaceConversion, priv->format&0xFF, priv->bpp);

        priv->framePlaneRGB = priv->width * priv->height * priv->rgbsurface->format->BytesPerPixel;
        priv->stridePlaneRGB = priv->width * priv->rgbsurface->format->BytesPerPixel;
    }
    
    erase_rectangle(0, 0, surfwidth, surfheight);

	return 0;
}


/**
 * Draw a frame to the SDL YUV overlay.
 *
 *   params : *src[] == the Y, U, and V planes that make up the frame.
 *  returns : non-zero on success, zero on error.
 **/

//static int sdl_draw_frame (frame_t *frame)
static int draw_frame(uint8_t *src[])
{
	struct sdl_priv_s *priv = &sdl_priv;
	uint8_t *dst;
	int i;
	uint8_t *mysrc = src[0];

    switch(priv->format){
        case IMGFMT_YUY2:
        case IMGFMT_UYVY:
        case IMGFMT_YVYU:
        SDL_OVR_LOCK(-1)
        dst = (uint8_t *) *(priv->overlay->pixels) + priv->overlay->pitches[0]*priv->y;
	    if(priv->flip) {
	    	mysrc+=priv->framePlaneYUY;
		for(i = 0; i < priv->height; i++) {
			mysrc-=priv->stridePlaneYUY;
			memcpy (dst, mysrc, priv->stridePlaneYUY);
                dst+=priv->overlay->pitches[0];
		}
	    }
	    else memcpy (dst, src[0], priv->framePlaneYUY);
	    SDL_OVR_UNLOCK
            break;
	
	case IMGFMT_RGB15:
	case IMGFMT_BGR15:	
	case IMGFMT_RGB16:
	case IMGFMT_BGR16:	
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:	
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
		if(priv->dblit) {
			SDL_SRF_LOCK(priv->surface, -1)
			dst = (uint8_t *) priv->surface->pixels + priv->y*priv->surface->pitch;
			if(priv->flip) {
				mysrc+=priv->framePlaneRGB;
				for(i = 0; i < priv->height; i++) {
					mysrc-=priv->stridePlaneRGB;
					memcpy (dst, mysrc, priv->stridePlaneRGB);
					dst += priv->surface->pitch;
				}
			}
			else memcpy (dst, src[0], priv->framePlaneRGB);
			SDL_SRF_UNLOCK(priv->surface)
		} else {
			SDL_SRF_LOCK(priv->rgbsurface, -1)
			dst = (uint8_t *) priv->rgbsurface->pixels + priv->y*priv->rgbsurface->pitch;
			if(priv->flip) {
				mysrc+=priv->framePlaneRGB;
				for(i = 0; i < priv->height; i++) {
					mysrc-=priv->stridePlaneRGB;
					memcpy (dst, mysrc, priv->stridePlaneRGB);
					dst += priv->rgbsurface->pitch;
				}
			}
			else memcpy (dst, src[0], priv->framePlaneRGB);
			SDL_SRF_UNLOCK(priv->rgbsurface)
		}
		break;

        }
        	
	return 0;
}


/**
 * Draw a slice (16 rows of image) to the SDL YUV overlay.
 *
 *   params : *src[] == the Y, U, and V planes that make up the slice.
 *  returns : non-zero on error, zero on success.
 **/

//static uint32_t draw_slice(uint8_t *src[], uint32_t slice_num)
static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	struct sdl_priv_s *priv = &sdl_priv;
	uint8_t *dst;

    SDL_OVR_LOCK(-1)

    y += priv->y;
    
    dst = priv->overlay->pixels[0] + priv->overlay->pitches[0]*y + x;
    memcpy_pic(dst, image[0], w, h, priv->overlay->pitches[0], stride[0]);
    x/=2;y/=2;w/=2;h/=2;

    switch(priv->format) {
    case IMGFMT_YV12:
        dst = priv->overlay->pixels[2] + priv->overlay->pitches[2]*y + x;
	memcpy_pic(dst, image[1], w, h, priv->overlay->pitches[2], stride[1]);

        dst = priv->overlay->pixels[1] + priv->overlay->pitches[1]*y + x;
	memcpy_pic(dst, image[2], w, h, priv->overlay->pitches[1], stride[2]);

    break;
    case IMGFMT_I420:
    case IMGFMT_IYUV:
        dst = priv->overlay->pixels[1] + priv->overlay->pitches[1]*y + x;
	memcpy_pic(dst, image[1], w, h, priv->overlay->pitches[1], stride[1]);

        dst = priv->overlay->pixels[2] + priv->overlay->pitches[2]*y + x;
	memcpy_pic(dst, image[2], w, h, priv->overlay->pitches[2], stride[2]);

    break;
    default:
	mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_SDL_UnsupportedImageFormatInDrawslice);
    }

	SDL_OVR_UNLOCK

	return 0;
}



/**
 * Checks for SDL keypress and window resize events
 *
 *   params : none
 *  returns : doesn't return
 **/

#include "osdep/keycodes.h"

#define shift_key (event.key.keysym.mod==(KMOD_LSHIFT||KMOD_RSHIFT)) 
static void check_events (void)
{
	struct sdl_priv_s *priv = &sdl_priv;
	SDL_Event event;
	SDLKey keypressed = 0;
	
	/* Poll the waiting SDL Events */
	while ( SDL_PollEvent(&event) ) {
		switch (event.type) {

			/* capture window resize events */
			case SDL_VIDEORESIZE:
				if(!priv->dblit)
                    set_video_mode(event.resize.w, event.resize.h, priv->bpp, priv->sdlflags);

				/* save video extents, to restore them after going fullscreen */
			 	//if(!(priv->surface->flags & SDL_FULLSCREEN)) {
				    priv->windowsize.w = priv->surface->w;
				    priv->windowsize.h = priv->surface->h;
				//}
				if( mp_msg_test(MSGT_VO,MSGL_DBG3) ) {
 					mp_msg(MSGT_VO,MSGL_DBG3, "SDL: Window resize\n"); }
			break;
			
			case SDL_MOUSEBUTTONDOWN:
				if(vo_nomouse_input)
				    break;
					mplayer_put_key((MOUSE_BTN0+event.button.button-1) | MP_KEY_DOWN);
				break;			    
		
			case SDL_MOUSEBUTTONUP:
				if(vo_nomouse_input)
				    break;
				mplayer_put_key(MOUSE_BTN0+event.button.button-1);
				break;
	
			/* graphics mode selection shortcuts */
#ifdef BUGGY_SDL
			case SDL_KEYDOWN:
				switch(event.key.keysym.sym) {
                                case SDLK_UP: mplayer_put_key(KEY_UP); break;
                                case SDLK_DOWN: mplayer_put_key(KEY_DOWN); break;
                                case SDLK_LEFT: mplayer_put_key(KEY_LEFT); break;
                                case SDLK_RIGHT: mplayer_put_key(KEY_RIGHT); break;
                                case SDLK_LESS: mplayer_put_key(shift_key?'>':'<'); break;
                                case SDLK_GREATER: mplayer_put_key('>'); break;
                                case SDLK_ASTERISK:
				case SDLK_KP_MULTIPLY:
				case SDLK_SLASH:
				case SDLK_KP_DIVIDE:
				default: break;
				}
			break;
			case SDL_KEYUP:
#else			
			case SDL_KEYDOWN:
#endif			
				keypressed = event.key.keysym.sym;
				if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
 					mp_msg(MSGT_VO,MSGL_DBG2, "SDL: Key pressed: '%i'\n", keypressed); }

				/* c key pressed. c cycles through available fullscreenmodes, if we have some */
				if ( ((keypressed == SDLK_c)) && (priv->fullmodes) ) {
					/* select next fullscreen mode */
					priv->fullmode++;
					if (priv->fullmode > (findArrayEnd(priv->fullmodes) - 1)) priv->fullmode = 0;
					set_fullmode(priv->fullmode);
	
					if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
 						mp_msg(MSGT_VO,MSGL_DBG2, "SDL: Set next available fullscreen mode.\n"); }
				}

				else if ( keypressed == SDLK_n ) {
#ifdef HAVE_X11					
					aspect(&priv->dstwidth, &priv->dstheight,A_NOZOOM);
#endif					
					if (priv->surface->w != priv->dstwidth || priv->surface->h != priv->dstheight) {
                        set_video_mode(priv->dstwidth, priv->dstheight, priv->bpp, priv->sdlflags);
					    	priv->windowsize.w = priv->surface->w;
						priv->windowsize.h = priv->surface->h;
						if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
 							mp_msg(MSGT_VO,MSGL_DBG2, "SDL: Normal size\n"); }
					} else
					if (priv->surface->w != priv->dstwidth * 2 || priv->surface->h != priv->dstheight * 2) {
                        set_video_mode(priv->dstwidth * 2, priv->dstheight * 2, priv->bpp, priv->sdlflags);
					    	priv->windowsize.w = priv->surface->w;
						priv->windowsize.h = priv->surface->h;
						if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
 							mp_msg(MSGT_VO,MSGL_DBG2, "SDL: Double size\n"); }
					}
				}	

                                else switch(keypressed){
				case SDLK_RETURN: mplayer_put_key(KEY_ENTER);break;
                                case SDLK_ESCAPE: mplayer_put_key(KEY_ESC);break;
				case SDLK_q: mplayer_put_key('q');break;
 				case SDLK_F1: mplayer_put_key(KEY_F+1);break;
 				case SDLK_F2: mplayer_put_key(KEY_F+2);break;
 				case SDLK_F3: mplayer_put_key(KEY_F+3);break;
 				case SDLK_F4: mplayer_put_key(KEY_F+4);break;
 				case SDLK_F5: mplayer_put_key(KEY_F+5);break;
 				case SDLK_F6: mplayer_put_key(KEY_F+6);break;
 				case SDLK_F7: mplayer_put_key(KEY_F+7);break;
 				case SDLK_F8: mplayer_put_key(KEY_F+8);break;
 				case SDLK_F9: mplayer_put_key(KEY_F+9);break;
 				case SDLK_F10: mplayer_put_key(KEY_F+10);break;
 				case SDLK_F11: mplayer_put_key(KEY_F+11);break;
 				case SDLK_F12: mplayer_put_key(KEY_F+12);break;
                                /*case SDLK_o: mplayer_put_key('o');break;
                                case SDLK_SPACE: mplayer_put_key(' ');break;
                                case SDLK_p: mplayer_put_key('p');break;*/
				case SDLK_7: mplayer_put_key(shift_key?'/':'7');
                                case SDLK_PLUS: mplayer_put_key(shift_key?'*':'+');
                                case SDLK_KP_PLUS: mplayer_put_key('+');break;
                                case SDLK_MINUS:
                                case SDLK_KP_MINUS: mplayer_put_key('-');break;
				case SDLK_TAB: mplayer_put_key('\t');break;
				case SDLK_PAGEUP: mplayer_put_key(KEY_PAGE_UP);break;
				case SDLK_PAGEDOWN: mplayer_put_key(KEY_PAGE_DOWN);break;  
#ifdef BUGGY_SDL
                                case SDLK_UP:
                                case SDLK_DOWN:
                                case SDLK_LEFT:
                                case SDLK_RIGHT:
                                case SDLK_ASTERISK:
				case SDLK_KP_MULTIPLY:
				case SDLK_SLASH:
				case SDLK_KP_DIVIDE:
				break;
#else				
                                case SDLK_UP: mplayer_put_key(KEY_UP);break;
                                case SDLK_DOWN: mplayer_put_key(KEY_DOWN);break;
                                case SDLK_LEFT: mplayer_put_key(KEY_LEFT);break;
                                case SDLK_RIGHT: mplayer_put_key(KEY_RIGHT);break;
                                case SDLK_LESS: mplayer_put_key(shift_key?'>':'<'); break;
                                case SDLK_GREATER: mplayer_put_key('>'); break;
                                case SDLK_ASTERISK:
				case SDLK_KP_MULTIPLY: mplayer_put_key('*'); break;
				case SDLK_SLASH:
				case SDLK_KP_DIVIDE: mplayer_put_key('/'); break;
#endif				
				case SDLK_KP0: mplayer_put_key(KEY_KP0); break;
				case SDLK_KP1: mplayer_put_key(KEY_KP1); break;
				case SDLK_KP2: mplayer_put_key(KEY_KP2); break;
				case SDLK_KP3: mplayer_put_key(KEY_KP3); break;
				case SDLK_KP4: mplayer_put_key(KEY_KP4); break;
				case SDLK_KP5: mplayer_put_key(KEY_KP5); break;
				case SDLK_KP6: mplayer_put_key(KEY_KP6); break;
				case SDLK_KP7: mplayer_put_key(KEY_KP7); break;
				case SDLK_KP8: mplayer_put_key(KEY_KP8); break;
				case SDLK_KP9: mplayer_put_key(KEY_KP9); break;
				case SDLK_KP_PERIOD: mplayer_put_key(KEY_KPDEC); break;
				case SDLK_KP_ENTER: mplayer_put_key(KEY_KPENTER); break;
				default:
					//printf("got scancode: %d keysym: %d mod: %d %d\n", event.key.keysym.scancode, keypressed, event.key.keysym.mod);
					mplayer_put_key(keypressed);
                                }
                                
				break;
				case SDL_QUIT: mplayer_put_key(KEY_CLOSE_WIN);break;
		}
	}
}
#undef shift_key

/* Erase (paint it black) the rectangle specified by x, y, w and h in the surface
   or overlay which is used for OSD
*/
static void erase_rectangle(int x, int y, int w, int h)
{
    struct sdl_priv_s *priv = &sdl_priv;

    switch(priv->format) {
        case IMGFMT_YV12:  
        case IMGFMT_I420:
        case IMGFMT_IYUV:
        {
            SDL_OVR_LOCK((void) 0)
                        
                    /* Erase Y plane */
                erase_area_1(x, w, h,
                             priv->overlay->pitches[0], 0,
                             priv->overlay->pixels[0] +
                             priv->overlay->pitches[0]*y);
                        
                /* Erase U and V planes */
                w /= 2;
                x /= 2;
                h /= 2;
                y /= 2;
                        
                erase_area_1(x, w, h,
                             priv->overlay->pitches[1], 128,
                             priv->overlay->pixels[1] +
                             priv->overlay->pitches[1]*y);
                        
                erase_area_1(x, w, h,
                             priv->overlay->pitches[2], 128,
                             priv->overlay->pixels[2] +
                             priv->overlay->pitches[2]*y);
            SDL_OVR_UNLOCK
                break;
        }

        case IMGFMT_YUY2:
        case IMGFMT_YVYU:
        {
                /* yuy2 and yvyu represent black the same way */
            uint8_t yuy2_black[] = {0, 128, 0, 128};

            SDL_OVR_LOCK((void) 0)
                erase_area_4(x*2, w*2, h,
                             priv->overlay->pitches[0],
                             *((uint32_t*) yuy2_black),
                             priv->overlay->pixels[0] +
                             priv->overlay->pitches[0]*y);
            SDL_OVR_UNLOCK
                break;
        }
            
        case IMGFMT_UYVY:
        {
            uint8_t uyvy_black[] = {128, 0, 128, 0};

            SDL_OVR_LOCK((void) 0)
                erase_area_4(x*2, w*2, h,
                             priv->overlay->pitches[0],
                             *((uint32_t*) uyvy_black),
                             priv->overlay->pixels[0] +
                             priv->overlay->pitches[0]*y);
            SDL_OVR_UNLOCK
                break;
        }
            
        case IMGFMT_RGB15:
        case IMGFMT_BGR15:
        case IMGFMT_RGB16:
        case IMGFMT_BGR16:
        case IMGFMT_RGB24:
        case IMGFMT_BGR24:
        case IMGFMT_RGB32:
        case IMGFMT_BGR32:
        {
            SDL_Rect rect;
            rect.w = w; rect.h = h;
            rect.x = x; rect.y = y;

            if(priv->dblit) {
                SDL_SRF_LOCK(priv->surface, (void) 0)
                    SDL_FillRect(priv->surface, &rect, 0);
                SDL_SRF_UNLOCK(priv->surface)
                    }
            else {
                SDL_SRF_LOCK(priv->rgbsurface, (void) 0)
                    SDL_FillRect(priv->rgbsurface, &rect, 0);
                SDL_SRF_UNLOCK(priv->rgbsurface)
                    }
            break;
        }
    }
}

static void draw_osd(void)
{	struct sdl_priv_s *priv = &sdl_priv;

    priv->osd_has_changed = vo_osd_changed(0);

    if(priv->osd_has_changed)
    {
        int i;
        
        for(i = 0; i < 2; i++) {
            if(priv->dirty_off_frame[i].x < 0 || priv->dirty_off_frame[i].y < 0)
                continue;

            erase_rectangle(priv->dirty_off_frame[i].x, priv->dirty_off_frame[i].y,
                            priv->dirty_off_frame[i].w, priv->dirty_off_frame[i].h);
            
            priv->dirty_off_frame[i].x = -1;
            priv->dirty_off_frame[i].y = -1;
        }
    }
 
	/* update osd/subtitles */
    if(priv->mode == YUV)
        vo_draw_text(priv->overlay->w, priv->overlay->h, draw_alpha);
    else {
        if(priv->dblit)
            vo_draw_text(priv->surface->w, priv->surface->h, draw_alpha);
        else
            vo_draw_text(priv->rgbsurface->w, priv->rgbsurface->h, draw_alpha);
    }
}

/* Fill area beginning at 'pixels' with 'color'. 'x_start', 'width' and 'pitch'
 * are given in bytes. 4 bytes at a time.
 */
static void erase_area_4(int x_start, int width, int height, int pitch, uint32_t color, uint8_t* pixels)
{
    int x_end = x_start/4 + width/4;
    int x, y;
    uint32_t* data = (uint32_t*) pixels;
    
    x_start /= 4;
    pitch /= 4;

    for(y = 0; y < height; y++) {
        for(x = x_start; x < x_end; x++)
            data[y*pitch + x] = color;
    }
}

/* Fill area beginning at 'pixels' with 'color'. 'x_start', 'width' and 'pitch'
 * are given in bytes. 1 byte at a time.
 */
static void erase_area_1(int x_start, int width, int height, int pitch, uint8_t color, uint8_t* pixels)
{
    int y;
    
    for(y = 0; y < height; y++) {
        memset(&pixels[y*pitch + x_start], color, width);
    }
}

/**
 * Display the surface we have written our data to
 *
 *   params : mode == index of the desired fullscreen mode
 *  returns : doesn't return
 **/

static void flip_page (void)
{
	struct sdl_priv_s *priv = &sdl_priv;

	switch(priv->format) {
	    case IMGFMT_RGB15:
	    case IMGFMT_BGR15:	
	    case IMGFMT_RGB16:
	    case IMGFMT_BGR16:	
	    case IMGFMT_RGB24:
	    case IMGFMT_BGR24:	
	    case IMGFMT_RGB32:
	    case IMGFMT_BGR32:
		if(!priv->dblit) {
		  	/* blit to the RGB surface */
			if(SDL_BlitSurface (priv->rgbsurface, NULL, priv->surface, NULL))
				mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_SDL_BlitFailed, SDL_GetError());
		}

		/* update screen */
		//SDL_UpdateRect(priv->surface, 0, 0, priv->surface->clip_rect.w, priv->surface->clip_rect.h);
        if(priv->osd_has_changed) {
            priv->osd_has_changed = 0;
		SDL_UpdateRects(priv->surface, 1, &priv->surface->clip_rect);
        }
        else
            SDL_UpdateRect(priv->surface, 0, priv->y_screen_top,
                           priv->surface->clip_rect.w, priv->y_screen_bottom);
		
		/* check if we have a double buffered surface and flip() if we do. */
		if ( priv->surface->flags & SDL_DOUBLEBUF )
			SDL_Flip(priv->surface);

	    break;
	    default:		
		/* blit to the YUV overlay */
		SDL_DisplayYUVOverlay (priv->overlay, &priv->surface->clip_rect);
		
		/* check if we have a double buffered surface and flip() if we do. */
		if ( priv->surface->flags & SDL_DOUBLEBUF )
			SDL_Flip(priv->surface);
		
		//SDL_LockYUVOverlay (priv->overlay); // removed because unused!?
	}	
}

static int
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
// it seems buggy (not hw accelerated), so just use YV12 instead!
//    case IMGFMT_I420:
//    case IMGFMT_IYUV:
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
    case IMGFMT_YVYU:
        return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD |
            VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
    case IMGFMT_RGB15:
    case IMGFMT_BGR15:
    case IMGFMT_RGB16:
    case IMGFMT_BGR16:
    case IMGFMT_RGB24:
    case IMGFMT_BGR24:
    case IMGFMT_RGB32:
    case IMGFMT_BGR32:
        return VFCAP_CSP_SUPPORTED | VFCAP_OSD | VFCAP_FLIP;
    }
    return 0;
}


static void
uninit(void)
{
#ifdef HAVE_X11
    struct sdl_priv_s *priv = &sdl_priv;
    if(priv->X) {
		if( mp_msg_test(MSGT_VO,MSGL_V) ) {
 			mp_msg(MSGT_VO,MSGL_V, "SDL: activating XScreensaver/DPMS\n"); }
		vo_x11_uninit();
	}
#endif
	sdl_close();

	/* Cleanup SDL */
    if(SDL_WasInit(SDL_INIT_VIDEO))
        SDL_QuitSubSystem(SDL_INIT_VIDEO);

	if( mp_msg_test(MSGT_VO,MSGL_DBG3) ) {
 		mp_msg(MSGT_VO,MSGL_DBG3, "SDL: Closed Plugin\n"); }

}

static int preinit(const char *arg)
{
    struct sdl_priv_s *priv = &sdl_priv;
    char * sdl_driver = NULL;
    int sdl_hwaccel;
    int sdl_forcexv;
    opt_t subopts[] = {
	    {"forcexv", OPT_ARG_BOOL,  &sdl_forcexv, NULL, 0},
	    {"hwaccel", OPT_ARG_BOOL,  &sdl_hwaccel, NULL, 0},
	    {"driver",  OPT_ARG_MSTRZ, &sdl_driver,  NULL, 0},
	    {NULL, 0, NULL, NULL, 0}
    };

    sdl_forcexv = 1;
    sdl_hwaccel = 1;

    if (subopt_parse(arg, subopts) != 0) return -1;

    priv->rgbsurface = NULL;
    priv->overlay = NULL;
    priv->surface = NULL;

    if( mp_msg_test(MSGT_VO,MSGL_DBG3) ) {
        mp_msg(MSGT_VO,MSGL_DBG3, "SDL: Opening Plugin\n"); }

    if(sdl_driver) {
        setenv("SDL_VIDEODRIVER", sdl_driver, 1);
    free(sdl_driver);
    }

    /* does the user want SDL to try and force Xv */
    if(sdl_forcexv)	setenv("SDL_VIDEO_X11_NODIRECTCOLOR", "1", 1);
    else setenv("SDL_VIDEO_X11_NODIRECTCOLOR", "0", 1);

    /* does the user want to disable Xv and use software scaling instead */
    if(sdl_hwaccel) setenv("SDL_VIDEO_YUV_HWACCEL", "1", 1);
    else setenv("SDL_VIDEO_YUV_HWACCEL", "0", 1);

    /* default to no fullscreen mode, we'll set this as soon we have the avail. modes */
    priv->fullmode = -2;

    priv->fullmodes = NULL;
    priv->bpp = 0;

    /* initialize the SDL Video system */
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        if (SDL_Init (SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE)) {
            mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SDL_InitializationFailed, SDL_GetError());

            return -1;
        }
    }

    SDL_VideoDriverName(priv->driver, 8);
    mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SDL_UsingDriver, priv->driver);

    priv->X = 0;
#ifdef HAVE_X11
    if(vo_init()) {
		if( mp_msg_test(MSGT_VO,MSGL_V) ) {
			mp_msg(MSGT_VO,MSGL_V, "SDL: deactivating XScreensaver/DPMS\n"); }
		priv->XWidth = vo_screenwidth;
		priv->XHeight = vo_screenheight;
		priv->X = 1;
		if( mp_msg_test(MSGT_VO,MSGL_V) ) {
			mp_msg(MSGT_VO,MSGL_V, "SDL: X11 Resolution %ix%i\n", priv->XWidth, priv->XHeight); }
	}
#endif

    return 0;
}

static uint32_t get_image(mp_image_t *mpi)
{
    struct sdl_priv_s *priv = &sdl_priv;

    if(priv->format != mpi->imgfmt) return VO_FALSE;
    if(mpi->type == MP_IMGTYPE_STATIC || mpi->type == MP_IMGTYPE_TEMP) {
        if(mpi->flags&MP_IMGFLAG_PLANAR) {
	    mpi->planes[0] = priv->overlay->pixels[0] + priv->y*priv->overlay->pitches[0];
	    mpi->stride[0] = priv->overlay->pitches[0];
	    if(mpi->flags&MP_IMGFLAG_SWAPPED) {
		mpi->planes[1] = priv->overlay->pixels[1] + priv->y*priv->overlay->pitches[1]/2;
		mpi->stride[1] = priv->overlay->pitches[1];
		mpi->planes[2] = priv->overlay->pixels[2] + priv->y*priv->overlay->pitches[2]/2;
		mpi->stride[2] = priv->overlay->pitches[2];
	    } else {
		mpi->planes[2] = priv->overlay->pixels[1] + priv->y*priv->overlay->pitches[1]/2;
		mpi->stride[2] = priv->overlay->pitches[1];
		mpi->planes[1] = priv->overlay->pixels[2] + priv->y*priv->overlay->pitches[2]/2;
		mpi->stride[1] = priv->overlay->pitches[2];
	    }
        }
        else if(IMGFMT_IS_RGB(priv->format) || IMGFMT_IS_BGR(priv->format)) {
            if(priv->dblit) {
                if(mpi->type == MP_IMGTYPE_STATIC && (priv->surface->flags & SDL_DOUBLEBUF))
                    return VO_FALSE;
                
                mpi->planes[0] = priv->surface->pixels + priv->y*priv->surface->pitch;
                mpi->stride[0] = priv->surface->pitch;
            }
            else {
                mpi->planes[0] = priv->rgbsurface->pixels + priv->y*priv->rgbsurface->pitch;
                mpi->stride[0] = priv->rgbsurface->pitch;
            }
        }
        else {
            mpi->planes[0] = priv->overlay->pixels[0] + priv->y*priv->overlay->pitches[0];
            mpi->stride[0] = priv->overlay->pitches[0];
        }

        mpi->flags|=MP_IMGFLAG_DIRECT;
        return VO_TRUE;
    }

    return VO_FALSE;
}

static int control(uint32_t request, void *data, ...)
{
  struct sdl_priv_s *priv = &sdl_priv;
  switch (request) {
  case VOCTRL_GET_IMAGE:
      return get_image(data);
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_FULLSCREEN:
    if (priv->surface->flags & SDL_FULLSCREEN) {
      set_video_mode(priv->windowsize.w, priv->windowsize.h, priv->bpp, priv->sdlflags);
      SDL_ShowCursor(1);
      if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
	mp_msg(MSGT_VO,MSGL_DBG2, "SDL: Windowed mode\n"); }
    } else if (priv->fullmodes) {
      set_fullmode(priv->fullmode);
      if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) {
	mp_msg(MSGT_VO,MSGL_DBG2, "SDL: Set fullscreen mode\n"); }
    }
    return VO_TRUE;
  }

  return VO_NOTIMPL;
}
