/*
 *  video_out.h
 *
 *      Copyright (C) Aaron Holtzman - Aug 1999
 *	Strongly modified, most parts rewritten: A'rpi/ESP-team - 2000-2001
 *
 */

#include <inttypes.h>

#define IMGFMT_YV12 0x32315659
//#define IMGFMT_YUY2 (('Y'<<24)|('U'<<16)|('Y'<<8)|'2')
#define IMGFMT_YUY2 (('2'<<24)|('Y'<<16)|('U'<<8)|'Y')

#define IMGFMT_RGB_MASK 0xFFFFFF00
#define IMGFMT_RGB (('R'<<24)|('G'<<16)|('B'<<8))
#define IMGFMT_BGR_MASK 0xFFFFFF00
#define IMGFMT_BGR (('B'<<24)|('G'<<16)|('R'<<8))
#define IMGFMT_RGB15 (IMGFMT_RGB|15)
#define IMGFMT_RGB16 (IMGFMT_RGB|16)
#define IMGFMT_RGB24 (IMGFMT_RGB|24)
#define IMGFMT_RGB32 (IMGFMT_RGB|32)

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

typedef struct vo_functions_s
{
        /*
         * Initialize the display driver.
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

// NULL terminated array of all drivers
extern vo_functions_t* video_out_drivers[];

