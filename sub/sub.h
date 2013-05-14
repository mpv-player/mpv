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

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "core/m_option.h"

// NOTE: VOs must support at least SUBBITMAP_RGBA.
enum sub_bitmap_format {
    SUBBITMAP_EMPTY = 0,// no bitmaps; always has num_parts==0
    SUBBITMAP_LIBASS,   // A8, with a per-surface blend color (libass.color)
    SUBBITMAP_RGBA,     // B8G8R8A8 (MSB=A, LSB=B), scaled, premultiplied alpha
    SUBBITMAP_INDEXED,  // scaled, bitmap points to osd_bmp_indexed

    SUBBITMAP_COUNT
};

// For SUBBITMAP_INDEXED
struct osd_bmp_indexed {
    uint8_t *bitmap;
    // Each entry is like a pixel in SUBBITMAP_RGBA format, but using straight
    // alpha.
    uint32_t palette[256];
};

struct sub_bitmap {
    void *bitmap;
    int stride;
    // Note: not clipped, going outside the screen area is allowed
    //       (except for SUBBITMAP_LIBASS, which is always clipped)
    int w, h;
    int x, y;
    int dw, dh;

    union {
        struct {
            uint32_t color;
        } libass;
    };
};

struct sub_bitmaps {
    // For VO cache state (limited by MAX_OSD_PARTS)
    int render_index;

    enum sub_bitmap_format format;

    // If false, dw==w && dh==h.
    // SUBBITMAP_LIBASS is never scaled.
    bool scaled;

    struct sub_bitmap *parts;
    int num_parts;

    // Incremented on each change
    int bitmap_id, bitmap_pos_id;
};

struct mp_osd_res {
    int w, h; // screen dimensions, including black borders
    int mt, mb, ml, mr; // borders (top, bottom, left, right)
    double display_par;
    double video_par; // PAR of the original video (for some sub decoders)
};

enum mp_osdtype {
    OSDTYPE_SUB,
    OSDTYPE_SUBTITLE,
    OSDTYPE_SPU,

    OSDTYPE_PROGBAR,
    OSDTYPE_OSD,

    MAX_OSD_PARTS
};

#define OSD_CONV_CACHE_MAX 4

struct osd_object {
    int type; // OSDTYPE_*
    bool is_sub;

    bool force_redraw;

    // caches for OSD conversion (internal to render_object())
    struct osd_conv_cache *cache[OSD_CONV_CACHE_MAX];
    struct sub_bitmaps cached;

    // VO cache state
    int vo_bitmap_id;
    int vo_bitmap_pos_id;
    struct mp_osd_res vo_res;

    // Internally used by osd_libass.c
    struct ass_track *osd_track;
    struct sub_bitmap *parts_cache;
};

struct osd_state {
    struct osd_object *objs[MAX_OSD_PARTS];

    struct ass_library *ass_library;
    struct ass_renderer *ass_renderer;
    struct sh_sub *sh_sub;
    double sub_offset;
    double vo_pts;

    bool render_subs_in_filter;

    bool want_redraw;

    // OSDTYPE_OSD
    char *osd_text;
    // OSDTYPE_PROGBAR
    int progbar_type;      // <0: disabled, 1-255: symbol, else: no symbol
    float progbar_value;   // range 0.0-1.0
    float *progbar_stops;  // used for chapter indicators (0.0-1.0 each)
    int progbar_num_stops;

    int switch_sub_id;

    struct MPOpts *opts;

    // Internal to sub.c
    struct mp_draw_sub_cache *draw_cache;

    // Internally used by osd_libass.c
    struct ass_renderer *osd_render;
    struct ass_library *osd_ass_library;
};

extern struct subtitle* vo_sub;

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

struct osd_style_opts {
    char *font;
    float font_size;
    struct m_color color;
    struct m_color border_color;
    struct m_color shadow_color;
    struct m_color back_color;
    float border_size;
    float shadow_offset;
    float spacing;
    int margin_x;
    int margin_y;
    float blur;
};

extern const struct m_sub_options osd_style_conf;

extern char *sub_cp;
extern int sub_pos;

extern float sub_delay;
extern float sub_fps;


struct osd_state *osd_create(struct MPOpts *opts, struct ass_library *asslib);
void osd_set_text(struct osd_state *osd, const char *text);
void vo_osd_changed(int new_value);
void osd_changed_all(struct osd_state *osd);
void osd_free(struct osd_state *osd);

enum mp_osd_draw_flags {
    OSD_DRAW_SUB_FILTER = (1 << 0),
    OSD_DRAW_SUB_ONLY   = (1 << 1),
};

void osd_draw(struct osd_state *osd, struct mp_osd_res res,
              double video_pts, int draw_flags,
              const bool formats[SUBBITMAP_COUNT],
              void (*cb)(void *ctx, struct sub_bitmaps *imgs), void *cb_ctx);

struct mp_image;
bool osd_draw_on_image(struct osd_state *osd, struct mp_osd_res res,
                       double video_pts, int draw_flags, struct mp_image *dest);

struct mp_image_pool;
void osd_draw_on_image_p(struct osd_state *osd, struct mp_osd_res res,
                         double video_pts, int draw_flags,
                         struct mp_image_pool *pool, struct mp_image *dest);

// defined in osd_libass.c and osd_dummy.c

void osd_object_get_bitmaps(struct osd_state *osd, struct osd_object *obj,
                            struct sub_bitmaps *out_imgs);
void osd_get_function_sym(char *buffer, size_t buffer_size, int osd_function);
void osd_init_backend(struct osd_state *osd);
void osd_destroy_backend(struct osd_state *osd);

#endif /* MPLAYER_SUB_H */
