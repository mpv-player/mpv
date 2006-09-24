#ifndef __ASS_BITMAP_H__
#define __ASS_BITMAP_H__

typedef struct ass_synth_priv_s ass_synth_priv_t;

ass_synth_priv_t* ass_synth_init();
void ass_synth_done(ass_synth_priv_t* priv);

typedef struct bitmap_s {
	int left, top;
	int w, h; // width, height
	unsigned char* buffer; // w x h buffer
} bitmap_t;

int glyph_to_bitmap(ass_synth_priv_t* priv, FT_Glyph glyph, FT_Glyph outline_glyph, bitmap_t** bm_g, bitmap_t** bm_o, bitmap_t** bm_s, int be);
void ass_free_bitmap(bitmap_t* bm);

#endif

