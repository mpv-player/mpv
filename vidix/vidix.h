/*
 * vidix.h
 * VIDIX - VIDeo Interface for *niX
 *   This interface is introduced as universal one to MPEG decoder,
 *   BES == Back End Scaler and YUV2RGB hw accelerators.
 * In the future it may be expanded up to capturing and audio things.
 * Main goal of this this interface imlpementation is providing DGA
 * everywhere where it's possible (unlike X11 and other).
 * Copyright 2002 Nick Kurshev
 * Licence: GPL
 * This interface is based on v4l2, fbvid.h, mga_vid.h projects
 * and personally my ideas.
 * NOTE: This interface is introduces as driver interface.
 * Don't use it for APP.
*/
#ifndef VIDIX_H
#define VIDIX_H

#ifdef __cplusplus
extern "C" {
#endif

#define VIDIX_VERSION 100

			/* returns driver version */
extern unsigned vixGetVersion( void );

#define PROBE_NORMAL	0 /* normal probing */
#define PROBE_FORCE	1 /* ignore device_id but recognize device if it's known */
			/* Probes video hw.
			   verbose - specifies verbose level.
			   force   - specifies force mode - driver should ignore
			             device_id (danger but useful for new devices)
			   Returns 0 if ok else errno */
extern int	vixProbe( int verbose, int force );
			/* Initializes driver. Returns 0 if ok else errno */
extern int	vixInit( void );
			/* Destroys driver */
extern void	vixDestroy( void );

typedef struct vidix_capability_s
{
	char	name[32];	/* Driver name */
#define TYPE_OUTPUT	0x00000000	/* Is a video playback device */
#define TYPE_CAPTURE	0x00000001	/* Is a capture device */
#define TYPE_CODEC	0x00000002	/* Device supports hw (de)coding */
#define TYPE_FX		0x00000004	/* Is a video effects device */
	int	type;		/* Device type, see below */
	unsigned reserved0[4];
	int	maxwidth;
	int	maxheight;
	int	minwidth;
	int	minheight;
	int	maxframerate;   /* -1 if unlimited */
#define FLAG_NONE		0x00000000 /* No flags defined */
#define FLAG_DMA		0x00000001 /* Card can use DMA */
#define FLAG_EQ_DMA		0x00000002 /* Card can use DMA only if src pitch == dest pitch */
#define FLAG_UPSCALER		0x00000010 /* Card supports hw upscaling */
#define FLAG_DOWNSCALER		0x00000020 /* Card supports hw downscaling */
#define FLAG_SUBPIC		0x00001000 /* Card supports DVD subpictures */
#define FLAG_EQUALIZER		0x00002000 /* Card supports equalizer */
	unsigned flags;		/* Feature flags, see above */
	unsigned short vendor_id;
	unsigned short device_id;
	unsigned reserved[4];
}vidix_capability_t;

			/* Should fill at least type before init.
			   Returns 0 if ok else errno */
extern int	vixGetCapability(vidix_capability_t *);

typedef struct vidix_fourcc_s
{
	unsigned fourcc;
#define VID_DEPTH_NONE		0x0000
#define VID_DEPTH_1BPP		0x0001
#define VID_DEPTH_2BPP		0x0002
#define VID_DEPTH_4BPP		0x0004
#define VID_DEPTH_8BPP		0x0008
#define VID_DEPTH_12BPP		0x0010
#define VID_DEPTH_15BPP		0x0020
#define VID_DEPTH_16BPP		0x0040
#define VID_DEPTH_24BPP		0x0080
#define VID_DEPTH_32BPP		0x0100
	unsigned depth;
#define VID_CAP_NONE			0x0000
#define VID_CAP_EXPAND			0x0001 /* if overlay can be bigger than source */
#define VID_CAP_SHRINK			0x0002 /* if overlay can be smaller than source */
#define VID_CAP_BLEND			0x0004 /* if overlay can be blended with framebuffer */
#define VID_CAP_COLORKEY		0x0008 /* if overlay can be restricted to a colorkey */
#define VID_CAP_ALPHAKEY		0x0010 /* if overlay can be restricted to an alpha channel */
#define VID_CAP_COLORKEY_ISRANGE	0x0020 /* if the colorkey can be a range */
#define VID_CAP_ALPHAKEY_ISRANGE	0x0040 /* if the alphakey can be a range */
#define VID_CAP_COLORKEY_ISMAIN		0x0080 /* colorkey is checked against framebuffer */
#define VID_CAP_COLORKEY_ISOVERLAY	0x0100 /* colorkey is checked against overlay */
#define VID_CAP_ALPHAKEY_ISMAIN		0x0200 /* alphakey is checked against framebuffer */
#define VID_CAP_ALPHAKEY_ISOVERLAY	0x0400 /* alphakey is checked against overlay */
	unsigned flags;
}vidix_fourcc_t;

			/* Returns 0 if ok else errno */
extern int	vixQueryFourcc(vidix_fourcc_t *);

typedef struct vidix_yuv_s
{
	unsigned y,u,v;
}vidix_yuv_t;

typedef struct vidix_rect_s
{
	unsigned x,y,w,h;	/* in pixels */
	vidix_yuv_t pitch;	/* line-align in bytes */
}vidix_rect_t;

typedef struct vidix_color_key_s
{
#define CKEY_FALSE	0
#define CKEY_TRUE	1
#define CKEY_EQ		2
#define CKEY_NEQ	3
	unsigned	op;		/* defines logical operation */
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
	unsigned char	reserved;
}vidix_ckey_t;

typedef struct vidix_video_key_s
{
#define VKEY_FALSE	0
#define VKEY_TRUE	1
#define VKEY_EQ		2
#define VKEY_NEQ	3
	unsigned	op;		/* defines logical operation */
	unsigned char	key[8];
}vidix_vkey_t;

typedef struct vidix_playback_s
{
	unsigned	fourcc;		/* app -> driver: movies's fourcc */
	unsigned	capability;	/* app -> driver: what capability to use */
	unsigned	blend_factor;	/* app -> driver: blenfing factor */
	vidix_rect_t	src;            /* app -> driver: original movie size */
	vidix_rect_t	dest;           /* app -> driver: destinition movie size. driver->app dest_pitch */
	/* memory model */
	unsigned	frame_size;		/* driver -> app; destinition frame size */
	unsigned	num_frames;		/* app -> driver: after call: driver -> app */
#define LVO_MAXFRAMES 32
	unsigned	offsets[LVO_MAXFRAMES];	/* driver -> app */
	vidix_yuv_t	offset;			/* driver -> app: relative offsets within frame for yuv planes */
	void*		dga_addr;		/* driver -> app: linear address */
}vidix_playback_t;

			/* Returns 0 if ok else errno */
extern int	vixConfigPlayback(vidix_playback_t *);

			/* Returns 0 if ok else errno */
extern int 	vixPlaybackOn( void );

			/* Returns 0 if ok else errno */
extern int 	vixPlaybackOff( void );

			/* Returns 0 if ok else errno */
extern int 	vixPlaybackFrameSelect( unsigned frame_idx );

typedef struct vidix_grkey_s
{
	vidix_ckey_t	ckey;		/* app -> driver: color key */
	vidix_vkey_t	vkey;		/* app -> driver: video key */
#define KEYS_PUT	0
#define KEYS_AND	1
#define KEYS_OR		2
#define KEYS_XOR	3
	unsigned	key_op;		/* app -> driver: keys operations */
}vidix_grkey_t;

			/* Returns 0 if ok else errno */
extern int 	vixGetGrKeys( vidix_grkey_t * );

			/* Returns 0 if ok else errno */
extern int 	vixSetGrKeys( const vidix_grkey_t * );


typedef struct vidix_video_eq_s
{
/* end-user app can have presets like: cold-normal-hot picture and so on */
	int		brightness;	/* -1000 : +1000 */
	int		contrast;	/* -1000 : +1000 */
	int		saturation;	/* -1000 : +1000 */
	int		hue;		/* -1000 : +1000 */
	int		red_intense;	/* -1000 : +1000 */
	int		green_intense;  /* -1000 : +1000 */
	int		blue_intense;   /* -1000 : +1000 */
#define VEQ_FLG_ITU_R_BT_601	0x00000000 /* ITU-R BT.601 colour space (default) */
#define VEQ_FLG_ITU_R_BT_709	0x00000001 /* ITU-R BT.709 colour space */
#define VEQ_FLG_ITU_MASK	0x0000000f
	int		flags;		/* currently specifies ITU YCrCb color space to use */
}vidix_video_eq_t;

			/* Returns 0 if ok else errno */
extern int 	vixPlaybackGetEq( vidix_video_eq_t * );

			/* Returns 0 if ok else errno */
extern int 	vixPlaybackSetEq( const vidix_video_eq_t * );

typedef struct vidix_deinterlace_s
{
#define CFG_NON_INTERLACED		0x00000000 /* stream is not interlaced */
#define CFG_INTERLACED			0x00000001 /* stream is interlaced */
#define CFG_EVEN_ODD_INTERLACING	0x00000002 /* first frame contains even fields but second - odd */
#define CFG_ODD_EVEN_INTERLACING	0x00000004 /* first frame contains odd fields but second - even */
#define CFG_UNIQUE_INTERLACING		0x00000008 /* field deinterlace_pattern is valid */
#define CFG_UNKNOWN_INTERLACING		0x0000000f /* unknown deinterlacing - use adaptive if it's possible */
	unsigned	flags;
	unsigned	deinterlace_pattern;	/* app -> driver: deinterlace pattern if flag CFG_UNIQUE_INTERLACING is set */
}vidix_deinterlace_t;

			/* Returns 0 if ok else errno */
extern int 	vixPlaybackGetDeint( vidix_deinterlace_t * );

			/* Returns 0 if ok else errno */
extern int 	vixPlaybackSetDeint( const vidix_deinterlace_t * );

typedef struct vidix_slice_s
{
	void*		address;		/* app -> driver */
	unsigned	size;			/* app -> driver */
	vidix_rect_t	slice;			/* app -> driver */
}vidix_slice_t;

typedef struct vidix_dma_s
{
	vidix_slice_t	src;                    /* app -> driver */
	vidix_slice_t	dest;			/* app -> driver */
#define LVO_DMA_NOSYNC		0
#define LVO_DMA_SYNC		1       /* means: wait vsync or hsync */
	unsigned	flags;			/* app -> driver */
}vidix_dma_t;

			/* Returns 0 if ok else errno */
extern int 	vixPlaybackCopyFrame( const vidix_dma_t * );

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
#define FX_TYPE_BOOLEAN		0x00000000
#define FX_TYPE_INTEGER		0x00000001
	int		type;			/* type of effects */
	int		num;			/* app -> driver: effect number. From 0 to max number of effects */
	int		minvalue;		/* min value of effect. 0 - for boolean */
	int		maxvalue;		/* max value of effect. 1 - for boolean */
	int		value;			/* current value of effect on 'get'; required on set */
	char *		name[80];		/* effect name to display */
}vidix_oem_fx_t;

			/* Returns 0 if ok else errno */
extern int	vixQueryNumOemEffects( unsigned * number );

			/* Returns 0 if ok else errno */
extern int	vixGetOemEffect( vidix_oem_fx_t * );

			/* Returns 0 if ok else errno */
extern int	vixSetOemEffect( const vidix_oem_fx_t * );

#ifdef __cplusplus
}
#endif

#endif
