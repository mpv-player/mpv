#ifndef MPLAYER_TV_H
#define MPLAYER_TV_H

//#include "libao2/afmt.h"
//#include "libmpcodecs/img_format.h"
//#include "mp_msg.h"

typedef struct tv_param_s {
    char *freq;
    char *channel;
    char *chanlist;
    char *norm;
    int automute;
    int normid;
    char *device;
    char *driver;
    int width;
    int height;
    int input;
    int outfmt;
    float fps;
    char **channels;
    int noaudio;
    int immediate;
    int audiorate;
    int audio_id;
    int amode;
    int volume;
    int bass;
    int treble;
    int balance;
    int forcechan;
    int force_audio;
    int buffer_size;
    int mjpeg;
    int decimation;
    int quality;
    int alsa;
    char* adevice;
    int brightness;
    int contrast;
    int hue;
    int saturation;
    int gain;
    char *tdevice;  ///< teletext device
    int tformat;    ///< teletext display format
    int tpage;      ///< start teletext page
    int tlang;      ///< primary language code

    int scan;
    int scan_threshold;
    float scan_period;
    /**
      Terminate stream with video renderer instead of Null renderer 
      Will help if video freezes but audio does not.
      May not work with -vo directx and -vf crop combination.
    */
    int hidden_video_renderer;
    /**
      For VIVO cards VP pin have to be rendered too.
      This tweak will cause VidePort pin stream to be terminated with video renderer 
      instead of removing it from graph.
      Use if your card have vp pin and video is still choppy.
      May not work with -vo directx and -vf crop combination.
    */
    int hidden_vp_renderer;
    /**
      Use system clock as sync source instead of default graph clock (usually the clock 
      from one of live sources in graph.
    */
    int system_clock;
    /**
      Some audio cards creates audio chunks with about 0.5 sec size.
      This can cause choppy video when using mplayer with immediatemode=0
      Use followingtweak to decrease audio chunk sizes.
      It will create audio chunks with time length equal to one video frame time.
    */
    int normalize_audio_chunks;
} tv_param_t;
  
extern tv_param_t stream_tv_defaults;
 
typedef struct tvi_info_s
{
    struct tvi_handle_s * (*tvi_init)(tv_param_t* tv_param);
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
    double (*grab_video_frame)();
    int (*get_video_framesize)();
    double (*grab_audio_frame)();
    int (*get_audio_framesize)();
} tvi_functions_t;

typedef struct tvi_handle_s {
    const tvi_functions_t	*functions;
    void		*priv;
    int 		seq;

    /* specific */
    int			norm;
    int			chanlist;
    const struct CHANLIST *chanlist_s;
    int			channel;
    tv_param_t          * tv_param;
    void                * scan;
} tvi_handle_t;

typedef struct tv_channels_s {
    int index;
    char number[5];
    char name[20];
    int norm;
    int   freq;
    struct tv_channels_s *next;
    struct tv_channels_s *prev;
} tv_channels_t;

extern tv_channels_t *tv_channel_list;
extern tv_channels_t *tv_channel_current, *tv_channel_last;
extern char *tv_channel_last_real;

typedef struct {
    unsigned int     scan_timer;
    int     channel_num;
    int     new_channels;
} tv_scan_t;

#define TVI_CONTROL_FALSE		0
#define TVI_CONTROL_TRUE		1
#define TVI_CONTROL_NA			-1
#define TVI_CONTROL_UNKNOWN		-2

/* ======================== CONTROLS =========================== */

/* GENERIC controls */
#define TVI_CONTROL_IS_AUDIO		0x1
#define TVI_CONTROL_IS_VIDEO		0x2
#define TVI_CONTROL_IS_TUNER		0x3
#define TVI_CONTROL_IMMEDIATE           0x4

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
#define TVI_CONTROL_VID_GET_BRIGHTNESS	0x115
#define TVI_CONTROL_VID_SET_BRIGHTNESS	0x116
#define TVI_CONTROL_VID_GET_HUE		0x117
#define TVI_CONTROL_VID_SET_HUE		0x118
#define TVI_CONTROL_VID_GET_SATURATION	0x119
#define TVI_CONTROL_VID_SET_SATURATION	0x11a
#define TVI_CONTROL_VID_GET_CONTRAST	0x11b
#define TVI_CONTROL_VID_SET_CONTRAST	0x11c
#define TVI_CONTROL_VID_GET_PICTURE	0x11d
#define TVI_CONTROL_VID_SET_PICTURE	0x11e
#define TVI_CONTROL_VID_SET_GAIN	0x11f
#define TVI_CONTROL_VID_GET_GAIN	0x120

/* TUNER controls */
#define TVI_CONTROL_TUN_GET_FREQ	0x201
#define TVI_CONTROL_TUN_SET_FREQ	0x202
#define TVI_CONTROL_TUN_GET_TUNER	0x203	/* update priv->tuner struct for used input */
#define TVI_CONTROL_TUN_SET_TUNER	0x204	/* update priv->tuner struct for used input */
#define TVI_CONTROL_TUN_GET_NORM	0x205
#define TVI_CONTROL_TUN_SET_NORM	0x206
#define TVI_CONTROL_TUN_GET_SIGNAL	0x207

/* AUDIO controls */
#define TVI_CONTROL_AUD_GET_FORMAT	0x301
#define TVI_CONTROL_AUD_GET_SAMPLERATE	0x302
#define TVI_CONTROL_AUD_GET_SAMPLESIZE	0x303
#define TVI_CONTROL_AUD_GET_CHANNELS	0x304
#define TVI_CONTROL_AUD_SET_SAMPLERATE	0x305

/* SPECIFIC controls */
#define TVI_CONTROL_SPC_GET_INPUT	0x401	/* set input channel (tv,s-video,composite..) */
#define TVI_CONTROL_SPC_SET_INPUT	0x402	/* set input channel (tv,s-video,composite..) */
#define TVI_CONTROL_SPC_GET_NORMID	0x403	/* get normid from norm name */

//tvi_* ioctl (not tvi_vbi.c !!!)
#define TVI_CONTROL_VBI_INIT           0x501   ///< vbi init

/* 
  TELETEXT controls (through tv_teletext_control() ) 
   NOTE: 
    _SET_ should be _GET_ +1
   _STEP_ should be _GET_ +2
*/
#define TV_VBI_CONTROL_GET_MODE        0x510   ///< get current mode teletext
#define TV_VBI_CONTROL_SET_MODE        0x511   ///< on/off grab teletext

#define TV_VBI_CONTROL_GET_PAGE        0x513   ///< get grabbed teletext page
#define TV_VBI_CONTROL_SET_PAGE        0x514   ///< set grab teletext page number
#define TV_VBI_CONTROL_STEP_PAGE       0x515   ///< step grab teletext page number

#define TV_VBI_CONTROL_GET_SUBPAGE     0x516   ///< get grabbed teletext page
#define TV_VBI_CONTROL_SET_SUBPAGE     0x517   ///< set grab teletext page number

#define TV_VBI_CONTROL_GET_FORMAT      0x519   ///< get eletext format
#define TV_VBI_CONTROL_SET_FORMAT      0x51a   ///< set teletext format

#define TV_VBI_CONTROL_GET_HALF_PAGE   0x51c   ///< get current half page
#define TV_VBI_CONTROL_SET_HALF_PAGE   0x51d   ///< switch half page

#define TV_VBI_CONTROL_IS_CHANGED      0x540   ///< teletext page is changed
#define TV_VBI_CONTROL_MARK_UNCHANGED  0x541   ///< teletext page is changed

#define TV_VBI_CONTROL_ADD_DEC         0x550   ///< add page number with dec
#define TV_VBI_CONTROL_GO_LINK         0x551   ///< go link (1..6) NYI
#define TV_VBI_CONTROL_GET_VBIPAGE     0x552   ///< get vbi_image for grabbed teletext page
#define TV_VBI_CONTROL_RESET           0x553   ///< vbi reset
#define TV_VBI_CONTROL_START           0x554   ///< vbi start
#define TV_VBI_CONTROL_STOP            0x555   ///< vbi stop
#define TV_VBI_CONTROL_DECODE_PAGE     0x556   ///< decode vbi page
#define TV_VBI_CONTROL_GET_NETWORKNAME 0x557   ///< get current network name

int tv_set_color_options(tvi_handle_t *tvh, int opt, int val);
int tv_get_color_options(tvi_handle_t *tvh, int opt, int* val);
#define TV_COLOR_BRIGHTNESS	1
#define TV_COLOR_HUE		2
#define TV_COLOR_SATURATION	3
#define TV_COLOR_CONTRAST	4

int tv_step_channel_real(tvi_handle_t *tvh, int direction);
int tv_step_channel(tvi_handle_t *tvh, int direction);
#define TV_CHANNEL_LOWER	1
#define TV_CHANNEL_HIGHER	2

int tv_last_channel(tvi_handle_t *tvh);

int tv_set_channel_real(tvi_handle_t *tvh, char *channel);
int tv_set_channel(tvi_handle_t *tvh, char *channel);

int tv_step_norm(tvi_handle_t *tvh);
int tv_step_chanlist(tvi_handle_t *tvh);

int tv_set_freq(tvi_handle_t *tvh, unsigned long freq);
int tv_get_freq(tvi_handle_t *tvh, unsigned long *freq);
int tv_get_signal(tvi_handle_t *tvh);
int tv_step_freq(tvi_handle_t *tvh, float step_interval);

int tv_set_norm(tvi_handle_t *tvh, char* norm);

void tv_start_scan(tvi_handle_t *tvh, int start);

#define TV_NORM_PAL		1
#define TV_NORM_NTSC		2
#define TV_NORM_SECAM		3
#define TV_NORM_PALNC		4
#define TV_NORM_PALM		5
#define TV_NORM_PALN		6
#define TV_NORM_NTSCJP		7

#define VBI_TFORMAT_TEXT    0               ///< text mode
#define VBI_TFORMAT_BW      1               ///< back&white mode
#define VBI_TFORMAT_GRAY    2               ///< grayscale mode
#define VBI_TFORMAT_COLOR   3               ///< color mode (require color_spu patch!)

#define VBI_MAX_PAGES      0x800            ///< max sub pages number
#define VBI_MAX_SUBPAGES   64               ///< max sub pages number

#define VBI_ROWS    25                      ///< teletext page height in rows
#define VBI_COLUMNS 40                      ///< teletext page width in chars
#define VBI_TIME_LINEPOS    26              ///< time line pos in page header

typedef
enum{
    TT_FORMAT_OPAQUE=0,       ///< opaque
    TT_FORMAT_TRANSPARENT,    ///< translarent
    TT_FORMAT_OPAQUE_INV,     ///< opaque with inverted colors
    TT_FORMAT_TRANSPARENT_INV ///< translarent with inverted colors
} teletext_format;

typedef
enum{
    TT_ZOOM_NORMAL=0,
    TT_ZOOM_TOP_HALF,
    TT_ZOOM_BOTTOM_HALF
} teletext_zoom;

typedef struct tt_char_s{
    unsigned int unicode; ///< unicode (utf8) character
    unsigned char fg;  ///< foreground color
    unsigned char bg;  ///< background color
    unsigned char gfx; ///< 0-no gfx, 1-solid gfx, 2-separated gfx
    unsigned char flh; ///< 0-no flash, 1-flash
    unsigned char hidden; ///< char is hidden (for subtitle pages)
    unsigned char ctl; ///< control character
    unsigned char lng; ///< lang: 0-secondary language,1-primary language
    unsigned char raw; ///< raw character (as received from device)
} tt_char;

typedef struct tt_link_s{
    int pagenum;          ///< page number
    int subpagenum;       ///< subpage number
} tt_link_t;    

typedef struct tt_page_s{
    int pagenum;          ///< page number
    int subpagenum;       ///< subpage number
    unsigned char primary_lang;   ///< primary language code
    unsigned char secondary_lang; ///< secondary language code
    unsigned char active; ///< page is complete and ready for rendering
    unsigned char flags;  ///< page flags
    unsigned char raw[VBI_ROWS*VBI_COLUMNS]; ///< page data
    struct tt_page_s* next_subpage;
    struct tt_link_s links[6];
}  tt_page;

#define TT_PGFL_SUPPRESS_HEADER  0x01
#define TT_PGFL_UPDATE_INDICATOR 0x02
#define TT_PGFL_INTERRUPTED_SEQ  0x04
#define TT_PGFL_INHIBIT_DISPLAY  0x08
#define TT_PGFL_NEWFLASH         0x10
#define TT_PGFL_SUBTITLE         0x20
#define TT_PGFL_ERASE_PAGE       0x40
#define TT_PGFL_MAGAZINE_SERIAL  0x80

typedef struct tt_stream_props_s{
    int sampling_rate;
    int samples_per_line;
    int offset;
    int count[2];     ///< number of lines in first and second fields
    int interlaced;   ///< vbi data are interlaced
    int bufsize;      ///< required buffer size
} tt_stream_props;

#endif /* MPLAYER_TV_H */
