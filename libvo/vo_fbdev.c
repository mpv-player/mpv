/*
 * Video driver for Framebuffer device
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * 
 * Some idea and code borrowed from Chris Lawrence's ppmtofb-0.27
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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

static int vt_active = -1;
static int vt_fd;

char *fb_dev_name = NULL;
static int fb_dev_fd;
static size_t fb_size;
static uint8_t *frame_buffer;
static int fb_pixel_size;
static int fb_bpp;
static int fb_bpp_on_screen;
struct fb_fix_screeninfo fb_fix_info;
struct fb_var_screeninfo fb_var_info;
static uint32_t fb_xres_virtual;
static uint32_t fb_yres_virtual;
static struct fb_cmap *oldcmap = NULL;

static int in_width;
static int in_height;
static int out_width;
static int out_height;
static uint8_t *next_frame;
static int screen_width;
static uint32_t pixel_format;

static int fb_init_done = 0;
static int fb_works = 0;

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

	if (!fb_dev_name && !(fb_dev_name = getenv("FRAMEBUFFER")))
		fb_dev_name = "/dev/fb0";
	printf("fb_init: using %s\n", fb_dev_name);

	if ((fb_dev_fd = open(fb_dev_name, O_RDWR)) == -1) {
		printf("fb_init: Can't open %s: %s\n", fb_dev_name, strerror(errno));
		goto err_out;
	}

	if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_var_info)) {
		printf("fb_init: Can't get VSCREENINFO: %s\n", strerror(errno));
		goto err_out_fd;
	}

	/* disable scrolling */
	fb_xres_virtual = fb_var_info.xres_virtual;
	fb_yres_virtual = fb_var_info.yres_virtual;
	fb_var_info.xres_virtual = fb_var_info.xres;
	fb_var_info.yres_virtual = fb_var_info.yres;

	if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_var_info)) {
		printf("fb_init: Can't put VSCREENINFO: %s\n", strerror(errno));
		goto err_out_fd;
	}

	if (ioctl(fb_dev_fd, FBIOGET_FSCREENINFO, &fb_fix_info)) {
		printf("fb_init: Can't get VSCREENINFO: %s\n", strerror(errno));
		goto err_out_fd;
		return 1;
	}
	switch (fb_fix_info.type) {
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
			printf("fb_init: unknown FB_TYPE: %d\n", fb_fix_info.type);
			goto err_out_fd;
	}
	if (fb_fix_info.visual == FB_VISUAL_DIRECTCOLOR) {
		printf("fb_init: creating cmap for directcolor\n");
		if (ioctl(fb_dev_fd, FBIOGETCMAP, oldcmap)) {
			printf("fb_init: can't get cmap: %s\n",
					strerror(errno));
			goto err_out_fd;
		}
		if (!(cmap = make_directcolor_cmap(&fb_var_info)))
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
	} else if (fb_fix_info.visual != FB_VISUAL_TRUECOLOR) {
		printf("fb_init: visual: %d not yet supported\n",
				fb_fix_info.visual);
		goto err_out_fd;
	}

	fb_pixel_size = fb_var_info.bits_per_pixel / 8;
	fb_bpp = fb_var_info.red.length + fb_var_info.green.length +
		fb_var_info.blue.length;
	fb_bpp_on_screen = (fb_pixel_size == 4) ? 32 : fb_bpp;
	screen_width = fb_fix_info.line_length;
	fb_size = fb_fix_info.smem_len;
	if ((frame_buffer = (uint8_t *) mmap(0, fb_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, fb_dev_fd, 0)) == (uint8_t *) -1) {
		printf("fb_init: Can't mmap %s: %s\n", fb_dev_name, strerror(errno));
		goto err_out_fd;
	}

	printf("fb_init: framebuffer @ %p\n", frame_buffer);
	printf("fb_init: framebuffer size: %d bytes\n", fb_size);
	printf("fb_init: bpp: %d\n", fb_bpp);
	printf("fb_init: bpp on screen: %d\n", fb_bpp_on_screen);
	printf("fb_init: pixel size: %d\n", fb_pixel_size);
	printf("fb_init: pixel per line: %d\n", screen_width / fb_pixel_size);
	printf("fb_init: visual: %d\n", fb_fix_info.visual);
	printf("fb_init: red: %d %d %d\n", fb_var_info.red.offset,
			fb_var_info.red.length, fb_var_info.red.msb_right);
	printf("fb_init: green: %d %d %d\n", fb_var_info.green.offset,
			fb_var_info.green.length, fb_var_info.green.msb_right);
	printf("fb_init: blue: %d %d %d\n", fb_var_info.blue.offset,
			fb_var_info.blue.length, fb_var_info.blue.msb_right);

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
//		yuv2rgb_init(fb_pixel_size * 8, MODE_RGB);
		yuv2rgb_init(fb_bpp_on_screen, MODE_RGB);
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
		if (bpp == fb_bpp_on_screen)
			return 1;
		else if (bpp == 15 && fb_bpp_on_screen == 16)
			return 1;
		else if (bpp == 24 && fb_bpp_on_screen == 32)
			return 1;
	}
	if (format == IMGFMT_YV12)
		return 1;
	return 0;
/*
	printf("vo_fbdev: query_format(%#x(%.4s)): ", format, &format);
	if (format & IMGFMT_BGR_MASK == IMGFMT_BGR)
		goto not_supported;
	switch (format) {
		case IMGFMT_YV12:
			goto supported;

		case IMGFMT_RGB32:
			if (fb_bpp == 32)
				goto supported;
			break;
		case IMGFMT_RGB24:
			if (fb_bpp == 24)
				goto supported;
			break;
		case IMGFMT_RGB16:
			if (fb_bpp == 16)
				goto supported;
			break;
		case IMGFMT_RGB15:
			if (fb_bpp == 15)
				goto supported;
			break;

		case IMGFMT_BGR|32:
			if (fb_bpp == 24 && fb_pixel_size == 4)
				goto supported;
			break;
		case IMGFMT_BGR|24:
			if (fb_bpp == 24 && fb_pixel_size == 3)
				goto supported;
			break;
		case IMGFMT_BGR|16:
			if (fb_bpp == 16)
				goto supported;
			break;
		case IMGFMT_BGR|15:
			if (fb_bpp == 15)
				goto supported;
			break;
	}
not_supported:
	printf("not_supported\n");
	return 0;
supported:
	printf("supported\n");
	return 1;
*/
}

static const vo_info_t *get_info(void)
{
	return &vo_info;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
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
			if (fb_bpp == 16 && pixel_format == (IMGFMT_BGR|15)) {
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
/*
	} else if ((pixel_format & IMGFMT_BGR_MASK) == IMGFMT_BGR) {
		if (pixel_format == fb_bpp_on_screen)
			memcpy(next_frame, src[0],
					in_width * in_height * fb_pixel_size);
		else {
			
		}
	}
*/
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
		out_offset += screen_width;
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
	if (oldcmap) {
		if (ioctl(fb_dev_fd, FBIOPUTCMAP, oldcmap))
			printf("vo_fbdev: Can't restore original cmap\n");
		oldcmap = NULL;
	}
	fb_var_info.xres_virtual = fb_xres_virtual;
	fb_var_info.yres_virtual = fb_yres_virtual;
	if (fb_dev_fd != -1) {
		if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_var_info))
			printf("vo_fbdev: Can't set virtual screensize to original value: %s\n", strerror(errno));
		close(fb_dev_fd);
	}
	memset(next_frame, '\0', in_height * in_width * fb_pixel_size);
	put_frame();
	if (vt_active >= 0)
		ioctl(vt_fd, VT_ACTIVATE, vt_active);
	free(next_frame);
	munmap(frame_buffer, fb_size);
}
