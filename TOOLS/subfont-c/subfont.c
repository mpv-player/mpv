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

#if 0			/* freetype 2.0.1 */
#include <freetype/freetype.h>
#else			/* freetype 2.0.3 */
#include <ft2build.h>	
#include FT_FREETYPE_H
#endif

#include FT_GLYPH_H



#define f266toInt(x)	(((x)+32)>>6)		/* round fractional fixed point number to integer */
						/* coordinates are in 26.6 pixels (i.e. 1/64th of pixels) */


int const	test = 1;

/* default values */
char		*encoding = "iso-8859-1";	/* target encoding */
/* gcc 2.1.3 doesn't support ucs-4le, but supports ucs-4 (==ucs-4be) */
char		*charmap = "ucs-4";		/* ucs-4le font charmap encoding */
int		ppem = 20;			/* font size in pixels */

int const	colors = 256;
int const	maxcolor = 255;
int		radius = 2;			/* blur radius */
double		minalpha = 1.0;			/* good value for minalpha is 0.5 */
double		alpha_factor = 1.0;

int const	first_char = 33;
int const	charset_size = 256;

char		*command;
char		*font_path = NULL;
/*char		*font_metrics = NULL;*/


unsigned char	*buffer;
unsigned char	*abuffer;
int		width, height;
static FT_ULong	ustring[256];

#define eprintf(...)		fprintf(stderr, __VA_ARGS__)
#define ERROR(msg, ...)		(eprintf("%s: error: " msg "\n", command, ##__VA_ARGS__), exit(1))
#define WARNING(msg, ...)	eprintf("%s: warning: " msg "\n", command, ##__VA_ARGS__)



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

    snprintf(name, max_name, "%s-b.raw", encoding);
    f = fopen(name, "wb");
    if (f==NULL) ERROR("fopen failed.",NULL);
    write_header(f);
    fwrite(buffer, 1, width*height, f);
    fclose(f);

    snprintf(name, max_name, "%s-a.raw", encoding);
    f = fopen(name, "wb");
    if (f==NULL) ERROR("fopen failed.",NULL);
    write_header(f);
    fwrite(abuffer, 1, width*height, f);
    fclose(f);
}

void render() {
    FT_Library	library;
    FT_Face	face;
    FT_Error	error;
    FT_GlyphSlot	slot;
    FT_ULong	glyph_index;
    FT_Glyph	glyphs[charset_size];
    FILE	*f;
    int	const	load_flags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING;
    int		pen_x, pen_xa, pen_y, ymin, ymax;
    int		i, c;
    int		baseline, space_advance = 20;


    /* initialize freetype */
    error = FT_Init_FreeType(&library);
    if (error) ERROR("Init_FreeType failed.",NULL);
    error = FT_New_Face(library, font_path, 0, &face);
    if (error) ERROR("New_Face failed.",NULL);

    /*
    if (font_metrics) {
	error = FT_Attach_File(face, font_metrics);
	if (error) WARNING("Attach_File failed.");
    }
    */


    if (face->charmap->encoding!=ft_encoding_unicode)
	WARNING("Selected font has no unicode charmap. Very bad!",NULL);


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
	WARNING("Selected font is not scalable. Using ppem=%i", face->available_sizes[j].height);
	error = FT_Set_Pixel_Sizes(face, face->available_sizes[j].width, face->available_sizes[j].height);
    }
    if (error) WARNING("Set_Pixel_Sizes failed.",NULL);


    if (FT_IS_FIXED_WIDTH(face))
	WARNING("Selected font is fixed-width.",NULL);


    /* compute space advance */
    error = FT_Load_Char(face, ' ', load_flags);
    if (error) WARNING("spacewidth set to default.",NULL);
    else space_advance = f266toInt(face->glyph->advance.x);	/* +32 is for rounding */


    /* create font.desc */
    f = fopen("font.desc", "w");
    if (f==NULL) ERROR("fopen failed.",NULL);

    /* print font.desc header */
    fprintf(f, "[info]\n");
    fprintf(f, "name 'File generated for %s encoding using `%s%s%s' face (%s), ppem=%i'\n",
	    encoding,
	    face->family_name,
	    face->style_name ? " ":"", face->style_name ? face->style_name:"",
	    font_path,
	    ppem);
    fprintf(f, "descversion 1\n");
    fprintf(f, "spacewidth %i\n",	2*radius + space_advance);
    fprintf(f, "charspace %i\n",	-2*radius);
    fprintf(f, "height %i\n",	f266toInt(face->size->metrics.height));
    fprintf(f, "\n[files]\n");
    fprintf(f, "alpha %s-a.raw\n",	encoding);
    fprintf(f, "bitmap %s-b.raw\n",	encoding);
    fprintf(f, "\n[characters]\n");


    /* compute bbox and [characters] section*/
    pen_x = 0;
    pen_y = 0;
    ymin = INT_MAX;
    ymax = INT_MIN;
    for (c= first_char, i= 0; c<charset_size; ++c, ++i) {
	FT_UInt	glyph_index;
	FT_BBox	bbox;

	glyph_index = FT_Get_Char_Index(face, ustring[i]);
	if (glyph_index<=0) {
	    WARNING("Glyph for char %3i|%2X|U%04X not found.", c, c, ustring[i]);
	    continue;
	}

	error = FT_Load_Glyph(face, glyph_index, load_flags);
	if (error) {
	    WARNING("Load_Glyph %3u|%2X (char %3i|%2X|U%04X) failed.", glyph_index, glyph_index, c, c, ustring[i]);
	    continue;
	}

	slot = face->glyph;
	error = FT_Get_Glyph(slot, &glyphs[i]);


	FT_Glyph_Get_CBox(glyphs[i], ft_glyph_bbox_pixels, &bbox);
	if (pen_y+bbox.yMax>ymax) {
	    ymax = pen_y+bbox.yMax;
	    /* eprintf("%3i: ymax %i (%c)\n", c, ymax, c); */
	}
	if (pen_y+bbox.yMin<ymin) {
	    ymin = pen_y+bbox.yMin;
	    /* eprintf("%3i: ymin %i (%c)\n", c, ymin, c); */
	}

	/* advance pen */
	pen_xa = pen_x + f266toInt(slot->advance.x) + 2*radius;
	/* pen_y += f266toInt(slot->advance.y);		// for vertical layout */

	/* font.desc */
	if (c=='\'')
	    fprintf(f, "\"%c\" %i %i\n", c, pen_x,  pen_xa-1);
	else
	    fprintf(f, "'%c' %i %i\n", c, pen_x, pen_xa-1);
	pen_x = (pen_xa+7)&~7;				/* 8 byte align */

    }

    fclose(f);

    if (ymax<=ymin) ERROR("Something went wrong.",NULL);


    width = pen_x;
    height = ymax - ymin + 2*radius;
    baseline = ymax + radius;
    eprintf("bitmap size: %ix%i\n", width, height);

    buffer = (unsigned char*)malloc(width*height);
    abuffer = (unsigned char*)malloc(width*height);
    if (buffer==NULL || abuffer==NULL) ERROR("malloc failed.",NULL);


    /* render glyphs */
    pen_x = 0;
    pen_y = baseline;
    for (c= first_char, i= 0; c<charset_size; ++c, ++i) {
	FT_UInt	glyph_index;

	glyph_index = FT_Get_Char_Index(face, ustring[i]);
	if (glyph_index==0) continue;
	error = FT_Load_Glyph(face, glyph_index, load_flags);
	if (error) {
	    /* WARNING("Load_Glyph failed"); */
	    continue;
	}

	error = FT_Render_Glyph(face->glyph, ft_render_mode_normal);
	if (error) WARNING("Render_Glyph %3i|%2X (char %3i|%2X|U%04X) failed.", glyph_index, glyph_index, c, c, ustring[i]);

	slot = face->glyph;

	paste_bitmap(&slot->bitmap,
	    pen_x + radius + slot->bitmap_left,
	    pen_y - slot->bitmap_top );

	/* advance pen */
	pen_x += f266toInt(slot->advance.x) + 2*radius;
	/* pen_y += f266toInt(slot->advance.y);	// for vertical layout */
	pen_x = (pen_x+7)&~7;			/* 8 byte align */
    }


    error = FT_Done_FreeType(library);
    if (error) ERROR("Done_FreeType failed.",NULL);
}

void prepare_charset() {
    iconv_t cd;
    unsigned char text[charset_size];
    char *inbuf = text;
    char *outbuf = (char*) ustring;
    int inbuf_left = charset_size;
    int outbuf_left = 4*charset_size;
    int i;
    size_t count;

    for (i = first_char; i<charset_size; ++i) text[i-first_char] = i;

    /* check if ucs-4le is available */
    cd = iconv_open(charmap, charmap);
    if (cd==(iconv_t)-1) ERROR("iconv doesn't know %s encoding. Use the source!", charmap);
    iconv_close(cd);

    cd = iconv_open(charmap, encoding);
    if (cd==(iconv_t)-1) ERROR("Unsupported encoding, use iconv -l to list character sets known on your system.",NULL);
    while (1) {
	count = iconv(cd, &inbuf, &inbuf_left, &outbuf, &outbuf_left);
    	if (inbuf_left==0) break;
	/* skip undefined characters */
	inbuf+= 1;
	inbuf_left-= 1;
	*(FT_ULong*)outbuf = 0;
	outbuf+=sizeof(FT_ULong);
    }
    iconv_close(cd);

    /* converting unicodes BE -> LE */
    for (i = 0; i<256; ++i){
	FT_ULong x=ustring[i];
	x=  ((x>>24)&255)
	 | (((x>>16)&255)<<8)
	 | (((x>> 8)&255)<<16)
	 | ((x&255)<<24);
	ustring[i]=x;
    }

}

void blur() {
    int const r = radius;
    int const w = 2*r+1;	/* matrix size */
    double const A = log(1.0/maxcolor)/((r+1)*(r+1));
    double const B = maxcolor;
    int sum=0;

    int i, x, y, mx, my;
    unsigned char *m = (unsigned char*)malloc(w*w);

    if (m==NULL) ERROR("malloc failed",NULL);


    /* Gaussian matrix */
    for (my = 0; my<w; ++my) {
	for (mx = 0; mx<w; ++mx) {
	    m[mx+my*w] = (int)(exp(A * ((mx-r)*(mx-r)+(my-r)*(my-r))) * B + .5);
	    sum+=m[mx+my*w];
	    if (test) eprintf("%3i ", m[mx+my*w]);
	}
	if (test) eprintf("\n");
    }
    printf("gauss sum = %d\n",sum);

    /* This is not a gaussian blur! */
    /* And is very slow */
    for (y = 0; y<height; ++y){
	for (x = 0; x<width; ++x) {
	    float max = 0;
	    for (my = -r; my<=r; ++my)
		if (y+my>0 && y+my<height-1){
		    int ay=(y+my)*width;
		    for (mx = -r; mx<=r; ++mx) {
			int ax=x+mx;
			if (ax>0 && ax<width-1) {
			    int p =

			  ( (buffer[ax-1+ay-width]) +
			    (buffer[ax-1+ay+width]) +
			    (buffer[ax+1+ay-width]) +
			    (buffer[ax+1+ay+width]) )/2 +

			  ( (buffer[ax-1+ay]) +
			    (buffer[ax+1+ay]) +
			    (buffer[ax+ay-width]) +
			    (buffer[ax+ay+width]) +

			    (buffer[ax+ay]) ) ;
			    
			    max+=(p>255?255:p)*m[mx+r+(my+r)*w];
			}
		    
		    }
		}
	    max*=alpha_factor/(float)sum;
//	    printf("%5.3f ",max);
	    if(max>255) max=255;
	    abuffer[x+y*width] = max;
	}
//	printf("\n");
    }
    free(m);
}

void usage() {
    printf("Usage: %s encoding ppem font [alphaFactor [minAlpha [radius]]]\n", command);
    printf(
	    "  Program creates 3 files: font.desc, <encoding>-a.raw, <encoding>-b.raw.\n"
	    "  You should append font.desc.tail (desc for OSD characters by a'rpi & chass) to font.desc,\n"
	    "  and copy font.desc and all *.raw files to ~/.mplayer/font/ directory.\n"
	    "\n"
	    "  encoding     must be 8 bit encoding, like iso-8859-2.\n"
	    "               To list encodings available on your system use iconv -l.\n"
	    "  ppem         Font size in pixels (e.g. 24).\n"
	    "  font         Font file path. Any format supported by freetype library (*.ttf, *.pf?, *).\n"
	    "  alphaFactor  Alpha map scaling factor (default is 1.0), float.\n"
	    "  minAlpha     Alpha map minimum value (default is 1.0, max is 255), float.\n"
	    "  radius       Alpha map blur radius (default is 6 pixels), integer.\n"
	    );
    exit(1);
}

void parse_args(int argc, char **argv) {
    int i;
    double d;

    command = strrchr(argv[0], '/');
    if (command==NULL) command = argv[0];
    else ++command;

    if (argc<4) usage();

    encoding = argv[1];

    i = atoi(argv[2]);
    if (i>1) ppem = i;

    font_path = argv[3];

    if (argc>4) {
	d = atof(argv[4]);
	if (d>0.001 && d<1000.) alpha_factor = d;
	else WARNING("alphaFactor set to default.",NULL);
    }
    if (argc>5) {
	d = atof(argv[5]);
	if (d>0.1 && d<=maxcolor) minalpha = d;
	else WARNING("minAlpha set to default.",NULL);
    }
    if (argc>6) {
	i = atoi(argv[6]);
	if (i>=0 && i<20) radius = i;
	else WARNING("radius set to default.",NULL);
    }
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

    prepare_charset();
    render();
    blur();
    write_bitmap();

    free(buffer);
    free(abuffer);

    puts(
	    "\n"
	    "*****************************************\n"
	    "*  Remember to run:                     *\n"
	    "*  cat font.desc.tail >> font.desc      *\n"
	    "*****************************************"
	    );

    return 0;
}
