// -*- c-basic-offset: 8; indent-tabs-mode: t -*-
// vim:ts=8:sw=8:noet:ai:
/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
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

#include "config.h"

#include <assert.h>
#include <math.h>
#include <inttypes.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include FT_GLYPH_H
#include FT_SYNTHESIS_H

#include "mputils.h"

#include "ass.h"
#include "ass_font.h"
#include "ass_bitmap.h"
#include "ass_cache.h"
#include "ass_utils.h"
#include "ass_fontconfig.h"
#include "ass_library.h"

#define MAX_GLYPHS 3000
#define MAX_LINES 300
#define BLUR_MAX_RADIUS 50.0
#define MAX_BE 100
#define ROUND(x) ((int) ((x) + .5))
#define SUBPIXEL_MASK 56	// d6 bitmask for subpixel accuracy adjustment

static int last_render_id = 0;

typedef struct ass_settings_s {
	int frame_width;
	int frame_height;
	double font_size_coeff; // font size multiplier
	double line_spacing; // additional line spacing (in frame pixels)
	int top_margin; // height of top margin. Everything except toptitles is shifted down by top_margin.
	int bottom_margin; // height of bottom margin. (frame_height - top_margin - bottom_margin) is original video height.
	int left_margin;
	int right_margin;
	int use_margins; // 0 - place all subtitles inside original frame
	                 // 1 - use margins for placing toptitles and subtitles
	double aspect; // frame aspect ratio, d_width / d_height.
	ass_hinting_t hinting;

	char* default_font;
	char* default_family;
} ass_settings_t;

// a rendered event
typedef struct event_images_s {
	ass_image_t* imgs;
	int top, height;
	int detect_collisions;
	int shift_direction;
	ass_event_t* event;
} event_images_t;

struct ass_renderer_s {
	ass_library_t* library;
	FT_Library ftlibrary;
	fc_instance_t* fontconfig_priv;
	ass_settings_t settings;
	int render_id;
	ass_synth_priv_t* synth_priv;

	ass_image_t* images_root; // rendering result is stored here
	ass_image_t* prev_images_root;

	event_images_t* eimg; // temporary buffer for sorting rendered events
	int eimg_size; // allocated buffer size
};

typedef enum {EF_NONE = 0, EF_KARAOKE, EF_KARAOKE_KF, EF_KARAOKE_KO} effect_t;

// describes a glyph
// glyph_info_t and text_info_t are used for text centering and word-wrapping operations
typedef struct glyph_info_s {
	unsigned symbol;
	FT_Glyph glyph;
	FT_Glyph outline_glyph;
	bitmap_t* bm; // glyph bitmap
	bitmap_t* bm_o; // outline bitmap
	bitmap_t* bm_s; // shadow bitmap
	FT_BBox bbox;
	FT_Vector pos;
	char linebreak; // the first (leading) glyph of some line ?
	uint32_t c[4]; // colors
	FT_Vector advance; // 26.6
	effect_t effect_type;
	int effect_timing; // time duration of current karaoke word
	                   // after process_karaoke_effects: distance in pixels from the glyph origin.
	                   // part of the glyph to the left of it is displayed in a different color.
	int effect_skip_timing; // delay after the end of last karaoke word
	int asc, desc; // font max ascender and descender
//	int height;
	int be; // blur edges
	double blur; // gaussian blur
	double shadow;
	double frx, fry, frz; // rotation
	
	bitmap_hash_key_t hash_key;
} glyph_info_t;

typedef struct line_info_s {
	int asc, desc;
} line_info_t;

typedef struct text_info_s {
	glyph_info_t* glyphs;
	int length;
	line_info_t lines[MAX_LINES];
	int n_lines;
	int height;
} text_info_t;


// Renderer state.
// Values like current font face, color, screen position, clipping and so on are stored here.
typedef struct render_context_s {
	ass_event_t* event;
	ass_style_t* style;
	
	ass_font_t* font;
	char* font_path;
	double font_size;
	
	FT_Stroker stroker;
	int alignment; // alignment overrides go here; if zero, style value will be used
	double frx, fry, frz;
	enum {	EVENT_NORMAL, // "normal" top-, sub- or mid- title
		EVENT_POSITIONED, // happens after pos(,), margins are ignored
		EVENT_HSCROLL, // "Banner" transition effect, text_width is unlimited
		EVENT_VSCROLL // "Scroll up", "Scroll down" transition effects
		} evt_type;
	double pos_x, pos_y; // position
	double org_x, org_y; // origin
	char have_origin; // origin is explicitly defined; if 0, get_base_point() is used
	double scale_x, scale_y;
	double hspacing; // distance between letters, in pixels
	double border; // outline width
	uint32_t c[4]; // colors(Primary, Secondary, so on) in RGBA
	int clip_x0, clip_y0, clip_x1, clip_y1;
	char detect_collisions;
	uint32_t fade; // alpha from \fad
	char be; // blur edges
	double blur; // gaussian blur
	double shadow;
	int drawing_mode; // not implemented; when != 0 text is discarded, except for style override tags

	effect_t effect_type;
	int effect_timing;
	int effect_skip_timing;

	enum { SCROLL_LR, // left-to-right
	       SCROLL_RL,
	       SCROLL_TB, // top-to-bottom
	       SCROLL_BT
	       } scroll_direction; // for EVENT_HSCROLL, EVENT_VSCROLL
	int scroll_shift;

	// face properties
	char* family;
	unsigned bold;
	unsigned italic;
	int treat_family_as_pattern;
	
} render_context_t;

// frame-global data
typedef struct frame_context_s {
	ass_renderer_t* ass_priv;
	int width, height; // screen dimensions
	int orig_height; // frame height ( = screen height - margins )
	int orig_width; // frame width ( = screen width - margins )
	int orig_height_nocrop; // frame height ( = screen height - margins + cropheight)
	int orig_width_nocrop; // frame width ( = screen width - margins + cropwidth)
	ass_track_t* track;
	long long time; // frame's timestamp, ms
	double font_scale;
	double font_scale_x; // x scale applied to all glyphs to preserve text aspect ratio
	double border_scale;
} frame_context_t;

static ass_renderer_t* ass_renderer;
static ass_settings_t* global_settings;
static text_info_t text_info;
static render_context_t render_context;
static frame_context_t frame_context;

struct render_priv_s {
	int top, height;
	int render_id;
};

static void ass_lazy_track_init(void)
{
	ass_track_t* track = frame_context.track;
	if (track->PlayResX && track->PlayResY)
		return;
	if (!track->PlayResX && !track->PlayResY) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_NeitherPlayResXNorPlayResYDefined);
		track->PlayResX = 384;
		track->PlayResY = 288;
	} else {
		double orig_aspect = (global_settings->aspect * frame_context.height * frame_context.orig_width) /
			frame_context.orig_height / frame_context.width;
		if (!track->PlayResY && track->PlayResX == 1280) {
			track->PlayResY = 1024;
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_PlayResYUndefinedSettingY, track->PlayResY);
		} else if (!track->PlayResY) {
			track->PlayResY = track->PlayResX / orig_aspect + .5;
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_PlayResYUndefinedSettingY, track->PlayResY);
		} else if (!track->PlayResX && track->PlayResY == 1024) {
			track->PlayResX = 1280;
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_PlayResXUndefinedSettingX, track->PlayResX);
		} else if (!track->PlayResX) {
			track->PlayResX = track->PlayResY * orig_aspect + .5;
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_PlayResXUndefinedSettingX, track->PlayResX);
		}
	}
}

ass_renderer_t* ass_renderer_init(ass_library_t* library)
{
	int error;
	FT_Library ft;
	ass_renderer_t* priv = 0;
	int vmajor, vminor, vpatch;
	
	memset(&render_context, 0, sizeof(render_context));
	memset(&frame_context, 0, sizeof(frame_context));
	memset(&text_info, 0, sizeof(text_info));

	error = FT_Init_FreeType( &ft );
	if ( error ) { 
		mp_msg(MSGT_ASS, MSGL_FATAL, MSGTR_LIBASS_FT_Init_FreeTypeFailed);
		goto ass_init_exit;
	}

	FT_Library_Version(ft, &vmajor, &vminor, &vpatch);
	mp_msg(MSGT_ASS, MSGL_V, "FreeType library version: %d.%d.%d\n",
	       vmajor, vminor, vpatch);
	mp_msg(MSGT_ASS, MSGL_V, "FreeType headers version: %d.%d.%d\n",
	       FREETYPE_MAJOR, FREETYPE_MINOR, FREETYPE_PATCH);

	priv = calloc(1, sizeof(ass_renderer_t));
	if (!priv) {
		FT_Done_FreeType(ft);
		goto ass_init_exit;
	}

	priv->synth_priv = ass_synth_init(BLUR_MAX_RADIUS);

	priv->library = library;
	priv->ftlibrary = ft;
	// images_root and related stuff is zero-filled in calloc
	
	ass_font_cache_init();
	ass_bitmap_cache_init();
	ass_composite_cache_init();
	ass_glyph_cache_init();

	text_info.glyphs = calloc(MAX_GLYPHS, sizeof(glyph_info_t));
	
ass_init_exit:
	if (priv) mp_msg(MSGT_ASS, MSGL_INFO, MSGTR_LIBASS_Init);
	else mp_msg(MSGT_ASS, MSGL_ERR, MSGTR_LIBASS_InitFailed);

	return priv;
}

void ass_renderer_done(ass_renderer_t* priv)
{
	ass_font_cache_done();
	ass_bitmap_cache_done();
	ass_composite_cache_done();
	ass_glyph_cache_done();
	if (render_context.stroker) {
		FT_Stroker_Done(render_context.stroker);
		render_context.stroker = 0;
	}
	if (priv && priv->ftlibrary) FT_Done_FreeType(priv->ftlibrary);
	if (priv && priv->fontconfig_priv) fontconfig_done(priv->fontconfig_priv);
	if (priv && priv->synth_priv) ass_synth_done(priv->synth_priv);
	if (priv && priv->eimg) free(priv->eimg);
	if (priv) free(priv);
	if (text_info.glyphs) free(text_info.glyphs);
}

/**
 * \brief Create a new ass_image_t
 * Parameters are the same as ass_image_t fields.
 */
static ass_image_t* my_draw_bitmap(unsigned char* bitmap, int bitmap_w, int bitmap_h, int stride, int dst_x, int dst_y, uint32_t color)
{
	ass_image_t* img = calloc(1, sizeof(ass_image_t));
	
	img->w = bitmap_w;
	img->h = bitmap_h;
	img->stride = stride;
	img->bitmap = bitmap;
	img->color = color;
	img->dst_x = dst_x;
	img->dst_y = dst_y;

	return img;
}

/**
 * \brief convert bitmap glyph into ass_image_t struct(s)
 * \param bit freetype bitmap glyph, FT_PIXEL_MODE_GRAY
 * \param dst_x bitmap x coordinate in video frame
 * \param dst_y bitmap y coordinate in video frame
 * \param color first color, RGBA
 * \param color2 second color, RGBA
 * \param brk x coordinate relative to glyph origin, color is used to the left of brk, color2 - to the right
 * \param tail pointer to the last image's next field, head of the generated list should be stored here
 * \return pointer to the new list tail
 * Performs clipping. Uses my_draw_bitmap for actual bitmap convertion.
 */
static ass_image_t** render_glyph(bitmap_t* bm, int dst_x, int dst_y, uint32_t color, uint32_t color2, int brk, ass_image_t** tail)
{
	// brk is relative to dst_x
	// color = color left of brk
	// color2 = color right of brk
	int b_x0, b_y0, b_x1, b_y1; // visible part of the bitmap
	int clip_x0, clip_y0, clip_x1, clip_y1;
	int tmp;
	ass_image_t* img;

	dst_x += bm->left;
	dst_y += bm->top;
	brk -= bm->left;
	
	// clipping
	clip_x0 = render_context.clip_x0;
	clip_y0 = render_context.clip_y0;
	clip_x1 = render_context.clip_x1;
	clip_y1 = render_context.clip_y1;
	b_x0 = 0;
	b_y0 = 0;
	b_x1 = bm->w;
	b_y1 = bm->h;
	
	tmp = dst_x - clip_x0;
	if (tmp < 0) {
		mp_msg(MSGT_ASS, MSGL_DBG2, "clip left\n");
		b_x0 = - tmp;
	}
	tmp = dst_y - clip_y0;
	if (tmp < 0) {
		mp_msg(MSGT_ASS, MSGL_DBG2, "clip top\n");
		b_y0 = - tmp;
	}
	tmp = clip_x1 - dst_x - bm->w;
	if (tmp < 0) {
		mp_msg(MSGT_ASS, MSGL_DBG2, "clip right\n");
		b_x1 = bm->w + tmp;
	}
	tmp = clip_y1 - dst_y - bm->h;
	if (tmp < 0) {
		mp_msg(MSGT_ASS, MSGL_DBG2, "clip bottom\n");
		b_y1 = bm->h + tmp;
	}
	
	if ((b_y0 >= b_y1) || (b_x0 >= b_x1))
		return tail;

	if (brk > b_x0) { // draw left part
		if (brk > b_x1) brk = b_x1;
		img = my_draw_bitmap(bm->buffer + bm->w * b_y0 + b_x0, 
			brk - b_x0, b_y1 - b_y0, bm->w,
			dst_x + b_x0, dst_y + b_y0, color);
		*tail = img;
		tail = &img->next;
	}
	if (brk < b_x1) { // draw right part
		if (brk < b_x0) brk = b_x0;
		img = my_draw_bitmap(bm->buffer + bm->w * b_y0 + brk, 
			b_x1 - brk, b_y1 - b_y0, bm->w,
			dst_x + brk, dst_y + b_y0, color2);
		*tail = img;
		tail = &img->next;
	}
	return tail;
}

/**
 * \brief Calculate overlapping area of two consecutive bitmaps and in case they
 * overlap, composite them together
 * Mainly useful for translucent glyphs and especially borders, to avoid the
 * luminance adding up where they overlap (which looks ugly)
 */
static void render_overlap(ass_image_t** last_tail, ass_image_t** tail, bitmap_hash_key_t *last_hash, bitmap_hash_key_t* hash) {
	int left, top, bottom, right;
	int old_left, old_top, w, h, cur_left, cur_top;
	int x, y, opos, cpos;
	char m;
	composite_hash_key_t hk;
	composite_hash_val_t *hv;
	composite_hash_key_t *nhk;
	int ax = (*last_tail)->dst_x;
	int ay = (*last_tail)->dst_y;
	int aw = (*last_tail)->w;
	int as = (*last_tail)->stride;
	int ah = (*last_tail)->h;
	int bx = (*tail)->dst_x;
	int by = (*tail)->dst_y;
	int bw = (*tail)->w;
	int bs = (*tail)->stride;
	int bh = (*tail)->h;
	unsigned char* a;
	unsigned char* b;

	if ((*last_tail)->bitmap == (*tail)->bitmap)
		return;

	if ((*last_tail)->color != (*tail)->color)
		return;

	// Calculate overlap coordinates
	left = (ax > bx) ? ax : bx;
	top = (ay > by) ? ay : by;
	right = ((ax+aw) < (bx+bw)) ? (ax+aw) : (bx+bw);
	bottom = ((ay+ah) < (by+bh)) ? (ay+ah) : (by+bh);
	if ((right <= left) || (bottom <= top))
		return;
	old_left = left-ax;
	old_top = top-ay;
	w = right-left;
	h = bottom-top;
	cur_left = left-bx;
	cur_top = top-by;

	// Query cache
	memcpy(&hk.a, last_hash, sizeof(*last_hash));
	memcpy(&hk.b, hash, sizeof(*hash));
	hk.aw = aw;
	hk.ah = ah;
	hk.bw = bw;
	hk.bh = bh;
	hk.ax = ax;
	hk.ay = ay;
	hk.bx = bx;
	hk.by = by;
	hv = cache_find_composite(&hk);
	if (hv) {
		(*last_tail)->bitmap = hv->a;
		(*tail)->bitmap = hv->b;
		return;
	}

	// Allocate new bitmaps and copy over data
	a = (*last_tail)->bitmap;
	b = (*tail)->bitmap;
	(*last_tail)->bitmap = malloc(as*ah);
	(*tail)->bitmap = malloc(bs*bh);
	memcpy((*last_tail)->bitmap, a, as*ah);
	memcpy((*tail)->bitmap, b, bs*bh);

	// Composite overlapping area
	for (y=0; y<h; y++)
		for (x=0; x<w; x++) {
			opos = (old_top+y)*(as) + (old_left+x);
			cpos = (cur_top+y)*(bs) + (cur_left+x);
			m = (a[opos] > b[cpos]) ? a[opos] : b[cpos];
			(*last_tail)->bitmap[opos] = 0;
			(*tail)->bitmap[cpos] = m;
		}

	// Insert bitmaps into the cache
	nhk = calloc(1, sizeof(*nhk));
	memcpy(nhk, &hk, sizeof(*nhk));
	hv = calloc(1, sizeof(*hv));
	hv->a = (*last_tail)->bitmap;
	hv->b = (*tail)->bitmap;
	cache_add_composite(nhk, hv);
}

/**
 * \brief Convert text_info_t struct to ass_image_t list
 * Splits glyphs in halves when needed (for \kf karaoke).
 */
static ass_image_t* render_text(text_info_t* text_info, int dst_x, int dst_y)
{
	int pen_x, pen_y;
	int i;
	bitmap_t* bm;
	ass_image_t* head;
	ass_image_t** tail = &head;
	ass_image_t** last_tail = 0;
	ass_image_t** here_tail = 0;
	bitmap_hash_key_t* last_hash = 0;

	for (i = 0; i < text_info->length; ++i) {
		glyph_info_t* info = text_info->glyphs + i;
		if ((info->symbol == 0) || (info->symbol == '\n') || !info->bm_s || (info->shadow == 0))
			continue;

		pen_x = dst_x + info->pos.x + ROUND(info->shadow * frame_context.border_scale);
		pen_y = dst_y + info->pos.y + ROUND(info->shadow * frame_context.border_scale);
		bm = info->bm_s;

		here_tail = tail;
		tail = render_glyph(bm, pen_x, pen_y, info->c[3], 0, 1000000, tail);
		if (last_tail && tail != here_tail && ((info->c[3] & 0xff) > 0))
			render_overlap(last_tail, here_tail, last_hash, &info->hash_key);
		last_tail = here_tail;
		last_hash = &info->hash_key;
	}

	last_tail = 0;
	for (i = 0; i < text_info->length; ++i) {
		glyph_info_t* info = text_info->glyphs + i;
		if ((info->symbol == 0) || (info->symbol == '\n') || !info->bm_o)
			continue;

		pen_x = dst_x + info->pos.x;
		pen_y = dst_y + info->pos.y;
		bm = info->bm_o;
		
		if ((info->effect_type == EF_KARAOKE_KO) && (info->effect_timing <= info->bbox.xMax)) {
			// do nothing
		} else {
			here_tail = tail;
			tail = render_glyph(bm, pen_x, pen_y, info->c[2], 0, 1000000, tail);
			if (last_tail && tail != here_tail && ((info->c[2] & 0xff) > 0))
				render_overlap(last_tail, here_tail, last_hash, &info->hash_key);
			last_tail = here_tail;
			last_hash = &info->hash_key;
		}
	}
	for (i = 0; i < text_info->length; ++i) {
		glyph_info_t* info = text_info->glyphs + i;
		if ((info->symbol == 0) || (info->symbol == '\n') || !info->bm)
			continue;

		pen_x = dst_x + info->pos.x;
		pen_y = dst_y + info->pos.y;
		bm = info->bm;

		if ((info->effect_type == EF_KARAOKE) || (info->effect_type == EF_KARAOKE_KO)) {
			if (info->effect_timing > info->bbox.xMax)
				tail = render_glyph(bm, pen_x, pen_y, info->c[0], 0, 1000000, tail);
			else
				tail = render_glyph(bm, pen_x, pen_y, info->c[1], 0, 1000000, tail);
		} else if (info->effect_type == EF_KARAOKE_KF) {
			tail = render_glyph(bm, pen_x, pen_y, info->c[0], info->c[1], info->effect_timing, tail);
		} else
			tail = render_glyph(bm, pen_x, pen_y, info->c[0], 0, 1000000, tail);
	}

	*tail = 0;
	return head;
}

/**
 * \brief Mapping between script and screen coordinates
 */
static int x2scr(double x) {
	return x*frame_context.orig_width_nocrop / frame_context.track->PlayResX +
		FFMAX(global_settings->left_margin, 0);
}
static double x2scr_pos(double x) {
	return x*frame_context.orig_width / frame_context.track->PlayResX +
		global_settings->left_margin;
}
/**
 * \brief Mapping between script and screen coordinates
 */
static double y2scr(double y) {
	return y * frame_context.orig_height_nocrop / frame_context.track->PlayResY +
		FFMAX(global_settings->top_margin, 0);
}
static double y2scr_pos(double y) {
	return y * frame_context.orig_height / frame_context.track->PlayResY +
		global_settings->top_margin;
}

// the same for toptitles
static int y2scr_top(double y) {
	if (global_settings->use_margins)
		return y * frame_context.orig_height_nocrop / frame_context.track->PlayResY;
	else
		return y * frame_context.orig_height_nocrop / frame_context.track->PlayResY +
			FFMAX(global_settings->top_margin, 0);
}
// the same for subtitles
static int y2scr_sub(double y) {
	if (global_settings->use_margins)
		return y * frame_context.orig_height_nocrop / frame_context.track->PlayResY +
			FFMAX(global_settings->top_margin, 0) +
			FFMAX(global_settings->bottom_margin, 0);
	else
		return y * frame_context.orig_height_nocrop / frame_context.track->PlayResY +
			FFMAX(global_settings->top_margin, 0);
}

static void compute_string_bbox( text_info_t* info, FT_BBox *abbox ) {
	FT_BBox bbox;
	int i;
	
	if (text_info.length > 0) {
		bbox.xMin = 32000;
		bbox.xMax = -32000;
		bbox.yMin = - d6_to_int(text_info.lines[0].asc) + text_info.glyphs[0].pos.y;
		bbox.yMax = d6_to_int(text_info.height - text_info.lines[0].asc) + text_info.glyphs[0].pos.y;

		for (i = 0; i < text_info.length; ++i) {
			int s = text_info.glyphs[i].pos.x;
			int e = s + d6_to_int(text_info.glyphs[i].advance.x);
			bbox.xMin = FFMIN(bbox.xMin, s);
			bbox.xMax = FFMAX(bbox.xMax, e);
		}
	} else
		bbox.xMin = bbox.xMax = bbox.yMin = bbox.yMax = 0;

	/* return string bbox */
	*abbox = bbox;
}


/**
 * \brief Check if starting part of (*p) matches sample. If true, shift p to the first symbol after the matching part.
 */
static inline int mystrcmp(char** p, const char* sample) {
	int len = strlen(sample);
	if (strncmp(*p, sample, len) == 0) {
		(*p) += len;
		return 1;
	} else
		return 0;
}

static void change_font_size(double sz)
{
	double size = sz * frame_context.font_scale;

	if (size < 1)
		size = 1;
	else if (size > frame_context.height * 2)
		size = frame_context.height * 2;

	ass_font_set_size(render_context.font, size);

	render_context.font_size = sz;
}

/**
 * \brief Change current font, using setting from render_context.
 */
static void update_font(void)
{
	unsigned val;
	ass_renderer_t* priv = frame_context.ass_priv;
	ass_font_desc_t desc;
	desc.family = strdup(render_context.family);
	desc.treat_family_as_pattern = render_context.treat_family_as_pattern;

	val = render_context.bold;
	// 0 = normal, 1 = bold, >1 = exact weight
	if (val == 0) val = 80; // normal
	else if (val == 1) val = 200; // bold
	desc.bold = val;

	val = render_context.italic;
	if (val == 0) val = 0; // normal
	else if (val == 1) val = 110; //italic
	desc.italic = val;

	render_context.font = ass_font_new(priv->library, priv->ftlibrary, priv->fontconfig_priv, &desc);
	free(desc.family);
	
	if (render_context.font)
		change_font_size(render_context.font_size);
}

/**
 * \brief Change border width
 * negative value resets border to style value
 */
static void change_border(double border)
{
	int b;
	if (!render_context.font) return;

	if (border < 0) {
		if (render_context.style->BorderStyle == 1)
			border = render_context.style->Outline;
		else
			border = 1.;
	}
	render_context.border = border;

	b = 64 * border * frame_context.border_scale;
	if (b > 0) {
		if (!render_context.stroker) {
			int error;
#if (FREETYPE_MAJOR > 2) || ((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR > 1))
			error = FT_Stroker_New( ass_renderer->ftlibrary, &render_context.stroker );
#else // < 2.2
			error = FT_Stroker_New( render_context.font->faces[0]->memory, &render_context.stroker );
#endif
			if (error) {
				mp_msg(MSGT_ASS, MSGL_V, "failed to get stroker\n");
				render_context.stroker = 0;
			}
		}
		if (render_context.stroker)
			FT_Stroker_Set( render_context.stroker, b,
					FT_STROKER_LINECAP_ROUND,
					FT_STROKER_LINEJOIN_ROUND,
					0 );
	} else {
		FT_Stroker_Done(render_context.stroker);
		render_context.stroker = 0;
	}
}

#define _r(c)  ((c)>>24)
#define _g(c)  (((c)>>16)&0xFF)
#define _b(c)  (((c)>>8)&0xFF)
#define _a(c)  ((c)&0xFF)

/**
 * \brief Calculate a weighted average of two colors
 * calculates c1*(1-a) + c2*a, but separately for each component except alpha
 */
static void change_color(uint32_t* var, uint32_t new, double pwr)
{
	(*var)= ((uint32_t)(_r(*var) * (1 - pwr) + _r(new) * pwr) << 24) +
		((uint32_t)(_g(*var) * (1 - pwr) + _g(new) * pwr) << 16) +
		((uint32_t)(_b(*var) * (1 - pwr) + _b(new) * pwr) << 8) +
		_a(*var);
}

// like change_color, but for alpha component only
static void change_alpha(uint32_t* var, uint32_t new, double pwr)
{
	*var = (_r(*var) << 24) + (_g(*var) << 16) + (_b(*var) << 8) + (_a(*var) * (1 - pwr) + _a(new) * pwr);
}

/**
 * \brief Multiply two alpha values
 * \param a first value
 * \param b second value
 * \return result of multiplication
 * Parameters and result are limited by 0xFF.
 */
static uint32_t mult_alpha(uint32_t a, uint32_t b)
{
	return 0xFF - (0xFF - a) * (0xFF - b) / 0xFF;
}

/**
 * \brief Calculate alpha value by piecewise linear function
 * Used for \fad, \fade implementation.
 */
static unsigned interpolate_alpha(long long now, 
		long long t1, long long t2, long long t3, long long t4,
		unsigned a1, unsigned a2, unsigned a3)
{
	unsigned a;
	double cf;
	if (now <= t1) {
		a = a1;
	} else if (now >= t4) {
		a = a3;
	} else if (now < t2) { // and > t1
		cf = ((double)(now - t1)) / (t2 - t1);
		a = a1 * (1 - cf) + a2 * cf;
	} else if (now > t3) {
		cf = ((double)(now - t3)) / (t4 - t3);
		a = a2 * (1 - cf) + a3 * cf;
	} else { // t2 <= now <= t3
		a = a2;
	}

	return a;
}

static void reset_render_context(void);

/**
 * \brief Parse style override tag.
 * \param p string to parse
 * \param pwr multiplier for some tag effects (comes from \t tags)
 */
static char* parse_tag(char* p, double pwr) {
#define skip_to(x) while ((*p != (x)) && (*p != '}') && (*p != 0)) { ++p;}
#define skip(x) if (*p == (x)) ++p; else { return p; }
	
	skip_to('\\');
	skip('\\');
	if ((*p == '}') || (*p == 0))
		return p;

	// New tags introduced in vsfilter 2.39
	if (mystrcmp(&p, "xbord")) {
		double val;
		if (mystrtod(&p, &val))
			mp_msg(MSGT_ASS, MSGL_V, "stub: \\xbord%.2f\n", val);
	} else if (mystrcmp(&p, "ybord")) {
		double val;
		if (mystrtod(&p, &val))
			mp_msg(MSGT_ASS, MSGL_V, "stub: \\ybord%.2f\n", val);
	} else if (mystrcmp(&p, "xshad")) {
		int val;
		if (mystrtoi(&p, &val))
			mp_msg(MSGT_ASS, MSGL_V, "stub: \\xshad%d\n", val);
	} else if (mystrcmp(&p, "yshad")) {
		int val;
		if (mystrtoi(&p, &val))
			mp_msg(MSGT_ASS, MSGL_V, "stub: \\yshad%d\n", val);
	} else if (mystrcmp(&p, "fax")) {
		int val;
		if (mystrtoi(&p, &val))
			mp_msg(MSGT_ASS, MSGL_V, "stub: \\fax%d\n", val);
	} else if (mystrcmp(&p, "fay")) {
		int val;
		if (mystrtoi(&p, &val))
			mp_msg(MSGT_ASS, MSGL_V, "stub: \\fay%d\n", val);
	} else if (mystrcmp(&p, "iclip")) {
		int x0, y0, x1, y1;
		int res = 1;
		skip('(');
		res &= mystrtoi(&p, &x0);
		skip(',');
		res &= mystrtoi(&p, &y0);
		skip(',');
		res &= mystrtoi(&p, &x1);
		skip(',');
		res &= mystrtoi(&p, &y1);
		skip(')');
		mp_msg(MSGT_ASS, MSGL_V, "stub: \\iclip(%d,%d,%d,%d)\n", x0, y0, x1, y1);
	} else if (mystrcmp(&p, "blur")) {
		double val;
		if (mystrtod(&p, &val)) {
			val = (val < 0) ? 0 : val;
			val = (val > BLUR_MAX_RADIUS) ? BLUR_MAX_RADIUS : val;
			render_context.blur = val;
		} else
			render_context.blur = 0.0;
	// ASS standard tags
	} else if (mystrcmp(&p, "fsc")) {
		char tp = *p++;
		double val;
		if (tp == 'x') {
			if (mystrtod(&p, &val)) {
				val /= 100;
				render_context.scale_x = render_context.scale_x * ( 1 - pwr) + val * pwr;
			} else
				render_context.scale_x = render_context.style->ScaleX;
		} else if (tp == 'y') {
			if (mystrtod(&p, &val)) {
				val /= 100;
				render_context.scale_y = render_context.scale_y * ( 1 - pwr) + val * pwr;
			} else
				render_context.scale_y = render_context.style->ScaleY;
		}
	} else if (mystrcmp(&p, "fsp")) {
		double val;
		if (mystrtod(&p, &val))
			render_context.hspacing = render_context.hspacing * ( 1 - pwr ) + val * pwr;
		else
			render_context.hspacing = render_context.style->Spacing;
	} else if (mystrcmp(&p, "fs")) {
		double val;
		if (mystrtod(&p, &val))
			val = render_context.font_size * ( 1 - pwr ) + val * pwr;
		else
			val = render_context.style->FontSize;
		if (render_context.font)
			change_font_size(val);
	} else if (mystrcmp(&p, "bord")) {
		double val;
		if (mystrtod(&p, &val))
			val = render_context.border * ( 1 - pwr ) + val * pwr;
		else
			val = -1.; // reset to default
		change_border(val);
	} else if (mystrcmp(&p, "move")) {
		double x1, x2, y1, y2;
		long long t1, t2, delta_t, t;
		double x, y;
		double k;
		skip('(');
		mystrtod(&p, &x1);
		skip(',');
		mystrtod(&p, &y1);
		skip(',');
		mystrtod(&p, &x2);
		skip(',');
		mystrtod(&p, &y2);
		if (*p == ',') {
			skip(',');
			mystrtoll(&p, &t1);
			skip(',');
			mystrtoll(&p, &t2);
			mp_msg(MSGT_ASS, MSGL_DBG2, "movement6: (%f, %f) -> (%f, %f), (%" PRId64 " .. %" PRId64 ")\n", 
				x1, y1, x2, y2, (int64_t)t1, (int64_t)t2);
		} else {
			t1 = 0;
			t2 = render_context.event->Duration;
			mp_msg(MSGT_ASS, MSGL_DBG2, "movement: (%f, %f) -> (%f, %f)\n", x1, y1, x2, y2);
		}
		skip(')');
		delta_t = t2 - t1;
		t = frame_context.time - render_context.event->Start;
		if (t < t1)
			k = 0.;
		else if (t > t2)
			k = 1.;
		else k = ((double)(t - t1)) / delta_t;
		x = k * (x2 - x1) + x1;
		y = k * (y2 - y1) + y1;
		if (render_context.evt_type != EVENT_POSITIONED) {
			render_context.pos_x = x;
			render_context.pos_y = y;
			render_context.detect_collisions = 0;
			render_context.evt_type = EVENT_POSITIONED;
		}
	} else if (mystrcmp(&p, "frx")) {
		double val;
		if (mystrtod(&p, &val)) {
			val *= M_PI / 180;
			render_context.frx = val * pwr + render_context.frx * (1-pwr);
		} else
			render_context.frx = 0.;
	} else if (mystrcmp(&p, "fry")) {
		double val;
		if (mystrtod(&p, &val)) {
			val *= M_PI / 180;
			render_context.fry = val * pwr + render_context.fry * (1-pwr);
		} else
			render_context.fry = 0.;
	} else if (mystrcmp(&p, "frz") || mystrcmp(&p, "fr")) {
		double val;
		if (mystrtod(&p, &val)) {
			val *= M_PI / 180;
			render_context.frz = val * pwr + render_context.frz * (1-pwr);
		} else
			render_context.frz = M_PI * render_context.style->Angle / 180.;
	} else if (mystrcmp(&p, "fn")) {
		char* start = p;
		char* family;
		skip_to('\\');
		if (p > start) {
			family = malloc(p - start + 1);
			strncpy(family, start, p - start);
			family[p - start] = '\0';
		} else
			family = strdup(render_context.style->FontName);
		if (render_context.family)
			free(render_context.family);
		render_context.family = family;
		update_font();
	} else if (mystrcmp(&p, "alpha")) {
		uint32_t val;
		int i;
		if (strtocolor(&p, &val)) {
			unsigned char a = val >> 24;
			for (i = 0; i < 4; ++i)
				change_alpha(&render_context.c[i], a, pwr);
		} else {
			change_alpha(&render_context.c[0], render_context.style->PrimaryColour, pwr);
			change_alpha(&render_context.c[1], render_context.style->SecondaryColour, pwr);
			change_alpha(&render_context.c[2], render_context.style->OutlineColour, pwr);
			change_alpha(&render_context.c[3], render_context.style->BackColour, pwr);
		}
		// FIXME: simplify
	} else if (mystrcmp(&p, "an")) {
		int val;
		if (mystrtoi(&p, &val) && val) {
			int v = (val - 1) / 3; // 0, 1 or 2 for vertical alignment
			mp_msg(MSGT_ASS, MSGL_DBG2, "an %d\n", val);
			if (v != 0) v = 3 - v;
			val = ((val - 1) % 3) + 1; // horizontal alignment
			val += v*4;
			mp_msg(MSGT_ASS, MSGL_DBG2, "align %d\n", val);
			render_context.alignment = val;
		} else
			render_context.alignment = render_context.style->Alignment;
	} else if (mystrcmp(&p, "a")) {
		int val;
		if (mystrtoi(&p, &val) && val)
			render_context.alignment = val;
		else
			render_context.alignment = render_context.style->Alignment;
	} else if (mystrcmp(&p, "pos")) {
		double v1, v2;
		skip('(');
		mystrtod(&p, &v1);
		skip(',');
		mystrtod(&p, &v2);
		skip(')');
		mp_msg(MSGT_ASS, MSGL_DBG2, "pos(%f, %f)\n", v1, v2);
		if (render_context.evt_type != EVENT_POSITIONED) {
			render_context.evt_type = EVENT_POSITIONED;
			render_context.detect_collisions = 0;
			render_context.pos_x = v1;
			render_context.pos_y = v2;
		}
	} else if (mystrcmp(&p, "fad")) {
		int a1, a2, a3;
		long long t1, t2, t3, t4;
		if (*p == 'e') ++p; // either \fad or \fade
		skip('(');
		mystrtoi(&p, &a1);
		skip(',');
		mystrtoi(&p, &a2);
		if (*p == ')') {
			// 2-argument version (\fad, according to specs)
			// a1 and a2 are fade-in and fade-out durations
			t1 = 0;
			t4 = render_context.event->Duration;
			t2 = a1;
			t3 = t4 - a2;
			a1 = 0xFF;
			a2 = 0;
			a3 = 0xFF;
		} else {
			// 6-argument version (\fade)
			// a1 and a2 (and a3) are opacity values
			skip(',');
			mystrtoi(&p, &a3);
			skip(',');
			mystrtoll(&p, &t1);
			skip(',');
			mystrtoll(&p, &t2);
			skip(',');
			mystrtoll(&p, &t3);
			skip(',');
			mystrtoll(&p, &t4);
		}
		skip(')');
		render_context.fade = interpolate_alpha(frame_context.time - render_context.event->Start, t1, t2, t3, t4, a1, a2, a3);
	} else if (mystrcmp(&p, "org")) {
		int v1, v2;
		skip('(');
		mystrtoi(&p, &v1);
		skip(',');
		mystrtoi(&p, &v2);
		skip(')');
		mp_msg(MSGT_ASS, MSGL_DBG2, "org(%d, %d)\n", v1, v2);
		//				render_context.evt_type = EVENT_POSITIONED;
		if (!render_context.have_origin) {
			render_context.org_x = v1;
			render_context.org_y = v2;
			render_context.have_origin = 1;
			render_context.detect_collisions = 0;
		}
	} else if (mystrcmp(&p, "t")) {
		double v[3];
		int v1, v2;
		double v3;
		int cnt;
		long long t1, t2, t, delta_t;
		double k;
		skip('(');
		for (cnt = 0; cnt < 3; ++cnt) {
			if (*p == '\\')
				break;
			v[cnt] = strtod(p, &p);
			skip(',');
		}
		if (cnt == 3) {
			v1 = v[0]; v2 = v[1]; v3 = v[2];
		} else if (cnt == 2) {
			v1 = v[0]; v2 = v[1]; v3 = 1.;
		} else if (cnt == 1) {
			v1 = 0; v2 = render_context.event->Duration; v3 = v[0];
		} else { // cnt == 0
			v1 = 0; v2 = render_context.event->Duration; v3 = 1.;
		}
		render_context.detect_collisions = 0;
		t1 = v1;
		t2 = v2;
		delta_t = v2 - v1;
		if (v3 < 0.)
			v3 = 0.;
		t = frame_context.time - render_context.event->Start; // FIXME: move to render_context
		if (t <= t1)
			k = 0.;
		else if (t >= t2)
			k = 1.;
		else {
			assert(delta_t != 0.);
			k = pow(((double)(t - t1)) / delta_t, v3);
		}
		while (*p == '\\')
			p = parse_tag(p, k); // maybe k*pwr ? no, specs forbid nested \t's 
		skip_to(')'); // in case there is some unknown tag or a comment
		skip(')');
	} else if (mystrcmp(&p, "clip")) {
		int x0, y0, x1, y1;
		int res = 1;
		skip('(');
		res &= mystrtoi(&p, &x0);
		skip(',');
		res &= mystrtoi(&p, &y0);
		skip(',');
		res &= mystrtoi(&p, &x1);
		skip(',');
		res &= mystrtoi(&p, &y1);
		skip(')');
		if (res) {
			render_context.clip_x0 = render_context.clip_x0 * (1-pwr) + x0 * pwr;
			render_context.clip_x1 = render_context.clip_x1 * (1-pwr) + x1 * pwr;
			render_context.clip_y0 = render_context.clip_y0 * (1-pwr) + y0 * pwr;
			render_context.clip_y1 = render_context.clip_y1 * (1-pwr) + y1 * pwr;
		} else {
			render_context.clip_x0 = 0;
			render_context.clip_y0 = 0;
			render_context.clip_x1 = frame_context.track->PlayResX;
			render_context.clip_y1 = frame_context.track->PlayResY;
		}
	} else if (mystrcmp(&p, "c")) {
		uint32_t val;
		if (!strtocolor(&p, &val))
			val = render_context.style->PrimaryColour;
		mp_msg(MSGT_ASS, MSGL_DBG2, "color: %X\n", val);
		change_color(&render_context.c[0], val, pwr);
	} else if ((*p >= '1') && (*p <= '4') && (++p) && (mystrcmp(&p, "c") || mystrcmp(&p, "a"))) {
		char n = *(p-2);
		int cidx = n - '1';
		char cmd = *(p-1);
		uint32_t val;
		assert((n >= '1') && (n <= '4'));
		if (!strtocolor(&p, &val))
			switch(n) {
				case '1': val = render_context.style->PrimaryColour; break;
				case '2': val = render_context.style->SecondaryColour; break;
				case '3': val = render_context.style->OutlineColour; break;
				case '4': val = render_context.style->BackColour; break;
				default : val = 0; break; // impossible due to assert; avoid compilation warning
			}
		switch (cmd) {
			case 'c': change_color(render_context.c + cidx, val, pwr); break;
			case 'a': change_alpha(render_context.c + cidx, val >> 24, pwr); break;
			default: mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_BadCommand, n, cmd); break;
		}
		mp_msg(MSGT_ASS, MSGL_DBG2, "single c/a at %f: %c%c = %X   \n", pwr, n, cmd, render_context.c[cidx]);
	} else if (mystrcmp(&p, "r")) {
		reset_render_context();
	} else if (mystrcmp(&p, "be")) {
		int val;
		if (mystrtoi(&p, &val)) {
			// Clamp to a safe upper limit, since high values need excessive CPU
			val = (val < 0) ? 0 : val;
			val = (val > MAX_BE) ? MAX_BE : val;
			render_context.be = val;
		} else
			render_context.be = 0;
	} else if (mystrcmp(&p, "b")) {
		int b;
		if (mystrtoi(&p, &b)) {
			if (pwr >= .5)
				render_context.bold = b;
		} else
			render_context.bold = render_context.style->Bold;
		update_font();
	} else if (mystrcmp(&p, "i")) {
		int i;
		if (mystrtoi(&p, &i)) {
			if (pwr >= .5)
				render_context.italic = i;
		} else
			render_context.italic = render_context.style->Italic;
		update_font();
	} else if (mystrcmp(&p, "kf") || mystrcmp(&p, "K")) {
		int val = 0;
		mystrtoi(&p, &val);
		render_context.effect_type = EF_KARAOKE_KF;
		if (render_context.effect_timing)
			render_context.effect_skip_timing += render_context.effect_timing;
		render_context.effect_timing = val * 10;
	} else if (mystrcmp(&p, "ko")) {
		int val = 0;
		mystrtoi(&p, &val);
		render_context.effect_type = EF_KARAOKE_KO;
		if (render_context.effect_timing)
			render_context.effect_skip_timing += render_context.effect_timing;
		render_context.effect_timing = val * 10;
	} else if (mystrcmp(&p, "k")) {
		int val = 0;
		mystrtoi(&p, &val);
		render_context.effect_type = EF_KARAOKE;
		if (render_context.effect_timing)
			render_context.effect_skip_timing += render_context.effect_timing;
		render_context.effect_timing = val * 10;
	} else if (mystrcmp(&p, "shad")) {
		int val;
		if (mystrtoi(&p, &val))
			render_context.shadow = val;
		else
			render_context.shadow = render_context.style->Shadow;
	} else if (mystrcmp(&p, "pbo")) {
		int val = 0;
		mystrtoi(&p, &val); // ignored
	} else if (mystrcmp(&p, "p")) {
		int val;
		if (!mystrtoi(&p, &val))
			val = 0;
		render_context.drawing_mode = !!val;
	}

	return p;

#undef skip
#undef skip_to
}

/**
 * \brief Get next ucs4 char from string, parsing and executing style overrides
 * \param str string pointer
 * \return ucs4 code of the next char
 * On return str points to the unparsed part of the string
 */
static unsigned get_next_char(char** str)
{
	char* p = *str;
	unsigned chr;
	if (*p == '{') { // '\0' goes here
		p++;
		while (1) {
			p = parse_tag(p, 1.);
			if (*p == '}') { // end of tag
				p++;
				if (*p == '{') {
					p++;
					continue;
				} else
					break;
			} else if (*p != '\\')
				mp_msg(MSGT_ASS, MSGL_V, "Unable to parse: \"%s\" \n", p);
			if (*p == 0)
				break;
		}
	}
	if (*p == '\t') {
		++p;
		*str = p;
		return ' ';
	}
	if (*p == '\\') {
		if ((*(p+1) == 'N') || ((*(p+1) == 'n') && (frame_context.track->WrapStyle == 2))) {
			p += 2;
			*str = p;
			return '\n';
		} else if ((*(p+1) == 'n') || (*(p+1) == 'h')) {
			p += 2;
			*str = p;
			return ' ';
		}
	}
	chr = utf8_get_char((const char **)&p);
	*str = p;
	return chr;
}

static void apply_transition_effects(ass_event_t* event)
{
	int v[4];
	int cnt;
	char* p = event->Effect;

	if (!p || !*p) return;

	cnt = 0;
	while (cnt < 4 && (p = strchr(p, ';'))) {
		v[cnt++] = atoi(++p);
	}
	
	if (strncmp(event->Effect, "Banner;", 7) == 0) {
		int delay;
		if (cnt < 1) {
			mp_msg(MSGT_ASS, MSGL_V, "Error parsing effect: %s \n", event->Effect);
			return;
		}
		if (cnt >= 2 && v[1] == 0) // right-to-left
			render_context.scroll_direction = SCROLL_RL;
		else // left-to-right
			render_context.scroll_direction = SCROLL_LR;

		delay = v[0];
		if (delay == 0) delay = 1; // ?
		render_context.scroll_shift = (frame_context.time - render_context.event->Start) / delay;
		render_context.evt_type = EVENT_HSCROLL;
		return;
	}

	if (strncmp(event->Effect, "Scroll up;", 10) == 0) {
		render_context.scroll_direction = SCROLL_BT;
	} else if (strncmp(event->Effect, "Scroll down;", 12) == 0) {
		render_context.scroll_direction = SCROLL_TB;
	} else {
		mp_msg(MSGT_ASS, MSGL_V, "Unknown transition effect: %s \n", event->Effect);
		return;
	}
	// parse scroll up/down parameters
	{
		int delay;
		int y0, y1;
		if (cnt < 3) {
			mp_msg(MSGT_ASS, MSGL_V, "Error parsing effect: %s \n", event->Effect);
			return;
		}
		delay = v[2];
		if (delay == 0) delay = 1; // ?
		render_context.scroll_shift = (frame_context.time - render_context.event->Start) / delay;
		if (v[0] < v[1]) {
			y0 = v[0]; y1 = v[1];
		} else {
			y0 = v[1]; y1 = v[0];
		}
		if (y1 == 0)
			y1 = frame_context.track->PlayResY; // y0=y1=0 means fullscreen scrolling
		render_context.clip_y0 = y0;
		render_context.clip_y1 = y1;
		render_context.evt_type = EVENT_VSCROLL;
		render_context.detect_collisions = 0;
	}

}

/**
 * \brief partially reset render_context to style values
 * Works like {\r}: resets some style overrides
 */
static void reset_render_context(void)
{
	render_context.c[0] = render_context.style->PrimaryColour;
	render_context.c[1] = render_context.style->SecondaryColour;
	render_context.c[2] = render_context.style->OutlineColour;
	render_context.c[3] = render_context.style->BackColour;
	render_context.font_size = render_context.style->FontSize;

	if (render_context.family)
		free(render_context.family);
	render_context.family = strdup(render_context.style->FontName);
	render_context.treat_family_as_pattern = render_context.style->treat_fontname_as_pattern;
	render_context.bold = render_context.style->Bold;
	render_context.italic = render_context.style->Italic;
	update_font();

	change_border(-1.);
	render_context.scale_x = render_context.style->ScaleX;
	render_context.scale_y = render_context.style->ScaleY;
	render_context.hspacing = render_context.style->Spacing;
	render_context.be = 0;
	render_context.blur = 0.0;
	render_context.shadow = render_context.style->Shadow;
	render_context.frx = render_context.fry = 0.;
	render_context.frz = M_PI * render_context.style->Angle / 180.;

	// FIXME: does not reset unsupported attributes.
}

/**
 * \brief Start new event. Reset render_context.
 */
static void init_render_context(ass_event_t* event)
{
	render_context.event = event;
	render_context.style = frame_context.track->styles + event->Style;

	reset_render_context();

	render_context.evt_type = EVENT_NORMAL;
	render_context.alignment = render_context.style->Alignment;
	render_context.pos_x = 0;
	render_context.pos_y = 0;
	render_context.org_x = 0;
	render_context.org_y = 0;
	render_context.have_origin = 0;
	render_context.clip_x0 = 0;
	render_context.clip_y0 = 0;
	render_context.clip_x1 = frame_context.track->PlayResX;
	render_context.clip_y1 = frame_context.track->PlayResY;
	render_context.detect_collisions = 1;
	render_context.fade = 0;
	render_context.drawing_mode = 0;
	render_context.effect_type = EF_NONE;
	render_context.effect_timing = 0;
	render_context.effect_skip_timing = 0;
	
	apply_transition_effects(event);
}

static void free_render_context(void)
{
}

/**
 * \brief Get normal and outline (border) glyphs
 * \param symbol ucs4 char
 * \param info out: struct filled with extracted data
 * \param advance subpixel shift vector used for cache lookup
 * Tries to get both glyphs from cache.
 * If they can't be found, gets a glyph from font face, generates outline with FT_Stroker,
 * and add them to cache.
 * The glyphs are returned in info->glyph and info->outline_glyph
 */
static void get_outline_glyph(int symbol, glyph_info_t* info, FT_Vector* advance)
{
	int error;
	glyph_hash_val_t* val;
	glyph_hash_key_t key;
	key.font = render_context.font;
	key.size = render_context.font_size;
	key.ch = symbol;
	key.scale_x = (render_context.scale_x * 0xFFFF);
	key.scale_y = (render_context.scale_y * 0xFFFF);
	key.advance = *advance;
	key.bold = render_context.bold;
	key.italic = render_context.italic;
	key.outline = render_context.border * 0xFFFF;

	memset(info, 0, sizeof(glyph_info_t));

	val = cache_find_glyph(&key);
	if (val) {
		FT_Glyph_Copy(val->glyph, &info->glyph);
		if (val->outline_glyph)
			FT_Glyph_Copy(val->outline_glyph, &info->outline_glyph);
		info->bbox = val->bbox_scaled;
		info->advance.x = val->advance.x;
		info->advance.y = val->advance.y;
	} else {
		glyph_hash_val_t v;
		info->glyph = ass_font_get_glyph(frame_context.ass_priv->fontconfig_priv, render_context.font, symbol, global_settings->hinting);
		if (!info->glyph)
			return;
		info->advance.x = d16_to_d6(info->glyph->advance.x);
		info->advance.y = d16_to_d6(info->glyph->advance.y);
		FT_Glyph_Get_CBox( info->glyph, FT_GLYPH_BBOX_PIXELS, &info->bbox);

		if (render_context.stroker) {
			info->outline_glyph = info->glyph;
			error = FT_Glyph_StrokeBorder( &(info->outline_glyph), render_context.stroker, 0 , 0 ); // don't destroy original
			if (error) {
				mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FT_Glyph_Stroke_Error, error);
			}
		}

		memset(&v, 0, sizeof(v));
		FT_Glyph_Copy(info->glyph, &v.glyph);
		if (info->outline_glyph)
			FT_Glyph_Copy(info->outline_glyph, &v.outline_glyph);
		v.advance = info->advance;
		v.bbox_scaled = info->bbox;
		cache_add_glyph(&key, &v);
	}
}

static void transform_3d(FT_Vector shift, FT_Glyph* glyph, FT_Glyph* glyph2, double frx, double fry, double frz);

/**
 * \brief Get bitmaps for a glyph
 * \param info glyph info
 * Tries to get glyph bitmaps from bitmap cache.
 * If they can't be found, they are generated by rotating and rendering the glyph.
 * After that, bitmaps are added to the cache.
 * They are returned in info->bm (glyph), info->bm_o (outline) and info->bm_s (shadow).
 */
static void get_bitmap_glyph(glyph_info_t* info)
{
	bitmap_hash_val_t* val;
	bitmap_hash_key_t* key = &info->hash_key;
	
	val = cache_find_bitmap(key);
/* 	val = 0; */
	
	if (val) {
		info->bm = val->bm;
		info->bm_o = val->bm_o;
		info->bm_s = val->bm_s;
	} else {
		FT_Vector shift;
		bitmap_hash_val_t hash_val;
		int error;
		info->bm = info->bm_o = info->bm_s = 0;
		if (info->glyph && info->symbol != '\n' && info->symbol != 0) {
			// calculating rotation shift vector (from rotation origin to the glyph basepoint)
			shift.x = int_to_d6(info->hash_key.shift_x);
			shift.y = int_to_d6(info->hash_key.shift_y);
			// apply rotation
			transform_3d(shift, &info->glyph, &info->outline_glyph, info->frx, info->fry, info->frz);

			// render glyph
			error = glyph_to_bitmap(ass_renderer->synth_priv,
					info->glyph, info->outline_glyph,
					&info->bm, &info->bm_o,
					&info->bm_s, info->be, info->blur * frame_context.border_scale);
			if (error)
				info->symbol = 0;

			// add bitmaps to cache
			hash_val.bm_o = info->bm_o;
			hash_val.bm = info->bm;
			hash_val.bm_s = info->bm_s;
			cache_add_bitmap(&(info->hash_key), &hash_val);
		}
	}
	// deallocate glyphs
	if (info->glyph)
		FT_Done_Glyph(info->glyph);
	if (info->outline_glyph)
		FT_Done_Glyph(info->outline_glyph);
}

/**
 * This function goes through text_info and calculates text parameters.
 * The following text_info fields are filled:
 *   height
 *   lines[].height
 *   lines[].asc
 *   lines[].desc
 */
static void measure_text(void)
{
	int cur_line = 0, max_asc = 0, max_desc = 0;
	int i;
	text_info.height = 0;
	for (i = 0; i < text_info.length + 1; ++i) {
		if ((i == text_info.length) || text_info.glyphs[i].linebreak) {
			text_info.lines[cur_line].asc = max_asc;
			text_info.lines[cur_line].desc = max_desc;
			text_info.height += max_asc + max_desc;
			cur_line ++;
			max_asc = max_desc = 0;
		}
		if (i < text_info.length) {
			glyph_info_t* cur = text_info.glyphs + i;
			if (cur->asc > max_asc)
				max_asc = cur->asc;
			if (cur->desc > max_desc)
				max_desc = cur->desc;
		}
	}
	text_info.height += (text_info.n_lines - 1) * double_to_d6(global_settings->line_spacing);
}

/**
 * \brief rearrange text between lines
 * \param max_text_width maximal text line width in pixels
 * The algo is similar to the one in libvo/sub.c:
 * 1. Place text, wrapping it when current line is full
 * 2. Try moving words from the end of a line to the beginning of the next one while it reduces
 * the difference in lengths between this two lines.
 * The result may not be optimal, but usually is good enough.
 */
static void wrap_lines_smart(int max_text_width)
{
	int i, j;
	glyph_info_t *cur, *s1, *e1, *s2, *s3, *w;
	int last_space;
	int break_type;
	int exit;
	int pen_shift_x;
	int pen_shift_y;
	int cur_line;

	last_space = -1;
	text_info.n_lines = 1;
	break_type = 0;
	s1 = text_info.glyphs; // current line start
	for (i = 0; i < text_info.length; ++i) {
		int break_at, s_offset, len;
		cur = text_info.glyphs + i;
		break_at = -1;
		s_offset = s1->bbox.xMin + s1->pos.x;
		len = (cur->bbox.xMax + cur->pos.x) - s_offset;

		if (cur->symbol == '\n') {
			break_type = 2;
			break_at = i;
			mp_msg(MSGT_ASS, MSGL_DBG2, "forced line break at %d\n", break_at);
		}
		
		if (len >= max_text_width) {
			break_type = 1;
			break_at = last_space;
			if (break_at == -1)
				break_at = i - 1;
			if (break_at == -1)
				break_at = 0;
			mp_msg(MSGT_ASS, MSGL_DBG2, "overfill at %d\n", i);
			mp_msg(MSGT_ASS, MSGL_DBG2, "line break at %d\n", break_at);
		}

		if (break_at != -1) {
			// need to use one more line
			// marking break_at+1 as start of a new line
			int lead = break_at + 1; // the first symbol of the new line
			if (text_info.n_lines >= MAX_LINES) {
				// to many lines ! 
				// no more linebreaks
				for (j = lead; j < text_info.length; ++j)
					text_info.glyphs[j].linebreak = 0;
				break;
			}
			if (lead < text_info.length)
				text_info.glyphs[lead].linebreak = break_type;
			last_space = -1;
			s1 = text_info.glyphs + lead;
			s_offset = s1->bbox.xMin + s1->pos.x;
			text_info.n_lines ++;
		}
		
		if (cur->symbol == ' ')
			last_space = i;

		// make sure the hard linebreak is not forgotten when
		// there was a new soft linebreak just inserted
		if (cur->symbol == '\n' && break_type == 1)
			i--;
	}
#define DIFF(x,y) (((x) < (y)) ? (y - x) : (x - y))
	exit = 0;
	while (!exit) {
		exit = 1;
		w = s3 = text_info.glyphs;
		s1 = s2 = 0;
		for (i = 0; i <= text_info.length; ++i) {
			cur = text_info.glyphs + i;
			if ((i == text_info.length) || cur->linebreak) {
				s1 = s2;
				s2 = s3;
				s3 = cur;
				if (s1 && (s2->linebreak == 1)) { // have at least 2 lines, and linebreak is 'soft'
					int l1, l2, l1_new, l2_new;

					w = s2;
					do { --w; } while ((w > s1) && (w->symbol == ' '));
					while ((w > s1) && (w->symbol != ' ')) { --w; }
					e1 = w;
					while ((e1 > s1) && (e1->symbol == ' ')) { --e1; }
					if (w->symbol == ' ') ++w;

					l1 = ((s2-1)->bbox.xMax + (s2-1)->pos.x) - (s1->bbox.xMin + s1->pos.x);
					l2 = ((s3-1)->bbox.xMax + (s3-1)->pos.x) - (s2->bbox.xMin + s2->pos.x);
					l1_new = (e1->bbox.xMax + e1->pos.x) - (s1->bbox.xMin + s1->pos.x);
					l2_new = ((s3-1)->bbox.xMax + (s3-1)->pos.x) - (w->bbox.xMin + w->pos.x);

					if (DIFF(l1_new, l2_new) < DIFF(l1, l2)) {
						w->linebreak = 1;
						s2->linebreak = 0;
						exit = 0;
					}
				}
			}
			if (i == text_info.length)
				break;
		}
		
	}
	assert(text_info.n_lines >= 1);
#undef DIFF
	
	measure_text();

	pen_shift_x = 0;
	pen_shift_y = 0;
	cur_line = 1;
	for (i = 0; i < text_info.length; ++i) {
		cur = text_info.glyphs + i;
		if (cur->linebreak) {
			int height = text_info.lines[cur_line - 1].desc + text_info.lines[cur_line].asc;
			cur_line ++;
			pen_shift_x = - cur->pos.x;
			pen_shift_y += d6_to_int(height + double_to_d6(global_settings->line_spacing));
			mp_msg(MSGT_ASS, MSGL_DBG2, "shifting from %d to %d by (%d, %d)\n", i, text_info.length - 1, pen_shift_x, pen_shift_y);
		}
		cur->pos.x += pen_shift_x;
		cur->pos.y += pen_shift_y;
	}
}

/**
 * \brief determine karaoke effects
 * Karaoke effects cannot be calculated during parse stage (get_next_char()),
 * so they are done in a separate step.
 * Parse stage: when karaoke style override is found, its parameters are stored in the next glyph's 
 * (the first glyph of the karaoke word)'s effect_type and effect_timing.
 * This function:
 * 1. sets effect_type for all glyphs in the word (_karaoke_ word)
 * 2. sets effect_timing for all glyphs to x coordinate of the border line between the left and right karaoke parts
 * (left part is filled with PrimaryColour, right one - with SecondaryColour).
 */
static void process_karaoke_effects(void)
{
	glyph_info_t *cur, *cur2;
	glyph_info_t *s1, *e1; // start and end of the current word
	glyph_info_t *s2; // start of the next word
	int i;
	int timing; // current timing
	int tm_start, tm_end; // timings at start and end of the current word
	int tm_current;
	double dt;
	int x;
	int x_start, x_end;

	tm_current = frame_context.time - render_context.event->Start;
	timing = 0;
	s1 = s2 = 0;
	for (i = 0; i <= text_info.length; ++i) {
		cur = text_info.glyphs + i;
		if ((i == text_info.length) || (cur->effect_type != EF_NONE)) {
			s1 = s2;
			s2 = cur;
			if (s1) {
				e1 = s2 - 1;
				tm_start = timing + s1->effect_skip_timing;
				tm_end = tm_start + s1->effect_timing;
				timing = tm_end;
				x_start = 1000000;
				x_end = -1000000;
				for (cur2 = s1; cur2 <= e1; ++cur2) {
					x_start = FFMIN(x_start, cur2->bbox.xMin + cur2->pos.x);
					x_end = FFMAX(x_end, cur2->bbox.xMax + cur2->pos.x);
				}

				dt = (tm_current - tm_start);
				if ((s1->effect_type == EF_KARAOKE) || (s1->effect_type == EF_KARAOKE_KO)) {
					if (dt > 0)
						x = x_end + 1;
					else
						x = x_start;
				} else if (s1->effect_type == EF_KARAOKE_KF) {
					dt /= (tm_end - tm_start);
					x = x_start + (x_end - x_start) * dt;
				} else {
					mp_msg(MSGT_ASS, MSGL_ERR, MSGTR_LIBASS_UnknownEffectType_InternalError);
					continue;
				}

				for (cur2 = s1; cur2 <= e1; ++cur2) {
					cur2->effect_type = s1->effect_type;
					cur2->effect_timing = x - cur2->pos.x;
				}
			}
		}
	}
}

/**
 * \brief Calculate base point for positioning and rotation
 * \param bbox text bbox
 * \param alignment alignment
 * \param bx, by out: base point coordinates
 */
static void get_base_point(FT_BBox bbox, int alignment, int* bx, int* by)
{
	const int halign = alignment & 3;
	const int valign = alignment & 12;
	if (bx)
		switch(halign) {
		case HALIGN_LEFT:
			*bx = bbox.xMin;
			break;
		case HALIGN_CENTER:
			*bx = (bbox.xMax + bbox.xMin) / 2;
			break;
		case HALIGN_RIGHT:
			*bx = bbox.xMax;
			break;
		}
	if (by)
		switch(valign) {
		case VALIGN_TOP:
			*by = bbox.yMin;
			break;
		case VALIGN_CENTER:
			*by = (bbox.yMax + bbox.yMin) / 2;
			break;
		case VALIGN_SUB:
			*by = bbox.yMax;
			break;
		}
}

/**
 * \brief Apply transformation to outline points of a glyph
 * Applies rotations given by frx, fry and frz and projects the points back
 * onto the screen plane.
 */
static void transform_3d_points(FT_Vector shift, FT_Glyph glyph, double frx, double fry, double frz) {
	double sx = sin(frx);
	double sy = sin(fry);
	double sz = sin(frz);
	double cx = cos(frx);
	double cy = cos(fry);
	double cz = cos(frz);
	FT_Outline *outline = &((FT_OutlineGlyph) glyph)->outline;
	FT_Vector* p = outline->points;
	double x, y, z, xx, yy, zz;
	int i;

	for (i=0; i<outline->n_points; i++) {
		x = p[i].x + shift.x;
		y = p[i].y + shift.y;
		z = 0.;

		xx = x*cz + y*sz;
		yy = -(x*sz - y*cz);
		zz = z;

		x = xx;
		y = yy*cx + zz*sx;
		z = yy*sx - zz*cx;

		xx = x*cy + z*sy;
		yy = y;
		zz = x*sy - z*cy;

		zz = FFMAX(zz, -19000);

		x = (xx * 20000) / (zz + 20000);
		y = (yy * 20000) / (zz + 20000);
		p[i].x = x - shift.x + 0.5;
		p[i].y = y - shift.y + 0.5;
	}
}

/**
 * \brief Apply 3d transformation to several objects
 * \param shift FreeType vector
 * \param glyph FreeType glyph
 * \param glyph2 FreeType glyph
 * \param frx x-axis rotation angle
 * \param fry y-axis rotation angle
 * \param frz z-axis rotation angle
 * Rotates both glyphs by frx, fry and frz. Shift vector is added before rotation and subtracted after it.
 */
static void transform_3d(FT_Vector shift, FT_Glyph* glyph, FT_Glyph* glyph2, double frx, double fry, double frz)
{
	frx = - frx;
	frz = - frz;
	if (frx != 0. || fry != 0. || frz != 0.) {
		if (glyph && *glyph)
			transform_3d_points(shift, *glyph, frx, fry, frz);

		if (glyph2 && *glyph2)
			transform_3d_points(shift, *glyph2, frx, fry, frz);
	}
}


/**
 * \brief Main ass rendering function, glues everything together
 * \param event event to render
 * Process event, appending resulting ass_image_t's to images_root.
 */
static int ass_render_event(ass_event_t* event, event_images_t* event_images)
{
	char* p;
	FT_UInt previous; 
	FT_UInt num_glyphs;
	FT_Vector pen;
	unsigned code;
	FT_BBox bbox;
	int i, j;
	FT_Vector shift;
	int MarginL, MarginR, MarginV;
	int last_break;
	int alignment, halign, valign;
	int device_x = 0, device_y = 0;

	if (event->Style >= frame_context.track->n_styles) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_NoStyleFound);
		return 1;
	}
	if (!event->Text) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_EmptyEvent);
		return 1;
	}

	init_render_context(event);

	text_info.length = 0;
	pen.x = 0;
	pen.y = 0;
	previous = 0;
	num_glyphs = 0;
	p = event->Text;
	// Event parsing.
	while (1) {
		// get next char, executing style override
		// this affects render_context
		do {
			code = get_next_char(&p);
		} while (code && render_context.drawing_mode); // skip everything in drawing mode
		
		// face could have been changed in get_next_char
		if (!render_context.font) {
			free_render_context();
			return 1;
		}

		if (code == 0)
			break;

		if (text_info.length >= MAX_GLYPHS) {
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_MAX_GLYPHS_Reached, 
					(int)(event - frame_context.track->events), event->Start, event->Duration, event->Text);
			break;
		}

		if ( previous && code ) {
			FT_Vector delta;
			delta = ass_font_get_kerning(render_context.font, previous, code);
			pen.x += delta.x * render_context.scale_x;
			pen.y += delta.y * render_context.scale_y;
		}

		shift.x = pen.x & SUBPIXEL_MASK;
		shift.y = pen.y & SUBPIXEL_MASK;

		if (render_context.evt_type == EVENT_POSITIONED) {
			shift.x += double_to_d6(x2scr_pos(render_context.pos_x)) & SUBPIXEL_MASK;
			shift.y -= double_to_d6(y2scr_pos(render_context.pos_y)) & SUBPIXEL_MASK;
		}

		ass_font_set_transform(render_context.font,
				       render_context.scale_x * frame_context.font_scale_x,
				       render_context.scale_y,
				       &shift );

		get_outline_glyph(code, text_info.glyphs + text_info.length, &shift);
		
		text_info.glyphs[text_info.length].pos.x = pen.x >> 6;
		text_info.glyphs[text_info.length].pos.y = pen.y >> 6;
		
		pen.x += text_info.glyphs[text_info.length].advance.x;
		pen.x += double_to_d6(render_context.hspacing);
		pen.y += text_info.glyphs[text_info.length].advance.y;
		
		previous = code;

		text_info.glyphs[text_info.length].symbol = code;
		text_info.glyphs[text_info.length].linebreak = 0;
		for (i = 0; i < 4; ++i) {
			uint32_t clr = render_context.c[i];
			change_alpha(&clr, mult_alpha(_a(clr), render_context.fade), 1.);
			text_info.glyphs[text_info.length].c[i] = clr;
		}
		text_info.glyphs[text_info.length].effect_type = render_context.effect_type;
		text_info.glyphs[text_info.length].effect_timing = render_context.effect_timing;
		text_info.glyphs[text_info.length].effect_skip_timing = render_context.effect_skip_timing;
		text_info.glyphs[text_info.length].be = render_context.be;
		text_info.glyphs[text_info.length].blur = render_context.blur;
		text_info.glyphs[text_info.length].shadow = render_context.shadow;
		text_info.glyphs[text_info.length].frx = render_context.frx;
		text_info.glyphs[text_info.length].fry = render_context.fry;
		text_info.glyphs[text_info.length].frz = render_context.frz;
		ass_font_get_asc_desc(render_context.font, code,
				      &text_info.glyphs[text_info.length].asc,
				      &text_info.glyphs[text_info.length].desc);
		text_info.glyphs[text_info.length].asc *= render_context.scale_y;
		text_info.glyphs[text_info.length].desc *= render_context.scale_y;

		// fill bitmap_hash_key
		text_info.glyphs[text_info.length].hash_key.font = render_context.font;
		text_info.glyphs[text_info.length].hash_key.size = render_context.font_size;
		text_info.glyphs[text_info.length].hash_key.outline = render_context.border * 0xFFFF;
		text_info.glyphs[text_info.length].hash_key.scale_x = render_context.scale_x * 0xFFFF;
		text_info.glyphs[text_info.length].hash_key.scale_y = render_context.scale_y * 0xFFFF;
		text_info.glyphs[text_info.length].hash_key.frx = render_context.frx * 0xFFFF;
		text_info.glyphs[text_info.length].hash_key.fry = render_context.fry * 0xFFFF;
		text_info.glyphs[text_info.length].hash_key.frz = render_context.frz * 0xFFFF;
		text_info.glyphs[text_info.length].hash_key.bold = render_context.bold;
		text_info.glyphs[text_info.length].hash_key.italic = render_context.italic;
		text_info.glyphs[text_info.length].hash_key.ch = code;
		text_info.glyphs[text_info.length].hash_key.advance = shift;
		text_info.glyphs[text_info.length].hash_key.be = render_context.be;
		text_info.glyphs[text_info.length].hash_key.blur = render_context.blur;

		text_info.length++;

		render_context.effect_type = EF_NONE;
		render_context.effect_timing = 0;
		render_context.effect_skip_timing = 0;
	}
	
	if (text_info.length == 0) {
		// no valid symbols in the event; this can be smth like {comment}
		free_render_context();
		return 1;
	}
	
	// depends on glyph x coordinates being monotonous, so it should be done before line wrap
	process_karaoke_effects();
	
	// alignments
	alignment = render_context.alignment;
	halign = alignment & 3;
	valign = alignment & 12;

	MarginL = (event->MarginL) ? event->MarginL : render_context.style->MarginL; 
	MarginR = (event->MarginR) ? event->MarginR : render_context.style->MarginR; 
	MarginV = (event->MarginV) ? event->MarginV : render_context.style->MarginV;

	if (render_context.evt_type != EVENT_HSCROLL) {
		int max_text_width;

		// calculate max length of a line
		max_text_width = x2scr(frame_context.track->PlayResX - MarginR) - x2scr(MarginL);

		// rearrange text in several lines
		wrap_lines_smart(max_text_width);

		// align text
		last_break = -1;
		for (i = 1; i < text_info.length + 1; ++i) { // (text_info.length + 1) is the end of the last line
			if ((i == text_info.length) || text_info.glyphs[i].linebreak) {
				int width, shift = 0;
				glyph_info_t* first_glyph = text_info.glyphs + last_break + 1;
				glyph_info_t* last_glyph = text_info.glyphs + i - 1;

				while ((last_glyph > first_glyph) && ((last_glyph->symbol == '\n') || (last_glyph->symbol == 0)))
					last_glyph --;

				width = last_glyph->pos.x + d6_to_int(last_glyph->advance.x) - first_glyph->pos.x;
				if (halign == HALIGN_LEFT) { // left aligned, no action
					shift = 0;
				} else if (halign == HALIGN_RIGHT) { // right aligned
					shift = max_text_width - width;
				} else if (halign == HALIGN_CENTER) { // centered
					shift = (max_text_width - width) / 2;
				}
				for (j = last_break + 1; j < i; ++j) {
					text_info.glyphs[j].pos.x += shift;
				}
				last_break = i - 1;
			}
		}
	} else { // render_context.evt_type == EVENT_HSCROLL
		measure_text();
	}
	
	// determing text bounding box
	compute_string_bbox(&text_info, &bbox);
	
	// determine device coordinates for text
	
	// x coordinate for everything except positioned events
	if (render_context.evt_type == EVENT_NORMAL ||
	    render_context.evt_type == EVENT_VSCROLL) {
		device_x = x2scr(MarginL);
	} else if (render_context.evt_type == EVENT_HSCROLL) {
		if (render_context.scroll_direction == SCROLL_RL)
			device_x = x2scr(frame_context.track->PlayResX - render_context.scroll_shift);
		else if (render_context.scroll_direction == SCROLL_LR)
			device_x = x2scr(render_context.scroll_shift) - (bbox.xMax - bbox.xMin);
	}

	// y coordinate for everything except positioned events
	if (render_context.evt_type == EVENT_NORMAL ||
	    render_context.evt_type == EVENT_HSCROLL) {
		if (valign == VALIGN_TOP) { // toptitle
			device_y = y2scr_top(MarginV) + d6_to_int(text_info.lines[0].asc);
		} else if (valign == VALIGN_CENTER) { // midtitle
			int scr_y = y2scr(frame_context.track->PlayResY / 2);
			device_y = scr_y - (bbox.yMax - bbox.yMin) / 2;
		} else { // subtitle
			int scr_y;
			if (valign != VALIGN_SUB)
				mp_msg(MSGT_ASS, MSGL_V, "Invalid valign, supposing 0 (subtitle)\n");
			scr_y = y2scr_sub(frame_context.track->PlayResY - MarginV);
			device_y = scr_y;
			device_y -= d6_to_int(text_info.height);
			device_y += d6_to_int(text_info.lines[0].asc);
		}
	} else if (render_context.evt_type == EVENT_VSCROLL) {
		if (render_context.scroll_direction == SCROLL_TB)
			device_y = y2scr(render_context.clip_y0 + render_context.scroll_shift) - (bbox.yMax - bbox.yMin);
		else if (render_context.scroll_direction == SCROLL_BT)
			device_y = y2scr(render_context.clip_y1 - render_context.scroll_shift);
	}

	// positioned events are totally different
	if (render_context.evt_type == EVENT_POSITIONED) {
		int base_x = 0;
		int base_y = 0;
		mp_msg(MSGT_ASS, MSGL_DBG2, "positioned event at %f, %f\n", render_context.pos_x, render_context.pos_y);
		get_base_point(bbox, alignment, &base_x, &base_y);
		device_x = x2scr_pos(render_context.pos_x) - base_x;
		device_y = y2scr_pos(render_context.pos_y) - base_y;
	}
	
	// fix clip coordinates (they depend on alignment)
	if (render_context.evt_type == EVENT_NORMAL ||
	    render_context.evt_type == EVENT_HSCROLL ||
	    render_context.evt_type == EVENT_VSCROLL) {
		render_context.clip_x0 = x2scr(render_context.clip_x0);
		render_context.clip_x1 = x2scr(render_context.clip_x1);
		if (valign == VALIGN_TOP) {
			render_context.clip_y0 = y2scr_top(render_context.clip_y0);
			render_context.clip_y1 = y2scr_top(render_context.clip_y1);
		} else if (valign == VALIGN_CENTER) {
			render_context.clip_y0 = y2scr(render_context.clip_y0);
			render_context.clip_y1 = y2scr(render_context.clip_y1);
		} else if (valign == VALIGN_SUB) {
			render_context.clip_y0 = y2scr_sub(render_context.clip_y0);
			render_context.clip_y1 = y2scr_sub(render_context.clip_y1);
		}
	} else if (render_context.evt_type == EVENT_POSITIONED) {
		render_context.clip_x0 = x2scr_pos(render_context.clip_x0);
		render_context.clip_x1 = x2scr_pos(render_context.clip_x1);
		render_context.clip_y0 = y2scr_pos(render_context.clip_y0);
		render_context.clip_y1 = y2scr_pos(render_context.clip_y1);
	}

	// calculate rotation parameters
	{
		FT_Vector center;
		
		if (render_context.have_origin) {
			center.x = x2scr(render_context.org_x);
			center.y = y2scr(render_context.org_y);
		} else {
			int bx = 0, by = 0;
			get_base_point(bbox, alignment, &bx, &by);
			center.x = device_x + bx;
			center.y = device_y + by;
		}

		for (i = 0; i < text_info.length; ++i) {
			glyph_info_t* info = text_info.glyphs + i;

			if (info->hash_key.frx || info->hash_key.fry || info->hash_key.frz) {
				info->hash_key.shift_x = info->pos.x + device_x - center.x;
				info->hash_key.shift_y = - (info->pos.y + device_y - center.y);
			} else {
				info->hash_key.shift_x = 0;
				info->hash_key.shift_y = 0;
			}
		}
	}

	// convert glyphs to bitmaps
	for (i = 0; i < text_info.length; ++i)
		get_bitmap_glyph(text_info.glyphs + i);

	event_images->top = device_y - d6_to_int(text_info.lines[0].asc);
	event_images->height = d6_to_int(text_info.height);
	event_images->detect_collisions = render_context.detect_collisions;
	event_images->shift_direction = (valign == VALIGN_TOP) ? 1 : -1;
	event_images->event = event;
	event_images->imgs = render_text(&text_info, device_x, device_y);

	free_render_context();
	
	return 0;
}

/**
 * \brief deallocate image list
 * \param img list pointer
 */
void ass_free_images(ass_image_t* img)
{
	while (img) {
		ass_image_t* next = img->next;
		free(img);
		img = next;
	}
}

static void ass_reconfigure(ass_renderer_t* priv)
{
	priv->render_id = ++last_render_id;
	ass_glyph_cache_reset();
	ass_bitmap_cache_reset();
	ass_composite_cache_reset();
	ass_free_images(priv->prev_images_root);
	priv->prev_images_root = 0;
}

void ass_set_frame_size(ass_renderer_t* priv, int w, int h)
{
	if (priv->settings.frame_width != w || priv->settings.frame_height != h) {
		priv->settings.frame_width = w;
		priv->settings.frame_height = h;
		if (priv->settings.aspect == 0.)
			priv->settings.aspect = ((double)w) / h;
		ass_reconfigure(priv);
	}
}

void ass_set_margins(ass_renderer_t* priv, int t, int b, int l, int r)
{
	if (priv->settings.left_margin != l ||
	    priv->settings.right_margin != r ||
	    priv->settings.top_margin != t ||
	    priv->settings.bottom_margin != b) {
		priv->settings.left_margin = l;
		priv->settings.right_margin = r;
		priv->settings.top_margin = t;
		priv->settings.bottom_margin = b;
		ass_reconfigure(priv);
	}
}

void ass_set_use_margins(ass_renderer_t* priv, int use)
{
	priv->settings.use_margins = use;
}

void ass_set_aspect_ratio(ass_renderer_t* priv, double ar)
{
	if (priv->settings.aspect != ar) {
		priv->settings.aspect = ar;
		ass_reconfigure(priv);
	}
}

void ass_set_font_scale(ass_renderer_t* priv, double font_scale)
{
	if (priv->settings.font_size_coeff != font_scale) {
		priv->settings.font_size_coeff = font_scale;
		ass_reconfigure(priv);
	}
}

void ass_set_hinting(ass_renderer_t* priv, ass_hinting_t ht)
{
	if (priv->settings.hinting != ht) {
		priv->settings.hinting = ht;
		ass_reconfigure(priv);
	}
}

void ass_set_line_spacing(ass_renderer_t* priv, double line_spacing)
{
	priv->settings.line_spacing = line_spacing;
}

static int ass_set_fonts_(ass_renderer_t* priv, const char* default_font, const char* default_family, int fc)
{
	if (priv->settings.default_font)
		free(priv->settings.default_font);
	if (priv->settings.default_family)
		free(priv->settings.default_family);

	priv->settings.default_font = default_font ? strdup(default_font) : 0;
	priv->settings.default_family = default_family ? strdup(default_family) : 0;

	if (priv->fontconfig_priv)
		fontconfig_done(priv->fontconfig_priv);
	priv->fontconfig_priv = fontconfig_init(priv->library, priv->ftlibrary, default_family, default_font, fc);

	return !!priv->fontconfig_priv;
}

int ass_set_fonts(ass_renderer_t* priv, const char* default_font, const char* default_family)
{
	return ass_set_fonts_(priv, default_font, default_family, 1);
}

int ass_set_fonts_nofc(ass_renderer_t* priv, const char* default_font, const char* default_family)
{
	return ass_set_fonts_(priv, default_font, default_family, 0);
}

/**
 * \brief Start a new frame
 */
static int ass_start_frame(ass_renderer_t *priv, ass_track_t* track, long long now)
{
	ass_renderer = priv;
	global_settings = &priv->settings;

	if (!priv->settings.frame_width && !priv->settings.frame_height)
		return 1; // library not initialized

	if (track->n_events == 0)
		return 1; // nothing to do
	
	frame_context.ass_priv = priv;
	frame_context.width = global_settings->frame_width;
	frame_context.height = global_settings->frame_height;
	frame_context.orig_width = global_settings->frame_width - global_settings->left_margin - global_settings->right_margin;
	frame_context.orig_height = global_settings->frame_height - global_settings->top_margin - global_settings->bottom_margin;
	frame_context.orig_width_nocrop = global_settings->frame_width -
		FFMAX(global_settings->left_margin, 0) -
		FFMAX(global_settings->right_margin, 0);
	frame_context.orig_height_nocrop = global_settings->frame_height -
		FFMAX(global_settings->top_margin, 0) -
		FFMAX(global_settings->bottom_margin, 0);
	frame_context.track = track;
	frame_context.time = now;

	ass_lazy_track_init();
	
	frame_context.font_scale = global_settings->font_size_coeff *
	                           frame_context.orig_height / frame_context.track->PlayResY;
	if (frame_context.track->ScaledBorderAndShadow)
		frame_context.border_scale = ((double)frame_context.orig_height) / frame_context.track->PlayResY;
	else
		frame_context.border_scale = 1.;

	frame_context.font_scale_x = 1.;

	priv->prev_images_root = priv->images_root;
	priv->images_root = 0;

	return 0;
}

static int cmp_event_layer(const void* p1, const void* p2)
{
	ass_event_t* e1 = ((event_images_t*)p1)->event;
	ass_event_t* e2 = ((event_images_t*)p2)->event;
	if (e1->Layer < e2->Layer)
		return -1;
	if (e1->Layer > e2->Layer)
		return 1;
	if (e1->ReadOrder < e2->ReadOrder)
		return -1;
	if (e1->ReadOrder > e2->ReadOrder)
		return 1;
	return 0;
}

#define MAX_EVENTS 100

static render_priv_t* get_render_priv(ass_event_t* event)
{
	if (!event->render_priv)
		event->render_priv = calloc(1, sizeof(render_priv_t));
	// FIXME: check render_id
	if (ass_renderer->render_id != event->render_priv->render_id) {
		memset(event->render_priv, 0, sizeof(render_priv_t));
		event->render_priv->render_id = ass_renderer->render_id;
	}
	return event->render_priv;
}

typedef struct segment_s {
	int a, b; // top and height
} segment_t;

static int overlap(segment_t* s1, segment_t* s2)
{
	if (s1->a >= s2->b || s2->a >= s1->b)
		return 0;
	return 1;
}

static int cmp_segment(const void* p1, const void* p2)
{
	return ((segment_t*)p1)->a - ((segment_t*)p2)->a;
}

static void shift_event(event_images_t* ei, int shift)
{
	ass_image_t* cur = ei->imgs;
	while (cur) {
		cur->dst_y += shift;
		// clip top and bottom
		if (cur->dst_y < 0) {
			int clip = - cur->dst_y;
			cur->h -= clip;
			cur->bitmap += clip * cur->stride;
			cur->dst_y = 0;
		}
		if (cur->dst_y + cur->h >= frame_context.height) {
			int clip = cur->dst_y + cur->h - frame_context.height;
			cur->h -= clip;
		}
		if (cur->h <= 0) {
			cur->h = 0;
			cur->dst_y = 0;
		}
		cur = cur->next;
	}
	ei->top += shift;
}

// dir: 1 - move down
//      -1 - move up
static int fit_segment(segment_t* s, segment_t* fixed, int* cnt, int dir)
{
	int i;
	int shift = 0;

	if (dir == 1) // move down
		for (i = 0; i < *cnt; ++i) {
			if (s->b + shift <= fixed[i].a || s->a + shift >= fixed[i].b)
				continue;
			shift = fixed[i].b - s->a;
		}
	else // dir == -1, move up
		for (i = *cnt-1; i >= 0; --i) {
			if (s->b + shift <= fixed[i].a || s->a + shift >= fixed[i].b)
				continue;
			shift = fixed[i].a - s->b;
		}

	fixed[*cnt].a = s->a + shift;
	fixed[*cnt].b = s->b + shift;
	(*cnt)++;
	qsort(fixed, *cnt, sizeof(segment_t), cmp_segment);
	
	return shift;
}

static void fix_collisions(event_images_t* imgs, int cnt)
{
	segment_t used[MAX_EVENTS];
	int cnt_used = 0;
	int i, j;

	// fill used[] with fixed events
	for (i = 0; i < cnt; ++i) {
		render_priv_t* priv;
		if (!imgs[i].detect_collisions) continue;
		priv = get_render_priv(imgs[i].event);
		if (priv->height > 0) { // it's a fixed event
			segment_t s;
			s.a = priv->top;
			s.b = priv->top + priv->height;
			if (priv->height != imgs[i].height) { // no, it's not
				mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_EventHeightHasChanged);
				priv->top = 0;
				priv->height = 0;
			}
			for (j = 0; j < cnt_used; ++j)
				if (overlap(&s, used + j)) { // no, it's not
					priv->top = 0;
					priv->height = 0;
				}
			if (priv->height > 0) { // still a fixed event
				used[cnt_used].a = priv->top;
				used[cnt_used].b = priv->top + priv->height;
				cnt_used ++;
				shift_event(imgs + i, priv->top - imgs[i].top);
			}
		}
	}
	qsort(used, cnt_used, sizeof(segment_t), cmp_segment);

	// try to fit other events in free spaces
	for (i = 0; i < cnt; ++i) {
		render_priv_t* priv;
		if (!imgs[i].detect_collisions) continue;
		priv = get_render_priv(imgs[i].event);
		if (priv->height == 0) { // not a fixed event
			int shift;
			segment_t s;
			s.a = imgs[i].top;
			s.b = imgs[i].top + imgs[i].height;
			shift = fit_segment(&s, used, &cnt_used, imgs[i].shift_direction);
			if (shift) shift_event(imgs + i, shift);
			// make it fixed
			priv->top = imgs[i].top;
			priv->height = imgs[i].height;
		}
		
	}
}

/**
 * \brief compare two images
 * \param i1 first image
 * \param i2 second image
 * \return 0 if identical, 1 if different positions, 2 if different content
 */
int ass_image_compare(ass_image_t *i1, ass_image_t *i2)
{
	if (i1->w != i2->w) return 2;
	if (i1->h != i2->h) return 2;
	if (i1->stride != i2->stride) return 2;
	if (i1->color != i2->color) return 2;
	if (i1->bitmap != i2->bitmap)
		return 2;
	if (i1->dst_x != i2->dst_x) return 1;
	if (i1->dst_y != i2->dst_y) return 1;
	return 0;
}

/**
 * \brief compare current and previous image list
 * \param priv library handle
 * \return 0 if identical, 1 if different positions, 2 if different content
 */
int ass_detect_change(ass_renderer_t *priv)
{
	ass_image_t* img, *img2;
	int diff;

	img = priv->prev_images_root;
	img2 = priv->images_root;
	diff = 0;
	while (img && diff < 2) {
		ass_image_t* next, *next2;
		next = img->next;
		if (img2) {
			int d = ass_image_compare(img, img2);
			if (d > diff) diff = d;
			next2 = img2->next;
		} else {
			// previous list is shorter
			diff = 2;
			break;
		}
		img = next;
		img2 = next2;
	}

	// is the previous list longer?
	if (img2)
		diff = 2;

	return diff;
}

/**
 * \brief render a frame
 * \param priv library handle
 * \param track track
 * \param now current video timestamp (ms)
 * \param detect_change a value describing how the new images differ from the previous ones will be written here:
 *        0 if identical, 1 if different positions, 2 if different content.
 *        Can be NULL, in that case no detection is performed.
 */
ass_image_t* ass_render_frame(ass_renderer_t *priv, ass_track_t* track, long long now, int* detect_change)
{
	int i, cnt, rc;
	event_images_t* last;
	ass_image_t** tail;
	
	// init frame
	rc = ass_start_frame(priv, track, now);
	if (rc != 0)
		return 0;

	// render events separately
	cnt = 0;
	for (i = 0; i < track->n_events; ++i) {
		ass_event_t* event = track->events + i;
		if ( (event->Start <= now) && (now < (event->Start + event->Duration)) ) {
			if (cnt >= priv->eimg_size) {
				priv->eimg_size += 100;
				priv->eimg = realloc(priv->eimg, priv->eimg_size * sizeof(event_images_t));
			}
			rc = ass_render_event(event, priv->eimg + cnt);
			if (!rc) ++cnt;
		}
	}

	// sort by layer
	qsort(priv->eimg, cnt, sizeof(event_images_t), cmp_event_layer);

	// call fix_collisions for each group of events with the same layer
	last = priv->eimg;
	for (i = 1; i < cnt; ++i)
		if (last->event->Layer != priv->eimg[i].event->Layer) {
			fix_collisions(last, priv->eimg + i - last);
			last = priv->eimg + i;
		}
	if (cnt > 0)
		fix_collisions(last, priv->eimg + cnt - last);

	// concat lists
	tail = &ass_renderer->images_root;
	for (i = 0; i < cnt; ++i) {
		ass_image_t* cur = priv->eimg[i].imgs;
		while (cur) {
			*tail = cur;
			tail = &cur->next;
			cur = cur->next;
		}
	}

	if (detect_change)
		*detect_change = ass_detect_change(priv);
	
	// free the previous image list
	ass_free_images(priv->prev_images_root);
	priv->prev_images_root = 0;

	return ass_renderer->images_root;
}

