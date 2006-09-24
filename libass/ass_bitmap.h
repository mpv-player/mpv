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

/**
 * \brief perform glyph rendering
 * \param glyph original glyph
 * \param outline_glyph "border" glyph, produced from original by FreeType's glyph stroker
 * \param bm_g out: pointer to the bitmap of original glyph is returned here
 * \param bm_o out: pointer to the bitmap of outline (border) glyph is returned here
 * \param bm_g out: pointer to the bitmap of glyph shadow is returned here
 * \param be 1 = produces blurred bitmaps, 0 = normal bitmaps
 */
int glyph_to_bitmap(ass_synth_priv_t* priv, FT_Glyph glyph, FT_Glyph outline_glyph, bitmap_t** bm_g, bitmap_t** bm_o, bitmap_t** bm_s, int be);

void ass_free_bitmap(bitmap_t* bm);

#endif

