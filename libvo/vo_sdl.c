/*
 *  vo_sdl.c
 *
 *  (was video_out_sdl.c from OMS project/mpeg2dec -> http://linuxvideo.org)
 *
 *  Copyright (C) Ryan C. Gordon <icculus@lokigames.com> - April 22, 2000.
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
 *    KNOWN BUGS:
 *    - Crashes with aalib (not resolved yet)
 */

/* define if you want to force Xv SDL output? */
#undef SDL_FORCEXV
/* define to force software-surface (video surface stored in system memory)*/
#undef SDL_NOHWSURFACE
/* define to disable usage of the xvideo extension */
#undef SDL_NOXV

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "fastmemcpy.h"

LIBVO_EXTERN(sdl)

//#include "log.h"
//#define LOG if(0)printf

extern int verbose;

static vo_info_t vo_info = 
{
	"SDL YUV overlay (SDL v1.1.7+ only!)",
	"sdl",
	"Ryan C. Gordon <icculus@lokigames.com>",
	""
};

#include <SDL/SDL.h>

/** Private SDL Data structure **/

static struct sdl_priv_s {

	/* SDL YUV surface & overlay */
	SDL_Surface *surface;
	SDL_Overlay *overlay;
//	SDL_Overlay *current_frame;

	/* available fullscreen modes */
	SDL_Rect **fullmodes;

	/* surface attributes for fullscreen and windowed mode */
	Uint32 sdlflags, sdlfullflags;

	/* save the windowed output extents */
	SDL_Rect windowsize;
	
	/* Bits per Pixel */
	Uint8 bpp;

	/* current fullscreen mode, 0 = highest available fullscreen mode */
	int fullmode;

	/* YUV ints */
	int framePlaneY, framePlaneUV;
	int stridePlaneY, stridePlaneUV;
        int width,height;
        int format;
} sdl_priv;


/** libvo Plugin functions **/

/**
 * draw_alpha is used for osd and subtitle display.
 *
 **/

//void vo_draw_alpha_yv12(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride);
//void vo_draw_alpha_yuy2(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride);

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
	struct sdl_priv_s *priv = &sdl_priv;
	int x,y;
	
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

//	LOG (LOG_DEBUG, "SDL video out: Opened Plugin");

	/* does the user want SDL to try and force Xv */
	#ifdef SDL_FORCEXV
		setenv("SDL_VIDEO_X11_NODIRECTCOLOR", "1", 1);
	#endif
	#ifdef SDL_NOXV
		setenv("SDL_VIDEO_YUV_HWACCEL", "0", 1);
	#endif	
	
	/* default to no fullscreen mode, we'll set this as soon we have the avail. modes */
	priv->fullmode = -2;
	/* other default values */
	#ifdef SDL_NOHWSURFACE
		if(verbose) printf("SDL: using software-surface\n");
		priv->sdlflags = SDL_SWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT;
		priv->sdlfullflags = SDL_SWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_ASYNCBLIT;
	#else	
		if(verbose) printf("SDL: using hardware-surface\n");
		priv->sdlflags = SDL_HWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT; //SDL_HWACCEL
		priv->sdlfullflags = SDL_HWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_ASYNCBLIT; //SDL_HWACCEL
	#endif	
	priv->surface = NULL;
	priv->overlay = NULL;
	priv->fullmodes = NULL;
	priv->bpp = 0; //added atmos

	/* initialize the SDL Video system */
	if (SDL_Init (SDL_INIT_VIDEO)) {
//		LOG (LOG_ERROR, "SDL video out: Initializing of SDL failed (SDL_Init). Please use the latest version of SDL.");
		return -1;
	}
	
	/* No Keyrepeats! */
	SDL_EnableKeyRepeat(0,0);

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
//			LOG (LOG_ERROR, "SDL video out: Couldn't get any acceptable SDL Mode for output. (SDL_ListModes failed)");
			return -1;
		}
	}
	
		
   /* YUV overlays need at least 16-bit color depth, but the
    * display might less. The SDL AAlib target says it can only do
    * 8-bits, for example. So, if the display is less than 16-bits,
    * we'll force the BPP to 16, and pray that SDL can emulate for us.
	 */
	priv->bpp = vidInfo->vfmt->BitsPerPixel;
	if (priv->bpp < 16) {
/*
		LOG (LOG_WARNING, "SDL video out: Your SDL display target wants to be at a color depth of (%d), but we need it to be at\
least 16 bits, so we need to emulate 16-bit color. This is going to slow things down; you might want to\
increase your display's color depth, if possible", priv->bpp);
*/
		priv->bpp = 16;  
	}
	
	/* We dont want those in out event queue */
	SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
	SDL_EventState(SDL_KEYDOWN, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_IGNORE);
	SDL_EventState(SDL_QUIT, SDL_IGNORE);
	SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
	SDL_EventState(SDL_USEREVENT, SDL_IGNORE);
	
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

	/* Free our blitting surface */
	if (priv->surface)
		SDL_FreeSurface(priv->surface);
	
	/* DONT attempt to free the fullscreen modes array. SDL_Quit* does this for us */
	
	/* Cleanup SDL */
	SDL_Quit(); /* might have to be changed to quitsubsystem only, if plugins become
		       changeable on the fly */

//	LOG (LOG_DEBUG, "SDL video out: Closed Plugin");

	return 0;
}


/**
 * Sets the specified fullscreen mode.
 *
 *   params : mode == index of the desired fullscreen mode
 *  returns : doesn't return
 **/
 
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
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
//static int sdl_setup (int width, int height)
{
	struct sdl_priv_s *priv = &sdl_priv;
        unsigned int sdl_format;
	
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
		default:
			printf("SDL: Unsupported image format (0x%X)\n",format);
			return -1;
	}

	sdl_open (NULL, NULL);

	/* Set output window title */
	SDL_WM_SetCaption (".: MPlayer : F = Fullscreen/Windowed : C = Cycle Fullscreen Resolutions :.", "SDL Video Out");

	/* Save the original Image size */
	
	priv->width  = width;
	priv->height = height;
        priv->format = format;
        
	/* bit 0 (0x01) means fullscreen (-fs)
	 * bit 1 (0x02) means mode switching (-vm)
	 * bit 2 (0x04) enables software scaling (-zoom)
	 */  
//      printf("SDL: fullscreenflag is set to: %i\n", fullscreen);
//	printf("SDL: Width: %i Height: %i D_Width %i D_Height: %i\n", width, height, d_width, d_height);
	switch(fullscreen){
	  case 0x01:
	  case 0x05:
	  	if(verbose) printf("SDL: setting zoomed fullscreen without modeswitching\n");
		priv->windowsize.w = d_width;
	  	priv->windowsize.h = d_height;
          	if(priv->surface = SDL_SetVideoMode (d_width, d_height, priv->bpp, priv->sdlfullflags))
			SDL_ShowCursor(0);
	  break;	
	  case 0x02:
	  case 0x03:
		priv->windowsize.w = width;
	  	priv->windowsize.h = height;
#ifdef SDL_NOXV	  
	 	if(verbose) printf("SDL: setting nonzoomed fullscreen with modeswitching\n");
          	if(priv->surface = SDL_SetVideoMode (width, height, priv->bpp, priv->sdlfullflags))
			SDL_ShowCursor(0);
#else
	 	if(verbose) printf("SDL: setting zoomed fullscreen with modeswitching\n");
          	priv->surface=NULL;
          	set_fullmode(priv->fullmode);
#endif		
	  break;		
	  case 0x06:
	  case 0x07:
	 	if(verbose) printf("SDL: setting zoomed fullscreen with modeswitching\n");
	  	priv->windowsize.w = width;
	  	priv->windowsize.h = height;
          	priv->surface=NULL;
          	set_fullmode(priv->fullmode);
	  break;  
          default:
	 	if(verbose) printf("SDL: setting windowed mode\n");
	  	priv->windowsize.w = d_width;
	  	priv->windowsize.h = d_height;
          	priv->surface = SDL_SetVideoMode (d_width, d_height, priv->bpp, priv->sdlflags);
        }
        if(!priv->surface) return -1; // cannot SetVideoMode

	/* Initialize and create the YUV Overlay used for video out */
	if (!(priv->overlay = SDL_CreateYUVOverlay (width, height, sdl_format, priv->surface))) {
		printf ("SDL video out: Couldn't create an SDL-based YUV overlay\n");
		return -1;
	}
	priv->framePlaneY = width * height;
	priv->framePlaneUV = (width * height) >> 2;
	priv->stridePlaneY = width;
	priv->stridePlaneUV = width/2;

	return 0;
}


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

//	priv->current_frame = (SDL_Overlay*) frame->private;
//	SDL_UnlockYUVOverlay (priv->current_frame);

	if (SDL_LockYUVOverlay (priv->overlay)) {
//		LOG (LOG_ERROR, "SDL video out: Couldn't lock SDL-based YUV overlay");
		return -1;
	}

        switch(priv->format){
        case IMGFMT_YV12:
        case IMGFMT_I420:
        case IMGFMT_IYUV:
	    dst = (uint8_t *) *(priv->overlay->pixels);
	    memcpy (dst, src[0], priv->framePlaneY);
	    dst += priv->framePlaneY;
	    memcpy (dst, src[2], priv->framePlaneUV);
	    dst += priv->framePlaneUV;
	    memcpy (dst, src[1], priv->framePlaneUV);
            break;

        case IMGFMT_YUY2:
        case IMGFMT_UYVY:
        case IMGFMT_YVYU:
	    dst = (uint8_t *) *(priv->overlay->pixels);
	    memcpy (dst, src[0], priv->width*priv->height*2);
            break;
        }
        	
	SDL_UnlockYUVOverlay (priv->overlay);
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

	//priv->current_frame = priv->overlay;
	
	if (SDL_LockYUVOverlay (priv->overlay)) {
//		LOG (LOG_ERROR, "SDL video out: Couldn't lock SDL-based YUV overlay");
		return -1;
	}

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

#if 0
	dst = (uint8_t *) *(priv->overlay->pixels) + (priv->slicePlaneY * slice_num);
	memcpy (dst, src[0], priv->slicePlaneY);
	dst = (uint8_t *) *(priv->overlay->pixels) + priv->framePlaneY + (priv->slicePlaneUV * slice_num);
	memcpy (dst, src[2], priv->slicePlaneUV);
	dst += priv->framePlaneUV;
	memcpy (dst, src[1], priv->slicePlaneUV);
#endif
	
	SDL_UnlockYUVOverlay (priv->overlay);

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
				priv->surface = SDL_SetVideoMode(event.resize.w, event.resize.h, priv->bpp, priv->sdlflags);

				/* save video extents, to restore them after going fullscreen */
			 	//if(!(priv->surface->flags & SDL_FULLSCREEN)) {
				    priv->windowsize.w = priv->surface->w;
				    priv->windowsize.h = priv->surface->h;
				//}
//				LOG (LOG_DEBUG, "SDL video out: Window resize");
			break;
			
			
			/* graphics mode selection shortcuts */
			case SDL_KEYUP:
				keypressed = event.key.keysym.sym;

				/* c key pressed. c cycles through available fullscreenmodes, if we have some */
				if ( ((keypressed == SDLK_c)) && (priv->fullmodes) ) {
					/* select next fullscreen mode */
					priv->fullmode++;
					if (priv->fullmode > (findArrayEnd(priv->fullmodes) - 1)) priv->fullmode = 0;
					set_fullmode(priv->fullmode);
	
//					LOG (LOG_DEBUG, "SDL video out: Set next available fullscreen mode.");
				}

				/* f key pressed toggles/exits fullscreenmode */
				else if ( keypressed == SDLK_f ) {
					if (priv->surface->flags & SDL_FULLSCREEN) {
						priv->surface = SDL_SetVideoMode(priv->windowsize.w, priv->windowsize.h, priv->bpp, priv->sdlflags);
						SDL_ShowCursor(1);
//						LOG (LOG_DEBUG, "SDL video out: Windowed mode");
					} 
					else if (priv->fullmodes){
						set_fullmode(priv->fullmode);

//						LOG (LOG_DEBUG, "SDL video out: Set fullscreen mode.");
					}
				}
                                
                                else switch(keypressed){
//                                case SDLK_q: if(!(priv->surface->flags & SDL_FULLSCREEN))mplayer_put_key('q');break;
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
                                case SDLK_UP: mplayer_put_key(KEY_UP);break;
                                case SDLK_DOWN: mplayer_put_key(KEY_DOWN);break;
                                case SDLK_LEFT: mplayer_put_key(KEY_LEFT);break;
                                case SDLK_RIGHT: mplayer_put_key(KEY_RIGHT);break;
                                case SDLK_PLUS:
                                case SDLK_KP_PLUS: mplayer_put_key('+');break;
                                case SDLK_MINUS:
                                case SDLK_KP_MINUS: mplayer_put_key('-');break;
                                case SDLK_ASTERISK:
				case SDLK_KP_MULTIPLY:
				case SDLK_w: mplayer_put_key('*');break;
				case SDLK_SLASH:
				case SDLK_KP_DIVIDE:
                                case SDLK_s: mplayer_put_key('/');break;
				default:
					mplayer_put_key(keypressed);
                                }
                                
				break;
		}
	}
}


/**
 * Display the surface we have written our data to and check for events.
 *
 *   params : mode == index of the desired fullscreen mode
 *  returns : doesn't return
 **/

static void flip_page (void)
{
	struct sdl_priv_s *priv = &sdl_priv;

	//vo_draw_alpha_yuy2(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride)
	vo_draw_text(priv->width,priv->height,draw_alpha);	
		
	/* check and react on keypresses and window resizes */
	check_events();

	/* blit to the YUV overlay */
	SDL_DisplayYUVOverlay (priv->overlay, &priv->surface->clip_rect);

	/* check if we have a double buffered surface and flip() if we do. */
	if ( priv->surface->flags & SDL_DOUBLEBUF )
        	SDL_Flip(priv->surface);
	
	SDL_LockYUVOverlay (priv->overlay);
}

#if 0
static frame_t* sdl_allocate_image_buffer(int width, int height)
{
	struct sdl_priv_s *priv = &sdl_priv;
	frame_t	*frame;

	if (!(frame = malloc (sizeof (frame_t))))
		return NULL;

	if (!(frame->private = (void*) SDL_CreateYUVOverlay (width, height, 
			SDL_IYUV_OVERLAY, priv->surface)))
	{
//		LOG (LOG_ERROR, "SDL video out: Couldn't create an SDL-based YUV overlay");
		return NULL;
	}
	
	frame->base[0] = (uint8_t*) ((SDL_Overlay*) (frame->private))->pixels[0];
	frame->base[1] = (uint8_t*) ((SDL_Overlay*) (frame->private))->pixels[1];
	frame->base[2] = (uint8_t*) ((SDL_Overlay*) (frame->private))->pixels[2];
	
	SDL_LockYUVOverlay ((SDL_Overlay*) frame->private);
	return frame;
}

static void sdl_free_image_buffer(frame_t* frame)
{
	SDL_FreeYUVOverlay((SDL_Overlay*) frame->private);
	free(frame);
}
#endif

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
//    case IMGFMT_RGB|24:
//    case IMGFMT_BGR|24:
        return 1;
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
sdl_close();
}

