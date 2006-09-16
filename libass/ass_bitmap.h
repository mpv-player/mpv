#ifndef __ASS_BITMAP_H__
#define __ASS_BITMAP_H__

typedef struct bitmap_s {
	int left, top;
	int w, h; // width, height
	unsigned char* buffer; // w x h buffer
} bitmap_t;

int glyph_to_bitmap(FT_Glyph glyph, FT_Glyph outline_glyph, bitmap_t** bm_g, bitmap_t** bm_o);
void ass_free_bitmap(bitmap_t* bm);

#endif

