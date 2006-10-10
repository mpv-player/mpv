#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif

#include "mp_msg.h"
#include "help_mp.h"

#include "vd_internal.h"
#include "wine/windef.h"

static vd_info_t info = {
	"RealVideo decoder",
	"realvid",
	"Alex Beregszaszi",
	"Florian Schneider, Arpad Gereoffy, Alex Beregszaszi, Donnie Smith",
	"binary real video codecs"
};

LIBVD_EXTERN(realvid)


/*
 * Structures for data packets.  These used to be tables of unsigned ints, but
 * that does not work on 64 bit platforms (e.g. Alpha).  The entries that are
 * pointers get truncated.  Pointers on 64 bit platforms are 8 byte longs.
 * So we have to use structures so the compiler will assign the proper space
 * for the pointer.
 */
typedef struct cmsg_data_s {
	uint32_t data1;
	uint32_t data2;
	uint32_t* dimensions;
} cmsg_data_t;

typedef struct transform_in_s {
	uint32_t len;
	uint32_t unknown1;
	uint32_t chunks;
	uint32_t* extra;
	uint32_t unknown2;
	uint32_t timestamp;
} transform_in_t;

static unsigned long (*rvyuv_custom_message)(cmsg_data_t* ,void*);
static unsigned long (*rvyuv_free)(void*);
static unsigned long (*rvyuv_hive_message)(unsigned long,unsigned long);
static unsigned long (*rvyuv_init)(void*, void*); // initdata,context
static unsigned long (*rvyuv_transform)(char*, char*,transform_in_t*,unsigned int*,void*);
#ifdef USE_WIN32DLL
static unsigned long WINAPI (*wrvyuv_custom_message)(cmsg_data_t* ,void*);
static unsigned long WINAPI (*wrvyuv_free)(void*);
static unsigned long WINAPI (*wrvyuv_hive_message)(unsigned long,unsigned long);
static unsigned long WINAPI (*wrvyuv_init)(void*, void*); // initdata,context
static unsigned long WINAPI (*wrvyuv_transform)(char*, char*,transform_in_t*,unsigned int*,void*);
#endif

static void *rv_handle=NULL;
static int inited=0;
#ifdef USE_WIN32DLL
static int dll_type = 0; /* 0 = unix dlopen, 1 = win32 dll */
#endif

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

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
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
#ifdef HAVE_LIBDL
static int load_syms_linux(char *path) {
		void *handle;

		mp_msg(MSGT_DECVIDEO,MSGL_V, "opening shared obj '%s'\n", path);
		handle = dlopen (path, RTLD_LAZY);
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
       rvyuv_transform)
    {
	rv_handle = handle;
	return 1;
    }
	
		rvyuv_custom_message = dlsym(handle, "RV40toYUV420CustomMessage");
		rvyuv_free = dlsym(handle, "RV40toYUV420Free");
		rvyuv_hive_message = dlsym(handle, "RV40toYUV420HiveMessage");
		rvyuv_init = dlsym(handle, "RV40toYUV420Init");
		rvyuv_transform = dlsym(handle, "RV40toYUV420Transform");

    if(rvyuv_custom_message &&
       rvyuv_free &&
       rvyuv_hive_message &&
       rvyuv_init &&
       rvyuv_transform)
    {
	rv_handle = handle;
	return 1;
    }

    mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error resolving symbols! (version incompatibility?)\n");
    dlclose(handle);
    return 0;
}
#endif

#ifdef USE_WIN32DLL

#ifdef WIN32_LOADER
#include "loader/ldt_keeper.h"
#endif
void* WINAPI LoadLibraryA(char* name);
void* WINAPI GetProcAddress(void* handle,char* func);
int WINAPI FreeLibrary(void *handle);

static int load_syms_windows(char *path) {
    void *handle;

    mp_msg(MSGT_DECVIDEO,MSGL_V, "opening win32 dll '%s'\n", path);
#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
#endif
    handle = LoadLibraryA(path);
    mp_msg(MSGT_DECVIDEO,MSGL_V,"win32 real codec handle=%p  \n",handle);
    if (!handle) {
	mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error loading dll\n");
	return 0;
    }

    wrvyuv_custom_message = GetProcAddress(handle, "RV20toYUV420CustomMessage");
    wrvyuv_free = GetProcAddress(handle, "RV20toYUV420Free");
    wrvyuv_hive_message = GetProcAddress(handle, "RV20toYUV420HiveMessage");
    wrvyuv_init = GetProcAddress(handle, "RV20toYUV420Init");
    wrvyuv_transform = GetProcAddress(handle, "RV20toYUV420Transform");

    if(wrvyuv_custom_message &&
       wrvyuv_free &&
       wrvyuv_hive_message &&
       wrvyuv_init &&
       wrvyuv_transform)
    {
	dll_type = 1;
	rv_handle = handle;
	return 1;
    }
    mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error resolving symbols! (version incompatibility?)\n");
    FreeLibrary(handle);
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
	char *path;
	int result;
	// we export codec id and sub-id from demuxer in bitmapinfohdr:
	unsigned int* extrahdr=(unsigned int*)(sh->bih+1);
	struct rv_init_t init_data={
		11, sh->disp_w, sh->disp_h,0,0,extrahdr[0],
		1,extrahdr[1]}; // rv30

	mp_msg(MSGT_DECVIDEO,MSGL_V,"realvideo codec id: 0x%08X  sub-id: 0x%08X\n",extrahdr[1],extrahdr[0]);

	path = malloc(strlen(REALCODEC_PATH)+strlen(sh->codec->dll)+2);
	if (!path) return 0;
	sprintf(path, REALCODEC_PATH "/%s", sh->codec->dll);

	/* first try to load linux dlls, if failed and we're supporting win32 dlls,
	   then try to load the windows ones */
#ifdef HAVE_LIBDL       
	if(strstr(sh->codec->dll,".dll") || !load_syms_linux(path))
#endif
#ifdef USE_WIN32DLL
	    if (!load_syms_windows(sh->codec->dll))
#endif
	{
		mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingDLLcodec,sh->codec->dll);
		mp_msg(MSGT_DECVIDEO,MSGL_HINT,"Read the RealVideo section of the DOCS!\n");
		free(path);
		return 0;
	}
	free(path);
	// only I420 supported
//	if((sh->format!=0x30335652) && !mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_I420)) return 0;
	// init codec:
	sh->context=NULL;
#ifdef USE_WIN32DLL
	if (dll_type == 1)
	    result=(*wrvyuv_init)(&init_data, &sh->context);
	else
#endif
	result=(*rvyuv_init)(&init_data, &sh->context);
	if (result){
	    mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Couldn't open RealVideo codec, error code: 0x%X  \n",result);
	    return 0;
	}
	// setup rv30 codec (codec sub-type and image dimensions):
	if((sh->format<=0x30335652) && (extrahdr[1]>=0x20200002)){
	    // We could read nonsense data while filling this, but input is big enough so no sig11
	    uint32_t cmsg24[10]={sh->disp_w,sh->disp_h,((unsigned char *)extrahdr)[8]*4,((unsigned char *)extrahdr)[9]*4,
	                        ((unsigned char *)extrahdr)[10]*4,((unsigned char *)extrahdr)[11]*4,
	                        ((unsigned char *)extrahdr)[12]*4,((unsigned char *)extrahdr)[13]*4,
	                        ((unsigned char *)extrahdr)[14]*4,((unsigned char *)extrahdr)[15]*4};
	    cmsg_data_t cmsg_data={0x24,1+((extrahdr[0]>>16)&7), &cmsg24[0]};

#ifdef USE_WIN32DLL
	    if (dll_type == 1)
		(*wrvyuv_custom_message)(&cmsg_data,sh->context);
	    else
#endif
	    (*rvyuv_custom_message)(&cmsg_data,sh->context);
	}
	mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: RealVideo codec init OK!\n");
	return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
#ifdef USE_WIN32DLL
	if (dll_type == 1)
	{
	    if (wrvyuv_free) wrvyuv_free(sh->context);
	} else
#endif
	if(rvyuv_free) rvyuv_free(sh->context);

#ifdef USE_WIN32DLL
	if (dll_type == 1)
	{
	    if (rv_handle) FreeLibrary(rv_handle);
	} else
#endif
#ifdef HAVE_LIBDL
	if(rv_handle) dlclose(rv_handle);
#endif
	rv_handle=NULL;
	inited = 0;
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
	unsigned char* buffer;

	unsigned int transform_out[5];
	transform_in_t transform_in={
		dp_hdr->len,	// length of the packet (sub-packets appended)
		0,		// unknown, seems to be unused
		dp_hdr->chunks,	// number of sub-packets - 1
		extra,		// table of sub-packet offsets
		0,		// unknown, seems to be unused
		dp_hdr->timestamp,// timestamp (the integer value from the stream)
	};

	if(len<=0 || flags&2) return NULL; // skipped frame || hardframedrop

	if(inited){  // rv30 width/height not yet known
	mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/,
		sh->disp_w, sh->disp_h);
	if(!mpi) return NULL;
	    buffer=mpi->planes[0];
	} else {
	    buffer=malloc(sh->disp_w*sh->disp_h*3/2);
	    if (!buffer) return 0;
	}
	
#ifdef USE_WIN32DLL
	if (dll_type == 1)
	    result=(*wrvyuv_transform)(dp_data, buffer, &transform_in,
		transform_out, sh->context);
	else
#endif
	result=(*rvyuv_transform)(dp_data, buffer, &transform_in,
		transform_out, sh->context);

	if(!inited){  // rv30 width/height now known
	    sh->aspect=(float)sh->disp_w/(float)sh->disp_h;
	    sh->disp_w=transform_out[3];
	    sh->disp_h=transform_out[4];
	    if (!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_I420)) return 0;
	    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/,
		    sh->disp_w, sh->disp_h);
	    if(!mpi) return NULL;
	    memcpy(mpi->planes[0],buffer,sh->disp_w*sh->disp_h*3/2);
	    free(buffer);
	    inited=1;
	} 

	return (result?NULL:mpi);
}
