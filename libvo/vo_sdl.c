/*
 *  video_out_sdl.c
 *
 *  Copyright (C) Ryan C. Gordon <icculus@lokigames.com> - April 22, 2000.
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
 *  the Free Software Foundation, 
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
 *    Felix Buenemann <Felix.Buenemann@ePost.de> - March 11, 2001
 *    - Added aspect-ratio awareness for fullscreen
 *    Felix Buenemann <Felix.Buenemann@gmx.de> - March 11, 2001
 *    - Fixed aspect-ratio awareness, did only vertical scaling (black bars above
 *       and below), now also does horizontal scaling (black bars left and right),
 *       so you get the biggest possible picture with correct aspect-ratio.
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - March 12, 2001
 *    - Minor bugfix to aspect-ratio vor non-4:3-resolutions (like 1280x1024)
 *    - Bugfix to check_events() to reveal mouse cursor after 'q'-quit in
 *       fullscreen-mode
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - March 12, 2001
 *    - Changed keypress-detection from keydown to keyup, seems to fix keyrepeat
 *       bug (key had to be pressed twice to be detected)
 *    - Changed key-handling: 'f' cycles fullscreen/windowed, ESC/RETURN/'q' quits
 *    - Bugfix which avoids exit, because return is passed to sdl-output on startup,
 *       which caused the player to exit (keyboard-buffer problem? better solution
 *       recommed)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"


#include "mmx.h"
LIBVO_EXTERN(sdl)

//#include "log.h"
//#define LOG if(0)printf

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


/** OMS Plugin functions **/


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
	
	/* default to no fullscreen mode, we'll set this as soon we have the avail. modes */
	priv->fullmode = -2;
	/* other default values */
	priv->sdlflags = SDL_HWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT;
	priv->sdlfullflags = SDL_HWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_ASYNCBLIT;
	priv->surface = NULL;
	priv->overlay = NULL;
	priv->fullmodes = NULL;

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
	//SDL_EventState(SDL_KEYUP, SDL_IGNORE);
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
	
//	LOG (LOG_DEBUG, "SDL video out: Closed Plugin");
//	LOG (LOG_INFO, "SDL video out: Closed Plugin");

	/* Cleanup YUV Overlay structure */
	if (priv->overlay) 
		SDL_FreeYUVOverlay(priv->overlay);

	/* Free our blitting surface */
	if (priv->surface)
		SDL_FreeSurface(priv->surface);
	
	/* TODO: cleanup the full_modes array */
	
	/* Cleanup SDL */
	SDL_Quit();

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
        unsigned int sdl_format, aspectheight;

        switch(format){
          case IMGFMT_YV12: sdl_format=SDL_YV12_OVERLAY;break;
          case IMGFMT_YUY2: sdl_format=SDL_YUY2_OVERLAY;break;
          default:
            printf("SDL: Unsupported image format (0x%X)\n",format);
            return -1;
        }

	sdl_open (NULL, NULL);

	/* Save the original Image size */
	
	priv->width  = width;
	priv->height = height;
        priv->format = format;
        
        if(fullscreen){
	    priv->windowsize.w = width;
	    priv->windowsize.h = height;
            priv->surface=NULL;
            set_fullmode(priv->fullmode);
        } else {
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
	    dst = (uint8_t *) *(priv->overlay->pixels);
	    memcpy (dst, src[0], priv->framePlaneY);
	    dst += priv->framePlaneY;
	    memcpy (dst, src[2], priv->framePlaneUV);
	    dst += priv->framePlaneUV;
	    memcpy (dst, src[1], priv->framePlaneUV);
            break;
        case IMGFMT_YUY2:
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

				/* plus key pressed. plus cycles through available fullscreenmodes, if we have some */
				if ( ((keypressed == SDLK_PLUS) || (keypressed == SDLK_KP_PLUS)) && (priv->fullmodes) ) {
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
                                case SDLK_p: mplayer_put_key('p');break;
                                case SDLK_SPACE: mplayer_put_key(' ');break;
                                case SDLK_UP: mplayer_put_key(KEY_UP);break;
                                case SDLK_DOWN: mplayer_put_key(KEY_DOWN);break;
                                case SDLK_LEFT: mplayer_put_key(KEY_LEFT);break;
                                case SDLK_RIGHT: mplayer_put_key(KEY_RIGHT);break;
                                case SDLK_PLUS:
                                case SDLK_KP_PLUS: mplayer_put_key('+');break;
                                case SDLK_MINUS:
                                case SDLK_KP_MINUS: mplayer_put_key('-');break;
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
    case IMGFMT_YUY2:
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

