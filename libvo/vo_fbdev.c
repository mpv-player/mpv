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

extern int verbose;

/******************************
*	fb.modes support      *
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
		} else if (!strcmp(token[0], "accel")) {
			if (get_token(1) < 0)
				goto err_out_parse_error;
			/*
			 * it's only used for text acceleration
			 * so we just ignore it.
			 */
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
		} else if (!strcmp(token[0], "double")) {
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
	if (fb_modes) {
		free(fb_modes);
		fb_modes = NULL;
	}
	nr_modes = 0;
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

typedef struct {
	float min;
	float max;
} range_t;

static int in_range(range_t *r, float f)
{
	for (/* NOTHING */; (r->min != -1 && r->max != -1); r++) {
		if (f >= r->min && f <= r->max)
			return 1;
	}
	return 0;
}

static fb_mode_t *find_best_mode(int xres, int yres, range_t *hfreq,
		range_t *vfreq, range_t *dotclock)
{
	int i;
	fb_mode_t *best = fb_modes;
	fb_mode_t *curr;

	/* find first working mode */
	for (i = nr_modes - 1; i; i--, best++) {
		if (in_range(hfreq, hsf(best)) && in_range(vfreq, vsf(best)) &&
				in_range(dotclock, dcf(best)))
			break;
		if (verbose > 1)
			printf(FBDEV "can't set %dx%d\n", best->xres, best->yres);
	}

	if (!i)
		return NULL;
	if (i == 1)
		return best;

	for (curr = best + 1; i; i--, curr++) {
		if (!in_range(hfreq, hsf(curr)))
			continue;
		if (!in_range(vfreq, vsf(curr)))
			continue;
		if (!in_range(dotclock, dcf(curr)))
			continue;
		if (verbose > 1)
			printf(FBDEV "%dx%d ", curr->xres, curr->yres);
		if ((best->xres < xres || best->yres < yres) &&
				(curr->xres > best->xres ||
				 curr->yres > best->yres)) {
			if (verbose > 1)
				printf("better than %dx%d\n", best->xres,
						best->yres);
			best = curr;
		} else if (curr->xres >= xres && curr->yres >= yres) {
			if (curr->xres < best->xres && curr->yres < best->yres) {
				if (verbose > 1)
					printf("smaller than %dx%d\n",
							best->xres, best->yres);
				best = curr;
			} else if (curr->xres == best->xres &&
					curr->yres == best->yres &&
					(vsf(curr) > vsf(best))) {
				if (verbose > 1)
					printf("faster screen refresh\n");
				best = curr;
			} else if (verbose > 1)
				printf("\n");
		} else if (verbose > 1)
			printf("is too small\n");
	}
	return best;
}

static void set_bpp(struct fb_var_screeninfo *p, int bpp)
{
	p->bits_per_pixel = (bpp + 1) & ~1;
	p->red.msb_right = p->green.msb_right = p->blue.msb_right = 0;
	p->transp.offset = p->transp.length = 0;
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
			p->blue.offset = 0;
			break;
		case 16:
			p->red.offset = 11;
			p->green.length = 6;
			p->red.length = 5;
			p->green.offset = 5;
			p->blue.length = 5;
			p->blue.offset = 0;
			break;
		case 15:
			p->red.offset = 10;
			p->green.length = 5;
			p->red.length = 5;
			p->green.offset = 5;
			p->blue.length = 5;
			p->blue.offset = 0;
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

static range_t *str2range(char *s)
{
	float tmp_min, tmp_max;
	char *endptr = s;	// to start the loop
	range_t *r = NULL;
	int i, j;

	if (!s)
		return NULL;
	for (i = 0; *endptr; i++) {
		if (*s == ',')
			goto out_err;
		if (!(r = (range_t *) realloc(r, sizeof(*r) * (i + 2)))) {
			printf("can't realloc 'r'\n");
			return NULL;
		}
		tmp_min = strtod(s, &endptr);
		if (*endptr == 'k' || *endptr == 'K') {
			tmp_min *= 1000.0;
			endptr++;
		} else if (*endptr == 'm' || *endptr == 'M') {
			tmp_min *= 1000000.0;
			endptr++;
		}
		if (*endptr == '-') {
			tmp_max = strtod(endptr + 1, &endptr);
			if (*endptr == 'k' || *endptr == 'K') {
				tmp_max *= 1000.0;
				endptr++;
			} else if (*endptr == 'm' || *endptr == 'M') {
				tmp_max *= 1000000.0;
				endptr++;
			}
			if (*endptr != ',' && *endptr)
				goto out_err;
		} else if (*endptr == ',' || !*endptr) {
			tmp_max = tmp_min;
		} else
			goto out_err;
		r[i].min = tmp_min;
		r[i].max = tmp_max;
		s = endptr + 1;
	}
	/* check if we have negative numbers... */
	for (j = 0; j < i; j++)
		if (r[j].min < 0 || r[j].max < 0)
			goto out_err;
	r[i].min = r[i].max = -1;
	return r;
out_err:
	if (r)
		free(r);
	return NULL;
}

/******************************
*	    vo_fbdev	      *
******************************/

/*
 * command line/config file options
 */
char *fb_dev_name = NULL;
char *fb_mode_cfgfile = "/etc/fb.modes";
char *fb_mode_name = NULL;
char *monitor_hfreq_str = NULL;
char *monitor_vfreq_str = NULL;
char *monitor_dotclock_str = NULL;

range_t *monitor_hfreq = NULL;
range_t *monitor_vfreq = NULL;
range_t *monitor_dotclock = NULL;

static int fb_preinit_done = 0;
static int fb_works = 0;
static int fb_dev_fd;
static size_t fb_size;
static uint8_t *frame_buffer;
static uint8_t *L123123875;	/* thx to .so */
static struct fb_fix_screeninfo fb_finfo;
static struct fb_var_screeninfo fb_orig_vinfo;
static struct fb_var_screeninfo fb_vinfo;
static struct fb_cmap fb_oldcmap;
static int fb_cmap_changed = 0;
static int fb_pixel_size;	// 32:  4  24:  3  16:  2  15:  2
static int fb_real_bpp;		// 32: 24  24: 24  16: 16  15: 15
static int fb_bpp;		// 32: 32  24: 24  16: 16  15: 15
static int fb_bpp_we_want;	// 32: 32  24: 24  16: 16  15: 15
static int fb_screen_width;
static fb_mode_t *fb_mode = NULL;

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

static int fb_preinit(void)
{
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

	fb_bpp = (fb_vinfo.bits_per_pixel == 32) ? 32 :
		(fb_vinfo.red.length + fb_vinfo.green.length +
		 fb_vinfo.blue.length);
	if (vo_dbpp) {
		if (vo_dbpp != 15 && vo_dbpp != 16 && vo_dbpp != 24 &&
				vo_dbpp != 32) {
			printf(FBDEV "can't switch to %d bpp\n", vo_dbpp);
			goto err_out;
		}
		fb_bpp = vo_dbpp;		
	}

	fb_preinit_done = 1;
	fb_works = 1;
	return 0;
err_out_fd:
	close(fb_dev_fd);
	fb_dev_fd = -1;
err_out:
	fb_preinit_done = 1;
	return 1;
}

static void clear_bg(void)
{
	int i, offset = 0;

	for (i = 0; i < out_height; i++, offset += fb_screen_width)
		memset(frame_buffer + offset, 0x0, out_width * fb_pixel_size);
}

static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width,
		uint32_t d_height, uint32_t fullscreen, char *title,
		uint32_t format)
{
#define FS	(fullscreen & 0x01)
#define VM	(fullscreen & 0x02)
#define ZOOM	(fullscreen & 0x04)

	struct fb_cmap *cmap;

	if (!fb_preinit_done)
		if (fb_preinit())
			return 1;
	if (!fb_works)
		return 1;

	if (ZOOM) {
		printf(FBDEV "-zoom is not supported\n");
		return 1;
	}
	if (fb_mode_name && !VM) {
		printf(FBDEV "-fbmode can be used only with -vm"
				" (is it the right behaviour?)\n");
		return 1;
	}
	if (VM)
		if (parse_fbmode_cfg(fb_mode_cfgfile) < 0)
			return 1;
	if ((!d_width + !d_height) == 1) {
		printf(FBDEV "use both -x and -y, or none of them\n");
		return 1;
	}
	if (d_width) {
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
	} else if (VM) {
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

	if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_vinfo)) {
		printf(FBDEV "Can't put VSCREENINFO: %s\n", strerror(errno));
		return 1;
	}
	if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
		printf(FBDEV "Can't get VSCREENINFO: %s\n", strerror(errno));
		return 1;
	}

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
	}
	if (ioctl(fb_dev_fd, FBIOGET_FSCREENINFO, &fb_finfo)) {
		printf(FBDEV "Can't get FSCREENINFO: %s\n", strerror(errno));
		return 1;
	}
	if (verbose > 0) {
		printf(FBDEV "fix info:\n");
		if (verbose > 1) {
			printf(FBDEV "id: %.16s\n", fb_finfo.id);
			printf(FBDEV "smem_start: %p\n", (void *) fb_finfo.smem_start);
		}
		printf(FBDEV "framebuffer size: %d bytes\n", fb_finfo.smem_len);
		printf(FBDEV "type: %lu\n", (unsigned long) fb_finfo.type);
		printf(FBDEV "type_aux: %lu\n", (unsigned long) fb_finfo.type_aux);
		printf(FBDEV "visual: %lu\n", (unsigned long) fb_finfo.visual);
		if (verbose > 1) {
			printf(FBDEV "xpanstep: %u\n", fb_finfo.xpanstep);
			printf(FBDEV "ypanstep: %u\n", fb_finfo.ypanstep);
			printf(FBDEV "ywrapstep: %u\n", fb_finfo.ywrapstep);
		}
		printf(FBDEV "line_length: %lu bytes\n", (unsigned long) fb_finfo.line_length);
		if (verbose > 1) {
			printf(FBDEV "mmio_start: %p\n", (void *) fb_finfo.mmio_start);
			printf(FBDEV "mmio_len: %u bytes\n", fb_finfo.mmio_len);
			printf(FBDEV "accel: %u\n", fb_finfo.accel);
		}
	}
	switch (fb_finfo.type) {
		case FB_TYPE_VGA_PLANES:
			printf(FBDEV "FB_TYPE_VGA_PLANES not supported.\n");
			return 1;
		case FB_TYPE_PLANES:
			printf(FBDEV "FB_TYPE_PLANES not supported.\n");
			return 1;
		case FB_TYPE_INTERLEAVED_PLANES:
			printf(FBDEV "FB_TYPE_INTERLEAVED_PLANES not supported.\n");
			return 1;
#ifdef FB_TYPE_TEXT
		case FB_TYPE_TEXT:
			printf(FBDEV "FB_TYPE_TEXT not supported.\n");
			return 1;
#endif
		case FB_TYPE_PACKED_PIXELS:
			/* OK */
			if (verbose > 0)
				printf(FBDEV "FB_TYPE_PACKED_PIXELS: OK\n");
			break;
		default:
			printf(FBDEV "unknown FB_TYPE: %d\n", fb_finfo.type);
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
		case FB_VISUAL_PSEUDOCOLOR:
			printf(FBDEV "visual is FB_VISUAL_PSEUDOCOLOR."
					"it's not tested!\n");
			break;
		default:
			printf(FBDEV "visual: %d not yet supported\n",
					fb_finfo.visual);
			return 1;
	}
	if (FS || (d_width && VM)) {
		out_width = fb_vinfo.xres;
		out_height = fb_vinfo.yres;
	}
	if (out_width < in_width || out_height < in_height) {
		printf(FBDEV "screensize is smaller than video size\n");
		return 1;
	}

	fb_pixel_size = fb_vinfo.bits_per_pixel / 8;
	fb_real_bpp = fb_vinfo.red.length + fb_vinfo.green.length +
		fb_vinfo.blue.length;
	fb_bpp = (fb_pixel_size == 4) ? 32 : fb_real_bpp;
	if (fb_bpp_we_want != fb_bpp)
		printf(FBDEV "requested %d bpp, got %d bpp)!!!\n",
				fb_bpp_we_want, fb_bpp);
	fb_screen_width = fb_finfo.line_length;
	fb_size = fb_finfo.smem_len;
	if ((frame_buffer = (uint8_t *) mmap(0, fb_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, fb_dev_fd, 0)) == (uint8_t *) -1) {
		printf(FBDEV "Can't mmap %s: %s\n", fb_dev_name, strerror(errno));
		return 1;
	}
	L123123875 = frame_buffer + (out_width - in_width) * fb_pixel_size /
		2 + (out_height - in_height) * fb_screen_width / 2;

	if (verbose > 0) {
		printf(FBDEV "other:\n");
		if (verbose > 1) {
			printf(FBDEV "frame_buffer @ %p\n", frame_buffer);
			printf(FBDEV "L123123875 @ %p\n", L123123875);
		}
		printf(FBDEV "fb_bpp: %d\n", fb_bpp);
		printf(FBDEV "fb_real_bpp: %d\n", fb_real_bpp);
		printf(FBDEV "fb_pixel_size: %d bytes\n", fb_pixel_size);
		printf(FBDEV "pixel per line: %d\n", fb_screen_width / fb_pixel_size);
	}

	if (!(next_frame = (uint8_t *) malloc(in_width * in_height * fb_pixel_size))) {
		printf(FBDEV "Can't malloc next_frame: %s\n", strerror(errno));
		return 1;
	}

	if (format == IMGFMT_YV12)
		yuv2rgb_init(fb_bpp, MODE_RGB);
	clear_bg();
	return 0;
}

static uint32_t query_format(uint32_t format)
{
	int ret = 0x4; /* osd/sub supported on all bpp */

	if (!fb_preinit_done)
		if (fb_preinit())
			return 0;
	if (!fb_works)
		return 0;

	if ((format & IMGFMT_BGR_MASK) == IMGFMT_BGR) {
		int bpp = format & 0xff;

		if (bpp == fb_bpp)
			return ret|0x2;
		else if (bpp == 15 && fb_bpp == 16)
			return ret|0x1;
		else if (bpp == 24 && fb_bpp == 32)
			return ret|0x1;
	}
	if (format == IMGFMT_YV12)
		return ret|0x2;
	return 0;
}

static const vo_info_t *get_info(void)
{
	return &vo_info;
}

extern void vo_draw_alpha_rgb32(int w, int h, unsigned char* src,
		unsigned char *srca, int srcstride, unsigned char* dstbase,
		int dststride);
extern void vo_draw_alpha_rgb24(int w, int h, unsigned char* src,
		unsigned char *srca, int srcstride, unsigned char* dstbase,
		int dststride);
extern void vo_draw_alpha_rgb16(int w, int h, unsigned char* src,
		unsigned char *srca, int srcstride, unsigned char* dstbase,
		int dststride);
extern void vo_draw_alpha_rgb15(int w, int h, unsigned char* src,
		unsigned char *srca, int srcstride, unsigned char* dstbase,
		int dststride);

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	uint8_t *dst = next_frame + (in_width * y0 + x0) * fb_pixel_size;
	int dstride = in_width * fb_pixel_size;

	switch (fb_bpp) {
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
		memcpy(L123123875 + out_offset, next_frame + in_offset,
				in_width * fb_pixel_size);
		out_offset += fb_screen_width;
		in_offset += in_width * fb_pixel_size;
	}
}

extern void vo_draw_text(int dxs, int dys, void (*draw_alpha)(int x0, int y0,
			int w, int h, unsigned char *src, unsigned char *srca,
			int stride));

static void flip_page(void)
{
	vo_draw_text(in_width, in_height, draw_alpha);
	check_events();
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
	free(next_frame);
	if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_vinfo))
		printf(FBDEV "ioctl FBIOGET_VSCREENINFO: %s\n", strerror(errno));
	fb_orig_vinfo.xoffset = fb_vinfo.xoffset;
	fb_orig_vinfo.yoffset = fb_vinfo.yoffset;
	if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_orig_vinfo))
		printf(FBDEV "Can't reset original fb_var_screeninfo: %s\n", strerror(errno));
	close(fb_dev_fd);
	munmap(frame_buffer, fb_size);
}
