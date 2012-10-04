/*
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

#ifndef MPLAYER_SUB_H
#define MPLAYER_SUB_H

#include <stdbool.h>

#include "subreader.h"
#include "dec_sub.h"

struct vo;

enum mp_osdtype {
    OSDTYPE_ASS,
    OSDTYPE_OSD,
    OSDTYPE_SUBTITLE,
    OSDTYPE_PROGBAR,
    OSDTYPE_SPU,

    MAX_OSD_PARTS
};

#define OSD_CONV_CACHE_MAX 3

struct osd_object {
    int type; // OSDTYPE_*
    bool force_redraw;

    // caches for OSD conversion (internal to render_object())
    struct osd_conv_cache *cache[OSD_CONV_CACHE_MAX];

    struct sub_bitmaps cached;

    // VO cache state
    int vo_bitmap_id;
    int vo_bitmap_pos_id;

    // Internally used by osd_libass.c
    struct ass_track *osd_track;
    struct sub_bitmap *parts_cache;
};

struct osd_state {
    struct osd_object *objs[MAX_OSD_PARTS];

    struct ass_library *ass_library;
    struct ass_renderer *ass_renderer;
    struct sh_sub *sh_sub;
    double sub_pts;
    double sub_offset;
    struct mp_eosd_res dim;
    double normal_scale;
    double vsfilter_scale;
    bool support_rgba;

    int w, h;

    char *osd_text;             // OSDTYPE_OSD
    int progbar_type, progbar_value; // OSDTYPE_PROGBAR

    // temporary for sub decoders
    int bitmap_id;
    int bitmap_pos_id;

    struct MPOpts *opts;

    // Internally used by osd_libass.c
    struct ass_renderer *osd_render;
    struct ass_library *osd_ass_library;
};

extern subtitle* vo_sub;

extern void* vo_spudec;
extern void* vo_vobsub;

// Start of OSD symbols in osd_font.pfb
#define OSD_CODEPOINTS 0xE000

// OSD symbols. osd_font.pfb has them starting from codepoint OSD_CODEPOINTS.
// Symbols with a value >= 32 are normal unicode codepoints.
enum mp_osd_font_codepoints {
    OSD_PLAY = 0x01,
    OSD_PAUSE = 0x02,
    OSD_STOP = 0x03,
    OSD_REW = 0x04,
    OSD_FFW = 0x05,
    OSD_CLOCK = 0x06,
    OSD_CONTRAST = 0x07,
    OSD_SATURATION = 0x08,
    OSD_VOLUME = 0x09,
    OSD_BRIGHTNESS = 0x0A,
    OSD_HUE = 0x0B,
    OSD_BALANCE = 0x0C,
    OSD_PANSCAN = 0x50,

    OSD_PB_START = 0x10,
    OSD_PB_0 = 0x11,
    OSD_PB_END = 0x12,
    OSD_PB_1 = 0x13,
};

/* now in textform */
extern char * const sub_osd_names[];
extern char * const sub_osd_names_short[];

extern int sub_unicode;
extern int sub_utf8;

extern char *sub_cp;
extern int sub_pos;
extern int sub_width_p;
extern int sub_bg_color; /* subtitles background color */
extern int sub_bg_alpha;
extern int spu_alignment;
extern int spu_aamode;
extern float spu_gaussvar;

extern char *subtitle_font_encoding;
extern float text_font_scale_factor;
extern float osd_font_scale_factor;
extern float subtitle_font_radius;
extern float subtitle_font_thickness;
extern int subtitle_autoscale;

extern char *font_name;
extern char *sub_font_name;
extern float font_factor;
extern float sub_delay;
extern float sub_fps;

extern int sub_justify;

void osd_draw_text(struct osd_state *osd, int dxs, int dys,
                   void (*draw_alpha)(void *ctx, int x0, int y0, int w, int h,
                                      unsigned char* src, unsigned char *srca,
                                      int stride),
                   void *ctx);
void osd_draw_text_ext(struct osd_state *osd, int dxs, int dys,
                       int left_border, int top_border, int right_border,
                       int bottom_border, int orig_w, int orig_h,
                       void (*draw_alpha)(void *ctx, int x0, int y0, int w,
                                          int h, unsigned char* src,
                                          unsigned char *srca,
                                          int stride),
                       void *ctx);

void draw_osd_with_eosd(struct vo *vo, struct osd_state *osd);

struct osd_state *osd_create(struct MPOpts *opts, struct ass_library *asslib);
void osd_set_text(struct osd_state *osd, const char *text);
void osd_update(struct osd_state *osd, int dxs, int dys);
void vo_osd_changed(int new_value);
void vo_osd_reset_changed(void);
bool vo_osd_has_changed(struct osd_state *osd);
void osd_free(struct osd_state *osd);

bool sub_bitmaps_bb(struct sub_bitmaps *imgs, int *x1, int *y1,
                    int *x2, int *y2);

// defined in osd_libass.c and osd_dummy.c

void osd_object_get_bitmaps(struct osd_state *osd, struct osd_object *obj,
                            struct sub_bitmaps *out_imgs);
void osd_get_function_sym(char *buffer, size_t buffer_size, int osd_function);
void osd_init_backend(struct osd_state *osd);
void osd_destroy_backend(struct osd_state *osd);

#endif /* MPLAYER_SUB_H */
