/*
 * Video driver for Framebuffer device
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 * 
 * Some idea and code borrowed from Chris Lawrence's ppmtofb-0.27
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/vt.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "yuv2rgb.h"
extern void rgb15to16_mmx(char *s0, char *d0, int count);

#include "fastmemcpy.h"

LIBVO_EXTERN(fbdev)

static vo_info_t vo_info = {
	"Framebuffer Device",
	"fbdev",
	"Szabolcs Berecz <szabi@inf.elte.hu>",
	""
};

/******************************
*	fb.modes parser       *
******************************/

/*
 * read the fb.modes manual page!
 */

typedef struct {
	char *name;
	uint32_t xres, yres, vxres, vyres, depth;
	uint32_t pixclock, left, right, upper, lower, hslen, vslen;
	uint32_t sync;
	uint32_t vmode;
} fb_mode_t;

#define PRINT_LINENUM printf(" at line %d\n", line_num)

#define MAX_NR_TOKEN	16

#define MAX_LINE_LEN	1000

#define RET_EOF		-1
#define RET_EOL		-2

static int validate_mode(fb_mode_t *m)
{
	if (!m->xres) {
		printf("needs geometry ");
		return 0;
	}
	if (!m->pixclock) {
		printf("needs timings ");
		return 0;
	}
	return 1;
}

static FILE *fp;
static int line_num = 0;
static char *line;
static char *token[MAX_NR_TOKEN];

static int get_token(int num)
{
	static int read_nextline = 1;
	static int line_pos;
	int i;
	char c;

	if (num >= MAX_NR_TOKEN) {
		printf("get_token(): max >= MAX_NR_TOKEN!");
		goto out_eof;
	}

	if (read_nextline) {
		if (!fgets(line, MAX_LINE_LEN, fp))
			goto out_eof;
		line_pos = 0;
		++line_num;
		read_nextline = 0;
	}
	for (i = 0; i < num; i++) {
		while (isspace(line[line_pos]))
			++line_pos;
		if (line[line_pos] == '\0' || line[line_pos] == '#') {
			read_nextline = 1;
			if (i == num)
				goto out_ok;
			goto out_eol;
		}
		token[i] = line + line_pos;
		c = line[line_pos];
		if (c == '"' || c == '\'') {
			token[i]++;
			while (line[++line_pos] != c && line[line_pos])
				/* NOTHING */;
		} else {
			for (/* NOTHING */; !isspace(line[line_pos]) &&
					line[line_pos]; line_pos++)
				/* NOTHING */;
		}
		if (!line[line_pos]) {
			read_nextline = 1;
			if (i == num - 1)
				goto out_ok;
			goto out_eol;
		}
		line[line_pos] = '\0';
		line_pos++;
	}
out_ok:
	return i;
out_eof:
	return RET_EOF;
out_eol:
	return RET_EOL;
}

static fb_mode_t *fb_modes = NULL;
static int nr_modes = 0;

static int parse_fbmode_cfg(char *cfgfile)
{
	fb_mode_t *mode = NULL;
	char *endptr;	// strtoul()...
	int tmp, i;

#ifdef DEBUG
	assert(cfgfile != NULL);
#endif

	printf("Reading %s: ", cfgfile);

	if ((fp = fopen(cfgfile, "r")) == NULL) {
		printf("can't open '%s': %s\n", cfgfile, strerror(errno));
		return -1;
	}

	if ((line = (char *) malloc(MAX_LINE_LEN + 1)) == NULL) {
		printf("can't get memory for 'line': %s\n", strerror(errno));
		return -2;
	}

	/*
	 * check if the cfgfile starts with 'mode'
	 */
	while ((tmp = get_token(1)) == RET_EOL)
		/* NOTHING */;
	if (tmp == RET_EOF)
		goto out;
	if (!strcmp(token[0], "mode"))
		goto loop_enter;
	goto err_out_parse_error;

	while ((tmp = get_token(1)) != RET_EOF) {
		if (tmp == RET_EOL)
			continue;
		if (!strcmp(token[0], "mode")) {
			if (!validate_mode(mode))
				goto err_out_not_valid;
		loop_enter:
		        if (!(fb_modes = (fb_mode_t *) realloc(fb_modes,
				sizeof(fb_mode_t) * (nr_modes + 1)))) {
			    printf("can't realloc 'fb_modes': %s\n", strerror(errno));
			    goto err_out;
		        }
			mode=fb_modes + nr_modes;
			++nr_modes;
                        memset(mode,0,sizeof(fb_mode_t));

			if (get_token(1) < 0)
				goto err_out_parse_error;
			for (i = 0; i < nr_modes - 1; i++) {
				if (!strcmp(token[0], fb_modes[i].name)) {
					printf("mode name '%s' isn't unique", token[0]);
					goto err_out_print_linenum;
				}
			}
			if (!(mode->name = strdup(token[0]))) {
				printf("can't strdup -> 'name': %s\n", strerror(errno));
				goto err_out;
			}
		} else if (!strcmp(token[0], "geometry")) {
			if (get_token(5) < 0)
				goto err_out_parse_error;
			mode->xres = strtoul(token[0], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->yres = strtoul(token[1], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->vxres = strtoul(token[2], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->vyres = strtoul(token[3], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->depth = strtoul(token[4], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "timings")) {
			if (get_token(7) < 0)
				goto err_out_parse_error;
			mode->pixclock = strtoul(token[0], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->left = strtoul(token[1], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->right = strtoul(token[2], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->upper = strtoul(token[3], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->lower = strtoul(token[4], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->hslen = strtoul(token[5], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->vslen = strtoul(token[6], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "endmode")) {
			/* NOTHING for now*/
		} else if (!strcmp(token[0], "hsync")) {
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "low"))
				mode->sync &= ~FB_SYNC_HOR_HIGH_ACT;
			else if(!strcmp(token[0], "high"))
				mode->sync |= FB_SYNC_HOR_HIGH_ACT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "vsync")) {
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "low"))
				mode->sync &= ~FB_SYNC_VERT_HIGH_ACT;
			else if(!strcmp(token[0], "high"))
				mode->sync |= FB_SYNC_VERT_HIGH_ACT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "csync")) {
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "low"))
				mode->sync &= ~FB_SYNC_COMP_HIGH_ACT;
			else if(!strcmp(token[0], "high"))
				mode->sync |= FB_SYNC_COMP_HIGH_ACT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "extsync")) {
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "false"))
				mode->sync &= ~FB_SYNC_EXT;
			else if(!strcmp(token[0], "true"))
				mode->sync |= FB_SYNC_EXT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "laced")) {
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "false"))
				mode->vmode = FB_VMODE_NONINTERLACED;
			else if (!strcmp(token[0], "true"))
				mode->vmode = FB_VMODE_INTERLACED;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "dblscan")) {
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "false"))
				;
			else if (!strcmp(token[0], "true"))
				mode->vmode = FB_VMODE_DOUBLE;
			else
				goto err_out_parse_error;
		} else
			goto err_out_parse_error;
	}
	if (!validate_mode(mode))
		goto err_out_not_valid;
out:
	printf("%d modes\n", nr_modes);
	free(line);
	fclose(fp);
	return nr_modes;
err_out_parse_error:
	printf("parse error");
err_out_print_linenum:
	PRINT_LINENUM;
err_out:
	if (fb_modes)
		free(fb_modes);
	free(line);
	free(fp);
	return -2;
err_out_not_valid:
	printf("mode is not definied correctly");
	goto err_out_print_linenum;
}

static fb_mode_t *find_mode_by_name(char *name)
{
	int i;

	for (i = 0; i < nr_modes; i++) {
		if (!strcmp(name, fb_modes[i].name))
			return fb_modes + i;
	}
	return NULL;
}

/******************************
*	    vo_fbdev	      *
******************************/

static int fb_init_done = 0;
static int fb_works = 0;
char *fb_dev_name = NULL;
static int fb_dev_fd;
static size_t fb_size;
static uint8_t *frame_buffer;
static struct fb_fix_screeninfo fb_finfo;
static struct fb_var_screeninfo fb_orig_vinfo;
static struct fb_var_screeninfo fb_vinfo;
static struct fb_cmap *fb_oldcmap = NULL;
static int fb_pixel_size;	// 32:  4  24:  3  16:  2  15:  2
static int fb_real_bpp;		// 32: 24  24: 24  16: 16  15: 15
static int fb_bpp;		// 32: 32  24: 24  16: 16  15: 15
static int fb_screen_width;

char *fb_mode_cfgfile = "/etc/fb.modes";
char *fb_mode_name = NULL;
static fb_mode_t *fb_mode = NULL;
static int fb_switch_mode = 0;

static uint8_t *next_frame;
static int in_width;
static int in_height;
static int out_width;
static int out_height;
static uint32_t pixel_format;

/*
 * Note: this function is completely cut'n'pasted from
 * Chris Lawrence's code.
 * (modified a bit to fit in my code...)
 */
struct fb_cmap *make_directcolor_cmap(struct fb_var_screeninfo *var)
{
  /* Hopefully any DIRECTCOLOR device will have a big enough palette
   * to handle mapping the full color depth.
   * e.g. 8 bpp -> 256 entry palette
   *
   * We could handle some sort of gamma here
   */
  int i, cols, rcols, gcols, bcols;
  uint16_t *red, *green, *blue;
  struct fb_cmap *cmap;
        
  rcols = 1 << var->red.length;
  gcols = 1 << var->green.length;
  bcols = 1 << var->blue.length;
  
  /* Make our palette the length of the deepest color */
  cols = (rcols > gcols ? rcols : gcols);
  cols = (cols > bcols ? cols : bcols);
  
  red = malloc(cols * sizeof(red[0]));
  if(!red) {
	  printf("Can't allocate red palette with %d entries.\n", cols);
	  return NULL;
  }
  for(i=0; i< rcols; i++)
    red[i] = (65535/(rcols-1)) * i;
  
  green = malloc(cols * sizeof(green[0]));
  if(!green) {
	  printf("Can't allocate green palette with %d entries.\n", cols);
	  free(red);
	  return NULL;
  }
  for(i=0; i< gcols; i++)
    green[i] = (65535/(gcols-1)) * i;
  
  blue = malloc(cols * sizeof(blue[0]));
  if(!blue) {
	  printf("Can't allocate blue palette with %d entries.\n", cols);
	  free(red);
	  free(green);
	  return NULL;
  }
  for(i=0; i< bcols; i++)
    blue[i] = (65535/(bcols-1)) * i;
  
  cmap = malloc(sizeof(struct fb_cmap));
  if(!cmap) {
	  printf("Can't allocate color map\n");
	  free(red);
	  free(green);
	  free(blue);
	  return NULL;
  }
  cmap->start = 0;
  cmap->transp = 0;
  cmap->len = cols;
  cmap->red = red;
  cmap->blue = blue;
  cmap->green = green;
  cmap->transp = NULL;
  
  return cmap;
}

static int fb_init(void)
{
	int fd;
	struct fb_cmap *cmap;

	if (fb_mode_name) {
		if (parse_fbmode_cfg(fb_mode_cfgfile) < 0)
			return 1;
		if (!(fb_mode = find_mode_by_name(fb_mode_name))) {
			printf("fb_init: can't find requested video mode\n");
			return 1;
		}
		fb_switch_mode = 1;
	}

	if (!fb_dev_name && !(fb_dev_name = getenv("FRAMEBUFFER")))
		fb_dev_name = "/dev/fb0";
	printf("fb_init: using %s\n", fb_dev_name);

	if ((fb_dev_fd = open(fb_dev_name, O_RDWR)) == -1) {
		printf("fb_init: Can't open %s: %s\n", fb_dev_name, strerror(errno));
		goto err_out;
	}

	if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
		printf("fb_init: Can't get VSCREENINFO: %s\n", strerror(errno));
		goto err_out_fd;
	}

	fb_orig_vinfo = fb_vinfo;
	if (fb_switch_mode) {
		fb_vinfo.xres = fb_mode->xres;
		fb_vinfo.yres = fb_mode->yres;
		fb_vinfo.xres_virtual = fb_mode->vxres;
		fb_vinfo.yres_virtual = fb_mode->vyres;
		fb_vinfo.bits_per_pixel = fb_mode->depth;
		switch (fb_mode->depth) {
			case 32:
			case 24:
				fb_vinfo.red.offset = 16;
				fb_vinfo.red.length = 8;
				fb_vinfo.red.msb_right = 0;
				fb_vinfo.green.offset = 8;
				fb_vinfo.green.length = 8;
				fb_vinfo.green.msb_right = 0;
				fb_vinfo.blue.offset = 0;
				fb_vinfo.blue.length = 8;
				fb_vinfo.blue.msb_right = 0;
			case 16:
				fb_vinfo.red.offset = 11;
				fb_vinfo.red.length = 5;
				fb_vinfo.red.msb_right = 0;
				fb_vinfo.green.offset = 5;
				fb_vinfo.green.length = 6;
				fb_vinfo.green.msb_right = 0;
				fb_vinfo.blue.offset = 0;
				fb_vinfo.blue.length = 5;
				fb_vinfo.blue.msb_right = 0;
			case 15:
				fb_vinfo.red.offset = 10;
				fb_vinfo.red.length = 5;
				fb_vinfo.red.msb_right = 0;
				fb_vinfo.green.offset = 5;
				fb_vinfo.green.length = 5;
				fb_vinfo.green.msb_right = 0;
				fb_vinfo.blue.offset = 0;
				fb_vinfo.blue.length = 5;
				fb_vinfo.blue.msb_right = 0;
		}
		fb_vinfo.pixclock = fb_mode->pixclock;
		fb_vinfo.left_margin = fb_mode->left;
		fb_vinfo.right_margin = fb_mode->right;
		fb_vinfo.upper_margin = fb_mode->upper;
		fb_vinfo.lower_margin = fb_mode->lower;
		fb_vinfo.hsync_len = fb_mode->hslen;
		fb_vinfo.vsync_len = fb_mode->vslen;
		fb_vinfo.sync = fb_mode->sync;
		fb_vinfo.vmode = fb_mode->vmode;
	}
	fb_vinfo.xres_virtual = fb_vinfo.xres;
	fb_vinfo.yres_virtual = fb_vinfo.yres;

	if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_vinfo)) {
		printf("fb_init: Can't put VSCREENINFO: %s\n", strerror(errno));
		goto err_out_fd;
	}

	if (ioctl(fb_dev_fd, FBIOGET_FSCREENINFO, &fb_finfo)) {
		printf("fb_init: Can't get VSCREENINFO: %s\n", strerror(errno));
		goto err_out_fd;
		return 1;
	}
	switch (fb_finfo.type) {
		case FB_TYPE_VGA_PLANES:
			printf("fb_init: FB_TYPE_VGA_PLANES not supported.\n");
			goto err_out_fd;
			break;
		case FB_TYPE_PLANES:
			printf("fb_init: FB_TYPE_PLANES not supported.\n");
			goto err_out_fd;
			break;
		case FB_TYPE_INTERLEAVED_PLANES:
			printf("fb_init: FB_TYPE_INTERLEAVED_PLANES not supported.\n");
			goto err_out_fd;
			break;
#ifdef FB_TYPE_TEXT
		case FB_TYPE_TEXT:
			printf("fb_init: FB_TYPE_TEXT not supported.\n");
			goto err_out_fd;
			break;
#endif
		case FB_TYPE_PACKED_PIXELS:
			/* OK */
			printf("fb_init: FB_TYPE_PACKED_PIXELS: OK\n");
			break;
		default:
			printf("fb_init: unknown FB_TYPE: %d\n", fb_finfo.type);
			goto err_out_fd;
	}
	if (fb_finfo.visual == FB_VISUAL_DIRECTCOLOR) {
		printf("fb_init: creating cmap for directcolor\n");
		if (ioctl(fb_dev_fd, FBIOGETCMAP, fb_oldcmap)) {
			printf("fb_init: can't get cmap: %s\n",
					strerror(errno));
			goto err_out_fd;
		}
		if (!(cmap = make_directcolor_cmap(&fb_vinfo)))
			goto err_out_fd;
		if (ioctl(fb_dev_fd, FBIOPUTCMAP, cmap)) {
			printf("fb_init: can't put cmap: %s\n",
					strerror(errno));
			goto err_out_fd;
		}
		free(cmap->red);
		free(cmap->green);
		free(cmap->blue);
		free(cmap);
	} else if (fb_finfo.visual != FB_VISUAL_TRUECOLOR) {
		printf("fb_init: visual: %d not yet supported\n",
				fb_finfo.visual);
		goto err_out_fd;
	}

	fb_pixel_size = fb_vinfo.bits_per_pixel / 8;
	fb_real_bpp = fb_vinfo.red.length + fb_vinfo.green.length +
		fb_vinfo.blue.length;
	fb_bpp = (fb_pixel_size == 4) ? 32 : fb_real_bpp;
	fb_screen_width = fb_finfo.line_length;
	fb_size = fb_finfo.smem_len;
	if ((frame_buffer = (uint8_t *) mmap(0, fb_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, fb_dev_fd, 0)) == (uint8_t *) -1) {
		printf("fb_init: Can't mmap %s: %s\n", fb_dev_name, strerror(errno));
		goto err_out_fd;
	}

	printf("fb_init: framebuffer @ %p\n", frame_buffer);
	printf("fb_init: framebuffer size: %d bytes\n", fb_size);
	printf("fb_init: bpp: %d\n", fb_bpp);
	printf("fb_init: real bpp: %d\n", fb_real_bpp);
	printf("fb_init: pixel size: %d bytes\n", fb_pixel_size);
	printf("fb_init: pixel per line: %d\n", fb_screen_width / fb_pixel_size);
	printf("fb_init: visual: %d\n", fb_finfo.visual);
	printf("fb_init: red: %d %d %d\n", fb_vinfo.red.offset,
			fb_vinfo.red.length, fb_vinfo.red.msb_right);
	printf("fb_init: green: %d %d %d\n", fb_vinfo.green.offset,
			fb_vinfo.green.length, fb_vinfo.green.msb_right);
	printf("fb_init: blue: %d %d %d\n", fb_vinfo.blue.offset,
			fb_vinfo.blue.length, fb_vinfo.blue.msb_right);

	fb_init_done = 1;
	fb_works = 1;
	return 0;
err_out_fd:
	close(fb_dev_fd);
	fb_dev_fd = -1;
err_out:
	fb_init_done = 1;
	return 1;
}

static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width,
		uint32_t d_height, uint32_t fullscreen, char *title,
		uint32_t format)
{
	if (!fb_init_done)
		if (fb_init())
			return 1;
	if (!fb_works)
		return 1;

	in_width = width;
	in_height = height;
	out_width = width;
	out_height = height;
	pixel_format = format;
	if (!(next_frame = (uint8_t *) malloc(in_width * in_height * fb_pixel_size))) {
		printf("Can't malloc next_frame: %s\n", strerror(errno));
		return 1;
	}

	if (format == IMGFMT_YV12)
		yuv2rgb_init(fb_bpp, MODE_RGB);
	return 0;
}

static uint32_t query_format(uint32_t format)
{
	if (!fb_init_done)
		if (fb_init())
			return 0;
	if (!fb_works)
		return 0;

	if ((format & IMGFMT_BGR_MASK) == IMGFMT_BGR) {
		int bpp = format & 0xff;
		if (bpp == fb_bpp)
			return 1;
		else if (bpp == 15 && fb_bpp == 16)
			return 1;
		else if (bpp == 24 && fb_bpp == 32)
			return 1;
	}
	if (format == IMGFMT_YV12)
		return 1;
	return 0;
}

static const vo_info_t *get_info(void)
{
	return &vo_info;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	uint8_t *dst = next_frame + (in_width * y0 + x0) * fb_pixel_size;
	int dstride = in_width * fb_pixel_size;

	switch (fb_real_bpp) {
	case 24:
		vo_draw_alpha_rgb24(w, h, src, srca, stride, dst, dstride);
		break;
	case 32:
		vo_draw_alpha_rgb32(w, h, src, srca, stride, dst, dstride);
		break;
	case 15:
		vo_draw_alpha_rgb15(w, h, src, srca, stride, dst, dstride);
		break;
	case 16:
		vo_draw_alpha_rgb16(w, h, src, srca, stride, dst, dstride);
		break;
	}
#if 0
	int x, y;
	uint8_t *dst;

//	if (pixel_format == IMGFMT_YV12) {
	for (y = 0; y < h; y++){
		dst = next_frame + (in_width * (y0 + y) + x0) * fb_pixel_size;
		for (x = 0; x < w; x++) {
			if (srca[x]) {
				dst[0]=((dst[0]*srca[x])>>8)+src[x];
				dst[1]=((dst[1]*srca[x])>>8)+src[x];
				dst[2]=((dst[2]*srca[x])>>8)+src[x];
			}
			dst += fb_pixel_size;
		}
		src += stride;
		srca += stride;
	}
//	}
#endif
}

static uint32_t draw_frame(uint8_t *src[])
{
	if (pixel_format == IMGFMT_YV12) {
		yuv2rgb(next_frame, src[0], src[1], src[2], in_width,
				in_height, in_width * fb_pixel_size,
				in_width, in_width / 2);
	} else {
		int sbpp = ((pixel_format & 0xff) + 7) / 8;
		char *d = next_frame;
		char *s = src[0];
		if (sbpp == fb_pixel_size) {
			if (fb_real_bpp == 16 && pixel_format == (IMGFMT_BGR|15)) {
#ifdef HAVE_MMX
				rgb15to16_mmx(s, d, 2 * in_width * in_height);
#else
				unsigned short *s1 = (unsigned short *) s;
				unsigned short *d1 = (unsigned short *) d;
				unsigned short *e = s1 + in_width * in_height;
				while (s1<e) {
					register x = *(s1++);
					*(d1++) = (x&0x001f)|((x&0x7fe0)<<1);
				}
#endif
			} else
				memcpy(d, s, sbpp * in_width * in_height);
		}
	}
	return 0;
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h, int x,
		int y)
{
	uint8_t *dest;

	dest = next_frame + (in_width * y + x) * fb_pixel_size;
	yuv2rgb(dest, src[0], src[1], src[2], w, h, in_width * fb_pixel_size,
			stride[0], stride[1]);
	return 0;
}

static void check_events(void)
{
}

static void put_frame(void)
{
	int i, out_offset = 0, in_offset = 0;

	for (i = 0; i < in_height; i++) {
		memcpy(frame_buffer + out_offset, next_frame + in_offset,
				in_width * fb_pixel_size);
		out_offset += fb_screen_width;
		in_offset += in_width * fb_pixel_size;
	}
}

static void flip_page(void)
{
	vo_draw_text(in_width, in_height, draw_alpha);
	check_events();
	put_frame();
}

static void uninit(void)
{
	printf("vo_fbdev: uninit\n");
	if (fb_oldcmap) {
		if (ioctl(fb_dev_fd, FBIOPUTCMAP, fb_oldcmap))
			printf("vo_fbdev: Can't restore original cmap\n");
		fb_oldcmap = NULL;
	}
	if (fb_switch_mode)
		fb_vinfo = fb_orig_vinfo;
	else {
		fb_vinfo.xres_virtual = fb_orig_vinfo.xres_virtual;
		fb_vinfo.yres_virtual = fb_orig_vinfo.yres_virtual;
	}
	if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_vinfo))
		printf("vo_fbdev: Can't set virtual screensize to original value: %s\n", strerror(errno));
	close(fb_dev_fd);
	memset(next_frame, '\0', in_height * in_width * fb_pixel_size);
	put_frame();
	free(next_frame);
	munmap(frame_buffer, fb_size);
}
