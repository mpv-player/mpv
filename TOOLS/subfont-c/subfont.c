/*
 * Renders antialiased fonts for mplayer using freetype library.
 * Should work with TrueType, Type1 and any other font supported by libfreetype.
 *
 * Goals:
 *  - internationalization: supports any 8 bit encoding (uses iconv).
 *  - nice look: creates glyph `shadows' using algorithm derived from gaussian blur.
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

#ifndef DEBUG
#define DEBUG	0
#endif


//// default values
char		*encoding = "iso-8859-1";	/* target encoding */
char		*charmap = "ucs-4";		/* font charmap encoding, I hope ucs-4 is always big endian */
						/* gcc 2.1.3 doesn't support ucs-4le, but supports ucs-4 (==ucs-4be) */
int		ppem = 22;			/* font size in pixels */

double		radius = 2;			/* blur radius */
double		thickness = 1.5;		/* outline thickness */
int		padding;

char*		font_desc = "font.desc";
//char*		font_desc = "/dev/stdout";

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

unsigned char	*buffer, *abuffer;
int		width, height;
static FT_ULong	charset[max_charset_size];		/* characters we want to render; Unicode */
static FT_ULong	charcodes[max_charset_size];	/* character codes in 'encoding' */
iconv_t cd;					// iconv conversion descriptor



#define eprintf(...)		fprintf(stderr, __VA_ARGS__)
#define ERROR_(msg, ...)	(eprintf("%s: error: " msg "\n", command, __VA_ARGS__), exit(1))
#define WARNING_(msg, ...)	eprintf("%s: warning: " msg "\n", command, __VA_ARGS__)
#define ERROR(...)		ERROR_(__VA_ARGS__, NULL)
#define WARNING(...)		WARNING_(__VA_ARGS__, NULL)

#define f266toInt(x)	(((x)+32)>>6)		// round fractional fixed point number to integer
						// coordinates are in 26.6 pixels (i.e. 1/64th of pixels)
#define ALIGN(x)	(((x)+7)&~7)		// 8 byte align



void paste_bitmap(FT_Bitmap *bitmap, int x, int y) {
    int drow = x+y*width;
    int srow = 0;
    int sp, dp, w, h;
    if (bitmap->pixel_mode==ft_pixel_mode_mono)
	for (h = bitmap->rows; h>0; --h, drow+=width, srow+=bitmap->pitch)
	    for (w = bitmap->width, sp=dp=0; w>0; --w, ++dp, ++sp)
		    buffer[drow+dp] = (bitmap->buffer[srow+sp/8] & (0x80>>(sp%8))) ? 255:0;
    else
	for (h = bitmap->rows; h>0; --h, drow+=width, srow+=bitmap->pitch)
	    for (w = bitmap->width, sp=dp=0; w>0; --w, ++dp, ++sp)
		    buffer[drow+dp] = bitmap->buffer[srow+sp];
}


void write_header(FILE *f) {
    static unsigned char   header[800] = "mhwanh";
    int i;
    header[7] = 4;
    header[8] = width>>8;	header[9] = (unsigned char)width;
    header[10] = height>>8;	header[11] = (unsigned char)height;
    header[12] = colors>>8;	header[13] = (unsigned char)colors;
    for (i = 32; i<800; ++i) header[i] = (i-32)/3;
    fwrite(header, 1, 800, f);
}


void write_bitmap() {
    FILE *f;
    int const max_name = 128;
    char name[max_name];

    snprintf(name, max_name, "%s-b.raw", encoding_name);
    f = fopen(name, "wb");
    if (f==NULL) ERROR("fopen failed.");
    write_header(f);
    fwrite(buffer, 1, width*height, f);
    fclose(f);

    snprintf(name, max_name, "%s-a.raw", encoding_name);
    f = fopen(name, "wb");
    if (f==NULL) ERROR("fopen failed.");
    write_header(f);
    fwrite(abuffer, 1, width*height, f);
    fclose(f);
}


void render() {
    FT_Library	library;
    FT_Face	face;
    FT_Error	error;
    FT_GlyphSlot	slot;
    FT_ULong	glyph_index, character, code;
    //FT_Glyph	glyphs[max_charset_size];
    FT_Glyph	glyph;
    FILE	*f;
    int	const	load_flags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING;
    int		pen_x, pen_xa, pen_y, ymin, ymax;
    int		i, uni_charmap = 1;
    int		baseline, space_advance = 20;


    /* initialize freetype */
    error = FT_Init_FreeType(&library);
    if (error) ERROR("Init_FreeType failed.");
    error = FT_New_Face(library, font_path, 0, &face);
    if (error) ERROR("New_Face failed.");

    /*
    if (font_metrics) {
	error = FT_Attach_File(face, font_metrics);
	if (error) WARNING("Attach_File failed.");
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
    //if (error) WARNING("Select_Charmap failed.");
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
	error = FT_Set_Pixel_Sizes(face, ppem, ppem);
    } else {
	int j = 0;
	int jppem = face->available_sizes[0].height;
	/* find closest size */
	for (i = 0; i<face->num_fixed_sizes; ++i) {
	    if (abs(face->available_sizes[i].height - ppem) < abs(face->available_sizes[i].height - jppem)) {
		j = i;
		jppem = face->available_sizes[i].height;
	    }
	}
	WARNING("Selected font is not scalable. Using ppem=%i.", face->available_sizes[j].height);
	error = FT_Set_Pixel_Sizes(face, face->available_sizes[j].width, face->available_sizes[j].height);
    }
    if (error) WARNING("Set_Pixel_Sizes failed.");


    if (FT_IS_FIXED_WIDTH(face))
	WARNING("Selected font is fixed-width.");


    /* compute space advance */
    error = FT_Load_Char(face, ' ', load_flags);
    if (error) WARNING("spacewidth set to default.");
    else space_advance = f266toInt(face->glyph->advance.x);


    /* create font.desc */
    f = fopen(font_desc, append_mode ? "a":"w");
    if (f==NULL) ERROR("fopen failed.");

    /* print font.desc header */
    if (append_mode) {
	fprintf(f, "\n\n# Subtitle font for %s encoding, face \"%s%s%s\", ppem=%i\n",
		encoding_name,
		face->family_name,
		face->style_name ? " ":"", face->style_name ? face->style_name:"",
		ppem);
    } else {
	fprintf(f, "# This file was generated with subfont for Mplayer.\n# Subfont by Artur Zaprzala <zybi@fanthom.irc.pl>.\n\n");
	fprintf(f, "[info]\n");
	fprintf(f, "name 'Subtitle font for %s encoding, face \"%s%s%s\", ppem=%i'\n",
		encoding_name,
		face->family_name,
		face->style_name ? " ":"", face->style_name ? face->style_name:"",
		ppem);
	fprintf(f, "descversion 1\n");
	fprintf(f, "spacewidth %i\n",	2*padding + space_advance);
	fprintf(f, "charspace %i\n",	-2*padding);
	fprintf(f, "height %i\n",		f266toInt(face->size->metrics.height));
    }
    fprintf(f, "\n[files]\n");
    fprintf(f, "alpha %s-a.raw\n",	encoding_name);
    fprintf(f, "bitmap %s-b.raw\n",	encoding_name);
    fprintf(f, "\n[characters]\n");


    /* compute bbox and [characters] section*/
    pen_x = 0;
    pen_y = 0;
    ymin = INT_MAX;
    ymax = INT_MIN;
    for (i= 0; i<charset_size; ++i) {
	FT_UInt	glyph_index;
	FT_BBox	bbox;
	character = charset[i];
	code = charcodes[i];

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

	error = FT_Load_Glyph(face, glyph_index, load_flags);
	if (error) {
	    WARNING("Load_Glyph 0x%02x (char 0x%02x|U+%04X) failed.", glyph_index, code, character);
	    continue;
	}

	slot = face->glyph;
	error = FT_Get_Glyph(slot, &glyph);


	FT_Glyph_Get_CBox(glyph, ft_glyph_bbox_pixels, &bbox);
	if (pen_y+bbox.yMax>ymax) {
	    ymax = pen_y+bbox.yMax;
	    // eprintf("%3i: ymax %i (%c)\n", code, ymax, code);
	}
	if (pen_y+bbox.yMin<ymin) {
	    ymin = pen_y+bbox.yMin;
	    // eprintf("%3i: ymin %i (%c)\n", code, ymin, code);
	}

	/* advance pen */
	pen_xa = pen_x + f266toInt(slot->advance.x) + 2*padding;
	// pen_y += f266toInt(slot->advance.y);		// for vertical layout

	/* font.desc */
	fprintf(f, "0x%02x %i %i;\tU+%04X|%c\n", code, pen_x,  pen_xa-1, character, code<' '||code>255 ? '.':code);
	pen_x = ALIGN(pen_xa);

    }

    fclose(f);

    if (ymax<=ymin) ERROR("Something went wrong. Use the source!");


    width = pen_x;
    height = ymax - ymin + 2*padding;
    baseline = ymax + padding;
    eprintf("bitmap size: %ix%i\n", width, height);

    buffer = (unsigned char*)malloc(width*height);
    abuffer = (unsigned char*)malloc(width*height);
    if (buffer==NULL || abuffer==NULL) ERROR("malloc failed.");

    memset(buffer, 0, width*height);


    /* render glyphs */
    pen_x = 0;
    pen_y = baseline;
    for (i= 0; i<charset_size; ++i) {
	FT_UInt	glyph_index;
	character = charset[i];
	code = charcodes[i];

	if (character==0)
	    glyph_index = 0;
	else {
	    glyph_index = FT_Get_Char_Index(face, uni_charmap ? character:code);
	    if (glyph_index==0)
		continue;
	}

	error = FT_Load_Glyph(face, glyph_index, load_flags);
	if (error) {
	    // WARNING("Load_Glyph failed.");
	    continue;
	}

	error = FT_Render_Glyph(face->glyph, ft_render_mode_normal);
	if (error) WARNING("Render_Glyph 0x%02x (char 0x%02x|U+%04X) failed.", glyph_index, code, character);

	slot = face->glyph;

	paste_bitmap(&slot->bitmap,
	    pen_x + padding + slot->bitmap_left,
	    pen_y - slot->bitmap_top );

	/* advance pen */
	pen_x += f266toInt(slot->advance.x) + 2*padding;
	// pen_y += f266toInt(slot->advance.y);	// for vertical layout
	pen_x = ALIGN(pen_x);
    }


    error = FT_Done_FreeType(library);
    if (error) ERROR("Done_FreeType failed.");
}


/* decode from 'encoding' to unicode */
FT_ULong decode_char(char c) {
    FT_ULong o;
    char *inbuf = &c;
    char *outbuf = (char*)&o;
    int inbytesleft = 1;
    int outbytesleft = sizeof(FT_ULong);

    size_t count = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
//    if (count==-1) o = 0; // not OK, at least my iconv() returns E2BIG for all
    if (outbytesleft!=0) o = 0;

    /* convert unicode BE -> LE */
    o = ((o>>24)&0xff)
      | ((o>>8)&0xff00)
      | ((o&0xff00)<<8)
      | ((o&0xff)<<24);

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


// brute-force gaussian blur
void blur(
	unsigned char *s,
	unsigned char *t,
	int width,
	int height,
	int *m,
	int r,
	int mwidth,
	unsigned volume) {

    int x, y;

    for (y = 0; y<height; ++y) {
	for (x = 0; x<width; ++x, ++s, ++t) {
	    unsigned sum = 0;
	    unsigned *mrow = m + r;
	    unsigned char *srow = s -r*width;
	    int x1=(x<r)?-x:-r;
	    int x2=(x+r>=width)?(width-x-1):r;
	    int my;

	    for (my = -r; my<=r; ++my, srow+= width, mrow+= mwidth) {
		int mx;
		if (y+my < 0) continue;
		if (y+my >= height) break;

		for (mx = x1; mx<=x2; ++mx)
		    sum+= srow[mx] * mrow[mx];

	    }
	    *t = (sum + volume/2) / volume;
	}
    }
}


void alpha() {
    int const g_r = ceil(radius);
    int const o_r = ceil(thickness);
    int const g_w = 2*g_r+1;		// matrix size
    int const o_w = 2*o_r+1;		// matrix size
    double const A = log(1.0/base)/(radius*radius*2);

    int mx, my;
    unsigned volume = 0;		// volume under Gaussian area is exactly -pi*base/A

    unsigned *gm = (unsigned*)malloc(g_w*g_w * sizeof(unsigned));
    unsigned *om = (unsigned*)malloc(o_w*o_w * sizeof(unsigned));
    unsigned char *tbuffer = (unsigned char*)malloc(width*height);
    if (gm==NULL || om==NULL || tbuffer==NULL) ERROR("malloc failed.");


    /* Gaussian matrix */
    for (my = 0; my<g_w; ++my) {
	for (mx = 0; mx<g_w; ++mx) {
	    gm[mx+my*g_w] = (unsigned)(exp(A * ((mx-g_r)*(mx-g_r)+(my-g_r)*(my-g_r))) * base + .5);
	    volume+= gm[mx+my*g_w];
	    if (DEBUG) eprintf("%3i ", gm[mx+my*g_w]);
	}
	if (DEBUG) eprintf("\n");
    }
    if (DEBUG) {
	eprintf("A= %f\n", A);
	eprintf("volume: %i; exact: %.0f; volume/exact: %.6f\n\n", volume, -M_PI*base/A, volume/(-M_PI*base/A));
    }


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
      outline1(buffer, tbuffer, width, height);	// FAST solid 1 pixel outline
    else
      outline(buffer, tbuffer, width, height, om, o_r, o_w);	// solid outline
    
    //outline(buffer, tbuffer, width, height, gm, g_r, g_w);	// Gaussian outline

    blur(tbuffer, abuffer, width, height, gm, g_r, g_w, volume);

    free(gm);
    free(om);
    free(tbuffer);
}


void usage() {
    printf("Usage: %s [--append] [--blur b] [--outline o] encoding ppem font\n", command);
    printf("\n"
	    "  Program creates 3 files: font.desc, <encoding>-a.raw, <encoding>-b.raw.\n"
	    "\n"
	    "  --append         append results to existing font.desc.\n"
	    "  --blur b         specify blur radius, float.\n"
	    "  --outline o      specify outline thickness, float.\n"
	    "  encoding         must be an 8 bit encoding, like iso-8859-2, or path to custom encoding file (see README).\n"
	    "                   To list encodings available on your system use iconv --list.\n"
	    "  ppem             Font size in pixels (e.g. 24).\n"
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

    if (argc==0) usage();
    if (strcmp(argv[a], "--append")==0) {
	append_mode = 1;
	++a; --argc;
    }


    if (argc==0) usage();
    if (strcmp(argv[a], "--blur")==0) {
	++a; --argc;
	if (argc==0) usage();

	d = atof(argv[a]);
	if (d>=0 && d<20) radius = d;
	else WARNING("using default blur radius.");
	++a; --argc;
    }

    if (argc==0) usage();
    if (strcmp(argv[a], "--outline")==0) {
	++a; --argc;
	if (argc==0) usage();

	d = atof(argv[a]);
	if (d>=0 && d<20) thickness = d;
	else WARNING("using default outline thickness.");
	++a; --argc;
    }

    if (argc<3) usage();

    if (argv[a][0]!=0)
	encoding = argv[a];
    encoding_name = strrchr(encoding, '/');
    if (!encoding_name) encoding_name=encoding;
    else ++encoding_name;
    
    ++a; --argc;

    i = atoi(argv[a]);
    if (i>1) ppem = i;
    ++a; --argc;

    font_path = argv[a];
    ++a; --argc;
}


int main(int argc, char **argv) {
    parse_args(argc, argv);
    padding = ceil(radius) + ceil(thickness);

    prepare_charset();
    render();
    alpha();
    write_bitmap();

    free(buffer);
    free(abuffer);

//    fflush(stderr);
    return 0;
}
