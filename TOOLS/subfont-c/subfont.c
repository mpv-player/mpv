/*
 * Renders antialiased fonts for mplayer using freetype library.
 * Should work with TrueType, Type1 and any other font supported by libfreetype.
 * Can generate font.desc for any encoding.
 *
 *
 * Artur Zaprzala <zybi@fanthom.irc.pl>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <iconv.h>
#include <math.h>
#include <string.h>
#include <libgen.h>


#ifndef OLD_FREETYPE2
#include <ft2build.h>	
#include FT_FREETYPE_H
#include FT_GLYPH_H
#else			/* freetype 2.0.1 */
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#endif


#include "../../bswap.h"


#ifndef DEBUG
#define DEBUG	0
#endif


//// default values
char		*encoding = "iso-8859-1";	/* target encoding */
char		*charmap = "ucs-4";		/* font charmap encoding, I hope ucs-4 is always big endian */
						/* gcc 2.1.3 doesn't support ucs-4le, but supports ucs-4 (==ucs-4be) */
float		ppem = 22;			/* font size in pixels */

double		radius = 2;			/* blur radius */
double		thickness = 1.5;		/* outline thickness */

char*		font_desc = "font.desc";
//char*		font_desc = "/dev/stdout";

char		*outdir = ".";

//// constants
int const	colors = 256;
int const	maxcolor = 255;
unsigned const	base = 256;
unsigned const	first_char = 33;
#define max_charset_size	60000
//int const	max_charset_size = 256;
unsigned	charset_size = 0;

////
char		*command;
char		*encoding_name;
char		*font_path;
//char		*font_metrics;
int		append_mode = 0;
int		unicode_desc = 0;

unsigned char	*bbuffer, *abuffer;
int		width, height;
int		padding;
static FT_ULong	charset[max_charset_size];		/* characters we want to render; Unicode */
static FT_ULong	charcodes[max_charset_size];	/* character codes in 'encoding' */
iconv_t cd;					// iconv conversion descriptor



#define eprintf(...)		fprintf(stderr, __VA_ARGS__)
#define ERROR_(msg, ...)	(eprintf("%s: error: " msg "\n", command, __VA_ARGS__), exit(1))
#define WARNING_(msg, ...)	eprintf("%s: warning: " msg "\n", command, __VA_ARGS__)
#define ERROR(...)		ERROR_(__VA_ARGS__, NULL)
#define WARNING(...)		WARNING_(__VA_ARGS__, NULL)


#define f266ToInt(x)		(((x)+32)>>6)	// round fractional fixed point number to integer
						// coordinates are in 26.6 pixels (i.e. 1/64th of pixels)
#define f266CeilToInt(x)	(((x)+63)>>6)	// ceiling
#define f266FloorToInt(x)	((x)>>6)	// floor
#define f1616ToInt(x)		(((x)+0x8000)>>16)	// 16.16
#define floatTof266(x)		((int)((x)*(1<<6)+0.5))

#define ALIGN(x)		(((x)+7)&~7)	// 8 byte align



void paste_bitmap(FT_Bitmap *bitmap, int x, int y) {
    int drow = x+y*width;
    int srow = 0;
    int sp, dp, w, h;
    if (bitmap->pixel_mode==ft_pixel_mode_mono)
	for (h = bitmap->rows; h>0; --h, drow+=width, srow+=bitmap->pitch)
	    for (w = bitmap->width, sp=dp=0; w>0; --w, ++dp, ++sp)
		    bbuffer[drow+dp] = (bitmap->buffer[srow+sp/8] & (0x80>>(sp%8))) ? 255:0;
    else
	for (h = bitmap->rows; h>0; --h, drow+=width, srow+=bitmap->pitch)
	    for (w = bitmap->width, sp=dp=0; w>0; --w, ++dp, ++sp)
		    bbuffer[drow+dp] = bitmap->buffer[srow+sp];
}


void write_header(FILE *f) {
    static unsigned char   header[800] = "mhwanh";
    int i;
    header[7] = 4;
    if (width < 0x10000) { // are two bytes enough for the width?
    header[8] = width>>8;	header[9] = (unsigned char)width;
    } else {               // store width using 4 bytes at the end of the header
	    header[8] = header[9] = 0;
	    header[28] = (width >> 030) & 0xFF;
	    header[29] = (width >> 020) & 0xFF;
	    header[30] = (width >> 010) & 0xFF;
	    header[31] = (width       ) & 0xFF;
    }
    header[10] = height>>8;	header[11] = (unsigned char)height;
    header[12] = colors>>8;	header[13] = (unsigned char)colors;
    for (i = 32; i<800; ++i) header[i] = (i-32)/3;
    fwrite(header, 1, 800, f);
}


void write_bitmap(void *buffer, char type) {
    FILE *f;
    int const max_name = 128;
    char name[max_name];

    snprintf(name, max_name, "%s/%s-%c.raw", outdir, encoding_name, type);
    f = fopen(name, "wb");
    if (f==NULL) ERROR("fopen failed.");
    write_header(f);
    fwrite(buffer, 1, width*height, f);
    fclose(f);
}


void render() {
    FT_Library	library;
    FT_Face	face;
    FT_Error	error;
    FT_Glyph	*glyphs;
    FT_BitmapGlyph glyph;
    FILE	*f;
    int	const	load_flags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING;
    int		pen_x = 0, pen_xa;
    int		ymin = INT_MAX, ymax = INT_MIN;
    int		i, uni_charmap = 1;
    int		baseline, space_advance = 20;
    int		glyphs_count = 0;


    /* initialize freetype */
    error = FT_Init_FreeType(&library);
    if (error) ERROR("Init_FreeType failed.");
    error = FT_New_Face(library, font_path, 0, &face);
    if (error) ERROR("New_Face failed. Maybe the font path `%s' is wrong.", font_path);

    /*
    if (font_metrics) {
	error = FT_Attach_File(face, font_metrics);
	if (error) WARNING("FT_Attach_File failed.");
    }
    */


#if 0
    /************************************************************/
    eprintf("Font encodings:\n");
    for (i = 0; i<face->num_charmaps; ++i)
	eprintf("'%.4s'\n", (char*)&face->charmaps[i]->encoding);

    //error = FT_Select_Charmap(face, ft_encoding_unicode);
    //error = FT_Select_Charmap(face, ft_encoding_adobe_standard);
    //error = FT_Select_Charmap(face, ft_encoding_adobe_custom);
    //error = FT_Set_Charmap(face, face->charmaps[1]);
    //if (error) WARNING("FT_Select_Charmap failed.");
#endif


#if 0
    /************************************************************/
    if (FT_HAS_GLYPH_NAMES(face)) {
	int const max_gname = 128;
	char gname[max_gname];
	for (i = 0; i<face->num_glyphs; ++i) {
	    FT_Get_Glyph_Name(face, i, gname, max_gname);
	    eprintf("%02x `%s'\n", i, gname);
	}

    }
#endif


    if (face->charmap==NULL || face->charmap->encoding!=ft_encoding_unicode) {
	WARNING("Unicode charmap not available for this font. Very bad!");
	uni_charmap = 0;
	error = FT_Set_Charmap(face, face->charmaps[0]);
	if (error) WARNING("No charmaps! Strange.");
    }



    /* set size */
    if (FT_IS_SCALABLE(face)) {
	error = FT_Set_Char_Size(face, floatTof266(ppem), 0, 0, 0);
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


    /* create font.desc */
{
    int const max_name = 128;
    char name[max_name];

    snprintf(name, max_name, "%s/%s", outdir, font_desc);
    f = fopen(name, append_mode ? "a":"w");
}
    if (f==NULL) ERROR("fopen failed.");


    /* print font.desc header */
    if (append_mode) {
	fprintf(f, "\n\n# ");
    } else {
	fprintf(f,  "# This file was generated with subfont for Mplayer.\n"
		    "# Subfont by Artur Zaprzala <zybi@fanthom.irc.pl>.\n\n");
	fprintf(f, "[info]\n");
    }

    fprintf(f, "name 'Subtitle font for %s %s, \"%s%s%s\" face, size: %.1f pixels.'\n",
	    encoding_name,
	    unicode_desc ? "charset, Unicode encoding":"encoding",
	    face->family_name ? face->family_name : font_path,
	    face->style_name ? " ":"", face->style_name ? face->style_name:"",
	    ppem);

    if (!append_mode) {
#ifdef NEW_DESC
	fprintf(f, "descversion 2\n");
#else
	fprintf(f, "descversion 1\n");
#endif
	fprintf(f, "spacewidth %i\n",	2*padding + space_advance);
#ifndef NEW_DESC
	fprintf(f, "charspace %i\n",	-2*padding);
#endif
	fprintf(f, "height %i\n",	f266ToInt(face->size->metrics.height));
#ifdef NEW_DESC
	fprintf(f, "ascender %i\n",	f266CeilToInt(face->size->metrics.ascender));
	fprintf(f, "descender %i\n",	f266FloorToInt(face->size->metrics.descender));
#endif
    }
    fprintf(f, "\n[files]\n");
    fprintf(f, "alpha %s-a.raw\n",	encoding_name);
    fprintf(f, "bitmap %s-b.raw\n",	encoding_name);
    fprintf(f, "\n[characters]\n");


    // render glyphs, compute bitmap size and [characters] section
    glyphs = (FT_Glyph*)malloc(charset_size*sizeof(FT_Glyph*));
    for (i= 0; i<charset_size; ++i) {
	FT_GlyphSlot	slot;
	FT_ULong	character, code;
	FT_UInt		glyph_index;
	FT_BBox		bbox;

	character = charset[i];
	code = charcodes[i];

	// get glyph index
	if (character==0)
	    glyph_index = 0;
	else {
	    glyph_index = FT_Get_Char_Index(face, uni_charmap ? character:code);
	    if (glyph_index==0) {
		WARNING("Glyph for char 0x%02x|U+%04X|%c not found.", code, character,
			 code<' '||code>255 ? '.':code);
		continue;
	    }
	}

	// load glyph
	error = FT_Load_Glyph(face, glyph_index, load_flags);
	if (error) {
	    WARNING("FT_Load_Glyph 0x%02x (char 0x%02x|U+%04X) failed.", glyph_index, code, character);
	    continue;
	}
	slot = face->glyph;

	// render glyph
	if (slot->format != ft_glyph_format_bitmap) {
	    error = FT_Render_Glyph(slot, ft_render_mode_normal);
	    if (error) {
		WARNING("FT_Render_Glyph 0x%04x (char 0x%02x|U+%04X) failed.", glyph_index, code, character);
		continue;
	    }
	}

	// extract glyph image
	error = FT_Get_Glyph(slot, (FT_Glyph*)&glyph);
	if (error) {
	    WARNING("FT_Get_Glyph 0x%04x (char 0x%02x|U+%04X) failed.", glyph_index, code, character);
	    continue;
	}
	glyphs[glyphs_count++] = (FT_Glyph)glyph;

#ifdef NEW_DESC
	// max height
	if (glyph->bitmap.rows > height) height = glyph->bitmap.rows;

	// advance pen
	pen_xa = pen_x + glyph->bitmap.width + 2*padding;

	// font.desc
	fprintf(f, "0x%04x %i %i %i %i %i %i;\tU+%04X|%c\n", unicode_desc ? character:code,
		pen_x,						// bitmap start
		glyph->bitmap.width + 2*padding,		// bitmap width
		glyph->bitmap.rows + 2*padding,			// bitmap height
		glyph->left - padding,				// left bearing
		glyph->top + padding,				// top bearing
		f266ToInt(slot->advance.x),			// advance
		character, code<' '||code>255 ? '.':code);
#else
	// max height
	if (glyph->top > ymax) {
	    ymax = glyph->top;
	    //eprintf("%3i: ymax %i (%c)\n", code, ymax, code);
	}
	if (glyph->top - glyph->bitmap.rows < ymin) {
	    ymin = glyph->top - glyph->bitmap.rows;
	    //eprintf("%3i: ymin %i (%c)\n", code, ymin, code);
	}

	/* advance pen */
	pen_xa = pen_x + f266ToInt(slot->advance.x) + 2*padding;

	/* font.desc */
	fprintf(f, "0x%04x %i %i;\tU+%04X|%c\n", unicode_desc ? character:code,
		pen_x,						// bitmap start
		pen_xa-1,					// bitmap end
		character, code<' '||code>255 ? '.':code);
#endif
	pen_x = ALIGN(pen_xa);
    }


    width = pen_x;
    pen_x = 0;
#ifdef NEW_DESC
    if (height<=0) ERROR("Something went wrong. Use the source!");
    height += 2*padding;
#else
    if (ymax<=ymin) ERROR("Something went wrong. Use the source!");
    height = ymax - ymin + 2*padding;
    baseline = ymax + padding;
#endif

    // end of font.desc
    if (DEBUG) eprintf("bitmap size: %ix%i\n", width, height);
    fprintf(f, "# bitmap size: %ix%i\n", width, height);
    fclose(f);

    bbuffer = (unsigned char*)malloc(width*height);
    if (bbuffer==NULL) ERROR("malloc failed.");
    memset(bbuffer, 0, width*height);


    /* paste glyphs */
    for (i= 0; i<glyphs_count; ++i) {
	glyph = (FT_BitmapGlyph)glyphs[i];
#ifdef NEW_DESC
	paste_bitmap(&glyph->bitmap,
	    pen_x + padding,
	    padding);

	/* advance pen */
	pen_x += glyph->bitmap.width + 2*padding;
#else
	paste_bitmap(&glyph->bitmap,
	    pen_x + padding + glyph->left,
	    baseline - glyph->top);

	/* advance pen */
	pen_x += f1616ToInt(glyph->root.advance.x) + 2*padding;
#endif
	pen_x = ALIGN(pen_x);

	FT_Done_Glyph((FT_Glyph)glyph);
    }
    free(glyphs);


    error = FT_Done_FreeType(library);
    if (error) ERROR("FT_Done_FreeType failed.");
}


/* decode from 'encoding' to unicode */
FT_ULong decode_char(char c) {
    FT_ULong o;
    char *inbuf = &c;
    char *outbuf = (char*)&o;
    int inbytesleft = 1;
    int outbytesleft = sizeof(FT_ULong);

    size_t count = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    /* convert unicode BigEndian -> MachineEndian */
    o = be2me_32(o);

    // if (count==-1) o = 0; // not OK, at least my iconv() returns E2BIG for all
    if (outbytesleft!=0) o = 0;

    /* we don't want control characters */
    if (o>=0x7f && o<0xa0) o = 0;
    return o;
}


void prepare_charset() {
    FILE *f;
    FT_ULong i;

    f = fopen(encoding, "r");		// try to read custom encoding
    if (f==NULL) {
	int count = 0;
	// check if ucs-4 is available
	cd = iconv_open(charmap, charmap);
	if (cd==(iconv_t)-1) ERROR("iconv doesn't know %s encoding. Use the source!", charmap);
	iconv_close(cd);

	cd = iconv_open(charmap, encoding);
	if (cd==(iconv_t)-1) ERROR("Unsupported encoding `%s', use iconv --list to list character sets known on your system.", encoding);

	charset_size = 256 - first_char;
	for (i = 0; i<charset_size; ++i) {
	    charcodes[count] = i+first_char;
	    charset[count] = decode_char(i+first_char);
	    //eprintf("%04X U%04X\n", charcodes[count], charset[count]);
	    if (charset[count]!=0) ++count;
	}
	charcodes[count] = charset[count] = 0; ++count;
	charset_size = count;

	iconv_close(cd);
    } else {
	unsigned int character, code;
	int count;

	eprintf("Reading custom encoding from file '%s'.\n", encoding);

       	while ((count = fscanf(f, "%x%*[ \t]%x", &character, &code)) != EOF) {
	    if (charset_size==max_charset_size) {
		WARNING("There is no place for  more than %i characters. Use the source!", max_charset_size);
		break;
	    }
	    if (count==0) ERROR("Unable to parse custom encoding file.");
	    if (character<32) continue;	// skip control characters
	    charset[charset_size] = character;
	    charcodes[charset_size] = count==2 ? code : character;
	    ++charset_size;
	}
	fclose(f);
//	encoding = basename(encoding);
    }
    if (charset_size==0) ERROR("No characters to render!");
}


// general outline
void outline(
	unsigned char *s,
	unsigned char *t,
	int width,
	int height,
	int *m,
	int r,
	int mwidth) {

    int x, y;
    for (y = 0; y<height; ++y) {
	for (x = 0; x<width; ++x, ++s, ++t) {
	    unsigned max = 0;
	    unsigned *mrow = m + r;
	    unsigned char *srow = s -r*width;
	    int x1=(x<r)?-x:-r;
	    int x2=(x+r>=width)?(width-x-1):r;
	    int my;

	    for (my = -r; my<=r; ++my, srow+= width, mrow+= mwidth) {
		int mx;
		if (y+my < 0) continue;
		if (y+my >= height) break;

		for (mx = x1; mx<=x2; ++mx) {
		    unsigned v = srow[mx] * mrow[mx];
		    if (v>max) max = v;
		}
	    }
	    *t = (max + base/2) / base;
	}
    }
}


// 1 pixel outline
void outline1(
	unsigned char *s,
	unsigned char *t,
	int width,
	int height) {

    int x, y, mx, my;

    for (x = 0; x<width; ++x, ++s, ++t) *t = *s;
    for (y = 1; y<height-1; ++y) {
	*t++ = *s++;
	for (x = 1; x<width-1; ++x, ++s, ++t) {
	    unsigned v = (
		    s[-1-width]+
		    s[-1+width]+
		    s[+1-width]+
		    s[+1+width]
		)/2 + (
		    s[-1]+
		    s[+1]+
		    s[-width]+
		    s[+width]+
		    s[0]
		);
	    *t = v>maxcolor ? maxcolor : v;
	}
	*t++ = *s++;
    }
    for (x = 0; x<width; ++x, ++s, ++t) *t = *s;
}


// gaussian blur
void blur(
	unsigned char *buffer,
	unsigned char *tmp,
	int width,
	int height,
	int *m,
	int r,
	int mwidth,
	unsigned volume) {

    int x, y;

    unsigned char *s = buffer - r;
    unsigned char *t = tmp;
    for (y = 0; y<height; ++y) {
	for (x = 0; x<width; ++x, ++s, ++t) {
	    unsigned sum = 0;
	    int x1 = (x<r) ? r-x:0;
	    int x2 = (x+r>=width) ? (r+width-x):mwidth;
	    int mx;
	    for (mx = x1; mx<x2; ++mx)
		sum+= s[mx] * m[mx];
	    *t = (sum + volume/2) / volume;
	    //*t = sum;
	}
    }
    tmp -= r*width;
    for (x = 0; x<width; ++x, ++tmp, ++buffer) {
	s = tmp;
	t = buffer;
	for (y = 0; y<height; ++y, s+= width, t+= width) {
	    unsigned sum = 0;
	    int y1 = (y<r) ? r-y:0;
	    int y2 = (y+r>=height) ? (r+height-y):mwidth;
	    unsigned char *smy = s + y1*width;
	    int my;
	    for (my = y1; my<y2; ++my, smy+= width)
		sum+= *smy * m[my];
	    *t = (sum + volume/2) / volume;
	}
    }
}


// Gaussian matrix
// Maybe for future use.
unsigned gmatrix(unsigned *m, int r, int w, double const A) {
    unsigned volume = 0;		// volume under Gaussian area is exactly -pi*base/A
    int mx, my;

    for (my = 0; my<w; ++my) {
	for (mx = 0; mx<w; ++mx) {
	    m[mx+my*w] = (unsigned)(exp(A * ((mx-r)*(mx-r)+(my-r)*(my-r))) * base + .5);
	    volume+= m[mx+my*w];
	    if (DEBUG) eprintf("%3i ", m[mx+my*w]);
	}
	if (DEBUG) eprintf("\n");
    }
    if (DEBUG) {
	eprintf("A= %f\n", A);
	eprintf("volume: %i; exact: %.0f; volume/exact: %.6f\n\n", volume, -M_PI*base/A, volume/(-M_PI*base/A));
    }
    return volume;
}


void alpha() {
    int const g_r = ceil(radius);
    int const o_r = ceil(thickness);
    int const g_w = 2*g_r+1;		// matrix size
    int const o_w = 2*o_r+1;		// matrix size
    double const A = log(1.0/base)/(radius*radius*2);

    int mx, my, i;
    unsigned volume = 0;		// volume under Gaussian area is exactly -pi*base/A

    unsigned *g = (unsigned*)malloc(g_w * sizeof(unsigned));
    unsigned *om = (unsigned*)malloc(o_w*o_w * sizeof(unsigned));
    if (g==NULL || om==NULL) ERROR("malloc failed.");

    // gaussian curve
    for (i = 0; i<g_w; ++i) {
	g[i] = (unsigned)(exp(A * (i-g_r)*(i-g_r)) * base + .5);
	volume+= g[i];
	if (DEBUG) eprintf("%3i ", g[i]);
    }
    //volume *= volume;
    if (DEBUG) eprintf("\n");

    /* outline matrix */
    for (my = 0; my<o_w; ++my) {
	for (mx = 0; mx<o_w; ++mx) {
	    // antialiased circle would be perfect here, but this one is good enough
	    double d = thickness + 1 - sqrt((mx-o_r)*(mx-o_r)+(my-o_r)*(my-o_r));
	    om[mx+my*o_w] = d>=1 ? base : d<=0 ? 0 : (d*base + .5);
	    if (DEBUG) eprintf("%3i ", om[mx+my*o_w]);
	}
	if (DEBUG) eprintf("\n");
    }
    if (DEBUG) eprintf("\n");


    if(thickness==1.0)
      outline1(bbuffer, abuffer, width, height);	// FAST solid 1 pixel outline
    else
      outline(bbuffer, abuffer, width, height, om, o_r, o_w);	// solid outline

    //outline(bbuffer, abuffer, width, height, gm, g_r, g_w);	// Gaussian outline

    blur(abuffer, bbuffer, width, height, g, g_r, g_w, volume);

    free(g);
    free(om);
}


void usage() {
    printf("Usage: %s [--outdir dir] [--append] [--unicode] [--blur b] [--outline o] encoding ppem font\n", command);
    printf("\n"
	    "  Program creates 3 files: font.desc, <encoding>-a.raw, <encoding>-b.raw.\n"
	    "\n"
	    "  --outdir         output directory to place files.\n"
	    "  --append         append results to existing font.desc, suppress info header.\n"
	    "  --unicode        use Unicode in font.desc. This will work with -utf8 option of mplayer.\n"
	    "  --blur b         specify blur radius, float.\n"
	    "  --outline o      specify outline thickness, float.\n"
	    "  encoding         must be an 8 bit encoding, like iso-8859-2, or path to custom encoding file (see README).\n"
	    "                   To list encodings available on your system use iconv --list.\n"
	    "  ppem             Font size in pixels (default 24), float.\n"
	    "  font             Font file path. Any format supported by the freetype library (*.ttf, *.pfb, ...).\n"
	    );
    exit(1);
}


void parse_args(int argc, char **argv) {
    int i, a = 0;
    double d;

    command = strrchr(argv[a], '/');
    if (command==NULL) command = argv[a];
    else ++command;
    ++a; --argc;

    if (argc>=1 && strcmp(argv[a], "--outdir")==0) {
	++a; --argc;
	if (argc==0) usage();

	outdir = strdup(argv[a]);
	++a; --argc;
    }

    if (argc>=1 && strcmp(argv[a], "--append")==0) {
	append_mode = 1;
	++a; --argc;
    }

    if (argc>=1 && strcmp(argv[a], "--unicode")==0) {
	unicode_desc = 1;
	++a; --argc;
    }

    if (argc>=1 && strcmp(argv[a], "--blur")==0) {
	++a; --argc;
	if (argc==0) usage();

	d = atof(argv[a]);
	if (d>=0 && d<20) radius = d;
	else WARNING("using default blur radius.");
	++a; --argc;
    }

    if (argc>=1 && strcmp(argv[a], "--outline")==0) {
	++a; --argc;
	if (argc==0) usage();

	d = atof(argv[a]);
	if (d>=0 && d<20) thickness = d;
	else WARNING("using default outline thickness.");
	++a; --argc;
    }

    if (argc<3) usage();

    // encoding
    if (argv[a][0]!=0)
	encoding = argv[a];
    encoding_name = strrchr(encoding, '/');
    if (!encoding_name) encoding_name=encoding;
    else ++encoding_name;
    ++a; --argc;

    // ppem
    d = atof(argv[a]);
    if (d>2.) ppem = d;
    ++a; --argc;

    // font
    font_path = argv[a];
    ++a; --argc;
}


int main(int argc, char **argv) {
    parse_args(argc, argv);

    padding = ceil(radius) + ceil(thickness);

    prepare_charset();

    render();
    write_bitmap(bbuffer, 'b');

    abuffer = (unsigned char*)malloc(width*height);
    if (abuffer==NULL) ERROR("malloc failed.");
    alpha();
    write_bitmap(abuffer, 'a');

    free(bbuffer);
    free(abuffer);

//    fflush(stderr);
    return 0;
}
