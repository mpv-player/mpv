// AVI & MPEG Player    v0.11   (C) 2000-2001. by A'rpi/ESP-team

// Enable ALSA emulation (using 32kB audio buffer) - timer testing only
//#define SIMULATE_ALSA

#ifdef USE_XMMP_AUDIO
#define OUTBURST 4096
#else
//#define OUTBURST 1024
#define OUTBURST 512
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <linux/cdrom.h>

#include "version.h"
#include "config.h"

#include "cfgparser.h"
#include "cfg-mplayer-def.h"

#include "subreader.h"

#include "libvo/video_out.h"
#include "libvo/sub.h"

// CODECS:
#include "mp3lib/mp3.h"
#include "libac3/ac3.h"
#include "libmpeg2/mpeg2.h"
#include "libmpeg2/mpeg2_internal.h"

#include "loader.h"
#include "wine/avifmt.h"

#include "codec-cfg.h"

#include "dvdauth.h"

#ifdef USE_DIRECTSHOW
#include "DirectShow/DS_VideoDec.h"
#include "DirectShow/DS_AudioDec.h"
#endif

#include "opendivx/decore.h"


#ifdef USE_XMMP_AUDIO
#include "libxmm/xmmp.h"
#include "libxmm/libxmm.h"
XMM xmm;
XMM_PluginSound *pSound=NULL;
#endif

extern int vo_screenwidth;

extern char* win32_codec_name;  // must be set before calling DrvOpen() !!!

extern int errno;

#include "linux/getch2.h"
#include "linux/keycodes.h"
#include "linux/timer.h"
#include "linux/shmem.h"

#ifdef HAVE_LIRC
#include "lirc_mp.h"
#endif

#include "help_mp.h"

#define DEBUG if(0)
int verbose=0;

#define ABS(x) (((x)>=0)?(x):(-(x)))

//**************************************************************************//
//             .SUB 
//**************************************************************************//

static current_sub=0;

static subtitle* subtitles=NULL;
static int nosub_range_start=-1;
static int nosub_range_end=-1;

void find_sub(int key){
    int i,j;
    if(vo_sub){
      if(key>=vo_sub->start && key<=vo_sub->end) return; // OK!
    } else {
      if(key>nosub_range_start && key<nosub_range_end) return; // OK!
    }
    // sub changed!
    
//    printf("\r---- sub changed ----\n");
    
    // check next sub.
    if(current_sub>=0 && current_sub+1<sub_num){
      if(key>subtitles[current_sub].end && key<subtitles[current_sub+1].start){
          // no sub
          nosub_range_start=subtitles[current_sub].end;
          nosub_range_end=subtitles[current_sub+1].start;
          vo_sub=NULL;
          return;
      }
      // next sub?
      ++current_sub;
      vo_sub=&subtitles[current_sub];
      if(key>=vo_sub->start && key<=vo_sub->end) return; // OK!
    }

//    printf("\r---- sub log search... ----\n");
    
    // use logarithmic search:
    i=0;j=sub_num-1;
//    printf("Searching %d in %d..%d\n",key,subtitles[i].start,subtitles[j].end);
    while(j>=i){
        current_sub=(i+j+1)/2;
        vo_sub=&subtitles[current_sub];
        if(key<vo_sub->start) j=current_sub-1;
        else if(key>vo_sub->end) i=current_sub+1;
        else return; // found!
    }
//    if(key>=vo_sub->start && key<=vo_sub->end) return; // OK!
    
    // check where are we...
    if(key<vo_sub->start){
      if(current_sub<=0){
          // before the first sub
          nosub_range_start=key-1; // tricky
          nosub_range_end=vo_sub->start;
//          printf("FIRST...  key=%d  end=%d  \n",key,vo_sub->start);
          vo_sub=NULL;
          return;
      }
      --current_sub;
      if(key>subtitles[current_sub].end && key<subtitles[current_sub+1].start){
          // no sub
          nosub_range_start=subtitles[current_sub].end;
          nosub_range_end=subtitles[current_sub+1].start;
//          printf("No sub... 1 \n");
          vo_sub=NULL;
          return;
      }
      printf("HEH????  ");
    } else {
      if(key<=vo_sub->end) printf("JAJJ!  "); else
      if(current_sub+1>=sub_num){
          // at the end?
          nosub_range_start=vo_sub->end;
          nosub_range_end=0x7FFFFFFF; // MAXINT
//          printf("END!?\n");
          vo_sub=NULL;
          return;
      } else
      if(key>subtitles[current_sub].end && key<subtitles[current_sub+1].start){
          // no sub
          nosub_range_start=subtitles[current_sub].end;
          nosub_range_end=subtitles[current_sub+1].start;
//          printf("No sub... 2 \n");
          vo_sub=NULL;
          return;
      }
    }
    
    printf("SUB ERROR:  %d  ?  %d --- %d  [%d]  \n",key,vo_sub->start,vo_sub->end,current_sub);

    vo_sub=NULL; // no sub here
}

//**************************************************************************//
//             Config file
//**************************************************************************//

static int cfg_inc_verbose(struct config *conf){
    ++verbose;
    return 0;
}

static int cfg_include(struct config *conf, char *filename){
	return parse_config_file(conf, filename);
}

char *get_path(char *filename){
	char *homedir;
	char *buff;
	static char *config_dir = "/.mplayer";
	int len;

	if ((homedir = getenv("HOME")) == NULL)
		return NULL;
	len = strlen(homedir) + strlen(config_dir) + 1;
	if (filename == NULL) {
		if ((buff = (char *) malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s", homedir, config_dir);
	} else {
		len += strlen(filename) + 1;
		if ((buff = (char *) malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s/%s", homedir, config_dir, filename);
	}
	return buff;
}

static int max_framesize=0;

//static int dbg_es_sent=0;
//static int dbg_es_rcvd=0;

//static int show_packets=0;

//**************************************************************************//
//**************************************************************************//
//             Input media streaming & demultiplexer:
//**************************************************************************//

#include "stream.c"
#include "demuxer.c"

#include "stheader.h"

typedef struct {
  // file:
  MainAVIHeader avih;
  unsigned int movi_start;
  unsigned int movi_end;
  // index:
  AVIINDEXENTRY* idx;
  int idx_size;
  int idx_pos;
  int idx_pos_a;
  int idx_pos_v;
  int idx_offset;  // ennyit kell hozzaadni az index offset ertekekhez
  // streams:
  sh_audio_t* a_streams[256];
  sh_video_t* v_streams[256];
  // video:
  unsigned int bitrate;
} avi_header_t;

avi_header_t avi_header;

#include "aviprint.c"

sh_audio_t* new_sh_audio(int id){
    if(avi_header.a_streams[id]){
        printf("Warning! Audio stream header %d redefined!\n",id);
    } else {
        printf("==> Found audio stream: %d\n",id);
        avi_header.a_streams[id]=malloc(sizeof(sh_audio_t));
        memset(avi_header.a_streams[id],0,sizeof(sh_audio_t));
    }
    return avi_header.a_streams[id];
}

sh_video_t* new_sh_video(int id){
    if(avi_header.v_streams[id]){
        printf("Warning! video stream header %d redefined!\n",id);
    } else {
        printf("==> Found video stream: %d\n",id);
        avi_header.v_streams[id]=malloc(sizeof(sh_video_t));
        memset(avi_header.v_streams[id],0,sizeof(sh_video_t));
    }
    return avi_header.v_streams[id];
}


#include "demux_avi.c"
#include "demux_mpg.c"

demuxer_t *demuxer=NULL;
demux_stream_t *d_audio=NULL;
demux_stream_t *d_video=NULL;

sh_audio_t *sh_audio=NULL;//&sh_audio_i;
sh_video_t *sh_video=NULL;//&sh_video_i;

char* encode_name=NULL;
char* encode_index_name=NULL;
int encode_bitrate=0;

// MPEG video stream parser:
#include "parse_es.c"

extern picture_t *picture;

static const int frameratecode2framerate[16] = {
   0, 24000*10000/1001, 24*10000,25*10000, 30000*10000/1001, 30*10000,50*10000,60000*10000/1001,
  60*10000, 0,0,0,0,0,0,0
};

//**************************************************************************//
//             Audio codecs:
//**************************************************************************//

// MP3 decoder buffer callback:
int mplayer_audio_read(char *buf,int size){
  int len;
  len=demux_read_data(sh_audio->ds,buf,size);
  return len;
}

// AC3 decoder buffer callback:
static void ac3_fill_buffer(uint8_t **start,uint8_t **end){
    int len=ds_get_packet(sh_audio->ds,(char**)start);
    //printf("<ac3:%d>\n",len);
    if(len<0)
          *start = *end = NULL;
    else
          *end = *start + len;
}

#include "alaw.c"
#include "xa/xa_gsm.h"

#include "dec_audio.c"

//**************************************************************************//
//             The OpenDivX stuff:
//**************************************************************************//

unsigned char *opendivx_src[3];
int opendivx_stride[3];

// callback, the opendivx decoder calls this for each frame:
void convert_linux(unsigned char *puc_y, int stride_y,
	unsigned char *puc_u, unsigned char *puc_v, int stride_uv,
	unsigned char *bmp, int width_y, int height_y){

//    printf("convert_yuv called  %dx%d  stride: %d,%d\n",width_y,height_y,stride_y,stride_uv);

    opendivx_src[0]=puc_y;
    opendivx_src[1]=puc_u;
    opendivx_src[2]=puc_v;
    
    opendivx_stride[0]=stride_y;
    opendivx_stride[1]=stride_uv;
    opendivx_stride[2]=stride_uv;
}

//**************************************************************************//

#ifdef SIMULATE_ALSA
// Simulate ALSA buffering on OSS device :)  (for testing...)

#define fake_ALSA_size 32768
char fake_ALSA_buffer[fake_ALSA_size];
int fake_ALSA_len=0;

void fake_ALSA_write(int audio_fd,char* a_buffer,int len){
while(len>0){
  int x=fake_ALSA_size-fake_ALSA_len;
  if(x>0){
    if(x>len) x=len;
    memcpy(&fake_ALSA_buffer[fake_ALSA_len],a_buffer,x);
    fake_ALSA_len+=x;len-=x;
  }
  if(fake_ALSA_len>=fake_ALSA_size){
    write(audio_fd,fake_ALSA_buffer,fake_ALSA_len);
    fake_ALSA_len=0;
  }
}
}
#endif
//**************************************************************************//

// AVI file header reader/parser/writer:
#include "aviheader.c"
#include "aviwrite.c"

// ASF headers:
#include "asfheader.c"
#include "demux_asf.c"

// DLL codecs init routines
#include "dll_init.c"

// Common FIFO functions, and keyboard/event FIFO code
#include "fifo.c"

//**************************************************************************//

static vo_functions_t *video_out=NULL;

static int play_in_bg=0;

void exit_player(char* how){
  if(how) printf("\nExiting... (%s)\n",how);
  if(verbose) printf("max framesize was %d bytes\n",max_framesize);
  // restore terminal:
  getch2_disable();
  video_out->uninit();
#ifdef USE_XMMP_AUDIO
  if(verbose) printf("XMM: closing audio driver...\n");
  if(pSound){
    pSound->Exit( pSound );
    xmm_Exit( &xmm );
  }
#endif
  if(encode_name) avi_fixate();
#ifdef HAVE_LIRC
  lirc_mp_cleanup();
#endif
  //if(play_in_bg) system("xsetroot -solid \\#000000");
  exit(1);
}

static char* current_module=NULL; // for debugging

void exit_sighandler(int x){
  static int sig_count=0;
  ++sig_count;
  if(sig_count==2) exit(1);
  if(sig_count>2){
    // can't stop :(
    kill(getpid(),SIGKILL);
  }
  printf("\nMPlayer interrupted by signal %d in module: %s \n",x,
      current_module?current_module:"unknown"
  );
  exit_player(NULL);
}

int divx_quality=0;
extern int vo_dbpp;

int main(int argc,char* argv[], char *envp[]){
char* filename=NULL; //"MI2-Trailer.avi";
int i;
int seek_to_sec=0;
int seek_to_byte=0;
int f; // filedes
int stream_type;
stream_t* stream=NULL;
int file_format=DEMUXER_TYPE_UNKNOWN;
int has_audio=1;
//int has_video=1;
//
int audio_format=0; // override
#ifdef ALSA_TIMER
int alsa=1;
#else
int alsa=0;
#endif
int audio_buffer_size=-1;
int audio_id=-1;
int video_id=-1;
float default_max_pts_correction=0.01f;
int delay_corrected=1;
float force_fps=0;
float audio_delay=0;
int vcd_track=0;
#ifdef VCD_CACHE
int vcd_cache_size=128;
#endif
int no_index=0;
#ifdef AVI_SYNC_BPS
int pts_from_bps=1;
#else
int pts_from_bps=0;
#endif
char* title="MPlayer";
// screen info:
char* video_driver=NULL; //"mga"; // default
int fullscreen=0;
int vidmode=0;
int softzoom=0;
int screen_size_x=0;//SCREEN_SIZE_X;
int screen_size_y=0;//SCREEN_SIZE_Y;
int screen_size_xy=0;
// movie info:
int out_fmt=0;
char *dsp="/dev/dsp";
int force_ni=0;
char *conffile;
int conffile_fd;
char *font_name=NULL;
float font_factor=0.75;
char *sub_name=NULL;
float sub_delay=0;
float sub_fps=0;
int   sub_auto = 1;
//int user_bpp=0;

#include "mixer.h"
#include "cfg-mplayer.h"

  printf("%s",banner_text);

if (parse_config_file(conf, "/etc/mplayer.conf") < 0)
  exit(1);
if ((conffile = get_path("")) == NULL) {
  printf("Can't find HOME dir\n");
} else {
  mkdir(conffile, 0777);
  free(conffile);
  if ((conffile = get_path("config")) == NULL) {
    printf("get_path(\"config\") sziiiivas\n");
  } else {
    if ((conffile_fd = open(conffile, O_CREAT | O_EXCL | O_WRONLY, 0666)) != -1) {
      printf("Creating config file: %s\n", conffile);
      write(conffile_fd, default_config, strlen(default_config));
      close(conffile_fd);
    }
    if (parse_config_file(conf, conffile) < 0)
      exit(1);
    free(conffile);
  }
}
if (parse_command_line(conf, argc, argv, envp, &filename) < 0)
  exit(1);

// Many users forget to include command line in bugreports...
if(verbose){
  printf("CommandLine:");
  for(i=1;i<argc;i++)printf(" '%s'",argv[i]);
  printf("\n");
}

if(video_driver && strcmp(video_driver,"help")==0){
  printf("Available video output drivers:\n");
  i=0;
  while (video_out_drivers[i]) {
    const vo_info_t *info = video_out_drivers[i++]->get_info ();
  	printf("\t%s\t%s\n", info->short_name, info->name);
  }
  printf("\n");
  exit(0);
}

if(!filename){
  if(vcd_track) filename="/dev/cdrom"; 
  else {
    printf("%s",help_text); exit(0);
  }
}

// check video_out driver name:
  if(!video_driver)
    video_out=video_out_drivers[0];
  else
  for (i=0; video_out_drivers[i] != NULL; i++){
    const vo_info_t *info = video_out_drivers[i]->get_info ();
    if(strcmp(info->short_name,video_driver) == 0){
      video_out = video_out_drivers[i];break;
    }
  }
  if(!video_out){
    printf("Invalid video output driver name: %s\n",video_driver);
    return 0;
  }

// check codec.conf
if(!parse_codec_cfg(get_path("codecs.conf"))){
    printf("(copy/link DOCS/codecs.conf to ~/.mplayer/codecs.conf)\n");
    exit(1);
}

// check font
  if(font_name){
       vo_font=read_font_desc(font_name,font_factor,verbose>1);
       if(!vo_font) printf("Can't load font: %s\n",font_name);
  } else {
      // try default:
       vo_font=read_font_desc(get_path("font/font.desc"),font_factor,verbose>1);
  }

// check .sub
  if(sub_name){
       subtitles=sub_read_file(sub_name);
       if(!subtitles) printf("Can't load subtitles: %s\n",font_name);
  } else {
    if ( sub_auto )
      {
       // auto load sub file ...
       subtitles=sub_read_file( sub_filename( filename ) );
       if ( subtitles == NULL ) subtitles=sub_read_file(get_path("default.sub")); // try default:
      } else subtitles=sub_read_file(get_path("default.sub")); // try default:
  }


if(vcd_track){
//============ Open VideoCD track ==============
  f=open(filename,O_RDONLY);
  if(f<0){ printf("CD-ROM Device '%s' not found!\n",filename);return 1; }
  vcd_read_toc(f);
  if(!vcd_seek_to_track(f,vcd_track)){ printf("Error selecting VCD track!\n");return 1;}
  seek_to_byte+=VCD_SECTOR_DATA*vcd_get_msf();
  if(verbose) printf("VCD start byte position: 0x%X\n",seek_to_byte);
  stream_type=STREAMTYPE_VCD;
#ifdef VCD_CACHE
  vcd_cache_init(vcd_cache_size);
#endif
} else {
//============ Open plain FILE ============
  f=open(filename,O_RDONLY);
  if(f<0){ printf("File not found: '%s'\n",filename);return 1; }
#ifdef HAVE_LIBCSS
  if (dvd_device) {
    if (dvd_auth(dvd_device,f)) exit(0);
    printf("DVD auth sequence seems to be OK.\n");
  }
#endif
  stream_type=STREAMTYPE_FILE;
}

stream=new_stream(f,stream_type);

//============ Open & Sync stream and detect file format ===============

if(!has_audio) audio_id=-2; // do NOT read audio packets...

//=============== Try to open as AVI file: =================
stream_reset(stream);
demuxer=new_demuxer(stream,DEMUXER_TYPE_AVI,audio_id,video_id);
stream_seek(demuxer->stream,seek_to_byte);
{ //---- RIFF header:
  int id=stream_read_dword_le(demuxer->stream); // "RIFF"
  if(id==mmioFOURCC('R','I','F','F')){
    stream_read_dword_le(demuxer->stream); //filesize
    id=stream_read_dword_le(demuxer->stream); // "AVI "
    if(id==formtypeAVI){ 
      printf("Detected AVI file format!\n");
      file_format=DEMUXER_TYPE_AVI;
    }
  }
}
//=============== Try to open as ASF file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN){
  stream_reset(stream);
  demuxer=new_demuxer(stream,DEMUXER_TYPE_ASF,audio_id,video_id);
  stream_seek(demuxer->stream,seek_to_byte);
  if(asf_check_header()){
      printf("Detected ASF file format!\n");
      file_format=DEMUXER_TYPE_ASF;
  }
}
//=============== Try to open as MPEG-PS file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN){
  stream_reset(stream);
  demuxer=new_demuxer(stream,DEMUXER_TYPE_MPEG_PS,audio_id,video_id);
  stream_seek(demuxer->stream,seek_to_byte);
  if(audio_format) demuxer->audio->type=audio_format; // override audio format
  if(ds_fill_buffer(demuxer->video)){
    printf("Detected MPEG-PS file format!\n");
    file_format=DEMUXER_TYPE_MPEG_PS;
  } else {
    // some hack to get meaningfull error messages to our unhappy users:
//    if(num_elementary_packets100>16 &&
//       abs(num_elementary_packets101-num_elementary_packets100)<8){
    if(num_elementary_packets100>=2 && num_elementary_packets101>=2 &&
       abs(num_elementary_packets101-num_elementary_packets100)<8){
      file_format=DEMUXER_TYPE_MPEG_ES; //  <-- hack is here :)
    } else {
      if(demuxer->synced==2)
        printf("Missing MPEG video stream!? contact the author, it may be a bug :(\n");
      else
        printf("Not MPEG System Stream format... (maybe Transport Stream?)\n");
    }
  }
}
//=============== Try to open as MPEG-ES file: =================
if(file_format==DEMUXER_TYPE_MPEG_ES){ // little hack, see above!
  stream_reset(stream);
  demuxer=new_demuxer(stream,DEMUXER_TYPE_MPEG_ES,audio_id,video_id);
  stream_seek(demuxer->stream,seek_to_byte);
  if(!ds_fill_buffer(demuxer->video)){
    printf("Invalid MPEG-ES stream??? contact the author, it may be a bug :(\n");
    file_format=DEMUXER_TYPE_UNKNOWN;
  } else {
    printf("Detected MPEG-ES file format!\n");
  }
}
//=============== Unknown, exiting... ===========================
if(file_format==DEMUXER_TYPE_UNKNOWN){
  printf("============= Sorry, this file format not recognized/supported ===============\n");
  printf("=== If this file is an AVI, ASF or MPEG stream, please contact the author! ===\n");
  exit(1);
}
//====== File format recognized, set up these for compatibility: =========
d_audio=demuxer->audio;
d_video=demuxer->video;
//d_audio->sh=sh_audio; 
//d_video->sh=sh_video; 
//sh_audio=d_audio->sh;sh_audio->ds=d_audio;
//sh_video=d_video->sh;sh_video->ds=d_video;

switch(file_format){
 case DEMUXER_TYPE_AVI: {
  //---- AVI header:
  read_avi_header(no_index);
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream,avi_header.movi_start);
  avi_header.idx_pos=0;
  avi_header.idx_pos_a=0;
  avi_header.idx_pos_v=0;
  if(avi_header.idx_size>0){
    // decide index format:
    if(avi_header.idx[0].dwChunkOffset<avi_header.movi_start)
      avi_header.idx_offset=avi_header.movi_start-4;
    else
      avi_header.idx_offset=0;
    if(verbose) printf("AVI index offset: %d\n",avi_header.idx_offset);
  }
  demuxer->endpos=avi_header.movi_end;
  
  if(avi_header.idx_size>0){
      // check that file is non-interleaved:
      int i;
      int a_pos=-1;
      int v_pos=-1;
      for(i=0;i<avi_header.idx_size;i++){
        AVIINDEXENTRY* idx=&avi_header.idx[i];
        demux_stream_t* ds=demux_avi_select_stream(demuxer,idx->ckid);
        int pos=idx->dwChunkOffset+avi_header.idx_offset;
        if(a_pos==-1 && ds==demuxer->audio){
          a_pos=pos;
          if(v_pos!=-1) break;
        }
        if(v_pos==-1 && ds==demuxer->video){
          v_pos=pos;
          if(a_pos!=-1) break;
        }
      }
      if(v_pos==-1){
        printf("AVI_NI: missing video stream!? contact the author, it may be a bug :(\n");
        exit(1);
      }
      if(a_pos==-1){
        printf("AVI_NI: No audio stream found -> nosound\n");
        has_audio=0;
      } else {
        if(force_ni || abs(a_pos-v_pos)>0x100000){  // distance > 1MB
          printf("Detected NON-INTERLEAVED AVI file-format!\n");
          demuxer->type=DEMUXER_TYPE_AVI_NI; // HACK!!!!
	  pts_from_bps=1; // force BPS sync!
        }
      }
  } else {
      // no index
      if(force_ni){
          printf("Using NON-INTERLEAVED Broken AVI file-format!\n");
          demuxer->type=DEMUXER_TYPE_AVI_NINI; // HACK!!!!
	  avi_header.idx_pos_a=
	  avi_header.idx_pos_v=avi_header.movi_start;
	  pts_from_bps=1; // force BPS sync!
      }
  }
  if(!ds_fill_buffer(d_video)){
    printf("AVI: missing video stream!? contact the author, it may be a bug :(\n");
    exit(1);
  }
  sh_video=d_video->sh;sh_video->ds=d_video;
  if(has_audio){
    if(verbose) printf("AVI: Searching for audio stream (id:%d)\n",d_audio->id);
    if(!ds_fill_buffer(d_audio)){
      printf("AVI: No Audio stream found...  ->nosound\n");
      has_audio=0;
      sh_audio=NULL;
    } else {
      sh_audio=d_audio->sh;sh_audio->ds=d_audio;
      sh_audio->format=sh_audio->wf->wFormatTag;
    }
  }
  // calc. FPS:
  sh_video->fps=(float)sh_video->video.dwRate/(float)sh_video->video.dwScale;
  sh_video->frametime=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
  // calculating video bitrate:
  avi_header.bitrate=avi_header.movi_end-avi_header.movi_start-avi_header.idx_size*8;
  if(sh_audio) avi_header.bitrate-=sh_audio->audio.dwLength;
  if(verbose) printf("AVI video length=%d\n",avi_header.bitrate);
  avi_header.bitrate=((float)avi_header.bitrate/(float)sh_video->video.dwLength)*sh_video->fps;
  printf("VIDEO:  [%.4s]  %dx%d  %dbpp  %4.2f fps  %5.1f kbps (%4.1f kbyte/s)\n",
    &sh_video->bih->biCompression,
    sh_video->bih->biWidth,
    sh_video->bih->biHeight,
    sh_video->bih->biBitCount,
    sh_video->fps,
    avi_header.bitrate*0.008f,
    avi_header.bitrate/1024.0f );
  break;
 }
 case DEMUXER_TYPE_ASF: {
  //---- ASF header:
  read_asf_header();
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream,avi_header.movi_start);
  avi_header.idx_pos=0;
  demuxer->endpos=avi_header.movi_end;
  if(!ds_fill_buffer(d_video)){
    printf("ASF: missing video stream!? contact the author, it may be a bug :(\n");
    exit(1);
  }
  sh_video=d_video->sh;sh_video->ds=d_video;
  if(has_audio){
    if(verbose) printf("ASF: Searching for audio stream (id:%d)\n",d_audio->id);
    if(!ds_fill_buffer(d_audio)){
      printf("ASF: No Audio stream found...  ->nosound\n");
      has_audio=0;
      sh_audio=NULL;
    } else {
      sh_audio=d_audio->sh;sh_audio->ds=d_audio;
      sh_audio->format=sh_audio->wf->wFormatTag;
    }
  }
  sh_video->fps=1000.0f; sh_video->frametime=0.001f; // 1ms
  printf("VIDEO:  [%.4s]  %dx%d  %dbpp\n",
    &sh_video->bih->biCompression,
    sh_video->bih->biWidth,
    sh_video->bih->biHeight,
    sh_video->bih->biBitCount);
  break;
 }
 case DEMUXER_TYPE_MPEG_ES: {
   d_audio->type=0;
   has_audio=0;sh_audio=NULL;   // ES streams has no audio channel
   d_video->sh=new_sh_video(0); // create dummy video stream header, id=0
   break;
 }
 case DEMUXER_TYPE_MPEG_PS: {
  if(has_audio)
  if(!ds_fill_buffer(d_audio)){
    printf("MPEG: No Audio stream found...  ->nosound\n");
    has_audio=0;sh_audio=NULL;
  } else {
    sh_audio=d_audio->sh;sh_audio->ds=d_audio;
    if(verbose) printf("detected MPG-PS audio format: %d\n",d_audio->type);
    switch(d_audio->type){
      case 1: sh_audio->format=0x50;break; // mpeg
      case 2: sh_audio->format=0x10001;break;  // dvd pcm
      case 3: sh_audio->format=0x2000;break; // ac3
      default: has_audio=0; // unknown type
    }
  }
  break;
 }
} // switch(file_format)

// Determine image properties:
switch(file_format){
 case DEMUXER_TYPE_AVI:
 case DEMUXER_TYPE_ASF: {
  // display info:
  sh_video->format=sh_video->bih->biCompression;
  sh_video->disp_w=sh_video->bih->biWidth;
  sh_video->disp_h=abs(sh_video->bih->biHeight);
  break;
 }
 case DEMUXER_TYPE_MPEG_ES:
 case DEMUXER_TYPE_MPEG_PS: {
   // Find sequence_header first:
   if(verbose) printf("Searching for sequence header... ");fflush(stdout);
   while(1){
      int i=sync_video_packet(d_video);
      if(i==0x1B3) break; // found it!
      if(!i || !skip_video_packet(d_video)){
        if(verbose)  printf("NONE :(\n");
        printf("MPEG: FATAL: EOF while searching for sequence header\n");
        exit(1);
      }
   }
   if(verbose) printf("OK!\n");
   sh_video=d_video->sh;sh_video->ds=d_video;
   sh_video->format=1; // mpeg video
   mpeg2_init();
   // ========= Read & process sequence header & extension ============
   videobuffer=shmem_alloc(VIDEOBUFFER_SIZE);
   if(!videobuffer){ printf("Cannot allocate shared memory\n");exit(0);}
   videobuf_len=0;
   if(!read_video_packet(d_video)){ printf("FATAL: Cannot read sequence header!\n");exit(1);}
   if(header_process_sequence_header (picture, &videobuffer[4])) {
     printf ("bad sequence header!\n"); exit(1);
   }
   if(sync_video_packet(d_video)==0x1B5){ // next packet is seq. ext.
    videobuf_len=0;
    if(!read_video_packet(d_video)){ printf("FATAL: Cannot read sequence header extension!\n");exit(1);}
    if(header_process_extension (picture, &videobuffer[4])) {
      printf ("bad sequence header extension!\n");  exit(1);
    }
   }
   // display info:
   sh_video->fps=frameratecode2framerate[picture->frame_rate_code]*0.0001f;
   sh_video->frametime=10000.0f/(float)frameratecode2framerate[picture->frame_rate_code];
   sh_video->disp_w=picture->display_picture_width;
   sh_video->disp_h=picture->display_picture_height;
   // info:
   if(verbose) printf("mpeg bitrate: %d (%X)\n",picture->bitrate,picture->bitrate);
   printf("VIDEO:  %s  %dx%d  (aspect %d)  %4.2f fps  %5.1f kbps (%4.1f kbyte/s)\n",
    picture->mpeg1?"MPEG1":"MPEG2",
    sh_video->disp_w,sh_video->disp_h,
    picture->aspect_ratio_information,
    sh_video->fps,
    picture->bitrate*0.5f,
    picture->bitrate/16.0f );
  break;
 }
} // switch(file_format)

//if(verbose) printf("file successfully opened  (has_audio=%d)\n",has_audio);

printf("[V] filefmt:%d  fourcc:0x%X  size:%dx%d  fps:%5.2f  ftime:=%6.4f\n",
   file_format,sh_video->format,sh_video->disp_w,sh_video->disp_h,
   sh_video->fps,sh_video->frametime
);

fflush(stdout);

//================== Init AUDIO (codec) ==========================
if(has_audio){
  // Go through the codec.conf and find the best codec...
  sh_audio->codec=find_codec(sh_audio->format,NULL,NULL,1);
  if(!sh_audio->codec){
    printf("Can't find codec for audio format 0x%X !\n",sh_audio->format);
    has_audio=0;
  } else {
    printf("Found audio codec: [%s] drv:%d (%s)\n",sh_audio->codec->name,sh_audio->codec->driver,sh_audio->codec->info);
    //has_audio=sh_audio->codec->driver;
  }
}

if(has_audio){
  if(verbose) printf("Initializing audio codec...\n");
  if(!init_audio(sh_audio)){
    printf("Couldn't initialize audio codec! -> nosound\n");
    has_audio=0;
  } else {
    printf("AUDIO: samplerate=%d  channels=%d  bps=%d\n",sh_audio->samplerate,sh_audio->channels,sh_audio->samplesize);
  }
}

//================== Init VIDEO (codec & libvo) ==========================

// Go through the codec.conf and find the best codec...
sh_video->codec=find_codec(sh_video->format,
    sh_video->bih?((unsigned int*) &sh_video->bih->biCompression):NULL,NULL,0);
if(!sh_video->codec){
    printf("Can't find codec for video format 0x%X !\n",sh_video->format);
    exit(1);
}
//has_video=sh_video->codec->driver;

printf("Found video codec: [%s] drv:%d (%s)\n",sh_video->codec->name,sh_video->codec->driver,sh_video->codec->info);

for(i=0;i<CODECS_MAX_OUTFMT;i++){
    int ret;
    out_fmt=sh_video->codec->outfmt[i];
    if(out_fmt==0xFFFFFFFF) continue;
    ret=video_out->query_format(out_fmt);
    if(verbose) printf("vo_debug: query(0x%X) returned 0x%X\n",out_fmt,ret);
    if(ret) break;
}
if(i>=CODECS_MAX_OUTFMT){
    printf("Sorry, selected video_out device is incompatible with this codec.\n");
    exit(1);
}
sh_video->outfmtidx=i;

if(verbose) printf("vo_debug1: out_fmt=0x%08X\n",out_fmt);

switch(sh_video->codec->driver){
 case 2: {
   if(!init_video_codec(out_fmt)) exit(1);
   if(verbose) printf("INFO: Win32 video codec init OK!\n");
   break;
 }
 case 4: { // Win32/DirectShow
#ifndef USE_DIRECTSHOW
   printf("MPlayer was compiled WITHOUT directshow support!\n");
   exit(1);
#else
   sh_video->our_out_buffer=NULL;
   if(DS_VideoDecoder_Open(sh_video->codec->dll,&sh_video->codec->guid, sh_video->bih, 0, &sh_video->our_out_buffer)){
        printf("ERROR: Couldn't open required DirectShow codec: %s\n",sh_video->codec->dll);
        printf("Maybe you forget to upgrade your win32 codecs?? It's time to download the new\n");
        printf("package from:  ftp://thot.banki.hu/esp-team/linux/MPlayer/w32codec.zip  !\n");
        printf("Or you should disable DShow support: make distclean;make -f Makefile.No-DS\n");
        exit(1);
   }
   
   switch(out_fmt){
   case IMGFMT_YUY2:
   case IMGFMT_UYVY:
     DS_VideoDecoder_SetDestFmt(16,out_fmt);break;        // packed YUV
   case IMGFMT_YV12:
   case IMGFMT_I420:
   case IMGFMT_IYUV:
     DS_VideoDecoder_SetDestFmt(12,out_fmt);break;        // planar YUV
   default:
     DS_VideoDecoder_SetDestFmt(out_fmt&255,0);           // RGB/BGR
   }

   DS_VideoDecoder_Start();

   printf("DivX setting result = %d\n", DS_SetAttr_DivX("Quality",divx_quality) );
//   printf("DivX setting result = %d\n", DS_SetValue_DivX("Brightness",60) );
   
   if(verbose) printf("INFO: Win32/DShow video codec init OK!\n");
   break;
#endif
 }
 case 3: {  // OpenDivX
   if(verbose) printf("OpenDivX video codec\n");
   { DEC_PARAM dec_param;
     DEC_SET dec_set;
	dec_param.x_dim = sh_video->bih->biWidth;
	dec_param.y_dim = sh_video->bih->biHeight;
	dec_param.color_depth = 32;
	decore(0x123, DEC_OPT_INIT, &dec_param, NULL);
	dec_set.postproc_level = divx_quality;
	decore(0x123, DEC_OPT_SETPP, &dec_set, NULL);
   }
   if(verbose) printf("INFO: OpenDivX video codec init OK!\n");
   break;
 }
 case 1: {
   // init libmpeg2:
#ifdef MPEG12_POSTPROC
   picture->pp_options=divx_quality;
#else
   if(divx_quality){
       printf("WARNING! You requested image postprocessing for an MPEG 1/2 video,\n");
       printf("         but compiled MPlayer without MPEG 1/2 postprocessing support!\n");
       printf("         #define MPEG12_POSTPROC in config.h, and recompile libmpeg2!\n");
   }
#endif
   mpeg2_allocate_image_buffers (picture);
   break;
 }
}

if(verbose) printf("vo_debug2: out_fmt=0x%08X\n",out_fmt);

// ================== Init output files for encoding ===============
   if(encode_name){
     // encode file!!!
     FILE *encode_file=fopen(encode_name,"rb");
     if(encode_file){
       fclose(encode_file);
       printf("File already exists: %s (don't overwrite your favourite AVI!)\n",encode_name);
       return 0;
     }
     encode_file=fopen(encode_name,"wb");
     if(!encode_file){
       printf("Cannot create file for encoding\n");
       return 0;
     }
     write_avi_header_1(encode_file,mmioFOURCC('d', 'i', 'v', 'x'),sh_video->fps,sh_video->disp_w,sh_video->disp_h);
     fclose(encode_file);
     encode_index_name=malloc(strlen(encode_name)+8);
     strcpy(encode_index_name,encode_name);
     strcat(encode_index_name,".index");
     if((encode_file=fopen(encode_index_name,"wb")))
       fclose(encode_file);
     else encode_index_name=NULL;
     has_audio=0; // disable audio !!!!!
   }

// ========== Init keyboard FIFO (connection to libvo) ============

make_pipe(&keyb_fifo_get,&keyb_fifo_put);

// ========== Init display (sh_video->disp_w*sh_video->disp_h/out_fmt) ============

#ifdef X11_FULLSCREEN
   if(fullscreen){
     if(vo_init()){
       //if(verbose) printf("X11 running at %dx%d depth: %d\n",vo_screenwidth,vo_screenheight,vo_depthonscreen);
     }
     if(!screen_size_xy) screen_size_xy=vo_screenwidth; // scale with asp.ratio
   }
#endif

   if(screen_size_xy>0){
     if(screen_size_xy<=8){
       screen_size_x=screen_size_xy*sh_video->disp_w;
       screen_size_y=screen_size_xy*sh_video->disp_h;
     } else {
       screen_size_x=screen_size_xy;
       screen_size_y=screen_size_xy*sh_video->disp_h/sh_video->disp_w;
     }
   } else if(!vidmode){
     if(!screen_size_x) screen_size_x=SCREEN_SIZE_X;
     if(!screen_size_y) screen_size_y=SCREEN_SIZE_Y;
     if(screen_size_x<=8) screen_size_x*=sh_video->disp_w;
     if(screen_size_y<=8) screen_size_y*=sh_video->disp_h;
   }

   { const vo_info_t *info = video_out->get_info();
     printf("VO: [%s] %dx%d => %dx%d %s%s%s ",info->short_name,
         sh_video->disp_w,sh_video->disp_h,
         screen_size_x,screen_size_y,
         fullscreen?"fs ":"",
         vidmode?"vm ":"",
         softzoom?"zoom ":""
//         fullscreen|(vidmode<<1)|(softzoom<<2)
     );
     if((out_fmt&IMGFMT_BGR_MASK)==IMGFMT_BGR)
       printf("BGR%d\n",out_fmt&255); else
     if((out_fmt&IMGFMT_BGR_MASK)==IMGFMT_RGB)
       printf("RGB%d\n",out_fmt&255); else
     if(out_fmt==IMGFMT_YUY2) printf("YUY2\n"); else
     if(out_fmt==IMGFMT_UYVY) printf("UYVY\n"); else
     if(out_fmt==IMGFMT_I420) printf("I420\n"); else
     if(out_fmt==IMGFMT_IYUV) printf("IYUV\n"); else
     if(out_fmt==IMGFMT_YV12) printf("YV12\n");
   }

//   if(verbose) printf("Destination size: %d x %d  out_fmt=%0X\n",
//                      screen_size_x,screen_size_y,out_fmt);

   if(verbose) printf("video_out->init(%dx%d->%dx%d,flags=%d,'%s',0x%X)\n",
                      sh_video->disp_w,sh_video->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2),
                      title,out_fmt);

if(verbose) printf("vo_debug3: out_fmt=0x%08X\n",out_fmt);

   if(video_out->init(sh_video->disp_w,sh_video->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2),
                      title,out_fmt)){
     printf("FATAL: Cannot initialize video driver!\n");exit(1);
   }
   if(verbose) printf("INFO: Video OUT driver init OK!\n");

   fflush(stdout);
   
//================== MAIN: ==========================
{
int audio_fd=-1;
float buffer_delay=0;
float frame_correction=0; // A-V timestamp kulonbseg atlagolas
int frame_corr_num=0;   //
float a_frame=0;    // Audio
float v_frame=0;    // Video
float time_frame=0; // Timer
float a_pts=0;
float v_pts=0;
float c_total=0;
float max_pts_correction=default_max_pts_correction;
int eof=0;
int force_redraw=0;
float num_frames=0;      // number of frames played
double video_time_usage=0;
double vout_time_usage=0;
double audio_time_usage=0;
int grab_frames=0;
char osd_text_buffer[64];
int osd_level=2;
int osd_visible=100;
int osd_function=OSD_PLAY;

#ifdef HAVE_LIRC
  lirc_mp_setup();
#endif

#ifdef USE_TERMCAP
  load_termcap(NULL); // load key-codes
#endif
  getch2_enable();

  //========= Catch terminate signals: ================
  // terminate requests:
  signal(SIGTERM,exit_sighandler); // kill
  signal(SIGHUP,exit_sighandler);  // kill -HUP  /  xterm closed
  signal(SIGINT,exit_sighandler);  // Interrupt from keyboard
  signal(SIGQUIT,exit_sighandler); // Quit from keyboard
  // fatal errors:
  signal(SIGBUS,exit_sighandler);  // bus error
  signal(SIGSEGV,exit_sighandler); // segfault
  signal(SIGILL,exit_sighandler);  // illegal instruction
  signal(SIGFPE,exit_sighandler);  // floating point exc.
  signal(SIGABRT,exit_sighandler); // abort()

//================ SETUP AUDIO ==========================
  current_module="setup_audio";

if(has_audio){
#ifdef USE_XMMP_AUDIO
  xmm_Init( &xmm );
  xmm.cSound = (XMM_PluginSound *)xmm_PluginRegister( XMMP_AUDIO_DRIVER );
  if( xmm.cSound ){
    pSound = xmm.cSound->Init( &xmm );
    if( pSound && pSound->Start( pSound, sh_audio->samplerate, sh_audio->channels,
                ( sh_audio->samplesize == 2 ) ?  XMM_SOUND_FMT_S16LE : XMM_SOUND_FMT_U8 )){
        printf("XMM: audio setup ok\n");
    } else {
      has_audio=0;
    }
  } else has_audio=0;
#else
  audio_fd=open(dsp, O_WRONLY);
  if(audio_fd<0){
    printf("Can't open audio device %s  -> nosound\n",dsp);
    has_audio=0;
  }
#endif
}

if(has_audio){
#ifdef USE_XMMP_AUDIO
  if(audio_buffer_size==-1){
    // Measuring buffer size:
    buffer_delay=pSound->QueryDelay(pSound, 0);
  } else {
    // -abs commandline option
    buffer_delay=audio_buffer_size/(float)(sh_audio->o_bps);
  }
#else
  int r;
  r=(sh_audio->samplesize==2)?AFMT_S16_LE:AFMT_U8;ioctl (audio_fd, SNDCTL_DSP_SETFMT, &r);
  r=sh_audio->channels-1; ioctl (audio_fd, SNDCTL_DSP_STEREO, &r);
  r=sh_audio->samplerate; if(ioctl (audio_fd, SNDCTL_DSP_SPEED, &r)==-1)
      printf("audio_setup: your card doesn't support %d Hz samplerate\n",r);

#if 0
//  r = (64 << 16) + 1024;
  r = (65536 << 16) + 512;
    if(ioctl (audio_fd, SNDCTL_DSP_SETFRAGMENT, &r)==-1)
      printf("audio_setup: your card doesn't support setting fragments\n",r);
#endif

  if(audio_buffer_size==-1){
    // Measuring buffer size:
    audio_buffer_size=0;
#ifdef HAVE_AUDIO_SELECT
    while(audio_buffer_size<0x40000){
      fd_set rfds;
      struct timeval tv;
      FD_ZERO(&rfds); FD_SET(audio_fd,&rfds);
      tv.tv_sec=0; tv.tv_usec = 0;
      if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) break;
      write(audio_fd,&sh_audio->a_buffer[sh_audio->a_buffer_len],OUTBURST);
      audio_buffer_size+=OUTBURST;
    }
    if(audio_buffer_size==0){
        printf("\n   ***  Your audio driver DOES NOT support select()  ***\n");
          printf("Recompile mplayer with #undef HAVE_AUDIO_SELECT in config.h !\n\n");
        exit_player("audio_init");
    }
#endif
  }
  buffer_delay=audio_buffer_size/(float)(sh_audio->o_bps);
#endif
  a_frame=-(buffer_delay);
  printf("Audio buffer size: %d bytes, delay: %5.3fs\n",audio_buffer_size,buffer_delay);
}


if(!has_audio){
  printf("Audio: no sound\n");
  if(verbose) printf("Freeing %d unused audio chunks\n",d_audio->packs);
  ds_free_packs(d_audio); // free buffered chunks
  d_audio->id=-2;         // do not read audio chunks
  if(sh_audio) if(sh_audio->a_buffer) free(sh_audio->a_buffer);
  alsa=1;
  // fake, required for timer:
  sh_audio=new_sh_audio(255); // FIXME!!!!!!!!!!
  sh_audio->samplerate=76800;
  sh_audio->samplesize=sh_audio->channels=2;
  sh_audio->o_bps=sh_audio->channels*sh_audio->samplerate*sh_audio->samplesize;
}

  current_module=NULL;

//==================== START PLAYING =======================

if(file_format==DEMUXER_TYPE_AVI){
  a_pts=d_audio->pts-(buffer_delay+audio_delay);
  audio_delay-=(float)(sh_audio->audio.dwInitialFrames-sh_video->video.dwInitialFrames)*sh_video->frametime;
//  audio_delay-=(float)(sh_audio->audio.dwInitialFrames-sh_video->video.dwInitialFrames)/default_fps;
  if(verbose){
    printf("AVI Initial frame delay: %5.3f\n",(float)(sh_audio->audio.dwInitialFrames-sh_video->video.dwInitialFrames)*sh_video->frametime);
    printf("v: audio_delay=%5.3f  buffer_delay=%5.3f  a_pts=%5.3f  a_frame=%5.3f\n",
             audio_delay,buffer_delay,a_pts,a_frame);
    printf("START:  a_pts=%5.3f  v_pts=%5.3f  \n",d_audio->pts,d_video->pts);
  }
  delay_corrected=0; // has to correct PTS diffs
  d_video->pts=0;d_audio->pts=0; // PTS is outdated now!
}
if(force_fps){
  sh_video->fps=force_fps;
  sh_video->frametime=1.0f/sh_video->fps;
}

printf("Start playing...\n");fflush(stdout);

InitTimer();

while(!eof){

/*========================== PLAY AUDIO ============================*/

while(has_audio){

  // Update buffer if needed
  unsigned int t=GetTimer();
  current_module="decode_audio";   // Enter AUDIO decoder module
  //sh_audio->codec->driver=has_audio; // FIXME!
  while(sh_audio->a_buffer_len<OUTBURST && !d_audio->eof){
    int ret=decode_audio(sh_audio,&sh_audio->a_buffer[sh_audio->a_buffer_len],sh_audio->a_buffer_size-sh_audio->a_buffer_len);
    if(ret>0) sh_audio->a_buffer_len+=ret; else break;
  }
  current_module=NULL;   // Leave AUDIO decoder module
  t=GetTimer()-t;audio_time_usage+=t*0.000001;


  // Play sound from the buffer:
  if(sh_audio->a_buffer_len>=OUTBURST){ // if not EOF
#ifdef USE_XMMP_AUDIO
    pSound->Write( pSound, sh_audio->a_buffer, OUTBURST );
#else
#ifdef SIMULATE_ALSA
    fake_ALSA_write(audio_fd,sh_audio->a_buffer,OUTBURST); // for testing purposes
#else
    write(audio_fd,sh_audio->a_buffer,OUTBURST);
#endif
#endif
    sh_audio->a_buffer_len-=OUTBURST;
    memcpy(sh_audio->a_buffer,&sh_audio->a_buffer[OUTBURST],sh_audio->a_buffer_len);
#ifndef USE_XMMP_AUDIO
#ifndef SIMULATE_ALSA
    // check buffer
#ifdef HAVE_AUDIO_SELECT
    {  fd_set rfds;
       struct timeval tv;
       FD_ZERO(&rfds);
       FD_SET(audio_fd, &rfds);
       tv.tv_sec = 0;
       tv.tv_usec = 0;
       if(select(audio_fd+1, NULL, &rfds, NULL, &tv)){
         a_frame+=OUTBURST/(float)(sh_audio->o_bps);
         a_pts+=OUTBURST/(float)(sh_audio->o_bps);
//         printf("Filling audio buffer...\n");
         continue;
//       } else {
//         printf("audio buffer full...\n");
       }
    }
#endif
#endif
#endif
  }

  break;
} // if(has_audio)

/*========================== UPDATE TIMERS ============================*/

  a_frame+=OUTBURST/(float)(sh_audio->o_bps);
  a_pts+=OUTBURST/(float)(sh_audio->o_bps);

  if(alsa){
    // Use system timer for sync, not audio card/driver
    time_frame+=OUTBURST/(float)(sh_audio->o_bps);
    time_frame-=GetRelativeTime();
    if(time_frame<-0.1 || time_frame>0.1){
      time_frame=0;
    } else {
        while(time_frame>0.022){
            usleep(time_frame-0.22);
            time_frame-=GetRelativeTime();
        }
        while(time_frame>0.007){
            usleep(0);
            time_frame-=GetRelativeTime();
        }
    }
  }


/*========================== PLAY VIDEO ============================*/

if(1)
  while(v_frame<a_frame || force_redraw){
  
    float frame_time=1;
    float pts1=d_video->pts;

    current_module="decode_video";

  //--------------------  Decode a frame: -----------------------
switch(sh_video->codec->driver){
  case 3: {
    // OpenDivX
    unsigned int t=GetTimer();
    unsigned int t2;
    DEC_FRAME dec_frame;
    char* start=NULL;
    int in_size=ds_get_packet(d_video,&start);
    if(in_size<0){ eof=1;break;}
    if(in_size>max_framesize) max_framesize=in_size;
    // let's decode
        dec_frame.length = in_size;
	dec_frame.bitstream = start;
	dec_frame.render_flag = 1;
	decore(0x123, 0, &dec_frame, NULL);
      t2=GetTimer();t=t2-t;video_time_usage+=t*0.000001f;

      if(opendivx_src[0]){
        video_out->draw_slice(opendivx_src,opendivx_stride,
                            sh_video->disp_w,sh_video->disp_h,0,0);
        opendivx_src[0]=NULL;
      }
      t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;

    break;
  }
#ifdef USE_DIRECTSHOW
  case 4: {        // W32/DirectShow
    char* start=NULL;
    unsigned int t=GetTimer();
    unsigned int t2;
    int in_size=ds_get_packet(d_video,&start);
    if(in_size<0){ eof=1;break;}
    if(in_size>max_framesize) max_framesize=in_size;

    DS_VideoDecoder_DecodeFrame(start, in_size, 0, 1);
    current_module="draw_frame";

      t2=GetTimer();t=t2-t;video_time_usage+=t*0.000001f;
      if(out_fmt==IMGFMT_YV12||out_fmt==IMGFMT_IYUV||out_fmt==IMGFMT_I420){
        uint8_t* dst[3];
        int stride[3];
        stride[0]=sh_video->disp_w;
        stride[1]=stride[2]=sh_video->disp_w/2;
        dst[0]=sh_video->our_out_buffer;
        dst[2]=dst[0]+sh_video->disp_w*sh_video->disp_h;
        dst[1]=dst[2]+sh_video->disp_w*sh_video->disp_h/4;
        video_out->draw_slice(dst,stride,sh_video->disp_w,sh_video->disp_h,0,0);
      } else
        video_out->draw_frame((uint8_t **)&sh_video->our_out_buffer);
      t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;
    break;
  }
#endif
  case 2: {
    HRESULT ret;
    char* start=NULL;
    unsigned int t=GetTimer();
    unsigned int t2;
    int in_size=ds_get_packet(d_video,&start);
    if(in_size<0){ eof=1;break;}
    if(in_size>max_framesize) max_framesize=in_size;
    
    if(in_size){
      sh_video->bih->biSizeImage = in_size;
//      ret = ICDecompress(avi_header.hic, ICDECOMPRESS_NOTKEYFRAME|(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL), 
      ret = ICDecompress(sh_video->hic, ICDECOMPRESS_NOTKEYFRAME, 
                         sh_video->bih,   start,
                        &sh_video->o_bih, sh_video->our_out_buffer);
      if(ret){ printf("Error decompressing frame, err=%d\n",ret);break; }
    }
    current_module="draw_frame";
      t2=GetTimer();t=t2-t;video_time_usage+=t*0.000001f;
//      if(out_fmt==IMGFMT_YV12){
      if(out_fmt==IMGFMT_YV12||out_fmt==IMGFMT_IYUV||out_fmt==IMGFMT_I420){
        uint8_t* dst[3];
        int stride[3];
        stride[0]=sh_video->disp_w;
        stride[1]=stride[2]=sh_video->disp_w/2;
        dst[0]=sh_video->our_out_buffer;
        dst[2]=dst[0]+sh_video->disp_w*sh_video->disp_h;
        dst[1]=dst[2]+sh_video->disp_w*sh_video->disp_h/4;
        video_out->draw_slice(dst,stride,sh_video->disp_w,sh_video->disp_h,0,0);
      } else
        video_out->draw_frame((uint8_t **)&sh_video->our_out_buffer);
      t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;

    break;
  }
  case 1: {
        int in_frame=0;
        int t=0;
        float newfps;
        videobuf_len=0;
        while(videobuf_len<VIDEOBUFFER_SIZE-MAX_VIDEO_PACKET_SIZE){
          int i=sync_video_packet(d_video);
          if(in_frame){
            if(i<0x101 || i>=0x1B0){  // not slice code -> end of frame
#if 1
              // send END OF FRAME code:
              videobuffer[videobuf_len+0]=0;
              videobuffer[videobuf_len+1]=0;
              videobuffer[videobuf_len+2]=1;
              videobuffer[videobuf_len+3]=0xFF;
              videobuf_len+=4;
#endif
              if(!i) eof=1; // EOF
              break;
            }
          } else {
            //if(i==0x100) in_frame=1; // picture startcode
            if(i>=0x101 && i<0x1B0) in_frame=1; // picture startcode
            else if(!i){ eof=1; break;} // EOF
          }
	  if(grab_frames==2 && (i==0x1B3 || i==0x1B8)) grab_frames=1;
          if(!read_video_packet(d_video)){ eof=1; break;} // EOF
          //printf("read packet 0x%X, len=%d\n",i,videobuf_len);
        }
        
        if(videobuf_len>max_framesize) max_framesize=videobuf_len; // debug
        //printf("--- SEND %d bytes\n",videobuf_len);
	if(grab_frames==1){
	      FILE *f=fopen("grab.mpg","ab");
	      fwrite(videobuffer,videobuf_len-4,1,f);
	      fclose(f);
	}
        
        t-=GetTimer();
          mpeg2_decode_data(video_out, videobuffer, videobuffer+videobuf_len);
        t+=GetTimer(); video_time_usage+=t*0.000001;

        newfps=frameratecode2framerate[picture->frame_rate_code]*0.0001f;
        if(ABS(sh_video->fps-newfps)>0.01f) if(!force_fps){
            printf("Warning! FPS changed %5.3f -> %5.3f  (%f) [%d]  \n",sh_video->fps,newfps,sh_video->fps-newfps,picture->frame_rate_code);
            sh_video->fps=newfps;
            sh_video->frametime=10000.0f/(float)frameratecode2framerate[picture->frame_rate_code];
        }
        
        frame_time=(100+picture->repeat_count)*0.01f;
        picture->repeat_count=0;

    break;
  }
} // switch
//------------------------ frame decoded. --------------------

    // Increase video timers:
    num_frames+=frame_time;
    frame_time*=sh_video->frametime;
    if(file_format==DEMUXER_TYPE_ASF && !force_fps){
        // .ASF files has no fixed FPS - just frame durations!
        float d=d_video->pts-pts1;
        if(d>=0 && d<5) frame_time=d;
    }
    v_frame+=frame_time;
    v_pts+=frame_time;

    current_module="flip_page";
    video_out->flip_page();
    current_module=NULL;

    if(eof) break;
    if(force_redraw){
      --force_redraw;
      if(!force_redraw) osd_function=OSD_PLAY;
    }

//    printf("A:%6.1f  V:%6.1f  A-V:%7.3f  frame=%5.2f   \r",d_audio->pts,d_video->pts,d_audio->pts-d_video->pts,a_frame);
//    fflush(stdout);

#if 1
/*================ A-V TIMESTAMP CORRECTION: =========================*/
  if(has_audio){
    if(pts_from_bps && (file_format==DEMUXER_TYPE_AVI)){
//      a_pts=(float)ds_tell(d_audio)/sh_audio->wf.nAvgBytesPerSec-(buffer_delay+audio_delay);
      a_pts=(float)ds_tell(d_audio)/sh_audio->wf->nAvgBytesPerSec-(buffer_delay);
      delay_corrected=1; // hack
    } else
    if(d_audio->pts){
//      printf("\n=== APTS  a_pts=%5.3f  v_pts=%5.3f ===  \n",d_audio->pts,d_video->pts);
#if 1
      if(!delay_corrected){
        float x=d_audio->pts-d_video->pts-(buffer_delay+audio_delay);
        float y=-(buffer_delay+audio_delay);
        printf("Initial PTS delay: %5.3f sec  (calculated: %5.3f)\n",x,y);
        audio_delay+=x;
        //a_pts-=x;
        delay_corrected=1;
        if(verbose)
        printf("v: audio_delay=%5.3f  buffer_delay=%5.3f  a.pts=%5.3f  v.pts=%5.3f\n",
               audio_delay,buffer_delay,d_audio->pts,d_video->pts);
      }
#endif
      a_pts=d_audio->pts-(buffer_delay+audio_delay);
      d_audio->pts=0;
    }
    if(d_video->pts) v_pts=d_video->pts;
    if(frame_corr_num==5){
      float x=(frame_correction/5.0f);
      if(delay_corrected){
        printf("A:%6.1f  V:%6.1f  A-V:%7.3f",a_pts,v_pts,x);
        x*=0.5f;
        if(x<-max_pts_correction) x=-max_pts_correction; else
        if(x> max_pts_correction) x= max_pts_correction;
        max_pts_correction=default_max_pts_correction;
        a_frame+=x; c_total+=x;
        printf("  ct:%7.3f  %3d  %2d%%  %2d%%  %3.1f%% \r",c_total,
        (int)num_frames,
        (v_frame>0.5)?(int)(100.0*video_time_usage/(double)v_frame):0,
        (v_frame>0.5)?(int)(100.0*vout_time_usage/(double)v_frame):0,
        (v_frame>0.5)?(100.0*audio_time_usage/(double)v_frame):0
//        dbg_es_sent-dbg_es_rcvd 
        );
        fflush(stdout);
      }
      frame_corr_num=0; frame_correction=0;
    }
    if(frame_corr_num>=0) frame_correction+=a_pts-v_pts;
  } else {
    // No audio:
    if(d_video->pts) v_pts=d_video->pts;
    if(frame_corr_num==5){
//      printf("A: ---   V:%6.1f   \r",v_pts);
      printf("V:%6.1f  %3d  %2d%%  %2d%%  %3.1f%% \r",v_pts,
        (int)num_frames,
        (v_frame>0.5)?(int)(100.0*video_time_usage/(double)v_frame):0,
        (v_frame>0.5)?(int)(100.0*vout_time_usage/(double)v_frame):0,
        (v_frame>0.5)?(100.0*audio_time_usage/(double)v_frame):0
//        dbg_es_sent-dbg_es_rcvd
        );

      fflush(stdout);
      frame_corr_num=0;
    }
  }
  ++frame_corr_num;
#endif

  if(osd_visible){
    --osd_visible;
    if(!osd_visible) vo_osd_progbar_type=-1; // disable
  }

  if(osd_function==OSD_PAUSE){
      printf("\n------ PAUSED -------\r");fflush(stdout);
      while(
#ifdef HAVE_LIRC
          lirc_mp_getinput()<=0 &&
#endif
          getch2(20)<=0 && mplayer_get_key()<=0){
	  video_out->check_events();
      }
      osd_function=OSD_PLAY;
  }

  } //  while(v_frame<a_frame || force_redraw)


//================= Keyboard events, SEEKing ====================

{ int rel_seek_secs=0;
  int c;
  while(
#ifdef HAVE_LIRC
      (c=lirc_mp_getinput())>0 ||
#endif
      (c=getch2(0))>0 || (c=mplayer_get_key())>0) switch(c){
    // seek 10 sec
    case KEY_RIGHT:
      osd_function=OSD_FFW;
      rel_seek_secs+=10;break;
    case KEY_LEFT:
      osd_function=OSD_REW;
      rel_seek_secs-=10;break;
    // seek 1 min
    case KEY_UP:
      osd_function=OSD_FFW;
      rel_seek_secs+=60;break;
    case KEY_DOWN:
      osd_function=OSD_REW;
      rel_seek_secs-=60;break;
    // delay correction:
    case '+':
      buffer_delay+=0.1;  // increase audio buffer delay
      a_frame-=0.1;
      break;
    case '-':
      buffer_delay-=0.1;  // decrease audio buffer delay
      a_frame+=0.1;
      break;
    // quit
    case KEY_ESC: // ESC
    case KEY_ENTER: // ESC
    case 'q': exit_player("Quit");
    case 'g': grab_frames=2;break;
    // pause
    case 'p':
    case ' ':
      osd_function=OSD_PAUSE;
      break;
    case 'o':  // toggle OSD
      osd_level=(osd_level+1)%3;
      break;
    case '*':
      mixer_incvolume();
      break;
    case '/':
      mixer_decvolume();
      break;
    case 'm':
      mixer_usemaster=!mixer_usemaster;
      break;
  }
  if(rel_seek_secs)
  if(file_format==DEMUXER_TYPE_AVI && avi_header.idx_size<=0){
    printf("Can't seek in raw .AVI streams! (index required)  \n");
  } else {
    int skip_audio_bytes=0;
    float skip_audio_secs=0;

    // clear demux buffers:
    if(has_audio) ds_free_packs(d_audio);
    ds_free_packs(d_video);
    
//    printf("sh_audio->a_buffer_len=%d  \n",sh_audio->a_buffer_len);
    sh_audio->a_buffer_len=0;

switch(file_format){

  case DEMUXER_TYPE_AVI: {
  //================= seek in AVI ==========================
    int rel_seek_frames=rel_seek_secs*sh_video->fps;
    int curr_audio_pos=0;
    int audio_chunk_pos=-1;
    int video_chunk_pos=d_video->pos;
    
    skip_video_frames=0;

      // SEEK streams
//      if(d_video->pts) avi_video_pts=d_video->pts;
      avi_audio_pts=0;
      d_video->pts=0;
      d_audio->pts=0;

      // find video chunk pos:
      if(rel_seek_frames>0){
        // seek forward
        while(video_chunk_pos<avi_header.idx_size){
          int id=avi_header.idx[video_chunk_pos].ckid;
//          if(LOWORD(id)==aviTWOCC('0','0')){ // video frame
          if(avi_stream_id(id)==d_video->id){  // video frame
            if((--rel_seek_frames)<0 && avi_header.idx[video_chunk_pos].dwFlags&AVIIF_KEYFRAME) break;
            v_pts+=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
            ++skip_audio_bytes;
          }
          ++video_chunk_pos;
        }
      } else {
        // seek backward
        while(video_chunk_pos>=0){
          int id=avi_header.idx[video_chunk_pos].ckid;
//          if(LOWORD(id)==aviTWOCC('0','0')){ // video frame
          if(avi_stream_id(id)==d_video->id){  // video frame
            if((++rel_seek_frames)>0 && avi_header.idx[video_chunk_pos].dwFlags&AVIIF_KEYFRAME) break;
            v_pts-=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
            --skip_audio_bytes;
          }
          --video_chunk_pos;
        }
      }
      avi_header.idx_pos_a=avi_header.idx_pos_v=
      avi_header.idx_pos=video_chunk_pos;
//      printf("%d frames skipped\n",skip_audio_bytes);

#if 1
      // re-calc video pts:
      avi_video_pts=0;
      for(i=0;i<video_chunk_pos;i++){
          int id=avi_header.idx[i].ckid;
//          if(LOWORD(id)==aviTWOCC('0','0')){ // video frame
          if(avi_stream_id(id)==d_video->id){  // video frame
            avi_video_pts+=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
          }
      }
      //printf("v-pts recalc! %5.3f -> %5.3f  \n",v_pts,avi_video_pts);
      v_pts=avi_video_pts;
#else
      avi_video_pts=v_pts;
#endif
      a_pts=avi_video_pts-(buffer_delay);
      //a_pts=v_pts; //-(buffer_delay+audio_delay);

      if(has_audio){
        int i;
        int apos=0;
        int last=0;
        int len=0;

        // calc new audio position in audio stream: (using avg.bps value)
        curr_audio_pos=(avi_video_pts) * sh_audio->wf->nAvgBytesPerSec;
        if(curr_audio_pos<0)curr_audio_pos=0;
#if 1
        curr_audio_pos&=~15; // requires for PCM formats!!!
#else
        curr_audio_pos/=sh_audio->wf->nBlockAlign;
        curr_audio_pos*=sh_audio->wf->nBlockAlign;
        avi_header.audio_seekable=1;
#endif

        // find audio chunk pos:
          for(i=0;i<video_chunk_pos;i++){
            int id=avi_header.idx[i].ckid;
            //if(TWOCCFromFOURCC(id)==cktypeWAVEbytes){
            if(avi_stream_id(id)==d_audio->id){
              int aid=StreamFromFOURCC(id);
              if(d_audio->id==aid || d_audio->id==-1){
                len=avi_header.idx[i].dwChunkLength;
                last=i;
                if(apos<=curr_audio_pos && curr_audio_pos<(apos+len)){
                  if(verbose)printf("break;\n");
                  break;
                }
                apos+=len;
              }
            }
          }
          if(verbose)printf("XXX i=%d  last=%d  apos=%d  curr_audio_pos=%d  \n",
           i,last,apos,curr_audio_pos);
//          audio_chunk_pos=last; // maybe wrong (if not break; )
          audio_chunk_pos=i; // maybe wrong (if not break; )
          skip_audio_bytes=curr_audio_pos-apos;

          // update stream position:
          d_audio->pos=audio_chunk_pos;
          d_audio->dpos=apos;
          avi_header.idx_pos_a=avi_header.idx_pos_v=
          avi_header.idx_pos=audio_chunk_pos;

          if(!(sh_audio->codec->flags&CODECS_FLAG_SEEKABLE)){
#if 0
//             curr_audio_pos=apos; // selected audio codec can't seek in chunk
             skip_audio_secs=(float)skip_audio_bytes/(float)sh_audio->wf->nAvgBytesPerSec;
             //printf("Seek_AUDIO: %d bytes --> %5.3f secs\n",skip_audio_bytes,skip_audio_secs);
             skip_audio_bytes=0;
#else
             int d=skip_audio_bytes % sh_audio->wf->nBlockAlign;
             skip_audio_bytes-=d;
//             curr_audio_pos-=d;
             skip_audio_secs=(float)d/(float)sh_audio->wf->nAvgBytesPerSec;
             //printf("Seek_AUDIO: %d bytes --> %5.3f secs\n",d,skip_audio_secs);
#endif
          }
          // now: audio_chunk_pos=pos in index
          //      skip_audio_bytes=bytes to skip from that chunk
          //      skip_audio_secs=time to play audio before video (if can't skip)
          
          // calc skip_video_frames & adjust video pts counter:
//          i=last;
          i=avi_header.idx_pos;
          while(i<video_chunk_pos){
            int id=avi_header.idx[i].ckid;
//            if(LOWORD(id)==aviTWOCC('0','0')){ // video frame
            if(avi_stream_id(id)==d_video->id){  // video frame
              ++skip_video_frames;
              // requires for correct audio pts calculation (demuxer):
              avi_video_pts-=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
            }
            ++i;
          }
          
      }

      if(verbose) printf("SEEK: idx=%d  (a:%d v:%d)  v.skip=%d  a.skip=%d/%4.3f  \n",
        avi_header.idx_pos,audio_chunk_pos,video_chunk_pos,
        skip_video_frames,skip_audio_bytes,skip_audio_secs);

        // Set OSD:
      if(osd_level){
        osd_visible=sh_video->fps; // 1 sec
        vo_osd_progbar_type=0;
        vo_osd_progbar_value=(demuxer->filepos)/((avi_header.movi_end-avi_header.movi_start)>>8);
        //printf("avi filepos = %d  \n",vo_osd_progbar_value);
  //      printf("avi filepos = %d  (len=%d)  \n",demuxer->filepos,(avi_header.movi_end-avi_header.movi_start));
      }

  }
  break;

  case DEMUXER_TYPE_ASF: {
  //================= seek in ASF ==========================
    float p_rate=10; // packets / sec
    int rel_seek_packs=rel_seek_secs*p_rate;
    int rel_seek_bytes=rel_seek_packs*fileh.packetsize;
    int newpos;
    //printf("ASF: packs: %d  duration: %d  \n",(int)fileh.packets,*((int*)&fileh.duration));
//    printf("ASF_seek: %d secs -> %d packs -> %d bytes  \n",
//       rel_seek_secs,rel_seek_packs,rel_seek_bytes);
    newpos=demuxer->filepos+rel_seek_bytes;
    if(newpos<0) newpos=0;
    stream_seek(demuxer->stream,newpos);
  }
  break;
  
  case DEMUXER_TYPE_MPEG_ES:
  case DEMUXER_TYPE_MPEG_PS: {
  //================= seek in MPEG ==========================
        int newpos;
        if(picture->bitrate==0x3FFFF) // unspecified?
          newpos=demuxer->filepos+2324*75*rel_seek_secs; // 174.3 kbyte/sec
        else
          newpos=demuxer->filepos+(picture->bitrate*1000/16)*rel_seek_secs;
//       picture->bitrate=2324*75*8; // standard VCD bitrate (75 sectors / sec)

        if(newpos<seek_to_byte) newpos=seek_to_byte;
        newpos&=~(STREAM_BUFFER_SIZE-1);  /* sector boundary */
        stream_seek(demuxer->stream,newpos);
        // re-sync video:
        videobuf_code_len=0; // reset ES stream buffer
        while(1){
          int i=sync_video_packet(d_video);
          if(i==0x1B3 || i==0x1B8) break; // found it!
          if(!i || !skip_video_packet(d_video)){ eof=1; break;} // EOF
        }
        // re-sync audio:  (must read to get actual audio PTS)
        // if(has_audio) ds_fill_buffer(d_audio);
  }
  break;

} // switch(file_format)

      //====================== re-sync audio: =====================
      if(has_audio){

        if(skip_audio_bytes){
          demux_read_data(d_audio,NULL,skip_audio_bytes);
          d_audio->pts=0; // PTS is outdated because of the raw data skipping
        }
        
        current_module="resync_audio";

        switch(sh_audio->codec->driver){
        case 1:
          MP3_DecodeFrame(NULL,-2); // resync
          MP3_DecodeFrame(NULL,-2); // resync
          MP3_DecodeFrame(NULL,-2); // resync
          break;
        case 3:
          ac3_bitstream_reset();    // reset AC3 bitstream buffer
    //      if(verbose){ printf("Resyncing AC3 audio...");fflush(stdout);}
          sh_audio->ac3_frame=ac3_decode_frame(); // resync
    //      if(verbose) printf(" OK!\n");
          break;
        case 4:
        case 7:
          sh_audio->a_in_buffer_len=0;        // reset ACM/DShow audio buffer
          break;
        }

        // re-sync PTS (MPEG-PS only!!!)
        if(file_format==DEMUXER_TYPE_MPEG_PS)
        if(d_video->pts && d_audio->pts){
          if (d_video->pts < d_audio->pts){
          
          } else {
            while(d_video->pts > d_audio->pts){
              switch(sh_audio->codec->driver){
                case 1: MP3_DecodeFrame(NULL,-2);break; // skip MPEG frame
                case 3: sh_audio->ac3_frame=ac3_decode_frame();break; // skip AC3 frame
                default: ds_fill_buffer(d_audio);  // skip PCM frame
              }
            }
          }
        }

        current_module=NULL;

        c_total=0; // kell ez?
        printf("A:%6.1f  V:%6.1f  A-V:%7.3f",d_audio->pts,d_video->pts,0.0f);
        printf("  ct:%7.3f   \r",c_total);fflush(stdout);
      } else {
        printf("A: ---   V:%6.1f   \r",d_video->pts);fflush(stdout);
      }

      max_pts_correction=0.1;
      frame_corr_num=-5; frame_correction=0;
      force_redraw=5;
      a_frame=-buffer_delay-skip_audio_secs;
//      a_frame=-audio_delay-buffer_delay-skip_audio_secs;
      v_frame=0; // !!!!!!
      audio_time_usage=0; video_time_usage=0; vout_time_usage=0;
//      num_frames=real_num_frames=0;
  }
} // keyboard event handler

//================= Update OSD ====================
{ int i;
  if(osd_level>=2){
      vo_osd_text=osd_text_buffer;
      sprintf(vo_osd_text,"%c %02d:%02d:%02d",osd_function,(int)v_pts/3600,((int)v_pts/60)%60,((int)v_pts)%60);
  } else {
      vo_osd_text=NULL;
  }
//  for(i=1;i<=11;i++) osd_text_buffer[10+i]=i;osd_text_buffer[10+i]=0;
//  vo_osd_text=osd_text_buffer;
  
  // find sub
  if(subtitles){
      if(sub_fps==0) sub_fps=sh_video->fps;
      current_module="find_sub";
      find_sub(sub_uses_time?(100*(v_pts+sub_delay)):((v_pts+sub_delay)*sub_fps)); // FIXME! frame counter...
      current_module=NULL;
  }
}

} // while(!eof)

//printf("\nEnd of file.\n");
exit_player("End of file");
}
return 1;
}
