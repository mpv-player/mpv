/*
 * VIDIX driver for Hauppauge PVR 350
 *
 * Copyright 2007 Lutz Koschorreck
 * Based on genfb_vid.c and ivtv_xv.c
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#include <linux/videodev2.h>
#endif
#include <linux/ivtv.h>
#include <linux/fb.h>

#include "vidix.h"
#include "fourcc.h"
#include "dha.h"
#include "pci_ids.h"
#include "pci_names.h"

#define VIDIX_STATIC ivtv_

#define IVTV_MSG "[ivtv-vid] "
#define MAXLINE 128
#define IVTVMAXWIDTH 720
#define IVTVMAXHEIGHT 576

static int fbdev = -1;
static int yuvdev = -1;
static void *memBase = NULL;
static int frameSize = 0;
static int probed = 0;
static int ivtv_verbose;
static vidix_rect_t destVideo;
static vidix_rect_t srcVideo;
static unsigned char *outbuf = NULL;
double fb_width;
double fb_height;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
static struct ivtvfb_ioctl_state_info fb_state_old;
static struct ivtvfb_ioctl_state_info fb_state_hide;
#else
struct v4l2_format format_old, format_hide;
#endif
int alpha_disable = 0;

/* VIDIX exports */

static vidix_capability_t ivtv_cap =
{
	"Hauppauge PVR 350 YUV Video",
	"Lutz Koschorreck",
	TYPE_OUTPUT,
	{ 0, 0, 0, 0 },
	IVTVMAXWIDTH,
	IVTVMAXHEIGHT,
	4,
	4,
	-1,
	FLAG_UPSCALER|FLAG_DOWNSCALER,
	-1,
	-1,
	{ 0, 0, 0, 0 }
};

static void de_macro_y(unsigned char *src, unsigned char *dst,
	unsigned int w, unsigned int h, int src_x, int src_y, int height, int width)
{
	unsigned int x, y, i;
	unsigned char *dst_2;
	unsigned int h_tail, w_tail;
	unsigned int h_size, w_size;

	// Always round the origin, but compensate by increasing the size
	if (src_x & 15) {
		w += src_x & 15;
		src_x &= ~15;
	}

	if (src_y & 15) {
		h += src_y & 15;
		src_y &= ~15;
	}

	// The right / bottom edge might not be a multiple of 16
	h_tail = h & 15;
	w_tail = w & 15;

	// One block is 16 pixels high
	h_size = 16;

	// descramble Y plane
	for (y = 0; y < h; y += 16) {

		// Clip if we've reached the bottom & the size isn't a multiple of 16
		if (y + 16 > h) h_size = h_tail;

		for (x = 0; x < w; x += 16) {
			if (x + 16 > w)
				w_size = w_tail;
			else
				w_size = 16;

			dst_2 = dst + (720 * y) + (720 * src_y) + (256 * (src_x>>4)) + (x * 16);

			for (i = 0; i < h_size; i++) {
				memcpy(dst_2, src + src_x + x + (y + i) * width + (src_y * width), w_size);
				dst_2 += 16;
			}
		}
	}
}

static void de_macro_uv(unsigned char *srcu, unsigned char *srcv,
	unsigned char *dst, unsigned int w, unsigned int h, int src_x, int src_y,
	int height, int width)
{
	unsigned int x, y, i, f;
	unsigned char *dst_2;
	unsigned int h_tail, w_tail;
	unsigned int h_size;

	// The uv plane is half the size of the y plane, so 'correct' all dimensions.
	w /= 2;
	h /= 2;
	src_x /= 2;
	src_y /= 2;
	height /= 2;
	width /= 2;

	// Always round the origin, but compensate by increasing the size
	if (src_x & 7) {
		w += src_x & 7;
		src_x &= ~7;
	}

	if (src_y & 15) {
		h += src_y & 15;
		src_y &= ~15;
	}

	// The right / bottom edge may not be a multiple of 16
	h_tail = h & 15;
	w_tail = w & 7;

	h_size = 16;

	// descramble U/V plane
	for (y = 0; y < h; y += 16) {
		if ( y + 16 > h ) h_size = h_tail;
		for (x = 0; x < w; x += 8) {
			dst_2 = dst + (720 * y) + (720 * src_y) + (256 * (src_x>>3)) + (x * 32);
			if (x + 8 <= w) {
				for (i = 0; i < h_size; i++) {
					int idx = src_x + x + ((y + i) * width) + (src_y * width);
					dst_2[0] = srcu[idx + 0];
					dst_2[1] = srcv[idx + 0];
					dst_2[2] = srcu[idx + 1];
					dst_2[3] = srcv[idx + 1];
					dst_2[4] = srcu[idx + 2];
					dst_2[5] = srcv[idx + 2];
					dst_2[6] = srcu[idx + 3];
					dst_2[7] = srcv[idx + 3];
					dst_2[8] = srcu[idx + 4];
					dst_2[9] = srcv[idx + 4];
					dst_2[10] = srcu[idx + 5];
					dst_2[11] = srcv[idx + 5];
					dst_2[12] = srcu[idx + 6];
					dst_2[13] = srcv[idx + 6];
					dst_2[14] = srcu[idx + 7];
					dst_2[15] = srcv[idx + 7];
					dst_2 += 16;
				}
			}
			else {
				for (i = 0; i < h_size; i ++) {
					int idx = src_x + x + ((y + i) * width) + (src_y * width);
					for (f = 0; f < w_tail; f++) {
						dst_2[0] = srcu[idx + f];
						dst_2[1] = srcv[idx + f];
						dst_2 += 2;
					}
/*
					// Used for testing edge cutoff. Sets colour to Green
					for (f = w_tail;f < 8;f ++) {
						dst_2[0] = 0;
						dst_2[1] = 0;
						dst_2 += 2;
					}
*/
					dst_2 += 16 - (w_tail << 1);
				}
			}
		}
	}
}

int ivtv_probe(int verbose, int force)
{
	unsigned char fb_number = 0;
	char *device_name = NULL;
	char *alpha = NULL;
	struct fb_var_screeninfo vinfo;
	char fb_dev_name[] = "/dev/fb0\0";
	pciinfo_t lst[MAX_PCI_DEVICES];
	int err = 0;
	unsigned int i, num_pci = 0;
	unsigned char yuv_device_number = 48, yuv_device = 48 + fb_number;
	char yuv_device_name[] = "/dev/videoXXX\0";

	if(verbose)
		printf(IVTV_MSG"probe\n");

	ivtv_verbose = verbose;

	err = pci_scan(lst, &num_pci);
	if(err)	{
		printf(IVTV_MSG"Error occured during pci scan: %s\n", strerror(err));
		return err;
	}

	if(ivtv_verbose)
		printf(IVTV_MSG"Found %d pci devices\n", num_pci);

	for(i = 0; i < num_pci; i++) {
		if(2 == ivtv_verbose)
			printf(IVTV_MSG"Found chip [%04X:%04X] '%s' '%s'\n"
				,lst[i].vendor
				,lst[i].device
				,pci_vendor_name(lst[i].vendor)
				,pci_device_name(lst[i].vendor,lst[i].device));
		if(VENDOR_INTERNEXT == lst[i].vendor) {
			switch(lst[i].device)
			{
			case DEVICE_INTERNEXT_ITVC15_MPEG_2_ENCODER:
				if(ivtv_verbose)
					printf(IVTV_MSG"Found PVR 350\n");
				goto card_found;
			}
		}
	}

	if(ivtv_verbose)
		printf(IVTV_MSG"Can't find chip\n");
	return ENXIO;

card_found:

	device_name = getenv("FRAMEBUFFER");
	if(NULL == device_name) {
		device_name = fb_dev_name;
	}

	fb_number = atoi(device_name+strlen("/dev/fb"));

	fbdev = open(device_name, O_RDWR);
	if(-1 != fbdev) {
		if(ioctl(fbdev, FBIOGET_VSCREENINFO, &vinfo) < 0) {
			printf(IVTV_MSG"Unable to read screen info\n");
			close(fbdev);
			return ENXIO;
		} else {
			fb_width = vinfo.xres;
			fb_height = vinfo.yres;
			if(2 == ivtv_verbose) {
				printf(IVTV_MSG"framebuffer width : %3.0f\n",fb_width);
				printf(IVTV_MSG"framebuffer height: %3.0f\n",fb_height);
			}
		}
		if(NULL != (alpha = getenv("VIDIXIVTVALPHA"))) {
			if(0 == strcmp(alpha, "disable")) {
				alpha_disable = 1;
			}
		}
	} else {
		printf(IVTV_MSG"Failed to open /dev/fb%u\n", fb_number);
		return ENXIO;
	}

	/* Try to find YUV device */
	do {
		sprintf(yuv_device_name, "/dev/video%u", yuv_device);
		yuvdev = open(yuv_device_name, O_RDWR);
		if(-1 != yuvdev) {
			if(ivtv_verbose)
				printf(IVTV_MSG"YUV device found /dev/video%u\n", yuv_device);
			goto yuv_found;
		} else {
			if(ivtv_verbose)
				printf(IVTV_MSG"YUV device not found: /dev/video%u\n", yuv_device);
		}
	} while(yuv_device-- > yuv_device_number);
	return ENXIO;

yuv_found:
	if(0 == alpha_disable) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
		if(ioctl(fbdev, IVTVFB_IOCTL_GET_STATE, &fb_state_old) < 0) {
			printf(IVTV_MSG"Unable to read fb state\n");
			close(yuvdev);
			close(fbdev);
			return ENXIO;
		} else {
			if(ivtv_verbose) {
				printf(IVTV_MSG"old alpha : %ld\n",fb_state_old.alpha);
				printf(IVTV_MSG"old status: 0x%lx\n",fb_state_old.status);
			}
			fb_state_hide.alpha = 0;
			fb_state_hide.status = fb_state_old.status | IVTVFB_STATUS_GLOBAL_ALPHA;
		}
#else
		memset(&format_old, 0, sizeof(format_old));
		format_old.type = format_hide.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if(ioctl(yuvdev, VIDIOC_G_FMT , &format_old) < 0) {
			printf(IVTV_MSG"Unable to read fb state\n");
			close(yuvdev);
			close(fbdev);
			return ENXIO;
		} else {
			if(ivtv_verbose) {
				printf(IVTV_MSG"old alpha : %d\n",format_old.fmt.win.global_alpha);
			}
			memcpy(&format_hide, &format_old, sizeof(format_old));
			format_hide.fmt.win.global_alpha = 0;
		}
#endif
	}
	probed = 1;
	return 0;
}

int ivtv_init(const char *args)
{
	if(ivtv_verbose)
		printf(IVTV_MSG"init\n");

	if (!probed) {
		if(ivtv_verbose)
			printf(IVTV_MSG"Driver was not probed but is being initialized\n");
		return EINTR;
	}
	outbuf = malloc((IVTVMAXHEIGHT * IVTVMAXWIDTH) + (IVTVMAXHEIGHT * IVTVMAXWIDTH / 2));
	if(NULL == outbuf) {
		if(ivtv_verbose)
			printf(IVTV_MSG"Not enough memory availabe!\n");
		return EINTR;
	}
	return 0;
}

void ivtv_destroy(void)
{
	if(ivtv_verbose)
		printf(IVTV_MSG"destroy\n");
	if(-1 != yuvdev)
		close(yuvdev);
	if(-1 != fbdev)
		close(fbdev);
	if(NULL != outbuf)
		free(outbuf);
	if(NULL != memBase)
		free(memBase);
}

int ivtv_get_caps(vidix_capability_t *to)
{
	if(ivtv_verbose)
		printf(IVTV_MSG"GetCap\n");
	memcpy(to, &ivtv_cap, sizeof(vidix_capability_t));
	return 0;
}

int ivtv_query_fourcc(vidix_fourcc_t *to)
{
	int supports = 0;

	if(ivtv_verbose)
		printf(IVTV_MSG"query fourcc (%x)\n", to->fourcc);

	switch(to->fourcc)
	{
	case IMGFMT_YV12:
		supports = 1;
		break;
	default:
		supports = 0;
	}

	if(!supports) {
		to->depth = to->flags = 0;
		return ENOTSUP;
	}
	to->depth = VID_DEPTH_12BPP |
		VID_DEPTH_15BPP | VID_DEPTH_16BPP |
		VID_DEPTH_24BPP | VID_DEPTH_32BPP;
	to->flags = 0;
	return 0;
}

int ivtv_config_playback(vidix_playback_t *info)
{
	if(ivtv_verbose)
		printf(IVTV_MSG"config playback\n");

	if(2 == ivtv_verbose){
		printf(IVTV_MSG"src : x:%d y:%d w:%d h:%d\n",
			info->src.x, info->src.y, info->src.w, info->src.h);
		printf(IVTV_MSG"dest: x:%d y:%d w:%d h:%d\n",
			info->dest.x, info->dest.y, info->dest.w, info->dest.h);
	}

	memcpy(&destVideo, &info->dest, sizeof(vidix_rect_t));
	memcpy(&srcVideo, &info->src, sizeof(vidix_rect_t));

	info->num_frames = 2;
	info->frame_size = frameSize = info->src.w*info->src.h+(info->src.w*info->src.h)/2;
	info->dest.pitch.y = 1;
	info->dest.pitch.u = info->dest.pitch.v = 2;
	info->offsets[0] = 0;
	info->offsets[1] = info->frame_size;
	info->offset.y = 0;
	info->offset.u = info->src.w * info->src.h;
	info->offset.v = info->offset.u + ((info->src.w * info->src.h)/4);
	info->dga_addr = memBase = malloc(info->num_frames*info->frame_size);
	if(ivtv_verbose)
		printf(IVTV_MSG"frame_size: %d, dga_addr: %p\n",
	info->frame_size, info->dga_addr);
	return 0;
}

int ivtv_playback_on(void)
{
	if(ivtv_verbose)
		printf(IVTV_MSG"playback on\n");

	if(0 == alpha_disable) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
		if (-1 != fbdev) {
			if (ioctl(fbdev, IVTVFB_IOCTL_SET_STATE, &fb_state_hide) < 0)
				printf (IVTV_MSG"Failed to set fb state\n");
		}
#else
		if (-1 != yuvdev) {
			if (ioctl(yuvdev, VIDIOC_S_FMT, &format_hide) < 0)
				printf (IVTV_MSG"Failed to set fb state\n");
		}
#endif
	}
	return 0;
}

int ivtv_playback_off(void)
{
	if(ivtv_verbose)
		printf(IVTV_MSG"playback off\n");

	if(0 == alpha_disable) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
		if (-1 != fbdev) {
			if (ioctl(fbdev, IVTVFB_IOCTL_SET_STATE, &fb_state_old) < 0)
				printf (IVTV_MSG"Failed to restore fb state\n");
		}
#else
		if (-1 != yuvdev) {
			if (ioctl(yuvdev, VIDIOC_S_FMT, &format_old) < 0)
				printf (IVTV_MSG"Failed to restore fb state\n");
		}
#endif
	}
	return 0;
}

int ivtv_frame_sel(unsigned int frame)
{

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
	struct ivtvyuv_ioctl_dma_host_to_ivtv_args args;
#else
	struct ivtv_dma_frame args;
#endif
	unsigned char *curMemBase = (unsigned char *)memBase + (frame * frameSize);

	de_macro_y(curMemBase, outbuf, srcVideo.w, srcVideo.h, srcVideo.x, srcVideo.y, srcVideo.h, srcVideo.w);
	de_macro_uv(curMemBase + (srcVideo.w * srcVideo.h) + srcVideo.w * srcVideo.h / 4,
		curMemBase + (srcVideo.w * srcVideo.h), outbuf + (IVTVMAXHEIGHT * IVTVMAXWIDTH),
		srcVideo.w,  srcVideo.h, srcVideo.x, srcVideo.y, srcVideo.h, srcVideo.w);

	args.y_source = outbuf;
	args.uv_source = outbuf + (IVTVMAXHEIGHT * IVTVMAXWIDTH);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
	args.src_x = srcVideo.x;
	args.src_y = srcVideo.y;
	args.dst_x = destVideo.x;
	args.dst_y = destVideo.y;
	args.src_w = srcVideo.w;
	args.dst_w = destVideo.w;
	args.srcBuf_width = srcVideo.w;
	args.src_h = srcVideo.h;
	args.dst_h = destVideo.h;
	args.srcBuf_height = srcVideo.h;
	args.yuv_type = 0;
#else
	args.src.left = srcVideo.x;
	args.src.top = srcVideo.y;
	args.dst.left = destVideo.x;
	args.dst.top = destVideo.y;
	args.src.width = srcVideo.w;
	args.dst.width = destVideo.w;
	args.src_width = srcVideo.w;
	args.src.height = srcVideo.h;
	args.dst.height = destVideo.h;
	args.src_height = srcVideo.h;
	args.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
	if(ioctl(yuvdev, IVTV_IOC_PREP_FRAME_YUV, &args) == -1) {
#else
	if(ioctl(yuvdev, IVTV_IOC_DMA_FRAME, &args) == -1) {
#endif
		printf("Ioctl IVTV_IOC_DMA_FRAME returned failed Error\n");
	}
	return 0;
}

VDXDriver ivtv_drv = {
	"ivtv",
	NULL,
	.probe = ivtv_probe,
	.get_caps = ivtv_get_caps,
	.query_fourcc = ivtv_query_fourcc,
	.init = ivtv_init,
	.destroy = ivtv_destroy,
	.config_playback = ivtv_config_playback,
	.playback_on = ivtv_playback_on,
	.playback_off = ivtv_playback_off,
	.frame_sel = ivtv_frame_sel,
};
