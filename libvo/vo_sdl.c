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
 *  from http://mplayer.sourceforge.net.
 *
 *  Current license is not decided yet, but we're heading for GPL.
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

#include "../config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "fastmemcpy.h"
#include "sub.h"
#include "aspect.h"

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include "x11_common.h"
#endif

LIBVO_EXTERN(sdl)

extern int verbose;
int sdl_noxv;
int sdl_forcexv;

static vo_info_t vo_info = 
{
	"SDL YUV/RGB/BGR renderer (SDL v1.1.7+ only!)",
	"sdl",
	"Ryan C. Gordon <icculus@lokigames.com>, Felix Buenemann <atmosfear@users.sourceforge.net>",
	""
};

#include <SDL.h>

#if	defined(sun) && defined(__svr4__)
/* setenv is missing on solaris */
static void setenv(const char *name, const char *val, int _xx)
{
    int len  = strlen(name) + strlen(val) + 2;
    char *env = malloc(len);

    if (env != NULL) {
	strcpy(env, name);
	strcat(env, "=");
	strcat(env, val);
	putenv(env);
    }
}
#endif


#define FS 0x01
#define VM 0x02
#define ZOOM 0x04
#define FLIP 0x08  

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

#ifdef HAVE_X11
	/* X11 Resolution */
	int XWidth, XHeight;
#endif
	
        /* original image dimensions */
	int width, height;

	/* destination dimensions */
	int dstwidth, dstheight;

	/* source image format (YUV/RGB/...) */
        int format;
} sdl_priv;


/** libvo Plugin functions **/

/**
 * draw_alpha is used for osd and subtitle display.
 *
 **/

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
	struct sdl_priv_s *priv = &sdl_priv;
	
	switch(priv->format) {
		case IMGFMT_YV12:  
		case IMGFMT_I420:
        	case IMGFMT_IYUV:
    			vo_draw_alpha_yv12(w,h,src,srca,stride,((uint8_t *) *(priv->overlay->pixels))+priv->width*y0+x0,priv->width);
		break;
		case IMGFMT_YUY2:
        	case IMGFMT_YVYU:		
    			vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) *(priv->overlay->pixels))+2*(priv->width*y0+x0),2*priv->width);
		break;	
        	case IMGFMT_UYVY:
    			vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) *(priv->overlay->pixels))+2*(priv->width*y0+x0)+1,2*priv->width);
		break;
		default:
		if(priv->dblit)		
		switch(priv->format) {
		case IMGFMT_RGB15:
		case IMGFMT_BGR15:
    			vo_draw_alpha_rgb15(w,h,src,srca,stride,((uint8_t *) priv->surface->pixels)+2*(y0*priv->width+x0),2*priv->width);
		break;
		case IMGFMT_RGB16:
		case IMGFMT_BGR16:
    			vo_draw_alpha_rgb16(w,h,src,srca,stride,((uint8_t *) priv->surface->pixels)+2*(y0*priv->width+x0),2*priv->width);
		break;
		case IMGFMT_RGB24:
		case IMGFMT_BGR24:
    			vo_draw_alpha_rgb24(w,h,src,srca,stride,((uint8_t *) priv->surface->pixels)+3*(y0*priv->width+x0),3*priv->width);
		break;
		case IMGFMT_RGB32:
		case IMGFMT_BGR32:
    			vo_draw_alpha_rgb32(w,h,src,srca,stride,((uint8_t *) priv->surface->pixels)+4*(y0*priv->width+x0),4*priv->width);
		break;
		}
		else
		switch(priv->format) {		
		case IMGFMT_RGB15:
		case IMGFMT_BGR15:
    			vo_draw_alpha_rgb15(w,h,src,srca,stride,((uint8_t *) priv->rgbsurface->pixels)+2*(y0*priv->width+x0),2*priv->width);
		break;
		case IMGFMT_RGB16:
		case IMGFMT_BGR16:
    			vo_draw_alpha_rgb16(w,h,src,srca,stride,((uint8_t *) priv->rgbsurface->pixels)+2*(y0*priv->width+x0),2*priv->width);
		break;
		case IMGFMT_RGB24:
		case IMGFMT_BGR24:
    			vo_draw_alpha_rgb24(w,h,src,srca,stride,((uint8_t *) priv->rgbsurface->pixels)+3*(y0*priv->width+x0),3*priv->width);
		break;
		case IMGFMT_RGB32:
		case IMGFMT_BGR32:
    			vo_draw_alpha_rgb32(w,h,src,srca,stride,((uint8_t *) priv->rgbsurface->pixels)+4*(y0*priv->width+x0),4*priv->width);
		break;
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
	static int opened = 0;
	
	if (opened)
	    return 0;
	opened = 1;

	if(verbose > 2) printf("SDL: Opening Plugin\n");

	if(vo_subdevice) setenv("SDL_VIDEODRIVER", vo_subdevice, 1);

	/* does the user want SDL to try and force Xv */
	if(sdl_forcexv)	setenv("SDL_VIDEO_X11_NODIRECTCOLOR", "1", 1);
	
	/* does the user want to disable Xv and use software scaling instead */
	if(sdl_noxv) setenv("SDL_VIDEO_YUV_HWACCEL", "0", 1);
	
	
	/* default to no fullscreen mode, we'll set this as soon we have the avail. modes */
	priv->fullmode = -2;
	
	priv->surface = NULL;
	priv->rgbsurface = NULL;
	priv->overlay = NULL;
	priv->fullmodes = NULL;
	priv->bpp = 0;

	/* initialize the SDL Video system */
	if (SDL_Init (SDL_INIT_VIDEO/*|SDL_INIT_NOPARACHUTE*/)) {
		printf("SDL: Initializing of SDL failed: %s.\n", SDL_GetError());
		return -1;
	}
	
	SDL_VideoDriverName(priv->driver, 8);
	printf("SDL: Using driver: %s\n", priv->driver);
	/* other default values */
	#ifdef SDL_NOHWSURFACE
		if(verbose) printf("SDL: using software-surface\n");
		priv->sdlflags = SDL_SWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_ANYFORMAT;
		priv->sdlfullflags = SDL_SWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_ASYNCBLIT|SDL_ANYFORMAT;
	#else	
		/*if((strcmp(priv->driver, "dga") == 0) && (priv->mode)) {
			if(verbose) printf("SDL: using software-surface\n");
			priv->sdlflags = SDL_SWSURFACE|SDL_FULLSCREEN|SDL_ASYNCBLIT|SDL_HWACCEL|SDL_ANYFORMAT;
			priv->sdlfullflags = SDL_SWSURFACE|SDL_FULLSCREEN|SDL_ASYNCBLIT|SDL_HWACCEL|SDL_ANYFORMAT;
		}	
		else {	*/
			if(verbose) printf("SDL: using hardware-surface\n");
			priv->sdlflags = SDL_HWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_HWACCEL/*|SDL_ANYFORMAT*/;
			priv->sdlfullflags = SDL_HWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_ASYNCBLIT|SDL_HWACCEL/*|SDL_ANYFORMAT*/;
		//}	
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
			printf("SDL: Couldn't get any acceptable SDL Mode for output.\n");
			return -1;
		}
	}
													
		
   /* YUV overlays need at least 16-bit color depth, but the
    * display might less. The SDL AAlib target says it can only do
    * 8-bits, for example. So, if the display is less than 16-bits,
    * we'll force the BPP to 16, and pray that SDL can emulate for us.
    */
	priv->bpp = vidInfo->vfmt->BitsPerPixel;
	if (!priv->mode && priv->bpp < 16) {

		if(verbose) printf("SDL: Your SDL display target wants to be at a color depth of (%d), but we need it to be at\
least 16 bits, so we need to emulate 16-bit color. This is going to slow things down; you might want to\
increase your display's color depth, if possible.\n", priv->bpp);

		priv->bpp = 16;  
	}
	
	/* We don't want those in our event queue. 
	 * We use SDL_KEYUP cause SDL_KEYDOWN seems to cause problems
	 * with keys need to be pressed twice, to be recognized.
	 */
#ifndef BUGGY_SDL
	SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_IGNORE);
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

	/* Cleanup YUV Overlay structure */
	if (priv->overlay) 
		SDL_FreeYUVOverlay(priv->overlay);

	/* Free RGB Surface */
	if (priv->rgbsurface)
		SDL_FreeSurface(priv->rgbsurface);

	/* Free our blitting surface */
	if (priv->surface)
		SDL_FreeSurface(priv->surface);
	
	/* DONT attempt to free the fullscreen modes array. SDL_Quit* does this for us */
	
	/* Cleanup SDL */
	//SDL_Quit();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	/* might have to be changed to quitsubsystem only, if plugins become
	 * changeable on the fly */

	if(verbose > 2) printf("SDL: Closed Plugin\n");

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
	if(verbose > 1) printf("SDL Aspect: src: %ix%i dst: %ix%i\n", srcw, srch, dstw, dsth);
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
	
	if(verbose) printf("SDL Aspect-Destinationres: %ix%i (x: %i, y: %i)\n", newres.w, newres.h, newres.x, newres.y);

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

static void set_fullmode (int mode) {
	struct sdl_priv_s *priv = &sdl_priv;
	SDL_Surface *newsurface = NULL;
	int newwidth = priv->dstwidth,
	    newheight= priv->dstheight;
	
	/* if we haven't set a fullmode yet, default to the lowest res fullmode first */
	if(mode < 0) 
		mode = priv->fullmode = findArrayEnd(priv->fullmodes) - 1;
	
	/* calculate new video size/aspect */
	if(!priv->mode) {
	if(priv->fulltype&FS) {
		aspect(&newwidth, &newheight, priv->XWidth ? priv->XWidth : priv->dstwidth, priv->XHeight ? priv->XHeight : priv->dstheight);
	} else
	if(priv->fulltype&VM) {	
		aspect(&newwidth, &newheight, priv->dstwidth, (int)((float)priv->dstwidth*((float)priv->XHeight / (float)priv->XWidth)));
	} else {
		aspect(&newwidth, &newheight, priv->fullmodes[mode]->w, priv->fullmodes[mode]->h);
	}
	}

	/* try to change to given fullscreenmode */
	newsurface = SDL_SetVideoMode(newwidth, newheight, priv->bpp, priv->sdlfullflags);
	
	/* if creation of new surface was successfull, save it and hide mouse cursor */
	if(newsurface) {
		if (priv->surface)
	    	    SDL_FreeSurface(priv->surface);
		priv->surface = newsurface;
		SDL_ShowCursor(0);
	}		
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

static uint32_t
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
//static int sdl_setup (int width, int height)
{
	struct sdl_priv_s *priv = &sdl_priv;
        unsigned int sdl_format;
#ifdef HAVE_X11	
	static Display *XDisplay;
	static int XScreen;
#endif
	int newwidth, newheight;

	sdl_format = format;
        switch(format){
		case IMGFMT_YV12:
			if(verbose) printf("SDL: Using 0x%X (YV12) image format\n", format); break;
		case IMGFMT_IYUV:
			if(verbose) printf("SDL: Using 0x%X (IYUV) image format\n", format); break;
		case IMGFMT_YUY2:
			if(verbose) printf("SDL: Using 0x%X (YUY2) image format\n", format); break;
		case IMGFMT_UYVY:
			if(verbose) printf("SDL: Using 0x%X (UYVY) image format\n", format); break;
		case IMGFMT_YVYU:
			if(verbose) printf("SDL: Using 0x%X (YVYU) image format\n", format); break;
		case IMGFMT_I420:
			if(verbose) printf("SDL: Using 0x%X (I420) image format\n", format);
			printf("SDL: Mapping I420 to IYUV\n");
			sdl_format = SDL_IYUV_OVERLAY;
		break;
		case IMGFMT_BGR15:	
			if(verbose) printf("SDL: Using 0x%X (BGR15) image format\n", format);
			priv->mode = BGR;
			break;
		case IMGFMT_RGB15:	
			if(verbose) printf("SDL: Using 0x%X (RGB15) image format\n", format);
			priv->mode = RGB;
			break;
		case IMGFMT_BGR16:	
			if(verbose) printf("SDL: Using 0x%X (BGR16) image format\n", format);
			priv->mode = BGR;
			break;
		case IMGFMT_RGB16:	
			if(verbose) printf("SDL: Using 0x%X (RGB16) image format\n", format);
			priv->mode = RGB;
			break;
		case IMGFMT_BGR24:	
			if(verbose) printf("SDL: Using 0x%X (BGR24) image format\n", format);
			priv->mode = BGR;
			break;
		case IMGFMT_RGB24:	
			if(verbose) printf("SDL: Using 0x%X (RGB24) image format\n", format);
			priv->mode = RGB;
			break;
		case IMGFMT_BGR32:	
			if(verbose) printf("SDL: Using 0x%X (BGR32) image format\n", format);
			priv->mode = BGR;
			break;
		case IMGFMT_RGB32:	
			if(verbose) printf("SDL: Using 0x%X (RGB32) image format\n", format);
			priv->mode = RGB;
			break;
		default:
			printf("SDL: Unsupported image format (0x%X)\n",format);
			return -1;
	}
	if(priv->mode) {
		priv->sdlflags |= SDL_ANYFORMAT;
		priv->sdlfullflags |= SDL_ANYFORMAT;
	}

#ifdef HAVE_X11
	if(getenv("DISPLAY")) {
		if(verbose) printf("SDL: deactivating XScreensaver/DPMS\n");
		XDisplay = XOpenDisplay(getenv("DISPLAY"));
		XScreen = DefaultScreen(XDisplay);
		priv->XWidth = DisplayWidth(XDisplay, XScreen);
		priv->XHeight = DisplayHeight(XDisplay, XScreen);
		priv->X = 1;
		if(verbose) printf("SDL: X11 Resolution %ix%i\n", priv->XWidth, priv->XHeight);
		saver_off(XDisplay);
		XCloseDisplay(XDisplay);
	}
#endif
	if (sdl_open(NULL, NULL) != 0)
	    return -1;

	/* Set output window title */
	SDL_WM_SetCaption (".: MPlayer : F = Fullscreen/Windowed : C = Cycle Fullscreen Resolutions :.", title);
	//SDL_WM_SetCaption (title, title);

	/* Save the original Image size */
	
	priv->width  = width;
	priv->height = height;
	priv->dstwidth  = d_width ? d_width : width;
	priv->dstheight = d_height ? d_height : height;
	newwidth  = priv->dstwidth;
	newheight = priv->dstheight;

	/*priv->width  = res.w;
	priv->height = res.h;*/
        priv->format = format;
#ifdef HAVE_X11
	aspect(&newwidth, &newheight, priv->dstwidth, (int)((float)priv->dstwidth*((float)priv->XHeight / (float)priv->XWidth)));
#endif
	priv->windowsize.w = newwidth;
  	priv->windowsize.h = newheight;
        
	/* bit 0 (0x01) means fullscreen (-fs)
	 * bit 1 (0x02) means mode switching (-vm)
	 * bit 2 (0x04) enables software scaling (-zoom)
	 * bit 3 (0x08) enables flipping (-flip)
	 */
//      printf("SDL: flags are set to: %i\n", flags);
//	printf("SDL: Width: %i Height: %i D_Width %i D_Height: %i\n", width, height, d_width, d_height);
	if(flags&FLIP) {
		if(verbose) printf("SDL: using flipped video (only with RGB/BGR/packed YUV)\n");
		priv->flip = 1; 
	}
	if(flags&FS) {
	  	if(verbose) printf("SDL: setting zoomed fullscreen without modeswitching\n");
		printf("SDL: Info - please use -vm (unscaled) or -zoom (scaled) for best fullscreen experience\n");
		priv->fulltype = FS;
		set_fullmode(priv->fullmode);
          	/*if((priv->surface = SDL_SetVideoMode (d_width, d_height, priv->bpp, priv->sdlfullflags)))
			SDL_ShowCursor(0);*/
	} else	
	if(flags&VM) {
	 	if(verbose) printf("SDL: setting nonzoomed fullscreen with modeswitching\n");
		printf("SDL: Info - please use -zoom switch to scale video\n");
		priv->fulltype = VM;
		set_fullmode(priv->fullmode);
          	/*if((priv->surface = SDL_SetVideoMode (d_width ? d_width : width, d_height ? d_height : height, priv->bpp, priv->sdlfullflags)))
			SDL_ShowCursor(0);*/
	} else
	if(flags&ZOOM) {
	 	if(verbose) printf("SDL: setting zoomed fullscreen with modeswitching\n");
		printf("SDL: Info - please use -vm switch instead if you don't want scaled video\n");
		priv->fulltype = ZOOM;
		set_fullmode(priv->fullmode);
	} 
        else {
		if((strcmp(priv->driver, "x11") == 0) || ((strcmp(priv->driver, "aalib") == 0) && priv->X)) {
			if(verbose) printf("SDL: setting windowed mode\n");
          	priv->surface = SDL_SetVideoMode (newwidth, newheight, priv->bpp, priv->sdlflags);
		}
		else {
			if(verbose) printf("SDL: setting zoomed fullscreen with modeswitching\n");
			printf("SDL: Info - please use -vm switch instead if you don't want scaled video\n");
			priv->fulltype = ZOOM;
			set_fullmode(priv->fullmode);
		}	
	}

        if(!priv->surface) { // cannot SetVideoMode
		printf("SDL: failed to set video mode: %s\n", SDL_GetError());
		return -1;
	}	

	switch(format) {
	    	/* Initialize and create the RGB Surface used for video out in BGR/RGB mode */
//SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);	
		//	SDL_SWSURFACE,SDL_HWSURFACE,SDL_SRCCOLORKEY, priv->flags?	guess: exchange Rmask and Bmask for BGR<->RGB
		// 32 bit: a:ff000000 r:ff000 g:ff00 b:ff
		// 24 bit: r:ff0000 g:ff00 b:ff
		// 16 bit: r:1111100000000000b g:0000011111100000b b:0000000000011111b
		// 15 bit: r:111110000000000b g:000001111100000b b:000000000011111b
		// FIXME: colorkey detect based on bpp, FIXME static bpp value, FIXME alpha value correct?
	    case IMGFMT_RGB15:
		if (!(priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, width, height, 15, 31, 992, 31744, 0))) {
			printf ("SDL: Couldn't create a RGB surface: %s\n", SDL_GetError());
			return -1;
		}
	    break;	
	    case IMGFMT_BGR15:
		if (!(priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, width, height, 15, 31744, 992, 31, 0))) {
			printf ("SDL: Couldn't create a RGB surface: %s\n", SDL_GetError());
			return -1;
		}
	    break;	
	    case IMGFMT_RGB16:
		if (!(priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, width, height, 16, 31, 2016, 63488, 0))) {
			printf ("SDL: Couldn't create a RGB surface: %s\n", SDL_GetError());
			return -1;
		}
	    break;	
	    case IMGFMT_BGR16:
		if (!(priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, width, height, 16, 63488, 2016, 31, 0))) {
			printf ("SDL: Couldn't create a RGB surface: %s\n", SDL_GetError());
			return -1;
		}
	    break;	
	    case IMGFMT_RGB24:
		if (!(priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, width, height, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0))) {
			printf ("SDL: Couldn't create a RGB surface: %s\n", SDL_GetError());
			return -1;
		}
	    break;	
	    case IMGFMT_BGR24:
		if (!(priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, width, height, 24, 0xFF0000, 0x00FF00, 0x0000FF, 0))) {
			printf ("SDL: Couldn't create a RGB surface: %s\n", SDL_GetError());
			return -1;
		}
	    break;	
	    case IMGFMT_RGB32:
		if (!(priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, width, height, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0/*0xFF000000*/))) {
			printf ("SDL: Couldn't create a RGB surface: %s\n", SDL_GetError());
			return -1;
		}
	    break;	
	    case IMGFMT_BGR32:
		if (!(priv->rgbsurface = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0/*0xFF000000*/))) {
			printf ("SDL: Couldn't create a RGB surface: %s\n", SDL_GetError());
			return -1;
		}
	    break;	
	    default:
		/* Initialize and create the YUV Overlay used for video out */
		if (!(priv->overlay = SDL_CreateYUVOverlay (width, height, sdl_format, priv->surface))) {
			printf ("SDL: Couldn't create a YUV overlay: %s\n", SDL_GetError());
			return -1;
		}
		priv->framePlaneY = width * height;
		priv->framePlaneUV = (width * height) >> 2;
		priv->framePlaneYUY = width * height * 2;
		priv->stridePlaneY = width;
		priv->stridePlaneUV = width/2;
		priv->stridePlaneYUY = width * 2;
	}
	
	if(priv->mode) {
		if((priv->format&0xFF) != priv->bpp) { priv->dblit = 0; printf("SDL: using depth/colorspace conversion, this will slow things down (%ibpp -> %ibpp).\n", priv->format&0xFF, priv->bpp); }
		else if(strcmp(priv->driver, "x11") == 0) priv->dblit = 1;
		priv->framePlaneRGB = width * height * priv->rgbsurface->format->BytesPerPixel;
		priv->stridePlaneRGB = width * priv->rgbsurface->format->BytesPerPixel;
	}	
	return 0;
}

#ifdef SDL_ENABLE_LOCKS
#define	SDL_OVR_LOCK        if (SDL_LockYUVOverlay (priv->overlay)) { \
				if(verbose) printf("SDL: Couldn't lock YUV overlay\n"); \
				return -1; \
	    		    }
#define SDL_OVR_UNLOCK      SDL_UnlockYUVOverlay (priv->overlay);

#define SDL_SRF_LOCK(srf)   if(SDL_MUSTLOCK(srf)) { \
				if(SDL_LockSurface (srf)) { \
					if(verbose) printf("SDL: Couldn't lock RGB surface\n"); \
					return -1; \
				} \
			    }

#define SDL_SRF_UNLOCK(srf) if(SDL_MUSTLOCK(srf)) \
				SDL_UnlockSurface (srf);
#else
#define SDL_OVR_LOCK
#define SDL_OVR_UNLOCK
#define SDL_SRF_LOCK(srf)
#define SDL_SRF_UNLOCK(srf)
#endif

/**
 * Draw a frame to the SDL YUV overlay.
 *
 *   params : *src[] == the Y, U, and V planes that make up the frame.
 *  returns : non-zero on success, zero on error.
 **/

//static int sdl_draw_frame (frame_t *frame)
static uint32_t draw_frame(uint8_t *src[])
{
	struct sdl_priv_s *priv = &sdl_priv;
	uint8_t *dst;
	int i;
	uint8_t *mysrc = src[0];

        switch(priv->format){
        case IMGFMT_YV12:
        case IMGFMT_I420:
        case IMGFMT_IYUV:
	    SDL_OVR_LOCK
	    dst = (uint8_t *) *(priv->overlay->pixels);
	    memcpy (dst, src[0], priv->framePlaneY);
	    dst += priv->framePlaneY;
	    memcpy (dst, src[2], priv->framePlaneUV);
	    dst += priv->framePlaneUV;
	    memcpy (dst, src[1], priv->framePlaneUV);
	    SDL_OVR_UNLOCK
            break;

        case IMGFMT_YUY2:
        case IMGFMT_UYVY:
        case IMGFMT_YVYU:
	    SDL_OVR_LOCK
	    dst = (uint8_t *) *(priv->overlay->pixels);
	    if(priv->flip) {
	    	mysrc+=priv->framePlaneYUY;
		for(i = 0; i < priv->height; i++) {
			mysrc-=priv->stridePlaneYUY;
			memcpy (dst, mysrc, priv->stridePlaneYUY);
			dst+=priv->stridePlaneYUY;
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
			SDL_SRF_LOCK(priv->surface)
			dst = (uint8_t *) priv->surface->pixels;
			if(priv->flip) {
				mysrc+=priv->framePlaneRGB;
				for(i = 0; i < priv->height; i++) {
					mysrc-=priv->stridePlaneRGB;
					memcpy (dst, mysrc, priv->stridePlaneRGB);
					dst+=priv->stridePlaneRGB;
				}
			}
			else memcpy (dst, src[0], priv->framePlaneRGB);
			SDL_SRF_UNLOCK(priv->surface)
		} else {
			SDL_SRF_LOCK(priv->rgbsurface)
			dst = (uint8_t *) priv->rgbsurface->pixels;
			if(priv->flip) {
				mysrc+=priv->framePlaneRGB;
				for(i = 0; i < priv->height; i++) {
					mysrc-=priv->stridePlaneRGB;
					memcpy (dst, mysrc, priv->stridePlaneRGB);
					dst+=priv->stridePlaneRGB;
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
static uint32_t draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	struct sdl_priv_s *priv = &sdl_priv;
	uint8_t *dst;
	uint8_t *src;
        int i;

	SDL_OVR_LOCK

	dst = (uint8_t *) *(priv->overlay->pixels) 
            + (priv->stridePlaneY * y + x);
        src = image[0];
        for(i=0;i<h;i++){
            memcpy(dst,src,w);
            src+=stride[0];
            dst+=priv->stridePlaneY;
        }
        
        x/=2;y/=2;w/=2;h/=2;

	dst = (uint8_t *) *(priv->overlay->pixels) + priv->framePlaneY
            + (priv->stridePlaneUV * y + x);
        src = image[2];
        for(i=0;i<h;i++){
            memcpy(dst,src,w);
            src+=stride[2];
            dst+=priv->stridePlaneUV;
        }
        
	dst = (uint8_t *) *(priv->overlay->pixels) + priv->framePlaneY
            + priv->framePlaneUV + (priv->stridePlaneUV * y + x);
        src = image[1];
        for(i=0;i<h;i++){
            memcpy(dst,src,w);
            src+=stride[1];
            dst+=priv->stridePlaneUV;
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

#include "../linux/keycodes.h"
extern void mplayer_put_key(int code);
 
static void check_events (void)
{
	struct sdl_priv_s *priv = &sdl_priv;
	SDL_Event event;
	SDLKey keypressed = 0;
	static int firstcheck = 0;
	
	/* Poll the waiting SDL Events */
	while ( SDL_PollEvent(&event) ) {
		switch (event.type) {

			/* capture window resize events */
			case SDL_VIDEORESIZE:
				if(!priv->dblit) priv->surface = SDL_SetVideoMode(event.resize.w, event.resize.h, priv->bpp, priv->sdlflags);

				/* save video extents, to restore them after going fullscreen */
			 	//if(!(priv->surface->flags & SDL_FULLSCREEN)) {
				    priv->windowsize.w = priv->surface->w;
				    priv->windowsize.h = priv->surface->h;
				//}
				if(verbose > 2) printf("SDL: Window resize\n");
			break;
			
			
			/* graphics mode selection shortcuts */
#ifdef BUGGY_SDL
			case SDL_KEYDOWN:
				switch(event.key.keysym.sym) {
                                case SDLK_UP: mplayer_put_key(KEY_UP); break;
                                case SDLK_DOWN: mplayer_put_key(KEY_DOWN); break;
                                case SDLK_LEFT: mplayer_put_key(KEY_LEFT); break;
                                case SDLK_RIGHT: mplayer_put_key(KEY_RIGHT); break;
                                case SDLK_ASTERISK:
				case SDLK_KP_MULTIPLY:
				case SDLK_w: mplayer_put_key('*'); break;
				case SDLK_SLASH:
				case SDLK_KP_DIVIDE:
                                case SDLK_s: mplayer_put_key('/'); break;
				default: break;
				}
			break;
			case SDL_KEYUP:
#else			
			case SDL_KEYDOWN:
#endif			
				keypressed = event.key.keysym.sym;
				if(verbose > 1) printf("SDL: Key pressed: '%i'\n", keypressed);

				/* c key pressed. c cycles through available fullscreenmodes, if we have some */
				if ( ((keypressed == SDLK_c)) && (priv->fullmodes) ) {
					/* select next fullscreen mode */
					priv->fullmode++;
					if (priv->fullmode > (findArrayEnd(priv->fullmodes) - 1)) priv->fullmode = 0;
					set_fullmode(priv->fullmode);
	
					if(verbose > 1) printf("SDL: Set next available fullscreen mode.\n");
				}

				/* f key pressed toggles/exits fullscreenmode */
				else if ( keypressed == SDLK_f ) {
					if (priv->surface->flags & SDL_FULLSCREEN) {
						priv->surface = SDL_SetVideoMode(priv->windowsize.w, priv->windowsize.h, priv->bpp, priv->sdlflags);
						SDL_ShowCursor(1);
						if(verbose > 1) printf("SDL: Windowed mode\n");
					} 
					else if (priv->fullmodes){
						set_fullmode(priv->fullmode);

						if(verbose > 1) printf("SDL: Set fullscreen mode\n");
					}
				}

				else if ( keypressed == SDLK_n ) {
					int newwidth = priv->dstwidth, newheight = priv->dstheight;
					aspect(&newwidth, &newheight, priv->dstwidth, (int)((float)priv->dstwidth*((float)priv->XHeight / (float)priv->XWidth)));
					if (priv->surface->w != newwidth || priv->surface->h != newheight) {
						priv->surface = SDL_SetVideoMode(newwidth, newheight, priv->bpp, priv->sdlflags);
					    	priv->windowsize.w = priv->surface->w;
						priv->windowsize.h = priv->surface->h;
						if(verbose > 1) printf("SDL: Normal size\n");
					} else
					if (priv->surface->w != newwidth * 2 || priv->surface->h != newheight * 2) {
						priv->surface = SDL_SetVideoMode(newwidth * 2, newheight * 2, priv->bpp, priv->sdlflags);
					    	priv->windowsize.w = priv->surface->w;
						priv->windowsize.h = priv->surface->h;
						if(verbose > 1) printf("SDL: Double size\n");
					}
				}	

                                else switch(keypressed){
				case SDLK_RETURN:
					if (!firstcheck) { firstcheck = 1; break; }
                                case SDLK_ESCAPE:
				case SDLK_q:
					SDL_ShowCursor(1);
					mplayer_put_key('q');
				break;
                                /*case SDLK_o: mplayer_put_key('o');break;
                                case SDLK_SPACE: mplayer_put_key(' ');break;
                                case SDLK_p: mplayer_put_key('p');break;*/
                                case SDLK_PLUS:
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
				case SDLK_w:
				case SDLK_SLASH:
				case SDLK_KP_DIVIDE:
                                case SDLK_s:
				break;
#else				
                                case SDLK_UP: mplayer_put_key(KEY_UP);break;
                                case SDLK_DOWN: mplayer_put_key(KEY_DOWN);break;
                                case SDLK_LEFT: mplayer_put_key(KEY_LEFT);break;
                                case SDLK_RIGHT: mplayer_put_key(KEY_RIGHT);break;
                                case SDLK_ASTERISK:
				case SDLK_KP_MULTIPLY:
				case SDLK_w: mplayer_put_key('*');break;
				case SDLK_SLASH:
				case SDLK_KP_DIVIDE:
                                case SDLK_s: mplayer_put_key('/');break;
#endif				
				default:
					mplayer_put_key(keypressed);
                                }
                                
				break;
				case SDL_QUIT:
					SDL_ShowCursor(1);
					mplayer_put_key('q');
				break;
		}
	}
}

static void draw_osd(void)
{	struct sdl_priv_s *priv = &sdl_priv;

	/* update osd/subtitles */
	vo_draw_text(priv->width,priv->height,draw_alpha);
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
				printf("SDL: Blit failed: %s\n", SDL_GetError());
		}

		/* update screen */
		//SDL_UpdateRect(priv->surface, 0, 0, priv->surface->clip_rect.w, priv->surface->clip_rect.h);
		SDL_UpdateRects(priv->surface, 1, &priv->surface->clip_rect);
		
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

static uint32_t
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
    case IMGFMT_YVYU:
    	return 0x6; // hw supported & osd
    case IMGFMT_RGB15:
    case IMGFMT_BGR15:
    case IMGFMT_RGB16:
    case IMGFMT_BGR16:
    case IMGFMT_RGB24:
    case IMGFMT_BGR24:
    case IMGFMT_RGB32:
    case IMGFMT_BGR32:
        return 0x5; // hw supported w/conversion & osd
    }
    return 0;
}

static const vo_info_t*
get_info(void)
{
	return &vo_info;
}


static void
uninit(void)
{
#ifdef HAVE_X11
	static Display *XDisplay;
	if(getenv("DISPLAY")) {
		if(verbose) printf("SDL: activating XScreensaver/DPMS\n");
		XDisplay = XOpenDisplay(getenv("DISPLAY"));
		saver_on(XDisplay);
		XCloseDisplay(XDisplay);
	}
#endif
	sdl_close();
}
