/*
  
  mplayer font creator for central-europe (latin-1 etc) charset

  This program uses gd & freetype2 library to draw each characters then
  write the image to stdout.

  Written by Sunjin Yang <lethean@realtime.ssu.ac.kr> May 03, 2001.
  Modified by Arpad Gereoffy <arpi@thot.banki.hu> Jun 18, 2001.

*/

#include <gd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define UPSCALE_FACTOR 2

#define X_ALIGN (8*UPSCALE_FACTOR)
#define ALIGNED(x) (((x)+(X_ALIGN-1))&(~(X_ALIGN-1)))

#define DEF_FONT_SIZE		16.0

#define DEF_CHAR_GAP		6
#define CHAR_SKIP(gap)		(gap / 4)

#define AUTHOR			"Sunjin Yang <lethean@realtime.ssu.ac.kr>"
#define VERSION			"0.1"

struct code_range {
	int start, end;
};

/* basic alphabet character range */
//static struct code_range ascii_range = { 0x21, 0x7E };
static struct code_range ascii_range = { 0x20, 0x1FF };

#ifdef USE_UNIFIED_KOREAN

/* Unified Hangul Code Encoding */
static struct code_range first_byte_range[] = {
	{ 0x81, 0xFE }, { 0, 0 }
};
static struct code_range second_byte_range[] = {
	{ 0x41, 0x5A }, { 0x61, 0x7A }, { 0x81, 0x9F }, { 0xA0, 0xBF },
	{ 0xC0, 0xDF }, { 0xE0, 0xFE },	{ 0, 0 }
};

#else

/* KSX 1001:1992 */
static struct code_range first_byte_range[] = {
	{ 0xA1, 0xAC }, { 0xB0, 0xFD }, { 0, 0 }
};
static struct code_range second_byte_range[] = {
	{ 0xA1, 0xAF }, { 0xB0, 0xBF }, { 0xC0, 0xCF }, { 0xD0, 0xDF },
	{ 0xE0, 0xEF }, { 0xF0, 0xFE }, { 0, 0 }
};

#endif

#define _output(msg...)	fprintf(stdout, ##msg)

/* debugging macros */
#define _print(msg...)	fprintf(stderr, ##msg)
#define _info(msg...)	{ _print("mpfc: "); _print(##msg); _print("\n"); }
#define _abort(msg...)	{ _info(##msg); exit(1); }

static double size;
static int gap,vgap;
static char *name, *font, *eng_font, *kor_font;
static int file_index;
static char filename[20];

static int base_x, char_count;
static gdImagePtr char_image[65536];

static gdImagePtr concat_char_images(void)
{
	gdImagePtr ret;
	int width, height, i, x,black, white;

	/* get image's width & height */
	height = size + (vgap * 2);
	for (width = 0, i = 0; i < char_count; i++)
		width += ALIGNED(char_image[i]->sx);

	ret = gdImageCreate(width, height);

	/* background color (first allocated) */
	black = gdImageColorResolve(ret, 0, 0, 0);
//	white = gdImageColorResolve(ret, 255, 255, 255);
	for(x=1;x<=255;x++)
	    white = gdImageColorResolve(ret, x,x,x);

	width = 0;
	for (i = 0; i < char_count; i++) {
		gdImageCopy(ret, char_image[i],	/* dst, src */
			    width + 0, 0,	/* dstX, dstY */
			    0, 0,	 	/* srcX, srcY */
			    char_image[i]->sx, char_image[i]->sy); /* size */
		width += ALIGNED(char_image[i]->sx);
		gdImageDestroy(char_image[i]);
	}
	char_count = 0;

	return ret;
}

static gdImagePtr create_char_image(int code)
{
	gdImagePtr im;
	int rect[8], black, white, width, height, x, y;
	char *err;
	char s[10];
	
#if 1
	sprintf(s,"&#%d;",code);
#else
	if(code>=0x100){
	  s[0]=code>>8;
	  s[1]=code&0xFF;
	  s[2]=0;
	} else {
	  s[0]=code;
	  s[1]=0;
	}
#endif

	/* obtain border rectangle so that we can size the image. */
	err = gdImageStringTTF(NULL, &rect[0], 0, font, size, .0, 0, 0, s);
	if (err)
		_abort("%s\n", err);

	/* create an image big enough for a string plus a little whitespace. */
	width = rect[2] - rect[6] + gap;
	height = size + (vgap * 2);
	im = gdImageCreate(width, height);

	/* background color (first allocated) */
	black = gdImageColorResolve(im, 0, 0, 0);
	for(x=1;x<=255;x++)
	    white = gdImageColorResolve(im, x,x,x);
//	white = gdImageColorResolve(im, 255, 255, 255);

	/* render the string, offset origin to center string.
	   note that we use top-left coordinate for adjustment
	   since gd origin is in top-left with y increasing downwards. */
	x = (gap / 2) - rect[6];
	y = (vgap) - rect[7] + (size + rect[7]);
	err = gdImageStringTTF(im, &rect[0], white, font, size, .0, x, y, s);
	if (err)
		_abort("%s\n", err);

	//if (*s == '"') _output("'%s' ", s); else _output("\"%s\" ", s);
	_output("0x%x %d %d\n", code,
		(base_x + CHAR_SKIP(gap))/UPSCALE_FACTOR -1, 
		(base_x + width - CHAR_SKIP(gap))/UPSCALE_FACTOR - 0);
	base_x += ALIGNED(width);
//	base_x = (base_x+width+7)&(~7); // align to 8-pixel boundary for fast MMX code

	return im;
}

void make_charset_font(struct code_range *first, struct code_range *second)
{
	gdImagePtr im;
	FILE *fd;
	int i, j;
	
	base_x = 0;
	char_count = 0;

	_output("[files]\n");
	//_output("alpha %s%d_a.raw\n", name, file_index);
	_output("alpha %s%02d_a.raw\n", name, file_index);
	_output("bitmap %s%02d_b.raw\n\n", name, file_index);
	_output("[characters]\n");

	for (i = first->start; i <= first->end; i++) {
		if (!second) {
			char_image[char_count++] = create_char_image(i);
		} else
			for (j = second->start; j <= second->end; j++) {
				char_image[char_count++]= create_char_image((i<<8)|j);
			}
	}

	_output("\n");
	
	/* concatenate each character images into one image. */
	im = concat_char_images();
	
	/* get filename and create one with it. */
	sprintf(filename, "%s%02d_b.png", name, file_index++);
	fd = fopen(filename, "w+");
	if (!fd)
		_abort(strerror(errno));

	/* write image to the PNG file. */
	gdImagePng(im, fd);

	fclose(fd);

	/* destroy it */
	gdImageDestroy(im);
}

int main(int argc, char **argv)
{
	int i, j;

	if (argc < 4)
		_abort("usage:%s name eng-ttf kor-ttf [size gap vgap]",argv[0]);

	/* get program parameter like font names, size... */
	name = argv[1];
	eng_font = argv[2];
	kor_font = argv[3];
	size = DEF_FONT_SIZE;
	gap = DEF_CHAR_GAP;
	vgap = DEF_CHAR_GAP;
	if (argc > 4) {
		float __s; sscanf(argv[4], "%f", &__s);
		size = (double)__s;
	}
	if (argc > 5)
		sscanf(argv[5], "%d", &gap);
	if (argc > 6)
		sscanf(argv[6], "%d", &vgap);

	/* write basic font information. */
	_output("[info]\n");
	_output("name \"%s version %s - created by %s\"\n",
		name, VERSION, AUTHOR);
	_output("descversion 1\n");
	_output("spacewidth %d\n", (int)(size / 2));
	_output("charspace -%d\n", CHAR_SKIP(gap) + 1);
	_output("; height %d\n\n", (int)size + DEF_CHAR_GAP);

	/* write general OSD fonts information. */
	_output("[files]\n");
	_output("alpha arpi_osd_a.raw\n");
	_output("bitmap arpi_osd_b.raw\n\n");
	_output("[characters]\n");
	_output("0x01 0 36\n");
	_output("0x02 35 71\n");
	_output("0x03 70 106\n");
	_output("0x04 116 152\n");
	_output("0x05 164 200\n");
	_output("0x06 209 245\n");
	_output("0x07 256 292\n");
	_output("0x08 305 342\n");
	_output("0x09 354 400\n");
	_output("0x0A 407 442\n");
	_output("0x0B 457 494\n");
	_output("[files]\n");
	_output("alpha arpi_progress_a.raw\n");
	_output("bitmap arpi_progress_b.raw\n\n");
	_output("[characters]\n");
	_output("0x10 4 21\n");
	_output("0x11 30 41\n");
	_output("0x12 50 66\n");
	_output("0x13 74 85\n\n");


	file_index = 0;
	
	/* create basic alphabet character set. */
	font = eng_font;
	make_charset_font(&ascii_range, NULL);

#if 0
	/* create korean character set. */
	font = kor_font;
	for (i = 0; first_byte_range[i].start != 0; i++)
		for (j = 0; second_byte_range[j].start != 0; j++)
			make_charset_font(&first_byte_range[i], &second_byte_range[j]);
#endif

	return 0;
}

