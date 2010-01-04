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

#ifndef MPLAYER_FONT_LOAD_H
#define MPLAYER_FONT_LOAD_H

#include "config.h"

#ifdef CONFIG_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

typedef struct {
    unsigned char *bmp;
    unsigned char *pal;
    int w,h,c;
#ifdef CONFIG_FREETYPE
    int charwidth,charheight,pen,baseline,padding;
    int current_count, current_alloc;
#endif
} raw_file;

typedef struct font_desc {
#ifdef CONFIG_FREETYPE
    int dynamic;
#endif
    char *name;
    char *fpath;
    int spacewidth;
    int charspace;
    int height;
//    char *fname_a;
//    char *fname_b;
    raw_file* pic_a[16];
    raw_file* pic_b[16];
    short font[65536];
    int start[65536];   // short is not enough for unicode fonts
    short width[65536];
    int freetype;

#ifdef CONFIG_FREETYPE
    int face_cnt;

    FT_Face faces[16];
    FT_UInt glyph_index[65536];

    int max_width, max_height;

    struct
    {
	int g_r;
	int o_r;
	int g_w;
	int o_w;
	int o_size;
	unsigned volume;

	unsigned *g;
	unsigned *gt2;
	unsigned *om;
	unsigned char *omt;
	unsigned short *tmp;
    } tables;
#endif

} font_desc_t;

extern font_desc_t* vo_font;
extern font_desc_t* sub_font;

extern char *subtitle_font_encoding;
extern float text_font_scale_factor;
extern float osd_font_scale_factor;
extern float subtitle_font_radius;
extern float subtitle_font_thickness;
extern int subtitle_autoscale;

extern int vo_image_width;
extern int vo_image_height;

extern int force_load_font;

int init_freetype(void);
int done_freetype(void);

font_desc_t* read_font_desc_ft(const char* fname,int face_index,int movie_width, int movie_height, float font_scale_factor);
void free_font_desc(font_desc_t *desc);

void render_one_glyph(font_desc_t *desc, int c);
int kerning(font_desc_t *desc, int prevc, int c);

void load_font_ft(int width, int height, font_desc_t **desc, const char *name, float font_scale_factor);

void blur(unsigned char *buffer, unsigned short *tmp2, int width, int height,
          int stride, int *m2, int r, int mwidth);

raw_file* load_raw(char *name,int verbose);
font_desc_t* read_font_desc(const char* fname,float factor,int verbose);

#endif /* MPLAYER_FONT_LOAD_H */
