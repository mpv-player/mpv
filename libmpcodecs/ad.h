
#include "mpc_info.h"
typedef mp_codec_info_t ad_info_t;

/* interface of video decoder drivers */
typedef struct ad_functions_s
{
	ad_info_t *info;
        int (*preinit)(sh_audio_t *sh);
        int (*init)(sh_audio_t *sh);
        void (*uninit)(sh_audio_t *sh);
        int (*control)(sh_audio_t *sh,int cmd,void* arg, ...);
        int (*decode_audio)(sh_audio_t *sh,unsigned char* buffer,int minlen,int maxlen);
} ad_functions_t;

// NULL terminated array of all drivers
extern ad_functions_t* mpcodecs_ad_drivers[];

// fallback if ADCTRL_RESYNC not implemented: sh_audio->a_in_buffer_len=0;
#define ADCTRL_RESYNC_STREAM 1       /* resync, called after seeking! */

// fallback if ADCTRL_SKIP not implemented: ds_fill_buffer(sh_audio->ds);
#define ADCTRL_SKIP_FRAME 2         /* skip block/frame, called while seeking! */

// fallback if ADCTRL_QUERY_FORMAT not implemented: sh_audio->sample_format
#define ADCTRL_QUERY_FORMAT 3 /* test for availabilty of a format */

// fallback: use hw mixer in libao
#define ADCTRL_SET_VOLUME 4 /* set volume (used for mp3lib and liba52) */

