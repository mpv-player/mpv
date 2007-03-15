
#include "codec-cfg.h"
#include "libaf/af_format.h"

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "ad.h"

static int init(sh_audio_t *sh);
static int preinit(sh_audio_t *sh);
static void uninit(sh_audio_t *sh);
static int control(sh_audio_t *sh,int cmd,void* arg, ...);
static int decode_audio(sh_audio_t *sh,unsigned char *buffer,int minlen,int maxlen);

#define LIBAD_EXTERN(x) ad_functions_t mpcodecs_ad_##x = {\
	&info,\
	preinit,\
	init,\
        uninit,\
	control,\
	decode_audio\
};

