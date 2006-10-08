/*
 * Video driver for Framebuffer device
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 * 
 * Some idea and code borrowed from Chris Lawrence's ppmtofb-0.27
 * Some fixes and small improvements by Joey Parrish <joey@nicewarrior.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <linux/fb.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "sub.h"
#ifdef CONFIG_VIDIX
#include "vosub_vidix.h"
#endif
#include "aspect.h"
#include "mp_msg.h"

static vo_info_t info = {
	"Framebuffer Device",
	"fbdev",
	"Szabolcs Berecz <szabi@inf.elte.hu>",
	""
};

LIBVO_EXTERN(fbdev)

#ifdef CONFIG_VIDIX
/* Name of VIDIX driver */
static const char *vidix_name = NULL;
static vidix_grkey_t gr_key;
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

#define MAX_NR_TOKEN	16

#define MAX_LINE_LEN	1000

#define RET_EOF		-1
#define RET_EOL		-2

static int validate_mode(fb_mode_t *m)
{
	if (!m->xres) {
		mp_msg(MSGT_VO, MSGL_V, "needs geometry ");
		return 0;
	}
	if (!m->pixclock) {
		mp_msg(MSGT_VO, MSGL_V, "needs timings ");
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
		mp_msg(MSGT_VO, MSGL_V, "get_token(): max >= MAX_NR_TOKEN!\n");
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
	if (!in_mode_def) {\
		mp_msg(MSGT_VO, MSGL_V, "'needs 'mode' first");\
		goto err_out_print_linenum;\
	}
	fb_mode_t *mode = NULL;
	char *endptr;	// strtoul()...
	int in_mode_def = 0;
	int tmp, i;

	/* If called more than once, reuse parsed data */
	if (nr_modes)
		return nr_modes;

	mp_msg(MSGT_VO, MSGL_V, "Reading %s: ", cfgfile);

	if ((fp = fopen(cfgfile, "r")) == NULL) {
		mp_msg(MSGT_VO, MSGL_V, "can't open '%s': %s\n", cfgfile, strerror(errno));
		return -1;
	}

	if ((line = (char *) malloc(MAX_LINE_LEN + 1)) == NULL) {
		mp_msg(MSGT_VO, MSGL_V, "can't get memory for 'line': %s\n", strerror(errno));
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
				mp_msg(MSGT_VO, MSGL_V, "'endmode' required");
				goto err_out_print_linenum;
			}
			if (!validate_mode(mode))
				goto err_out_not_valid;
		loop_enter:
		        if (!(fb_modes = (fb_mode_t *) realloc(fb_modes,
				sizeof(fb_mode_t) * (nr_modes + 1)))) {
			    mp_msg(MSGT_VO, MSGL_V, "can't realloc 'fb_modes' (nr_modes = %d):"
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
					mp_msg(MSGT_VO, MSGL_V, "mode name '%s' isn't unique", token[0]);
					goto err_out_print_linenum;
				}
			}
			if (!(mode->name = strdup(token[0]))) {
				mp_msg(MSGT_VO, MSGL_V, "can't strdup -> 'name': %s\n", strerror(errno));
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
	mp_msg(MSGT_VO, MSGL_V, "%d modes\n", nr_modes);
	free(line);
	fclose(fp);
	return nr_modes;
err_out_parse_error:
	mp_msg(MSGT_VO, MSGL_V, "parse error");
err_out_print_linenum:
	mp_msg(MSGT_VO, MSGL_V, " at line %d\n", line_num);
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
	mp_msg(MSGT_VO, MSGL_V, "previous mode is not correct");
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

	mp_msg(MSGT_VO, MSGL_DBG2, "mode %dx%d:", m->xres, m->yres);
	if (!in_range(hfreq, h)) {
		ret = 0;
		mp_msg(MSGT_VO, MSGL_DBG2, " hsync out of range.");
	}
	if (!in_range(vfreq, v)) {
		ret = 0;
		mp_msg(MSGT_VO, MSGL_DBG2, " vsync out of range.");
	}
	if (!in_range(dotclock, d)) {
		ret = 0;
		mp_msg(MSGT_VO, MSGL_DBG2, " dotclock out of range.");
	}
	if (ret)
		mp_msg(MSGT_VO, MSGL_DBG2, " hsync, vsync, dotclock ok.\n");
	else
		mp_msg(MSGT_VO, MSGL_DBG2, "\n");

	return ret;
}

static fb_mode_t *find_best_mode(int xres, int yres, range_t *hfreq,
		range_t *vfreq, range_t *dotclock)
{
	int i;
	fb_mode_t *best = fb_modes;
	fb_mode_t *curr;

	mp_msg(MSGT_VO, MSGL_DBG2, "Searching for first working mode\n");

	for (i = 0; i < nr_modes; i++, best++)
		if (mode_works(best, hfreq, vfreq, dotclock))
			break;

	if (i == nr_modes)
		return NULL;
	if (i == nr_modes - 1)
		return best;

	mp_msg(MSGT_VO, MSGL_DBG2, "First working mode: %dx%d\n", best->xres, best->yres);
	mp_msg(MSGT_VO, MSGL_DBG2, "Searching for better modes\n");

	for (curr = best + 1; i < nr_modes - 1; i++, curr++) {
		if (!mode_works(curr, hfreq, vfreq, dotclock))
			continue;

		if (best->xres < xres || best->yres < yres) {
			if (curr->xres > best->xres || curr->yres > best->yres) {
				mp_msg(MSGT_VO, MSGL_DBG2, "better than %dx%d, which is too small.\n",
							best->xres, best->yres);
				best = curr;
			} else
				mp_msg(MSGT_VO, MSGL_DBG2, "too small.\n");
		} else if (curr->xres == best->xres && curr->yres == best->yres &&
				vsf(curr) > vsf(best)) {
			mp_msg(MSGT_VO, MSGL_DBG2, "faster screen refresh.\n");
			best = curr;
		} else if ((curr->xres <= best->xres && curr->yres <= best->yres) &&
				(curr->xres >= xres && curr->yres >= yres)) {
			mp_msg(MSGT_VO, MSGL_DBG2, "better than %dx%d, which is too large.\n",
						best->xres, best->yres);
			best = curr;
		} else {
			if (curr->xres < xres || curr->yres < yres)
				mp_msg(MSGT_VO, MSGL_DBG2, "too small.\n");
			else if (curr->xres > best->xres || curr->yres > best->yres)
				mp_msg(MSGT_VO, MSGL_DBG2, "too large.\n");
			else mp_msg(MSGT_VO, MSGL_DBG2, "it's worse, don't know why.\n");
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
char *fb_mode_cfgfile = NULL;
char *fb_mode_name = NULL;

static fb_mode_t *fb_mode = NULL;

/* vt related variables */
static FILE *vt_fp = NULL;
static int vt_doit = 1;

/* vo_fbdev related variables */
static int fb_dev_fd;
static int fb_tty_fd = -1;
static size_t fb_size;
static uint8_t *frame_buffer;
static uint8_t *center;	/* thx .so :) */
static struct fb_fix_screeninfo fb_finfo;
static struct fb_var_screeninfo fb_orig_vinfo;
static struct fb_var_screeninfo fb_vinfo;
static unsigned short fb_ored[256], fb_ogreen[256], fb_oblue[256];
static struct fb_cmap fb_oldcmap = { 0, 256, fb_ored, fb_ogreen, fb_oblue };
static int fb_cmap_changed = 0;
static int fb_pixel_size;	// 32:  4  24:  3  16:  2  15:  2
static int fb_bpp;		// 32: 32  24: 24  16: 16  15: 15
static int fb_bpp_we_want;	// 32: 32  24: 24  16: 16  15: 15
static int fb_line_len;
static int fb_xres;
static int fb_yres;
static void (*draw_alpha_p)(int w, int h, unsigned char *src,
		unsigned char *srca, int stride, unsigned char *dst,
		int dstride);

static int in_width;
static int in_height;
static int out_width;
static int out_height;
static int first_row;
static int last_row;
static uint32_t pixel_format;
static int fs;

/*
 * Note: this function is completely cut'n'pasted from
 * Chris Lawrence's code.
 * (modified a bit to fit in my code...)
 */
static struct fb_cmap *make_directcolor_cmap(struct fb_var_screeninfo *var)
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
	  mp_msg(MSGT_VO, MSGL_V, "Can't allocate red palette with %d entries.\n", cols);
	  return NULL;
  }
  for(i=0; i< rcols; i++)
    red[i] = (65535/(rcols-1)) * i;
  
  green = malloc(cols * sizeof(green[0]));
  if(!green) {
	  mp_msg(MSGT_VO, MSGL_V, "Can't allocate green palette with %d entries.\n", cols);
	  free(red);
	  return NULL;
  }
  for(i=0; i< gcols; i++)
    green[i] = (65535/(gcols-1)) * i;
  
  blue = malloc(cols * sizeof(blue[0]));
  if(!blue) {
	  mp_msg(MSGT_VO, MSGL_V, "Can't allocate blue palette with %d entries.\n", cols);
	  free(red);
	  free(green);
	  return NULL;
  }
  for(i=0; i< bcols; i++)
    blue[i] = (65535/(bcols-1)) * i;
  
  cmap = malloc(sizeof(struct fb_cmap));
  if(!cmap) {
	  mp_msg(MSGT_VO, MSGL_V, "Can't allocate color map\n");
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


static int fb_preinit(int reset)
{
	static int fb_preinit_done = 0;
	static int fb_works = 0;

	if (reset)
	{
	    fb_preinit_done = 0;
	    return 0;
	}

	if (fb_preinit_done)
		return fb_works;

	if (!fb_dev_name && !(fb_dev_name = getenv("FRAMEBUFFER")))
		fb_dev_name = strdup("/dev/fb0");
	mp_msg(MSGT_VO, MSGL_V, "using %s\n", fb_dev_name);

	if ((fb_dev_fd = open(fb_dev_name, O_RDWR)) == -1) {
		mp_msg(MSGT_VO, MSGL_ERR, "Can't open %s: %s\n", fb_dev_name, strerror(errno));
		goto err_out;
	}
	if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
		mp_msg(MSGT_VO, MSGL_ERR, "Can't get VSCREENINFO: %s\n", strerror(errno));
		goto err_out_fd;
	}
	fb_orig_vinfo = fb_vinfo;

        if ((fb_tty_fd = open("/dev/tty", O_RDWR)) < 0) {
                mp_msg(MSGT_VO, MSGL_ERR, "notice: Can't open /dev/tty: %s\n", strerror(errno));
        }

	fb_bpp = fb_vinfo.red.length + fb_vinfo.green.length +
		fb_vinfo.blue.length + fb_vinfo.transp.length;

	if (fb_bpp == 8 && !vo_dbpp) {
		mp_msg(MSGT_VO, MSGL_ERR, "8 bpp output is not supported.\n");
		goto err_out_tty_fd;
	}

	if (vo_dbpp) {
		if (vo_dbpp != 15 && vo_dbpp != 16 && vo_dbpp != 24 &&
				vo_dbpp != 32) {
			mp_msg(MSGT_VO, MSGL_ERR, "can't switch to %d bpp\n", vo_dbpp);
			goto err_out_fd;
		}
		fb_bpp = vo_dbpp;		
	}
	
	if (!fb_mode_cfgfile)
	    fb_mode_cfgfile = strdup("/etc/fb.modes");

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
	mp_msg(MSGT_VO, MSGL_V, "var info:\n");
	mp_msg(MSGT_VO, MSGL_V, "xres: %u\n", fb_vinfo.xres);
	mp_msg(MSGT_VO, MSGL_V, "yres: %u\n", fb_vinfo.yres);
	mp_msg(MSGT_VO, MSGL_V, "xres_virtual: %u\n", fb_vinfo.xres_virtual);
	mp_msg(MSGT_VO, MSGL_V, "yres_virtual: %u\n", fb_vinfo.yres_virtual);
	mp_msg(MSGT_VO, MSGL_V, "xoffset: %u\n", fb_vinfo.xoffset);
	mp_msg(MSGT_VO, MSGL_V, "yoffset: %u\n", fb_vinfo.yoffset);
	mp_msg(MSGT_VO, MSGL_V, "bits_per_pixel: %u\n", fb_vinfo.bits_per_pixel);
	mp_msg(MSGT_VO, MSGL_V, "grayscale: %u\n", fb_vinfo.grayscale);
	mp_msg(MSGT_VO, MSGL_V, "red: %lu %lu %lu\n",
			(unsigned long) fb_vinfo.red.offset,
			(unsigned long) fb_vinfo.red.length,
			(unsigned long) fb_vinfo.red.msb_right);
	mp_msg(MSGT_VO, MSGL_V, "green: %lu %lu %lu\n",
			(unsigned long) fb_vinfo.green.offset,
			(unsigned long) fb_vinfo.green.length,
			(unsigned long) fb_vinfo.green.msb_right);
	mp_msg(MSGT_VO, MSGL_V, "blue: %lu %lu %lu\n",
			(unsigned long) fb_vinfo.blue.offset,
			(unsigned long) fb_vinfo.blue.length,
			(unsigned long) fb_vinfo.blue.msb_right);
	mp_msg(MSGT_VO, MSGL_V, "transp: %lu %lu %lu\n",
			(unsigned long) fb_vinfo.transp.offset,
			(unsigned long) fb_vinfo.transp.length,
			(unsigned long) fb_vinfo.transp.msb_right);
	mp_msg(MSGT_VO, MSGL_V, "nonstd: %u\n", fb_vinfo.nonstd);
	mp_msg(MSGT_VO, MSGL_DBG2, "activate: %u\n", fb_vinfo.activate);
	mp_msg(MSGT_VO, MSGL_DBG2, "height: %u\n", fb_vinfo.height);
	mp_msg(MSGT_VO, MSGL_DBG2, "width: %u\n", fb_vinfo.width);
	mp_msg(MSGT_VO, MSGL_DBG2, "accel_flags: %u\n", fb_vinfo.accel_flags);
	mp_msg(MSGT_VO, MSGL_DBG2, "timing:\n");
	mp_msg(MSGT_VO, MSGL_DBG2, "pixclock: %u\n", fb_vinfo.pixclock);
	mp_msg(MSGT_VO, MSGL_DBG2, "left_margin: %u\n", fb_vinfo.left_margin);
	mp_msg(MSGT_VO, MSGL_DBG2, "right_margin: %u\n", fb_vinfo.right_margin);
	mp_msg(MSGT_VO, MSGL_DBG2, "upper_margin: %u\n", fb_vinfo.upper_margin);
	mp_msg(MSGT_VO, MSGL_DBG2, "lower_margin: %u\n", fb_vinfo.lower_margin);
	mp_msg(MSGT_VO, MSGL_DBG2, "hsync_len: %u\n", fb_vinfo.hsync_len);
	mp_msg(MSGT_VO, MSGL_DBG2, "vsync_len: %u\n", fb_vinfo.vsync_len);
	mp_msg(MSGT_VO, MSGL_DBG2, "sync: %u\n", fb_vinfo.sync);
	mp_msg(MSGT_VO, MSGL_DBG2, "vmode: %u\n", fb_vinfo.vmode);
	mp_msg(MSGT_VO, MSGL_V, "fix info:\n");
	mp_msg(MSGT_VO, MSGL_V, "framebuffer size: %d bytes\n", fb_finfo.smem_len);
	mp_msg(MSGT_VO, MSGL_V, "type: %lu\n", (unsigned long) fb_finfo.type);
	mp_msg(MSGT_VO, MSGL_V, "type_aux: %lu\n", (unsigned long) fb_finfo.type_aux);
	mp_msg(MSGT_VO, MSGL_V, "visual: %lu\n", (unsigned long) fb_finfo.visual);
	mp_msg(MSGT_VO, MSGL_V, "line_length: %lu bytes\n", (unsigned long) fb_finfo.line_length);
	mp_msg(MSGT_VO, MSGL_DBG2, "id: %.16s\n", fb_finfo.id);
	mp_msg(MSGT_VO, MSGL_DBG2, "smem_start: %p\n", (void *) fb_finfo.smem_start);
	mp_msg(MSGT_VO, MSGL_DBG2, "xpanstep: %u\n", fb_finfo.xpanstep);
	mp_msg(MSGT_VO, MSGL_DBG2, "ypanstep: %u\n", fb_finfo.ypanstep);
	mp_msg(MSGT_VO, MSGL_DBG2, "ywrapstep: %u\n", fb_finfo.ywrapstep);
	mp_msg(MSGT_VO, MSGL_DBG2, "mmio_start: %p\n", (void *) fb_finfo.mmio_start);
	mp_msg(MSGT_VO, MSGL_DBG2, "mmio_len: %u bytes\n", fb_finfo.mmio_len);
	mp_msg(MSGT_VO, MSGL_DBG2, "accel: %u\n", fb_finfo.accel);
	mp_msg(MSGT_VO, MSGL_V, "fb_bpp: %d\n", fb_bpp);
	mp_msg(MSGT_VO, MSGL_V, "fb_pixel_size: %d bytes\n", fb_pixel_size);
	mp_msg(MSGT_VO, MSGL_V, "other:\n");
	mp_msg(MSGT_VO, MSGL_V, "in_width: %d\n", in_width);
	mp_msg(MSGT_VO, MSGL_V, "in_height: %d\n", in_height);
	mp_msg(MSGT_VO, MSGL_V, "out_width: %d\n", out_width);
	mp_msg(MSGT_VO, MSGL_V, "out_height: %d\n", out_height);
	mp_msg(MSGT_VO, MSGL_V, "first_row: %d\n", first_row);
	mp_msg(MSGT_VO, MSGL_V, "last_row: %d\n", last_row);
	mp_msg(MSGT_VO, MSGL_DBG2, "draw_alpha_p:%dbpp = %p\n", fb_bpp, draw_alpha_p);
}

static void vt_set_textarea(int u, int l)
{
	/* how can I determine the font height?
	 * just use 16 for now
	 */
	int urow = ((u + 15) / 16) + 1;
	int lrow = l / 16;

	mp_msg(MSGT_VO, MSGL_DBG2, "vt_set_textarea(%d,%d): %d,%d\n", u, l, urow, lrow);
	if(vt_fp) {
	  fprintf(vt_fp, "\33[%d;%dr\33[%d;%dH", urow, lrow, lrow, 0);
	  fflush(vt_fp);
	}
}

static int config(uint32_t width, uint32_t height, uint32_t d_width,
		uint32_t d_height, uint32_t flags, char *title,
		uint32_t format)
{
	struct fb_cmap *cmap;
	int vm = flags & VOFLAG_MODESWITCHING;
	int zoom = flags & VOFLAG_SWSCALE;
	int vt_fd;

	fs = flags & VOFLAG_FULLSCREEN;

	if(pre_init_err == -2)
	{
	    mp_msg(MSGT_VO, MSGL_ERR, "Internal fatal error: config() was called before preinit()\n");
	    return -1;
	}

	if (pre_init_err) return 1;

	if (fb_mode_name && !vm) {
		mp_msg(MSGT_VO, MSGL_ERR, "-fbmode can only be used with -vm\n");
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
			mp_msg(MSGT_VO, MSGL_ERR, "can't find requested video mode\n");
			return 1;
		}
		fb_mode2fb_vinfo(fb_mode, &fb_vinfo);
	} else if (vm) {
		monitor_hfreq = str2range(monitor_hfreq_str);
		monitor_vfreq = str2range(monitor_vfreq_str);
		monitor_dotclock = str2range(monitor_dotclock_str);
		if (!monitor_hfreq || !monitor_vfreq || !monitor_dotclock) {
			mp_msg(MSGT_VO, MSGL_ERR, "you have to specify the capabilities of"
					" the monitor.\n");
			return 1;
		}
		if (!(fb_mode = find_best_mode(out_width, out_height,
					monitor_hfreq, monitor_vfreq,
					monitor_dotclock))) {
			mp_msg(MSGT_VO, MSGL_ERR, "can't find best video mode\n");
			return 1;
		}
		mp_msg(MSGT_VO, MSGL_V, "using mode %dx%d @ %.1fHz\n", fb_mode->xres,
				fb_mode->yres, vsf(fb_mode));
		fb_mode2fb_vinfo(fb_mode, &fb_vinfo);
	}
	fb_bpp_we_want = fb_bpp;
	set_bpp(&fb_vinfo, fb_bpp);
	fb_vinfo.xres_virtual = fb_vinfo.xres;
	fb_vinfo.yres_virtual = fb_vinfo.yres;

        if (fb_tty_fd >= 0 && ioctl(fb_tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
                mp_msg(MSGT_VO, MSGL_V, "Can't set graphics mode: %s\n", strerror(errno));
                close(fb_tty_fd);
                fb_tty_fd = -1;
        }

	if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_vinfo)) {
		mp_msg(MSGT_VO, MSGL_ERR, "Can't put VSCREENINFO: %s\n", strerror(errno));
                if (fb_tty_fd >= 0 && ioctl(fb_tty_fd, KDSETMODE, KD_TEXT) < 0) {
                        mp_msg(MSGT_VO, MSGL_ERR, "Can't restore text mode: %s\n", strerror(errno));
                }
		return 1;
	}

	fb_pixel_size = fb_vinfo.bits_per_pixel / 8;
	fb_bpp = fb_vinfo.red.length + fb_vinfo.green.length +
		fb_vinfo.blue.length + fb_vinfo.transp.length;
	if (fb_bpp_we_want != fb_bpp)
		mp_msg(MSGT_VO, MSGL_WARN, "requested %d bpp, got %d bpp!!!\n",
				fb_bpp_we_want, fb_bpp);

	switch (fb_bpp) {
		case 32: draw_alpha_p = vo_draw_alpha_rgb32; break;
		case 24: draw_alpha_p = vo_draw_alpha_rgb24; break;
		case 16: draw_alpha_p = vo_draw_alpha_rgb16; break;
		case 15: draw_alpha_p = vo_draw_alpha_rgb15; break;
		default: return 1;
	}

	fb_xres = fb_vinfo.xres;
	fb_yres = fb_vinfo.yres;

	if (vm || fs) {
		out_width = fb_xres;
		out_height = fb_yres;
	}
	if (out_width < in_width || out_height < in_height) {
		mp_msg(MSGT_VO, MSGL_ERR, "screensize is smaller than video size\n");
		return 1;
	}

	first_row = (out_height - in_height) / 2;
	last_row = (out_height + in_height) / 2;

	if (ioctl(fb_dev_fd, FBIOGET_FSCREENINFO, &fb_finfo)) {
		mp_msg(MSGT_VO, MSGL_ERR, "Can't get FSCREENINFO: %s\n", strerror(errno));
		return 1;
	}

	lots_of_printf();

	if (fb_finfo.type != FB_TYPE_PACKED_PIXELS) {
		mp_msg(MSGT_VO, MSGL_ERR, "type %d not supported\n", fb_finfo.type);
		return 1;
	}

	switch (fb_finfo.visual) {
		case FB_VISUAL_TRUECOLOR:
			break;
		case FB_VISUAL_DIRECTCOLOR:
			mp_msg(MSGT_VO, MSGL_V, "creating cmap for directcolor\n");
			if (ioctl(fb_dev_fd, FBIOGETCMAP, &fb_oldcmap)) {
				mp_msg(MSGT_VO, MSGL_ERR, "can't get cmap: %s\n",
						strerror(errno));
				return 1;
			}
			if (!(cmap = make_directcolor_cmap(&fb_vinfo)))
				return 1;
			if (ioctl(fb_dev_fd, FBIOPUTCMAP, cmap)) {
				mp_msg(MSGT_VO, MSGL_ERR, "can't put cmap: %s\n",
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
			mp_msg(MSGT_VO, MSGL_ERR, "visual: %d not yet supported\n",
					fb_finfo.visual);
			return 1;
	}

	fb_line_len = fb_finfo.line_length;
	fb_size = fb_finfo.smem_len;
	frame_buffer = NULL;
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
			    fb_xres,fb_yres) != 0)
		{
		    mp_msg(MSGT_VO, MSGL_ERR, "Can't initialize VIDIX driver\n");
		    vidix_name = NULL;
		    vidix_term();
		    return -1;
		}
		else mp_msg(MSGT_VO, MSGL_V, "Using VIDIX\n");
		vidix_start();
		if (vidix_grkey_support())
		{
		    vidix_grkey_get(&gr_key);
		    gr_key.key_op = KEYS_PUT;
		    if (!(vo_colorkey & 0xff000000))
		    {
			gr_key.ckey.op = CKEY_TRUE;
			gr_key.ckey.red = (vo_colorkey & 0x00ff0000) >> 16;
			gr_key.ckey.green = (vo_colorkey & 0x0000ff00) >> 8;
			gr_key.ckey.blue = vo_colorkey & 0x000000ff;
		    }
		    else
			gr_key.ckey.op = CKEY_FALSE;
		    vidix_grkey_set(&gr_key);
		}
	}
	else
#endif
	{
	    int x_offset=0,y_offset=0;
	    if ((frame_buffer = (uint8_t *) mmap(0, fb_size, PROT_READ | PROT_WRITE,
				    MAP_SHARED, fb_dev_fd, 0)) == (uint8_t *) -1) {
		mp_msg(MSGT_VO, MSGL_ERR, "Can't mmap %s: %s\n", fb_dev_name, strerror(errno));
		return 1;
	    }

	    center = frame_buffer +
		    ( (out_width - in_width) / 2 ) * fb_pixel_size +
		    ( (out_height - in_height) / 2 ) * fb_line_len +
		    x_offset * fb_pixel_size + y_offset * fb_line_len;

	    mp_msg(MSGT_VO, MSGL_DBG2, "frame_buffer @ %p\n", frame_buffer);
	    mp_msg(MSGT_VO, MSGL_DBG2, "center @ %p\n", center);
	    mp_msg(MSGT_VO, MSGL_V, "pixel per line: %d\n", fb_line_len / fb_pixel_size);

	    if (fs || vm)
		memset(frame_buffer, '\0', fb_line_len * fb_yres);
	}
	if (vt_doit && (vt_fd = open("/dev/tty", O_WRONLY)) == -1) {
		mp_msg(MSGT_VO, MSGL_ERR, "can't open /dev/tty: %s\n", strerror(errno));
		vt_doit = 0;
	}
	if (vt_doit && !(vt_fp = fdopen(vt_fd, "w"))) {
		mp_msg(MSGT_VO, MSGL_ERR, "can't fdopen /dev/tty: %s\n", strerror(errno));
		vt_doit = 0;
	}

	if (vt_doit)
		vt_set_textarea(last_row, fb_yres);

	return 0;
}

static int query_format(uint32_t format)
{
	if (!fb_preinit(0))
		return 0;
#ifdef CONFIG_VIDIX
	if(vidix_name)
		return (vidix_query_fourcc(format));
#endif
	if ((format & IMGFMT_BGR_MASK) == IMGFMT_BGR) {
		int bpp = format & 0xff;

		if (bpp == fb_bpp)
			return VFCAP_ACCEPT_STRIDE | VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
	}
	return 0;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
	unsigned char *dst;

	dst = center + fb_line_len * y0 + fb_pixel_size * x0;

	(*draw_alpha_p)(w, h, src, srca, stride, dst, fb_line_len);
}

static int draw_frame(uint8_t *src[]) { return 1; }

static int draw_slice(uint8_t *src[], int stride[], int w, int h, int x,
		int y)
{
	uint8_t *d;
	uint8_t *s;

	d = center + fb_line_len * y + fb_pixel_size * x;

	s = src[0];
	while (h) {
		memcpy(d, s, w * fb_pixel_size);
		d += fb_line_len;
		s += stride[0];
		h--;
	}

	return 0;
}

static void check_events(void)
{
}

static void flip_page(void)
{
}

static void draw_osd(void)
{
	vo_draw_text(in_width, in_height, draw_alpha);
}

static void uninit(void)
{
	if (fb_cmap_changed) {
		if (ioctl(fb_dev_fd, FBIOPUTCMAP, &fb_oldcmap))
			mp_msg(MSGT_VO, MSGL_WARN, "Can't restore original cmap\n");
		fb_cmap_changed = 0;
	}
	if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_vinfo))
		mp_msg(MSGT_VO, MSGL_WARN, "ioctl FBIOGET_VSCREENINFO: %s\n", strerror(errno));
	fb_orig_vinfo.xoffset = fb_vinfo.xoffset;
	fb_orig_vinfo.yoffset = fb_vinfo.yoffset;
	if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_orig_vinfo))
		mp_msg(MSGT_VO, MSGL_WARN, "Can't reset original fb_var_screeninfo: %s\n", strerror(errno));
        if (fb_tty_fd >= 0) {
                if (ioctl(fb_tty_fd, KDSETMODE, KD_TEXT) < 0)
                        mp_msg(MSGT_VO, MSGL_WARN, "Can't restore text mode: %s\n", strerror(errno));
        }
	if (vt_doit)
		vt_set_textarea(0, fb_orig_vinfo.yres);
        close(fb_tty_fd);
	close(fb_dev_fd);
	if(frame_buffer) munmap(frame_buffer, fb_size);
	frame_buffer = NULL;
#ifdef CONFIG_VIDIX
	if(vidix_name) vidix_term();
#endif
	fb_preinit(1);
}

static int preinit(const char *vo_subdevice)
{
    pre_init_err = 0;

    if(vo_subdevice)
    {
#ifdef CONFIG_VIDIX
	if (memcmp(vo_subdevice, "vidix", 5) == 0)
	    vidix_name = &vo_subdevice[5];
	if(vidix_name)
	    pre_init_err = vidix_preinit(vidix_name,&video_out_fbdev);
	else
#endif
	{
	    if (fb_dev_name) free(fb_dev_name);
	    fb_dev_name = strdup(vo_subdevice);
	}
    }
    if(!pre_init_err) return (pre_init_err=(fb_preinit(0)?0:-1));
    return(-1);
}

static uint32_t get_image(mp_image_t *mpi)
{
    if (
	!IMGFMT_IS_BGR(mpi->imgfmt) ||
	(IMGFMT_BGR_DEPTH(mpi->imgfmt) != fb_bpp) ||
	((mpi->type != MP_IMGTYPE_STATIC) && (mpi->type != MP_IMGTYPE_TEMP)) ||
	(mpi->flags & MP_IMGFLAG_PLANAR) ||
	(mpi->flags & MP_IMGFLAG_YUV) ||
	(mpi->width != in_width) ||
	(mpi->height != in_height)
       )
    return(VO_FALSE);

    mpi->planes[0] = center;
    mpi->stride[0] = fb_line_len;
    mpi->flags |= MP_IMGFLAG_DIRECT;
    return(VO_TRUE);
}

static int control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_GET_IMAGE:
    return get_image(data);
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }

#ifdef CONFIG_VIDIX
  if (vidix_name) {
      switch (request) {
      case VOCTRL_SET_EQUALIZER:
	  {
	      va_list ap;
	      int value;
	      
	      va_start(ap, data);
	      value = va_arg(ap, int);
	      va_end(ap);
	      
	      return vidix_control(request, data, value);
         }
      case VOCTRL_GET_EQUALIZER:
	  {
	      va_list ap;
	      int *value;
	      
	      va_start(ap, data);
	      value = va_arg(ap, int*);
	      va_end(ap);
	      
	      return vidix_control(request, data, value);
	  }
      }
  }
#endif

  return VO_NOTIMPL;
}
