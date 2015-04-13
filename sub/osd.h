/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_SUB_H
#define MPLAYER_SUB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "options/m_option.h"

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

    struct {
        uint32_t color;
    } libass;
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

    int change_id;  // Incremented on each change
};

struct mp_osd_res {
    int w, h; // screen dimensions, including black borders
    int mt, mb, ml, mr; // borders (top, bottom, left, right)
    double display_par;
};

enum mp_osdtype {
    OSDTYPE_SUB,
    OSDTYPE_SUB2,

    OSDTYPE_NAV_HIGHLIGHT,      // dvdnav fake highlights

    OSDTYPE_PROGBAR,
    OSDTYPE_OSD,

    OSDTYPE_EXTERNAL,
    OSDTYPE_EXTERNAL2,

    MAX_OSD_PARTS
};

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
    int align_x;
    int align_y;
    float blur;
    int bold;
};

extern const struct m_sub_options osd_style_conf;
extern const struct m_sub_options sub_style_conf;

struct osd_state;
struct osd_object;
struct mpv_global;

struct osd_state *osd_create(struct mpv_global *global);
void osd_changed(struct osd_state *osd, int new_value);
void osd_changed_all(struct osd_state *osd);
void osd_free(struct osd_state *osd);

bool osd_query_and_reset_want_redraw(struct osd_state *osd);

void osd_set_text(struct osd_state *osd, int obj, const char *text);

struct osd_sub_state {
    struct dec_sub *dec_sub;
    double video_offset;
    bool render_bitmap_subs;
};
void osd_set_sub(struct osd_state *osd, int obj, struct osd_sub_state *substate);

bool osd_get_render_subs_in_filter(struct osd_state *osd);
void osd_set_render_subs_in_filter(struct osd_state *osd, bool s);

struct osd_progbar_state {
    int type;           // <0: disabled, 1-255: symbol, else: no symbol
    float value;        // range 0.0-1.0
    float *stops;       // used for chapter indicators (0.0-1.0 each)
    int num_stops;
};
void osd_set_progbar(struct osd_state *osd, struct osd_progbar_state *s);

void osd_set_external(struct osd_state *osd, int res_x, int res_y, char *text);

void osd_set_external2(struct osd_state *osd, struct sub_bitmaps *imgs);

void osd_set_nav_highlight(struct osd_state *osd, void *priv);

enum mp_osd_draw_flags {
    OSD_DRAW_SUB_FILTER = (1 << 0),
    OSD_DRAW_SUB_ONLY   = (1 << 1),
    OSD_DRAW_OSD_ONLY   = (1 << 2),
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

struct mp_image_params;
struct mp_osd_res osd_res_from_image_params(const struct mp_image_params *p);

void osd_object_get_scale_factor(struct osd_state *osd, int obj,
                                 double *sw, double *sh);

void osd_coords_to_video(struct osd_state *osd, int frame_w, int frame_h,
                         int *x, int *y);

struct mp_osd_res osd_get_vo_res(struct osd_state *osd, int obj);

void osd_rescale_bitmaps(struct sub_bitmaps *imgs, int frame_w, int frame_h,
                         struct mp_osd_res res, double compensate_par);

// defined in osd_libass.c and osd_dummy.c

// internal use only
void osd_object_get_bitmaps(struct osd_state *osd, struct osd_object *obj,
                            struct sub_bitmaps *out_imgs);
void osd_init_backend(struct osd_state *osd);
void osd_destroy_backend(struct osd_state *osd);

// doesn't need locking
void osd_get_function_sym(char *buffer, size_t buffer_size, int osd_function);
extern const char *const osd_ass_0;
extern const char *const osd_ass_1;

// defined in backend, but locks if required
void osd_object_get_resolution(struct osd_state *osd, int obj,
                               int *out_w, int *out_h);

// defined in player
void mp_nav_get_highlight(void *priv, struct mp_osd_res res,
                          struct sub_bitmaps *out_imgs);

#endif /* MPLAYER_SUB_H */
