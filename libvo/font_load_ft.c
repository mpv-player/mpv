/*
 * Renders antialiased fonts for mplayer using freetype library.
 * Should work with TrueType, Type1 and any other font supported by libfreetype.
 *
 * Artur Zaprzala <zybi@fanthom.irc.pl>
 *
 * ported inside mplayer by Jindrich Makovicka 
 * <makovick@gmail.com>
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef CONFIG_ICONV
#include <iconv.h>
#endif

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#ifdef CONFIG_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

#include "libavutil/common.h"
#include "mpbswap.h"
#include "font_load.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "mplayer.h"
#include "get_path.h"
#include "osd_font.h"

#if (FREETYPE_MAJOR > 2) || (FREETYPE_MAJOR == 2 && FREETYPE_MINOR >= 1)
#define HAVE_FREETYPE21
#endif

char *subtitle_font_encoding = NULL;
float text_font_scale_factor = 5.0;
float osd_font_scale_factor = 6.0;
float subtitle_font_radius = 2.0;
float subtitle_font_thickness = 2.0;
// 0 = no autoscale
// 1 = video height
// 2 = video width
// 3 = diagonal
int subtitle_autoscale = 3;

int vo_image_width = 0;
int vo_image_height = 0;
int force_load_font;

int using_freetype = 0;
int font_fontconfig = 0;

//// constants
static unsigned int const colors = 256;
static unsigned int const maxcolor = 255;
static unsigned const	base = 256;
static unsigned const first_char = 33;
#define MAX_CHARSET_SIZE 60000

static FT_Library library;

#define OSD_CHARSET_SIZE 15

static FT_ULong	osd_charset[OSD_CHARSET_SIZE] =
{
    0xe001, 0xe002, 0xe003, 0xe004, 0xe005, 0xe006, 0xe007, 0xe008,
    0xe009, 0xe00a, 0xe00b, 0xe010, 0xe011, 0xe012, 0xe013
};

static FT_ULong	osd_charcodes[OSD_CHARSET_SIZE] =
{
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x09,0x0a,0x0b,0x10,0x11,0x12,0x13
};

#define f266ToInt(x)		(((x)+32)>>6)	// round fractional fixed point number to integer
						// coordinates are in 26.6 pixels (i.e. 1/64th of pixels)
#define f266CeilToInt(x)	(((x)+63)>>6)	// ceiling
#define f266FloorToInt(x)	((x)>>6)	// floor
#define f1616ToInt(x)		(((x)+0x8000)>>16)	// 16.16
#define floatTof266(x)		((int)((x)*(1<<6)+0.5))

#define ALIGN(x)                (((x)+7)&~7)    // 8 byte align

#define WARNING(msg, args...)      mp_msg(MSGT_OSD, MSGL_WARN, msg "\n", ## args)

#define DEBUG 0

//static double ttime;


static void paste_bitmap(unsigned char *bbuffer, FT_Bitmap *bitmap, int x, int y, int width, int height, int bwidth) {
    int drow = x+y*width;
    int srow = 0;
    int sp, dp, w, h;
    if (bitmap->pixel_mode==ft_pixel_mode_mono)
	for (h = bitmap->rows; h>0 && height > 0; --h, height--, drow+=width, srow+=bitmap->pitch)
	    for (w = bwidth, sp=dp=0; w>0; --w, ++dp, ++sp)
		    bbuffer[drow+dp] = (bitmap->buffer[srow+sp/8] & (0x80>>(sp%8))) ? 255:0;
    else
	for (h = bitmap->rows; h>0 && height > 0; --h, height--, drow+=width, srow+=bitmap->pitch)
	    for (w = bwidth, sp=dp=0; w>0; --w, ++dp, ++sp)
		    bbuffer[drow+dp] = bitmap->buffer[srow+sp];
}


static int check_font(font_desc_t *desc, float ppem, int padding, int pic_idx,
		      int charset_size, FT_ULong *charset, FT_ULong *charcodes,
		      int unicode) {
    FT_Error	error;
    FT_Face face = desc->faces[pic_idx];
    int	const	load_flags = FT_LOAD_DEFAULT;
    int		ymin = INT_MAX, ymax = INT_MIN;
    int		space_advance = 20;
    int         width, height;
    unsigned char *bbuffer;
    int i, uni_charmap = 1;
    
    error = FT_Select_Charmap(face, ft_encoding_unicode);
//    fprintf(stderr, "select unicode charmap: %d\n", error);

    if (face->charmap==NULL || face->charmap->encoding!=ft_encoding_unicode) {
	WARNING("Unicode charmap not available for this font. Very bad!");
	uni_charmap = 0;
	error = FT_Set_Charmap(face, face->charmaps[0]);
	if (error) WARNING("No charmaps! Strange.");
    }

    /* set size */
    if (FT_IS_SCALABLE(face)) {
	error = FT_Set_Char_Size(face, 0, floatTof266(ppem), 0, 0);
	if (error) WARNING("FT_Set_Char_Size failed.");
    } else {
	int j = 0;
	int jppem = face->available_sizes[0].height;
	/* find closest size */
	for (i = 0; i<face->num_fixed_sizes; ++i) {
	    if (fabs(face->available_sizes[i].height - ppem) < abs(face->available_sizes[i].height - jppem)) {
		j = i;
		jppem = face->available_sizes[i].height;
	    }
	}
	WARNING("Selected font is not scalable. Using ppem=%i.", face->available_sizes[j].height);
	error = FT_Set_Pixel_Sizes(face, face->available_sizes[j].width, face->available_sizes[j].height);
	if (error) WARNING("FT_Set_Pixel_Sizes failed.");
    }

    if (FT_IS_FIXED_WIDTH(face))
	WARNING("Selected font is fixed-width.");

    /* compute space advance */
    error = FT_Load_Char(face, ' ', load_flags);
    if (error) WARNING("spacewidth set to default.");
    else space_advance = f266ToInt(face->glyph->advance.x);

    if (!desc->spacewidth) desc->spacewidth = 2*padding + space_advance;
    if (!desc->charspace) desc->charspace = -2*padding;
    if (!desc->height) desc->height = f266ToInt(face->size->metrics.height);


    for (i= 0; i<charset_size; ++i) {
	FT_ULong	character, code;
	FT_UInt		glyph_index;

	character = charset[i];
	code = charcodes[i];
	desc->font[unicode?character:code] = pic_idx;
	// get glyph index
	if (character==0)
	    glyph_index = 0;
	else {
	    glyph_index = FT_Get_Char_Index(face, uni_charmap ? character:code);
	    if (glyph_index==0) {
		WARNING("Glyph for char 0x%02lx|U+%04lX|%c not found.", code, character,
			code<' '||code>255 ? '.':(char)code);
		desc->font[unicode?character:code] = -1;
		continue;
	    }
	}
	desc->glyph_index[unicode?character:code] = glyph_index;
    }
//    fprintf(stderr, "font height: %lf\n", (double)(face->bbox.yMax-face->bbox.yMin)/(double)face->units_per_EM*ppem);
//    fprintf(stderr, "font width: %lf\n", (double)(face->bbox.xMax-face->bbox.xMin)/(double)face->units_per_EM*ppem);

    ymax = (double)(face->bbox.yMax)/(double)face->units_per_EM*ppem+1;
    ymin = (double)(face->bbox.yMin)/(double)face->units_per_EM*ppem-1;
    
    width = ppem*(face->bbox.xMax-face->bbox.xMin)/face->units_per_EM+3+2*padding;
    if (desc->max_width < width) desc->max_width = width;
    width = ALIGN(width);
    desc->pic_b[pic_idx]->charwidth = width;

    if (width <= 0) {
	mp_msg(MSGT_OSD, MSGL_ERR, "Wrong bounding box, width <= 0 !\n");
	return -1;
    }

    if (ymax<=ymin) {
	mp_msg(MSGT_OSD, MSGL_ERR, "Something went wrong. Use the source!\n");
	return -1;
    }
    
    height = ymax - ymin + 2*padding;
    if (height <= 0) {
	mp_msg(MSGT_OSD, MSGL_ERR, "Wrong bounding box, height <= 0 !\n");
	return -1;
    }

    if (desc->max_height < height) desc->max_height = height;
    desc->pic_b[pic_idx]->charheight = height;
    
//    fprintf(stderr, "font height2: %d\n", height);
    desc->pic_b[pic_idx]->baseline = ymax + padding;
    desc->pic_b[pic_idx]->padding = padding;
    desc->pic_b[pic_idx]->current_alloc = 0;
    desc->pic_b[pic_idx]->current_count = 0;

    bbuffer = NULL;
    
    desc->pic_b[pic_idx]->w = width;
    desc->pic_b[pic_idx]->h = height;
    desc->pic_b[pic_idx]->c = colors;
    desc->pic_b[pic_idx]->bmp = bbuffer;
    desc->pic_b[pic_idx]->pen = 0;
    return 0;
}

// general outline
static void outline(
	unsigned char *s,
	unsigned char *t,
	int width,
	int height,
	int stride,
	unsigned char *m,
	int r,
	int mwidth,
	int msize) {

    int x, y;
    
    for (y = 0; y<height; y++) {
	for (x = 0; x<width; x++) {
	    const int src= s[x];
	    if(src==0) continue;
	    {
		const int x1=(x<r) ? r-x : 0;
		const int y1=(y<r) ? r-y : 0;
		const int x2=(x+r>=width ) ? r+width -x : 2*r+1;
		const int y2=(y+r>=height) ? r+height-y : 2*r+1;
		register unsigned char *dstp= t + (y1+y-r)* stride + x-r;
		//register int *mp  = m +  y1     *mwidth;
		register unsigned char *mp= m + msize*src + y1*mwidth;
		int my;

		for(my= y1; my<y2; my++){
		    register int mx;
		    for(mx= x1; mx<x2; mx++){
			if(dstp[mx] < mp[mx]) dstp[mx]= mp[mx];
		    }
		    dstp+=stride;
		    mp+=mwidth;
		}
            }
	}
	s+= stride;
    }
}


// 1 pixel outline
static void outline1(
	unsigned char *s,
	unsigned char *t,
	int width,
	int height,
	int stride) {

    int x, y;
    int skip = stride-width;

    for (x = 0; x<width; ++x, ++s, ++t) *t = *s;
    s += skip;
    t += skip;
    for (y = 1; y<height-1; ++y) {
	*t++ = *s++;
	for (x = 1; x<width-1; ++x, ++s, ++t) {
	    unsigned v = (
		    s[-1-stride]+
		    s[-1+stride]+
		    s[+1-stride]+
		    s[+1+stride]
		)/2 + (
		    s[-1]+
		    s[+1]+
		    s[-stride]+
		    s[+stride]+
		    s[0]
		);
	    *t = v>maxcolor ? maxcolor : v;
	}
	*t++ = *s++;
	s += skip;
	t += skip;
    }
    for (x = 0; x<width; ++x, ++s, ++t) *t = *s;
}

// "0 pixel outline"
static void outline0(
	unsigned char *s,
	unsigned char *t,
	int width,
	int height,
	int stride) {
    int y;
    for (y = 0; y<height; ++y) {
	memcpy(t, s, width);
	s += stride;
	t += stride;
    }
}

// gaussian blur
void blur(
	unsigned char *buffer,
	unsigned short *tmp2,
	int width,
	int height,
	int stride,
	int *m2,
	int r,
	int mwidth) {

    int x, y;

    unsigned char  *s = buffer;
    unsigned short *t = tmp2+1;
    for(y=0; y<height; y++){
	memset(t-1, 0, (width+1)*sizeof(short));

	for(x=0; x<r; x++){
	    const int src= s[x];
	    if(src){
		register unsigned short *dstp= t + x-r;
		int mx;
		unsigned *m3= m2 + src*mwidth;
		for(mx=r-x; mx<mwidth; mx++){
		    dstp[mx]+= m3[mx];
		}
	    }
	}

	for(; x<width-r; x++){
	    const int src= s[x];
	    if(src){
		register unsigned short *dstp= t + x-r;
		int mx;
		unsigned *m3= m2 + src*mwidth;
		for(mx=0; mx<mwidth; mx++){
		    dstp[mx]+= m3[mx];
		}
	    }
	}

	for(; x<width; x++){
	    const int src= s[x];
	    if(src){
		register unsigned short *dstp= t + x-r;
		int mx;
		const int x2= r+width -x;
		unsigned *m3= m2 + src*mwidth;
		for(mx=0; mx<x2; mx++){
		    dstp[mx]+= m3[mx];
		}
	    }
	}

	s+= stride;
	t+= width + 1;
    }

    t = tmp2;
    for(x=0; x<width; x++){
	for(y=0; y<r; y++){
	    unsigned short *srcp= t + y*(width+1) + 1;
	    int src= *srcp;
	    if(src){
		register unsigned short *dstp= srcp - 1 + width+1;
		const int src2= (src + 128)>>8;
		unsigned *m3= m2 + src2*mwidth;

		int mx;
		*srcp= 128;
		for(mx=r-1; mx<mwidth; mx++){
		    *dstp += m3[mx];
		    dstp+= width+1;
		}
	    }
	}
	for(; y<height-r; y++){
	    unsigned short *srcp= t + y*(width+1) + 1;
	    int src= *srcp;
	    if(src){
		register unsigned short *dstp= srcp - 1 - r*(width+1);
		const int src2= (src + 128)>>8;
		unsigned *m3= m2 + src2*mwidth;

		int mx;
		*srcp= 128;
		for(mx=0; mx<mwidth; mx++){
		    *dstp += m3[mx];
		    dstp+= width+1;
		}
	    }
	}
	for(; y<height; y++){
	    unsigned short *srcp= t + y*(width+1) + 1;
	    int src= *srcp;
	    if(src){
		const int y2=r+height-y;
		register unsigned short *dstp= srcp - 1 - r*(width+1);
		const int src2= (src + 128)>>8;
		unsigned *m3= m2 + src2*mwidth;

		int mx;
		*srcp= 128;
		for(mx=0; mx<y2; mx++){
		    *dstp += m3[mx];
		    dstp+= width+1;
		}
	    }
	}
	t++;
    }

    t = tmp2;
    s = buffer;
    for(y=0; y<height; y++){
	for(x=0; x<width; x++){
	    s[x]= t[x]>>8;
	}
	s+= stride;
	t+= width + 1;
    }
}

static void resample_alpha(unsigned char *abuf, unsigned char *bbuf, int width, int height, int stride, float factor)
{
        int f=factor*256.0f;
        int i,j;
	for (i = 0; i < height; i++) {
	    unsigned char *a = abuf+i*stride;
	    unsigned char *b = bbuf+i*stride;
	    for(j=0;j<width;j++,a++,b++){
		int x=*a;	// alpha
		int y=*b;	// bitmap
		x=255-((x*f)>>8); // scale
		if (x+y>255) x=255-y; // to avoid overflows
		if (x<1) x=1; else if (x>=252) x=0;
		*a=x;
	    }
	}
}

#define ALLOC_INCR 32
void render_one_glyph(font_desc_t *desc, int c)
{
    FT_GlyphSlot	slot;
    FT_UInt		glyph_index;
    FT_BitmapGlyph glyph;
    int width, height, stride, maxw, off;
    unsigned char *abuffer, *bbuffer;
    
    int	const	load_flags = FT_LOAD_DEFAULT;
    int		pen_xa;
    int font = desc->font[c];
    int error;
    
//    fprintf(stderr, "render_one_glyph %d\n", c);

    if (!desc->dynamic) return;
    if (desc->width[c] != -1) return;
    if (desc->font[c] == -1) return;

    glyph_index = desc->glyph_index[c];
    
    // load glyph
    error = FT_Load_Glyph(desc->faces[font], glyph_index, load_flags);
    if (error) {
	WARNING("FT_Load_Glyph 0x%02x (char 0x%04x) failed.", glyph_index, c);
	desc->font[c] = -1;
	return;
    }
    slot = desc->faces[font]->glyph;

    // render glyph
    if (slot->format != ft_glyph_format_bitmap) {
	error = FT_Render_Glyph(slot, ft_render_mode_normal);
	if (error) {
	    WARNING("FT_Render_Glyph 0x%04x (char 0x%04x) failed.", glyph_index, c);
	    desc->font[c] = -1;
	    return;
	}
    }

    // extract glyph image
    error = FT_Get_Glyph(slot, (FT_Glyph*)&glyph);
    if (error) {
	WARNING("FT_Get_Glyph 0x%04x (char 0x%04x) failed.", glyph_index, c);
	desc->font[c] = -1;
	return;
    }

//    fprintf(stderr, "glyph generated\n");

    maxw = desc->pic_b[font]->charwidth;
    
    if (glyph->bitmap.width > maxw) {
	fprintf(stderr, "glyph too wide!\n");
    }

    // allocate new memory, if needed
//    fprintf(stderr, "\n%d %d %d\n", desc->pic_b[font]->charwidth, desc->pic_b[font]->charheight, desc->pic_b[font]->current_alloc);
    if (desc->pic_b[font]->current_count >= desc->pic_b[font]->current_alloc) {
	int newsize = desc->pic_b[font]->charwidth*desc->pic_b[font]->charheight*(desc->pic_b[font]->current_alloc+ALLOC_INCR);
	int increment = desc->pic_b[font]->charwidth*desc->pic_b[font]->charheight*ALLOC_INCR;
	desc->pic_b[font]->current_alloc += ALLOC_INCR;

//	fprintf(stderr, "\nns = %d inc = %d\n", newsize, increment);

	desc->pic_b[font]->bmp = realloc(desc->pic_b[font]->bmp, newsize);
	desc->pic_a[font]->bmp = realloc(desc->pic_a[font]->bmp, newsize);

	off = desc->pic_b[font]->current_count*desc->pic_b[font]->charwidth*desc->pic_b[font]->charheight;
	memset(desc->pic_b[font]->bmp+off, 0, increment);
	memset(desc->pic_a[font]->bmp+off, 0, increment);
    }
    
    abuffer = desc->pic_a[font]->bmp;
    bbuffer = desc->pic_b[font]->bmp;

    off = desc->pic_b[font]->current_count*desc->pic_b[font]->charwidth*desc->pic_b[font]->charheight;

    paste_bitmap(bbuffer+off,
		 &glyph->bitmap,
		 desc->pic_b[font]->padding + glyph->left,
		 desc->pic_b[font]->baseline - glyph->top,
		 desc->pic_b[font]->charwidth, desc->pic_b[font]->charheight,
		 glyph->bitmap.width <= maxw ? glyph->bitmap.width : maxw);
    
//    fprintf(stderr, "glyph pasted\n");
    FT_Done_Glyph((FT_Glyph)glyph);
    
    /* advance pen */
    pen_xa = f266ToInt(slot->advance.x) + 2*desc->pic_b[font]->padding;
    if (pen_xa > maxw) pen_xa = maxw;

    desc->start[c] = off;
    width = desc->width[c] = pen_xa;
    height = desc->pic_b[font]->charheight;
    stride = desc->pic_b[font]->w;

    if (desc->tables.o_r == 0) {
	outline0(bbuffer+off, abuffer+off, width, height, stride);
    } else if (desc->tables.o_r == 1) {
	outline1(bbuffer+off, abuffer+off, width, height, stride);
    } else {
	outline(bbuffer+off, abuffer+off, width, height, stride,
		desc->tables.omt, desc->tables.o_r, desc->tables.o_w,
		desc->tables.o_size);
    }
//    fprintf(stderr, "fg: outline t = %lf\n", GetTimer()-t);
    
    if (desc->tables.g_r) {
	blur(abuffer+off, desc->tables.tmp, width, height, stride,
	     desc->tables.gt2, desc->tables.g_r,
	     desc->tables.g_w);
//	fprintf(stderr, "fg: blur t = %lf\n", GetTimer()-t);
    }

    resample_alpha(abuffer+off, bbuffer+off, width, height, stride, font_factor);

    desc->pic_b[font]->current_count++;
}


static int prepare_font(font_desc_t *desc, FT_Face face, float ppem, int pic_idx,
			int charset_size, FT_ULong *charset, FT_ULong *charcodes, int unicode,
			double thickness, double radius)
{
    int i, err;
    int padding = ceil(radius) + ceil(thickness);

    desc->faces[pic_idx] = face;

    desc->pic_a[pic_idx] = malloc(sizeof(raw_file));
    if (!desc->pic_a[pic_idx]) return -1;
    desc->pic_b[pic_idx] = malloc(sizeof(raw_file));
    if (!desc->pic_b[pic_idx]) return -1;

    desc->pic_a[pic_idx]->bmp = NULL;
    desc->pic_a[pic_idx]->pal = NULL;
    desc->pic_b[pic_idx]->bmp = NULL;
    desc->pic_b[pic_idx]->pal = NULL;

    desc->pic_a[pic_idx]->pal = malloc(sizeof(unsigned char)*256*3);
    if (!desc->pic_a[pic_idx]->pal) return -1;
    for (i = 0; i<768; ++i) desc->pic_a[pic_idx]->pal[i] = i/3;

    desc->pic_b[pic_idx]->pal = malloc(sizeof(unsigned char)*256*3);
    if (!desc->pic_b[pic_idx]->pal) return -1;
    for (i = 0; i<768; ++i) desc->pic_b[pic_idx]->pal[i] = i/3;

//    ttime = GetTimer();
    err = check_font(desc, ppem, padding, pic_idx, charset_size, charset, charcodes, unicode);
//    ttime=GetTimer()-ttime;
//    printf("render:   %7lf us\n",ttime);
    if (err) return -1;
//    fprintf(stderr, "fg: render t = %lf\n", GetTimer()-t);

    desc->pic_a[pic_idx]->w = desc->pic_b[pic_idx]->w;
    desc->pic_a[pic_idx]->h = desc->pic_b[pic_idx]->h;
    desc->pic_a[pic_idx]->c = colors;

    desc->pic_a[pic_idx]->bmp = NULL;

//    fprintf(stderr, "fg: w = %d, h = %d\n", desc->pic_a[pic_idx]->w, desc->pic_a[pic_idx]->h);
    return 0;
    
}

static int generate_tables(font_desc_t *desc, double thickness, double radius)
{
    int width = desc->max_height;
    int height = desc->max_width;
    
    double A = log(1.0/base)/(radius*radius*2);
    int mx, my, i;
    double volume_diff, volume_factor = 0;
    unsigned char *omtp;
    
    desc->tables.g_r = ceil(radius);
    desc->tables.o_r = ceil(thickness);
    desc->tables.g_w = 2*desc->tables.g_r+1;
    desc->tables.o_w = 2*desc->tables.o_r+1;
    desc->tables.o_size = desc->tables.o_w * desc->tables.o_w;

//    fprintf(stderr, "o_r = %d\n", desc->tables.o_r);

    if (desc->tables.g_r) {
	desc->tables.g = malloc(desc->tables.g_w * sizeof(unsigned));
	desc->tables.gt2 = malloc(256 * desc->tables.g_w * sizeof(unsigned));
	if (desc->tables.g==NULL || desc->tables.gt2==NULL) {
	    return -1;
	}
    }
    desc->tables.om = malloc(desc->tables.o_w*desc->tables.o_w * sizeof(unsigned));
    desc->tables.omt = malloc(desc->tables.o_size*256);

    omtp = desc->tables.omt;
    desc->tables.tmp = malloc((width+1)*height*sizeof(short));
    
    if (desc->tables.om==NULL || desc->tables.omt==NULL || desc->tables.tmp==NULL) {
	return -1;
    };

    if (desc->tables.g_r) {
	// gaussian curve with volume = 256
	for (volume_diff=10000000; volume_diff>0.0000001; volume_diff*=0.5){
	    volume_factor+= volume_diff;
	    desc->tables.volume=0;
	    for (i = 0; i<desc->tables.g_w; ++i) {
		desc->tables.g[i] = (unsigned)(exp(A * (i-desc->tables.g_r)*(i-desc->tables.g_r)) * volume_factor + .5);
		desc->tables.volume+= desc->tables.g[i];
	    }
	    if(desc->tables.volume>256) volume_factor-= volume_diff;
	}
	desc->tables.volume=0;
	for (i = 0; i<desc->tables.g_w; ++i) {
	    desc->tables.g[i] = (unsigned)(exp(A * (i-desc->tables.g_r)*(i-desc->tables.g_r)) * volume_factor + .5);
	    desc->tables.volume+= desc->tables.g[i];
	}

	// gauss table:
	for(mx=0;mx<desc->tables.g_w;mx++){
	    for(i=0;i<256;i++){
		desc->tables.gt2[mx+i*desc->tables.g_w] = i*desc->tables.g[mx];
	    }
	}
    }
    
    /* outline matrix */
    for (my = 0; my<desc->tables.o_w; ++my) {
	for (mx = 0; mx<desc->tables.o_w; ++mx) {
	    // antialiased circle would be perfect here, but this one is good enough
	    double d = thickness + 1 - sqrt((mx-desc->tables.o_r)*(mx-desc->tables.o_r)+(my-desc->tables.o_r)*(my-desc->tables.o_r));
	    desc->tables.om[mx+my*desc->tables.o_w] = d>=1 ? base : d<=0 ? 0 : (d*base + .5);
	}
    }

    // outline table:
    for(i=0;i<256;i++){
	for(mx=0;mx<desc->tables.o_size;mx++) *(omtp++) = (i*desc->tables.om[mx] + (base/2))/base;
    }

    return 0;
}

#ifdef CONFIG_ICONV
/* decode from 'encoding' to unicode */
static FT_ULong decode_char(iconv_t *cd, char c) {
    FT_ULong o;
    char *inbuf = &c;
    char *outbuf = (char*)&o;
    size_t inbytesleft = 1;
    size_t outbytesleft = sizeof(FT_ULong);

    iconv(*cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    /* convert unicode BigEndian -> MachineEndian */
    o = be2me_32(o);

    // if (count==-1) o = 0; // not OK, at least my iconv() returns E2BIG for all
    if (outbytesleft!=0) o = 0;

    /* we don't want control characters */
    if (o>=0x7f && o<0xa0) o = 0;
    return o;
}

static int prepare_charset(char *charmap, char *encoding, FT_ULong *charset, FT_ULong *charcodes) {
    FT_ULong i;
    int count = 0;
    int charset_size;
    iconv_t cd;
    
    // check if ucs-4 is available
    cd = iconv_open(charmap, charmap);
    if (cd==(iconv_t)-1) {
	mp_msg(MSGT_OSD, MSGL_ERR, "iconv doesn't know %s encoding. Use the source!\n", charmap);
	return -1;
    }
    
    iconv_close(cd);
    
    cd = iconv_open(charmap, encoding);
    if (cd==(iconv_t)-1) {
	mp_msg(MSGT_OSD, MSGL_ERR, "Unsupported encoding `%s', use iconv --list to list character sets known on your system.\n", encoding);
	return -1;
    }
    
    charset_size = 256 - first_char;
    for (i = 0; i<charset_size; ++i) {
	charcodes[count] = i+first_char;
	charset[count] = decode_char(&cd, i+first_char);
	if (charset[count]!=0) ++count;
    }
    charcodes[count] = charset[count] = 0; ++count;
    charset_size = count;

    iconv_close(cd);
    if (charset_size==0) {
	mp_msg(MSGT_OSD, MSGL_ERR, "No characters to render!\n");
	return -1;
    }
    
    return charset_size;
}

static int prepare_charset_unicode(FT_Face face, FT_ULong *charset, FT_ULong *charcodes) {
#ifdef HAVE_FREETYPE21
    FT_ULong  charcode;
#else
    int j;
#endif
    FT_UInt   gindex;
    int i;

    if (face->charmap==NULL || face->charmap->encoding!=ft_encoding_unicode) {
	WARNING("Unicode charmap not available for this font. Very bad!");
	return -1;
    }
#ifdef HAVE_FREETYPE21
    i = 0;
    charcode = FT_Get_First_Char( face, &gindex );
    while (gindex != 0) {
	if (charcode < 65536 && charcode >= 33) { // sanity check
	    charset[i] = charcode;
	    charcodes[i] = 0;
	    i++;
	}
	charcode = FT_Get_Next_Char( face, charcode, &gindex );
    }
#else
    // for FT < 2.1 we have to use brute force enumeration
    i = 0;
    for (j = 33; j < 65536; j++) {
	gindex = FT_Get_Char_Index(face, j);
	if (gindex > 0) {
	    charset[i] = j;
	    charcodes[i] = 0;
	    i++;
	}
    }
#endif
    mp_msg(MSGT_OSD, MSGL_V, "Unicode font: %d glyphs.\n", i);

    return i;
}
#endif

static font_desc_t* init_font_desc(void)
{
    font_desc_t *desc;
    int i;

    desc = malloc(sizeof(font_desc_t));
    if(!desc) return NULL;
    memset(desc,0,sizeof(font_desc_t));

    desc->dynamic = 1;
    
    /* setup sane defaults */
    desc->name = NULL;
    desc->fpath = NULL;

    desc->face_cnt = 0;
    desc->charspace = 0;
    desc->spacewidth = 0;
    desc->height = 0;
    desc->max_width = 0;
    desc->max_height = 0;
    desc->freetype = 1;

    desc->tables.g = NULL;
    desc->tables.gt2 = NULL;
    desc->tables.om = NULL;
    desc->tables.omt = NULL;
    desc->tables.tmp = NULL;

    for(i = 0; i < 65536; i++)
	desc->start[i] = desc->width[i] = desc->font[i] = -1;
    for(i = 0; i < 16; i++)
	desc->pic_a[i] = desc->pic_b[i] = NULL;
    
    return desc;
}

void free_font_desc(font_desc_t *desc)
{
    int i;
    
    if (!desc) return;

//    if (!desc->dynamic) return; // some vo_aa crap, better leaking than crashing

    if (desc->name) free(desc->name);
    if (desc->fpath) free(desc->fpath);
    
    for(i = 0; i < 16; i++) {
	if (desc->pic_a[i]) {
	    if (desc->pic_a[i]->bmp) free(desc->pic_a[i]->bmp);
	    if (desc->pic_a[i]->pal) free(desc->pic_a[i]->pal);
	    free (desc->pic_a[i]);
	}
	if (desc->pic_b[i]) {
	    if (desc->pic_b[i]->bmp) free(desc->pic_b[i]->bmp);
	    if (desc->pic_b[i]->pal) free(desc->pic_b[i]->pal);
	    free (desc->pic_b[i]);
	}
    }

    if (desc->tables.g) free(desc->tables.g);
    if (desc->tables.gt2) free(desc->tables.gt2);
    if (desc->tables.om) free(desc->tables.om);
    if (desc->tables.omt) free(desc->tables.omt);
    if (desc->tables.tmp) free(desc->tables.tmp);

    for(i = 0; i < desc->face_cnt; i++) {
	FT_Done_Face(desc->faces[i]);
    }
    
    free(desc);
}

static int load_sub_face(const char *name, int face_index, FT_Face *face)
{
    int err = -1;
    
    if (name) err = FT_New_Face(library, name, face_index, face);

    if (err) {
	char *font_file = get_path("subfont.ttf");
	err = FT_New_Face(library, font_file, 0, face);
	free(font_file);
	if (err) {
	    err = FT_New_Face(library, MPLAYER_DATADIR "/subfont.ttf", 0, face);
	    if (err) {
	        mp_msg(MSGT_OSD, MSGL_ERR, MSGTR_LIBVO_FONT_LOAD_FT_NewFaceFailed);
		return -1;
	    }
	}
    }
    return err;
}

static int load_osd_face(FT_Face *face)
{
    if ( FT_New_Memory_Face(library, osd_font_pfb, sizeof(osd_font_pfb), 0, face) ) {
	mp_msg(MSGT_OSD, MSGL_ERR, MSGTR_LIBVO_FONT_LOAD_FT_NewMemoryFaceFailed);
	return -1;
    }
    return 0;
}

int kerning(font_desc_t *desc, int prevc, int c)
{
    FT_Vector kern;
    
    if (!desc->dynamic) return 0;
    if (prevc < 0 || c < 0) return 0;
    if (desc->font[prevc] != desc->font[c]) return 0;
    if (desc->font[prevc] == -1 || desc->font[c] == -1) return 0;
    FT_Get_Kerning(desc->faces[desc->font[c]], 
		   desc->glyph_index[prevc], desc->glyph_index[c],
		   ft_kerning_default, &kern);

//    fprintf(stderr, "kern: %c %c %d\n", prevc, c, f266ToInt(kern.x));

    return f266ToInt(kern.x);
}

font_desc_t* read_font_desc_ft(const char *fname, int face_index, int movie_width, int movie_height, float font_scale_factor)
{
    font_desc_t *desc = NULL;

    FT_Face face;

    FT_ULong *my_charset = malloc(MAX_CHARSET_SIZE * sizeof(FT_ULong)); /* characters we want to render; Unicode */
    FT_ULong *my_charcodes = malloc(MAX_CHARSET_SIZE * sizeof(FT_ULong)); /* character codes in 'encoding' */

    char *charmap = "ucs-4";
    int err;
    int charset_size;
    int i, j;
    int unicode;
    
    float movie_size;

    float subtitle_font_ppem;
    float osd_font_ppem;

    if (my_charset == NULL || my_charcodes == NULL) {
	mp_msg(MSGT_OSD, MSGL_ERR, "subtitle font: malloc failed.\n");
	goto err_out;
    }

    switch (subtitle_autoscale) {
    case 1:
	movie_size = movie_height;
	break;
    case 2:
	movie_size = movie_width;
	break;
    case 3:
	movie_size = sqrt(movie_height*movie_height+movie_width*movie_width);
	break;
    default:
	movie_size = 100;
	break;
    }

    subtitle_font_ppem = movie_size*font_scale_factor/100.0;
    osd_font_ppem = movie_size*(font_scale_factor+1)/100.0;

    if (subtitle_font_ppem < 5) subtitle_font_ppem = 5;
    if (osd_font_ppem < 5) osd_font_ppem = 5;

    if (subtitle_font_ppem > 128) subtitle_font_ppem = 128;
    if (osd_font_ppem > 128) osd_font_ppem = 128;

    if ((subtitle_font_encoding == NULL)
	|| (strcasecmp(subtitle_font_encoding, "unicode") == 0)) {
	unicode = 1;
    } else {
	unicode = 0;
    }

    desc = init_font_desc();
    if(!desc) goto err_out;

//    t=GetTimer();

    /* generate the subtitle font */
    err = load_sub_face(fname, face_index, &face);
    if (err) {
	mp_msg(MSGT_OSD, MSGL_WARN, MSGTR_LIBVO_FONT_LOAD_FT_SubFaceFailed);
	goto gen_osd;
    }
    desc->face_cnt++;

#ifdef CONFIG_ICONV
    if (unicode) {
	charset_size = prepare_charset_unicode(face, my_charset, my_charcodes);
    } else {
	if (subtitle_font_encoding) {
	    charset_size = prepare_charset(charmap, subtitle_font_encoding, my_charset, my_charcodes);
	} else {
	    charset_size = prepare_charset(charmap, "iso-8859-1", my_charset, my_charcodes);
	}
    }

    if (charset_size < 0) {
	mp_msg(MSGT_OSD, MSGL_ERR, MSGTR_LIBVO_FONT_LOAD_FT_SubFontCharsetFailed);
	goto err_out;
    }
#else
    goto err_out;
#endif

//    fprintf(stderr, "fg: prepare t = %lf\n", GetTimer()-t);

    err = prepare_font(desc, face, subtitle_font_ppem, desc->face_cnt-1,
		       charset_size, my_charset, my_charcodes, unicode,
		       subtitle_font_thickness, subtitle_font_radius);

    if (err) {
	mp_msg(MSGT_OSD, MSGL_ERR, MSGTR_LIBVO_FONT_LOAD_FT_CannotPrepareSubtitleFont);
	goto err_out;
    }

gen_osd:

    /* generate the OSD font */
    err = load_osd_face(&face);
    if (err) {
	goto err_out;
    }
    desc->face_cnt++;

    err = prepare_font(desc, face, osd_font_ppem, desc->face_cnt-1,
		       OSD_CHARSET_SIZE, osd_charset, osd_charcodes, 0,
		       subtitle_font_thickness, subtitle_font_radius);
    
    if (err) {
	mp_msg(MSGT_OSD, MSGL_ERR, MSGTR_LIBVO_FONT_LOAD_FT_CannotPrepareOSDFont);
	goto err_out;
    }

    err = generate_tables(desc, subtitle_font_thickness, subtitle_font_radius);
    
    if (err) {
	mp_msg(MSGT_OSD, MSGL_ERR, MSGTR_LIBVO_FONT_LOAD_FT_CannotGenerateTables);
	goto err_out;
    }

    // final cleanup
    desc->font[' ']=-1;
    desc->width[' ']=desc->spacewidth;

    j = '_';
    if (desc->font[j] < 0) j = '?';
    if (desc->font[j] < 0) j = ' ';
    render_one_glyph(desc, j);
    for(i = 0; i < 65536; i++) {
	if (desc->font[i] < 0 && i != ' ') {
	    desc->start[i] = desc->start[j];
	    desc->width[i] = desc->width[j];
	    desc->font[i] = desc->font[j];
	}
    }
    free(my_charset);
    free(my_charcodes);
    return desc;

err_out:
    if (desc)
      free_font_desc(desc);
    free(my_charset);
    free(my_charcodes);
    return NULL;
}

int init_freetype(void)
{
    int err;
    
    /* initialize freetype */
    err = FT_Init_FreeType(&library);
    if (err) {
	mp_msg(MSGT_OSD, MSGL_ERR, "Init_FreeType failed.\n");
	return -1;
    }
    mp_msg(MSGT_OSD, MSGL_V, "init_freetype\n");
    using_freetype = 1;
    return 0;
}

int done_freetype(void)
{
    int err;

    if (!using_freetype)
	return 0;
    
    err = FT_Done_FreeType(library);
    if (err) {
	mp_msg(MSGT_OSD, MSGL_ERR, MSGTR_LIBVO_FONT_LOAD_FT_DoneFreeTypeFailed);
	return -1;
    }

    return 0;
}

void load_font_ft(int width, int height, font_desc_t** fontp, const char *font_name, float font_scale_factor)
{
#ifdef CONFIG_FONTCONFIG
    FcPattern *fc_pattern;
    FcPattern *fc_pattern2;
    FcChar8 *s;
    int face_index;
    FcBool scalable;
#endif
    font_desc_t *vo_font = *fontp;
    vo_image_width = width;
    vo_image_height = height;

    // protection against vo_aa font hacks
    if (vo_font && !vo_font->dynamic) return;

    if (vo_font) free_font_desc(vo_font);

#ifdef CONFIG_FONTCONFIG
    if (font_fontconfig > 0)
    {
	if (!font_name)
	    font_name = strdup("sans-serif");
	FcInit();
	fc_pattern = FcNameParse(font_name);
	FcConfigSubstitute(0, fc_pattern, FcMatchPattern);
	FcDefaultSubstitute(fc_pattern);
	fc_pattern2 = fc_pattern;
	fc_pattern = FcFontMatch(0, fc_pattern, 0);
	FcPatternDestroy(fc_pattern2);
	FcPatternGetBool(fc_pattern, FC_SCALABLE, 0, &scalable);
	if (scalable != FcTrue) {
	    FcPatternDestroy(fc_pattern);
    	    fc_pattern = FcNameParse("sans-serif");
    	    FcConfigSubstitute(0, fc_pattern, FcMatchPattern);
    	    FcDefaultSubstitute(fc_pattern);
	    fc_pattern2 = fc_pattern;
    	    fc_pattern = FcFontMatch(0, fc_pattern, 0);
	    FcPatternDestroy(fc_pattern2);
	}
	// s doesn't need to be freed according to fontconfig docs
	FcPatternGetString(fc_pattern, FC_FILE, 0, &s);
	FcPatternGetInteger(fc_pattern, FC_INDEX, 0, &face_index);
	*fontp=read_font_desc_ft(s, face_index, width, height, font_scale_factor);
	FcPatternDestroy(fc_pattern);
    }
    else
#endif
    *fontp=read_font_desc_ft(font_name, 0, width, height, font_scale_factor);
}
