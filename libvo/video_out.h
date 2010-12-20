/*
 * Copyright (C) Aaron Holtzman - Aug 1999
 * Strongly modified, most parts rewritten: A'rpi/ESP-team - 2000-2001
 * (C) MPlayer developers
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_VIDEO_OUT_H
#define MPLAYER_VIDEO_OUT_H

#include <inttypes.h>
#include <stdbool.h>

//#include "font_load.h"
#include "libmpcodecs/img_format.h"
//#include "vidix/vidix.h"

#define MP_NOPTS_VALUE (-1LL<<63)

#define VO_EVENT_EXPOSE 1
#define VO_EVENT_RESIZE 2
#define VO_EVENT_KEYPRESS 4
#define VO_EVENT_REINIT 8
#define VO_EVENT_MOVE 16

/* Obsolete: VOCTRL_QUERY_VAA 1 */
/* does the device support the required format */
#define VOCTRL_QUERY_FORMAT 2
/* signal a device reset seek */
#define VOCTRL_RESET 3
/* used to switch to fullscreen */
#define VOCTRL_FULLSCREEN 5
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
/* equalizer controls */
#define VOCTRL_SET_EQUALIZER 17
struct voctrl_set_equalizer_args {
    const char *name;
    int value;
};
#define VOCTRL_GET_EQUALIZER 18
struct voctrl_get_equalizer_args {
    const char *name;
    int *valueptr;
};
/* Frame duplication */
#define VOCTRL_DUPLICATE_FRAME 20
// ... 21
#define VOCTRL_START_SLICE 21

#define VOCTRL_ONTOP 25
#define VOCTRL_ROOTWIN 26
#define VOCTRL_BORDER 27
#define VOCTRL_DRAW_EOSD 28
#define VOCTRL_GET_EOSD_RES 29
typedef struct {
  int w, h; // screen dimensions, including black borders
  int mt, mb, ml, mr; // borders (top, bottom, left, right)
} mp_eosd_res_t;

#define VOCTRL_SET_DEINTERLACE 30
#define VOCTRL_GET_DEINTERLACE 31

#define VOCTRL_UPDATE_SCREENINFO 32

#define VOCTRL_SET_YUV_COLORSPACE 33
#define VOCTRL_GET_YUV_COLORSPACE 34

// Vo can be used by xover
#define VOCTRL_XOVERLAY_SUPPORT 22

#define VOCTRL_XOVERLAY_SET_COLORKEY 24
typedef struct {
  uint32_t x11; // The raw x11 color
  uint16_t r,g,b;
} mp_colorkey_t;

#define VOCTRL_XOVERLAY_SET_WIN 23
#define VOCTRL_REDRAW_OSD 24

typedef struct {
  int x,y;
  int w,h;
} mp_win_t;

#define VO_TRUE		1
#define VO_FALSE	0
#define VO_ERROR	-1
#define VO_NOTAVAIL	-2
#define VO_NOTIMPL	-3

#define VOFLAG_FULLSCREEN	0x01
#define VOFLAG_MODESWITCHING	0x02
#define VOFLAG_SWSCALE		0x04
#define VOFLAG_FLIPPING		0x08
#define VOFLAG_HIDDEN		0x10  //< Use to create a hidden window
#define VOFLAG_STEREO		0x20  //< Use to create a stereo-capable window
#define VOFLAG_XOVERLAY_SUB_VO  0x10000

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

struct vo;
struct osd_state;
struct mp_image;

struct vo_driver {
    // Driver uses new API
    bool is_new;
    // Driver buffers or adds (deinterlace) frames and will keep track
    // of pts values itself
    bool buffer_frames;

    // This is set if the driver is not new and contains pointers to
    // old-API functions to be used instead of the ones below.
    struct vo_old_functions *old_functions;

    const vo_info_t *info;
    /*
     * Preinitializes driver (real INITIALIZATION)
     *   arg - currently it's vo_subdevice
     *   returns: zero on successful initialization, non-zero on error.
     */
    int (*preinit)(struct vo *vo, const char *arg);
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
    int (*config)(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t fullscreen,
                  char *title, uint32_t format);

    /*
     * Control interface
     */
    int (*control)(struct vo *vo, uint32_t request, void *data);

    void (*draw_image)(struct vo *vo, struct mp_image *mpi, double pts);

    /*
     * Get extra frames from the VO, such as those added by VDPAU
     * deinterlace. Preparing the next such frame if any could be done
     * automatically by the VO after a previous flip_page(), but having
     * it as a separate step seems to allow making code more robust.
     */
    void (*get_buffered_frame)(struct vo *vo, bool eof);

    /*
     * Draw a planar YUV slice to the buffer:
     * params:
     *   src[3] = source image planes (Y,U,V)
     *   stride[3] = source image planes line widths (in bytes)
     *   w,h = width*height of area to be copied (in Y pixels)
     *   x,y = position at the destination image (in Y pixels)
     */
    int (*draw_slice)(struct vo *vo, uint8_t *src[], int stride[], int w,
                      int h, int x, int y);

    /*
     * Draws OSD to the screen buffer
     */
    void (*draw_osd)(struct vo *vo, struct osd_state *osd);

    /*
     * Blit/Flip buffer to the screen. Must be called after each frame!
     */
    void (*flip_page)(struct vo *vo);
    void (*flip_page_timed)(struct vo *vo, unsigned int pts_us, int duration);

    /*
     * This func is called after every frames to handle keyboard and
     * other events. It's called in PAUSE mode too!
     */
    void (*check_events)(struct vo *vo);

    /*
     * Closes driver. Should restore the original state of the system.
     */
    void (*uninit)(struct vo *vo);
};

struct vo_old_functions {
    int (*preinit)(const char *arg);
    int (*config)(uint32_t width, uint32_t height, uint32_t d_width,
                  uint32_t d_height, uint32_t fullscreen, char *title,
                  uint32_t format);
    int (*control)(uint32_t request, void *data);
    int (*draw_frame)(uint8_t *src[]);
    int (*draw_slice)(uint8_t *src[], int stride[], int w,int h, int x,int y);
    void (*draw_osd)(void);
    void (*flip_page)(void);
    void (*check_events)(void);
    void (*uninit)(void);
};

struct vo {
    int config_ok;  // Last config call was successful?
    int config_count;  // Total number of successful config calls

    bool frame_loaded;  // Is there a next frame the VO could flip to?
    double next_pts;    // pts value of the next frame if any
    double next_pts2;   // optional pts of frame after that

    double flip_queue_offset; // queue flip events at most this much in advance

    const struct vo_driver *driver;
    void *priv;
    struct MPOpts *opts;
    struct vo_x11_state *x11;
    struct mp_fifo *key_fifo;
    struct input_ctx *input_ctx;
    int event_fd;  // check_events() should be called when this has input
    int registered_fd;  // set to event_fd when registered in input system

    // requested position/resolution
    int dx;
    int dy;
    int dwidth;
    int dheight;

    int panscan_x;
    int panscan_y;
    float panscan_amount;
    float monitor_aspect;
    struct aspect_data {
        int orgw; // real width
        int orgh; // real height
        int prew; // prescaled width
        int preh; // prescaled height
        int scrw; // horizontal resolution
        int scrh; // vertical resolution
        float asp;
    } aspdat;
};

struct vo *init_best_video_out(struct MPOpts *opts, struct vo_x11_state *x11,
                               struct mp_fifo *key_fifo,
                               struct input_ctx *input_ctx);
int vo_config(struct vo *vo, uint32_t width, uint32_t height,
                     uint32_t d_width, uint32_t d_height, uint32_t flags,
                     char *title, uint32_t format);
void list_video_out(void);

int vo_control(struct vo *vo, uint32_t request, void *data);
int vo_draw_image(struct vo *vo, struct mp_image *mpi, double pts);
int vo_get_buffered_frame(struct vo *vo, bool eof);
int vo_draw_frame(struct vo *vo, uint8_t *src[]);
int vo_draw_slice(struct vo *vo, uint8_t *src[], int stride[], int w, int h, int x, int y);
void vo_draw_osd(struct vo *vo, struct osd_state *osd);
void vo_flip_page(struct vo *vo, unsigned int pts_us, int duration);
void vo_check_events(struct vo *vo);
void vo_seek_reset(struct vo *vo);
void vo_destroy(struct vo *vo);


// NULL terminated array of all drivers
extern const struct vo_driver *video_out_drivers[];

extern int xinerama_screen;
extern int xinerama_x;
extern int xinerama_y;

extern int vo_grabpointer;
extern int vo_doublebuffering;
extern int vo_directrendering;
extern int vo_vsync;
extern int vo_fs;
extern int vo_fsmode;
extern float vo_panscan;
extern int vo_adapter_num;
extern int vo_refresh_rate;
extern int vo_keepaspect;
extern int vo_rootwin;
extern int vo_border;

extern int vo_nomouse_input;
extern int enable_mouse_movements;

extern int vo_pts;
extern float vo_fps;

extern char *vo_subdevice;

extern int vo_colorkey;

extern int64_t WinID;

typedef struct {
        float min;
	float max;
	} range_t;

float range_max(range_t *r);
int in_range(range_t *r, float f);
range_t *str2range(char *s);
extern char *monitor_hfreq_str;
extern char *monitor_vfreq_str;
extern char *monitor_dotclock_str;

struct mp_keymap {
  int from;
  int to;
};
int lookup_keymap_table(const struct mp_keymap *map, int key);
struct vo_rect {
  int left, right, top, bottom, width, height;
};
void calc_src_dst_rects(struct vo *vo, int src_width, int src_height,
                        struct vo_rect *src, struct vo_rect *dst,
                        struct vo_rect *borders, const struct vo_rect *crop);
struct input_ctx;
void vo_mouse_movement(struct vo *vo, int posx, int posy);

static inline int aspect_scaling(void)
{
  return vo_keepaspect || vo_fs;
}

#endif /* MPLAYER_VIDEO_OUT_H */
