
#define VCODEC_FRAMENO 1
#define VCODEC_DIVX4 2

#define ACODEC_PCM 1
#define ACODEC_VBRMP3 2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "config.h"
#include "mp_msg.h"
#include "version.h"
#include "help_mp.h"

static char* banner_text=
"\n\n"
"MEncoder " VERSION "(C) 2000-2001 Arpad Gereoffy (see DOCS!)\n"
"\n";

#include "cpudetect.h"


#include "codec-cfg.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "aviwrite.h"

#ifdef USE_LIBVO2
#include "libvo2/libvo2.h"
#else
#include "libvo/video_out.h"
#endif

#include "dec_audio.h"
#include "dec_video.h"

#include <encore2.h>
#include "divx4_vbr.h"

#ifdef HAVE_MP3LAME
#include <lame/lame.h>
#endif

#include <inttypes.h>
#include "../postproc/swscale.h"

//--------------------------

// cache2:
static int stream_cache_size=0;
#ifdef USE_STREAM_CACHE
extern int cache_fill_status;
#else
#define cache_fill_status 0
#endif

int vcd_track=0;
int audio_id=-1;
int video_id=-1;
int dvdsub_id=-1;

char *audio_codec=NULL; // override audio codec
char *video_codec=NULL; // override video codec
int audio_family=-1;     // override audio codec family 
int video_family=-1;     // override video codec family 

#ifdef HAVE_MP3LAME
int out_audio_codec=ACODEC_VBRMP3;
#else
int out_audio_codec=ACODEC_PCM;
#endif

int out_video_codec=VCODEC_DIVX4;

// audio stream skip/resync functions requires only for seeking.
// (they should be implemented in the audio codec layer)
//void skip_audio_frame(sh_audio_t *sh_audio){}
//void resync_audio_stream(sh_audio_t *sh_audio){}

int verbose=0; // must be global!

double video_time_usage=0;
double vout_time_usage=0;
static double audio_time_usage=0;
static int total_time_usage_start=0;
static int benchmark=0;

// A-V sync:
int delay_corrected=1;
static float default_max_pts_correction=-1;//0.01f;
static float max_pts_correction=0;//default_max_pts_correction;
static float c_total=0;

float force_fps=0;
float force_ofps=0; // set to 24 for inverse telecine

int force_srate=0;

char* out_filename="test.avi";
char* mp3_filename=NULL;
char* ac3_filename=NULL;

static int pass=0;
static char* passtmpfile="divx2pass.log";

static int play_n_frames=-1;

//char *out_audio_codec=NULL; // override audio codec
//char *out_video_codec=NULL; // override video codec

//#include "libmpeg2/mpeg2.h"
//#include "libmpeg2/mpeg2_internal.h"

ENC_PARAM divx4_param;
int divx4_crispness=100;

#ifdef HAVE_MP3LAME
int lame_param_quality=0; // best
int lame_param_vbr=vbr_default;
int lame_param_mode=-1; // unset
int lame_param_padding=-1; // unset
int lame_param_br=-1; // unset
int lame_param_ratio=-1; // unset
#endif

static int scale_srcW=0;
static int scale_srcH=0;
static int vo_w=0, vo_h=0;
//-------------------------- config stuff:

#include "cfgparser.h"

static int cfg_inc_verbose(struct config *conf){ ++verbose; return 0;}

static int cfg_include(struct config *conf, char *filename){
	return parse_config_file(conf, filename);
}

#include "get_path.c"

#include "cfg-mplayer-def.h"
#include "cfg-mencoder.h"

//---------------------------------------------------------------------------

// dummy datas for gui :(

#ifdef HAVE_NEW_GUI
float rel_seek_secs=0;
int abs_seek_pos=0;
int use_gui=0;

void exit_player(char* how){
}
void vo_x11_putkey(int key){
}
void vo_setwindow( int w,int g ) {
}
void vo_setwindowsize( int w,int h ) {
}

int       vo_resize = 0;
int       vo_expose = 0;

#endif
// ---

// mini dummy libvo:

static unsigned char* vo_image=NULL;
static unsigned char* vo_image_ptr=NULL;

static uint32_t draw_slice(uint8_t *src[], int stride[], int w,int h, int x0,int y0){
  int y;
//  printf("draw_slice %dx%d %d;%d\n",w,h,x0,y0);
  if(scale_srcW)
  {
      uint8_t* dstPtr[3]= {
      		vo_image, 
		vo_image + vo_w*vo_h*5/4,
      		vo_image + vo_w*vo_h};
      SwScale_YV12slice(src, stride, y0, h, dstPtr, vo_w, 12, scale_srcW, scale_srcH, vo_w, vo_h);
  }
  else 
  {
  // copy Y:
  for(y=0;y<h;y++){
      unsigned char* s=src[0]+stride[0]*y;
      unsigned char* d=vo_image+vo_w*(y0+y)+x0;
      memcpy(d,s,w);
  }
  x0>>=1;y0>>=1;
  w>>=1;h>>=1;
  // copy U:
  for(y=0;y<h;y++){
      unsigned char* s=src[2]+stride[2]*y;
      unsigned char* d=vo_image+vo_w*vo_h+(vo_w>>1)*(y0+y)+x0;
      memcpy(d,s,w);
  }
  // copy V:
  for(y=0;y<h;y++){
      unsigned char* s=src[1]+stride[1]*y;
      unsigned char* d=vo_image+vo_w*vo_h+vo_w*vo_h/4+(vo_w>>1)*(y0+y)+x0;
      memcpy(d,s,w);
  }
  } // !swscaler
}

static uint32_t draw_frame(uint8_t *src[]){
  // printf("This function shouldn't be called - report bug!\n");
  // later: add YUY2->YV12 conversion here!
  vo_image_ptr=src[0];
}

vo_functions_t video_out;

//---------------------------------------------------------------------------

int dec_audio(sh_audio_t *sh_audio,unsigned char* buffer,int total){
    int size=0;
    int eof=0;
    while(size<total && !eof){
	int len=total-size;
		if(len>MAX_OUTBURST) len=MAX_OUTBURST;
		if(len>sh_audio->a_buffer_size) len=sh_audio->a_buffer_size;
		if(len>sh_audio->a_buffer_len){
		    int ret=decode_audio(sh_audio,
			&sh_audio->a_buffer[sh_audio->a_buffer_len],
    			len-sh_audio->a_buffer_len,
			sh_audio->a_buffer_size-sh_audio->a_buffer_len);
		    if(ret>0) sh_audio->a_buffer_len+=ret; else eof=1;
		}
		if(len>sh_audio->a_buffer_len) len=sh_audio->a_buffer_len;
		memcpy(buffer+size,sh_audio->a_buffer,len);
		sh_audio->a_buffer_len-=len; size+=len;
		if(sh_audio->a_buffer_len>0)
		    memcpy(sh_audio->a_buffer,&sh_audio->a_buffer[len],sh_audio->a_buffer_len);
    }
    return size;
}

//---------------------------------------------------------------------------

static int eof=0;
static int interrupted=0;

static void exit_sighandler(int x){
    eof=1;
    interrupted=1;
}

int main(int argc,char* argv[], char *envp[]){

stream_t* stream=NULL;
demuxer_t* demuxer=NULL;
demux_stream_t *d_audio=NULL;
demux_stream_t *d_video=NULL;
demux_stream_t *d_dvdsub=NULL;
sh_audio_t *sh_audio=NULL;
sh_video_t *sh_video=NULL;
int file_format=DEMUXER_TYPE_UNKNOWN;
int i;
unsigned int out_fmt;

aviwrite_t* muxer=NULL;
aviwrite_stream_t* mux_a=NULL;
aviwrite_stream_t* mux_v=NULL;
FILE* muxer_f=NULL;

ENC_FRAME enc_frame;
ENC_RESULT enc_result;
void* enc_handle=NULL;

#ifdef HAVE_MP3LAME
lame_global_flags *lame;
#endif

float audio_preload=0.5;

double v_pts_corr=0;
double v_timer_corr=0;

char** filenames=NULL;
char* filename=NULL;
int num_filenames;

int decoded_frameno=0;

//int out_buffer_size=0x200000;
//unsigned char* out_buffer=malloc(out_buffer_size);

  mp_msg_init(MSGL_STATUS);
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"%s",banner_text);

// check codec.conf
if(!parse_codec_cfg(get_path("codecs.conf"))){
  if(!parse_codec_cfg(DATADIR"/codecs.conf")){
    mp_msg(MSGT_MENCODER,MSGL_HINT,MSGTR_CopyCodecsConf);
    exit(0);
  }
}

  /* Test for cpu capabilities (and corresponding OS support) for optimizing */
#ifdef ARCH_X86
  GetCpuCaps(&gCpuCaps);
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"CPUflags: Type: %d MMX: %d MMX2: %d 3DNow: %d 3DNow2: %d SSE: %d SSE2: %d\n",
      gCpuCaps.cpuType,gCpuCaps.hasMMX,gCpuCaps.hasMMX2,
      gCpuCaps.has3DNow, gCpuCaps.has3DNowExt,
      gCpuCaps.hasSSE, gCpuCaps.hasSSE2);
#endif


// set some defaults, before parsing configfile/commandline:
divx4_param.min_quantizer = 2;
divx4_param.max_quantizer = 31;
divx4_param.rc_period = 2000;
divx4_param.rc_reaction_period = 10;
divx4_param.rc_reaction_ratio  = 20;


  num_filenames=parse_command_line(conf, argc, argv, envp, &filenames);
  if(num_filenames<0) exit(1); // error parsing cmdline
  if(!num_filenames && !vcd_track && !dvd_title){
	printf("\nMissing filename!\n\n");
	exit(1);
  }

  mp_msg_init(verbose+MSGL_STATUS);

  filename=(num_filenames>0)?filenames[0]:NULL;
  stream=open_stream(filename,vcd_track,&file_format);

  if(!stream){
	printf("Cannot open file/device\n");
	exit(1);
  }

  printf("success: format: %d  data: 0x%X - 0x%X\n",file_format, (int)(stream->start_pos),(int)(stream->end_pos));

  if(stream_cache_size) stream_enable_cache(stream,stream_cache_size*1024);

  //demuxer=demux_open(stream,file_format,video_id,audio_id,dvdsub_id);
  demuxer=demux_open(stream,file_format,audio_id,video_id,dvdsub_id);
  if(!demuxer){
	printf("Cannot open demuxer\n");
	exit(1);
  }

d_audio=demuxer->audio;
d_video=demuxer->video;
d_dvdsub=demuxer->sub;
sh_audio=d_audio->sh;
sh_video=d_video->sh;

  if(!video_read_properties(sh_video)){
      printf("Couldn't read video properties\n");
      exit(1);
  }

  mp_msg(MSGT_MENCODER,MSGL_INFO,"[V] filefmt:%d  fourcc:0x%X  size:%dx%d  fps:%5.2f  ftime:=%6.4f\n",
   demuxer->file_format,sh_video->format, sh_video->disp_w,sh_video->disp_h,
   sh_video->fps,sh_video->frametime
  );
  

sh_video->codec=NULL;
if(out_video_codec>1){

if(video_family!=-1) mp_msg(MSGT_MENCODER,MSGL_INFO,MSGTR_TryForceVideoFmt,video_family);
while(1){
  sh_video->codec=find_codec(sh_video->format,
    sh_video->bih?((unsigned int*) &sh_video->bih->biCompression):NULL,sh_video->codec,0);
  if(!sh_video->codec){
    if(video_family!=-1) {
      sh_video->codec=NULL; /* re-search */
      mp_msg(MSGT_MENCODER,MSGL_WARN,MSGTR_CantFindVfmtFallback);
      video_family=-1;
      continue;      
    }
    mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_CantFindVideoCodec,sh_video->format);
    mp_msg(MSGT_MENCODER,MSGL_HINT, MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
    exit(1);
  }
  if(video_codec && strcmp(sh_video->codec->name,video_codec)) continue;
  else if(video_family!=-1 && sh_video->codec->driver!=video_family) continue;
  break;
}

mp_msg(MSGT_MENCODER,MSGL_INFO,"%s video codec: [%s] drv:%d (%s)\n",video_codec?"Forcing":"Detected",sh_video->codec->name,sh_video->codec->driver,sh_video->codec->info);

for(i=0;i<CODECS_MAX_OUTFMT;i++){
    out_fmt=sh_video->codec->outfmt[i];
    if(out_fmt==0xFFFFFFFF) continue;
    if(out_fmt==IMGFMT_YV12) break;
    if(out_fmt==IMGFMT_I420) break;
    if(out_fmt==IMGFMT_IYUV) break;
    if(out_fmt==IMGFMT_YUY2) break;
    if(out_fmt==IMGFMT_UYVY) break;
}
if(i>=CODECS_MAX_OUTFMT){
    mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_VOincompCodec);
    exit(1); // exit_player(MSGTR_Exit_error);
}
sh_video->outfmtidx=i;

if(out_fmt==IMGFMT_YV12 && (vo_w!=0 || vo_h!=0))
{
	scale_srcW= sh_video->disp_w;
	scale_srcH= sh_video->disp_h;
	if(!vo_w) vo_w=sh_video->disp_w;
	if(!vo_h) vo_h=sh_video->disp_h;
}
else
{
	vo_w=sh_video->disp_w;
	vo_h=sh_video->disp_h;
}

if(out_fmt==IMGFMT_YV12 || out_fmt==IMGFMT_I420 || out_fmt==IMGFMT_IYUV){
    vo_image=malloc(vo_w*vo_h*3/2);
    vo_image_ptr=vo_image;
}

if(!init_video(sh_video)){
     mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_CouldntInitVideoCodec);
     exit(1);
}

} // if(out_video_codec)

if(sh_audio){
  // Go through the codec.conf and find the best codec...
  sh_audio->codec=NULL;
  if(audio_family!=-1) mp_msg(MSGT_MENCODER,MSGL_INFO,MSGTR_TryForceAudioFmt,audio_family);
  while(1){
    sh_audio->codec=find_codec(sh_audio->format,NULL,sh_audio->codec,1);
    if(!sh_audio->codec){
      if(audio_family!=-1) {
        sh_audio->codec=NULL; /* re-search */
        mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_CantFindAfmtFallback);
        audio_family=-1;
        continue;      
      }
      mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_CantFindAudioCodec,sh_audio->format);
      mp_msg(MSGT_MENCODER,MSGL_HINT, MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
      sh_audio=d_audio->sh=NULL;
      break;
    }
    if(audio_codec && strcmp(sh_audio->codec->name,audio_codec)) continue;
    else if(audio_family!=-1 && sh_audio->codec->driver!=audio_family) continue;
    mp_msg(MSGT_MENCODER,MSGL_INFO,"%s audio codec: [%s] drv:%d (%s)\n",audio_codec?"Forcing":"Detected",sh_audio->codec->name,sh_audio->codec->driver,sh_audio->codec->info);
    break;
  }
}

if(sh_audio){
  mp_msg(MSGT_MENCODER,MSGL_V,"Initializing audio codec...\n");
  if(!init_audio(sh_audio)){
    mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_CouldntInitAudioCodec);
    sh_audio=d_audio->sh=NULL;
  } else {
    mp_msg(MSGT_MENCODER,MSGL_INFO,"AUDIO: srate=%d  chans=%d  bps=%d  sfmt=0x%X  ratio: %d->%d\n",sh_audio->samplerate,sh_audio->channels,sh_audio->samplesize,
        sh_audio->sample_format,sh_audio->i_bps,sh_audio->o_bps);
  }
}



// set up video encoder:
SwScale_Init();
video_out.draw_slice=draw_slice;
video_out.draw_frame=draw_frame;

// set up output file:
muxer_f=fopen(out_filename,"wb");
if(!muxer_f) {
  printf("Cannot open output file '%s'\n", out_filename);
  exit(1);
}

muxer=aviwrite_new_muxer();

// ============= VIDEO ===============

mux_v=aviwrite_new_stream(muxer,AVIWRITE_TYPE_VIDEO);

mux_v->buffer_size=0x200000;
mux_v->buffer=malloc(mux_v->buffer_size);

mux_v->source=sh_video;

mux_v->h.dwSampleSize=0; // VBR
mux_v->h.dwScale=10000;
mux_v->h.dwRate=mux_v->h.dwScale*(force_ofps?force_ofps:sh_video->fps);

mux_v->codec=out_video_codec;

switch(mux_v->codec){
case 0:
    mux_v->bih=sh_video->bih;
    break;
case VCODEC_FRAMENO:
    mux_v->bih=malloc(sizeof(BITMAPINFOHEADER));
    mux_v->bih->biSize=sizeof(BITMAPINFOHEADER);
    mux_v->bih->biWidth=vo_w;
    mux_v->bih->biHeight=vo_h;
    mux_v->bih->biPlanes=1;
    mux_v->bih->biBitCount=24;
    mux_v->bih->biCompression=mmioFOURCC('F','r','N','o');
    mux_v->bih->biSizeImage=mux_v->bih->biWidth*mux_v->bih->biHeight*(mux_v->bih->biBitCount/8);
    break;
case VCODEC_DIVX4:
    mux_v->bih=malloc(sizeof(BITMAPINFOHEADER));
    mux_v->bih->biSize=sizeof(BITMAPINFOHEADER);
    mux_v->bih->biWidth=vo_w;
    mux_v->bih->biHeight=vo_h;
    mux_v->bih->biPlanes=1;
    mux_v->bih->biBitCount=24;
    mux_v->bih->biCompression=mmioFOURCC('d','i','v','x');
    mux_v->bih->biSizeImage=mux_v->bih->biWidth*mux_v->bih->biHeight*(mux_v->bih->biBitCount/8);
    break;
}

// ============= AUDIO ===============
if(sh_audio){

mux_a=aviwrite_new_stream(muxer,AVIWRITE_TYPE_AUDIO);

mux_a->buffer_size=0x100000; //16384;
mux_a->buffer=malloc(mux_a->buffer_size);

mux_a->source=sh_audio;

mux_a->codec=out_audio_codec;

switch(mux_a->codec){
case 0:
    mux_a->h.dwSampleSize=sh_audio->audio.dwSampleSize;
    mux_a->h.dwScale=sh_audio->audio.dwScale;
    mux_a->h.dwRate=sh_audio->audio.dwRate;
    mux_a->wf=sh_audio->wf;
    break;
case ACODEC_PCM:
    printf("CBR PCM audio selected\n");
    mux_a->h.dwSampleSize=2*sh_audio->channels;
    mux_a->h.dwScale=1;
    mux_a->h.dwRate=sh_audio->samplerate;
    mux_a->wf=malloc(sizeof(WAVEFORMATEX));
    mux_a->wf->nBlockAlign=mux_a->h.dwSampleSize;
    mux_a->wf->wFormatTag=0x1; // PCM
    mux_a->wf->nChannels=sh_audio->channels;
    mux_a->wf->nSamplesPerSec=sh_audio->samplerate;
    mux_a->wf->nAvgBytesPerSec=mux_a->h.dwSampleSize*mux_a->wf->nSamplesPerSec;
    mux_a->wf->wBitsPerSample=16;
    mux_a->wf->cbSize=0; // FIXME for l3codeca.acm
    break;
case ACODEC_VBRMP3:
    mux_a->h.dwSampleSize=0; // VBR
    mux_a->h.dwScale=1152; // samples/frame
    mux_a->h.dwRate=sh_audio->samplerate;
    if(sizeof(MPEGLAYER3WAVEFORMAT)!=30) mp_msg(MSGT_MENCODER,MSGL_WARN,"sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, maybe broken C compiler?\n",sizeof(MPEGLAYER3WAVEFORMAT));
    mux_a->wf=malloc(sizeof(MPEGLAYER3WAVEFORMAT)); // should be 30
    mux_a->wf->wFormatTag=0x55; // MP3
    mux_a->wf->nChannels=sh_audio->channels;
    mux_a->wf->nSamplesPerSec=force_srate?force_srate:sh_audio->samplerate;
    mux_a->wf->nAvgBytesPerSec=192000/8; // FIXME!
    mux_a->wf->nBlockAlign=1152; // requires for l3codeca.acm + WMP 6.4
    mux_a->wf->wBitsPerSample=0; //16;
    // from NaNdub:  (requires for l3codeca.acm)
    mux_a->wf->cbSize=12;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->wID=1;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->fdwFlags=2;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nBlockSize=1152; // ???
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nFramesPerBlock=1;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nCodecDelay=0;
    break;
}
}

printf("Writing AVI header...\n");
aviwrite_write_header(muxer,muxer_f);

switch(mux_v->codec){
case 0:
    break;
case VCODEC_FRAMENO:
    decoded_frameno=0;
    break;
case VCODEC_DIVX4:
    // init divx4linux:
    divx4_param.x_dim=vo_w;
    divx4_param.y_dim=vo_h;
    divx4_param.framerate=(float)mux_v->h.dwRate/mux_v->h.dwScale;
    if(!divx4_param.bitrate) divx4_param.bitrate=800000;
    else if(divx4_param.bitrate<=16000) divx4_param.bitrate*=1000;
    if(!divx4_param.quality) divx4_param.quality=5; // the quality of compression ( 1 - fastest, 5 - best )
    divx4_param.handle=NULL;
    encore(NULL,ENC_OPT_INIT,&divx4_param,NULL);
    enc_handle=divx4_param.handle;
    switch(out_fmt){
    case IMGFMT_YV12:	enc_frame.colorspace=ENC_CSP_YV12; break;
    case IMGFMT_IYUV:
    case IMGFMT_I420:	enc_frame.colorspace=ENC_CSP_I420; break;
    case IMGFMT_YUY2:	enc_frame.colorspace=ENC_CSP_YUY2; break;
    case IMGFMT_UYVY:	enc_frame.colorspace=ENC_CSP_UYVY; break;
    case IMGFMT_RGB24:
    case IMGFMT_BGR24:
    	enc_frame.colorspace=ENC_CSP_RGB24; break;
    default:
	mp_msg(MSGT_MENCODER,MSGL_ERR,"divx4: unsupported out_fmt!\n");
    }
    switch(pass){
    case 1:
	VbrControl_init_2pass_vbr_analysis(passtmpfile, divx4_param.quality);
	break;
    case 2:
        VbrControl_init_2pass_vbr_encoding(passtmpfile,
					 divx4_param.bitrate,
					 divx4_param.framerate,
					 divx4_crispness,
					 divx4_param.quality);
	break;
    }
    break;
}

if(sh_audio)
switch(mux_a->codec){
#ifdef HAVE_MP3LAME
case ACODEC_VBRMP3:

lame=lame_init();
lame_set_bWriteVbrTag(lame,0);
lame_set_in_samplerate(lame,sh_audio->samplerate);
lame_set_num_channels(lame,mux_a->wf->nChannels);
lame_set_out_samplerate(lame,mux_a->wf->nSamplesPerSec);
if(lame_param_vbr){  // VBR:
    lame_set_VBR(lame,lame_param_vbr); // vbr mode
    lame_set_VBR_q(lame,lame_param_quality+1); // 1 = best vbr q  6=~128k
    if(lame_param_br>0) lame_set_VBR_mean_bitrate_kbps(lame,lame_param_br);
} else {    // CBR:
    lame_set_quality(lame,lame_param_quality); // 0 = best q
    if(lame_param_br>0) lame_set_brate(lame,lame_param_br);
}
if(lame_param_mode>=0) lame_set_mode(lame,lame_param_mode); // j-st
if(lame_param_ratio>0) lame_set_compression_ratio(lame,lame_param_ratio);
lame_init_params(lame);
if(verbose){
    lame_print_config(lame);
    lame_print_internals(lame);
}
break;
#endif
    
}

signal(SIGINT,exit_sighandler);  // Interrupt from keyboard
signal(SIGQUIT,exit_sighandler); // Quit from keyboard
signal(SIGTERM,exit_sighandler); // kill

while(!eof){

    float frame_time=0;
    int blit_frame=0;
    float a_pts=0;
    float v_pts=0;
    unsigned char* start=NULL;
    int in_size;
    int skip_flag=0; // 1=skip  -1=duplicate

    if(play_n_frames>=0){
      --play_n_frames;
      if(play_n_frames<0) break;
    }

if(sh_audio){
    // get audio:
    while(mux_a->timer-audio_preload<mux_v->timer){
	int len=0;
	if(mux_a->h.dwSampleSize){
	    // CBR - copy 0.5 sec of audio
	    switch(mux_a->codec){
	    case 0: // copy
		len=sh_audio->i_bps/2;
		len/=mux_a->h.dwSampleSize;if(len<1) len=1;
		len*=mux_a->h.dwSampleSize;
		len=demux_read_data(sh_audio->ds,mux_a->buffer,len);
		break;
	    case ACODEC_PCM:
		len=mux_a->h.dwSampleSize*(mux_a->h.dwRate/2);
		len=dec_audio(sh_audio,mux_a->buffer,len);
		break;
	    }
	} else {
	    // VBR - encode/copy an audio frame
	    switch(mux_a->codec){
	    case 0: // copy
		printf("not yet implemented!\n");
		break;
#ifdef HAVE_MP3LAME
	    case ACODEC_VBRMP3:
		while(mux_a->buffer_len<4){
		  unsigned char tmp[2304];
		  int len=dec_audio(sh_audio,tmp,2304);
		  if(len<=0) break; // eof
		  len=lame_encode_buffer_interleaved(lame,
		      tmp,len/4,
		      mux_a->buffer+mux_a->buffer_len,mux_a->buffer_size-mux_a->buffer_len);
		  if(len<0) break; // error
		  mux_a->buffer_len+=len;
		}
		if(mux_a->buffer_len<4) break;
		len=mp_decode_mp3_header(mux_a->buffer);
		//printf("%d\n",len);
		if(len<=0) break; // bad frame!
		while(mux_a->buffer_len<len){
		  unsigned char tmp[2304];
		  int len=dec_audio(sh_audio,tmp,2304);
		  if(len<=0) break; // eof
		  len=lame_encode_buffer_interleaved(lame,
		      tmp,len/4,
		      mux_a->buffer+mux_a->buffer_len,mux_a->buffer_size-mux_a->buffer_len);
		  if(len<0) break; // error
		  mux_a->buffer_len+=len;
		}
		break;
#endif
	    }
	}
	if(len<=0) break; // EOF?
	aviwrite_write_chunk(muxer,mux_a,muxer_f,len,0);
	if(!mux_a->h.dwSampleSize && mux_a->timer>0)
	    mux_a->wf->nAvgBytesPerSec=0.5f+(double)mux_a->size/mux_a->timer; // avg bps (VBR)
	if(mux_a->buffer_len>=len){
	    mux_a->buffer_len-=len;
	    memcpy(mux_a->buffer,mux_a->buffer+len,mux_a->buffer_len);
	}
    }
}

    // get video frame!
    in_size=video_read_frame(sh_video,&frame_time,&start,force_fps);
    if(in_size<0){ eof=1; break; }
    sh_video->timer+=frame_time; ++decoded_frameno;
    
    v_timer_corr-=frame_time-(float)mux_v->h.dwScale/mux_v->h.dwRate;

// check frame duplicate/drop:

if(v_timer_corr>=(float)mux_v->h.dwScale/mux_v->h.dwRate){
    v_timer_corr-=(float)mux_v->h.dwScale/mux_v->h.dwRate;
    ++skip_flag; // skip
} else
while(v_timer_corr<=-(float)mux_v->h.dwScale/mux_v->h.dwRate){
    v_timer_corr+=(float)mux_v->h.dwScale/mux_v->h.dwRate;
    --skip_flag; // dup
}

while( (v_pts_corr<=-(float)mux_v->h.dwScale/mux_v->h.dwRate && skip_flag>0)
 || (v_pts_corr<=-2*(float)mux_v->h.dwScale/mux_v->h.dwRate) ){
    v_pts_corr+=(float)mux_v->h.dwScale/mux_v->h.dwRate;
    --skip_flag; // dup
}
if( (v_pts_corr>=(float)mux_v->h.dwScale/mux_v->h.dwRate && skip_flag<0)
 || (v_pts_corr>=2*(float)mux_v->h.dwScale/mux_v->h.dwRate) )
  if(skip_flag<=0){ // we can't skip more than 1 frame now
    v_pts_corr-=(float)mux_v->h.dwScale/mux_v->h.dwRate;
    ++skip_flag; // skip
  }


switch(mux_v->codec){
case 0:
    mux_v->buffer=start;
    if(skip_flag<=0) aviwrite_write_chunk(muxer,mux_v,muxer_f,in_size,(sh_video->ds->flags&1)?0x10:0);
    break;
case VCODEC_FRAMENO:
    mux_v->buffer=&decoded_frameno; // tricky
    if(skip_flag<=0) aviwrite_write_chunk(muxer,mux_v,muxer_f,sizeof(int),0x10);
    break;
case VCODEC_DIVX4:
    blit_frame=decode_video(&video_out,sh_video,start,in_size,0);
    if(skip_flag>0) break;
    if(!blit_frame){
	// empty.
	aviwrite_write_chunk(muxer,mux_v,muxer_f,0,0);
	break;
    }
    enc_frame.image=vo_image_ptr;
    enc_frame.bitstream=mux_v->buffer;
    enc_frame.length=mux_v->buffer_size;
    enc_frame.mvs=NULL;
    enc_frame.quant=0;
    enc_frame.intra=0;
    if(pass==2){	// handle 2-pass:
	enc_frame.quant = VbrControl_get_quant();
	enc_frame.intra = VbrControl_get_intra();
	encore(enc_handle,ENC_OPT_ENCODE_VBR,&enc_frame,&enc_result);
        VbrControl_update_2pass_vbr_encoding(enc_result.motion_bits,
					    enc_result.texture_bits,
					    enc_result.total_bits);
    } else {
	encore(enc_handle,ENC_OPT_ENCODE,&enc_frame,&enc_result);
	if(pass==1){
	  VbrControl_update_2pass_vbr_analysis(enc_result.is_key_frame, 
					       enc_result.motion_bits, 
					       enc_result.texture_bits, 
					       enc_result.total_bits, 
					       enc_result.quantizer);
	}
    }
    
//    printf("encoding...\n");
//    printf("  len=%d  key:%d  qualt:%d  \n",enc_frame.length,enc_result.is_key_frame,enc_result.quantizer);
    aviwrite_write_chunk(muxer,mux_v,muxer_f,enc_frame.length,enc_result.is_key_frame?0x10:0);
    break;
}

if(skip_flag<0){
    // duplicate frame
    printf("\nduplicate %d frame(s)!!!    \n",-skip_flag);
    while(skip_flag<0){
	aviwrite_write_chunk(muxer,mux_v,muxer_f,0,0);
	++skip_flag;
    }
} else
if(skip_flag>0){
    // skip frame
    printf("\nskip frame!!!    \n");
    --skip_flag;
}

if(sh_audio){
    float AV_delay,x;
    // A-V sync!
    if(pts_from_bps){
        unsigned int samples=(sh_audio->audio.dwSampleSize)?
          ((ds_tell(d_audio)-sh_audio->a_in_buffer_len)/sh_audio->audio.dwSampleSize) :
          (d_audio->pack_no); // <- used for VBR audio
        a_pts=samples*(float)sh_audio->audio.dwScale/(float)sh_audio->audio.dwRate;
      delay_corrected=1;
    } else {
      // PTS = (last timestamp) + (bytes after last timestamp)/(bytes per sec)
      a_pts=d_audio->pts;
      if(!delay_corrected) if(a_pts) delay_corrected=1;
      //printf("*** %5.3f ***\n",a_pts);
      a_pts+=(ds_tell_pts(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    }
    v_pts=d_video->pts;
    // av = compensated (with out buffering delay) A-V diff
    AV_delay=(a_pts-v_pts); AV_delay-=mux_a->timer-(mux_v->timer-(v_timer_corr+v_pts_corr));
	// compensate input video timer by av:
        x=AV_delay*0.1f;
        if(x<-max_pts_correction) x=-max_pts_correction; else
        if(x> max_pts_correction) x= max_pts_correction;
        if(default_max_pts_correction>=0)
          max_pts_correction=default_max_pts_correction;
        else
          max_pts_correction=sh_video->frametime*0.10; // +-10% of time
	// sh_video->timer-=x;
	c_total+=x;
	v_pts_corr+=x;

    printf("A:%6.1f V:%6.1f A-V:%7.3f oAV:%7.3f diff:%7.3f ct:%7.3f vpc:%7.3f   \r",
	a_pts,v_pts,a_pts-v_pts,
	(float)(mux_a->timer-mux_v->timer),
	AV_delay, c_total, v_pts_corr );

}

#if 0
    mp_msg(MSGT_AVSYNC,MSGL_STATUS,"A:%6.1f V:%6.1f A-V:%7.3f ct:%7.3f  %3d/%3d  %2d%% %2d%% %4.1f%%  %d%%\r",
	  a_pts,v_pts,a_pts-v_pts,c_total,
          (int)sh_video->num_frames,(int)sh_video->num_frames_decoded,
          (sh_video->timer>0.5)?(int)(100.0*video_time_usage/(double)sh_video->timer):0,
          (sh_video->timer>0.5)?(int)(100.0*vout_time_usage/(double)sh_video->timer):0,
          (sh_video->timer>0.5)?(100.0*audio_time_usage/(double)sh_video->timer):0
	  ,cache_fill_status
        );
#endif

        fflush(stdout);



} // while(!eof)

#ifdef HAVE_MP3LAME
// fixup CBR mp3 audio header:
if(sh_audio && mux_a->codec==ACODEC_VBRMP3 && !lame_param_vbr){
    mux_a->h.dwSampleSize=1;
    mux_a->h.dwRate=mux_a->wf->nAvgBytesPerSec;
    mux_a->h.dwScale=1;
    printf("\n\nCBR audio effective bitrate: %8.3f kbit/s  (%d bytes/sec)\n",
	    mux_a->h.dwRate*8.0f/1000.0f,mux_a->h.dwRate);
}
#endif

printf("\nWriting AVI index...\n");
aviwrite_write_index(muxer,muxer_f);
printf("Fixup AVI header...\n");
fseek(muxer_f,0,SEEK_SET);
aviwrite_write_header(muxer,muxer_f); // update header
fclose(muxer_f);

printf("\nVideo stream: %8.3f kbit/s  (%d bps)  size: %d bytes  %5.3f secs\n",
    (float)(mux_v->size/mux_v->timer*8.0f/1000.0f), (int)(mux_v->size/mux_v->timer), mux_v->size, (float)mux_v->timer);
if(sh_audio)
printf("\nAudio stream: %8.3f kbit/s  (%d bps)  size: %d bytes  %5.3f secs\n",
    (float)(mux_a->size/mux_a->timer*8.0f/1000.0f), (int)(mux_a->size/mux_a->timer), mux_a->size, (float)mux_a->timer);

if(stream) free_stream(stream); // kill cache thread

return interrupted;
}
