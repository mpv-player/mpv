#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <ft2build.h>
#include FT_GLYPH_H

#include "mp_msg.h"
#include "ass_bitmap.h"

static bitmap_t* alloc_bitmap(int w, int h)
{
	bitmap_t* bm;
	bm = calloc(1, sizeof(bitmap_t));
	bm->buffer = malloc(w*h);
	bm->w = w;
	bm->h = h;
	bm->left = bm->top = 0;
	return bm;
}

void ass_free_bitmap(bitmap_t* bm)
{
	if (bm) {
		if (bm->buffer) free(bm->buffer);
		free(bm);
	}
}

static bitmap_t* glyph_to_bitmap_internal(FT_Glyph glyph, int bord)
{
	FT_BitmapGlyph bg;
	FT_Bitmap* bit;
	bitmap_t* bm;
	int w, h;
	unsigned char* src;
	unsigned char* dst;
	int i;
	int error;

	error = FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, 0, 0);
	if (error) {
		mp_msg(MSGT_GLOBAL, MSGL_WARN, "FT_Glyph_To_Bitmap error %d \n", error);
		return 0;
	}

	bg = (FT_BitmapGlyph)glyph;
	bit = &(bg->bitmap);
	if (bit->pixel_mode != FT_PIXEL_MODE_GRAY) {
		mp_msg(MSGT_GLOBAL, MSGL_WARN, "Unsupported pixel mode: %d\n", (int)(bit->pixel_mode));
		FT_Done_Glyph(glyph);
		return 0;
	}

	w = bit->width;
	h = bit->rows;
	bm = alloc_bitmap(w + 2*bord, h + 2*bord);
	memset(bm->buffer, 0, bm->w * bm->h);
	bm->left = bg->left - bord;
	bm->top = - bg->top - bord;

	src = bit->buffer;
	dst = bm->buffer + bord + bm->w * bord;
	for (i = 0; i < h; ++i) {
		memcpy(dst, src, w);
		src += bit->pitch;
		dst += bm->w;
	}

	return bm;
}

int glyph_to_bitmap(FT_Glyph glyph, FT_Glyph outline_glyph, bitmap_t** bm_g, bitmap_t** bm_o)
{
	assert(bm_g);

	if (glyph)
		*bm_g = glyph_to_bitmap_internal(glyph, 0);
	if (!*bm_g)
		return 1;
	if (outline_glyph && bm_o) {
		*bm_o = glyph_to_bitmap_internal(outline_glyph, 0);
		if (!*bm_o) {
			ass_free_bitmap(*bm_g);
			return 1;
		}
	}

	return 0;
}

