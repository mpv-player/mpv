
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

#ifdef USE_REALCODECS

//#include <stddef.h>
#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif
#include "help_mp.h"

#include "ad_internal.h"
#include "wine/windef.h"

#ifdef USE_MACSHLB
#include <CoreServices/CoreServices.h>
#endif

static ad_info_t info =  {
	"RealAudio decoder",
	"realaud",
	"Alex Beregszaszi",
	"Florian Schneider, Arpad Gereoffy, Alex Beregszaszi, Donnie Smith",
	"binary real audio codecs"
};

LIBAD_EXTERN(realaud)

void *__builtin_new(unsigned long size) {
	return malloc(size);
}

// required for cook's uninit:
void __builtin_delete(void* ize) {
	free(ize);
}

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
void *__ctype_b=NULL;
#endif

static unsigned long (*raCloseCodec)(void*);
static unsigned long (*raDecode)(void*, char*,unsigned long,char*,unsigned int*,long);
static unsigned long (*raFlush)(unsigned long,unsigned long,unsigned long);
static unsigned long (*raFreeDecoder)(void*);
static void*         (*raGetFlavorProperty)(void*,unsigned long,unsigned long,int*);
//static unsigned long (*raGetNumberOfFlavors2)(void);
static unsigned long (*raInitDecoder)(void*, void*);
static unsigned long (*raOpenCodec)(void*);
static unsigned long (*raOpenCodec2)(void*, void*);
static unsigned long (*raSetFlavor)(void*,unsigned long);
static void  (*raSetDLLAccessPath)(char*);
static void  (*raSetPwd)(char*,char*);
#ifdef USE_WIN32DLL
static unsigned long WINAPI (*wraCloseCodec)(void*);
static unsigned long WINAPI (*wraDecode)(void*, char*,unsigned long,char*,unsigned int*,long);
static unsigned long WINAPI (*wraFlush)(unsigned long,unsigned long,unsigned long);
static unsigned long WINAPI (*wraFreeDecoder)(void*);
static void*         WINAPI (*wraGetFlavorProperty)(void*,unsigned long,unsigned long,int*);
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

    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "opening shared obj '%s'\n", path);
    handle = dlopen(path, RTLD_LAZY);
    if (!handle)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_WARN, "Error: %s\n", dlerror());
	return 0;
    }

    raCloseCodec = dlsym(handle, "RACloseCodec");
    raDecode = dlsym(handle, "RADecode");
    raFlush = dlsym(handle, "RAFlush");
    raFreeDecoder = dlsym(handle, "RAFreeDecoder");
    raGetFlavorProperty = dlsym(handle, "RAGetFlavorProperty");
    raOpenCodec = dlsym(handle, "RAOpenCodec");
    raOpenCodec2 = dlsym(handle, "RAOpenCodec2");
    raInitDecoder = dlsym(handle, "RAInitDecoder");
    raSetFlavor = dlsym(handle, "RASetFlavor");
    raSetDLLAccessPath = dlsym(handle, "SetDLLAccessPath");
    raSetPwd = dlsym(handle, "RASetPwd"); // optional, used by SIPR
    
    if (raCloseCodec && raDecode && /*raFlush && */raFreeDecoder &&
	raGetFlavorProperty && (raOpenCodec||raOpenCodec2) && raSetFlavor &&
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

#ifdef USE_WIN32DLL

#ifdef WIN32_LOADER
#include "../loader/ldt_keeper.h"
#endif
void* WINAPI LoadLibraryA(char* name);
void* WINAPI GetProcAddress(void* handle,char *func);
int WINAPI FreeLibrary(void *handle);

static int load_syms_windows(char *path)
{
    void *handle;
    
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "opening win32 dll '%s'\n", path);
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
    wraFlush = GetProcAddress(handle, "RAFlush");
    wraFreeDecoder = GetProcAddress(handle, "RAFreeDecoder");
    wraGetFlavorProperty = GetProcAddress(handle, "RAGetFlavorProperty");
    wraOpenCodec = GetProcAddress(handle, "RAOpenCodec");
    wraOpenCodec2 = GetProcAddress(handle, "RAOpenCodec2");
    wraInitDecoder = GetProcAddress(handle, "RAInitDecoder");
    wraSetFlavor = GetProcAddress(handle, "RASetFlavor");
    wraSetDLLAccessPath = GetProcAddress(handle, "SetDLLAccessPath");
    wraSetPwd = GetProcAddress(handle, "RASetPwd"); // optional, used by SIPR
    
    if (wraCloseCodec && wraDecode && /*wraFlush && */wraFreeDecoder &&
	wraGetFlavorProperty && (wraOpenCodec || wraOpenCodec2) && wraSetFlavor &&
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


#ifdef USE_MACSHLB
/*
 Helper function to create a function pointer (from a null terminated (!)
 pascal string) like GetProcAddress(). Some assembler is required due
 to different calling conventions, for further details, see 
 http://developer.apple.com/ samplecode/CFM_MachO_CFM/listing1.html .

 Caller is expected to DisposePtr(mfp).
 N.B.: Code is used by vd_realaud.c as well.
*/
void *load_one_sym_mac(char *symbolName, CFragConnectionID *connID) {
    OSErr err;
    Ptr symbolAddr;
    CFragSymbolClass symbolClass;
    UInt32  *mfp;
    char realname[255];
    
    if (strlen(symbolName) > 255)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_V, "FindSymbol symbolname overflow\n");
	return NULL;
    }
    
    snprintf(realname, 255, "%c%s", strlen(symbolName), symbolName);

    if ( (err = FindSymbol( *connID, realname, 
                            &symbolAddr, &symbolClass )) != noErr ) {
        mp_msg(MSGT_DECVIDEO,MSGL_V,"FindSymbol( \"%s\" ) failed with error code %d.\n", symbolName + 1, err );
        return NULL;
    }

    if ( (mfp = (UInt32 *)NewPtr( 6 * sizeof(UInt32) )) == nil )
        return NULL;

    mfp[0] = 0x3D800000 | ((UInt32)symbolAddr >> 16);
    mfp[1] = 0x618C0000 | ((UInt32)symbolAddr & 0xFFFF);
    mfp[2] = 0x800C0000;
    mfp[3] = 0x804C0004;
    mfp[4] = 0x7C0903A6;
    mfp[5] = 0x4E800420;
    MakeDataExecutable( mfp, 6 * sizeof(UInt32) );

    return( mfp );
}

static int load_syms_mac(char *path)
{
    Ptr mainAddr;
    OSStatus status;
    FSRef fsref;
    FSSpec fsspec;
    OSErr err;
    Str255 errMessage;
    CFragConnectionID *connID;

    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "opening mac shlb '%s'\n", path);

    if ( (connID = (CFragConnectionID *)NewPtr( sizeof( CFragConnectionID ))) == nil ) {
        mp_msg(MSGT_DECVIDEO,MSGL_WARN,"NewPtr() failed.\n" );
        return 0;
    }

    if ( (status = FSPathMakeRef( path, &fsref, NULL )) != noErr ) {
        mp_msg(MSGT_DECVIDEO,MSGL_WARN,"FSPathMakeRef() failed with error %d.\n", status );
        return 0;
    }

    if ( (status = FSGetCatalogInfo( &fsref, kFSCatInfoNone, NULL, NULL, &fsspec, NULL )) != noErr ) {
        mp_msg(MSGT_DECVIDEO,MSGL_WARN,"FSGetCatalogInfo() failed with error %d.\n", status );
        return 0;
    }

    if ( (err = GetDiskFragment( &fsspec, 0, kCFragGoesToEOF, NULL, kPrivateCFragCopy, connID, &mainAddr, errMessage )) != noErr ) {

        p2cstrcpy( errMessage, errMessage );
        mp_msg(MSGT_DECVIDEO,MSGL_WARN,"GetDiskFragment() failed with error %d: %s\n", err, errMessage );
        return 0;
    }

    raCloseCodec = load_one_sym_mac( "RACloseCodec", connID);
    raDecode = load_one_sym_mac("RADecode", connID);
    raFlush = load_one_sym_mac("RAFlush", connID);
    raFreeDecoder = load_one_sym_mac("RAFreeDecoder", connID);
    raGetFlavorProperty = load_one_sym_mac("RAGetFlavorProperty", connID);
    raOpenCodec = load_one_sym_mac("RAOpenCodec", connID);
    raOpenCodec2 = load_one_sym_mac("RAOpenCodec2", connID);
    raInitDecoder = load_one_sym_mac("RAInitDecoder", connID);
    raSetFlavor = load_one_sym_mac("RASetFlavor", connID);
    raSetDLLAccessPath = load_one_sym_mac("SetDLLAccessPath", connID);
    raSetPwd = load_one_sym_mac("RASetPwd", connID); // optional, used by SIPR

    if (raCloseCodec && raDecode && /*raFlush && */raFreeDecoder &&
    raGetFlavorProperty && (raOpenCodec || raOpenCodec2) && raSetFlavor &&
    /*raSetDLLAccessPath &&*/ raInitDecoder)
    {
    rv_handle = connID;
    return 1;
    }

    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Cannot resolve symbols - incompatible shlb: %s\n",path);
    (void)CloseConnection(connID);
    return 0;
}

#endif

static int preinit(sh_audio_t *sh){
  // let's check if the driver is available, return 0 if not.
  // (you should do that if you use external lib(s) which is optional)
  unsigned int result;
  int len=0;
  void* prop;
  char *path;

  path = malloc(strlen(REALCODEC_PATH)+strlen(sh->codec->dll)+2);
  if (!path) return 0;
  sprintf(path, REALCODEC_PATH "/%s", sh->codec->dll);

    /* first try to load linux dlls, if failed and we're supporting win32 dlls,
       then try to load the windows ones */
      
#ifdef USE_MACSHLB
    if (strstr(sh->codec->dll,".shlb") && !load_syms_mac(path))
#endif
#ifdef HAVE_LIBDL       
    if (strstr(sh->codec->dll,".dll") || !load_syms_linux(path))
#endif
#ifdef USE_WIN32DLL
	if (!load_syms_windows(sh->codec->dll))
#endif
    {
	mp_msg(MSGT_DECVIDEO, MSGL_ERR, MSGTR_MissingDLLcodec, sh->codec->dll);
	mp_msg(MSGT_DECVIDEO, MSGL_HINT, "Read the RealAudio section of the DOCS!\n");
	free(path);
	return 0;
    }

#ifdef USE_WIN32DLL
  if((raSetDLLAccessPath && dll_type == 0) || (wraSetDLLAccessPath && dll_type == 1)){
#else
  if(raSetDLLAccessPath){
#endif
      int i;
      // used by 'SIPR'
      path = realloc(path, strlen(REALCODEC_PATH) + 12);
      sprintf(path, "DT_Codecs=" REALCODEC_PATH);
      if(path[strlen(path)-1]!='/'){
        path[strlen(path)+1]=0;
        path[strlen(path)]='/';
      }
      path[strlen(path)+1]=0;
#ifdef USE_WIN32DLL
    if (dll_type == 1)
    {
      for (i=0; i < strlen(path); i++)
        if (path[i] == '/') path[i] = '\\';
      wraSetDLLAccessPath(path);
    }
    else
#endif
      raSetDLLAccessPath(path);
  }

#ifdef USE_WIN32DLL
    if (dll_type == 1){
      if(wraOpenCodec2)
	result=wraOpenCodec2(&sh->context,REALCODEC_PATH "\\");
      else
	result=wraOpenCodec(&sh->context);
    } else
#endif
    if(raOpenCodec2)
      result=raOpenCodec2(&sh->context,REALCODEC_PATH "/");
    else
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
	((short*)(sh->wf+1))[0],  // subpacket size
	((short*)(sh->wf+1))[3],  // coded frame size
	((short*)(sh->wf+1))[4], // codec data length
	((char*)(sh->wf+1))+10 // extras
    };
#if defined(USE_WIN32DLL) || defined(USE_MACSHLB)
    wra_init_t winit_data={
	sh->wf->nSamplesPerSec,
	sh->wf->wBitsPerSample,
	sh->wf->nChannels,
	100, // quality
	((short*)(sh->wf+1))[0],  // subpacket size
	((short*)(sh->wf+1))[3],  // coded frame size
	((short*)(sh->wf+1))[4], // codec data length
	((char*)(sh->wf+1))+10 // extras
    };
#endif
#ifdef USE_MACSHLB
	result=raInitDecoder(sh->context,&winit_data);
#else
#ifdef USE_WIN32DLL
    if (dll_type == 1)
	result=wraInitDecoder(sh->context,&winit_data);
    else
#endif
    result=raInitDecoder(sh->context,&init_data);
#endif
    if(result){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Decoder init failed, error code: 0x%X\n",result);
      return 0;
    }
//    printf("initdecoder ok (result: %x)\n", result);
  }

#ifdef USE_WIN32DLL
    if((raSetPwd && dll_type == 0) || (wraSetPwd && dll_type == 1)){
#else
    if(raSetPwd){
#endif
	// used by 'SIPR'
#ifdef USE_WIN32DLL
	if (dll_type == 1)
	    wraSetPwd(sh->context,"Ardubancel Quazanga");
	else
#endif
	raSetPwd(sh->context,"Ardubancel Quazanga"); // set password... lol.
    }
  
#ifdef USE_WIN32DLL
    if (dll_type == 1)
	result=wraSetFlavor(sh->context,((short*)(sh->wf+1))[2]);
    else
#endif
    result=raSetFlavor(sh->context,((short*)(sh->wf+1))[2]);
    if(result){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Decoder flavor setup failed, error code: 0x%X\n",result);
      return 0;
    }

#ifdef USE_WIN32DLL
    if (dll_type == 1)
	prop=wraGetFlavorProperty(sh->context,((short*)(sh->wf+1))[2],0,&len);
    else
#endif
    prop=raGetFlavorProperty(sh->context,((short*)(sh->wf+1))[2],0,&len);
    mp_msg(MSGT_DECAUDIO,MSGL_INFO,"Audio codec: [%d] %s\n",((short*)(sh->wf+1))[2],prop);

#ifdef USE_WIN32DLL
    if (dll_type == 1)
	prop=wraGetFlavorProperty(sh->context,((short*)(sh->wf+1))[2],1,&len);
    else
#endif
    prop=raGetFlavorProperty(sh->context,((short*)(sh->wf+1))[2],1,&len);
    if(prop){
      sh->i_bps=((*((int*)prop))+4)/8;
      mp_msg(MSGT_DECAUDIO,MSGL_INFO,"Audio bitrate: %5.3f kbit/s (%d bps)  \n",(*((int*)prop))*0.001f,sh->i_bps);
    } else
      sh->i_bps=12000; // dunno :(((  [12000 seems to be OK for crash.rmvb too]
    
//    prop=raGetFlavorProperty(sh->context,((short*)(sh->wf+1))[2],0x13,&len);
//    mp_msg(MSGT_DECAUDIO,MSGL_INFO,"Samples/block?: %d  \n",(*((int*)prop)));

  sh->audio_out_minsize=128000; // no idea how to get... :(
  sh->audio_in_minsize=((short*)(sh->wf+1))[1]*sh->wf->nBlockAlign;
  
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
#ifdef USE_WIN32DLL
    if (dll_type == 1)
    {
	if (wraFreeDecoder) wraFreeDecoder(sh->context);
	if (wraCloseCodec) wraCloseCodec(sh->context);
    }
#endif

    if (raFreeDecoder) raFreeDecoder(sh->context);
    if (raCloseCodec) raCloseCodec(sh->context);

#ifdef USE_MACSHLB
    if (rv_handle){
      (void)CloseConnection(rv_handle);
      DisposePtr((Ptr)rv_handle);
    }
    if (raCloseCodec) DisposePtr((Ptr)raCloseCodec);
    if (raDecode) DisposePtr((Ptr)raDecode);
    if (raFlush) DisposePtr((Ptr)raFlush);
    if (raFreeDecoder) DisposePtr((Ptr)raFreeDecoder);
    if (raGetFlavorProperty) DisposePtr((Ptr)raGetFlavorProperty);
    if (raOpenCodec) DisposePtr((Ptr)raOpenCodec);
    if (raOpenCodec2) DisposePtr((Ptr)raOpenCodec2);
    if (raInitDecoder) DisposePtr((Ptr)raInitDecoder);
#endif

#ifdef USE_WIN32DLL
    if (dll_type == 1)
    {
	if (rv_handle) FreeLibrary(rv_handle);
    } else
#endif
// this dlclose() causes some memory corruption, and crashes soon (in caller):
//    if (rv_handle) dlclose(rv_handle);
    rv_handle = NULL;
}

static unsigned char sipr_swaps[38][2]={
    {0,63},{1,22},{2,44},{3,90},{5,81},{7,31},{8,86},{9,58},{10,36},{12,68},
    {13,39},{14,73},{15,53},{16,69},{17,57},{19,88},{20,34},{21,71},{24,46},
    {25,94},{26,54},{28,75},{29,50},{32,70},{33,92},{35,74},{38,85},{40,56},
    {42,87},{43,65},{45,59},{48,79},{49,93},{51,89},{55,95},{61,76},{67,83},
    {77,80} };

static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen){
  int result;
  int len=-1;
  int sps=((short*)(sh->wf+1))[0];
  int w=sh->wf->nBlockAlign; // 5
  int h=((short*)(sh->wf+1))[1];
  int cfs=((short*)(sh->wf+1))[3];

//  printf("bs=%d  sps=%d  w=%d h=%d \n",sh->wf->nBlockAlign,sps,w,h);
  
#if 1
  if(sh->a_in_buffer_len<=0){
      // fill the buffer!
		if (sh->format == mmioFOURCC('1','4','_','4')) {
			demux_read_data(sh->ds, sh->a_in_buffer, sh->wf->nBlockAlign);
			sh->a_in_buffer_size=
			sh->a_in_buffer_len=sh->wf->nBlockAlign;
		} else
		if (sh->format == mmioFOURCC('2','8','_','8')) {
			int i,j;
			for (j = 0; j < h; j++)
				for (i = 0; i < h/2; i++)
					demux_read_data(sh->ds, sh->a_in_buffer+i*2*w+j*cfs, cfs);
			sh->a_in_buffer_size=
			sh->a_in_buffer_len=sh->wf->nBlockAlign*h;
		} else
    if(!sps){
      // 'sipr' way
      int j,n;
      int bs=h*w*2/96; // nibbles per subpacket
      unsigned char *p=sh->a_in_buffer;
      demux_read_data(sh->ds, p, h*w);
      for(n=0;n<38;n++){
          int i=bs*sipr_swaps[n][0];
          int o=bs*sipr_swaps[n][1];
	  // swap nibbles of block 'i' with 'o'      TODO: optimize
	  for(j=0;j<bs;j++){
	      int x=(i&1) ? (p[(i>>1)]>>4) : (p[(i>>1)]&15);
	      int y=(o&1) ? (p[(o>>1)]>>4) : (p[(o>>1)]&15);
	      if(o&1) p[(o>>1)]=(p[(o>>1)]&0x0F)|(x<<4);
	        else  p[(o>>1)]=(p[(o>>1)]&0xF0)|x;
	      if(i&1) p[(i>>1)]=(p[(i>>1)]&0x0F)|(y<<4);
	        else  p[(i>>1)]=(p[(i>>1)]&0xF0)|y;
	      ++i;++o;
	  }
      }
      sh->a_in_buffer_size=
      sh->a_in_buffer_len=w*h;
    } else {
      // 'cook' way
      int x,y;
      w/=sps;
      for(y=0;y<h;y++)
        for(x=0;x<w;x++){
	    demux_read_data(sh->ds, sh->a_in_buffer+sps*(h*x+((h+1)/2)*(y&1)+(y>>1)), sps);
	}
      sh->a_in_buffer_size=
      sh->a_in_buffer_len=w*h*sps;
    }
  }

#else
  if(sh->a_in_buffer_len<=0){
      // fill the buffer!
      demux_read_data(sh->ds, sh->a_in_buffer, sh->wf->nBlockAlign);
      sh->a_in_buffer_size=
      sh->a_in_buffer_len=sh->wf->nBlockAlign;
  }
#endif
  
#ifdef USE_WIN32DLL
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

#endif
