#include "config.h"

#ifdef USE_TV
#include "../libao2/afmt.h"
#include "../libvo/img_format.h"
#include "../libvo/fastmemcpy.h"

extern unsigned long tv_param_freq;
extern char *tv_param_channel;
extern char *tv_param_norm;
extern int tv_param_on;
extern char *tv_param_device;
extern char *tv_param_driver;
extern int tv_param_width;
extern int tv_param_height;
extern int tv_param_input;
extern char *tv_param_outfmt;

typedef struct tvi_info_s
{
    const char *name;
    const char *short_name;
    const char *author;
    const char *comment;
} tvi_info_t;

typedef struct tvi_functions_s
{
    int (*init)();
    int (*uninit)();
    int (*control)();
    int (*start)();
    int (*grab_video_frame)();
    int (*get_video_framesize)();
    int (*grab_audio_frame)();
    int (*get_audio_framesize)();
} tvi_functions_t;

typedef struct tvi_param_s {
    const char	*opt;
    void 	*value;
} tvi_param_t;

typedef struct tvi_handle_s {
    tvi_info_t		*info;
    tvi_functions_t	*functions;
    void		*priv;
    tvi_param_t		*params;
    int 		seq;
} tvi_handle_t;


#define TVI_CONTROL_FALSE		0
#define TVI_CONTROL_TRUE		1
#define TVI_CONTROL_NA			-1
#define TVI_CONTROL_UNKNOWN		-2

/* ======================== CONTROLS =========================== */

/* GENERIC controls */
#define TVI_CONTROL_IS_AUDIO		0x1
#define TVI_CONTROL_IS_VIDEO		0x2
#define TVI_CONTROL_IS_TUNER		0x3

/* VIDEO controls */
#define TVI_CONTROL_VID_GET_FPS		0x101
#define TVI_CONTROL_VID_GET_PLANES	0x102
#define TVI_CONTROL_VID_GET_BITS	0x103
#define TVI_CONTROL_VID_CHK_BITS	0x104
#define TVI_CONTROL_VID_SET_BITS	0x105
#define TVI_CONTROL_VID_GET_FORMAT	0x106
#define TVI_CONTROL_VID_CHK_FORMAT	0x107
#define TVI_CONTROL_VID_SET_FORMAT	0x108
#define TVI_CONTROL_VID_GET_WIDTH	0x109
#define TVI_CONTROL_VID_CHK_WIDTH	0x110
#define TVI_CONTROL_VID_SET_WIDTH	0x111
#define TVI_CONTROL_VID_GET_HEIGHT	0x112
#define TVI_CONTROL_VID_CHK_HEIGHT	0x113
#define TVI_CONTROL_VID_SET_HEIGHT	0x114

/* TUNER controls */
#define TVI_CONTROL_TUN_GET_FREQ	0x201
#define TVI_CONTROL_TUN_SET_FREQ	0x202
#define TVI_CONTROL_TUN_GET_TUNER	0x203	/* update priv->tuner struct for used input */
#define TVI_CONTROL_TUN_SET_TUNER	0x204	/* update priv->tuner struct for used input */
#define TVI_CONTROL_TUN_GET_NORM	0x205
#define TVI_CONTROL_TUN_SET_NORM	0x206

/* AUDIO controls */
#define TVI_CONTROL_AUD_GET_FORMAT	0x301
#define TVI_CONTROL_AUD_GET_SAMPLERATE	0x302
#define TVI_CONTROL_AUD_GET_SAMPLESIZE	0x303
#define TVI_CONTROL_AUD_GET_CHANNELS	0x304

/* SPECIFIC controls */
#define TVI_CONTROL_SPC_GET_INPUT	0x401	/* set input channel (tv,s-video,composite..) */
#define TVI_CONTROL_SPC_SET_INPUT	0x402	/* set input channel (tv,s-video,composite..) */

//extern int demux_tv_fill_buffer(demuxer_t *demux, tvi_handle_t *tvh);
//extern int demux_open_tv(demuxer_t *demux, tvi_handle_t *tvh);
extern tvi_handle_t *tv_begin(void);
extern int tv_init(tvi_handle_t *tvh);

#endif /* USE_TV */
