
#include "codec-cfg.h"
#include "img_format.h"

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "vd.h"

extern int divx_quality;

// prototypes:
//static vd_info_t info;
static int control(sh_video_t *sh,int cmd,void* arg,...);
static int init(sh_video_t *sh);
static void uninit(sh_video_t *sh);
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags);

#define LIBVD_EXTERN(x) vd_functions_t mpcodecs_vd_##x = {\
	&info,\
	init,\
        uninit,\
	control,\
	decode\
};

