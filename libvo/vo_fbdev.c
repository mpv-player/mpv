/*
 * Video driver for Framebuffer device
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 * 
 * Some idea and code borrowed from Chris Lawrence's ppmtofb-0.27
 */

#define FBDEV "fbdev: "

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
#include <sys/kd.h>
#include <linux/fb.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "sub.h"
#include "../postproc/rgb2rgb.h"
#ifdef CONFIG_VIDIX
#include "vosub_vidix.h"
#endif
#include "aspect.h"

#ifdef HAVE_PNG
extern vo_functions_t video_out_png;
#endif

LIBVO_EXTERN(fbdev)

static vo_info_t vo_info = {
	"Framebuffer Device",
	"fbdev",
	"Szabolcs Berecz <szabi@inf.elte.hu>",
	""
};

extern int verbose;

#ifdef CONFIG_VIDIX
/* Name of VIDIX driver */
static const char *vidix_name = NULL;
#endif
static signed int pre_init_err = -2;
/******************************
*	fb.modes support      *
******************************/

extern char *monitor_hfreq_str;
extern char *monitor_vfreq_str;
extern char *monitor_dotclock_str;

static range_t *monitor_hfreq = NULL;
static range_t *monitor_vfreq = NULL;
static range_t *monitor_dotclock = NULL;

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
static uint32_t dstFourcc;

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
			goto out_eol;
		}
		token[i] = line + line_pos;
		c = line[line_pos];
		if (c == '"' || c == '\'') {
			token[i]++;
			while (line[++line_pos] != c && line[line_pos])
				/* NOTHING */;
			if (!line[line_pos])
				goto out_eol;
			line[line_pos] = ' ';
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
		line[line_pos++] = '\0';
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
#define CHECK_IN_MODE_DEF\
	do {\
	if (!in_mode_def) {\
		printf("'needs 'mode' first");\
		goto err_out_print_linenum;\
	}\
	} while (0)

	fb_mode_t *mode = NULL;
	char *endptr;	// strtoul()...
	int in_mode_def = 0;
	int tmp, i;

	if (verbose > 0)
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
			if (in_mode_def) {
				printf("'endmode' required");
				goto err_out_print_linenum;
			}
			if (!validate_mode(mode))
				goto err_out_not_valid;
		loop_enter:
		        if (!(fb_modes = (fb_mode_t *) realloc(fb_modes,
				sizeof(fb_mode_t) * (nr_modes + 1)))) {
			    printf("can't realloc 'fb_modes' (nr_modes = %d):"
					    " %s\n", nr_modes, strerror(errno));
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
			in_mode_def = 1;
		} else if (!strcmp(token[0], "geometry")) {
			CHECK_IN_MODE_DEF;
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
			CHECK_IN_MODE_DEF;
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
			CHECK_IN_MODE_DEF;
			in_mode_def = 0;
		} else if (!strcmp(token[0], "accel")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			/*
			 * it's only used for text acceleration
			 * so we just ignore it.
			 */
		} else if (!strcmp(token[0], "hsync")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "low"))
				mode->sync &= ~FB_SYNC_HOR_HIGH_ACT;
			else if(!strcmp(token[0], "high"))
				mode->sync |= FB_SYNC_HOR_HIGH_ACT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "vsync")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "low"))
				mode->sync &= ~FB_SYNC_VERT_HIGH_ACT;
			else if(!strcmp(token[0], "high"))
				mode->sync |= FB_SYNC_VERT_HIGH_ACT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "csync")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "low"))
				mode->sync &= ~FB_SYNC_COMP_HIGH_ACT;
			else if(!strcmp(token[0], "high"))
				mode->sync |= FB_SYNC_COMP_HIGH_ACT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "extsync")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "false"))
				mode->sync &= ~FB_SYNC_EXT;
			else if(!strcmp(token[0], "true"))
				mode->sync |= FB_SYNC_EXT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "laced")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "false"))
				mode->vmode = FB_VMODE_NONINTERLACED;
			else if (!strcmp(token[0], "true"))
				mode->vmode = FB_VMODE_INTERLACED;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "double")) {
			CHECK_IN_MODE_DEF;
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
	if (verbose > 0)
		printf("%d modes\n", nr_modes);
	free(line);
	fclose(fp);
	return nr_modes;
err_out_parse_error:
	printf("parse error");
err_out_print_linenum:
	PRINT_LINENUM;
err_out:
	if (fb_modes) {
		free(fb_modes);
		fb_modes = NULL;
	}
	nr_modes = 0;
	free(line);
	free(fp);
	return -2;
err_out_not_valid:
	printf("previous mode is not correct");
	goto err_out_print_linenum;
}

static fb_mode_t *find_mode_by_name(char *name)
{
	int i;

	for (i = 0; i < nr_modes; i++)
		if (!strcmp(name, fb_modes[i].name))
			return fb_modes + i;
	return NULL;
}

static float dcf(fb_mode_t *m)	//driving clock frequency
{
	return 1e12f / m->pixclock;
}

static float hsf(fb_mode_t *m)	//horizontal scan frequency
{
	int htotal = m->left + m->xres + m->right + m->hslen;
	return dcf(m) / htotal;
}

static float vsf(fb_mode_t *m)	//vertical scan frequency
{
	int vtotal = m->upper + m->yres + m->lower + m->vslen;
	return hsf(m) / vtotal;
}


static int mode_works(fb_mode_t *m, range_t *hfreq, range_t *vfreq,
		range_t *dotclock)
{
	float h = hsf(m);
	float v = vsf(m);
	float d = dcf(m);
	int ret = 1;

	if (verbose > 1)
		printf(FBDEV "mode %dx%d:", m->xres, m->yres);
	if (!in_range(hfreq, h)) {
		ret = 0;
		if (verbose > 1)
			printf(" hsync out of range.");
	}
	if (!in_range(vfreq, v)) {
		ret = 0;
		if (verbose > 1)
			printf(" vsync out of range.");
	}
	if (!in_range(dotclock, d)) {
		ret = 0;
		if (verbose > 1)
			printf(" dotclock out of range.");
	}
	if (verbose > 1) {
		if (ret)
			printf(" hsync, vsync, dotclock ok.\n");
		else
			printf("\n");
	}

	return ret;
}

static fb_mode_t *find_best_mode(int xres, int yres, range_t *hfreq,
		range_t *vfreq, range_t *dotclock)
{
	int i;
	fb_mode_t *best = fb_modes;
	fb_mode_t *curr;

	if (verbose > 1)
		printf(FBDEV "Searching for first working mode\n");

	for (i = 0; i < nr_modes; i++, best++)
		if (mode_works(best, hfreq, vfreq, dotclock))
			break;

	if (i == nr_modes)
		return NULL;
	if (i == nr_modes - 1)
		return best;

	if (verbose > 1) {
		printf(FBDEV "First working mode: %dx%d\n", best->xres, best->yres);
		printf(FBDEV "Searching for better modes\n");
	}

	for (curr = best + 1; i < nr_modes - 1; i++, curr++) {
		if (!mode_works(curr, hfreq, vfreq, dotclock))
			continue;

		if (verbose > 1)
			printf(FBDEV);

		if (best->xres < xres || best->yres < yres) {
			if (curr->xres > best->xres || curr->yres > best->yres) {
				if (verbose > 1)
					printf("better than %dx%d, which is too small.\n",
							best->xres, best->yres);
				best = curr;
			} else if (verbose > 1)
				printf("too small.\n");
		} else if (curr->xres == best->xres && curr->yres == best->yres &&
				vsf(curr) > vsf(best)) {
			if (verbose > 1)
				printf("faster screen refresh.\n");
			best = curr;
		} else if ((curr->xres <= best->xres && curr->yres <= best->yres) &&
				(curr->xres >= xres && curr->yres >= yres)) {
			if (verbose > 1)
				printf("better than %dx%d, which is too large.\n",
						best->xres, best->yres);
			best = curr;
		} else if (verbose > 1) {
			if (curr->xres < xres || curr->yres < yres)
				printf("too small.\n");
			else if (curr->xres > best->xres || curr->yres > best->yres)
				printf("too large.\n");
			else printf("it's worse, don't know why.\n");
		}
	}

	return best;
}

static void set_bpp(struct fb_var_screeninfo *p, int bpp)
{
	p->bits_per_pixel = (bpp + 1) & ~1;
	p->red.msb_right = p->green.msb_right = p->blue.msb_right = p->transp.msb_right = 0;
	p->transp.offset = p->transp.length = 0;
	p->blue.offset = 0;
	switch (bpp) {
		case 32:
			p->transp.offset = 24;
			p->transp.length = 8;
		case 24:
			p->red.offset = 16;
			p->red.length = 8;
			p->green.offset = 8;
			p->green.length = 8;
			p->blue.length = 8;
			break;
		case 16:
			p->red.offset = 11;
			p->green.length = 6;
			p->red.length = 5;
			p->green.offset = 5;
			p->blue.length = 5;
			break;
		case 15:
			p->red.offset = 10;
			p->green.length = 5;
			p->red.length = 5;
			p->green.offset = 5;
			p->blue.length = 5;
			break;
	}
}

static void fb_mode2fb_vinfo(fb_mode_t *m, struct fb_var_screeninfo *v)
{
	v->xres = m->xres;
	v->yres = m->yres;
	v->xres_virtual = m->vxres;
	v->yres_virtual = m->vyres;
	set_bpp(v, m->depth);
	v->pixclock = m->pixclock;
	v->left_margin = m->left;
	v->right_margin = m->right;
	v->upper_margin = m->upper;
	v->lower_margin = m->lower;
	v->hsync_len = m->hslen;
	v->vsync_len = m->vslen;
	v->sync = m->sync;
	v->vmode = m->vmode;
}


/******************************
*	    vo_fbdev	      *
******************************/

/* command line/config file options */
char *fb_dev_name = NULL;
char *fb_mode_cfgfile = "/etc/fb.modes";
char *fb_mode_name = NULL;

static fb_mode_t *fb_mode = NULL;

/* vt related variables */
static int vt_fd;
static FILE *vt_fp;
static int vt_doit = 1;

/* vo_fbdev related variables */
static int fb_dev_fd;
static int fb_tty_fd;
static size_t fb_size;
static uint8_t *frame_buffer;
static uint8_t *L123123875;	/* thx .so :) */
static struct fb_fix_screeninfo fb_finfo;
static struct fb_var_screeninfo fb_orig_vinfo;
static struct fb_var_screeninfo fb_vinfo;
static struct fb_cmap fb_oldcmap;
static int fb_cmap_changed = 0;
static int fb_pixel_size;	// 32:  4  24:  3  16:  2  15:  2
static int fb_real_bpp;		// 32: 24  24: 24  16: 16  15: 15
static int fb_bpp;		// 32: 32  24: 24  16: 16  15: 15
static int fb_bpp_we_want;	// 32: 32  24: 24  16: 16  15: 15
static int fb_line_len;
static int fb_xres;
static int fb_yres;
static void (*draw_alpha_p)(int w, int h, unsigned char *src,
		unsigned char *srca, int stride, unsigned char *dst,
		int dstride);

static uint8_t *next_frame;
static int in_width;
static int in_height;
static int out_width;
static int out_height;
static int first_row;
static int last_row;
static uint32_t pixel_format;
static int fs;
static int flip;

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

#ifdef CONFIG_VIDIX
static uint32_t parseSubDevice(const char *sd)
{
   if(memcmp(sd,"vidix",5) == 0) vidix_name = &sd[5]; /* vidix_name will be valid within init() */
   else { printf(FBDEV "Unknown subdevice: '%s'\n", sd); return -1; }
   return 0;
}
#endif

static int fb_preinit(void)
{
	static int fb_preinit_done = 0;
	static int fb_works = 0;

	if (fb_preinit_done)
		return fb_works;

	if (!fb_dev_name && !(fb_dev_name = getenv("FRAMEBUFFER")))
		fb_dev_name = "/dev/fb0";
	if (verbose > 0)
		printf(FBDEV "using %s\n", fb_dev_name);

	if ((fb_dev_fd = open(fb_dev_name, O_RDWR)) == -1) {
		printf(FBDEV "Can't open %s: %s\n", fb_dev_name, strerror(errno));
		goto err_out;
	}
	if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
		printf(FBDEV "Can't get VSCREENINFO: %s\n", strerror(errno));
		goto err_out_fd;
	}
	fb_orig_vinfo = fb_vinfo;

        if ((fb_tty_fd = open("/dev/tty", O_RDWR)) < 0) {
                if (verbose > 0)
                        printf(FBDEV "notice: Can't open /dev/tty: %s\n", strerror(errno));
        }

	fb_bpp = fb_vinfo.bits_per_pixel;

	if (fb_bpp == 8 && !vo_dbpp) {
		printf(FBDEV "8 bpp output is not supported.\n");
		goto err_out_tty_fd;
	}

	/* 16 and 15 bpp is reported as 16 bpp */
	if (fb_bpp == 16)
		fb_bpp = fb_vinfo.red.length + fb_vinfo.green.length +
			fb_vinfo.blue.length;

	if (vo_dbpp) {
		if (vo_dbpp != 15 && vo_dbpp != 16 && vo_dbpp != 24 &&
				vo_dbpp != 32) {
			printf(FBDEV "can't switch to %d bpp\n", vo_dbpp);
			goto err_out_fd;
		}
		fb_bpp = vo_dbpp;		
	}

	fb_preinit_done = 1;
	fb_works = 1;
	return 1;
err_out_tty_fd:
        close(fb_tty_fd);
        fb_tty_fd = -1;
err_out_fd:
	close(fb_dev_fd);
	fb_dev_fd = -1;
err_out:
	fb_preinit_done = 1;
	fb_works = 0;
	return 0;
}

static void lots_of_printf(void)
{
	if (verbose > 0) {
		printf(FBDEV "var info:\n");
		printf(FBDEV "xres: %u\n", fb_vinfo.xres);
		printf(FBDEV "yres: %u\n", fb_vinfo.yres);
		printf(FBDEV "xres_virtual: %u\n", fb_vinfo.xres_virtual);
		printf(FBDEV "yres_virtual: %u\n", fb_vinfo.yres_virtual);
		printf(FBDEV "xoffset: %u\n", fb_vinfo.xoffset);
		printf(FBDEV "yoffset: %u\n", fb_vinfo.yoffset);
		printf(FBDEV "bits_per_pixel: %u\n", fb_vinfo.bits_per_pixel);
		printf(FBDEV "grayscale: %u\n", fb_vinfo.grayscale);
		printf(FBDEV "red: %lu %lu %lu\n",
				(unsigned long) fb_vinfo.red.offset,
				(unsigned long) fb_vinfo.red.length,
				(unsigned long) fb_vinfo.red.msb_right);
		printf(FBDEV "green: %lu %lu %lu\n",
				(unsigned long) fb_vinfo.green.offset,
				(unsigned long) fb_vinfo.green.length,
				(unsigned long) fb_vinfo.green.msb_right);
		printf(FBDEV "blue: %lu %lu %lu\n",
				(unsigned long) fb_vinfo.blue.offset,
				(unsigned long) fb_vinfo.blue.length,
				(unsigned long) fb_vinfo.blue.msb_right);
		printf(FBDEV "transp: %lu %lu %lu\n",
				(unsigned long) fb_vinfo.transp.offset,
				(unsigned long) fb_vinfo.transp.length,
				(unsigned long) fb_vinfo.transp.msb_right);
		printf(FBDEV "nonstd: %u\n", fb_vinfo.nonstd);
		if (verbose > 1) {
			printf(FBDEV "activate: %u\n", fb_vinfo.activate);
			printf(FBDEV "height: %u\n", fb_vinfo.height);
			printf(FBDEV "width: %u\n", fb_vinfo.width);
			printf(FBDEV "accel_flags: %u\n", fb_vinfo.accel_flags);
			printf(FBDEV "timing:\n");
			printf(FBDEV "pixclock: %u\n", fb_vinfo.pixclock);
			printf(FBDEV "left_margin: %u\n", fb_vinfo.left_margin);
			printf(FBDEV "right_margin: %u\n", fb_vinfo.right_margin);
			printf(FBDEV "upper_margin: %u\n", fb_vinfo.upper_margin);
			printf(FBDEV "lower_margin: %u\n", fb_vinfo.lower_margin);
			printf(FBDEV "hsync_len: %u\n", fb_vinfo.hsync_len);
			printf(FBDEV "vsync_len: %u\n", fb_vinfo.vsync_len);
			printf(FBDEV "sync: %u\n", fb_vinfo.sync);
			printf(FBDEV "vmode: %u\n", fb_vinfo.vmode);
		}
		printf(FBDEV "fix info:\n");
		printf(FBDEV "framebuffer size: %d bytes\n", fb_finfo.smem_len);
		printf(FBDEV "type: %lu\n", (unsigned long) fb_finfo.type);
		printf(FBDEV "type_aux: %lu\n", (unsigned long) fb_finfo.type_aux);
		printf(FBDEV "visual: %lu\n", (unsigned long) fb_finfo.visual);
		printf(FBDEV "line_length: %lu bytes\n", (unsigned long) fb_finfo.line_length);
		if (verbose > 1) {
			printf(FBDEV "id: %.16s\n", fb_finfo.id);
			printf(FBDEV "smem_start: %p\n", (void *) fb_finfo.smem_start);
			printf(FBDEV "xpanstep: %u\n", fb_finfo.xpanstep);
			printf(FBDEV "ypanstep: %u\n", fb_finfo.ypanstep);
			printf(FBDEV "ywrapstep: %u\n", fb_finfo.ywrapstep);
			printf(FBDEV "mmio_start: %p\n", (void *) fb_finfo.mmio_start);
			printf(FBDEV "mmio_len: %u bytes\n", fb_finfo.mmio_len);
			printf(FBDEV "accel: %u\n", fb_finfo.accel);
		}
		printf(FBDEV "fb_bpp: %d\n", fb_bpp);
		printf(FBDEV "fb_real_bpp: %d\n", fb_real_bpp);
		printf(FBDEV "fb_pixel_size: %d bytes\n", fb_pixel_size);
		printf(FBDEV "other:\n");
		printf(FBDEV "in_width: %d\n", in_width);
		printf(FBDEV "in_height: %d\n", in_height);
		printf(FBDEV "out_width: %d\n", out_width);
		printf(FBDEV "out_height: %d\n", out_height);
		printf(FBDEV "first_row: %d\n", first_row);
		printf(FBDEV "last_row: %d\n", last_row);
		if (verbose > 1)
			printf(FBDEV "draw_alpha_p:%dbpp = %p\n", fb_bpp, draw_alpha_p);
	}
}

static void vt_set_textarea(int u, int l)
{
	/* how can I determine the font height?
	 * just use 16 for now
	 */
	int urow = ((u + 15) / 16) + 1;
	int lrow = l / 16;

	if (verbose > 1)
		printf(FBDEV "vt_set_textarea(%d,%d): %d,%d\n", u, l, urow, lrow);
	fprintf(vt_fp, "\33[%d;%dr\33[%d;%dH", urow, lrow, lrow, 0);
	fflush(vt_fp);
}

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,
		uint32_t d_height, uint32_t flags, char *title,
		uint32_t format,const vo_tune_info_t *info)
{
	struct fb_cmap *cmap;
	int vm = flags & 0x02;
	int zoom = flags & 0x04;

	fs = flags & 0x01;
	flip = flags & 0x08;

	if(pre_init_err == -2)
	{
	    printf(FBDEV "Internal fatal error: init() was called before preinit()\n");
	    return -1;
	}

	if (pre_init_err) return 1;

	if (zoom
#ifdef CONFIG_VIDIX
	 && !vidix_name
#endif
	 ) {
		printf(FBDEV "-zoom is not supported\n");
		return 1;
	}
	if (fb_mode_name && !vm) {
		printf(FBDEV "-fbmode can only be used with -vm\n");
		return 1;
	}
	if (vm && (parse_fbmode_cfg(fb_mode_cfgfile) < 0))
			return 1;
	if (d_width && (fs || vm)) {
		out_width = d_width;
		out_height = d_height;
	} else {
		out_width = width;
		out_height = height;
	}
	in_width = width;
	in_height = height;
	pixel_format = format;

	if (fb_mode_name) {
		if (!(fb_mode = find_mode_by_name(fb_mode_name))) {
			printf(FBDEV "can't find requested video mode\n");
			return 1;
		}
		fb_mode2fb_vinfo(fb_mode, &fb_vinfo);
	} else if (vm) {
		monitor_hfreq = str2range(monitor_hfreq_str);
		monitor_vfreq = str2range(monitor_vfreq_str);
		monitor_dotclock = str2range(monitor_dotclock_str);
		if (!monitor_hfreq || !monitor_vfreq || !monitor_dotclock) {
			printf(FBDEV "you have to specify the capabilities of"
					" the monitor.\n");
			return 1;
		}
		if (!(fb_mode = find_best_mode(out_width, out_height,
					monitor_hfreq, monitor_vfreq,
					monitor_dotclock))) {
			printf(FBDEV "can't find best video mode\n");
			return 1;
		}
		printf(FBDEV "using mode %dx%d @ %.1fHz\n", fb_mode->xres,
				fb_mode->yres, vsf(fb_mode));
		fb_mode2fb_vinfo(fb_mode, &fb_vinfo);
	}
	fb_bpp_we_want = fb_bpp;
	set_bpp(&fb_vinfo, fb_bpp);
	fb_vinfo.xres_virtual = fb_vinfo.xres;
	fb_vinfo.yres_virtual = fb_vinfo.yres;

        if (fb_tty_fd >= 0 && ioctl(fb_tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
                if (verbose > 0)
                        printf(FBDEV "Can't set graphics mode: %s\n", strerror(errno));
                close(fb_tty_fd);
                fb_tty_fd = -1;
        }

	if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_vinfo)) {
		printf(FBDEV "Can't put VSCREENINFO: %s\n", strerror(errno));
                if (fb_tty_fd >= 0 && ioctl(fb_tty_fd, KDSETMODE, KD_TEXT) < 0) {
                        printf(FBDEV "Can't restore text mode: %s\n", strerror(errno));
                }
		return 1;
	}

	fb_pixel_size = fb_vinfo.bits_per_pixel / 8;
	fb_real_bpp = fb_vinfo.red.length + fb_vinfo.green.length +
		fb_vinfo.blue.length;
	fb_bpp = (fb_pixel_size == 4) ? 32 : fb_real_bpp;
	if (fb_bpp_we_want != fb_bpp)
		printf(FBDEV "requested %d bpp, got %d bpp!!!\n",
				fb_bpp_we_want, fb_bpp);

	switch (fb_bpp) {
	case 32:
		draw_alpha_p = vo_draw_alpha_rgb32;
		dstFourcc = IMGFMT_BGR32;
		break;
	case 24:
		draw_alpha_p = vo_draw_alpha_rgb24;
		dstFourcc = IMGFMT_BGR24;
		break;
	default:
	case 16:
		draw_alpha_p = vo_draw_alpha_rgb16;
		dstFourcc = IMGFMT_BGR16;
		break;
	case 15:
		draw_alpha_p = vo_draw_alpha_rgb15;
		dstFourcc = IMGFMT_BGR15;
		break;
	}

	if (flip & ((((pixel_format & 0xff) + 7) / 8) != fb_pixel_size)) {
		printf(FBDEV "Flipped output with depth conversion is not "
				"supported\n");
		return 1;
	}

	fb_xres = fb_vinfo.xres;
	fb_yres = fb_vinfo.yres;

	if (vm || fs) {
		out_width = fb_xres;
		out_height = fb_yres;
	}
	if (out_width < in_width || out_height < in_height) {
		printf(FBDEV "screensize is smaller than video size\n");
		return 1;
	}

	first_row = (out_height - in_height) / 2;
	last_row = (out_height + in_height) / 2;

	if (ioctl(fb_dev_fd, FBIOGET_FSCREENINFO, &fb_finfo)) {
		printf(FBDEV "Can't get FSCREENINFO: %s\n", strerror(errno));
		return 1;
	}

	lots_of_printf();

	if (fb_finfo.type != FB_TYPE_PACKED_PIXELS) {
		printf(FBDEV "type %d not supported\n", fb_finfo.type);
		return 1;
	}

	switch (fb_finfo.visual) {
		case FB_VISUAL_TRUECOLOR:
			break;
		case FB_VISUAL_DIRECTCOLOR:
			if (verbose > 0)
				printf(FBDEV "creating cmap for directcolor\n");
			if (ioctl(fb_dev_fd, FBIOGETCMAP, &fb_oldcmap)) {
				printf(FBDEV "can't get cmap: %s\n",
						strerror(errno));
				return 1;
			}
			if (!(cmap = make_directcolor_cmap(&fb_vinfo)))
				return 1;
			if (ioctl(fb_dev_fd, FBIOPUTCMAP, cmap)) {
				printf(FBDEV "can't put cmap: %s\n",
						strerror(errno));
				return 1;
			}
			fb_cmap_changed = 1;
			free(cmap->red);
			free(cmap->green);
			free(cmap->blue);
			free(cmap);
			break;
		default:
			printf(FBDEV "visual: %d not yet supported\n",
					fb_finfo.visual);
			return 1;
	}

	fb_line_len = fb_finfo.line_length;
	fb_size = fb_finfo.smem_len;
	frame_buffer = NULL;
	next_frame = NULL;
#ifdef CONFIG_VIDIX
	if(vidix_name)
	{
	    unsigned image_width,image_height,x_offset,y_offset;
	    if(zoom || fs){
		aspect_save_orig(width,height);
		aspect_save_prescale(d_width,d_height);
		aspect_save_screenres(fb_xres,fb_yres);
		aspect(&image_width,&image_height,fs ? A_ZOOM : A_NOZOOM);
	    } else {
		image_width=width;
		image_height=height;
	    }
		if(fb_xres > image_width)
		    x_offset = (fb_xres - image_width) / 2;
		else x_offset = 0;
		if(fb_yres > image_height)
		    y_offset = (fb_yres - image_height) / 2;
		else y_offset = 0;
		if(vidix_init(width,height,x_offset,y_offset,image_width,
			    image_height,format,fb_bpp,
			    fb_xres,fb_yres,info) != 0)
		{
		    printf(FBDEV "Can't initialize VIDIX driver\n");
		    vidix_name = NULL;
		    vidix_term();
		    return -1;
		}
		else printf(FBDEV "Using VIDIX\n");
		vidix_start();
	}
	else
#endif
	{
	    if ((frame_buffer = (uint8_t *) mmap(0, fb_size, PROT_READ | PROT_WRITE,
				    MAP_SHARED, fb_dev_fd, 0)) == (uint8_t *) -1) {
		printf(FBDEV "Can't mmap %s: %s\n", fb_dev_name, strerror(errno));
		return 1;
	    }
	    L123123875 = frame_buffer + (out_width - in_width) * fb_pixel_size /
		    2 + (out_height - in_height) * fb_line_len / 2;

	    if (verbose > 0) {
		if (verbose > 1) {
			printf(FBDEV "frame_buffer @ %p\n", frame_buffer);
			printf(FBDEV "L123123875 @ %p\n", L123123875);
		}
		printf(FBDEV "pixel per line: %d\n", fb_line_len / fb_pixel_size);
	    }

	    if (!(next_frame = (uint8_t *) malloc(in_width * in_height * fb_pixel_size))) {
		printf(FBDEV "Can't malloc next_frame: %s\n", strerror(errno));
		return 1;
	    }
	    if (fs || vm)
		memset(frame_buffer, '\0', fb_line_len * fb_yres);

	    if (format == IMGFMT_YV12)
		yuv2rgb_init(fb_bpp, MODE_RGB);
	}
	if (vt_doit && (vt_fd = open("/dev/tty", O_WRONLY)) == -1) {
		printf(FBDEV "can't open /dev/tty: %s\n", strerror(errno));
		vt_doit = 0;
	}
	if (vt_doit && !(vt_fp = fdopen(vt_fd, "w"))) {
		printf(FBDEV "can't fdopen /dev/tty: %s\n", strerror(errno));
		vt_doit = 0;
	}

	if (vt_doit)
		vt_set_textarea(last_row, fb_yres);

	return 0;
}

static uint32_t query_format(uint32_t format)
{
	int ret = VFCAP_OSD|VFCAP_CSP_SUPPORTED; /* osd/sub is supported on every bpp */

	if (!fb_preinit())
		return 0;
#ifdef CONFIG_VIDIX
	if(vidix_name)
		return (vidix_query_fourcc(format));
#endif
	if ((format & IMGFMT_BGR_MASK) == IMGFMT_BGR) {
		int bpp = format & 0xff;

		if (bpp == fb_bpp)
			return ret|VFCAP_CSP_SUPPORTED_BY_HW;
		else if (bpp == 15 && fb_bpp == 16)
			return ret;
		else if (bpp == 24 && fb_bpp == 32)
			return ret;
	}
	if (format == IMGFMT_YV12)
		return ret;
	return 0;
}

static const vo_info_t *get_info(void)
{
	return &vo_info;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	unsigned char *dst;
	int dstride;

#ifdef USE_CONVERT2FB
	if (pixel_format == IMGFMT_YV12) {
	  dst = L123123875 + (fb_xres * y0 + x0) * fb_pixel_size;
	  dstride = fb_xres * fb_pixel_size;
	}
	else
#endif
	  {
	    dst = next_frame + (in_width * y0 + x0) * fb_pixel_size;
	    dstride = in_width * fb_pixel_size;
	  }

	(*draw_alpha_p)(w, h, src, srca, stride, dst, dstride);
}

static uint32_t draw_frame(uint8_t *src[])
{
	if (pixel_format == IMGFMT_YV12) {
#ifdef USE_CONVERT2FB
		yuv2rgb(L123123875, src[0], src[1], src[2], fb_xres,
				fb_yres, fb_xres * fb_pixel_size,
				in_width, in_width / 2);
#else
		yuv2rgb(next_frame, src[0], src[1], src[2], in_width,
				in_height, in_width * fb_pixel_size,
				in_width, in_width / 2);
#endif

	} else if (flip) {
		int h = in_height;
		int len = in_width * fb_pixel_size;
		char *d = next_frame + (in_height - 1) * len;
		char *s = src[0];
		while (h--) {
			memcpy(d, s, len);
			s += len;
			d -= len;
		}
	} else {
		int sbpp = ((pixel_format & 0xff) + 7) / 8;
		char *d = next_frame;
		char *s = src[0];
		if (sbpp == fb_pixel_size) {
		    if (fb_real_bpp == 16 && pixel_format == (IMGFMT_BGR|15))
			rgb15to16(s, d, 2 * in_width * in_height);
		    else
			memcpy(d, s, sbpp * in_width * in_height);
		}
	}
	return 0;
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h, int x,
		int y)
{
	uint8_t *dest;

#ifdef USE_CONVERT2FB
	if (pixel_format == IMGFMT_YV12) {
	  if(x < fb_xres && y < fb_yres) {
	    if(x+w > fb_xres) w= fb_xres-x;
	    if(y+h > fb_yres) h= fb_yres-y;

	    dest = L123123875 + (fb_xres * y + x) * fb_pixel_size;
	    yuv2rgb(dest, src[0], src[1], src[2], w, h, fb_xres * fb_pixel_size,
		    stride[0], stride[1]);
	  }

	  return 0;
	}
#endif

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

#ifdef USE_CONVERT2FB
	if(pixel_format == IMGFMT_YV12)
	  return;
#endif

	for (i = 0; i < in_height; i++) {
		memcpy(L123123875 + out_offset, next_frame + in_offset,
				in_width * fb_pixel_size);
		out_offset += fb_line_len;
		in_offset += in_width * fb_pixel_size;
	}
}

static void draw_osd(void)
{
	vo_draw_text(in_width, in_height, draw_alpha);
}

static void flip_page(void)
{
	put_frame();
}

static void uninit(void)
{
	if (verbose > 0)
		printf(FBDEV "uninit\n");
	if (fb_cmap_changed) {
		if (ioctl(fb_dev_fd, FBIOPUTCMAP, &fb_oldcmap))
			printf(FBDEV "Can't restore original cmap\n");
		fb_cmap_changed = 0;
	}
	if(next_frame) free(next_frame);
	if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_vinfo))
		printf(FBDEV "ioctl FBIOGET_VSCREENINFO: %s\n", strerror(errno));
	fb_orig_vinfo.xoffset = fb_vinfo.xoffset;
	fb_orig_vinfo.yoffset = fb_vinfo.yoffset;
	if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_orig_vinfo))
		printf(FBDEV "Can't reset original fb_var_screeninfo: %s\n", strerror(errno));
        if (fb_tty_fd >= 0) {
                if (ioctl(fb_tty_fd, KDSETMODE, KD_TEXT) < 0)
                        printf(FBDEV "Can't restore text mode: %s\n", strerror(errno));
        }
	if (vt_doit)
		vt_set_textarea(0, fb_orig_vinfo.yres);
        close(fb_tty_fd);
	close(fb_dev_fd);
	if(frame_buffer) munmap(frame_buffer, fb_size);
#ifdef CONFIG_VIDIX
	if(vidix_name) vidix_term();
#endif
}

static uint32_t preinit(const char *arg)
{
    pre_init_err = 0;
#ifdef CONFIG_VIDIX
    if(vo_subdevice) parseSubDevice(vo_subdevice);
    if(vidix_name) pre_init_err = vidix_preinit(vidix_name,&video_out_fbdev);
    if(verbose > 2)
	printf("vo_subdevice: initialization returns: %i\n",pre_init_err);
#endif
    if(!pre_init_err) return (pre_init_err=(fb_preinit()?0:-1));
    return(-1);
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
