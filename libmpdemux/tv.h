#include "config.h"

#ifdef USE_TV
extern float tv_param_freq;
extern char *tv_param_channel;
extern char *tv_param_norm;
extern int tv_param_on;
extern char *tv_param_device;
extern char *tv_param_driver;
extern int tv_param_width;
extern int tv_param_height;

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
    int (*exit)();
    int (*control)();
    int (*grab_video_frame)();
    int (*get_video_framesize)();
    int (*grab_audio_frame)();
    int (*get_audio_framesize)();
} tvi_functions_t;

typedef struct tvi_handle_s {
    tvi_info_t *info;
    tvi_functions_t *functions;
    void *priv;
} tvi_handle_t;

#define TVI_CONTROL_FALSE		0
#define TVI_CONTROL_TRUE		1
#define TVI_CONTROL_NA			-1
#define TVI_CONTROL_UNKNOWN		-2


#define TVI_CONTROL_VID_GET_FPS		1
#define TVI_CONTROL_VID_GET_PLANES	2
#define TVI_CONTROL_VID_GET_BITS	3
#define TVI_CONTROL_VID_CHK_BITS	4
#define TVI_CONTROL_VID_SET_BITS	5
#define TVI_CONTROL_VID_GET_FORMAT	6
#define TVI_CONTROL_VID_CHK_FORMAT	7
#define TVI_CONTROL_VID_SET_FORMAT	8
#define TVI_CONTROL_VID_GET_WIDTH	9
#define TVI_CONTROL_VID_CHK_WIDTH	10
#define TVI_CONTROL_VID_SET_WIDTH	11
#define TVI_CONTROL_VID_GET_HEIGHT	12
#define TVI_CONTROL_VID_CHK_HEIGHT	13
#define TVI_CONTROL_VID_SET_HEIGHT	14

#define TVI_CONTROL_TUN_GET_FREQ	100
#define TVI_CONTROL_TUN_SET_FREQ	101

#endif /* USE_TV */
