#ifndef __MPLAYER_FONT_LOAD_H
#define __MPLAYER_FONT_LOAD_H

#ifdef HAVE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

typedef struct {
    unsigned char *bmp;
    unsigned char *pal;
    int w,h,c;
#ifdef HAVE_FREETYPE
    int charwidth,charheight,pen,baseline,padding;
    int current_count, current_alloc;
#endif
} raw_file;

typedef struct {
#ifdef HAVE_FREETYPE
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

#ifdef HAVE_FREETYPE
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

#ifdef HAVE_FREETYPE

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

font_desc_t* read_font_desc_ft(const char* fname,int movie_width, int movie_height);
void free_font_desc(font_desc_t *desc);

void render_one_glyph(font_desc_t *desc, int c);
int kerning(font_desc_t *desc, int prevc, int c);

void load_font_ft(int width, int height);

void blur(unsigned char *buffer, unsigned short *tmp2, int width, int height,
          int stride, int *m2, int r, int mwidth);

#else

static void render_one_glyph(font_desc_t *desc, int c) {}
static int kerning(font_desc_t *desc, int prevc, int c) { return 0; }

#endif

raw_file* load_raw(char *name,int verbose);
font_desc_t* read_font_desc(const char* fname,float factor,int verbose);

#endif /* ! __MPLAYER_FONT_LOAD_H */
