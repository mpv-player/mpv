/* 
    zoran - Iomega Buz driver

    Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>

   based on

    zoran.0.0.3 Copyright (C) 1998 Dave Perks <dperks@ibm.net>

   and

    bttv - Bt848 frame grabber driver
    Copyright (C) 1996,97 Ralph Metzler (rjkm@thp.uni-koeln.de)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _BUZ_H_
#define _BUZ_H_

#include <linux/config.h>

#if LINUX_VERSION_CODE < 0x20212
typedef struct wait_queue *wait_queue_head_t;
#endif

/* The Buz only supports a maximum width of 720, but some V4L
   applications (e.g. xawtv are more happy with 768).
   If XAWTV_HACK is defined, we try to fake a device with bigger width */

//#define XAWTV_HACK

//#ifdef XAWTV_HACK
//#define   BUZ_MAX_WIDTH   768   /* never display more than 768 pixels */
#define   BUZ_MAX_WIDTH   (zr->timing->Wa)
//#else
//#define   BUZ_MAX_WIDTH   720   /* never display more than 720 pixels */
//#endif
//#define   BUZ_MAX_HEIGHT  576   /* never display more than 576 rows */
#define   BUZ_MAX_HEIGHT  (zr->timing->Ha)
#define   BUZ_MIN_WIDTH    32	/* never display less than 32 pixels */
#define   BUZ_MIN_HEIGHT   24	/* never display less than 24 rows */

struct zoran_requestbuffers {
	unsigned long count;	/* Number of buffers for MJPEG grabbing */
	unsigned long size;	/* Size PER BUFFER in bytes */
};

struct zoran_sync {
	unsigned long frame;	/* number of buffer that has been free'd */
	unsigned long length;	/* number of code bytes in buffer (capture only) */
	unsigned long seq;	/* frame sequence number */
	struct timeval timestamp;	/* timestamp */
};

struct zoran_status {
	int input;		/* Input channel, has to be set prior to BUZIOC_G_STATUS */
	int signal;		/* Returned: 1 if valid video signal detected */
	int norm;		/* Returned: VIDEO_MODE_PAL or VIDEO_MODE_NTSC */
	int color;		/* Returned: 1 if color signal detected */
};

struct zoran_params {

	/* The following parameters can only be queried */

	int major_version;	/* Major version number of driver */
	int minor_version;	/* Minor version number of driver */

	/* Main control parameters */

	int input;		/* Input channel: 0 = Composite, 1 = S-VHS */
	int norm;		/* Norm: VIDEO_MODE_PAL or VIDEO_MODE_NTSC */
	int decimation;		/* decimation of captured video,
				   enlargement of video played back.
				   Valid values are 1, 2, 4 or 0.
				   0 is a special value where the user
				   has full control over video scaling */

	/* The following parameters only have to be set if decimation==0,
	   for other values of decimation they provide the data how the image is captured */

	int HorDcm;		/* Horizontal decimation: 1, 2 or 4 */
	int VerDcm;		/* Vertical decimation: 1 or 2 */
	int TmpDcm;		/* Temporal decimation: 1 or 2,
				   if TmpDcm==2 in capture every second frame is dropped,
				   in playback every frame is played twice */
	int field_per_buff;	/* Number of fields per buffer: 1 or 2 */
	int img_x;		/* start of image in x direction */
	int img_y;		/* start of image in y direction */
	int img_width;		/* image width BEFORE decimation,
				   must be a multiple of HorDcm*16 */
	int img_height;		/* image height BEFORE decimation,
				   must be a multiple of VerDcm*8 */

	/* --- End of parameters for decimation==0 only --- */

	/* JPEG control parameters */

	int quality;		/* Measure for quality of compressed images.
				   Scales linearly with the size of the compressed images.
				   Must be beetween 0 and 100, 100 is a compression
				   ratio of 1:4 */

	int odd_even;		/* Which field should come first ??? */

	int APPn;		/* Number of APP segment to be written, must be 0..15 */
	int APP_len;		/* Length of data in JPEG APPn segment */
	char APP_data[60];	/* Data in the JPEG APPn segment. */

	int COM_len;		/* Length of data in JPEG COM segment */
	char COM_data[60];	/* Data in JPEG COM segment */

	unsigned long jpeg_markers;	/* Which markers should go into the JPEG output.
					   Unless you exactly know what you do, leave them untouched.
					   Inluding less markers will make the resulting code
					   smaller, but there will be fewer aplications
					   which can read it.
					   The presence of the APP and COM marker is
					   influenced by APP0_len and COM_len ONLY! */
#define JPEG_MARKER_DHT (1<<3)	/* Define Huffman Tables */
#define JPEG_MARKER_DQT (1<<4)	/* Define Quantization Tables */
#define JPEG_MARKER_DRI (1<<5)	/* Define Restart Interval */
#define JPEG_MARKER_COM (1<<6)	/* Comment segment */
#define JPEG_MARKER_APP (1<<7)	/* App segment, driver will allways use APP0 */

	int VFIFO_FB;		/* Flag for enabling Video Fifo Feedback.
				   If this flag is turned on and JPEG decompressing
				   is going to the screen, the decompress process
				   is stopped every time the Video Fifo is full.
				   This enables a smooth decompress to the screen
				   but the video output signal will get scrambled */

	/* Misc */

	char reserved[312];	/* Makes 512 bytes for this structure */
};

/*
Private IOCTL to set up for displaying MJPEG
*/
#define BUZIOC_G_PARAMS       _IOR ('v', BASE_VIDIOCPRIVATE+0,  struct zoran_params)
#define BUZIOC_S_PARAMS       _IOWR('v', BASE_VIDIOCPRIVATE+1,  struct zoran_params)
#define BUZIOC_REQBUFS        _IOWR('v', BASE_VIDIOCPRIVATE+2,  struct zoran_requestbuffers)
#define BUZIOC_QBUF_CAPT      _IOW ('v', BASE_VIDIOCPRIVATE+3,  int)
#define BUZIOC_QBUF_PLAY      _IOW ('v', BASE_VIDIOCPRIVATE+4,  int)
#define BUZIOC_SYNC           _IOR ('v', BASE_VIDIOCPRIVATE+5,  struct zoran_sync)
#define BUZIOC_G_STATUS       _IOWR('v', BASE_VIDIOCPRIVATE+6,  struct zoran_status)


#ifdef __KERNEL__

#define BUZ_NUM_STAT_COM    4
#define BUZ_MASK_STAT_COM   3

#define BUZ_MAX_FRAME     256	/* Must be a power of 2 */
#define BUZ_MASK_FRAME    255	/* Must be BUZ_MAX_FRAME-1 */

#if VIDEO_MAX_FRAME <= 32
#   define   V4L_MAX_FRAME   32
#elif VIDEO_MAX_FRAME <= 64
#   define   V4L_MAX_FRAME   64
#else
#   error   "Too many video frame buffers to handle"
#endif
#define   V4L_MASK_FRAME   (V4L_MAX_FRAME - 1)


#include "zr36057.h"

enum card_type {
        UNKNOWN = 0,
        DC10,
        DC10plus,
        LML33,
        BUZ
};

enum zoran_codec_mode {
	BUZ_MODE_IDLE,		/* nothing going on */
	BUZ_MODE_MOTION_COMPRESS,	/* grabbing frames */
	BUZ_MODE_MOTION_DECOMPRESS,	/* playing frames */
	BUZ_MODE_STILL_COMPRESS,	/* still frame conversion */
	BUZ_MODE_STILL_DECOMPRESS	/* still frame conversion */
};

enum zoran_buffer_state {
	BUZ_STATE_USER,		/* buffer is owned by application */
	BUZ_STATE_PEND,		/* buffer is queued in pend[] ready to feed to I/O */
	BUZ_STATE_DMA,		/* buffer is queued in dma[] for I/O */
	BUZ_STATE_DONE		/* buffer is ready to return to application */
};

struct zoran_gbuffer {
	u32 *frag_tab;		/* addresses of frag table */
	u32 frag_tab_bus;	/* same value cached to save time in ISR */
	enum zoran_buffer_state state;	/* non-zero if corresponding buffer is in use in grab queue */
	struct zoran_sync bs;	/* DONE: info to return to application */
};

struct v4l_gbuffer {
	char *fbuffer;			/* virtual  address of frame buffer */
	unsigned long fbuffer_phys;	/* physical address of frame buffer */
	unsigned long fbuffer_bus;	/* bus      address of frame buffer */
	enum zoran_buffer_state state;	/* state: unused/pending/done */
};

struct tvnorm {
	u16 Wt, Wa, HStart, HSyncStart, Ht, Ha, VStart;
};

struct zoran {
	struct video_device video_dev;
	struct i2c_bus i2c;

	int initialized;		/* flag if zoran has been correctly initalized */
	int user;			/* number of current users (0 or 1) */
        enum card_type card;
        struct tvnorm *timing;

	unsigned short id;		/* number of this device */
	char name[32];			/* name of this device */
	struct pci_dev *pci_dev;	/* PCI device */
	unsigned char revision;		/* revision of zr36057 */
	unsigned int zr36057_adr;	/* bus address of IO mem returned by PCI BIOS */
	unsigned char *zr36057_mem;	/* pointer to mapped IO memory */

	int map_mjpeg_buffers;		/* Flag which bufferset will map by next mmap() */

	spinlock_t lock;		/* Spinlock irq and hardware */
	struct semaphore sem;		/* Guard parallel ioctls and mmap */

	/* Video for Linux parameters */

	struct video_picture picture;	/* Current picture params */
	struct video_buffer buffer;	/* Current buffer params */
	struct video_window window;	/* Current window params */
	int buffer_set, window_set;	/* Flags if the above structures are set */
	int video_interlace;		/* Image on screen is interlaced */

	u32 *overlay_mask;
        wait_queue_head_t v4l_capq;

	int v4l_overlay_active;		/* Overlay grab is activated */
	int v4l_memgrab_active;		/* Memory grab is activated */

	int v4l_grab_frame;		/* Frame number being currently grabbed */
#define NO_GRAB_ACTIVE (-1)
	int v4l_grab_seq;		/* Number of frames grabbed */
	int gwidth;			/* Width of current memory capture */
	int gheight;			/* Height of current memory capture */
	int gformat;			/* Format of ... */
	int gbpl;			/* byte per line of ... */

	/* V4L grab queue of frames pending */

	unsigned v4l_pend_head;
	unsigned v4l_pend_tail;
	int v4l_pend[V4L_MAX_FRAME];

	struct v4l_gbuffer v4l_gbuf[VIDEO_MAX_FRAME];	/* V4L   buffers' info */

	/* Buz MJPEG parameters */

	unsigned long jpg_nbufs;	/* Number of buffers */
	unsigned long jpg_bufsize;	/* Size of mjpeg buffers in bytes */
	int jpg_buffers_allocated;	/* Flag if buffers are allocated  */
	int need_contiguous;	/* Flag if contiguous buffers are needed */

	enum zoran_codec_mode codec_mode;	/* status of codec */
	struct zoran_params params;	/* structure with a lot of things to play with */

	wait_queue_head_t jpg_capq;	/* wait here for grab to finish */

	/* grab queue counts/indices, mask with BUZ_MASK_STAT_COM before using as index */
	/* (dma_head - dma_tail) is number active in DMA, must be <= BUZ_NUM_STAT_COM */
	/* (value & BUZ_MASK_STAT_COM) corresponds to index in stat_com table */
	unsigned long jpg_que_head;	/* Index where to put next buffer which is queued */
	unsigned long jpg_dma_head;	/* Index of next buffer which goes into stat_com  */
	unsigned long jpg_dma_tail;	/* Index of last buffer in stat_com               */
	unsigned long jpg_que_tail;	/* Index of last buffer in queue                  */
	unsigned long jpg_seq_num;	/* count of frames since grab/play started        */
	unsigned long jpg_err_seq;	/* last seq_num before error                      */
	unsigned long jpg_err_shift;
	unsigned long jpg_queued_num;	/* count of frames queued since grab/play started */

	/* zr36057's code buffer table */
	u32 *stat_com;			/* stat_com[i] is indexed by dma_head/tail & BUZ_MASK_STAT_COM */

	/* (value & BUZ_MASK_FRAME) corresponds to index in pend[] queue */
	int jpg_pend[BUZ_MAX_FRAME];

	/* array indexed by frame number */
	struct zoran_gbuffer jpg_gbuf[BUZ_MAX_FRAME];	/* MJPEG buffers' info */

	/* Additional stuff for testing */
	struct proc_dir_entry *zoran_proc;

	int testing;
	int jpeg_error;
	int intr_counter_GIRQ1;
	int intr_counter_GIRQ0;
	int intr_counter_CodRepIRQ;
	int intr_counter_JPEGRepIRQ;
	int field_counter;
	int IRQ1_in;
	int IRQ1_out;
	int JPEG_in;
	int JPEG_out;
	int JPEG_0;
	int JPEG_1;
	int END_event_missed;
	int JPEG_missed;
	int JPEG_error;
	int num_errors;
	int JPEG_max_missed;
	int JPEG_min_missed;

	u32 last_isr;
	unsigned long frame_num;

	wait_queue_head_t test_q;
};

#endif

/*The following should be done in more portable way. It depends on define
  of _ALPHA_BUZ in the Makefile.*/

#ifdef _ALPHA_BUZ
#define btwrite(dat,adr)    writel((dat),(char *) (zr->zr36057_adr+(adr)))
#define btread(adr)         readl(zr->zr36057_adr+(adr))
#else
#define btwrite(dat,adr)    writel((dat), (char *) (zr->zr36057_mem+(adr)))
#define btread(adr)         readl(zr->zr36057_mem+(adr))
#endif

#define btand(dat,adr)      btwrite((dat) & btread(adr), adr)
#define btor(dat,adr)       btwrite((dat) | btread(adr), adr)
#define btaor(dat,mask,adr) btwrite((dat) | ((mask) & btread(adr)), adr)

#define I2C_TSA5522        0xc2
#define I2C_TDA9850        0xb6
#define I2C_HAUPEE         0xa0
#define I2C_STBEE          0xae
#define   I2C_SAA7111        0x48
#define   I2C_SAA7110        0x9c
#define   I2C_SAA7185        0x88
//#define   I2C_ADV7175        0xd4
#define   I2C_ADV7175        0x54

#define TDA9850_CON1       0x04
#define TDA9850_CON2       0x05
#define TDA9850_CON3       0x06
#define TDA9850_CON4       0x07
#define TDA9850_ALI1       0x08
#define TDA9850_ALI2       0x09
#define TDA9850_ALI3       0x0a

#endif
