/*
 *  video_out.h
 *
 *      Copyright (C) Aaron Holtzman - Aug 1999
 *	Strongly modified, most parts rewritten: A'rpi/ESP-team - 2000-2001
 *
 */
 
#ifndef __VIDEO_OUT_H
#define __VIDEO_OUT_H 1

#include <inttypes.h>
#include <stdarg.h>

//#include "font_load.h"
#include "img_format.h"
#include "../vidix/vidix.h"

#define VO_EVENT_EXPOSE 1
#define VO_EVENT_RESIZE 2
#define VO_EVENT_KEYPRESS 4

/* takes a pointer to a vo_vaa_s struct */
#define VOCTRL_QUERY_VAA 1
/* takes a pointer to uint32_t fourcc */
#define VOCTRL_QUERY_FORMAT 2
/* signal a device reset seek */
#define VOCTRL_RESET 3
/* true if vo driver can use GUI created windows */
#define VOCTRL_GUISUPPORT 4
/* used to switch to fullscreen */
#define VOCTRL_FULLSCREEN 5
/* user wants to have screen shot. (currently without args)*/
#define VOCTRL_SCREENSHOT 6
/* signal a device pause */
#define VOCTRL_PAUSE 7
/* start/resume playback */
#define VOCTRL_RESUME 8
/* libmpcodecs direct rendering: */
#define VOCTRL_GET_IMAGE 9
#define VOCTRL_DRAW_IMAGE 13
#define VOCTRL_SET_SPU_PALETTE 14
/* decoding ahead: */
#define VOCTRL_GET_NUM_FRAMES 10
#define VOCTRL_GET_FRAME_NUM  11
#define VOCTRL_SET_FRAME_NUM  12
#define VOCTRL_GET_PANSCAN 15
#define VOCTRL_SET_PANSCAN 16

#define VO_TRUE		1
#define VO_FALSE	0
#define VO_ERROR	-1
#define VO_NOTAVAIL	-2
#define VO_NOTIMPL	-3

#define VOFLAG_FULLSCREEN	0x01
#define VOFLAG_MODESWITCHING	0x02
#define VOFLAG_SWSCALE		0x04
#define VOFLAG_FLIPPING		0x08

typedef struct vo_info_s
{
        /* driver name ("Matrox Millennium G200/G400" */
        const char *name;
        /* short name (for config strings) ("mga") */
        const char *short_name;
        /* author ("Aaron Holtzman <aholtzma@ess.engr.uvic.ca>") */
        const char *author;
        /* any additional comments */
        const char *comment;
} vo_info_t;

/* Direct access to BES */
typedef struct bes_da_s
{
	vidix_rect_t	dest;           /* This field should be filled by x,y,w,h
					   from vidix:src but pitches from
					   vidix:dest */
	int		flags;          /* Probably will work only when flag == 0 */
	/* memory model */
	unsigned	frame_size;		/* destination frame size */
	unsigned	num_frames;		/* number of available frames */
	unsigned	offsets[VID_PLAY_MAXFRAMES]; /* relative offset of each frame from begin of video memory */
	vidix_yuv_t	offset;			/* relative offsets within frame for yuv planes */
	void*		dga_addr;		/* linear address of BES */
}bes_da_t;

/*
   Video Accelearted Architecture.
   Every field of this structure can be set to NULL that means that
   features is not supported
*/
typedef struct vo_vaa_s
{
	uint32_t    flags; /* currently undefined */
		/*
		 * Query Direct Access to BES
		 * info - information to be filled
		 * returns: 0 on success errno on error.
		 */
	int  (*query_bes_da)(bes_da_t *info);
	int  (*get_video_eq)(vidix_video_eq_t *info);
	int  (*set_video_eq)(const vidix_video_eq_t *info);
	int  (*get_num_fx)(unsigned *info);
	int  (*get_oem_fx)(vidix_oem_fx_t *info);
	int  (*set_oem_fx)(const vidix_oem_fx_t *info);
	int  (*set_deint)(const vidix_deinterlace_t *info);
}vo_vaa_t;

/* Misc info to tuneup vo driver */
typedef struct vo_tune_info_s
{
	int	pitch[3]; /* Should be 0 if unknown else power of 2 */
}vo_tune_info_t;

typedef struct vo_functions_s
{
	/*
	 * Preinitializes driver (real INITIALIZATION)
	 *   arg - currently it's vo_subdevice
	 *   returns: zero on successful initialization, non-zero on error.
	 */
	uint32_t (*preinit)(const char *arg);
        /*
         * Initialize (means CONFIGURE) the display driver.
	 * params:
         *   width,height: image source size
	 *   d_width,d_height: size of the requested window size, just a hint
	 *   fullscreen: flag, 0=windowd 1=fullscreen, just a hint
	 *   title: window title, if available
	 *   format: fourcc of pixel format
         * returns : zero on successful initialization, non-zero on error.
         */
        uint32_t (*config)(uint32_t width, uint32_t height, uint32_t d_width,
			 uint32_t d_height, uint32_t fullscreen, char *title,
			 uint32_t format,const vo_tune_info_t *);

	/*
	 * Control interface
	 */
	uint32_t (*control)(uint32_t request, void *data, ...);

        /*
         * Return driver information.
         *   returns : read-only pointer to a vo_info_t structure.
         */
        const vo_info_t* (*get_info)(void);

        /*
         * Display a new RGB/BGR frame of the video to the screen.
         * params:
	 *   src[0] - pointer to the image
         */
        uint32_t (*draw_frame)(uint8_t *src[]);

        /*
         * Draw a planar YUV slice to the buffer:
	 * params:
	 *   src[3] = source image planes (Y,U,V)
         *   stride[3] = source image planes line widths (in bytes)
	 *   w,h = width*height of area to be copied (in Y pixels)
         *   x,y = position at the destination image (in Y pixels)
         */
        uint32_t (*draw_slice)(uint8_t *src[], int stride[], int w,int h, int x,int y);

   	/*
         * Draws OSD to the screen buffer
         */
        void (*draw_osd)(void);

        /*
         * Blit/Flip buffer to the screen. Must be called after each frame!
         */
        void (*flip_page)(void);

        /*
         * This func is called after every frames to handle keyboard and
	 * other events. It's called in PAUSE mode too!
         */
        void (*check_events)(void);

        /*
         * Closes driver. Should restore the original state of the system.
         */
        void (*uninit)(void);

} vo_functions_t;

char *vo_format_name(int format);
int vo_init(void);

// NULL terminated array of all drivers
extern vo_functions_t* video_out_drivers[];

extern int vo_flags;

extern int vo_config_count;

// correct resolution/bpp on screen:  (should be autodetected by vo_init())
extern int vo_depthonscreen;
extern int vo_screenwidth;
extern int vo_screenheight;

// requested resolution/bpp:  (-x -y -bpp options)
extern int vo_dx;
extern int vo_dy;
extern int vo_dwidth;
extern int vo_dheight;
extern int vo_dbpp;

extern int vo_old_x;
extern int vo_old_y; 
extern int vo_old_width;
extern int vo_old_height;

extern int vo_doublebuffering;
extern int vo_directrendering;
extern int vo_vsync;
extern int vo_fs;
extern int vo_fsmode;
extern float vo_panscan;

extern int vo_mouse_timer_const;

extern int vo_pts;
extern float vo_fps;

extern char *vo_subdevice;

#endif
