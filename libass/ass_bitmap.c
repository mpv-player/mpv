#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <ft2build.h>
#include FT_GLYPH_H

#include "mp_msg.h"
#include "libvo/font_load.h" // for blur()
#include "ass_bitmap.h"

struct ass_synth_priv_s {
	int tmp_w, tmp_h;
	unsigned short* tmp;

	int g_r;
	int g_w;

	unsigned *g;
	unsigned *gt2;
};

static const unsigned int maxcolor = 255;
static const unsigned base = 256;
static const double blur_radius = 1.5;

static int generate_tables(ass_synth_priv_t* priv, double radius)
{
    double A = log(1.0/base)/(radius*radius*2);
    int mx, i;
    double volume_diff, volume_factor = 0;
    unsigned volume;
    
    priv->g_r = ceil(radius);
    priv->g_w = 2*priv->g_r+1;

    if (priv->g_r) {
	priv->g = malloc(priv->g_w * sizeof(unsigned));
	priv->gt2 = malloc(256 * priv->g_w * sizeof(unsigned));
	if (priv->g==NULL || priv->gt2==NULL) {
	    return -1;
	}
    }

    if (priv->g_r) {
	// gaussian curve with volume = 256
	for (volume_diff=10000000; volume_diff>0.0000001; volume_diff*=0.5){
	    volume_factor+= volume_diff;
	    volume=0;
	    for (i = 0; i<priv->g_w; ++i) {
		priv->g[i] = (unsigned)(exp(A * (i-priv->g_r)*(i-priv->g_r)) * volume_factor + .5);
		volume+= priv->g[i];
	    }
	    if(volume>256) volume_factor-= volume_diff;
	}
	volume=0;
	for (i = 0; i<priv->g_w; ++i) {
	    priv->g[i] = (unsigned)(exp(A * (i-priv->g_r)*(i-priv->g_r)) * volume_factor + .5);
	    volume+= priv->g[i];
	}

	// gauss table:
	for(mx=0;mx<priv->g_w;mx++){
	    for(i=0;i<256;i++){
		priv->gt2[mx+i*priv->g_w] = i*priv->g[mx];
	    }
	}
    }
    
    return 0;
}

static void resize_tmp(ass_synth_priv_t* priv, int w, int h)
{
	if (priv->tmp_w >= w && priv->tmp_h >= h)
		return;
	if (priv->tmp_w == 0)
		priv->tmp_w = 64;
	if (priv->tmp_h == 0)
		priv->tmp_h = 64;
	while (priv->tmp_w < w) priv->tmp_w *= 2;
	while (priv->tmp_h < h) priv->tmp_h *= 2;
	if (priv->tmp)
		free(priv->tmp);
	priv->tmp = malloc((priv->tmp_w + 1) * priv->tmp_h * sizeof(short));
}

ass_synth_priv_t* ass_synth_init()
{
	ass_synth_priv_t* priv = calloc(1, sizeof(ass_synth_priv_t));
	generate_tables(priv, blur_radius);
	return priv;
}

void ass_synth_done(ass_synth_priv_t* priv)
{
	free(priv);
}

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

static void fix_outline(bitmap_t* bm_g, bitmap_t* bm_o)
{
	int x, y;
	const int l = bm_o->left > bm_g->left ? bm_o->left : bm_g->left;
	const int t = bm_o->top > bm_g->top ? bm_o->top : bm_g->top;
	const int r = bm_o->left + bm_o->w < bm_g->left + bm_g->w ? bm_o->left + bm_o->w : bm_g->left + bm_g->w;
	const int b = bm_o->top + bm_o->h < bm_g->top + bm_g->h ? bm_o->top + bm_o->h : bm_g->top + bm_g->h;
	unsigned char* g = bm_g->buffer + (t - bm_g->top) * bm_g->w + (l - bm_g->left);
	unsigned char* o = bm_o->buffer + (t - bm_o->top) * bm_o->w + (l - bm_o->left);
	
	for (y = 0; y < b - t; ++y) {
		for (x = 0; x < r - l; ++x) {
			unsigned char c_g, c_o;
			c_g = g[x];
			c_o = o[x];
			o[x] = (c_o > c_g) ? c_o - c_g : 0;
		}
		g += bm_g->w;
		o += bm_o->w;
	}
}

int glyph_to_bitmap(ass_synth_priv_t* priv, FT_Glyph glyph, FT_Glyph outline_glyph, bitmap_t** bm_g, bitmap_t** bm_o, int be)
{
	const int bord = ceil(blur_radius);

	assert(bm_g && bm_o);

	if (glyph)
		*bm_g = glyph_to_bitmap_internal(glyph, bord);
	else
		*bm_g = 0;
	if (!*bm_g)
		return 1;
	if (outline_glyph) {
		*bm_o = glyph_to_bitmap_internal(outline_glyph, bord);
		if (!*bm_o) {
			ass_free_bitmap(*bm_g);
			return 1;
		}
	} else
		*bm_o = 0;

	if (*bm_o)
		resize_tmp(priv, (*bm_o)->w, (*bm_o)->h);
	resize_tmp(priv, (*bm_g)->w, (*bm_g)->h);
	
	if (be) {
		blur((*bm_g)->buffer, priv->tmp, (*bm_g)->w, (*bm_g)->h, (*bm_g)->w, (int*)priv->gt2, priv->g_r, priv->g_w);
		if (*bm_o)
			blur((*bm_o)->buffer, priv->tmp, (*bm_o)->w, (*bm_o)->h, (*bm_o)->w, (int*)priv->gt2, priv->g_r, priv->g_w);
	}

	if (*bm_o)
		fix_outline(*bm_g, *bm_o);

	return 0;
}

