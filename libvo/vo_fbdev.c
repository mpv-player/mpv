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

LIBVO_EXTERN(fbdev)

//#include "yuv2rgb.h"

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
static int fb_bpp;
struct fb_fix_screeninfo fb_fix_info;
struct fb_var_screeninfo fb_var_info;

static int in_width;
static int in_height;
static int out_width;
static int out_height;
static uint8_t *next_frame;
static int screen_width;
static uint32_t pixel_format;

static int fb_init_done = 0;

static int fb_init(void)
{
	int fd, vt;
	char vt_name[11];
	struct vt_stat vt_state;
	struct vt_mode vt_mode;

#if 0
	/* get a free vt */
	if ((fd = open("/dev/tty0", O_WRONLY, 0)) == -1) {
		printf("Can't open /dev/tty0: %s\n", strerror(errno));
		return 1;
	}
	if (ioctl(fd, VT_OPENQRY, &vt) < 0 || vt == -1) {
		printf("Can't open a free VT: %s\n", strerror(errno));
		return 1;
	}
	close(fd);
#endif
#if 0
	/* open the vt */
	snprintf(vt_name, 10, "/dev/tty%d", vt);
	if ((vt_fd = open(vt_name, O_RDWR | O_NONBLOCK, 0)) == -1) {
		printf("Can't open %s: %s\n", vt_name, strerror(errno));
		return 1;
	}

	/* save the current vtnum */
	if (!ioctl(vt_fd, VT_GETSTATE, &vt_state))
		vt_active = vt_state.v_active;

	/* detach the controlling tty */
	if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
		ioctl(fd, TIOCNOTTY, 0);
		close(fd);
	}
#endif
#if 0
	/* switch to the new vt */
	if (ioctl(vt_fd, VT_ACTIVATE, vt_active))
		printf("ioctl VT_ACTIVATE: %s\n", strerror(errno));
	if (ioctl(vt_fd, VT_WAITACTIVE, vt_active))
		printf("ioctl VT_WAITACTIVE: %s\n", strerror(errno));
	if (ioctl(vt_fd, VT_GETMODE, &vt_mode) < 0) {
		printf("ioctl VT_GETMODE: %s\n", strerror(errno));
		return 1;
	}
	signal(SIGUSR1, vt_request);
	vt_mode.mode = VT_PROCESS;
	vt_mode.relsig = SIGUSR1;
	vt_mode.acqsig = SIGUSR1;
	if (ioctl(vt_fd, VT_SETMODE, &vt_mode) < 0) {
		printf("ioctl VT_SETMODE: %s\n", strerror(errno));
		return 1;
	}
#endif
	if (!fb_dev_name && !(fb_dev_name = getenv("FRAMEBUFFER")))
		fb_dev_name = "/dev/fb0";
	printf("fb_init: using %s\n", fb_dev_name);
	if ((fb_dev_fd = open(fb_dev_name, O_RDWR)) == -1) {
		printf("fb_init: Can't open %s: %s\n", fb_dev_name, strerror(errno));
		return 1;
	}
	if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_var_info)) {
		printf("fb_init: Can't get VSCREENINFO: %s\n", strerror(errno));
		return 1;
	}
	if (ioctl(fb_dev_fd, FBIOGET_FSCREENINFO, &fb_fix_info)) {
		printf("fb_init: Can't get VSCREENINFO: %s\n", strerror(errno));
		return 1;
	}
	switch (fb_fix_info.type) {
		case FB_TYPE_VGA_PLANES:
			printf("fb_init: FB_TYPE_VGA_PLANES not supported.\n");
			return 1;
			break;
		case FB_TYPE_PLANES:
			printf("fb_init: FB_TYPE_PLANES not supported.\n");
			return 1;
			break;
		case FB_TYPE_INTERLEAVED_PLANES:
			printf("fb_init: FB_TYPE_INTERLEAVED_PLANES not supported.\n");
			return 1;
			break;
#ifdef FB_TYPE_TEXT
		case FB_TYPE_TEXT:
			printf("fb_init: FB_TYPE_TEXT not supported.\n");
			return 1;
			break;
#endif
		case FB_TYPE_PACKED_PIXELS:
			/* OK */
			printf("fb_init: FB_TYPE_PACKED_PIXELS: OK\n");
			break;
		default:
			printf("fb_init: unknown FB_TYPE: %d\n", fb_fix_info.type);
			return 1;
	}			
	fb_bpp = fb_var_info.bits_per_pixel;
	screen_width = fb_fix_info.line_length;
	fb_size = fb_fix_info.smem_len;
	if ((frame_buffer = (uint8_t *) mmap(0, fb_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, fb_dev_fd, 0)) == (uint8_t *) -1) {
		printf("fb_init: Can't mmap %s: %s\n", fb_dev_name, strerror(errno));
		return 1;
	}
	close(fb_dev_fd);

	printf("fb_init: framebuffer @ %p\n", frame_buffer);
	printf("fb_init: framebuffer size: %d bytes\n", fb_size);
	printf("fb_init: bpp: %d\n", fb_bpp);
	printf("fb_init: pixel per line: %d\n", screen_width / (fb_bpp / 8));
	printf("fb_init: visual: %d\n", fb_fix_info.visual);

	fb_init_done = 1;
	return 0;
}

static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width,
		uint32_t d_height, uint32_t fullscreen, char *title,
		uint32_t format)
{
	if (!fb_init_done)
		if (fb_init())
			return 1;

	in_width = width;
	in_height = height;
	out_width = width;
	out_height = height;
	pixel_format = format;
	if (!(next_frame = (uint8_t *) malloc(in_width * in_height * (fb_bpp / 8)))) {
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
	printf("vo_fbdev: query_format(%#x): ", format);
//	if (format & IMGFMT_BGR_MASK == IMGFMT_BGR)
//		goto not_supported;
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
			if (fb_bpp == 32)
				goto supported;
			break;
		case IMGFMT_BGR|24:
			if (fb_bpp == 24)
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

	if (pixel_format == IMGFMT_YV12) {
		for (y = 0; y < h; y++){
			dst = next_frame + (in_width * (y0 + y) + x0) * (fb_bpp / 8);
			for (x = 0; x < w; x++) {
				if (srca[x]) {
					dst[0] = (dst[0]*(srca[x]^255)+src[x]*(srca[x]))>>8;
					dst[1] = (dst[1]*(srca[x]^255)+src[x]*(srca[x]))>>8;
					dst[2] = (dst[2]*(srca[x]^255)+src[x]*(srca[x]))>>8;
				}
				dst += fb_bpp / 8;
			}
			src += stride;
			srca += stride;
		}
	}
}

static uint32_t draw_frame(uint8_t *src[])
{
	if (pixel_format == IMGFMT_YV12) {
		yuv2rgb(next_frame, src[0], src[1], src[2], in_width,
				in_height, in_width * (fb_bpp / 8),
				in_width, in_width / 2);
	} else if ((pixel_format & IMGFMT_BGR_MASK) == IMGFMT_BGR) {
		memcpy(next_frame, src[0], in_width * in_height * (fb_bpp / 8));
	} else if ((pixel_format & IMGFMT_RGB_MASK) == IMGFMT_RGB) {
	}
	return 0;
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h, int x,
		int y)
{
	uint8_t *dest;

	dest = next_frame + (in_width * y + x) * (fb_bpp / 8);
	yuv2rgb(dest, src[0], src[1], src[2], w, h, in_width * (fb_bpp / 8),
			stride[0], stride[1]);
	return 0;
}

static void check_events(void)
{
}

static void flip_page(void)
{
	int i, out_offset = 0, in_offset = 0;

	vo_draw_text(in_width, in_height, draw_alpha);
	check_events();
	for (i = 0; i < in_height; i++) {
		memcpy(frame_buffer + out_offset, next_frame + in_offset,
				in_width * (fb_bpp / 8));
		out_offset += screen_width;
		in_offset += in_width * (fb_bpp / 8);
	}
}

static void uninit(void)
{
	if (vt_active >= 0)
		ioctl(vt_fd, VT_ACTIVATE, vt_active);
	printf("vo_fbdev: uninit\n");
	free(next_frame);
	munmap(frame_buffer, fb_size);
}
