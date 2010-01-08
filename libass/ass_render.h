/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 * Copyright (C) 2009 Grigori Goronzy <greg@geekmind.org>
 *
 * This file is part of libass.
 *
 * libass is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libass is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libass; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef LIBASS_RENDER_H
#define LIBASS_RENDER_H

#include <inttypes.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include FT_GLYPH_H
#include FT_SYNTHESIS_H

#include "ass.h"
#include "ass_font.h"
#include "ass_bitmap.h"
#include "ass_cache.h"
#include "ass_utils.h"
#include "ass_fontconfig.h"
#include "ass_library.h"
#include "ass_drawing.h"

typedef struct {
    double xMin;
    double xMax;
    double yMin;
    double yMax;
} DBBox;

typedef struct {
    double x;
    double y;
} DVector;

typedef struct free_list {
    void *object;
    struct free_list *next;
} FreeList;

typedef struct {
    int frame_width;
    int frame_height;
    double font_size_coeff;     // font size multiplier
    double line_spacing;        // additional line spacing (in frame pixels)
    int top_margin;             // height of top margin. Everything except toptitles is shifted down by top_margin.
    int bottom_margin;          // height of bottom margin. (frame_height - top_margin - bottom_margin) is original video height.
    int left_margin;
    int right_margin;
    int use_margins;            // 0 - place all subtitles inside original frame
    // 1 - use margins for placing toptitles and subtitles
    double aspect;              // frame aspect ratio, d_width / d_height.
    double storage_aspect;      // pixel ratio of the source image
    ASS_Hinting hinting;

    char *default_font;
    char *default_family;
} ASS_Settings;

// a rendered event
typedef struct {
    ASS_Image *imgs;
    int top, height, left, width;
    int detect_collisions;
    int shift_direction;
    ASS_Event *event;
} EventImages;

typedef enum {
    EF_NONE = 0,
    EF_KARAOKE,
    EF_KARAOKE_KF,
    EF_KARAOKE_KO
} Effect;

// describes a glyph
// GlyphInfo and TextInfo are used for text centering and word-wrapping operations
typedef struct {
    unsigned symbol;
    unsigned skip;              // skip glyph when layouting text
    FT_Glyph glyph;
    FT_Glyph outline_glyph;
    Bitmap *bm;                 // glyph bitmap
    Bitmap *bm_o;               // outline bitmap
    Bitmap *bm_s;               // shadow bitmap
    FT_BBox bbox;
    FT_Vector pos;
    char linebreak;             // the first (leading) glyph of some line ?
    uint32_t c[4];              // colors
    FT_Vector advance;          // 26.6
    Effect effect_type;
    int effect_timing;          // time duration of current karaoke word
    // after process_karaoke_effects: distance in pixels from the glyph origin.
    // part of the glyph to the left of it is displayed in a different color.
    int effect_skip_timing;     // delay after the end of last karaoke word
    int asc, desc;              // font max ascender and descender
    int be;                     // blur edges
    double blur;                // gaussian blur
    double shadow_x;
    double shadow_y;
    double frx, fry, frz;       // rotation
    double fax, fay;            // text shearing

    BitmapHashKey hash_key;
} GlyphInfo;

typedef struct {
    double asc, desc;
} LineInfo;

typedef struct {
    GlyphInfo *glyphs;
    int length;
    LineInfo *lines;
    int n_lines;
    double height;
    int max_glyphs;
    int max_lines;
} TextInfo;

// Renderer state.
// Values like current font face, color, screen position, clipping and so on are stored here.
typedef struct {
    ASS_Event *event;
    ASS_Style *style;

    ASS_Font *font;
    char *font_path;
    double font_size;
    int flags;                  // decoration flags (underline/strike-through)

    FT_Stroker stroker;
    int alignment;              // alignment overrides go here; if zero, style value will be used
    double frx, fry, frz;
    double fax, fay;            // text shearing
    enum {
        EVENT_NORMAL,           // "normal" top-, sub- or mid- title
        EVENT_POSITIONED,       // happens after pos(,), margins are ignored
        EVENT_HSCROLL,          // "Banner" transition effect, text_width is unlimited
        EVENT_VSCROLL           // "Scroll up", "Scroll down" transition effects
    } evt_type;
    double pos_x, pos_y;        // position
    double org_x, org_y;        // origin
    char have_origin;           // origin is explicitly defined; if 0, get_base_point() is used
    double scale_x, scale_y;
    double hspacing;            // distance between letters, in pixels
    double border_x;            // outline width
    double border_y;
    uint32_t c[4];              // colors(Primary, Secondary, so on) in RGBA
    int clip_x0, clip_y0, clip_x1, clip_y1;
    char clip_mode;             // 1 = iclip
    char detect_collisions;
    uint32_t fade;              // alpha from \fad
    char be;                    // blur edges
    double blur;                // gaussian blur
    double shadow_x;
    double shadow_y;
    int drawing_mode;           // not implemented; when != 0 text is discarded, except for style override tags
    ASS_Drawing *drawing;       // current drawing
    ASS_Drawing *clip_drawing;  // clip vector
    int clip_drawing_mode;      // 0 = regular clip, 1 = inverse clip

    Effect effect_type;
    int effect_timing;
    int effect_skip_timing;

    enum {
        SCROLL_LR,              // left-to-right
        SCROLL_RL,
        SCROLL_TB,              // top-to-bottom
        SCROLL_BT
    } scroll_direction;         // for EVENT_HSCROLL, EVENT_VSCROLL
    int scroll_shift;

    // face properties
    char *family;
    unsigned bold;
    unsigned italic;
    int treat_family_as_pattern;
    int wrap_style;
} RenderContext;

typedef struct {
    Hashmap *font_cache;
    Hashmap *glyph_cache;
    Hashmap *bitmap_cache;
    Hashmap *composite_cache;
    size_t glyph_max;
    size_t bitmap_max_size;
} CacheStore;

struct ass_renderer {
    ASS_Library *library;
    FT_Library ftlibrary;
    FCInstance *fontconfig_priv;
    ASS_Settings settings;
    int render_id;
    ASS_SynthPriv *synth_priv;

    ASS_Image *images_root;     // rendering result is stored here
    ASS_Image *prev_images_root;

    EventImages *eimg;          // temporary buffer for sorting rendered events
    int eimg_size;              // allocated buffer size

    // frame-global data
    int width, height;          // screen dimensions
    int orig_height;            // frame height ( = screen height - margins )
    int orig_width;             // frame width ( = screen width - margins )
    int orig_height_nocrop;     // frame height ( = screen height - margins + cropheight)
    int orig_width_nocrop;      // frame width ( = screen width - margins + cropwidth)
    ASS_Track *track;
    long long time;             // frame's timestamp, ms
    double font_scale;
    double font_scale_x;        // x scale applied to all glyphs to preserve text aspect ratio
    double border_scale;

    RenderContext state;
    TextInfo text_info;
    CacheStore cache;

    FreeList *free_head;
    FreeList *free_tail;
};

typedef struct render_priv {
    int top, height, left, width;
    int render_id;
} RenderPriv;

typedef struct {
    int x0;
    int y0;
    int x1;
    int y1;
} Rect;

typedef struct {
    int a, b;                   // top and height
    int ha, hb;                 // left and width
} Segment;

void reset_render_context(ASS_Renderer *render_priv);

#endif /* LIBASS_RENDER_H */
