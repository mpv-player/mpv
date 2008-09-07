/*
 * playback on Zoran cards
 * copyright (C) 2001, 2003 Rik Snel
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

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include "config.h"
#include "videodev_mjpeg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "mp_msg.h"
#include "m_option.h"
#include "fastmemcpy.h"

#include "jpeg_enc.h"

static const vo_info_t info = 
{
	"Zoran ZR360[56]7/ZR36060 Driver (DC10(+)/buz/lml33/MatroxRR)",
	"zr",
	"Rik Snel <rsnel@cube.dyndns.org>",
	""
};

const LIBVO_EXTERN (zr)

#define	ZR_MAX_DEVICES 4
/* General variables */

typedef struct {
	int width;
	int height;
	int xoff;
	int yoff;
	int set;
} geo_t;

static int zr_count = 1;
static int zr_parsing = 0;
static int framenum;

typedef struct {
	/* commandline args given for this device (and defaults) */
	int vdec, hdec; 	/* requested decimation 1,2,4 */
	int fd; 		/* force decimation */
	int xdoff, ydoff;	/* offset from upperleft of screen 
				 * default is 'centered' */
	int quality; 		/* jpeg quality 1=best, 20=bad */
	geo_t g;		/* view window (zrcrop) */
	char *device;		/* /dev/video1 */
	int bw;			/* if bw == 1, display in black&white */
	int norm;	 	/* PAL/NTSC */

	/* buffers + pointers + info */

	unsigned char *image;
	int image_width, image_height, size;
	int off_y, off_c, stride;    /* for use by 'draw slice/frame' */

	unsigned char *buf;   /* the jpeg images will be placed here */
	jpeg_enc_t *j;
	unsigned char *y_data, *u_data, *v_data; /* used by the jpeg encoder */
	int y_stride, u_stride, v_stride; /* these point somewhere in image */

	/* information for (and about) the zoran card */

	int vdes;			/* file descriptor of card */
	int frame, synco, queue; 	/* buffer management */
	struct mjpeg_sync zs;		/* state information */
	struct mjpeg_params p;
	struct mjpeg_requestbuffers zrq;
	struct video_capability vc;	/* max resolution and so on */
	int fields, stretchy; /* must the *image be interlaced
					   or stretched to fit on the screen? */
} zr_info_t;

static zr_info_t zr_info[ZR_MAX_DEVICES] = {
	{1, 1, 1, -1, -1, 2, {0, 0, 0, 0, 0}, NULL, 0, VIDEO_MODE_AUTO, NULL, 0, 0, 0, 0, 0, 
	0, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, -1, -1, 2, {0, 0, 0, 0, 0}, NULL, 0, VIDEO_MODE_AUTO, NULL, 0, 0, 0, 0, 0, 
	0, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, -1, -1, 2, {0, 0, 0, 0, 0}, NULL, 0, VIDEO_MODE_AUTO, NULL, 0, 0, 0, 0, 0, 
	0, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, -1, -1, 2, {0, 0, 0, 0, 0}, NULL, 0, VIDEO_MODE_AUTO, NULL, 0, 0, 0, 0, 0, 
	0, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};




#define MJPEG_NBUFFERS	2
#define MJPEG_SIZE	1024*256


int zoran_getcap(zr_info_t *zr) {
	char* dev = NULL;

	if (zr->device)
		dev = zr->device;
	else {
		struct stat vstat;
		const char *devs[] = {
		    "/dev/video",
		    "/dev/video0",
		    "/dev/v4l/video0",
		    "/dev/v4l0",
		    "/dev/v4l",
		    NULL
		};
		int i = 0;
		
		do
		{
		    if ((stat(devs[i], &vstat) == 0) && S_ISCHR(vstat.st_mode))
		    {
			dev = devs[i];
			mp_msg(MSGT_VO, MSGL_V, "zr: found video device %s\n", dev);
			break;
		    }
		} while (devs[++i] != NULL);

		if (!dev)
		{
		    mp_msg(MSGT_VO, MSGL_ERR, "zr: unable to find video device\n");
		    return 1;
		}
	}
			
	zr->vdes = open(dev, O_RDWR);

	if (zr->vdes < 0) {
		mp_msg(MSGT_VO, MSGL_ERR, "zr: error opening %s: %s\n", 
		       dev, strerror(errno));
		return 1;
	}

	/* before we can ask for the maximum resolution, we must set 
	 * the correct tv norm */

	if (ioctl(zr->vdes, MJPIOC_G_PARAMS, &zr->p) < 0) {
		mp_msg(MSGT_VO, MSGL_ERR, "zr: device at %s is probably not a DC10(+)/buz/lml33\n", dev);
		return 1;
	}
	
	if (zr->p.norm != zr->norm && zr->norm != VIDEO_MODE_AUTO) {
		/* attempt to set requested norm */
		zr->p.norm = zr->norm;
		if (ioctl(zr->vdes, MJPIOC_S_PARAMS, &zr->p) < 0) {
			mp_msg(MSGT_VO, MSGL_ERR,
				"zr: unable to change video norm, use another program to change it (XawTV)\n");
			return 1;
		}
		ioctl(zr->vdes, MJPIOC_G_PARAMS, &zr->p);
		if (zr->norm != zr->p.norm) {
			mp_msg(MSGT_VO, MSGL_ERR,
				"zr: unable to change video norm, use another program to change it (XawTV)\n");
			return 1;
		}
	}
	
	if (ioctl(zr->vdes, VIDIOCGCAP, &zr->vc) < 0) {
		mp_msg(MSGT_VO, MSGL_ERR, "zr: error getting video capabilities from %s\n", dev);
		return 1;
	}
	mp_msg(MSGT_VO, MSGL_V, "zr: MJPEG card reports maxwidth=%d, maxheight=%d\n", zr->vc.maxwidth, zr->vc.maxheight);
	
	return 0;
}
	
int init_zoran(zr_info_t *zr, int stretchx, int stretchy) {
	/* center the image, and stretch it as far as possible (try to keep
	 * aspect) and check if it fits */
	if (zr->image_width > zr->vc.maxwidth) {
		mp_msg(MSGT_VO, MSGL_ERR, "zr: movie to be played is too wide, max width currently %d\n", zr->vc.maxwidth);
		return 1;
	}

	if (zr->image_height > zr->vc.maxheight) {
		mp_msg(MSGT_VO, MSGL_ERR, "zr: movie to be played is too high, max height currently %d\n", zr->vc.maxheight);
		return 1;
	}

	zr->p.decimation = 0;
	zr->p.HorDcm = stretchx; 
	zr->p.VerDcm = stretchy;
	zr->p.TmpDcm = 1;
	zr->p.field_per_buff = zr->fields;
	if (zr->xdoff == -1) {
		zr->p.img_x = (zr->vc.maxwidth - 
				zr->p.HorDcm*(int)zr->image_width/zr->hdec)/2;
	} else {
		zr->p.img_x = zr->xdoff;
	}
	if (zr->ydoff == -1) {
		zr->p.img_y = (zr->vc.maxheight - zr->p.VerDcm*
				(3-zr->fields)*(int)zr->image_height)/4;
	} else {
		zr->p.img_y = zr->ydoff;
	}
	zr->p.img_width = zr->p.HorDcm*zr->image_width/zr->hdec;
	zr->p.img_height = zr->p.VerDcm*zr->image_height/zr->fields;
	mp_msg(MSGT_VO, MSGL_V, "zr: geometry (after 'scaling'): %dx%d+%d+%d fields=%d, w=%d, h=%d\n", zr->p.img_width, (3-zr->fields)*zr->p.img_height, zr->p.img_x, zr->p.img_y, zr->fields, zr->image_width/zr->hdec, zr->image_height);

	if (ioctl(zr->vdes, MJPIOC_S_PARAMS, &zr->p) < 0) {
		mp_msg(MSGT_VO, MSGL_ERR, "zr: error setting display parameters\n");
		return 1;
	}

	zr->zrq.count = MJPEG_NBUFFERS;
	zr->zrq.size = MJPEG_SIZE;

	if (ioctl(zr->vdes, MJPIOC_REQBUFS, &zr->zrq)) {
		mp_msg(MSGT_VO, MSGL_ERR, "zr: error requesting %ld buffers of size %ld\n", zr->zrq.count, zr->zrq.size);
		return 1;
	}

	/* the buffer count allocated may be different to the request */
	zr->buf = (unsigned char*)mmap(0, zr->zrq.count*zr->zrq.size, 
			PROT_READ|PROT_WRITE, MAP_SHARED, zr->vdes, 0);

	if (zr->buf == MAP_FAILED) {
		mp_msg(MSGT_VO, MSGL_ERR, "zr: error requesting %ld buffers of size %ld\n", zr->zrq.count, zr->zrq.size);
		return 1;
	}
	
	mp_msg(MSGT_VO, MSGL_V, "zr: got %ld buffers of size %ld (wanted %d buffers of size %d)\n", zr->zrq.count, zr->zrq.size, MJPEG_NBUFFERS, MJPEG_SIZE);
	if (zr->zrq.count < MJPEG_NBUFFERS) {
		mp_msg(MSGT_VO, MSGL_V, "zr: got not enough buffers\n");
		return 1;
	}

	zr->queue = 0;
	zr->synco = 0;

	return 0;
}

void uninit_zoran(zr_info_t *zr) {
	if (zr->image) {
		free(zr->image);
		zr->image=NULL;
	}
	while (zr->queue > zr->synco + 1) {
		if (ioctl(zr->vdes, MJPIOC_SYNC, &zr->zs) < 0) 
			mp_msg(MSGT_VO, MSGL_ERR, "zr: error waiting for buffers to become free\n"); 
		zr->synco++;
	}
	/* stop streaming */
	zr->frame = -1;
	if (ioctl(zr->vdes, MJPIOC_QBUF_PLAY, &zr->frame) < 0) 
		mp_msg(MSGT_VO, MSGL_ERR, "zr: error stopping playback of last frame\n");
	if (munmap(zr->buf,zr->zrq.count*zr->zrq.size))
	   mp_msg(MSGT_VO, MSGL_ERR, "zr: error unmapping buffer\n");
	close(zr->vdes);
}

int zr_geometry_sane(geo_t *g, unsigned int width, unsigned int height) {
	if (g->set) {
		if (g->width%2 != 0 || g->height%2 != 0 ||
				g->xoff%2 != 0 || g->yoff%2 != 0) {
			mp_msg(MSGT_VO, MSGL_ERR, "zr: arguments in -zrcrop must be multiples of 2\n");
			return 1;
		}
		if (g->width <= 0 || g->height <= 0 || 
				g->xoff < 0 || g->yoff < 0) {
			mp_msg(MSGT_VO, MSGL_ERR, "zr: width and height must be positive and offset nonnegative\n");
			return 1;
		}
		if (g->width + g->xoff > width) {
			mp_msg(MSGT_VO, MSGL_ERR, "zr: width+xoffset (%d+%d>%d) is too big\n", g->width, g->xoff, width);
			return 1;
		}
		if (g->height + g->yoff > height) {
			mp_msg(MSGT_VO, MSGL_ERR, "zr: height+yoffset (%d+%d>%d) is too big\n", g->height, g->yoff, height);
			return 1;
		}
	} else {
		g->width = width;
		g->height = height;
		g->xoff = 0;
		g->yoff = 0;
		g->set = 1;
	}
	return 0;
}


static int config(uint32_t width, uint32_t height, uint32_t d_width, 
	uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
	int i, tmp, stretchx, stretchy;
	framenum = 0;
	if (format != IMGFMT_YV12 && format != IMGFMT_YUY2) {
		printf("vo_zr called with wrong format");
		return 1;
	}
	for (i = 0; i < zr_count; i++) {
		zr_info_t *zr = &zr_info[i];
		geo_t *g = &zr->g;

		zr->stride = 2*width;
		if (zr_geometry_sane(g, width, height)) return 1;

		/* we must know the maximum resolution of the device
	 	 * it differs for DC10+ and buz for example */
		zoran_getcap(zr); /*must be called before init_zoran */
		/* make the scaling decision
		 * we are capable of stretching the image in the horizontal
		 * direction by factors 1, 2 and 4
		 * we can stretch the image in the vertical direction by a 
		 * factor of 1 and 2 AND we must decide about interlacing */
		if (g->width > zr->vc.maxwidth/2 || 
				g->height > zr->vc.maxheight/2) {
			stretchx = 1;
			stretchy = 1;
			zr->fields = 2;
			if (zr->vdec == 2) {
				zr->fields = 1;
			} else if (zr->vdec == 4) {
				zr->fields = 1;
				stretchy = 2;
			}
			stretchx = zr->hdec;
		} else if (g->width > zr->vc.maxwidth/4 || 
				g->height > zr->vc.maxheight/4) {
			stretchx = 2;
			stretchy = 1;
			zr->fields = 1;
			if (zr->vdec == 2) {
				stretchy = 2;
			} else if (zr->vdec == 4) {
				if (!zr->fd) {
					mp_msg(MSGT_VO, MSGL_WARN, "zr: vertical decimation too high, changing to 2 (use -zrfd to keep vdec=4)\n");
					zr->vdec = 2;
				}
				stretchy = 2;
			}
			if (zr->hdec == 2) {
				stretchx = 4;
			} else if (zr->hdec == 4){
				if (!zr->fd) {
					mp_msg(MSGT_VO, MSGL_WARN, "zr: horizontal decimation too high, changing to 2 (use -zrfd to keep hdec=4)\n");
					zr->hdec = 2;
				}
				stretchx = 4;
			}
		} else {
			/* output image is maximally stretched */
			stretchx = 4;
			stretchy = 2;
			zr->fields = 1;
			if (zr->vdec != 1 && !zr->fd) {
				mp_msg(MSGT_VO, MSGL_WARN, "zr: vertical decimation too high, changing to 1 (use -zrfd to keep vdec=%d)\n", zr->vdec);
				zr->vdec = 1;
			}
			if (zr->hdec != 1 && !zr->fd) {
				mp_msg(MSGT_VO, MSGL_WARN, "zr: vertical decimation too high, changing to 1 (use -zrfd to keep hdec=%d)\n", zr->hdec);
				zr->hdec = 1;
			}
		}
		/* It can be that the original frame was too big for display,
		 * or that the width of the decimated image (for example) after
		 * padding up to a multiple of 16 has become too big. (orig
		 * width 720 (exactly right for the Buz) after decimation 360,
		 * after padding up to a multiple of 16 368, display 736 -> too
		 * large). In these situations we auto(re)crop. */
		tmp = 16*((g->width - 1)/(zr->hdec*16) + 1);
		if (stretchx*tmp > zr->vc.maxwidth) {
			g->xoff += 2*((g->width - zr->hdec*(tmp-16))/4);
			/* g->off must be a multiple of 2 */
			g->width = zr->hdec*(tmp - 16);
			g->set = 0; /* we abuse this field to 
				       report that g has changed*/
		}
		tmp = 8*zr->fields*((g->height - 1)/(zr->vdec*zr->fields*8)+1);
		if (stretchy*tmp > zr->vc.maxheight) {
			g->yoff += 2*((g->height - zr->vdec*
						(tmp - 8*zr->fields))/4);
			g->height = zr->vdec*(tmp - 8*zr->fields);
			g->set = 0;
		}
		if (!g->set) 
			mp_msg(MSGT_VO, MSGL_V, "zr: auto(re)cropping %dx%d+%d+%d to make the image fit on the screen\n", g->width, g->height, g->xoff, g->yoff);

		/* the height must be a multiple of fields*8 and the width
		 * must be a multiple of 16 */
		/* add some black borders to make it so, and center the image*/
		zr->image_height = zr->fields*8*((g->height/zr->vdec - 1)/
				(zr->fields*8) + 1);
		zr->image_width = (zr->hdec*16)*((g->width - 1)/(zr->hdec*16) + 1);
		zr->off_y = (zr->image_height - g->height/zr->vdec)/2;
		if (zr->off_y%2 != 0) zr->off_y++;
		zr->off_y *= zr->image_width;
		zr->off_c = zr->off_y/4;
		zr->off_y += (zr->image_width - g->width)/2;
		if (zr->off_y%2 != 0) zr->off_y--;
		zr->off_c += (zr->image_width - g->width)/4;
		zr->size = zr->image_width*zr->image_height;
		mp_msg(MSGT_VO, MSGL_V, "zr: input: %dx%d, cropped: %dx%d, output: %dx%d, off_y=%d, off_c=%d\n", width, height, g->width, g->height, zr->image_width, zr->image_height, zr->off_y, zr->off_c);
	
		zr->image = malloc(2*zr->size); /* this buffer allows for YUV422 data,
					 * so it is a bit too big for YUV420 */
		if (!zr->image) {
			mp_msg(MSGT_VO, MSGL_ERR, "zr: Memory exhausted\n");
			return 1;
		}
		/* and make sure that the borders are _really_ black */
		switch (format) {
			case IMGFMT_YV12:
				memset(zr->image, 0, zr->size);
				memset(zr->image + zr->size, 0x80, zr->size/4);
				memset(zr->image + 3*zr->size/2, 0x80, zr->size/4);
				zr->y_data = zr->image;
				zr->u_data = zr->image + zr->size;
				zr->v_data = zr->image + 3*zr->size/2;
	
				zr->y_stride = zr->image_width;
				zr->u_stride = zr->image_width/2;
				zr->v_stride = zr->image_width/2;
	
				zr->j = jpeg_enc_init(zr->image_width/zr->hdec, 
						zr->image_height/zr->fields,
						zr->hdec, zr->y_stride*zr->fields,
						zr->hdec, zr->u_stride*zr->fields,
						zr->hdec, zr->v_stride*zr->fields, 
						1, zr->quality, zr->bw);
				break;
			case IMGFMT_YUY2:
				for (tmp = 0; tmp < 2*zr->size; tmp+=4) {
					zr->image[tmp] = 0;
					zr->image[tmp+1] = 0x80;
					zr->image[tmp+2] = 0;
					zr->image[tmp+3] = 0x80;
				}
	
				zr->y_data = zr->image;
				zr->u_data = zr->image + 1;
				zr->v_data = zr->image + 3;
	
				zr->y_stride = 2*zr->image_width;
				zr->u_stride = 2*zr->image_width;
				zr->v_stride = 2*zr->image_width;
	
				zr->j = jpeg_enc_init(zr->image_width/zr->hdec, 
						zr->image_height/zr->fields,
						zr->hdec*2, 
						zr->y_stride*zr->fields,
						zr->hdec*4, 
						zr->u_stride*zr->fields,
						zr->hdec*4, 
						zr->v_stride*zr->fields,
						0, zr->quality, zr->bw);
				break;
			default:
				mp_msg(MSGT_VO, MSGL_FATAL, "zr: internal inconsistency in vo_zr\n");
		}
	
	
		if (zr->j == NULL) {
			mp_msg(MSGT_VO, MSGL_ERR, "zr: error initializing the jpeg encoder\n");
			return 1;
		}
	
		if (init_zoran(zr, stretchx, stretchy)) {
			return 1;
		}
	
	}
	return 0;
}

static void draw_osd(void) {
}

static void flip_page (void) {
	int i, j, k;
	//FILE *fp;
	//char filename[100];
	/* do we have a free buffer? */
	for (j = 0; j < zr_count; j++) {
		zr_info_t *zr = &zr_info[j];
		/* using MJPEG_NBUFFERS here, using the real number of 
		 * buffers may give sync issues (real number of buffers
		 * is always sufficient) */
		if (zr->queue-zr->synco < MJPEG_NBUFFERS) {
			zr->frame = zr->queue;
		} else {
			if (ioctl(zr->vdes, MJPIOC_SYNC, &zr->zs) < 0) 
				mp_msg(MSGT_VO, MSGL_ERR, "zr: error waiting for buffers to become free\n"); 
			zr->frame = zr->zs.frame;
			zr->synco++;
		}
		k=0;
		for (i = 0; i < zr->fields; i++) 
			k+=jpeg_enc_frame(zr->j, zr->y_data + i*zr->y_stride, 
					zr->u_data + i*zr->u_stride, 
					zr->v_data + i*zr->v_stride, 
					zr->buf + zr->frame*zr->zrq.size+k);
		if (k > zr->zrq.size) mp_msg(MSGT_VO, MSGL_WARN, "zr: jpeg image too large for maximum buffer size. Lower the jpeg encoding\nquality or the resolution of the movie.\n");
	}
	/* Warning: Only the first jpeg image contains huffman- and 
	 * quantisation tables, so don't expect files other than
	 * test0001.jpg to be readable */
	/*sprintf(filename, "test%04d.jpg", framenum);
	fp = fopen(filename, "w");
	if (!fp) exit(1);
	fwrite(buf+frame*zrq.size, 1, k, fp);
	fclose(fp);*/
	/*fp = fopen("test1.jpg", "r");
	fread(buf+frame*zrq.size, 1, 2126, fp);
	fclose(fp);*/
	
	for (j = 0; j < zr_count; j++) {
		zr_info_t *zr = &zr_info[j];
		if (ioctl(zr->vdes, MJPIOC_QBUF_PLAY, &zr->frame) < 0) 
			mp_msg(MSGT_VO, MSGL_ERR, "zr: error queueing buffer for playback\n");
		zr->queue++;
	}

	framenum++;
	return;
}

static int draw_frame(uint8_t * src[]) {
	int i, j;
	char *source, *dest;
	//printf("draw frame called\n");
	for (j = 0; j < zr_count; j++) {
		zr_info_t *zr = &zr_info[j];
		geo_t *g = &zr->g;
		source = src[0] + 2*g->yoff*zr->vdec*zr->stride + 2*g->xoff;
		dest = zr->image + 2*zr->off_y;
		for (i = 0; i < g->height/zr->vdec; i++) {
			fast_memcpy(dest, source, zr->image_width*2);
			dest += 2*zr->image_width;
			source += zr->vdec*zr->stride;
		}
	}
	return 0;
}

static int query_format(uint32_t format) {
	if(format==IMGFMT_YV12 || format==IMGFMT_YUY2) 
	    return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW;
	return 0;
}

static void uninit(void) {
	int j;
	mp_msg(MSGT_VO, MSGL_V, "zr: uninit called\n");
	for (j = 0; j < zr_count; j++) {
		jpeg_enc_uninit(zr_info[j].j);
		uninit_zoran(&zr_info[j]);
	}
}

static void check_events(void) {
}


static int draw_slice(uint8_t *srcimg[], int stride[],
		int wf, int hf, int xf, int yf) {
	int i, j, w, h, x, y;
	/* Apply 'geometry', crop unwanted parts */
	uint8_t *dst;
	//printf("before: w=%d, h=%d, x=%d, y=%d, src0=%p, src1=%p, src2=%p\n", w, h, x, y, srcimg[0], srcimg[1], srcimg[2]);
	for (j = 0; j < zr_count; j++) {
		uint8_t *src=srcimg[0];
		uint8_t *src1=srcimg[1];
		uint8_t *src2=srcimg[2];
		zr_info_t *zr = &zr_info[j];
		geo_t *g = &zr->g;
		w = wf; h = hf; x = xf; y = yf;
		if (x < g->xoff) {
			src += g->xoff - x;
			src1 += (g->xoff - x)/2;
			src2 += (g->xoff - x)/2;
			w -= g->xoff - x;
			if (w < 0) break; //return 0;
			x = 0 /*g.xoff*/;
		} else {
			x -= g->xoff;
		}
		if (x + w > g->width) {
			w = g->width - x;
			if (w < 0) break; //return 0;
		}
		if (y < g->yoff) {
			src += (g->yoff - y)*stride[0];
			src1 += ((g->yoff - y)/2)*stride[1];
			src2 += ((g->yoff - y)/2)*stride[2];
			h -= g->yoff - y;
			if (h < 0) break; //return 0;
			y = 0;
		} else {
			y -= g->yoff;
		}
		if (y + h > g->height) {
			h = g->height - y;
			if (h < 0) break; //return 0;
		}
		//printf("after: w=%d, h=%d, x=%d, y=%d, src0=%p, src1=%p, src2=%p\n", w, h, x, y, srcimg[0], srcimg[1], srcimg[2]);
		dst=zr->image + zr->off_y + zr->image_width*(y/zr->vdec)+x;
		// copy Y:
		for (i = 0; i < h; i++) {
			if ((i + x)%zr->vdec == 0) {
				fast_memcpy(dst,src,w);
				dst+=zr->image_width;
			}
			src+=stride[0];

		}
		if (!zr->bw) {
    			// copy U+V:
			uint8_t *dst1=zr->image + zr->size + zr->off_c+ (y/(zr->vdec*2))*zr->image_width/2+(x/2);
			uint8_t *dst2=zr->image + 3*zr->size/2 + zr->off_c + 
					(y/(zr->vdec*2))*
					zr->image_width/2+(x/2);
			for (i = 0; i< h/2; i++) {
				if ((i+x/2)%zr->vdec == 0) {
					fast_memcpy(dst1,src1,w/2);
					fast_memcpy(dst2,src2,w/2);
					dst1+=zr->image_width/2;
					dst2+=zr->image_width/2;
				}
				src1+=stride[1];
				src2+=stride[2];
			}
    		}
	}
 	return 0;
}


/* copied and adapted from vo_aa_parseoption */
int
vo_zr_parseoption(const m_option_t* conf, const char *opt, const char *param){
    /* got an option starting with zr */
    zr_info_t *zr = &zr_info[zr_parsing];
    int i;
    /* do WE need it ?, always */
    if (!strcasecmp(opt, "zrdev")) {
	if (param == NULL) return ERR_MISSING_PARAM;
	//if ((i=getcolor(param))==-1) return ERR_OUT_OF_RANGE;
	//aaopt_osdcolor=i;
	free(zr->device);
	zr->device = malloc(strlen(param)+1);
	strcpy(zr->device, param);
	mp_msg(MSGT_VO, MSGL_V, "zr: using device %s\n", zr->device);
	return 1;
    } else if (!strcasecmp(opt, "zrbw")) {
	    if (param != NULL) {
		    return ERR_OUT_OF_RANGE;
	    }
	    zr->bw = 1;
	    return 1;
    } else if (!strcasecmp(opt, "zrfd")) {
	    if (param != NULL) {
		    return ERR_OUT_OF_RANGE;
	    }
	    zr->fd = 1;
	    return 1;
    } else if (!strcasecmp(opt, "zrcrop")){
        geo_t *g = &zr->g;
	if (g->set == 1) {
		zr_parsing++;
		zr_count++;
    		zr = &zr_info[zr_parsing];
        	g = &zr->g;
		if (zr_count > 4) {
			mp_msg(MSGT_VO, MSGL_ERR, "zr: too many simultaneus display devices requested (max. is 4)\n");
			return ERR_OUT_OF_RANGE;
		}
	}
	if (param == NULL) return ERR_MISSING_PARAM;
	if (sscanf(param, "%dx%d+%d+%d", &g->width, &g->height, 
				&g->xoff, &g->yoff) != 4) {
		g->xoff = 0; g->yoff = 0;
		if (sscanf(param, "%dx%d", &g->width, &g->height) != 2) {
			mp_msg(MSGT_VO, MSGL_ERR, "zr: argument to -zrcrop must be of the form 352x288+16+0\n");
			return ERR_OUT_OF_RANGE;
		}
	}
	g->set = 1;
	mp_msg(MSGT_VO, MSGL_V, "zr: cropping %s\n", param);
	return 1;
    }else if (!strcasecmp(opt, "zrhdec")) {
        i = atoi(param);
	if (i != 1 && i != 2 && i != 4) return ERR_OUT_OF_RANGE;
	zr->hdec = i;
	return 1;
    }else if (!strcasecmp(opt, "zrvdec")) {
        i = atoi(param);
	if (i != 1 && i != 2 && i != 4) return ERR_OUT_OF_RANGE;
	zr->vdec = i;
	return 1;
    }else if (!strcasecmp(opt, "zrxdoff")) {
        i = atoi(param);
	zr->xdoff = i;
	return 1;
    }else if (!strcasecmp(opt, "zrydoff")) {
        i = atoi(param);
	zr->ydoff = i;
	return 1;
    }else if (!strcasecmp(opt, "zrquality")) {
        i = atoi(param);
	if (i < 1 || i > 20) return ERR_OUT_OF_RANGE;
	zr->quality = i;
	return 1;
    }else if (!strcasecmp(opt, "zrnorm")) {
	if (param == NULL) return ERR_MISSING_PARAM;
	if (!strcasecmp(param, "NTSC")) {
            mp_msg(MSGT_VO, MSGL_V, "zr: Norm set to NTSC\n");
            zr->norm = VIDEO_MODE_NTSC;
	    return 1;
	} else if (!strcasecmp(param, "PAL")) {
	    mp_msg(MSGT_VO, MSGL_V, "zr: Norm set to PAL\n");
            zr->norm = VIDEO_MODE_PAL;
	    return 1;
	} else {
           return ERR_OUT_OF_RANGE;
        }
    }else if (!strcasecmp(opt, "zrhelp")){
	printf("Help for -vo zr: Zoran ZR360[56]7/ZR36060 based MJPEG capture/playback cards\n");
	printf("\n");
	printf("Here are the zr options:\n");
	printf(
		    "\n"
		    "  -zrcrop     specify part of the input image that\n"
		    "              you want to see as an x-style geometry string\n"
		    "              example: -zrcrop 352x288+16+0\n"
		    "  -zrvdec     vertical decimation 1, 2 or 4\n"
		    "  -zrhdec     horizontal decimation 1, 2 or 4\n"
		    "  -zrfd       decimation is only done if the primitive\n"
		    "              hardware upscaler can correct for the decimation,\n"
		    "              this switch allows you to see the effects\n"
		    "              of too much decimation\n"
		    "  -zrbw       display in black&white (speed increase)\n"
		    "  -zrxdoff    x offset from upper-left of TV screen (default is 'centered')\n"
		    "  -zrydoff    y offset from upper-left of TV screen (default is 'centered')\n"
		    "  -zrquality  jpeg compression quality [BEST] 1 - 20 [VERY BAD]\n"
		    "  -zrdev      playback device (example -zrdev /dev/video1)\n"
		    "  -zrnorm     specify norm PAL/NTSC (default: leave at current setting)\n"
		    "\n"
		    "Cinerama support: additional occurances of -zrcrop activate cinerama mode,\n"
		    "suppose you have a 704x272 movie, two DC10+ cards and two beamers (or tv's),\n"              
		    "then you would issue the following command:\n\n"
		    "mplayer -vo zr -zrcrop 352x272+0+0 -zrdev /dev/video0 -zrcrop 352x272+352+0 \\\n"
		    "       -zrdev /dev/video1 movie.avi\n\n"
		    "Options appearing after the second -zrcrop apply to the second card, it is\n"
		    "possible to dispay at a different jpeg quality or at different decimations.\n\n"
		    "The parameters -zrxdoff and -zrydoff can be used to align the two images.\n"
		    "The maximum number of zoran cards participating in cinerama is 4, so you can\n"
		    "build a 2x2 vidiwall. (untested for obvious reasons, the setup wit a buz and\n"
		    "a DC10+ (and no beamers) is tested, however)\n"
	      );
	exit(0);
		
    }
    return ERR_NOT_AN_OPTION;
}

void vo_zr_revertoption(const m_option_t* opt,const char* param) {

  zr_info_t *zr = &zr_info[1];
  zr_count = 1;
  zr_parsing = 0;

  if (!strcasecmp(param, "zrdev")) {
    if(zr->device)
      free(zr->device);
    zr->device=NULL;
  } else if (!strcasecmp(param, "zrbw"))
    zr->bw=0;
  else if (!strcasecmp(param, "zrfd"))
    zr->fd=0;
  else if (!strcasecmp(param, "zrcrop"))
    zr->g.set = zr->g.xoff = zr->g.yoff = 0;
  else if (!strcasecmp(param, "zrhdec"))
    zr->hdec = 1;
  else if (!strcasecmp(param, "zrvdec"))
    zr->vdec = 1;
  else if (!strcasecmp(param, "zrxdoff"))
    zr->xdoff = -1;
  else if (!strcasecmp(param, "zrydoff"))
    zr->ydoff = -1;
  else if (!strcasecmp(param, "zrquality"))
    zr->quality = 2;
  else if (!strcasecmp(param, "zrnorm"))
    zr->norm = VIDEO_MODE_AUTO;

}

static int preinit(const char *arg)
{
    if(arg) 
    {
	printf("vo_zr: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }
    return 0;
}

static int control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
