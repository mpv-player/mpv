
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

#ifdef USE_REALCODECS

#include <stddef.h>
#include <dlfcn.h>

#include "ad_internal.h"

static ad_info_t info =  {
	"RealAudio decoder",  // name of the driver
	"real",   // driver name. should be the same as filename without ad_
	AFM_REAL,    // replace with registered AFM number
	"A'rpi",     // writer/maintainer of _this_ file
	"",          // writer/maintainer/site of the _codec_
	""           // comments
};

LIBAD_EXTERN(real)

typedef unsigned long ulong;

static void *handle=NULL;

void *__builtin_new(unsigned long size) {
	return malloc(size);
}

#ifdef __FreeBSD__
void* __ctype_b=NULL;
#endif

static ulong (*raCloseCodec)(ulong);
static ulong (*raDecode)(ulong,ulong,ulong,ulong,ulong,ulong);
static ulong (*raFlush)(ulong,ulong,ulong);
static ulong (*raFreeDecoder)(ulong);
static ulong (*raGetFlavorProperty)(ulong,ulong,ulong,ulong);
//static ulong (*raGetNumberOfFlavors2)(void);
static ulong (*raInitDecoder)(ulong,ulong);
static ulong (*raOpenCodec2)(ulong);
static ulong (*raSetFlavor)(ulong,ulong);
static void  (*raSetDLLAccessPath)(ulong);

typedef struct {
    int samplerate;
    short bits;
    short channels;
    int unk1;
    int unk2;
    int packetsize;
    int unk3;
    void* unk4;
} ra_init_t;

static int preinit(sh_audio_t *sh){
  // let's check if the driver is available, return 0 if not.
  // (you should do that if you use external lib(s) which is optional)
  unsigned int result;
  char path[4096];
  sprintf(path, LIBDIR "/real/%s", sh->codec->dll);
  handle = dlopen (path, RTLD_LAZY);
  if(!handle){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Cannot open dll: %s\n",dlerror());
      return 0;
  }

    raCloseCodec = dlsym(handle, "RACloseCodec");
    raDecode = dlsym(handle, "RADecode");
    raFlush = dlsym(handle, "RAFlush");
    raFreeDecoder = dlsym(handle, "RAFreeDecoder");
    raGetFlavorProperty = dlsym(handle, "RAGetFlavorProperty");
    raOpenCodec2 = dlsym(handle, "RAOpenCodec2");
    raInitDecoder = dlsym(handle, "RAInitDecoder");
    raSetFlavor = dlsym(handle, "RASetFlavor");
    raSetDLLAccessPath = dlsym(handle, "SetDLLAccessPath");
    
  if(!raCloseCodec || !raDecode || !raFlush || !raFreeDecoder ||
     !raGetFlavorProperty || !raOpenCodec2 || !raSetFlavor ||
     !raSetDLLAccessPath || !raInitDecoder){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Cannot resolve symbols - incompatible dll\n");
      return 0;
  }

    result=raOpenCodec2(&sh->context);
    if(result){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Decoder open failed, error code: 0x%X\n",result);
      return 0;
    }

  sh->samplerate=sh->wf->nSamplesPerSec;
  sh->samplesize=sh->wf->wBitsPerSample/8;
  sh->channels=sh->wf->nChannels;

  { unsigned char temp2[16]={1,0,0,3,4,0,0,0x14,0,0,0,0,0,1,0,3};
    // note: temp2[] come from audio stream extra header (last 16 of the total 24 bytes)
    ra_init_t init_data={
	sh->wf->nSamplesPerSec,sh->wf->wBitsPerSample,sh->wf->nChannels,
	100, // ???
	((short*)(sh->wf+1))[0],  // subpacket size
	sh->wf->nBlockAlign,
	16, // ??
	((char*)(sh->wf+1))+6+8
    };
    result=raInitDecoder(sh->context,&init_data);
    if(result){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Decoder init failed, error code: 0x%X\n",result);
      return 0;
    }
  }
  
    result=raSetFlavor(sh->context,((short*)(sh->wf+1))[2]);
    if(result){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Decoder flavor setup failed, error code: 0x%X\n",result);
      return 0;
    }

  sh->audio_out_minsize=128000; //sh->samplerate*sh->samplesize*sh->channels;
  sh->audio_in_minsize=((short*)(sh->wf+1))[1]*sh->wf->nBlockAlign;
//  sh->samplesize=2;
//  sh->channels=2;
//  sh->samplerate=44100;
//  sh->sample_format=AFMT_S16_LE;
  sh->i_bps=64000/8;
  
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
}

static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen){
  int result;
  int len=-1;
  int sps=((short*)(sh->wf+1))[0];
  int w=sh->wf->nBlockAlign/sps; // 5
  int h=((short*)(sh->wf+1))[1];
  
//  printf("bs=%d  sps=%d  w=%d h=%d \n",sh->wf->nBlockAlign,sps,w,h);

#if 1
  if(sh->a_in_buffer_len<=0){
      // fill the buffer!
      int x,y;
      for(y=0;y<h;y++)
        for(x=0;x<w;x++){
	    demux_read_data(sh->ds, sh->a_in_buffer+sps*(h*x+(h/2)*(y&1)+(y>>1)), sps);
	}
      sh->a_in_buffer_size=
      sh->a_in_buffer_len=w*h*sps;
  }

#else
  if(sh->a_in_buffer_len<=0){
      // fill the buffer!
      demux_read_data(sh->ds, sh->a_in_buffer, sh->wf->nBlockAlign);
      sh->a_in_buffer_size=
      sh->a_in_buffer_len=sh->wf->nBlockAlign;
  }
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
