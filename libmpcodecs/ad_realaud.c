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
#include <unistd.h>

#include "config.h"

//#include <stddef.h>
#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif
#include "help_mp.h"
#include "path.h"

#include "ad_internal.h"
#include "loader/wine/windef.h"

static const ad_info_t info = {
	"RealAudio decoder",
	"realaud",
	"Alex Beregszaszi",
	"Florian Schneider, Arpad Gereoffy, Alex Beregszaszi, Donnie Smith",
	"binary real audio codecs"
};

LIBAD_EXTERN(realaud)

/* These functions are required for loading Real binary libs.
 * Add forward declarations to avoid warnings with -Wmissing-prototypes. */
void *__builtin_new(unsigned long size);
void  __builtin_delete(void *ize);
void *__builtin_vec_new(unsigned long size);
void  __builtin_vec_delete(void *mem);
void  __pure_virtual(void);

void *__builtin_new(unsigned long size)
{
	return malloc(size);
}

void __builtin_delete(void* ize)
{
	free(ize);
}

void *__builtin_vec_new(unsigned long size)
{
	return malloc(size);
}

void __builtin_vec_delete(void *mem)
{
	free(mem);
}

void __pure_virtual(void)
{
	printf("FATAL: __pure_virtual() called!\n");
//	exit(1);
}

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
void ___brk_addr(void);
void ___brk_addr(void) {exit(0);}
char **__environ={NULL};
#undef stderr
FILE *stderr=NULL;
void *__ctype_b=NULL;
#endif

static unsigned long (*raCloseCodec)(void*);
static unsigned long (*raDecode)(void*, char*,unsigned long,char*,unsigned int*,long);
static unsigned long (*raFreeDecoder)(void*);
//static unsigned long (*raGetNumberOfFlavors2)(void);
static unsigned long (*raInitDecoder)(void*, void*);
static unsigned long (*raOpenCodec)(void*);
static unsigned long (*raOpenCodec2)(void*, void*);
static unsigned long (*raSetFlavor)(void*,unsigned long);
static void  (*raSetDLLAccessPath)(char*);
static void  (*raSetPwd)(char*,char*);
#ifdef CONFIG_WIN32DLL
static unsigned long WINAPI (*wraCloseCodec)(void*);
static unsigned long WINAPI (*wraDecode)(void*, char*,unsigned long,char*,unsigned int*,long);
static unsigned long WINAPI (*wraFreeDecoder)(void*);
static unsigned long WINAPI (*wraInitDecoder)(void*, void*);
static unsigned long WINAPI (*wraOpenCodec)(void*);
static unsigned long WINAPI (*wraOpenCodec2)(void*, void*);
static unsigned long WINAPI (*wraSetFlavor)(void*,unsigned long);
static void          WINAPI (*wraSetDLLAccessPath)(char*);
static void          WINAPI (*wraSetPwd)(char*,char*);

static int dll_type = 0; /* 0 = unix dlopen, 1 = win32 dll */
#endif

static void *rv_handle = NULL;

#if 0
typedef struct {
    int samplerate;
    short bits;
    short channels;
    int unk1;
    int unk2;
    int packetsize;
    int unk3;
    void* unk4;
} ra_init_t ;
#else

/*
 Probably the linux .so-s were compiled with old GCC without setting
 packing, so it adds 2 bytes padding after the quality field.
 In windows it seems that there's no padding in it.

 -- alex
*/

/* linux dlls doesn't need packing */
typedef struct /*__attribute__((__packed__))*/ {
    int samplerate;
    short bits;
    short channels;
    short quality;
    /* 2bytes padding here, by gcc */
    int bits_per_frame;
    int packetsize;
    int extradata_len;
    void* extradata;
} ra_init_t;

/* windows dlls need packed structs (no padding) */
typedef struct __attribute__((__packed__)) {
    int samplerate;
    short bits;
    short channels;
    short quality;
    int bits_per_frame;
    int packetsize;
    int extradata_len;
    void* extradata;
} wra_init_t;
#endif

#ifdef HAVE_LIBDL
static int load_syms_linux(char *path)
{
    void *handle;

    mp_msg(MSGT_DECVIDEO, MSGL_V, "opening shared obj '%s'\n", path);
    handle = dlopen(path, RTLD_LAZY);
    if (!handle)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_WARN, "Error: %s\n", dlerror());
	return 0;
    }

    raCloseCodec = dlsym(handle, "RACloseCodec");
    raDecode = dlsym(handle, "RADecode");
    raFreeDecoder = dlsym(handle, "RAFreeDecoder");
    raOpenCodec = dlsym(handle, "RAOpenCodec");
    raOpenCodec2 = dlsym(handle, "RAOpenCodec2");
    raInitDecoder = dlsym(handle, "RAInitDecoder");
    raSetFlavor = dlsym(handle, "RASetFlavor");
    raSetDLLAccessPath = dlsym(handle, "SetDLLAccessPath");
    raSetPwd = dlsym(handle, "RASetPwd"); // optional, used by SIPR

    if (raCloseCodec && raDecode && raFreeDecoder &&
	(raOpenCodec||raOpenCodec2) && raSetFlavor &&
	/*raSetDLLAccessPath &&*/ raInitDecoder)
    {
	rv_handle = handle;
	return 1;
    }

    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Cannot resolve symbols - incompatible dll: %s\n",path);
    dlclose(handle);
    return 0;
}
#endif

#ifdef CONFIG_WIN32DLL

#ifdef WIN32_LOADER
#include "loader/ldt_keeper.h"
#endif
void* WINAPI LoadLibraryA(char* name);
void* WINAPI GetProcAddress(void* handle,char *func);
int WINAPI FreeLibrary(void *handle);

static int load_syms_windows(char *path)
{
    void *handle;

    mp_msg(MSGT_DECVIDEO, MSGL_V, "opening win32 dll '%s'\n", path);
#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
#endif
    handle = LoadLibraryA(path);
    if (!handle)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_WARN, "Error loading dll\n");
	return 0;
    }

    wraCloseCodec = GetProcAddress(handle, "RACloseCodec");
    wraDecode = GetProcAddress(handle, "RADecode");
    wraFreeDecoder = GetProcAddress(handle, "RAFreeDecoder");
    wraOpenCodec = GetProcAddress(handle, "RAOpenCodec");
    wraOpenCodec2 = GetProcAddress(handle, "RAOpenCodec2");
    wraInitDecoder = GetProcAddress(handle, "RAInitDecoder");
    wraSetFlavor = GetProcAddress(handle, "RASetFlavor");
    wraSetDLLAccessPath = GetProcAddress(handle, "SetDLLAccessPath");
    wraSetPwd = GetProcAddress(handle, "RASetPwd"); // optional, used by SIPR

    if (wraCloseCodec && wraDecode && wraFreeDecoder &&
	(wraOpenCodec || wraOpenCodec2) && wraSetFlavor &&
	/*wraSetDLLAccessPath &&*/ wraInitDecoder)
    {
	rv_handle = handle;
	dll_type = 1;
	return 1;
    }

    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Cannot resolve symbols - incompatible dll: %s\n",path);
    FreeLibrary(handle);
    return 0;

}
#endif


static int preinit(sh_audio_t *sh){
  // let's check if the driver is available, return 0 if not.
  // (you should do that if you use external lib(s) which is optional)
  unsigned int result;
  char *path;

  path = malloc(strlen(codec_path) + strlen(sh->codec->dll) + 2);
  if (!path) return 0;
  sprintf(path, "%s/%s", codec_path, sh->codec->dll);

    /* first try to load linux dlls, if failed and we're supporting win32 dlls,
       then try to load the windows ones */

#ifdef HAVE_LIBDL
    if (strstr(sh->codec->dll,".dll") || !load_syms_linux(path))
#endif
#ifdef CONFIG_WIN32DLL
	if (!load_syms_windows(sh->codec->dll))
#endif
    {
	mp_msg(MSGT_DECVIDEO, MSGL_ERR, MSGTR_MissingDLLcodec, sh->codec->dll);
	mp_msg(MSGT_DECVIDEO, MSGL_HINT, "Read the RealAudio section of the DOCS!\n");
	free(path);
	return 0;
    }

#ifdef CONFIG_WIN32DLL
  if((raSetDLLAccessPath && dll_type == 0) || (wraSetDLLAccessPath && dll_type == 1)){
#else
  if(raSetDLLAccessPath){
#endif
      // used by 'SIPR'
      path = realloc(path, strlen(codec_path) + 13);
      sprintf(path, "DT_Codecs=%s", codec_path);
      if(path[strlen(path)-1]!='/'){
        path[strlen(path)+1]=0;
        path[strlen(path)]='/';
      }
      path[strlen(path)+1]=0;
#ifdef CONFIG_WIN32DLL
    if (dll_type == 1)
    {
      int i;
      for (i=0; i < strlen(path); i++)
        if (path[i] == '/') path[i] = '\\';
      wraSetDLLAccessPath(path);
    }
    else
#endif
      raSetDLLAccessPath(path);
  }

#ifdef CONFIG_WIN32DLL
    if (dll_type == 1){
      if (wraOpenCodec2) {
        sprintf(path, "%s\\", codec_path);
        result = wraOpenCodec2(&sh->context, path);
      } else
	result=wraOpenCodec(&sh->context);
    } else
#endif
    if (raOpenCodec2) {
      sprintf(path, "%s/", codec_path);
      result = raOpenCodec2(&sh->context, path);
    } else
      result=raOpenCodec(&sh->context);
    if(result){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Decoder open failed, error code: 0x%X\n",result);
      return 0;
    }
//    printf("opencodec ok (result: %x)\n", result);
  free(path); /* after this it isn't used anymore */

  sh->samplerate=sh->wf->nSamplesPerSec;
  sh->samplesize=sh->wf->wBitsPerSample/8;
  sh->channels=sh->wf->nChannels;

  {
    ra_init_t init_data={
	sh->wf->nSamplesPerSec,
	sh->wf->wBitsPerSample,
	sh->wf->nChannels,
	100, // quality
	sh->wf->nBlockAlign, // subpacket size
	sh->wf->nBlockAlign, // coded frame size
	sh->wf->cbSize, // codec data length
	(char*)(sh->wf+1) // extras
    };
#ifdef CONFIG_WIN32DLL
    wra_init_t winit_data={
	sh->wf->nSamplesPerSec,
	sh->wf->wBitsPerSample,
	sh->wf->nChannels,
	100, // quality
	sh->wf->nBlockAlign, // subpacket size
	sh->wf->nBlockAlign, // coded frame size
	sh->wf->cbSize, // codec data length
	(char*)(sh->wf+1) // extras
    };
#endif
#ifdef CONFIG_WIN32DLL
    if (dll_type == 1)
	result=wraInitDecoder(sh->context,&winit_data);
    else
#endif
    result=raInitDecoder(sh->context,&init_data);

    if(result){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Decoder init failed, error code: 0x%X\n",result);
      return 0;
    }
//    printf("initdecoder ok (result: %x)\n", result);
  }

#ifdef CONFIG_WIN32DLL
    if((raSetPwd && dll_type == 0) || (wraSetPwd && dll_type == 1)){
#else
    if(raSetPwd){
#endif
	// used by 'SIPR'
#ifdef CONFIG_WIN32DLL
	if (dll_type == 1)
	    wraSetPwd(sh->context,"Ardubancel Quazanga");
	else
#endif
	raSetPwd(sh->context,"Ardubancel Quazanga"); // set password... lol.
    }

  if (sh->format == mmioFOURCC('s','i','p','r')) {
    short flavor;

    if (sh->wf->nAvgBytesPerSec > 1531)
        flavor = 3;
    else if (sh->wf->nAvgBytesPerSec > 937)
        flavor = 1;
    else if (sh->wf->nAvgBytesPerSec > 719)
        flavor = 0;
    else
        flavor = 2;
    mp_msg(MSGT_DECAUDIO,MSGL_V,"Got sipr flavor %d from bitrate %d\n",flavor, sh->wf->nAvgBytesPerSec);

#ifdef CONFIG_WIN32DLL
    if (dll_type == 1)
	result=wraSetFlavor(sh->context,flavor);
    else
#endif
    result=raSetFlavor(sh->context,flavor);
    if(result){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Decoder flavor setup failed, error code: 0x%X\n",result);
      return 0;
    }
  } // sipr flavor

    sh->i_bps=sh->wf->nAvgBytesPerSec;

  sh->audio_out_minsize=128000; // no idea how to get... :(
  sh->audio_in_minsize = sh->wf->nBlockAlign;

  return 1; // return values: 1=OK 0=ERROR
}

static int init(sh_audio_t *sh_audio){
  // initialize the decoder, set tables etc...

  // you can store HANDLE or private struct pointer at sh->context
  // you can access WAVEFORMATEX header at sh->wf

  // set sample format/rate parameters if you didn't do it in preinit() yet.

  return 1; // return values: 1=OK 0=ERROR
}

static void uninit(sh_audio_t *sh){
  // uninit the decoder etc...
  // again: you don't have to free() a_in_buffer here! it's done by the core.
#ifdef CONFIG_WIN32DLL
    if (dll_type == 1)
    {
	if (wraFreeDecoder) wraFreeDecoder(sh->context);
	if (wraCloseCodec) wraCloseCodec(sh->context);
    }
#endif

    if (raFreeDecoder) raFreeDecoder(sh->context);
    if (raCloseCodec) raCloseCodec(sh->context);


#ifdef CONFIG_WIN32DLL
    if (dll_type == 1)
    {
	if (rv_handle) FreeLibrary(rv_handle);
    } else
#endif
// this dlclose() causes some memory corruption, and crashes soon (in caller):
//    if (rv_handle) dlclose(rv_handle);
    rv_handle = NULL;
}

static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen){
  int result;
  int len=-1;

  if(sh->a_in_buffer_len<=0){
      // fill the buffer!
      if (sh->ds->eof)
          return 0;
      demux_read_data(sh->ds, sh->a_in_buffer, sh->wf->nBlockAlign);
      sh->a_in_buffer_size=
      sh->a_in_buffer_len=sh->wf->nBlockAlign;
  }

#ifdef CONFIG_WIN32DLL
    if (dll_type == 1)
      result=wraDecode(sh->context, sh->a_in_buffer+sh->a_in_buffer_size-sh->a_in_buffer_len, sh->wf->nBlockAlign,
       buf, &len, -1);
    else
#endif
  result=raDecode(sh->context, sh->a_in_buffer+sh->a_in_buffer_size-sh->a_in_buffer_len, sh->wf->nBlockAlign,
       buf, &len, -1);
  sh->a_in_buffer_len-=sh->wf->nBlockAlign;

//  printf("radecode: %d bytes, res=0x%X  \n",len,result);

  return len; // return value: number of _bytes_ written to output buffer,
              // or -1 for EOF (or uncorrectable error)
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...){
    // various optional functions you MAY implement:
    switch(cmd){
      case ADCTRL_RESYNC_STREAM:
        // it is called once after seeking, to resync.
	// Note: sh_audio->a_in_buffer_len=0; is done _before_ this call!
	return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
        // it is called to skip (jump over) small amount (1/10 sec or 1 frame)
	// of audio data - used to sync audio to video after seeking
	// if you don't return CONTROL_TRUE, it will defaults to:
	//      ds_fill_buffer(sh_audio->ds);  // skip 1 demux packet
	return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}
