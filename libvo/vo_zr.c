/* 
 * vo_zr.c - playback on zoran cards 
 * Copyright (C) Rik Snel 2001,2002, License GNU GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include "zoran.h"

#include "config.h"
#define ZR_USES_LIBJPEG

#include "video_out.h"
#include "video_out_internal.h"
#include "../mp_msg.h"
#include "../cfgparser.h"
#include "fastmemcpy.h"

LIBVO_EXTERN (zr)

static vo_info_t vo_info = 
{
	"Zoran ZR360[56]7/ZR36060 Driver (DC10(+)/buz/lml33/MatroxRR)",
	"zr",
	"Rik Snel <snel@phys.uu.nl>",
	""
};

/* General variables */

static int image_width;
static int image_height;
static int off_y, off_c, stride; /* for use by 'draw slice' */
static int framenum;
static int fields = 1; /* currently no interlacing */
static int forceinter = 0;
static int vdec = 1;
static int size;
static int quality = 70;

typedef struct {
	int width;
	int height;
	int xoff;
	int yoff;
	int set;
} geo;
geo g = {0, 0, 0, 0, 0};

static uint8_t *image=NULL;
static uint8_t *buf=NULL;


/* Variables needed for Zoran */

int vdes;  /* the file descriptor of the video device */
int frame = 0, synco = 0, queue = 0; /* buffer management */
struct zoran_params zp;
struct zoran_requestbuffers zrq;
struct zoran_sync zs;
struct video_capability vc;
#define MJPEG_NBUFFERS	2
#define MJPEG_SIZE	1024*256

//should be command line options
int norm = VIDEO_MODE_AUTO;
#ifndef VO_ZR_DEFAULT_DEVICE
#define VO_ZR_DEFAULT_DEVICE "/dev/video"
#endif
char *device = NULL;


#ifdef ZR_USES_LIBJPEG
#include<jpeglib.h>
int ccount;
unsigned char *ccbuf;
struct jpeg_compress_struct cinfo;
struct jpeg_destination_mgr jdest;
struct jpeg_error_mgr jerr;

/* minimal destination handler to output to buffer */
METHODDEF(void) init_destination(struct jpeg_compress_struct *cinfo) {
//	printf("init_destination called %p %d\n", ccbuf, ccount);
	cinfo->dest->next_output_byte = (JOCTET*)(ccbuf+ccount);
	cinfo->dest->free_in_buffer = MJPEG_SIZE - ccount;
}

METHODDEF(boolean) empty_output_buffer(struct jpeg_compress_struct *cinfo) {
//	printf("empty_output_buffer called\n");
	mp_msg(MSGT_VO, MSGL_ERR, "empty_output_buffer called, may not happen because buffer must me large enough\n");
	return(FALSE);
}

METHODDEF(void) term_destination(struct jpeg_compress_struct *cinfo) {
//	printf("term_destination called %p %d\n", ccbuf, ccount);
	ccount = MJPEG_SIZE - cinfo->dest->free_in_buffer;
}
/* end of minimal destination handler */

JSAMPARRAY ***jsi;

#else
#include "../libavcodec/avcodec.h"
AVCodec *codec;
AVCodecContext codec_context;
AVPicture picture;
#endif 

static int jpegdct = JDCT_IFAST; 

int init_codec() {
#ifdef ZR_USES_LIBJPEG
	int i, j, k;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	cinfo.dest = &jdest;
	cinfo.dest->init_destination = init_destination;
	cinfo.dest->empty_output_buffer = empty_output_buffer;
	cinfo.dest->term_destination = term_destination;

	cinfo.input_components = 3;

	jpeg_set_defaults(&cinfo);

	cinfo.image_width = image_width;
	cinfo.image_height = image_height/fields;
	cinfo.input_gamma = 1.0;
	cinfo.in_color_space = JCS_YCbCr;
	cinfo.raw_data_in = TRUE;
	cinfo.comp_info[0].h_samp_factor = 2;
	cinfo.comp_info[0].v_samp_factor = 1;
	cinfo.comp_info[1].h_samp_factor = 1;
	cinfo.comp_info[1].v_samp_factor = 1;
	cinfo.comp_info[2].h_samp_factor = 1;
	cinfo.comp_info[2].v_samp_factor = 1;
	cinfo.dct_method = jpegdct;
	jpeg_set_quality(&cinfo, quality, FALSE);
	jsi = malloc(sizeof(JSAMPARRAY**)*fields);

	/* Just some clutter to give libjpeg the pointers,
	 * and I don't want to recalculate everything everytime
	 * it is needed */
	for (k = 0; k < fields; k++) {
	jsi[k] = malloc(sizeof(JSAMPARRAY*)*image_height/(8*fields));

	for (i = 0; i < image_height/(8*fields); i++) {
		jsi[k][i] = malloc(3*sizeof(JSAMPARRAY));
		jsi[k][i][0] = malloc(8*sizeof(JSAMPROW));
		jsi[k][i][1] = malloc(8*sizeof(JSAMPROW));
		jsi[k][i][2] = malloc(8*sizeof(JSAMPROW));
		for (j = 0; j < 8; j++) {
			jsi[k][i][0][j] = (JSAMPROW)(image + 
					(fields*(8*i + j) + k)*image_width);
			jsi[k][i][1][j] = (JSAMPROW)(image + size + 
					(fields*(8*i + j)/2)*image_width/2);
			jsi[k][i][2][j] = (JSAMPROW)(image + 3*size/2 + 
					(fields*(8*i + j)/2)*image_width/2);
		}
	}

	}
#else
	AVCodecContext *c = &codec_context;
	codec = avcodec_find_encoder(CODEC_ID_MJPEG);
	if (!codec) {
		/* maybe libavcodec was not initialized */
		avcodec_init();
		avcodec_register_all();
		codec = avcodec_find_encoder(CODEC_ID_MJPEG);
		if (!codec) {
			mp_msg(MSGT_VO, MSGL_ERR, "MJPG codec not found in libavcodec\n");
			return 1;
		}
	}
	/* put default values */
	memset(c, 0, sizeof(*c));

	c->width = image_width;
	c->height = image_height;
	c->bit_rate = 4000000;
	c->frame_rate = 25*FRAME_RATE_BASE;
	//c->gop_size = 1;
	c->pix_fmt = PIX_FMT_YUV422P;

	if (avcodec_open(c, codec) < 0) {
		mp_msg(MSGT_VO, MSGL_ERR, "MJPG codec could not be opened\n");
		return 1;
	}

	picture.data[0] = image;
	picture.data[1] = image + size;
	picture.data[2] = image + 3*size/2;
	picture.linesize[0] = image_width;
	picture.linesize[1] = image_width/2;
	picture.linesize[2] = image_width/2;
#endif 
	return 0;
}


int zoran_getcap() {
	char* dev = device ? device : VO_ZR_DEFAULT_DEVICE;
	vdes = open(dev, O_RDWR);
	/* before we can ask for the maximum resolution, we must set 
	 * the correct tv norm */

	if (ioctl(vdes, BUZIOC_G_PARAMS, &zp) < 0) {
		mp_msg(MSGT_VO, MSGL_ERR, "device at %s is probably not a DC10(+)/buz/lml33\n", dev);
		return 1;
	}
	
	if (zp.norm != norm && norm != VIDEO_MODE_AUTO) {
		/* attempt to set requested norm */
		zp.norm = norm;
		if (ioctl(vdes, BUZIOC_S_PARAMS, &zp) < 0) {
			mp_msg(MSGT_VO, MSGL_ERR,
				"unable to change video norm, use another program to change it (XawTV)\n");
			return 1;
		}
		ioctl(vdes, BUZIOC_G_PARAMS, &zp);
		if (norm != zp.norm) {
			mp_msg(MSGT_VO, MSGL_ERR,
				"unable to change video norm, use another program to change it (XawTV)\n");
			return 1;
		}
	}
	
	if (vdes < 0) {
		mp_msg(MSGT_VO, MSGL_ERR, "error opening %s\n", 
				dev);
		return 1;
	}


	if (ioctl(vdes, VIDIOCGCAP, &vc) < 0) {
		mp_msg(MSGT_VO, MSGL_ERR, "error getting video capabilities from %s\n");
		return 1;
	}
	mp_msg(MSGT_VO, MSGL_V, "zr36067 reports: maxwidth=%d, maxheight=%d\n", vc.maxwidth, vc.maxheight);
	
	return 0;
}
	
int init_zoran() {
	/* center the image, and stretch it as far as possible (try to keep
	 * aspect) and check if it fits */
	if (image_width > vc.maxwidth) {
		mp_msg(MSGT_VO, MSGL_ERR, "movie to be played is too wide, max width currenty %d\n", vc.maxwidth);
		return 1;
	}

	if (image_height > vc.maxheight) {
		mp_msg(MSGT_VO, MSGL_ERR, "movie to be played is too high, max height currenty %d\n", vc.maxheight);
		return 1;
	}

	zp.decimation = 0;
	zp.HorDcm = (vc.maxwidth >= 2*(int)image_width) ? 2 : 1;
	zp.VerDcm = 1;
	if (zp.HorDcm == 2 && 4*image_width <= vc.maxwidth && 
			4*image_height/fields <= vc.maxheight) {
		zp.HorDcm = 4;
		zp.VerDcm = 2;
	}
	if (((forceinter == 0 && vdec >= 2) || (forceinter == 1 && vdec == 4)) && 4*image_height/fields <= vc.maxheight) {
		zp.VerDcm = 2;
	}
	zp.TmpDcm = 1;
	zp.field_per_buff = fields;
	zp.img_x = (vc.maxwidth - zp.HorDcm*(int)image_width)/2;
	zp.img_y = (vc.maxheight - zp.VerDcm*(3-fields)*(int)image_height)/4;
	zp.img_width = zp.HorDcm*image_width;
	zp.img_height = zp.VerDcm*image_height/fields;
	mp_msg(MSGT_VO, MSGL_V, "zr: geometry (after 'scaling'): %dx%d+%d+%d fields=%d, w=%d, h=%d\n", zp.img_width, zp.img_height, zp.img_x, zp.img_y, fields, image_width, image_height);

	if (ioctl(vdes, BUZIOC_S_PARAMS, &zp) < 0) {
		mp_msg(MSGT_VO, MSGL_ERR, "error setting display parameters\n");
		return 1;
	}

	zrq.count = MJPEG_NBUFFERS;
	zrq.size = MJPEG_SIZE;

	if (ioctl(vdes, BUZIOC_REQBUFS, &zrq)) {
		mp_msg(MSGT_VO, MSGL_ERR, "error requesting %d buffers of size %d\n", zrq.count, zrq.size);
		return 1;
	}

	buf = (char*)mmap(0, zrq.count*zrq.size, PROT_READ|PROT_WRITE,
			MAP_SHARED, vdes, 0);

	if (buf == MAP_FAILED) {
		mp_msg(MSGT_VO, MSGL_ERR, "error requesting %d buffers of size %d\n", zrq.count, zrq.size);
		return 1;
	}
	return 0;
}

void uninit_zoran(void) {
	if (image) {
		free(image);
		image=NULL;
	}
	while (queue > synco + 1) {
		if (ioctl(vdes, BUZIOC_SYNC, &zs) < 0) 
			mp_msg(MSGT_VO, MSGL_ERR, "error waiting for buffers to become free"); 
		synco++;
	}
	/* stop streaming */
	frame = -1;
	if (ioctl(vdes, BUZIOC_QBUF_PLAY, &frame) < 0) 
		mp_msg(MSGT_VO, MSGL_ERR, "error stopping playback of last frame");
	close(vdes);
}

static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width, 
	uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
	int j;
	/* this allows to crop parts from incoming picture,
	 * for easy 512x240 -> 352x240 */
	/* These values must be multples of 2 */

	if (g.set) {
		if (g.width%2 != 0 || g.height%2 != 0 ||
				g.xoff%2 != 0 || g.yoff%2 != 0) {
			mp_msg(MSGT_VO, MSGL_ERR, "arguments in -zrcrop must be multiples of 2\n");
			return 1;
		}
		if (g.width <= 0 || g.height <= 0 ||
				g.xoff < 0 || g.yoff < 0) {
			mp_msg(MSGT_VO, MSGL_ERR, "width and height must be positive and offset nonnegative\n");
			return 1;
		}
		if (g.width + g.xoff > width) {
			mp_msg(MSGT_VO, MSGL_ERR, "width+xoffset (%d+%d>%d) is too big\n", g.width, g.xoff, width);
			return 1;
		}
		if (g.height + g.yoff > height) {
			mp_msg(MSGT_VO, MSGL_ERR, "height+yoffset (%d+%d>%d) is too big\n", g.height, g.yoff, height);
			return 1;
		}
	} else {
		g.width = width;
		g.height = height;
		g.xoff = 0;
		g.yoff = 0;
	}
	/* we must know the maximum resolution of the device
	 * it differs for DC10+ and buz for example */
	zoran_getcap(); /*must be called before init_zoran */
	if (g.height/vdec > vc.maxheight/2 || (forceinter == 1 && vdec == 1))
		fields = 2;
	printf("fields=%d\n", fields);
	/* the height must be a multiple of fields*8 and the width
	 * must be a multiple of 16 */
	/* add some black borders to make it so, and center the image*/
	image_height = fields*8*((g.height/vdec - 1)/(fields*8) + 1);
	image_width = 16*((g.width - 1)/16 + 1);
	off_y = (image_height - g.height/vdec)/2;
	if (off_y%2 != 0) off_y++;
	off_y *= image_width;
	off_c = off_y/4;
	off_y += (image_width - g.width)/2;
	if (off_y%2 != 0) off_y--;
	off_c += (image_width - g.width)/4;
	framenum = 0;
	size = image_width*image_height;
	mp_msg(MSGT_VO, MSGL_V, "input: %dx%d, cropped: %dx%d, output: %dx%d, off_y=%d, off_c=%d\n", width, height, g.width, g.height, image_width, image_height, off_y, off_c);
	
	image = malloc(2*size); /* this buffer allows for YUV422 data,
				 * so it is a bit too big for YUV420 */
	if (!image) {
		mp_msg(MSGT_VO, MSGL_ERR, "Memory exhausted\n");
		return 1;
	}
	/* and make sure that the borders are _really_ black */
	memset(image, 0, image_width*image_height);
	memset(image + size, 0x80, image_width*image_height/4);
	memset(image + 3*size/2, 0x80, image_width*image_height/4);

	if (init_codec()) {
		return 1;
	}
	
	if (init_zoran()) {
#ifdef ZR_USES_LIBJPEG
		jpeg_destroy_compress(&cinfo);
#else
		avcodec_close(&codec_context);
#endif
		return 1;
	}

	return 0;
}

static const vo_info_t* get_info(void) {
	return &vo_info;
}

static void draw_osd(void) {
}

static void flip_page (void) {
#ifdef ZR_USES_LIBJPEG
	int i, j, k;
#else
	AVCodecContext *c = &codec_context;
#endif

	/* do we have a free buffer? */
	if (queue-synco < zrq.count) {
		frame = queue;
	} else {
		if (ioctl(vdes, BUZIOC_SYNC, &zs) < 0) 
			mp_msg(MSGT_VO, MSGL_ERR, "error waiting for buffers to become free"); 
		frame = zs.frame;
		synco++;
	}

#ifdef ZR_USES_LIBJPEG
	ccbuf = buf + frame*zrq.size;
	ccount = 0;
	k = fields;
	for (j=0; j < k; j++) {

	jpeg_start_compress(&cinfo, TRUE);
	i=0;
	while (cinfo.next_scanline < cinfo.image_height) {
		jpeg_write_raw_data(&cinfo, jsi[j][i], 8);
		i++;
	}
	jpeg_finish_compress(&cinfo);

	}
#else
	avcodec_encode_video(c, buf + frame*zrq.size, MJPEG_SIZE, &picture);
#endif

	if (ioctl(vdes, BUZIOC_QBUF_PLAY, &frame) < 0) 
		mp_msg(MSGT_VO, MSGL_ERR,
				"error queueing buffer for playback");
	queue++;

	framenum++;
	return;
}

static uint32_t draw_frame(uint8_t * src[]) {
	return 0;
}

static uint32_t query_format(uint32_t format) {
	if(format==IMGFMT_YV12) return 1;
	return 0;
}

static void uninit(void) {
	uninit_zoran();

#ifdef ZR_USES_LIBJPEG
	jpeg_destroy_compress(&cinfo);
#else
	avcodec_close(&codec_context);
#endif
}

static void check_events(void) {
}


static uint32_t draw_slice(uint8_t *srcimg[], int stride[],
		int w, int h, int x, int y) {
	int i;
	/* Apply 'geometry', crop unwanted parts */
	uint8_t *dst;
	uint8_t *src;
	//printf("before: w=%d, h=%d, x=%d, y=%d, src0=%p, src1=%p, src2=%p\n", w, h, x, y, srcimg[0], srcimg[1], srcimg[2]);
	if (x < g.xoff) {
		srcimg[0] += g.xoff - x;
		srcimg[1] += (g.xoff - x)/2;
		srcimg[2] += (g.xoff - x)/2;
		w -= g.xoff - x;
		if (w < 0) return 0;
		x = 0 /*g.xoff*/;
	} else {
		x -= g.xoff;
	}
	if (x + w > g.width) {
		w = g.width - x;
		if (w < 0) return 0;
	}
	if (y < g.yoff) {
		srcimg[0] += (g.yoff - y)*stride[0];
		srcimg[1] += ((g.yoff - y)/2)*stride[1];
		srcimg[2] += ((g.yoff - y)/2)*stride[2];
		h -= g.yoff - y;
		if (h < 0) return 0;
		y = 0;
	} else {
		y -= g.yoff;
	}
	if (y + h > g.height) {
		h = g.height - y;
		if (h < 0) return 0;
	}
	//printf("after: w=%d, h=%d, x=%d, y=%d, src0=%p, src1=%p, src2=%p\n", w, h, x, y, srcimg[0], srcimg[1], srcimg[2]);
	dst=image + off_y + image_width*(y/vdec)+x;
	src=srcimg[0];
	// copy Y:
	for (i = 0; i < h; i++) {
		if ((i + x)%vdec == 0) {
			memcpy(dst,src,w);
			dst+=image_width;
		}
		src+=stride[0];

	}
	{
    		// copy U+V:
		uint8_t *src1=srcimg[1];
		uint8_t *src2=srcimg[2];
		uint8_t *dst1=image + size + off_c+ (y/(vdec*2))*image_width/2+(x/2);
		uint8_t *dst2=image + 3*size/2 + off_c + 
			(y/(vdec*2))*image_width/2+(x/2);
		for (i = 0; i< h/2; i++) {
			if ((i+x/2)%vdec == 0) {
				memcpy(dst1,src1,w/2);
				memcpy(dst2,src2,w/2);
				dst1+=image_width/2;
				dst2+=image_width/2;
			}
			src1+=stride[1];
			src2+=stride[2];
		}
    	}
 	return 0;
}


/* copied and adapted from vo_aa_parseoption */
int
vo_zr_parseoption(struct config * conf, char *opt, char *param){
    /* got an option starting with zr */
    char *x, *help;
    int i;
    /* do WE need it ?, always */
    if (!strcasecmp(opt, "zrdev")) {
	if (param == NULL) return ERR_MISSING_PARAM;
	//if ((i=getcolor(param))==-1) return ERR_OUT_OF_RANGE;
	//aaopt_osdcolor=i;
	device = malloc(strlen(param)+1);
	strcpy(device, param);
	mp_msg(MSGT_VO, MSGL_V, "zr: using device %s\n", device);
	return 1;
    } else if (!strcasecmp(opt, "zrfi")) {
	    if (param != NULL) {
		    return ERR_OUT_OF_RANGE;
	    }
	    forceinter = 1;
	    return 1;
    } else if (!strcasecmp(opt, "zrcrop")){
	if (param == NULL) return ERR_MISSING_PARAM;
	if (sscanf(param, "%dx%d+%d+%d", &g.width, &g.height, 
				&g.xoff, &g.yoff) != 4) {
		g.xoff = 0; g.yoff = 0;
		if (sscanf(param, "%dx%d", &g.width, &g.height) != 2) {
			mp_msg(MSGT_VO, MSGL_ERR, "argument to -zrcrop must be of the form 352x288+16+0\n");
			return ERR_OUT_OF_RANGE;
		}
	}
	g.set = 1;
	mp_msg(MSGT_VO, MSGL_V, "zr: cropping %s\n", param);
	return 1;
    }else if (!strcasecmp(opt, "zrvdec")) {
        i = atoi(param);
	if (i != 1 && i != 2 && i != 4) return ERR_OUT_OF_RANGE;
	vdec = i;
	return 1;
    }else if (!strcasecmp(opt, "zrquality")) {
        i = atoi(param);
	if (i < 30 || i > 100) return ERR_OUT_OF_RANGE;
	quality = i;
	return 1;
    }else if (!strcasecmp(opt, "zrdct")) {
	if (param == NULL) return ERR_MISSING_PARAM;
	if (!strcasecmp(param, "IFAST")) {
            jpegdct = JDCT_IFAST;
	    return 1;
	} else if (!strcasecmp(param, "ISLOW")) {
            jpegdct = JDCT_ISLOW;
	    return 1;
	} else if (!strcasecmp(param, "FLOAT")) {
            jpegdct = JDCT_FLOAT;
	    return 1;
	} else {
           return ERR_OUT_OF_RANGE;
        }
    }else if (!strcasecmp(opt, "zrnorm")) {
	if (param == NULL) return ERR_MISSING_PARAM;
	if (!strcasecmp(param, "NTSC")) {
            mp_msg(MSGT_VO, MSGL_V, "zr: Norm set to NTSC\n");
            norm = VIDEO_MODE_NTSC;
	    return 1;
	} else if (!strcasecmp(param, "PAL")) {
	    mp_msg(MSGT_VO, MSGL_V, "zr: Norm set to PAL\n");
            norm = VIDEO_MODE_PAL;
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
		    "  -zrfi       force interlacing ('wide screen')\n"
		    "              (by default we only interlace if the movie\n"
		    "              is higher than half of the screen height)\n"
		    "  -zrquality  jpeg compression quality 30-100\n"
                    "  -zrdct      specify DCT method: IFAST, ISLOW or FLOAT\n"
		    "  -zrdev      playback device (example -zrdev /dev/video1\n"
		    "  -zrnorm     specify norm PAL/NTSC [dev: leave at current setting]\n"
		    "\n"
	      );
	exit(0);
		
    }
    return ERR_NOT_AN_OPTION;
}

void vo_zr_revertoption(config_t* opt,char* param) {

  if (!strcasecmp(param, "zrdev")) {
    if(device)
      free(device);
    device=NULL;
  } else if (!strcasecmp(param, "zrfi"))
    forceinter=0;
  else if (!strcasecmp(param, "zrcrop"))
    g.set = g.xoff = g.yoff = 0;
  else if (!strcasecmp(param, "zrvdec"))
    vdec = 1;
  else if (!strcasecmp(param, "zrquality"))
    quality = 70;
  else if (!strcasecmp(param, "zrdct"))
    jpegdct = JDCT_IFAST;
  else if (!strcasecmp(param, "zrnorm"))
    norm = VIDEO_MODE_AUTO;

}
