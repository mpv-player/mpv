#define VCODEC_COPY 0
#define VCODEC_FRAMENO 1
// real codecs:
#define VCODEC_DIVX4 2
#define VCODEC_LIBAVCODEC 4
#define VCODEC_RAWRGB 6
#define VCODEC_VFW 7
#define VCODEC_LIBDV 8
#define VCODEC_XVID 9
#define VCODEC_QTVIDEO 10
#define VCODEC_NUV 11

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

#include "version.h"
#include "mp_msg.h"
#include "help_mp.h"

static char* banner_text=
"\n\n"
"MEncoder " VERSION MSGTR_MEncoderCopyright
"\n";

#include "cpudetect.h"

#include "codec-cfg.h"
#ifdef NEW_CONFIG
#include "m_option.h"
#include "m_config.h"
#include "parser-mecmd.h"
#else
#include "cfgparser.h"
#include "playtree.h"
#endif

#include "libmpdemux/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libmpdemux/mp3_hdr.h"
#include "libmpdemux/muxer.h"


#include "libvo/video_out.h"

#include "libao2/afmt.h"

#include "libmpcodecs/mp_image.h"
#include "libmpcodecs/dec_audio.h"
#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/vf.h"

// for MPEGLAYER3WAVEFORMAT:
#include "loader/wine/mmreg.h"

#ifdef HAVE_MP3LAME
#undef CDECL
#include <lame/lame.h>
#endif

#ifdef HAVE_LIBCSS
#include "libmpdemux/dvdauth.h"
#endif

#include <inttypes.h>

#include "libvo/fastmemcpy.h"

#include "osdep/timer.h"

int vo_doublebuffering=0;
int vo_directrendering=0;
int vo_config_count=0;

//--------------------------

// cache2:
int stream_cache_size=-1;
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
static char* audio_lang=NULL;
static char* dvdsub_lang=NULL;
static char* spudec_ifo=NULL;

static char** audio_codec_list=NULL;  // override audio codec
static char** video_codec_list=NULL;  // override video codec
static char** audio_fm_list=NULL;     // override audio codec family 
static char** video_fm_list=NULL;     // override video codec family 

static int out_audio_codec=-1;
static int out_video_codec=-1;

int out_file_format=MUXER_TYPE_AVI;	// default to AVI

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
int benchmark=0;

// A-V sync:
int delay_corrected=1;
static float default_max_pts_correction=-1;//0.01f;
static float max_pts_correction=0;//default_max_pts_correction;
static float c_total=0;

static float audio_preload=0.5;
static float audio_delay=0.0;
static int audio_density=2;

static float force_fps=0;
static float force_ofps=0; // set to 24 for inverse telecine
static int skip_limit=-1;

static int force_srate=0;
static int audio_output_format=0;

char *vobsub_out=NULL;
unsigned int vobsub_out_index=0;
char *vobsub_out_id=NULL;

char* out_filename="test.avi";

char *force_fourcc=NULL;

char* passtmpfile="divx2pass.log";

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
int   suboverlap_enabled = 1;

#ifdef USE_SUB
static subtitle* subtitles=NULL;
float sub_last_pts = -303;
#endif

int auto_expand=1;

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
#if HAVE_MP3LAME >= 392
int lame_param_fast=0; // unset
static char* lame_param_preset=NULL; // unset
static int  lame_presets_set( lame_t gfp, int fast, int cbr, const char* preset_name );
static void  lame_presets_longinfo_dm ( FILE* msgfp );
#endif
#endif

//static int vo_w=0, vo_h=0;

//-------------------------- config stuff:

m_config_t* mconfig;

#ifdef NEW_CONFIG
extern int
m_config_parse_config_file(m_config_t* config, char *conffile);
#endif


static int cfg_inc_verbose(struct config *conf){ ++verbose; return 0;}

static int cfg_include(struct config *conf, char *filename){
	return m_config_parse_config_file(mconfig, filename);
}

static char *seek_to_sec=NULL;
static off_t seek_to_byte=0;

static int parse_end_at(struct config *conf, const char* param);
//static uint8_t* flip_upside_down(uint8_t* dst, const uint8_t* src, int width, int height);

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

static int dec_audio(sh_audio_t *sh_audio,unsigned char* buffer,int total){
    int size=0;
    int at_eof=0;
    while(size<total && !at_eof){
	int len=total-size;
		if(len>MAX_OUTBURST) len=MAX_OUTBURST;
		if(len>sh_audio->a_out_buffer_size) len=sh_audio->a_out_buffer_size;
		if(len>sh_audio->a_out_buffer_len){
		    int ret=decode_audio(sh_audio,
			&sh_audio->a_out_buffer[sh_audio->a_out_buffer_len],
    			len-sh_audio->a_out_buffer_len,
			sh_audio->a_out_buffer_size-sh_audio->a_out_buffer_len);
		    if(ret>0) sh_audio->a_out_buffer_len+=ret; else at_eof=1;
		}
		if(len>sh_audio->a_out_buffer_len) len=sh_audio->a_out_buffer_len;
		memcpy(buffer+size,sh_audio->a_out_buffer,len);
		sh_audio->a_out_buffer_len-=len; size+=len;
		if(sh_audio->a_out_buffer_len>0)
		    memcpy(sh_audio->a_out_buffer,&sh_audio->a_out_buffer[len],sh_audio->a_out_buffer_len);
    }
    return size;
}

//---------------------------------------------------------------------------

static int at_eof=0;
static int interrupted=0;

enum end_at_type_t {END_AT_NONE, END_AT_TIME, END_AT_SIZE};
static enum end_at_type_t end_at_type = END_AT_NONE;
static double end_at;

static void exit_sighandler(int x){
    at_eof=1;
    interrupted=1;
}

static muxer_t* muxer=NULL;
static FILE* muxer_f=NULL;

extern void print_wave_header(WAVEFORMATEX *h);

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
uint32_t badframes=0;

muxer_stream_t* mux_a=NULL;
muxer_stream_t* mux_v=NULL;
off_t muxer_f_size=0;

#ifdef HAVE_MP3LAME
lame_global_flags *lame;
#endif

double v_pts_corr=0;
double v_timer_corr=0;

#ifdef NEW_CONFIG
m_entry_t* filelist = NULL;
#else
play_tree_t* playtree;
play_tree_iter_t* playtree_iter;
#endif
char* filename=NULL;
char* frameno_filename="frameno.avi";

int decoded_frameno=0;
int next_frameno=-1;

unsigned int timer_start;

  mp_msg_init();
  mp_msg_set_level(MSGL_STATUS);
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"%s",banner_text);

  /* Test for cpu capabilities (and corresponding OS support) for optimizing */
  GetCpuCaps(&gCpuCaps);
#ifdef ARCH_X86
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"CPUflags: Type: %d MMX: %d MMX2: %d 3DNow: %d 3DNow2: %d SSE: %d SSE2: %d\n",
      gCpuCaps.cpuType,gCpuCaps.hasMMX,gCpuCaps.hasMMX2,
      gCpuCaps.has3DNow, gCpuCaps.has3DNowExt,
      gCpuCaps.hasSSE, gCpuCaps.hasSSE2);
#endif

// check codec.conf
if(!parse_codec_cfg(get_path("codecs.conf"))){
  if(!parse_codec_cfg(CONFDIR"/codecs.conf")){
    if(!parse_codec_cfg(NULL)){
      mp_msg(MSGT_MENCODER,MSGL_HINT,MSGTR_CopyCodecsConf);
      exit(0);
    }
    mp_msg(MSGT_MENCODER,MSGL_INFO,MSGTR_BuiltinCodecsConf);
  }
}

  // FIXME: get rid of -dvd and other tricky options
  stream2=open_stream(frameno_filename,0,&i);
  if(stream2){
    demuxer2=demux_open(stream2,DEMUXER_TYPE_AVI,-1,-1,-2,NULL);
    if(demuxer2) printf(MSGTR_UsingPass3ControllFile,frameno_filename);
    else mp_msg(MSGT_DEMUXER,MSGL_ERR,MSGTR_FormatNotRecognized);
  }

  // New config code
#ifdef NEW_CONFIG
 mconfig = m_config_new();
 m_config_register_options(mconfig,mencoder_opts);
 parse_cfgfiles(mconfig);
 filelist = m_config_parse_me_command_line(mconfig, argc, argv);
 if(!filelist) mencoder_exit(1, "error parsing cmdline");
 m_entry_set_options(mconfig,&filelist[0]);
 filename = filelist[0].name;
 // Warn the user if he put more than 1 filename ?
#else
  playtree = play_tree_new();
  mconfig = m_config_new(playtree);
  m_config_register_options(mconfig,mencoder_opts);
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
#endif

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

  if(stream_cache_size>0) stream_enable_cache(stream,stream_cache_size*1024,0,0);

  if(demuxer2) audio_id=-2; /* do NOT read audio packets... */

  //demuxer=demux_open(stream,file_format,video_id,audio_id,dvdsub_id);
  demuxer=demux_open(stream,file_format,audio_id,video_id,dvdsub_id,filename);
  if(!demuxer){
        mp_msg(MSGT_DEMUXER,MSGL_ERR,MSGTR_FormatNotRecognized);
	printf(MSGTR_CannotOpenDemuxer);
	mencoder_exit(1,NULL);
  }

d_audio=demuxer2 ? demuxer2->audio : demuxer->audio;
d_video=demuxer->video;
d_dvdsub=demuxer->sub;
sh_audio=d_audio->sh;
sh_video=d_video->sh;

  if(!video_read_properties(sh_video)){
      printf(MSGTR_CannotReadVideoProperties);
      mencoder_exit(1,NULL);
  }

  mp_msg(MSGT_MENCODER,MSGL_INFO,"[V] filefmt:%d  fourcc:0x%X  size:%dx%d  fps:%5.2f  ftime:=%6.4f\n",
   demuxer->file_format,sh_video->format, sh_video->disp_w,sh_video->disp_h,
   sh_video->fps,sh_video->frametime
  );

  if(force_fps){
    sh_video->fps=force_fps;
    sh_video->frametime=1.0f/sh_video->fps;
    mp_msg(MSGT_MENCODER,MSGL_INFO,"input fps will be interpreted as %5.2f instead\n", sh_video->fps);
  }

  if(sh_audio && out_audio_codec<0){
    if(audio_id==-2)
	mp_msg(MSGT_MENCODER,MSGL_ERR,"This demuxer doesn't support -nosound yet.\n");
    mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_NoAudioEncoderSelected);
    mencoder_exit(1,NULL);
  }
  if(sh_video && out_video_codec<0){
    mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_NoVideoEncoderSelected);
    mencoder_exit(1,NULL);
  }

if(sh_audio && (out_audio_codec || seek_to_sec || !sh_audio->wf)){
  // Go through the codec.conf and find the best codec...
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"==========================================================================\n");
  if(!init_best_audio_codec(sh_audio,audio_codec_list,audio_fm_list)){
    sh_audio=d_audio->sh=NULL; // failed to init :(
  }
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"==========================================================================\n");
}

// set up video encoder:

if (vobsub_out) {
    unsigned int palette[16], width, height;
    unsigned char tmp[3] = { 0, 0, 0 };
    if (spudec_ifo && vobsub_parse_ifo(NULL,spudec_ifo, palette, &width, &height, 1, dvdsub_id, tmp) >= 0)
	vobsub_writer = vobsub_out_open(vobsub_out, palette, sh_video->disp_w, sh_video->disp_h,
					vobsub_out_id?vobsub_out_id:(char *)tmp, vobsub_out_index);
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

muxer=muxer_new_muxer(out_file_format,muxer_f);

// ============= VIDEO ===============

mux_v=muxer_new_stream(muxer,MUXER_TYPE_VIDEO);

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
    mux_v->bih->biWidth=sh_video->disp_w;
    mux_v->bih->biHeight=sh_video->disp_h;
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
    case VCODEC_XVID:
        sh_video->vfilter=vf_open_encoder(NULL,"xvid",(char *)mux_v); break;
    case VCODEC_QTVIDEO:
        sh_video->vfilter=vf_open_encoder(NULL,"qtvideo",(char *)mux_v); break;
    case VCODEC_NUV:        
        sh_video->vfilter=vf_open_encoder(NULL,"nuv",(char *)mux_v); break;

    }
    if(!mux_v->bih || !sh_video->vfilter){
        mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_EncoderOpenFailed);
        mencoder_exit(1,NULL);
    }
    // append 'expand' filter, it fixes stride problems and renders osd:
    if (auto_expand) sh_video->vfilter=vf_open_filter(sh_video->vfilter,"expand","-1:-1:-1:-1:1");
    sh_video->vfilter=append_filters(sh_video->vfilter);

    mp_msg(MSGT_CPLAYER,MSGL_INFO,"==========================================================================\n");
    init_best_video_codec(sh_video,video_codec_list,video_fm_list);
    mp_msg(MSGT_CPLAYER,MSGL_INFO,"==========================================================================\n");
    if(!sh_video->inited) mencoder_exit(1,NULL);

}

/* force output fourcc to .. */
if ((force_fourcc != NULL) && (strlen(force_fourcc) >= 4))
{
    mux_v->bih->biCompression = mmioFOURCC(force_fourcc[0], force_fourcc[1],
					    force_fourcc[2], force_fourcc[3]);
    printf(MSGTR_ForcingOutputFourcc,
	mux_v->bih->biCompression, (char *)&mux_v->bih->biCompression);
}

//if(demuxer->file_format!=DEMUXER_TYPE_AVI) pts_from_bps=0; // it must be 0 for mpeg/asf!

// ============= AUDIO ===============
if(sh_audio){

mux_a=muxer_new_stream(muxer,MUXER_TYPE_AUDIO);

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
    printf("audiocodec: framecopy (format=%x chans=%d rate=%ld bits=%d bps=%ld sample=%ld)\n",
	mux_a->wf->wFormatTag, mux_a->wf->nChannels, mux_a->wf->nSamplesPerSec,
	mux_a->wf->wBitsPerSample, mux_a->wf->nAvgBytesPerSec, mux_a->h.dwSampleSize);
    break;
case ACODEC_PCM:
    printf("CBR PCM audio selected\n");
    mux_a->h.dwSampleSize=2*sh_audio->channels;
    mux_a->h.dwScale=1;
    mux_a->h.dwRate=force_srate?force_srate:sh_audio->samplerate;
    mux_a->wf=malloc(sizeof(WAVEFORMATEX));
    mux_a->wf->nBlockAlign=mux_a->h.dwSampleSize;
    mux_a->wf->wFormatTag=0x1; // PCM
    mux_a->wf->nChannels=audio_output_channels?audio_output_channels:sh_audio->channels;
    mux_a->wf->nSamplesPerSec=mux_a->h.dwRate;
    mux_a->wf->nAvgBytesPerSec=mux_a->h.dwSampleSize*mux_a->wf->nSamplesPerSec;
    mux_a->wf->wBitsPerSample=16;
    mux_a->wf->cbSize=0; // FIXME for l3codeca.acm
    // setup filter:
    if(!init_audio_filters(sh_audio, 
        sh_audio->samplerate,
	sh_audio->channels, sh_audio->sample_format, sh_audio->samplesize,
	mux_a->wf->nSamplesPerSec, mux_a->wf->nChannels,
	(mux_a->wf->wBitsPerSample==8)?	AFMT_U8:AFMT_S16_LE,
	mux_a->wf->wBitsPerSample/8,
	16384, mux_a->wf->nAvgBytesPerSec)){
      mp_msg(MSGT_CPLAYER,MSGL_ERR,"Couldn't find matching filter / ao format!\n");
    }
    break;
#ifdef HAVE_MP3LAME
case ACODEC_VBRMP3:
    printf("MP3 audio selected\n");
    mux_a->h.dwSampleSize=0; // VBR
    mux_a->h.dwRate=force_srate?force_srate:sh_audio->samplerate;
    mux_a->h.dwScale=(mux_a->h.dwRate<32000)?576:1152; // samples/frame
    if(sizeof(MPEGLAYER3WAVEFORMAT)!=30) mp_msg(MSGT_MENCODER,MSGL_WARN,"sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, maybe broken C compiler?\n",sizeof(MPEGLAYER3WAVEFORMAT));
    mux_a->wf=malloc(sizeof(MPEGLAYER3WAVEFORMAT)); // should be 30
    mux_a->wf->wFormatTag=0x55; // MP3
    mux_a->wf->nChannels= (lame_param_mode<0) ? sh_audio->channels :
	((lame_param_mode==3) ? 1 : 2);
    mux_a->wf->nSamplesPerSec=mux_a->h.dwRate;
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
    // setup filter:
    if(!init_audio_filters(sh_audio, 
        sh_audio->samplerate,
	sh_audio->channels, sh_audio->sample_format, sh_audio->samplesize,
	mux_a->wf->nSamplesPerSec, mux_a->wf->nChannels,
#ifdef WORDS_BIGENDIAN
	AFMT_S16_BE, 2,
#else
	AFMT_S16_LE, 2,
#endif
	4608, mux_a->h.dwRate*mux_a->wf->nChannels*2)){
      mp_msg(MSGT_CPLAYER,MSGL_ERR,"Couldn't find matching filter / ao format!\n");
    }
    break;
#endif
}

if (verbose>1) print_wave_header(mux_a->wf);

if(audio_delay!=0.0){
    mux_a->h.dwStart=audio_delay*mux_a->h.dwRate/mux_a->h.dwScale;
    printf("Setting AUDIO DELAY to %5.3f\n",mux_a->h.dwStart*mux_a->h.dwScale/(float)mux_a->h.dwRate);
}

} // if(sh_audio)

printf(MSGTR_WritingAVIHeader);
muxer_write_header(muxer);

decoded_frameno=0;

if(sh_audio)
switch(mux_a->codec){
#ifdef HAVE_MP3LAME
case ACODEC_VBRMP3:

lame=lame_init();
lame_set_bWriteVbrTag(lame,0);
lame_set_in_samplerate(lame,mux_a->wf->nSamplesPerSec);
//lame_set_in_samplerate(lame,sh_audio->samplerate); // if resampling done by lame
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
#if HAVE_MP3LAME >= 392
if(lame_param_preset != NULL){
  printf ("\npreset=%s\n\n",lame_param_preset);
  lame_presets_set(lame,lame_param_fast, (lame_param_vbr==0), lame_param_preset);
}
#endif
lame_init_params(lame);
if(verbose>0){
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
//  there is 2 way to handle the -ss option in 3-pass mode:
// > 1. do the first pass for the whole file, and use -ss for 2nd/3rd pases only
// > 2. do all the 3 passes with the same -ss value
//  this line enables behaviour 1. (and kills 2. at the same time):
//    if(demuxer2) demux_seek(demuxer2, d, 1);
}

if (out_file_format == MUXER_TYPE_MPEG)
	{
	if (audio_preload > 0.4) {
	  fprintf(stderr,"Limiting audio preload to 0.4s\n");
	  audio_preload = 0.4;
	}
	if (audio_density < 4) {
	  fprintf(stderr,"Increasing audio density to 4\n");
	  audio_density = 4;
	}
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
	muxer_write_chunk(mux_a,len,0x10);
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

    v_timer_corr-=frame_time-(float)mux_v->h.dwScale/mux_v->h.dwRate;

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

ptimer_start = GetTimerMS();

switch(mux_v->codec){
case VCODEC_COPY:
    mux_v->buffer=start;
    if(skip_flag<=0) muxer_write_chunk(mux_v,in_size,(sh_video->ds->flags&1)?0x10:0);
    break;
case VCODEC_FRAMENO:
    mux_v->buffer=(unsigned char *)&decoded_frameno; // tricky
    if(skip_flag<=0) muxer_write_chunk(mux_v,sizeof(int),0x10);
    break;
default:
    // decode_video will callback down to ve_*.c encoders, through the video filters
    blit_frame=decode_video(sh_video,start,in_size,(skip_flag>0)?1:0);
    if(!blit_frame){
      badframes++;
      if(skip_flag<=0){
	// unwanted skipping of a frame, what to do?
	if(skip_limit==0){
	    // skipping not allowed -> write empty frame:
	    muxer_write_chunk(mux_v,0,0);
	} else {
	    // skipping allowed -> skip it and distriubute timer error:
	    v_timer_corr-=(float)mux_v->h.dwScale/mux_v->h.dwRate;
	}
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
	muxer_write_chunk(mux_v,0,0);
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
#if 0
    if(pts_from_bps){
        unsigned int samples=(sh_audio->audio.dwSampleSize)?
          ((ds_tell(d_audio)-sh_audio->a_in_buffer_len)/sh_audio->audio.dwSampleSize) :
          (d_audio->block_no); // <- used for VBR audio
//	printf("samples=%d  \n",samples);
        a_pts=samples*(float)sh_audio->audio.dwScale/(float)sh_audio->audio.dwRate;
      delay_corrected=1;
    } else 
#endif
    {
      // PTS = (last timestamp) + (bytes after last timestamp)/(bytes per sec)
      a_pts=d_audio->pts;
      if(!delay_corrected) if(a_pts) delay_corrected=1;
      //printf("*** %5.3f ***\n",a_pts);
      a_pts+=(ds_tell_pts(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    }
    v_pts=sh_video ? sh_video->pts : d_video->pts;
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
#if 0
	if(!len && sh_audio && sh_audio->audio.dwLength>100){
	    p=(sh_audio->audio.dwSampleSize? ds_tell(sh_audio->ds)/sh_audio->audio.dwSampleSize : sh_audio->ds->block_no)
	     / (float)(sh_audio->audio.dwLength);
	}
#endif
#if 0
	mp_msg(MSGT_AVSYNC,MSGL_STATUS,"%d < %d < %d  \r",
	    (int)demuxer->movi_start,
	    (int)demuxer->filepos,
	    (int)demuxer->movi_end);
#else
	if(verbose>0) {
		mp_msg(MSGT_AVSYNC,MSGL_STATUS,"Pos:%6.1fs %6df (%2d%%) %3dfps Trem:%4dmin %3dmb  A-V:%5.3f [%d:%d] A/Vms %d/%d D/B/S %d/%d/%d \r",
	    	mux_v->timer, decoded_frameno, (int)(p*100),
	    	(t>1) ? (int)(decoded_frameno/t+0.5) : 0,
	    	(p>0.001) ? (int)((t/p-t)/60) : 0, 
	    	(p>0.001) ? (int)(ftello(muxer_f)/p/1024/1024) : 0,
	    	v_pts_corr,
	    	(mux_v->timer>1) ? (int)(mux_v->size/mux_v->timer/125) : 0,
	    	(mux_a && mux_a->timer>1) ? (int)(mux_a->size/mux_a->timer/125) : 0,
			audiorate/audiosamples, videorate/videosamples,
			duplicatedframes, badframes, skippedframes
		);
	} else
	mp_msg(MSGT_AVSYNC,MSGL_STATUS,"Pos:%6.1fs %6df (%2d%%) %3dfps Trem:%4dmin %3dmb  A-V:%5.3f [%d:%d]\r",
	    mux_v->timer, decoded_frameno, (int)(p*100),
	    (t>1) ? (int)(decoded_frameno/t+0.5) : 0,
	    (p>0.001) ? (int)((t/p-t)/60) : 0, 
	    (p>0.001) ? (int)(ftello(muxer_f)/p/1024/1024) : 0,
	    v_pts_corr,
	    (mux_v->timer>1) ? (int)(mux_v->size/mux_v->timer/125) : 0,
	    (mux_a && mux_a->timer>1) ? (int)(mux_a->size/mux_a->timer/125) : 0
	);
#endif
    }
        fflush(stdout);

#ifdef USE_SUB
  // find sub
  if(subtitles && sh_video->pts>0){
      float pts=sh_video->pts;
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
	 mp_msg(MSGT_MENCODER,MSGL_V,"\rDVD sub: len=%d  v_pts=%5.3f  s_pts=%5.3f  \n",len,sh_video->pts,d_dvdsub->pts);
	 if (vo_spudec)
	 spudec_assemble(vo_spudec,packet,len,90000*d_dvdsub->pts);
	 if (vobsub_writer)
	     vobsub_out_output(vobsub_writer,packet,len,mux_v->timer + d_dvdsub->pts - sh_video->pts);
     }
     if (vo_spudec) {
     spudec_heartbeat(vo_spudec,90000*sh_video->pts);
     vo_osd_changed(OSDTYPE_SPU);
     }
 }
#endif

 if(ferror(muxer_f)) {
     mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_ErrorWritingFile, out_filename);
     mencoder_exit(1, NULL);
 }

} // while(!at_eof)

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
    printf("\n\nCBR audio: %ld bytes/sec, %d bytes/block\n",
	    mux_a->h.dwRate,((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nBlockSize);
}
#endif

printf(MSGTR_WritingAVIIndex);
muxer_write_index(muxer);
muxer_f_size=ftello(muxer_f);
printf(MSGTR_FixupAVIHeader);
fseek(muxer_f,0,SEEK_SET);
muxer_write_header(muxer); // update header
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
    printf(MSGTR_RecommendedVideoBitrate,"2 x 650MB",(int)((2*650*1024*1024-muxer_f_size)/mux_v->timer/125));
    printf(MSGTR_RecommendedVideoBitrate,"2 x 700MB",(int)((2*700*1024*1024-muxer_f_size)/mux_v->timer/125));
    printf(MSGTR_RecommendedVideoBitrate,"2 x 800MB",(int)((2*800*1024*1024-muxer_f_size)/mux_v->timer/125));
}

printf(MSGTR_VideoStreamResult,
    (float)(mux_v->size/mux_v->timer*8.0f/1000.0f), (int)(mux_v->size/mux_v->timer), (int)mux_v->size, (float)mux_v->timer, decoded_frameno);
if(sh_audio)
printf(MSGTR_AudioStreamResult,
    (float)(mux_a->size/mux_a->timer*8.0f/1000.0f), (int)(mux_a->size/mux_a->timer), (int)mux_a->size, (float)mux_a->timer);

if(sh_video){ uninit_video(sh_video);sh_video=NULL; }
if(demuxer) free_demuxer(demuxer);
if(stream) free_stream(stream); // kill cache thread

return interrupted;
}

static int parse_end_at(struct config *conf, const char* param)
{

    end_at_type = END_AT_NONE;
    
    /* End at size parsing */
    {
        char unit[4];
        
        end_at_type = END_AT_SIZE;

        if(sscanf(param, "%lf%3s", &end_at, unit) == 2) {
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

#if 0
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
#endif

#if HAVE_MP3LAME >= 392
/* lame_presets_set 
   taken out of presets_set in lame-3.93.1/frontend/parse.c and modified */
static int  lame_presets_set( lame_t gfp, int fast, int cbr, const char* preset_name )
{
    int mono = 0;

    if (strcmp(preset_name, "help") == 0) {
        fprintf(stdout, "LAME version %s (%s)\n\n", get_lame_version(), get_lame_url());
        lame_presets_longinfo_dm( stdout );
        return -1;
    }



    //aliases for compatibility with old presets

    if (strcmp(preset_name, "phone") == 0) {
        preset_name = "16";
        mono = 1;
    }
    if ( (strcmp(preset_name, "phon+") == 0) ||
         (strcmp(preset_name, "lw") == 0) ||
         (strcmp(preset_name, "mw-eu") == 0) ||
         (strcmp(preset_name, "sw") == 0)) {
        preset_name = "24";
        mono = 1;
    }
    if (strcmp(preset_name, "mw-us") == 0) {
        preset_name = "40";
        mono = 1;
    }
    if (strcmp(preset_name, "voice") == 0) {
        preset_name = "56";
        mono = 1;
    }
    if (strcmp(preset_name, "fm") == 0) {
        preset_name = "112";
    }
    if ( (strcmp(preset_name, "radio") == 0) ||
         (strcmp(preset_name, "tape") == 0)) {
        preset_name = "112";
    }
    if (strcmp(preset_name, "hifi") == 0) {
        preset_name = "160";
    }
    if (strcmp(preset_name, "cd") == 0) {
        preset_name = "192";
    }
    if (strcmp(preset_name, "studio") == 0) {
        preset_name = "256";
    }

#if HAVE_MP3LAME >= 393
    if (strcmp(preset_name, "medium") == 0) {

        if (fast > 0)
           lame_set_preset(gfp, MEDIUM_FAST);
        else
           lame_set_preset(gfp, MEDIUM);

        return 0;
    }
#endif
    
    if (strcmp(preset_name, "standard") == 0) {

        if (fast > 0)
           lame_set_preset(gfp, STANDARD_FAST);
        else
           lame_set_preset(gfp, STANDARD);

        return 0;
    }
    
    else if (strcmp(preset_name, "extreme") == 0){

        if (fast > 0)
           lame_set_preset(gfp, EXTREME_FAST);
        else
           lame_set_preset(gfp, EXTREME);

        return 0;
    }
    					
    else if (((strcmp(preset_name, "insane") == 0) || 
              (strcmp(preset_name, "320"   ) == 0))   && (fast < 1)) {

        lame_set_preset(gfp, INSANE);
 
        return 0;
    }

    // Generic ABR Preset
    if (((atoi(preset_name)) > 0) &&  (fast < 1)) {
        if ((atoi(preset_name)) >= 8 && (atoi(preset_name)) <= 320){
            lame_set_preset(gfp, atoi(preset_name));

            if (cbr == 1 )
                lame_set_VBR(gfp, vbr_off);

            if (mono == 1 ) {
                lame_set_mode(gfp, MONO);
            }

            return 0;

        }
        else {
            fprintf(stderr, "LAME version %s (%s)\n\n", get_lame_version(), get_lame_url());
            fprintf(stderr,"Error: The bitrate specified is out of the valid range for this preset\n"
                           "\n"
                           "When using this mode you must enter a value between \"8\" and \"320\"\n"
                           "\n"
                           "For further information try: \"-lameopts preset=help\"\n"                  
                   );
            return -1;
        }
    }



    fprintf(stderr, "LAME version %s (%s)\n\n", get_lame_version(), get_lame_url());
    fprintf(stderr,"Error: You did not enter a valid profile and/or options with preset\n"
                   "\n"
                   "Available profiles are:\n"
                   "\n"
                   "   <fast>        standard\n"
                   "   <fast>        extreme\n"
                   "                 insane\n"
                   "   <cbr> (ABR Mode) - The ABR Mode is implied. To use it,\n"
                   "                      simply specify a bitrate. For example:\n"
                   "                      \"preset=185\" activates this\n"
                   "                      preset and uses 185 as an average kbps.\n" 
                   "\n"
                   "    Some examples:\n"
                   "\n"
   		   "    \"-lameopts fast:preset=standard  \"\n"
   		   " or \"-lameopts  cbr:preset=192       \"\n"
   		   " or \"-lameopts      preset=172       \"\n"
   		   " or \"-lameopts      preset=extreme   \"\n"
                   "\n"
                   "For further information try: \"-lameopts preset=help\"\n"                  
           );
    mencoder_exit(1, "error parsing cmdline");
}
#endif

#if HAVE_MP3LAME >= 392
/* lame_presets_longinfo_dm
   taken out of presets_longinfo_dm in lame-3.93.1/frontend/parse.c and modified */
static void  lame_presets_longinfo_dm ( FILE* msgfp )
{
        fprintf ( msgfp,
        "\n" 
        "The preset switches are designed to provide the highest possible quality.\n"
        "\n"
        "They have for the most part been subject to and tuned via rigorous double blind\n"
        "listening tests to verify and achieve this objective.\n"
        "\n"
        "These are continually updated to coincide with the latest developments that\n"
        "occur and as a result should provide you with nearly the best quality\n"
        "currently possible from LAME.\n"
        "\n"
        "To activate these presets:\n"
        "\n"
        "   For VBR modes (generally highest quality):\n"
        "\n"
        "     \"preset=standard\" This preset should generally be transparent\n"
        "                             to most people on most music and is already\n"
        "                             quite high in quality.\n"
        "\n"
        "     \"preset=extreme\" If you have extremely good hearing and similar\n"
        "                             equipment, this preset will generally provide\n"
        "                             slightly higher quality than the \"standard\"\n"
        "                             mode.\n"
        "\n"
        "   For CBR 320kbps (highest quality possible from the preset switches):\n"
        "\n"
        "     \"preset=insane\"  This preset will usually be overkill for most\n"
        "                             people and most situations, but if you must\n"
        "                             have the absolute highest quality with no\n"
        "                             regard to filesize, this is the way to go.\n"
        "\n"
        "   For ABR modes (high quality per given bitrate but not as high as VBR):\n"
        "\n"
        "     \"preset=<kbps>\"  Using this preset will usually give you good\n"
        "                             quality at a specified bitrate. Depending on the\n"
        "                             bitrate entered, this preset will determine the\n"
        "                             optimal settings for that particular situation.\n"
        "                             While this approach works, it is not nearly as\n"
        "                             flexible as VBR, and usually will not attain the\n"
        "                             same level of quality as VBR at higher bitrates.\n"  
        "\n"
        "The following options are also available for the corresponding profiles:\n"
        "\n"
        "   <fast>        standard\n"
        "   <fast>        extreme\n"
        "                 insane\n"
        "   <cbr> (ABR Mode) - The ABR Mode is implied. To use it,\n"
        "                      simply specify a bitrate. For example:\n"
        "                      \"preset=185\" activates this\n"
        "                      preset and uses 185 as an average kbps.\n" 
        "\n"
        "   \"fast\" - Enables the new fast VBR for a particular profile. The\n"
        "            disadvantage to the speed switch is that often times the\n"
        "            bitrate will be slightly higher than with the normal mode\n"
        "            and quality may be slightly lower also.\n"
	"   Warning: with the current version fast presets might result in too\n"
	"            high bitrate compared to regular presets.\n"
        "\n"
        "   \"cbr\"  - If you use the ABR mode (read above) with a significant\n"
        "            bitrate such as 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"
        "            you can use the \"cbr\" option to force CBR mode encoding\n"
        "            instead of the standard abr mode. ABR does provide higher\n"
        "            quality but CBR may be useful in situations such as when\n"
        "            streaming an mp3 over the internet may be important.\n"
        "\n"
        "    For example:\n"
        "\n"
        "    \"-lameopts fast:preset=standard  \"\n"
        " or \"-lameopts  cbr:preset=192       \"\n"
        " or \"-lameopts      preset=172       \"\n"
        " or \"-lameopts      preset=extreme   \"\n"
        "\n"
        "\n"
        "A few aliases are available for ABR mode:\n"
        "phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"
        "mw-us => 40kbps/mono        voice => 56kbps/mono\n"
        "fm/radio/tape => 112kbps    hifi => 160kbps\n"
        "cd => 192kbps               studio => 256kbps");
	mencoder_exit(0, NULL);
}
#endif
