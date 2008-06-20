/*
 * VIDIX - VIDeo Interface for *niX.
 *
 * This interface is introduced as universal one to MPEG decoder,
 * Back End Scaler (BES) and YUV2RGB hw accelerators.
 *
 * In the future it may be expanded up to capturing and audio things.
 * Main goal of this this interface imlpementation is providing DGA
 * everywhere where it's possible (unlike X11 and other).
 *
 * This interface is based on v4l2, fbvid.h, mga_vid.h projects
 * and my personal ideas.
 *
 * NOTE: This interface is introduced as driver interface.
 *
 * Copyright (C) 2002 Nick Kurshev
 * Copyright (C) 2007 Benjamin Zores <ben@geexbox.org>
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

#ifndef MPLAYER_VIDIX_H
#define MPLAYER_VIDIX_H

#define PROBE_NORMAL    0 /* normal probing */
#define PROBE_FORCE     1 /* ignore device_id but recognize device if it's known */
  
typedef enum vidix_dev_type {
  TYPE_OUTPUT  =        0x00000000,	/* Is a video playback device */
  TYPE_CAPTURE =	0x00000001,	/* Is a capture device */
  TYPE_CODEC   =        0x00000002,	/* Device supports hw (de)coding */
  TYPE_FX      =	0x00000004,	/* Is a video effects device */
} vidix_dev_type_t;

typedef enum vidix_dev_flag {
  FLAG_NONE       =	0x00000000, /* No flags defined */
  FLAG_DMA        =	0x00000001, /* Card can use DMA */
  /* Card can use DMA only if src pitch == dest pitch */
  FLAG_EQ_DMA     =	0x00000002,
  /* Possible to wait for DMA to finish. See BM_DMA_SYNC and BM_DMA_BLOCK */
  FLAG_SYNC_DMA   =     0x00000004,
  FLAG_UPSCALER   =	0x00000010, /* Card supports hw upscaling */
  FLAG_DOWNSCALER =	0x00000020, /* Card supports hw downscaling */
  FLAG_SUBPIC	  =	0x00001000, /* Card supports DVD subpictures */
  FLAG_EQUALIZER  =	0x00002000, /* Card supports equalizer */
} vidix_dev_flag_t;
  
typedef struct vidix_capability_s
{
  char name[64]; /* Driver name */
  char author[64]; /* Author name */
  vidix_dev_type_t type;
  unsigned reserved0[4];
  int maxwidth;
  int maxheight;
  int minwidth;
  int minheight;
  int maxframerate; /* -1 if unlimited */
  vidix_dev_flag_t flags;
  unsigned short vendor_id;
  unsigned short device_id;
  unsigned reserved1[4];
} vidix_capability_t;

typedef enum vidix_depth {
  VID_DEPTH_NONE  =		0x0000,
  VID_DEPTH_1BPP  =		0x0001,
  VID_DEPTH_2BPP  =		0x0002,
  VID_DEPTH_4BPP  =		0x0004,
  VID_DEPTH_8BPP  =		0x0008,
  VID_DEPTH_12BPP =		0x0010,
  VID_DEPTH_15BPP =		0x0020,
  VID_DEPTH_16BPP =		0x0040,
  VID_DEPTH_24BPP =		0x0080,
  VID_DEPTH_32BPP =		0x0100,
  VID_DEPTH_ALL   =             VID_DEPTH_1BPP  | VID_DEPTH_2BPP  | \
                                VID_DEPTH_4BPP  | VID_DEPTH_8BPP  | \
		                VID_DEPTH_12BPP | VID_DEPTH_15BPP | \
		                VID_DEPTH_16BPP | VID_DEPTH_24BPP | \
                                VID_DEPTH_32BPP,
} vidix_depth_t;

typedef enum vidix_cap {
  VID_CAP_NONE               =	0x0000,
  /* if overlay can be bigger than source */
  VID_CAP_EXPAND             =	0x0001,
  /* if overlay can be smaller than source */
  VID_CAP_SHRINK             =	0x0002,
  /* if overlay can be blended with framebuffer */
  VID_CAP_BLEND              =	0x0004,
  /* if overlay can be restricted to a colorkey */
  VID_CAP_COLORKEY           =	0x0008,
  /* if overlay can be restricted to an alpha channel */
  VID_CAP_ALPHAKEY           =	0x0010,
  /* if the colorkey can be a range */
  VID_CAP_COLORKEY_ISRANGE   =	0x0020,
  /* if the alphakey can be a range */
  VID_CAP_ALPHAKEY_ISRANGE   =	0x0040,
  /* colorkey is checked against framebuffer */
  VID_CAP_COLORKEY_ISMAIN    =	0x0080,
  /* colorkey is checked against overlay */
  VID_CAP_COLORKEY_ISOVERLAY =	0x0100,
  /* alphakey is checked against framebuffer */
  VID_CAP_ALPHAKEY_ISMAIN    =	0x0200,
  /* alphakey is checked against overlay */
  VID_CAP_ALPHAKEY_ISOVERLAY =  0x0400,
} vidix_cap_t;

typedef struct vidix_fourcc_s
{
  unsigned fourcc;      /* input: requested fourcc */
  vidix_depth_t depth;	/* output: screen depth for given fourcc */
  vidix_cap_t flags;	/* output: capability */
} vidix_fourcc_t;

typedef struct vidix_yuv_s
{
  unsigned y,u,v;
} vidix_yuv_t;

typedef struct vidix_rect_s
{
  unsigned x,y,w,h;	/* in pixels */
  vidix_yuv_t pitch;	/* line-align in bytes */
} vidix_rect_t;

typedef enum vidix_color_key_op {
  CKEY_FALSE =	0,
  CKEY_TRUE  =	1,
  CKEY_EQ    =	2,
  CKEY_NEQ   =	3,
  CKEY_ALPHA =	4,
} vidix_color_key_op_t;

typedef struct vidix_color_key_s
{
  vidix_color_key_op_t op;	/* defines logical operation */
  unsigned char	red;
  unsigned char	green;
  unsigned char	blue;
  unsigned char	reserved;
}vidix_ckey_t;

typedef enum vidix_video_key_op {
  VKEY_FALSE =	0,
  VKEY_TRUE  =	1,
  VKEY_EQ    =	2,
  VKEY_NEQ   =	3,
} vidix_video_key_op_t;

typedef struct vidix_video_key_s {
  vidix_video_key_op_t op;	/* defines logical operation */
  unsigned char	key[8];
} vidix_vkey_t;

typedef enum vidix_interleave {
  VID_PLAY_INTERLEAVED_UV =             0x00000001,
  /* UVUVUVUVUV used by Matrox G200 */
  INTERLEAVING_UV         =             0x00001000,
  /* VUVUVUVUVU */
  INTERLEAVING_VU         =		0x00001001,
} vidix_interleave_t;

#define VID_PLAY_MAXFRAMES 64	/* unreal limitation */

typedef struct vidix_playback_s
{
  unsigned fourcc;		/* app -> driver: movies's fourcc */
  unsigned capability;	        /* app -> driver: what capability to use */
  unsigned blend_factor;	/* app -> driver: blending factor */
  vidix_rect_t src;             /* app -> driver: original movie size */
  vidix_rect_t dest;            /* app -> driver: destinition movie size.
                                   driver->app dest_pitch */
  vidix_interleave_t flags;     /* driver -> app: interleaved UV planes */
  /* memory model */
  unsigned frame_size;		/* driver -> app: destinition frame size */
  unsigned num_frames;		/* app -> driver: after call: driver -> app */
  unsigned offsets[VID_PLAY_MAXFRAMES];	/* driver -> app */
  vidix_yuv_t offset;		/* driver -> app: relative offsets
                                   within frame for yuv planes */
  void *dga_addr;		/* driver -> app: linear address */
} vidix_playback_t;

typedef enum vidix_key_op {
  KEYS_PUT =	0,
  KEYS_AND =	1,
  KEYS_OR  =	2,
  KEYS_XOR =	3,
} vidix_key_op_t;

typedef struct vidix_grkey_s
{
  vidix_ckey_t ckey;		/* app -> driver: color key */
  vidix_vkey_t vkey;		/* app -> driver: video key */
  vidix_key_op_t key_op;	/* app -> driver: keys operations */
} vidix_grkey_t;

typedef enum vidix_veq_cap {
  VEQ_CAP_NONE		=	0x00000000UL,
  VEQ_CAP_BRIGHTNESS	=	0x00000001UL,
  VEQ_CAP_CONTRAST	=	0x00000002UL,
  VEQ_CAP_SATURATION	=	0x00000004UL,
  VEQ_CAP_HUE		=	0x00000008UL,
  VEQ_CAP_RGB_INTENSITY =	0x00000010UL,
} vidix_veq_cap_t;

typedef enum vidix_veq_flag {
  VEQ_FLG_ITU_R_BT_601 = 0x00000000, /* ITU-R BT.601 colour space (default) */
  VEQ_FLG_ITU_R_BT_709 = 0x00000001, /* ITU-R BT.709 colour space */
  VEQ_FLG_ITU_MASK     = 0x0000000f,
} vidix_veq_flag_t;

typedef struct vidix_video_eq_s {
  vidix_veq_cap_t cap;	 /* on get_eq should contain capability of
                            equalizer on set_eq should contain using fields */
  /* end-user app can have presets like: cold-normal-hot picture and so on */
  int brightness;	        /* -1000 : +1000 */
  int contrast;	                /* -1000 : +1000 */
  int saturation;	        /* -1000 : +1000 */
  int hue;		        /* -1000 : +1000 */
  int red_intensity;	        /* -1000 : +1000 */
  int green_intensity;          /* -1000 : +1000 */
  int blue_intensity;           /* -1000 : +1000 */
  vidix_veq_flag_t flags;	/* currently specifies ITU YCrCb color
                                   space to use */
} vidix_video_eq_t;

typedef enum vidix_interlace_flag {
  /* stream is not interlaced */
  CFG_NON_INTERLACED       =	0x00000000,
  /* stream is interlaced */
  CFG_INTERLACED           =	0x00000001,
  /* first frame contains even fields but second - odd */
  CFG_EVEN_ODD_INTERLACING =	0x00000002,
  /* first frame contains odd fields but second - even */
  CFG_ODD_EVEN_INTERLACING =	0x00000004,
  /* field deinterlace_pattern is valid */
  CFG_UNIQUE_INTERLACING   =	0x00000008,
  /* unknown deinterlacing - use adaptive if it's possible */
  CFG_UNKNOWN_INTERLACING  =	0x0000000f,
} vidix_interlace_flag_t;

typedef struct vidix_deinterlace_s {
  vidix_interlace_flag_t flags;
  unsigned deinterlace_pattern;	/* app -> driver: deinterlace pattern if
                                   flag CFG_UNIQUE_INTERLACING is set */
} vidix_deinterlace_t;

typedef struct vidix_slice_s {
  void *address;		/* app -> driver */
  unsigned size;		/* app -> driver */
  vidix_rect_t slice;		/* app -> driver */
} vidix_slice_t;

typedef enum vidix_bm_flag {
  LVO_DMA_NOSYNC	     = 0,
  /* waits for vsync or hsync */
  LVO_DMA_SYNC               = 1,
} vidix_dma_flag_t;

typedef struct vidix_dma_s
{
  vidix_slice_t	src;                    /* app -> driver */
  vidix_slice_t	dest;			/* app -> driver */
  vidix_dma_flag_t flags;		/* app -> driver */
} vidix_dma_t;

typedef enum vidix_fx_type {
  FX_TYPE_BOOLEAN =		0x00000000,
  FX_TYPE_INTEGER =		0x00000001,
} vidix_fx_type_t;

/*
   This structure is introdused to support OEM effects like:
   - sharpness
   - exposure
   - (auto)gain
   - H(V)flip
   - black level
   - white balance
   and many other
*/
typedef struct vidix_oem_fx_s
{
  vidix_fx_type_t type;	/* type of effects */
  int num;		/* app -> driver: effect number.
                           From 0 to max number of effects */
  int minvalue;		/* min value of effect. 0 - for boolean */
  int maxvalue;		/* max value of effect. 1 - for boolean */
  int value;   	        /* current value of effect on get; required on set */
  char *name[80];	/* effect name to display */
} vidix_oem_fx_t;

typedef struct VDXDriver {
  const char *name;
  struct VDXDriver *next;
  int (* probe) (int verbose, int force);
  int (* get_caps) (vidix_capability_t *cap);
  int (*query_fourcc)(vidix_fourcc_t *);
  int (*init)(void);
  void (*destroy)(void);
  int (*config_playback)(vidix_playback_t *);
  int (*playback_on)( void );
  int (*playback_off)( void );
  /* Functions below can be missed in driver ;) */
  int (*frame_sel)( unsigned frame_idx );
  int (*get_eq)( vidix_video_eq_t * );
  int (*set_eq)( const vidix_video_eq_t * );
  int (*get_deint)( vidix_deinterlace_t * );
  int (*set_deint)( const vidix_deinterlace_t * );
  int (*copy_frame)( const vidix_dma_t * );
  int (*get_gkey)( vidix_grkey_t * );
  int (*set_gkey)( const vidix_grkey_t * );
} VDXDriver;

typedef struct VDXContext {
  VDXDriver *drv;
  /* might be filled in by much more info later on */
} VDXContext;

/***************************************************************************/
/*                              PUBLIC API                                 */
/***************************************************************************/

/* Opens corresponded video driver and returns handle of associated stream.
 *   path - specifies path where drivers are located.
 *   name - specifies prefered driver name (can be NULL).
 *   cap  - specifies driver capability (TYPE_* constants).
 *   verbose - specifies verbose level
 * returns handle if ok else NULL.
 */
VDXContext *vdlOpen (const char *name,unsigned cap,int verbose);

/* Closes stream and corresponded driver. */
void vdlClose (VDXContext *ctx);

/* Queries driver capabilities. Return 0 if ok else errno */
int vdlGetCapability (VDXContext *, vidix_capability_t *);

/* Queries support for given fourcc. Returns 0 if ok else errno */
int vdlQueryFourcc (VDXContext *, vidix_fourcc_t *);

/* Returns 0 if ok else errno */
int vdlConfigPlayback (VDXContext *, vidix_playback_t *);

/* Returns 0 if ok else errno */
int vdlPlaybackOn (VDXContext *);

/* Returns 0 if ok else errno */
int vdlPlaybackOff (VDXContext *);

/* Returns 0 if ok else errno */
int vdlPlaybackFrameSelect (VDXContext *, unsigned frame_idx);

/* Returns 0 if ok else errno */
int vdlGetGrKeys (VDXContext *, vidix_grkey_t *);

/* Returns 0 if ok else errno */
int vdlSetGrKeys (VDXContext *, const vidix_grkey_t *);

/* Returns 0 if ok else errno */
int vdlPlaybackGetEq (VDXContext *, vidix_video_eq_t *);

/* Returns 0 if ok else errno */
int vdlPlaybackSetEq (VDXContext *, const vidix_video_eq_t *);

/* Returns 0 if ok else errno */
int vdlPlaybackGetDeint (VDXContext *, vidix_deinterlace_t *);

/* Returns 0 if ok else errno */
int vdlPlaybackSetDeint (VDXContext *, const vidix_deinterlace_t *);

/* Returns 0 if ok else errno */
int vdlQueryNumOemEffects (VDXContext *, unsigned *number);

#endif /* MPLAYER_VIDIX_H */
