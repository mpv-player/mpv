/*
 *  video_out.h
 *
 *      Copyright (C) Aaron Holtzman - Aug 1999
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
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct vo_image_buffer_s
{
        uint32_t height;
        uint32_t width;
        uint32_t format;
        uint8_t *base;
        void *private;
} vo_image_buffer_t;

typedef struct vo_functions_s
{
        /*
         * Initialize the display driver.
         *
         *    params : width  == width of video to display.
         *             height == height of video to display.
         *             fullscreen == non-zero if driver should attempt to
         *                           render in fullscreen mode. Zero if
         *                           a windowed mode is requested. This is
         *                           merely a request; if the driver can only do
         *                           fullscreen (like fbcon) or windowed (like X11),
         *                           than this param may be disregarded.
         *             title == string for titlebar of window. May be disregarded
         *                      if there is no such thing as a window to your
         *                      driver. Make a copy of this string, if you need it.
         *             format == desired fourCC code to use for image buffers
         *   returns : zero on successful initialization, non-zero on error.
         *              The program will probably respond to an error condition
         *              by terminating.
         */

        uint32_t (*init)(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format);

        uint32_t (*query_format)(uint32_t format);

        /*
         * Return driver information.
         *
         *    params : none.
         *   returns : read-only pointer to a vo_info_t structure.
         *             Fields are non-NULL.
         *             Should not return NULL.
         */

        const vo_info_t* (*get_info)(void);

        /*
         * Display a new frame of the video to the screen. This may get called very
         *  rapidly, so the more efficient you can make your implementation of this
         *  function, the better.
         *
         *    params : *src[] == An array with three elements. This is a YUV
         *                       stream, with the Y plane in src[0], U in src[1],
         *                       and V in src[2]. There is enough data for an image
         *                       that is (WxH) pixels, where W and H are the width
         *                       and height parameters that were previously passed
         *                       to display_init().
         *                         Information on the YUV format can be found at:
         *                           http://www.webartz.com/fourcc/fccyuv.htm#IYUV
         *
         *   returns : zero on successful rendering, non-zero on error.
         *              The program will probably respond to an error condition
         *              by terminating.
         */

        uint32_t (*draw_frame)(uint8_t *src[]);

        /*
         * Update a section of the offscreen buffer. A "slice" is an area of the
         *  video image that is 16 rows of pixels at the width of the video image.
         *  Position (0, 0) is the upper left corner of slice #0 (the first slice),
         *  and position (0, 15) is the lower right. The next slice, #1, is bounded
         *  by (0, 16) and (0, 31), and so on.
         *
         * Note that slices are not drawn directly to the screen, and should be
         *  buffered until your implementation of display_flip_page() (see below)
         *  is called.
         *
         * This may get called very rapidly, so the more efficient you can make your
         *  implementation of this function, the better.
         *
         *     params : *src[] == see display_frame(), above. The data passed in this
         *                         array is just what enough data to contain the
         *                         new slice, and NOT the entire frame.
         *              slice_num == The index of the slice. Starts at 0, not 1.
         *
         *   returns : zero on successful rendering, non-zero on error.
         *              The program will probably respond to an error condition
         *              by terminating.
         */

    // src[3] = source image planes (Y,U,V)
    // stride[3] = source image planes line widths (in bytes)
    // w,h = width*height of area to be copied (in Y pixels)
    // x,y = position at the destination image (in Y pixels)

        uint32_t (*draw_slice)(uint8_t *src[], int stride[], int w,int h, int x,int y);

        /*
         * Draw the current image buffer to the screen. There may be several
         *  display_slice() calls before display_flip_page() is used. Note that
         *  display_frame does an implicit page flip, so you might or might not
         *  want to call this internally from your display_frame() implementation.
         *
         * This may get called very rapidly, so the more efficient you can make
         *  your implementation of this function, the better.
         *
         *     params : void.
         *    returns : void.
         */

        void (*flip_page)(void);

        void (*uninit)(void);

} vo_functions_t;

// NULL terminated array of all drivers
extern vo_functions_t* video_out_drivers[];


#ifdef X11_FULLSCREEN

// X11 keyboard codes
#include "wskeys.h"

extern int vo_depthonscreen;
extern int vo_screenwidth;
extern int vo_screenheight;
int vo_init( void );
//void vo_decoration( Display * vo_Display,Window w,int d );

extern int vo_eventhandler_pid;
void vo_kill_eventhandler();

#endif


#ifdef __cplusplus
}
#endif
