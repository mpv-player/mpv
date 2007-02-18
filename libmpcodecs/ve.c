#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

extern vf_info_t ve_info_lavc;
extern vf_info_t ve_info_vfw;
extern vf_info_t ve_info_raw;
extern vf_info_t ve_info_libdv;
extern vf_info_t ve_info_xvid;
extern vf_info_t ve_info_qtvideo;
extern vf_info_t ve_info_nuv;
extern vf_info_t ve_info_x264;

static vf_info_t* encoder_list[]={
#ifdef USE_LIBAVCODEC
    &ve_info_lavc,
#endif
#ifdef USE_WIN32DLL
    &ve_info_vfw,
#ifdef USE_QTX_CODECS
    &ve_info_qtvideo,
#endif
#endif
#ifdef HAVE_LIBDV095
    &ve_info_libdv,
#endif
    &ve_info_raw,
#ifdef HAVE_XVID4
    &ve_info_xvid,
#endif
#ifdef USE_LIBLZO
    &ve_info_nuv,
#endif
#ifdef HAVE_X264
    &ve_info_x264,
#endif
    NULL
};

vf_instance_t* vf_open_encoder(vf_instance_t* next, const char *name, char *args){
    char* vf_args[] = { "_oldargs_", args, NULL };
    return vf_open_plugin(encoder_list,next,name,vf_args);
}

