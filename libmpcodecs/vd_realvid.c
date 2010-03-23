/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif

#include "mp_msg.h"
#include "help_mp.h"
#include "mpbswap.h"
#include "path.h"

#include "vd_internal.h"
#include "loader/wine/windef.h"

static const vd_info_t info = {
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
static unsigned long (*rvyuv_init)(void*, void*); // initdata,context
static unsigned long (*rvyuv_transform)(char*, char*,transform_in_t*,unsigned int*,void*);
#ifdef CONFIG_WIN32DLL
static unsigned long WINAPI (*wrvyuv_custom_message)(cmsg_data_t* ,void*);
static unsigned long WINAPI (*wrvyuv_free)(void*);
static unsigned long WINAPI (*wrvyuv_init)(void*, void*); // initdata,context
static unsigned long WINAPI (*wrvyuv_transform)(char*, char*,transform_in_t*,unsigned int*,void*);
#endif

static void *rv_handle=NULL;
static int initialized=0;
static uint8_t *buffer = NULL;
static int bufsz = 0;
#ifdef CONFIG_WIN32DLL
static int dll_type = 0; /* 0 = unix dlopen, 1 = win32 dll */
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
		rvyuv_init = dlsym(handle, "RV20toYUV420Init");
		rvyuv_transform = dlsym(handle, "RV20toYUV420Transform");

    if(rvyuv_custom_message &&
       rvyuv_free &&
       rvyuv_init &&
       rvyuv_transform)
    {
	rv_handle = handle;
	return 1;
    }

		rvyuv_custom_message = dlsym(handle, "RV40toYUV420CustomMessage");
		rvyuv_free = dlsym(handle, "RV40toYUV420Free");
		rvyuv_init = dlsym(handle, "RV40toYUV420Init");
		rvyuv_transform = dlsym(handle, "RV40toYUV420Transform");

    if(rvyuv_custom_message &&
       rvyuv_free &&
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

#ifdef CONFIG_WIN32DLL

#ifdef WIN32_LOADER
#include "loader/ldt_keeper.h"
#endif
void* WINAPI LoadLibraryA(char* name);
void* WINAPI GetProcAddress(void* handle,char* func);
int WINAPI FreeLibrary(void *handle);

#ifndef WIN32_LOADER
void * WINAPI GetModuleHandleA(char *);
static int patch_dll(uint8_t *patchpos, const uint8_t *oldcode,
                     const uint8_t *newcode, int codesize) {
  void *handle = GetModuleHandleA("kernel32");
  int WINAPI (*VirtProt)(void *, unsigned, int, int *);
  int res = 0;
  int prot, tmp;
  VirtProt = GetProcAddress(handle, "VirtualProtect");
  // change permissions to PAGE_WRITECOPY
  if (!VirtProt ||
      !VirtProt(patchpos, codesize, 0x08, &prot)) {
    mp_msg(MSGT_DECVIDEO, MSGL_WARN, "VirtualProtect failed at %p\n", patchpos);
    return 0;
  }
  if (memcmp(patchpos, oldcode, codesize) == 0) {
    memcpy(patchpos, newcode, codesize);
    res = 1;
  }
  VirtProt(patchpos, codesize, prot, &tmp);
  return res;
}
#endif

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
    wrvyuv_init = GetProcAddress(handle, "RV20toYUV420Init");
    wrvyuv_transform = GetProcAddress(handle, "RV20toYUV420Transform");

    if(wrvyuv_custom_message &&
       wrvyuv_free &&
       wrvyuv_init &&
       wrvyuv_transform)
    {
	dll_type = 1;
	rv_handle = handle;
#ifndef WIN32_LOADER
	{
	    if (strstr(path, "drv43260.dll")) {
		int patched;
		// patch away multithreaded decoding, it causes crashes
		static const uint8_t oldcode[13] = {
		    0x83, 0xbb, 0xf8, 0x05, 0x00, 0x00, 0x01,
		    0x0f, 0x86, 0xd0, 0x00, 0x00, 0x00 };
		static const uint8_t newcode[13] = {
		    0x31, 0xc0,
		    0x89, 0x83, 0xf8, 0x05, 0x00, 0x00,
		    0xe9, 0xd0, 0x00, 0x00, 0x00 };
		patched = patch_dll(
			(char*)wrvyuv_transform + 0x634132fa - 0x634114d0,
			oldcode, newcode, sizeof(oldcode));
		if (!patched)
		    mp_msg(MSGT_DECVIDEO, MSGL_WARN, "Could not patch Real codec, this might crash on multi-CPU systems\n");
	    }
	}
#endif
	return 1;
    }

    wrvyuv_custom_message = GetProcAddress(handle, "RV40toYUV420CustomMessage");
    wrvyuv_free = GetProcAddress(handle, "RV40toYUV420Free");
    wrvyuv_init = GetProcAddress(handle, "RV40toYUV420Init");
    wrvyuv_transform = GetProcAddress(handle, "RV40toYUV420Transform");
    if(wrvyuv_custom_message &&
       wrvyuv_free &&
       wrvyuv_init &&
       wrvyuv_transform) {
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
	unsigned char* extrahdr=(unsigned char*)(sh->bih+1);
	unsigned int extrahdr_size = sh->bih->biSize - sizeof(BITMAPINFOHEADER);
	struct rv_init_t init_data;

	if(extrahdr_size < 8) {
	    mp_msg(MSGT_DECVIDEO,MSGL_ERR,"realvideo: extradata too small (%u)\n", sh->bih->biSize - sizeof(BITMAPINFOHEADER));
	    return 0;
	}
	init_data = (struct rv_init_t){11, sh->disp_w, sh->disp_h, 0, 0, be2me_32(((unsigned int*)extrahdr)[0]), 1, be2me_32(((unsigned int*)extrahdr)[1])}; // rv30

	mp_msg(MSGT_DECVIDEO,MSGL_V,"realvideo codec id: 0x%08X  sub-id: 0x%08X\n",be2me_32(((unsigned int*)extrahdr)[1]),be2me_32(((unsigned int*)extrahdr)[0]));

	path = malloc(strlen(codec_path) + strlen(sh->codec->dll) + 2);
	if (!path) return 0;
	sprintf(path, "%s/%s", codec_path, sh->codec->dll);

	/* first try to load linux dlls, if failed and we're supporting win32 dlls,
	   then try to load the windows ones */
#ifdef HAVE_LIBDL
	if(strstr(sh->codec->dll,".dll") || !load_syms_linux(path))
#endif
#ifdef CONFIG_WIN32DLL
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
#ifdef CONFIG_WIN32DLL
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
	if((sh->format<=0x30335652) && (be2me_32(((unsigned int*)extrahdr)[1])>=0x20200002)){
	    int i, cmsg_cnt;
	    uint32_t cmsg24[16]={sh->disp_w,sh->disp_h};
	    cmsg_data_t cmsg_data={0x24,1+(extrahdr[1]&7), &cmsg24[0]};

	    mp_msg(MSGT_DECVIDEO,MSGL_V,"realvideo: using cmsg24 with %u elements.\n",extrahdr[1]&7);
	    cmsg_cnt = (extrahdr[1]&7)*2;
	    if (extrahdr_size-8 < cmsg_cnt) {
	        mp_msg(MSGT_DECVIDEO,MSGL_WARN,"realvideo: not enough extradata (%u) to make %u cmsg24 elements.\n",extrahdr_size-8,extrahdr[1]&7);
	        cmsg_cnt = extrahdr_size-8;
	    }
	    for (i = 0; i < cmsg_cnt; i++)
	        cmsg24[2+i] = extrahdr[8+i]*4;
	    if (extrahdr_size-8 > cmsg_cnt)
	        mp_msg(MSGT_DECVIDEO,MSGL_WARN,"realvideo: %u bytes of unknown extradata remaining.\n",extrahdr_size-8-cmsg_cnt);

#ifdef CONFIG_WIN32DLL
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
#ifdef CONFIG_WIN32DLL
	if (dll_type == 1)
	{
	    if (wrvyuv_free) wrvyuv_free(sh->context);
	} else
#endif
	if(rvyuv_free) rvyuv_free(sh->context);

#ifdef CONFIG_WIN32DLL
	if (dll_type == 1)
	{
	    if (rv_handle) FreeLibrary(rv_handle);
	} else
#endif
#ifdef HAVE_LIBDL
	if(rv_handle) dlclose(rv_handle);
#endif
	rv_handle=NULL;
	initialized = 0;
	if (buffer)
	    free(buffer);
	buffer = NULL;
	bufsz = 0;
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
	mp_image_t* mpi;
	unsigned long result;
	uint8_t *buf = data;
	int chunks = *buf++;
	int extra_size = 8*(chunks+1);
	uint32_t data_size = len-1-extra_size;
	unsigned char* dp_data=buf+extra_size;
	uint32_t* extra=(uint32_t*)buf;
	int i;

	unsigned int transform_out[5];
	transform_in_t transform_in={
		data_size,	// length of the packet (sub-packets appended)
		0,		// unknown, seems to be unused
		chunks,	// number of sub-packets - 1
		extra,		// table of sub-packet offsets
		0,		// unknown, seems to be unused
		0,		// timestamp (should be unneded)
	};

	if(len<=0 || flags&2) return NULL; // skipped frame || hardframedrop

	if (bufsz < sh->disp_w*sh->disp_h*3/2) {
	    if (buffer) free(buffer);
	    bufsz = sh->disp_w*sh->disp_h*3/2;
	    buffer=malloc(bufsz);
	    if (!buffer) return 0;
	}

	for (i=0; i<2*(chunks+1); i++)
		extra[i] = le2me_32(extra[i]);

#ifdef CONFIG_WIN32DLL
	if (dll_type == 1)
	    result=(*wrvyuv_transform)(dp_data, buffer, &transform_in,
		transform_out, sh->context);
	else
#endif
	result=(*rvyuv_transform)(dp_data, buffer, &transform_in,
		transform_out, sh->context);

	if(!initialized){  // rv30 width/height now known
	    sh->aspect=(float)sh->disp_w/(float)sh->disp_h;
	    sh->disp_w=transform_out[3];
	    sh->disp_h=transform_out[4];
	    if (!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_I420)) return 0;
	    initialized=1;
	}
	    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/,
		    sh->disp_w, sh->disp_h);
	    if(!mpi) return NULL;
	    mpi->planes[0] = buffer;
	    mpi->stride[0] = sh->disp_w;
	    mpi->planes[1] = buffer + sh->disp_w*sh->disp_h;
	    mpi->stride[1] = sh->disp_w / 2;
	    mpi->planes[2] = buffer + sh->disp_w*sh->disp_h*5/4;
	    mpi->stride[2] = sh->disp_w / 2;

	if(transform_out[0] &&
	   (sh->disp_w != transform_out[3] || sh->disp_h != transform_out[4]))
	    initialized = 0;

	return result ? NULL : mpi;
}
