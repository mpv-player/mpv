/*
 *  video_out.h
 *
 *      Copyright (C) Aaron Holtzman - Aug 1999
 *	Strongly modified, most parts rewritten: A'rpi/ESP-team - 2000-2001
 *
 */

#include <inttypes.h>

#include "font_load.h"
#include "img_format.h"
#include "../vidix/vidix.h"

#define VO_EVENT_EXPOSE 1
#define VO_EVENT_RESIZE 2
#define VO_EVENT_KEYPRESS 4

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
	unsigned	frame_size;		/* destinition frame size */
	unsigned	num_frames;		/* number available frames */
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
        uint32_t (*init)(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format);

        /*
         * Query that given pixel format is supported or not.
	 * params:
	 *   format: fourcc of pixel format
         * returns : 1 if supported, 0 if unsupported
         */
        uint32_t (*query_format)(uint32_t format);

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

	/*
	 * Query Video Accelerated Architecture information.
	 * params:
	 *   vaa: address of struct to be filled.
	 *  (Note: driver should memset it to ZERO if it doesn't support vaa.)
	 */
	void (*query_vaa)(vo_vaa_t *vaa);

} vo_functions_t;

char *vo_format_name(int format);
int vo_init(void);

// NULL terminated array of all drivers
extern vo_functions_t* video_out_drivers[];

// currect resolution/bpp on screen:  (should be autodetected by vo_init())
extern int vo_depthonscreen;
extern int vo_screenwidth;
extern int vo_screenheight;

// requested resolution/bpp:  (-x -y -bpp options)
extern int vo_dwidth;
extern int vo_dheight;
extern int vo_dbpp;

extern int vo_doublebuffering;
extern int vo_fsmode;

extern int vo_pts;
extern float vo_fps;

extern char *vo_subdevice;

