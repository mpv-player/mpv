#define VCODEC_COPY 0
#define VCODEC_FRAMENO 1
// real codecs:
#define VCODEC_DIVX4 2
#define VCODEC_LIBAVCODEC 4
#define VCODEC_RAWRGB 6
#define VCODEC_VFW 7
#define VCODEC_LIBDV 8

#define ACODEC_COPY 0
#define ACODEC_PCM 1
#define ACODEC_VBRMP3 2
#define ACODEC_NULL 3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

#include "config.h"
#include "mp_msg.h"
#include "version.h"
#include "help_mp.h"

static char* banner_text=
"\n\n"
"MEncoder " VERSION MSGTR_MEncoderCopyright
"\n";

#include "cpudetect.h"

#include "codec-cfg.h"
#include "cfgparser.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "playtree.h"

#include "aviwrite.h"

#include "libvo/video_out.h"

#include "libmpcodecs/dec_audio.h"
#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/vf.h"

#include "libmpdemux/mp3_hdr.h"

#ifdef HAVE_MP3LAME
#undef CDECL
#include <lame/lame.h>
#endif

#ifdef HAVE_LIBCSS
#include "libmpdemux/dvdauth.h"
#endif

#include <inttypes.h>

#include "fastmemcpy.h"

int vo_doublebuffering=0;
int vo_directrendering=0;
int vo_config_count=0;

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
int vobsub_id=-1;
char* audio_lang=NULL;
char* dvdsub_lang=NULL;
static char* spudec_ifo=NULL;

static int has_audio=1;
char *audio_codec=NULL; // override audio codec
char *video_codec=NULL; // override video codec
char* audio_fm=NULL;     // override audio codec family 
char* video_fm=NULL;     // override video codec family 

// libvo opts: (defiend at libmpcodecs/vd.c)
extern int screen_size_xy;
extern float movie_aspect;
extern int softzoom;
extern int flip;


int out_audio_codec=-1;
int out_video_codec=-1;

// audio stream skip/resync functions requires only for seeking.
// (they should be implemented in the audio codec layer)
//void skip_audio_frame(sh_audio_t *sh_audio){}
//void resync_audio_stream(sh_audio_t *sh_audio){}

int verbose=0; // must be global!
double video_time_usage=0;
double vout_time_usage=0;
double max_video_time_usage=0;
double max_vout_time_usage=0;
double cur_video_time_usage=0;
double cur_vout_time_usage=0;
static double audio_time_usage=0;
static int total_time_usage_start=0;
int benchmark=0;

// A-V sync:
int delay_corrected=1;
static float default_max_pts_correction=-1;//0.01f;
static float max_pts_correction=0;//default_max_pts_correction;
static float c_total=0;

float audio_preload=0.5;
float audio_delay=0.0;
int audio_density=2;

float force_fps=0;
float force_ofps=0; // set to 24 for inverse telecine
static int skip_limit=-1;

int force_srate=0;

char *vobsub_out=NULL;
unsigned int vobsub_out_index=0;
char *vobsub_out_id=NULL;

char* out_filename="test.avi";

char *force_fourcc=NULL;

int pass=0;
char* passtmpfile="divx2pass.log";
int pass_working=0;

static int play_n_frames=-1;
static int play_n_frames_mf=-1;

#include "libvo/font_load.h"
#include "libvo/sub.h"

// sub:
char *font_name=NULL;
float font_factor=0.75;
char *sub_name=NULL;
float sub_delay=0;
float sub_fps=0;
int   sub_auto = 0;
int   subcc_enabled=0;

#ifdef USE_SUB
static subtitle* subtitles=NULL;
float sub_last_pts = -303;
#endif

// infos are empty by default
char *info_name=NULL;
char *info_artist=NULL;
char *info_genre=NULL;
char *info_subject=NULL;
char *info_copyright=NULL;
char *info_sourceform=NULL;
char *info_comment=NULL;



//char *out_audio_codec=NULL; // override audio codec
//char *out_video_codec=NULL; // override video codec

//#include "libmpeg2/mpeg2.h"
//#include "libmpeg2/mpeg2_internal.h"

#ifdef HAVE_MP3LAME
int lame_param_quality=0; // best
int lame_param_algqual=5; // same as old default
int lame_param_vbr=vbr_default;
int lame_param_mode=-1; // unset
int lame_param_padding=-1; // unset
int lame_param_br=-1; // unset
int lame_param_ratio=-1; // unset
float lame_param_scale=-1; // unset
#endif

static int vo_w=0, vo_h=0;
static int input_pitch, input_bpp;

//-------------------------- config stuff:

m_config_t* mconfig;

static int cfg_inc_verbose(struct config *conf){ ++verbose; return 0;}

static int cfg_include(struct config *conf, char *filename){
	return m_config_parse_config_file(mconfig, filename);
}

static char *seek_to_sec=NULL;
static off_t seek_to_byte=0;

static int parse_end_at(struct config *conf, const char* param);
static uint8_t* flip_upside_down(uint8_t* dst, const uint8_t* src, int width, int height);

#include "get_path.c"

#include "cfg-mplayer-def.h"
#include "cfg-mencoder.h"

#ifdef USE_DVDREAD
#include "spudec.h"
#endif
#include "vobsub.h"

/* FIXME */
void mencoder_exit(int level, char *how)
{
    if (how)
	printf("Exiting... (%s)\n", how);
    else
	printf("Exiting...\n");

    exit(level);
}

void parse_cfgfiles( m_config_t* conf )
{
  char *conffile;
  if ((conffile = get_path("mencoder")) == NULL) {
    mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_GetpathProblem);
  } else {
    if (m_config_parse_config_file(conf, conffile) < 0)
      mencoder_exit(1,"configfile error");
    free(conffile);
  }
}


//---------------------------------------------------------------------------

int dec_audio(sh_audio_t *sh_audio,unsigned char* buffer,int total){
    int size=0;
    int at_eof=0;
    while(size<total && !at_eof){
	int len=total-size;
		if(len>MAX_OUTBURST) len=MAX_OUTBURST;
		if(len>sh_audio->a_buffer_size) len=sh_audio->a_buffer_size;
		if(len>sh_audio->a_buffer_len){
		    int ret=decode_audio(sh_audio,
			&sh_audio->a_buffer[sh_audio->a_buffer_len],
    			len-sh_audio->a_buffer_len,
			sh_audio->a_buffer_size-sh_audio->a_buffer_len);
		    if(ret>0) sh_audio->a_buffer_len+=ret; else at_eof=1;
		}
		if(len>sh_audio->a_buffer_len) len=sh_audio->a_buffer_len;
		memcpy(buffer+size,sh_audio->a_buffer,len);
		sh_audio->a_buffer_len-=len; size+=len;
		if(sh_audio->a_buffer_len>0)
		    memcpy(sh_audio->a_buffer,&sh_audio->a_buffer[len],sh_audio->a_buffer_len);
    }
    return size;
}

extern void me_register_options(m_config_t* cfg);

//---------------------------------------------------------------------------

static int at_eof=0;
static int interrupted=0;

enum end_at_type_t {END_AT_NONE, END_AT_TIME, END_AT_SIZE};
static enum end_at_type_t end_at_type = END_AT_NONE;
static int end_at;

static void exit_sighandler(int x){
    at_eof=1;
    interrupted=1;
}

aviwrite_t* muxer=NULL;
FILE* muxer_f=NULL;

// callback for ve_*.c:
void mencoder_write_chunk(aviwrite_stream_t *s,int len,unsigned int flags){
    aviwrite_write_chunk(muxer,s,muxer_f,len,flags);
}


int main(int argc,char* argv[]){

stream_t* stream=NULL;
demuxer_t* demuxer=NULL;
stream_t* stream2=NULL;
demuxer_t* demuxer2=NULL;
demux_stream_t *d_audio=NULL;
demux_stream_t *d_video=NULL;
demux_stream_t *d_dvdsub=NULL;
sh_audio_t *sh_audio=NULL;
sh_video_t *sh_video=NULL;
int file_format=DEMUXER_TYPE_UNKNOWN;
int i;
void *vobsub_writer=NULL;

uint32_t ptimer_start;
uint32_t audiorate=0;
uint32_t videorate=0;
uint32_t audiosamples=1;
uint32_t videosamples=1;
uint32_t skippedframes=0;
uint32_t duplicatedframes=0;

aviwrite_stream_t* mux_a=NULL;
aviwrite_stream_t* mux_v=NULL;
off_t muxer_f_size=0;

#ifdef HAVE_MP3LAME
lame_global_flags *lame;
#endif

double v_pts_corr=0;
double v_timer_corr=0;

play_tree_t* playtree;
play_tree_iter_t* playtree_iter;
char* filename=NULL;
char* frameno_filename="frameno.avi";

int decoded_frameno=0;
int next_frameno=-1;

unsigned int timer_start;

  mp_msg_init();
  mp_msg_set_level(MSGL_STATUS);
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"%s",banner_text);

  /* Test for cpu capabilities (and corresponding OS support) for optimizing */
#ifdef ARCH_X86
  GetCpuCaps(&gCpuCaps);
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"CPUflags: Type: %d MMX: %d MMX2: %d 3DNow: %d 3DNow2: %d SSE: %d SSE2: %d\n",
      gCpuCaps.cpuType,gCpuCaps.hasMMX,gCpuCaps.hasMMX2,
      gCpuCaps.has3DNow, gCpuCaps.has3DNowExt,
      gCpuCaps.hasSSE, gCpuCaps.hasSSE2);
#endif

// check codec.conf
if(!parse_codec_cfg(get_path("codecs.conf"))){
  if(!parse_codec_cfg(CONFDIR"/codecs.conf")){
    mp_msg(MSGT_MENCODER,MSGL_HINT,MSGTR_CopyCodecsConf);
    mencoder_exit(1,NULL);
  }
}

  // FIXME: get rid of -dvd and other tricky options and config/playtree
  stream2=open_stream(frameno_filename,0,&i);
  if(stream2){
    demuxer2=demux_open(stream2,DEMUXER_TYPE_AVI,-1,-1,-2);
    if(demuxer2) printf(MSGTR_UsingPass3ControllFile,frameno_filename);
  }

  playtree = play_tree_new();
  mconfig = m_config_new(playtree);
  m_config_register_options(mconfig,mencoder_opts);
  me_register_options(mconfig);
  parse_cfgfiles(mconfig);

  if(m_config_parse_command_line(mconfig, argc, argv) < 0) mencoder_exit(1, "error parsing cmdline");
  playtree = play_tree_cleanup(playtree);
  if(playtree) {
    playtree_iter = play_tree_iter_new(playtree,mconfig);
    if(playtree_iter) {  
      if(play_tree_iter_step(playtree_iter,0,0) != PLAY_TREE_ITER_ENTRY) {
	play_tree_iter_free(playtree_iter);
	playtree_iter = NULL;
      }
      filename = play_tree_iter_get_file(playtree_iter,1);
    }
  }

  if(!filename && !vcd_track && !dvd_title && !tv_param_on){
	printf(MSGTR_MissingFilename);
	mencoder_exit(1,NULL);
  }

  mp_msg_set_level(verbose+MSGL_STATUS);

// check font
#ifdef USE_OSD
#ifdef HAVE_FREETYPE
  init_freetype();
#else
  if(font_name){
       vo_font=read_font_desc(font_name,font_factor,verbose>1);
       if(!vo_font) mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CantLoadFont,font_name);
  } else {
      // try default:
       vo_font=read_font_desc(get_path("font/font.desc"),font_factor,verbose>1);
       if(!vo_font)
       vo_font=read_font_desc(DATADIR"/font/font.desc",font_factor,verbose>1);
  }
#endif
#endif

  vo_init_osd();

  stream=open_stream(filename,vcd_track,&file_format);

  if(!stream){
	printf(MSGTR_CannotOpenFile_Device);
	mencoder_exit(1,NULL);
  }

  printf("success: format: %d  data: 0x%X - 0x%X\n",file_format, (int)(stream->start_pos),(int)(stream->end_pos));

#ifdef USE_DVDREAD
if(stream->type==STREAMTYPE_DVD){
  if(audio_lang && audio_id==-1) audio_id=dvd_aid_from_lang(stream,audio_lang);
  if(dvdsub_lang && dvdsub_id==-1) dvdsub_id=dvd_sid_from_lang(stream,dvdsub_lang);
}
#endif

  stream->start_pos+=seek_to_byte;

#ifdef HAVE_LIBCSS
//  current_module="libcss";
  if (dvdimportkey) {
    if (dvd_import_key(dvdimportkey)) {
      mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_ErrorDVDkey);
      mencoder_exit(1,NULL);
    }
    mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_CmdlineDVDkey);
  }
  if (dvd_auth_device) {
    //  if (dvd_auth(dvd_auth_device,f)) {
    if (dvd_auth(dvd_auth_device,filename)) {
      mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_ErrorDVDAuth);
      mencoder_exit(1,NULL);
    }
    mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_DVDauthOk);
  }
#endif

  if(stream_cache_size) stream_enable_cache(stream,stream_cache_size*1024,0,0);

  if(!has_audio || demuxer2) audio_id=-2; /* do NOT read audio packets... */

  //demuxer=demux_open(stream,file_format,video_id,audio_id,dvdsub_id);
  demuxer=demux_open(stream,file_format,audio_id,video_id,dvdsub_id);
  if(!demuxer){
	printf(MSGTR_CannotOpenDemuxer);
	mencoder_exit(1,NULL);
  }

d_audio=demuxer2 ? demuxer2->audio : demuxer->audio;
d_video=demuxer->video;
d_dvdsub=demuxer->sub;
sh_audio=d_audio->sh;
sh_video=d_video->sh;

  if(!video_read_properties(sh_video)){
      printf(MSGTR_CannotReadVideoPropertiers);
      mencoder_exit(1,NULL);
  }

  mp_msg(MSGT_MENCODER,MSGL_INFO,"[V] filefmt:%d  fourcc:0x%X  size:%dx%d  fps:%5.2f  ftime:=%6.4f\n",
   demuxer->file_format,sh_video->format, sh_video->disp_w,sh_video->disp_h,
   sh_video->fps,sh_video->frametime
  );

  if(sh_audio && out_audio_codec<0){
    mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_NoAudioEncoderSelected);
    mencoder_exit(1,NULL);
  }
  if(sh_video && out_video_codec<0){
    mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_NoVideoEncoderSelected);
    mencoder_exit(1,NULL);
  }

if(sh_audio && (out_audio_codec || seek_to_sec || !sh_audio->wf)){
  // Go through the codec.conf and find the best codec...
  sh_audio->codec=NULL;
  if(audio_fm) mp_msg(MSGT_MENCODER,MSGL_INFO,MSGTR_TryForceAudioFmtStr,audio_fm);
  while(1){
    sh_audio->codec=find_codec(sh_audio->format,NULL,sh_audio->codec,1);
    if(!sh_audio->codec){
      if(audio_fm) {
        sh_audio->codec=NULL; /* re-search */
        mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_CantFindAfmtFallback);
        audio_fm=NULL;
        continue;      
      }
      mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_CantFindAudioCodec,sh_audio->format);
      mp_msg(MSGT_MENCODER,MSGL_HINT, MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
      sh_audio=d_audio->sh=NULL;
      break;
    }
    if(audio_codec && strcmp(sh_audio->codec->name,audio_codec)) continue;
    else if(audio_fm && strcmp(sh_audio->codec->drv,audio_fm)) continue;
    mp_msg(MSGT_MENCODER,MSGL_INFO,"%s audio codec: [%s] afm:%s (%s)\n",audio_codec?"Forcing":"Detected",sh_audio->codec->name,sh_audio->codec->drv,sh_audio->codec->info);
    break;
  }
}

if(sh_audio && (out_audio_codec || seek_to_sec || !sh_audio->wf)){
  mp_msg(MSGT_MENCODER,MSGL_V,MSGTR_InitializingAudioCodec);
  if(!init_audio(sh_audio)){
    mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_CouldntInitAudioCodec);
    sh_audio=d_audio->sh=NULL;
  } else {
    mp_msg(MSGT_MENCODER,MSGL_INFO,"AUDIO: srate=%d  chans=%d  bps=%d  sfmt=0x%X  ratio: %d->%d\n",sh_audio->samplerate,sh_audio->channels,sh_audio->samplesize,
        sh_audio->sample_format,sh_audio->i_bps,sh_audio->o_bps);
  }
}

// set up video encoder:

if (vobsub_out) {
    unsigned int palette[16], width, height;
    unsigned char tmp[3] = { 0, 0, 0 };
    if (spudec_ifo && vobsub_parse_ifo(NULL,spudec_ifo, palette, &width, &height, 1, dvdsub_id, tmp) >= 0)
	vobsub_writer = vobsub_out_open(vobsub_out, palette, sh_video->disp_w, sh_video->disp_h,
					vobsub_out_id?vobsub_out_id:tmp, vobsub_out_index);
#ifdef USE_DVDREAD
    if (vobsub_writer == NULL) {
	char tmp[3];
	if (vobsub_out_id == NULL && stream->type == STREAMTYPE_DVD) {
	    int i;
	    dvd_priv_t *dvd = (dvd_priv_t*)stream->priv;
	    for (i = 0; i < dvd->nr_of_subtitles; ++i)
		if (dvd->subtitles[i].id == dvdsub_id) {
		    tmp[0] = (dvd->subtitles[i].language >> 8) & 0xff;
		    tmp[1] = dvd->subtitles[i].language & 0xff;
		    tmp[2] = 0;
		    vobsub_out_id = tmp;
		    break;
		}
	}
	vobsub_writer=vobsub_out_open(vobsub_out, stream->type==STREAMTYPE_DVD?((dvd_priv_t *)(stream->priv))->cur_pgc->palette:NULL,
				      sh_video->disp_w, sh_video->disp_h, vobsub_out_id, vobsub_out_index);
    }
#endif
}
else {
if (spudec_ifo) {
  unsigned int palette[16], width, height;
  if (vobsub_parse_ifo(NULL,spudec_ifo, palette, &width, &height, 1, -1, NULL) >= 0)
    vo_spudec=spudec_new_scaled(palette, sh_video->disp_w, sh_video->disp_h);
}
#ifdef USE_DVDREAD
if (vo_spudec==NULL) {
vo_spudec=spudec_new_scaled(stream->type==STREAMTYPE_DVD?((dvd_priv_t *)(stream->priv))->cur_pgc->palette:NULL,
			    sh_video->disp_w, sh_video->disp_h);
}
#endif
}

#ifdef USE_SUB
// after reading video params we should load subtitles because
// we know fps so now we can adjust subtitles time to ~6 seconds AST
// check .sub
//  current_module="read_subtitles_file";
  if(sub_name){
    subtitles=sub_read_file(sub_name, sh_video->fps);
    if(!subtitles) mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CantLoadSub,sub_name);
  } else
  if(sub_auto) { // auto load sub file ...
    subtitles=sub_read_file( filename ? sub_filename( get_path("sub/"), filename )
	                              : "default.sub", sh_video->fps );
  }
#endif	


// set up output file:
muxer_f=fopen(out_filename,"wb");
if(!muxer_f) {
  printf(MSGTR_CannotOpenOutputFile, out_filename);
  mencoder_exit(1,NULL);
}

muxer=aviwrite_new_muxer();

// ============= VIDEO ===============

mux_v=aviwrite_new_stream(muxer,AVIWRITE_TYPE_VIDEO);

mux_v->buffer_size=0x200000; // 2MB
mux_v->buffer=malloc(mux_v->buffer_size);

mux_v->source=sh_video;

mux_v->h.dwSampleSize=0; // VBR
mux_v->h.dwScale=10000;
mux_v->h.dwRate=mux_v->h.dwScale*(force_ofps?force_ofps:sh_video->fps);

mux_v->codec=out_video_codec;

mux_v->bih=NULL;
sh_video->codec=NULL;
sh_video->video_out=NULL;
sh_video->vfilter=NULL; // fixme!

switch(mux_v->codec){
case VCODEC_COPY:
    if (sh_video->bih)
	mux_v->bih=sh_video->bih;
    else
    {
	mux_v->bih=malloc(sizeof(BITMAPINFOHEADER));
	mux_v->bih->biSize=sizeof(BITMAPINFOHEADER);
	mux_v->bih->biWidth=sh_video->disp_w;
	mux_v->bih->biHeight=sh_video->disp_h;
	mux_v->bih->biCompression=sh_video->format;
	mux_v->bih->biPlanes=1;
	mux_v->bih->biBitCount=24; // FIXME!!!
	mux_v->bih->biSizeImage=mux_v->bih->biWidth*mux_v->bih->biHeight*(mux_v->bih->biBitCount/8);
    }
    printf("videocodec: framecopy (%dx%d %dbpp fourcc=%x)\n",
	mux_v->bih->biWidth, mux_v->bih->biHeight,
	mux_v->bih->biBitCount, mux_v->bih->biCompression);
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
default:

    switch(mux_v->codec){
    case VCODEC_DIVX4:
	sh_video->vfilter=vf_open_encoder(NULL,"divx4",(char *)mux_v); break;
    case VCODEC_LIBAVCODEC:
        sh_video->vfilter=vf_open_encoder(NULL,"lavc",(char *)mux_v); break;
    case VCODEC_RAWRGB:
        sh_video->vfilter=vf_open_encoder(NULL,"rawrgb",(char *)mux_v); break;
    case VCODEC_VFW:
        sh_video->vfilter=vf_open_encoder(NULL,"vfw",(char *)mux_v); break;
    case VCODEC_LIBDV:
        sh_video->vfilter=vf_open_encoder(NULL,"libdv",(char *)mux_v); break;
    }
    if(!mux_v->bih || !sh_video->vfilter){
        mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_EncoderOpenFailed);
        mencoder_exit(1,NULL);
    }
    // append 'expand' filter, it fixes stride problems and renders osd:
    sh_video->vfilter=vf_open_filter(sh_video->vfilter,"expand","-1:-1:-1:-1:1");
    sh_video->vfilter=append_filters(sh_video->vfilter);

mp_msg(MSGT_CPLAYER,MSGL_INFO,"==========================================================================\n");
// Go through the codec.conf and find the best codec...
sh_video->inited=0;
codecs_reset_selection(0);
if(video_codec){
    // forced codec by name:
    mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_ForcedVideoCodec,video_codec);
    init_video(sh_video,video_codec,NULL,-1);
} else {
    int status;
    // try in stability order: UNTESTED, WORKING, BUGGY, BROKEN
    if(video_fm) mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_TryForceVideoFmtStr,video_fm);
    for(status=CODECS_STATUS__MAX;status>=CODECS_STATUS__MIN;--status){
	if(video_fm) // try first the preferred codec family:
	    if(init_video(sh_video,NULL,video_fm,status)) break;
	if(init_video(sh_video,NULL,NULL,status)) break;
    }
}
if(!sh_video->inited){
    mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CantFindVideoCodec,sh_video->format);
    mp_msg(MSGT_CPLAYER,MSGL_HINT, MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
    mencoder_exit(1,NULL);
}
mp_msg(MSGT_CPLAYER,MSGL_INFO,"%s video codec: [%s] vfm:%s (%s)\n",
    video_codec?mp_gettext("Forcing"):mp_gettext("Detected"),sh_video->codec->name,sh_video->codec->drv,sh_video->codec->info);
mp_msg(MSGT_CPLAYER,MSGL_INFO,"==========================================================================\n");

}

/* force output fourcc to .. */
if ((force_fourcc != NULL) && (strlen(force_fourcc) >= 4))
{
    mux_v->bih->biCompression = mmioFOURCC(force_fourcc[0], force_fourcc[1],
					    force_fourcc[2], force_fourcc[3]);
    printf(MSGTR_ForcingOutputFourcc,
	mux_v->bih->biCompression, (char *)&mux_v->bih->biCompression);
}

if(demuxer->file_format!=DEMUXER_TYPE_AVI) pts_from_bps=0; // it must be 0 for mpeg/asf!

// ============= AUDIO ===============
if(sh_audio){

mux_a=aviwrite_new_stream(muxer,AVIWRITE_TYPE_AUDIO);

mux_a->buffer_size=0x100000; //16384;
mux_a->buffer=malloc(mux_a->buffer_size);

mux_a->source=sh_audio;

mux_a->codec=out_audio_codec;

switch(mux_a->codec){
case ACODEC_COPY:
    if (sh_audio->wf){
	mux_a->wf=sh_audio->wf;
	if(!sh_audio->i_bps) sh_audio->i_bps=mux_a->wf->nAvgBytesPerSec;
    } else {
	mux_a->wf = malloc(sizeof(WAVEFORMATEX));
	mux_a->wf->nBlockAlign = 1; //mux_a->h.dwSampleSize;
	mux_a->wf->wFormatTag = sh_audio->format;
	mux_a->wf->nChannels = sh_audio->channels;
	mux_a->wf->nSamplesPerSec = sh_audio->samplerate;
	mux_a->wf->nAvgBytesPerSec=sh_audio->i_bps; //mux_a->h.dwSampleSize*mux_a->wf->nSamplesPerSec;
	mux_a->wf->wBitsPerSample = 16; // FIXME
	mux_a->wf->cbSize=0; // FIXME for l3codeca.acm
    }
    if(sh_audio->audio.dwScale){
	mux_a->h.dwSampleSize=sh_audio->audio.dwSampleSize;
	mux_a->h.dwScale=sh_audio->audio.dwScale;
	mux_a->h.dwRate=sh_audio->audio.dwRate;
//	mux_a->h.dwStart=sh_audio->audio.dwStart;
    } else {
	mux_a->h.dwSampleSize=mux_a->wf->nBlockAlign;
	mux_a->h.dwScale=mux_a->h.dwSampleSize;
	mux_a->h.dwRate=mux_a->wf->nAvgBytesPerSec;
    }
    printf("audiocodec: framecopy (format=%x chans=%d rate=%d bits=%d bps=%d sample=%d)\n",
	mux_a->wf->wFormatTag, mux_a->wf->nChannels, mux_a->wf->nSamplesPerSec,
	mux_a->wf->wBitsPerSample, mux_a->wf->nAvgBytesPerSec, mux_a->h.dwSampleSize);
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
    printf("MP3 audio selected\n");
    mux_a->h.dwSampleSize=0; // VBR
    mux_a->h.dwRate=force_srate?force_srate:sh_audio->samplerate;
    mux_a->h.dwScale=(mux_a->h.dwRate<32000)?576:1152; // samples/frame
    if(sizeof(MPEGLAYER3WAVEFORMAT)!=30) mp_msg(MSGT_MENCODER,MSGL_WARN,"sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, maybe broken C compiler?\n",sizeof(MPEGLAYER3WAVEFORMAT));
    mux_a->wf=malloc(sizeof(MPEGLAYER3WAVEFORMAT)); // should be 30
    mux_a->wf->wFormatTag=0x55; // MP3
    mux_a->wf->nChannels= sh_audio->channels;
    mux_a->wf->nSamplesPerSec=force_srate?force_srate:sh_audio->samplerate;
    mux_a->wf->nAvgBytesPerSec=192000/8; // FIXME!
    mux_a->wf->nBlockAlign=(mux_a->h.dwRate<32000)?576:1152; // required for l3codeca.acm + WMP 6.4
    mux_a->wf->wBitsPerSample=0; //16;
    // from NaNdub:  (requires for l3codeca.acm)
    mux_a->wf->cbSize=12;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->wID=1;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->fdwFlags=2;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nBlockSize=(mux_a->h.dwRate<32000)?576:1152; // ???
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nFramesPerBlock=1;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nCodecDelay=0;
    break;
}

if (verbose>1) print_wave_header(mux_a->wf);

if(audio_delay!=0.0){
    mux_a->h.dwStart=audio_delay*mux_a->h.dwRate/mux_a->h.dwScale;
    printf("Setting AUDIO DELAY to %5.3f\n",mux_a->h.dwStart*mux_a->h.dwScale/(float)mux_a->h.dwRate);
}

} // if(sh_audio)

printf(MSGTR_WritingAVIHeader);
aviwrite_write_header(muxer,muxer_f);

decoded_frameno=0;

if(sh_audio)
switch(mux_a->codec){
#ifdef HAVE_MP3LAME
case ACODEC_VBRMP3:

lame=lame_init();
lame_set_bWriteVbrTag(lame,0);
lame_set_in_samplerate(lame,sh_audio->samplerate);
lame_set_num_channels(lame,mux_a->wf->nChannels);
lame_set_out_samplerate(lame,mux_a->wf->nSamplesPerSec);
lame_set_quality(lame,lame_param_algqual); // 0 = best q
if(lame_param_vbr){  // VBR:
    lame_set_VBR(lame,lame_param_vbr); // vbr mode
    lame_set_VBR_q(lame,lame_param_quality+1); // 1 = best vbr q  6=~128k
    if(lame_param_br>0) lame_set_VBR_mean_bitrate_kbps(lame,lame_param_br);
} else {    // CBR:
    if(lame_param_br>0) lame_set_brate(lame,lame_param_br);
}
if(lame_param_mode>=0) lame_set_mode(lame,lame_param_mode); // j-st
if(lame_param_ratio>0) lame_set_compression_ratio(lame,lame_param_ratio);
if(lame_param_scale>0) {
    printf("Setting audio input gain to %f\n", lame_param_scale);
    lame_set_scale(lame,lame_param_scale);
}
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

timer_start=GetTimerMS();

if (seek_to_sec) {
    int a,b; float d;

    if (sscanf(seek_to_sec, "%d:%d:%f", &a,&b,&d)==3)
        d += 3600*a + 60*b;
    else if (sscanf(seek_to_sec, "%d:%f", &a, &d)==2)
        d += 60*a;
    else 
        sscanf(seek_to_sec, "%f", &d);

    demux_seek(demuxer, d, 1);
    if(demuxer2) demux_seek(demuxer2, d, 1);
}

if(tv_param_on == 1) 
	{
	fprintf(stderr,"Forcing audio preload to 0, max pts correction to 0\n");
	audio_preload = 0.0;
	default_max_pts_correction = 0;
	}

play_n_frames=play_n_frames_mf;

while(!at_eof){

    float frame_time=0;
    int blit_frame=0;
    float a_pts=0;
    float v_pts=0;
    unsigned char* start=NULL;
    int in_size;
    int skip_flag=0; // 1=skip  -1=duplicate

    if((end_at_type == END_AT_SIZE && end_at <= ftello(muxer_f))  ||
       (end_at_type == END_AT_TIME && end_at < sh_video->timer))
        break;

    if(play_n_frames>=0){
      --play_n_frames;
      if(play_n_frames<0) break;
    }

if(sh_audio){
    // get audio:
    while(mux_a->timer-audio_preload<mux_v->timer){
	int len=0;

	ptimer_start = GetTimerMS();

	if(mux_a->h.dwSampleSize){
	    // CBR - copy 0.5 sec of audio
	    switch(mux_a->codec){
	    case ACODEC_COPY: // copy
		len=mux_a->wf->nAvgBytesPerSec/audio_density;
		len/=mux_a->h.dwSampleSize;if(len<1) len=1;
		len*=mux_a->h.dwSampleSize;
		len=demux_read_data(sh_audio->ds,mux_a->buffer,len);
		break;
	    case ACODEC_PCM:
		len=mux_a->h.dwSampleSize*(mux_a->h.dwRate/audio_density);
		len=dec_audio(sh_audio,mux_a->buffer,len);
		break;
	    }
	} else {
	    // VBR - encode/copy an audio frame
	    switch(mux_a->codec){
	    case ACODEC_COPY: // copy
		len=ds_get_packet(sh_audio->ds,(unsigned char**) &mux_a->buffer);
//		printf("VBR audio framecopy not yet implemented!\n");
		break;
#ifdef HAVE_MP3LAME
	    case ACODEC_VBRMP3:
		while(mux_a->buffer_len<4){
		  unsigned char tmp[2304];
		  int len=dec_audio(sh_audio,tmp,2304);
		  if(len<=0) break; // eof
		  /* mono encoding, a bit tricky */
		  if (mux_a->wf->nChannels == 1)
		  {
		    len = lame_encode_buffer(lame, (short *)tmp, (short *)tmp, len/2,
			mux_a->buffer+mux_a->buffer_len, mux_a->buffer_size-mux_a->buffer_len);
		  }
		  else
		  {
		    len=lame_encode_buffer_interleaved(lame,
		      (short *)tmp,len/4,
		      mux_a->buffer+mux_a->buffer_len,mux_a->buffer_size-mux_a->buffer_len);
		  }
		  if(len<0) break; // error
		  mux_a->buffer_len+=len;
		}
		if(mux_a->buffer_len<4) break;
		len=mp_decode_mp3_header(mux_a->buffer);
		//printf("%d\n",len);
		if(len<=0) break; // bad frame!
//		printf("[%d]\n",mp_mp3_get_lsf(mux_a->buffer));
		while(mux_a->buffer_len<len){
		  unsigned char tmp[2304];
		  int len=dec_audio(sh_audio,tmp,2304);
		  if(len<=0) break; // eof
		  /* mono encoding, a bit tricky */
		  if (mux_a->wf->nChannels == 1)
		  {
		    len = lame_encode_buffer(lame, (short *)tmp, (short *)tmp, len/2,
			mux_a->buffer+mux_a->buffer_len, mux_a->buffer_size-mux_a->buffer_len);
		  }
		  else
		  {
		    len=lame_encode_buffer_interleaved(lame,
		      (short *)tmp,len/4,
		      mux_a->buffer+mux_a->buffer_len,mux_a->buffer_size-mux_a->buffer_len);
		  }
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


	audiosamples++;
	audiorate+= (GetTimerMS() - ptimer_start);
    }
}

    // get video frame!

    in_size=video_read_frame(sh_video,&frame_time,&start,force_fps);
    if(in_size<0){ at_eof=1; break; }
    sh_video->timer+=frame_time; ++decoded_frameno;

if(demuxer2){	// 3-pass encoding, read control file (frameno.avi)
    // find our frame:
	while(next_frameno<decoded_frameno){
	    int* start;
	    int len=ds_get_packet(demuxer2->video,(unsigned char**) &start);
	    if(len<0){ at_eof=1;break;}
	    if(len==0) --skip_flag; else  // duplicate
	    if(len==4) next_frameno=start[0];
	}
    if(at_eof) break;
	// if(skip_flag) printf("!!!!!!!!!!!!\n");
	skip_flag=next_frameno-decoded_frameno;
    // find next frame:
	while(next_frameno<=decoded_frameno){
	    int* start;
	    int len=ds_get_packet(demuxer2->video,(unsigned char**) &start);
	    if(len<0){ at_eof=1;break;}
	    if(len==0) --skip_flag; else  // duplicate
	    if(len==4) next_frameno=start[0];
	}
//    if(at_eof) break;
//	    printf("Current fno=%d  requested=%d  skip=%d  \n",decoded_frameno,fno,skip_flag);
} else {

// check frame duplicate/drop:

//printf("\r### %5.3f ###\n",v_timer_corr);

if(v_timer_corr>=(float)mux_v->h.dwScale/mux_v->h.dwRate &&
    (skip_limit<0 || skip_flag<skip_limit) ){
    v_timer_corr-=(float)mux_v->h.dwScale/mux_v->h.dwRate;
    ++skip_flag; // skip
} else
while(v_timer_corr<=-(float)mux_v->h.dwScale/mux_v->h.dwRate &&
    (skip_limit<0 || (-skip_flag)<skip_limit) ){
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

} // demuxer2

v_timer_corr-=frame_time-(float)mux_v->h.dwScale/mux_v->h.dwRate;

ptimer_start = GetTimerMS();

switch(mux_v->codec){
case VCODEC_COPY:
    mux_v->buffer=start;
    if(skip_flag<=0) aviwrite_write_chunk(muxer,mux_v,muxer_f,in_size,(sh_video->ds->flags&1)?0x10:0);
    break;
case VCODEC_FRAMENO:
    mux_v->buffer=(unsigned char *)&decoded_frameno; // tricky
    if(skip_flag<=0) aviwrite_write_chunk(muxer,mux_v,muxer_f,sizeof(int),0x10);
    break;
default:
    // decode_video will callback down to ve_*.c encoders, through the video filters
    blit_frame=decode_video(sh_video,start,in_size,(skip_flag>0)?1:0);
    if(!blit_frame && skip_flag<=0){
	// unwanted skipping of a frame, what to do?
	if(skip_limit==0){
	    // skipping not allowed -> write empty frame:
	    aviwrite_write_chunk(muxer,mux_v,muxer_f,0,0);
	} else {
	    // skipping allowed -> skip it and distriubute timer error:
	    v_timer_corr-=(float)mux_v->h.dwScale/mux_v->h.dwRate;
	}
    }
}

videosamples++;
videorate+=(GetTimerMS() - ptimer_start);

if(skip_flag<0){
    // duplicate frame
	if(!tv_param_on && !verbose) printf(MSGTR_DuplicateFrames,-skip_flag);
    while(skip_flag<0){
	duplicatedframes++;
	aviwrite_write_chunk(muxer,mux_v,muxer_f,0,0);
	++skip_flag;
    }
} else
if(skip_flag>0){
    // skip frame
	if(!tv_param_on && !verbose) printf(MSGTR_SkipFrame);
	skippedframes++;
    --skip_flag;
}

if(sh_audio && !demuxer2){
    float AV_delay,x;
    // A-V sync!
    if(pts_from_bps){
        unsigned int samples=(sh_audio->audio.dwSampleSize)?
          ((ds_tell(d_audio)-sh_audio->a_in_buffer_len)/sh_audio->audio.dwSampleSize) :
          (d_audio->block_no); // <- used for VBR audio
//	printf("samples=%d  \n",samples);
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
}

//    printf("A:%6.1f V:%6.1f A-V:%7.3f oAV:%7.3f diff:%7.3f ct:%7.3f vpc:%7.3f   \r",
//	a_pts,v_pts,a_pts-v_pts,
//	(float)(mux_a->timer-mux_v->timer),
//	AV_delay, c_total, v_pts_corr );
//    printf("V:%6.1f \r", d_video->pts );

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

    {	float t=(GetTimerMS()-timer_start)*0.001f;
	float len=(demuxer->movi_end-demuxer->movi_start);
	float p=len>1000 ? (float)(demuxer->filepos-demuxer->movi_start) / len : 0;
	if(!len && sh_audio && sh_audio->audio.dwLength>100){
	    p=(sh_audio->audio.dwSampleSize? ds_tell(sh_audio->ds)/sh_audio->audio.dwSampleSize : sh_audio->ds->block_no)
	     / (float)(sh_audio->audio.dwLength);
	}
#if 0
	mp_msg(MSGT_AVSYNC,MSGL_STATUS,"%d < %d < %d  \r",
	    (int)demuxer->movi_start,
	    (int)demuxer->filepos,
	    (int)demuxer->movi_end);
#else
	if(verbose) {
		mp_msg(MSGT_AVSYNC,MSGL_STATUS,"Pos:%6.1fs %6df (%2d%%) %3dfps Trem:%4dmin %3dmb  A-V:%5.3f [%d:%d] A/Vms %d/%d D/S %d/%d \r",
	    	mux_v->timer, decoded_frameno, (int)(p*100),
	    	(t>1) ? (int)(decoded_frameno/t+0.5) : 0,
	    	(p>0.001) ? (int)((t/p-t)/60) : 0, 
	    	(p>0.001) ? (int)(ftello(muxer_f)/p/1024/1024) : 0,
	    	v_pts_corr,
	    	(mux_v->timer>1) ? (int)(mux_v->size/mux_v->timer/125) : 0,
	    	(mux_a && mux_a->timer>1) ? (int)(mux_a->size/mux_a->timer/125) : 0,
			audiorate/audiosamples, videorate/videosamples,
			duplicatedframes, skippedframes
		);
	} else
	mp_msg(MSGT_AVSYNC,MSGL_STATUS,"Pos:%6.1fs %6df (%2d%%) %3dfps Trem:%4dmin %3dmb  A-V:%5.3f [%d:%d]\r",
	    mux_v->timer, decoded_frameno, (int)(p*100),
	    (t>1) ? (int)(decoded_frameno/t+0.5) : 0,
	    (p>0.001) ? (int)((t/p-t)/60) : 0, 
	    (p>0.001) ? (int)(ftell(muxer_f)/p/1024/1024) : 0,
	    v_pts_corr,
	    (mux_v->timer>1) ? (int)(mux_v->size/mux_v->timer/125) : 0,
	    (mux_a && mux_a->timer>1) ? (int)(mux_a->size/mux_a->timer/125) : 0
	);
#endif
    }
        fflush(stdout);

#ifdef USE_SUB
  // find sub
  if(subtitles && d_video->pts>0){
      float pts=d_video->pts;
      if(sub_fps==0) sub_fps=sh_video->fps;
      if (pts > sub_last_pts || pts < sub_last_pts-1.0 ) {
         find_sub(subtitles,sub_uses_time?(100*(pts+sub_delay)):((pts+sub_delay)*sub_fps)); // FIXME! frame counter...
         sub_last_pts = pts;
      }
  }
#endif

#ifdef USE_DVDREAD
// DVD sub:
 if(vo_spudec||vobsub_writer){
     unsigned char* packet=NULL;
     int len;
     while((len=ds_get_packet_sub(d_dvdsub,&packet))>0){
	 mp_msg(MSGT_MENCODER,MSGL_V,"\rDVD sub: len=%d  v_pts=%5.3f  s_pts=%5.3f  \n",len,d_video->pts,d_dvdsub->pts);
	 if (vo_spudec)
	 spudec_assemble(vo_spudec,packet,len,90000*d_dvdsub->pts);
	 if (vobsub_writer)
	     vobsub_out_output(vobsub_writer,packet,len,mux_v->timer + d_dvdsub->pts - d_video->pts);
     }
     if (vo_spudec) {
     spudec_heartbeat(vo_spudec,90000*d_video->pts);
     vo_osd_changed(OSDTYPE_SPU);
     }
 }
#endif

 if(ferror(muxer_f)) {
     mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_ErrorWritingFile, out_filename);
     mencoder_exit(1, NULL);
 }

} // while(!at_eof)

if (demuxer) free_demuxer(demuxer);

#ifdef HAVE_MP3LAME
// fixup CBR mp3 audio header:
if(sh_audio && mux_a->codec==ACODEC_VBRMP3 && !lame_param_vbr){
    mux_a->h.dwSampleSize=1;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nBlockSize=
	(mux_a->size+(mux_a->h.dwLength>>1))/mux_a->h.dwLength;
    mux_a->h.dwLength=mux_a->size;
    mux_a->h.dwRate=mux_a->wf->nAvgBytesPerSec;
    mux_a->h.dwScale=1;
    mux_a->wf->nBlockAlign=1;
    printf("\n\nCBR audio: %d bytes/sec, %d bytes/block\n",
	    mux_a->h.dwRate,((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nBlockSize);
}
#endif

printf(MSGTR_WritingAVIIndex);
aviwrite_write_index(muxer,muxer_f);
muxer_f_size=ftello(muxer_f);
printf(MSGTR_FixupAVIHeader);
fseek(muxer_f,0,SEEK_SET);
aviwrite_write_header(muxer,muxer_f); // update header
if(ferror(muxer_f) || fclose(muxer_f) != 0) {
    mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_ErrorWritingFile, out_filename);
    mencoder_exit(1, NULL);
}
if(vobsub_writer)
    vobsub_out_close(vobsub_writer);

if(out_video_codec==VCODEC_FRAMENO && mux_v->timer>100){
    printf(MSGTR_RecommendedVideoBitrate,"650MB",(int)((650*1024*1024-muxer_f_size)/mux_v->timer/125));
    printf(MSGTR_RecommendedVideoBitrate,"700MB",(int)((700*1024*1024-muxer_f_size)/mux_v->timer/125));
    printf(MSGTR_RecommendedVideoBitrate,"800MB",(int)((800*1024*1024-muxer_f_size)/mux_v->timer/125));
}

printf(MSGTR_VideoStreamResult,
    (float)(mux_v->size/mux_v->timer*8.0f/1000.0f), (int)(mux_v->size/mux_v->timer), (int)mux_v->size, (float)mux_v->timer, decoded_frameno);
if(sh_audio)
printf(MSGTR_AudioStreamResult,
    (float)(mux_a->size/mux_a->timer*8.0f/1000.0f), (int)(mux_a->size/mux_a->timer), (int)mux_a->size, (float)mux_a->timer);

if(stream) free_stream(stream); // kill cache thread

return interrupted;
}

static int parse_end_at(struct config *conf, const char* param)
{
    int i;

    end_at_type = END_AT_NONE;
    
    /* End at size parsing */
    {
        char unit[4];
        
        end_at_type = END_AT_SIZE;

        if(sscanf(param, "%d%3s", &end_at, unit) == 2) {
            if(!strcasecmp(unit, "b"))
                ;
            else if(!strcasecmp(unit, "kb"))
                end_at *= 1024;
            else if(!strcasecmp(unit, "mb"))
                end_at *= 1024*1024;
            else
                end_at_type = END_AT_NONE;
        }
        else
            end_at_type = END_AT_NONE;
    }

    /* End at time parsing. This has to be last because of
     * sscanf("%f", ...) below */
    if(end_at_type == END_AT_NONE)
    {
        int a,b; float d;

        end_at_type = END_AT_TIME;
        
        if (sscanf(param, "%d:%d:%f", &a, &b, &d) == 3)
            end_at = 3600*a + 60*b + d;
        else if (sscanf(param, "%d:%f", &a, &d) == 2)
            end_at = 60*a + d;
        else if (sscanf(param, "%f", &d) == 1)
            end_at = d;
        else
            end_at_type = END_AT_NONE;
    }

    if(end_at_type == END_AT_NONE)
        return ERR_FUNC_ERR;

    return 1;
}

/* Flip the image in src and store the result in dst. src and dst may overlap.
   width is the size of each line in bytes. */
static uint8_t* flip_upside_down(uint8_t* dst, const uint8_t* src, int width,
                                 int height)
{
    uint8_t* tmp = malloc(width);
    int i;

    for(i = 0; i < height/2; i++) {
        memcpy(tmp, &src[i*width], width);
        memcpy(&dst[i * width], &src[(height - i) * width], width);
        memcpy(&dst[(height - i) * width], tmp, width);
    }

    free(tmp);
    return dst;
}

