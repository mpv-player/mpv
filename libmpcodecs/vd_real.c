#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#ifdef USE_REALCODECS

#include <dlfcn.h>

#include "mp_msg.h"
#include "help_mp.h"

#include "vd_internal.h"

static vd_info_t info = {
	"RealVideo decoder",
	"real",
	VFM_REAL,
	"Florian Schneider & A'rpi",
	"using original closed source codecs for Linux",
	"binary real video codecs"
};

LIBVD_EXTERN(real)


unsigned long (*rvyuv_custom_message)(unsigned long,unsigned long);
unsigned long (*rvyuv_free)(unsigned long);
unsigned long (*rvyuv_hive_message)(unsigned long,unsigned long);
unsigned long (*rvyuv_init)(unsigned long,unsigned long);
unsigned long (*rvyuv_transform)(unsigned long,unsigned long,unsigned long,unsigned long,unsigned long);

void *rv_handle=NULL;

void *__builtin_vec_new(unsigned long size) {
	return malloc(size);
}

void __builtin_vec_delete(void *mem) {
	free(mem);
}

void __pure_virtual(void) {
	printf("FATAL: __pure_virtual() called!\n");
//	exit(1);
}

#if defined(__FreeBSD__) || defined(__NetBSD__)
void ___brk_addr(void) {exit(0);}
char **__environ={NULL};
#undef stderr
FILE *stderr=NULL;
#endif

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
//    switch(cmd){
//    case VDCTRL_QUERY_MAX_PP_LEVEL:
//	return 9;
//    case VDCTRL_SET_PP_LEVEL:
//	vfw_set_postproc(sh,10*(*((int*)arg)));
//	return CONTROL_OK;
//    }
    return CONTROL_UNKNOWN;
}

/* exits program when failure */
int load_syms_linux(char *path) {
		void *handle;
		char *error;

		mp_msg(MSGT_DECVIDEO,MSGL_INFO, "opening shared obj '%s'\n", path);
		rv_handle = dlopen (path, RTLD_LAZY);
		handle=rv_handle;
		if (!handle) {
			mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error: %s\n",dlerror());
			return 0;
		}

		rvyuv_custom_message = dlsym(handle, "RV20toYUV420CustomMessage");
		rvyuv_free = dlsym(handle, "RV20toYUV420Free");
		rvyuv_hive_message = dlsym(handle, "RV20toYUV420HiveMessage");
		rvyuv_init = dlsym(handle, "RV20toYUV420Init");
		rvyuv_transform = dlsym(handle, "RV20toYUV420Transform");

    if(rvyuv_custom_message &&
       rvyuv_free &&
       rvyuv_hive_message &&
       rvyuv_init &&
       rvyuv_transform) return 1;

    mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error resolving symbols! (version incompatibility?)\n");
    return 0;
}

#ifdef USE_WIN32DLL
int load_syms_windows(char *path) {
    void *handle;
    Setup_LDT_Keeper();
    rv_handle = handle = LoadLibraryA(path);
    mp_msg(MSGT_DECVIDEO,MSGL_V,"win32 real codec handle=%p  \n",handle);

    rvyuv_custom_message = GetProcAddress(handle, "RV20toYUV420CustomMessage");
    rvyuv_free = GetProcAddress(handle, "RV20toYUV420Free");
    rvyuv_hive_message = GetProcAddress(handle, "RV20toYUV420HiveMessage");
    rvyuv_init = GetProcAddress(handle, "RV20toYUV420Init");
    rvyuv_transform = GetProcAddress(handle, "RV20toYUV420Transform");
    
    if(rvyuv_custom_message &&
       rvyuv_free &&
       rvyuv_hive_message &&
       rvyuv_init &&
       rvyuv_transform) return 1;
    return 0; // error
}
#endif

/* we need exact positions */
struct rv_init_t {
	short unk1;
	short w;
	short h;
	short unk3;
	int unk2;
	int subformat;
	int unk5;
	int format;
} rv_init_t;

// init driver
static int init(sh_video_t *sh){
	//unsigned int out_fmt;
	char path[4096];
	int result;
	// we export codec id and sub-id from demuxer in bitmapinfohdr:
	unsigned int* extrahdr=(unsigned int*)(sh->bih+1);
	struct rv_init_t init_data={
		11, sh->disp_w, sh->disp_h,0,0,extrahdr[0],
		1,extrahdr[1]}; // rv30

	mp_msg(MSGT_DECVIDEO,MSGL_V,"realvideo codec id: 0x%08X  sub-id: 0x%08X\n",extrahdr[1],extrahdr[0]);

	sprintf(path, REALCODEC_PATH "/%s", sh->codec->dll);

	/* first try to load linux dlls, if failed and we're supporting win32 dlls,
	   then try to load the windows ones */
	if(!load_syms_linux(path))
#ifdef USE_WIN32DLL
	    if (!load_syms_windows(path))
#endif
	{
		mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingDLLcodec,sh->codec->dll);
		mp_msg(MSGT_DECVIDEO,MSGL_HINT,"You need to copy the contents from the RealPlayer codecs directory\n");
		mp_msg(MSGT_DECVIDEO,MSGL_HINT,"into " REALCODEC_PATH "/ !\n");
		return 0;
	}
	// only I420 supported
	if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_I420)) return 0;
	// init codec:
	sh->context=NULL;
	result=(*rvyuv_init)(&init_data, &sh->context);
	if (result){
	    mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Couldn't open RealVideo codec, error code: 0x%X  \n",result);
	    return 0;
	}
	// setup rv30 codec (codec sub-type and image dimensions):
	if(extrahdr[1]>=0x30000000){
	    unsigned long cmsg24[4]={sh->disp_w,sh->disp_h,sh->disp_w,sh->disp_h};
	    unsigned long cmsg_data[3]={0x24,1+((extrahdr[0]>>16)&7),&cmsg24};
	    (*rvyuv_custom_message)(cmsg_data,sh->context);
	}
	mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: RealVideo codec init OK!\n");
	return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
	if(rv_handle) dlclose(rv_handle);
	rv_handle=NULL;
}

// copypaste from demux_real.c - it should match to get it working!
typedef struct dp_hdr_s {
    uint32_t chunks;	// number of chunks
    uint32_t timestamp; // timestamp from packet header
    uint32_t len;	// length of actual data
    uint32_t chunktab;	// offset to chunk offset array
} dp_hdr_t;

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
	mp_image_t* mpi;
	unsigned long result;
	dp_hdr_t* dp_hdr=(dp_hdr_t*)data;
	unsigned char* dp_data=((unsigned char*)data)+sizeof(dp_hdr_t);
	uint32_t* extra=(uint32_t*)(((char*)data)+dp_hdr->chunktab);

	unsigned long transform_out[5];
	unsigned long transform_in[6]={
		dp_hdr->len,	// length of the packet (sub-packets appended)
		0,		// unknown, seems to be unused
		dp_hdr->chunks,	// number of sub-packets - 1
		extra,		// table of sub-packet offsets
		0,		// unknown, seems to be unused
		dp_hdr->timestamp,// timestamp (the integer value from the stream)
	};

	if(len<=0 || flags&2) return NULL; // skipped frame || hardframedrop

	mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/,
		sh->disp_w, sh->disp_h);
	if(!mpi) return NULL;
	
	result=(*rvyuv_transform)(dp_data, mpi->planes[0], transform_in,
		transform_out, sh->context);

	return (result?NULL:mpi);
}

#endif
