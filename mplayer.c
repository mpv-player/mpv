// AVI & MPEG Player    v0.11   (C) 2000-2001. by A'rpi/ESP-team

// Enable ALSA emulation (using 32kB audio buffer) - timer testing only
//#define SIMULATE_ALSA

// Define, if you want to run libmpeg2 in a new process (using codec-ctrl)
//#define HAVE_CODECCTRL

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

#include "libvo/video_out.h"

// CODECS:
#include "mp3lib/mp3.h"
#include "libac3/ac3.h"
#include "libmpeg2/mpeg2.h"
#include "libmpeg2/mpeg2_internal.h"

#include "loader.h"
#include "wine/avifmt.h"

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
static int verbose=0;

static int cfg_inc_verbose(struct config *conf){
    ++verbose;
    return 0;
}

static int max_framesize=0;

static int dbg_es_sent=0;
static int dbg_es_rcvd=0;

//static int show_packets=0;

//**************************************************************************//

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
//  int a_idx;
//  int v_idx;
  // video:
  AVIStreamHeader video;
  char *video_codec;
  BITMAPINFOHEADER bih;   // in format
  BITMAPINFOHEADER o_bih; // out format
  HIC hic;
  void *our_out_buffer;
  unsigned int bitrate;
  // video format flags:  (filled by codecs.c)
  char yuv_supported;   // 1 if codec support YUY2 output format
  char yuv_hack_needed; // requires for divx & mpeg4
  char no_32bpp_support; // requires for INDEO 3.x, 4.x
  char flipped;         // image is upside-down
  // audio:
  AVIStreamHeader audio;
  char *audio_codec;
  int audio_seekable;
  char wf_ext[64];     // in format
  WAVEFORMATEX wf;     // out format
  HACMSTREAM srcstream;
  int audio_in_minsize;
  int audio_out_minsize;
} avi_header_t;

avi_header_t avi_header;

#include "aviprint.c"
#include "codecs.c"

extern picture_t *picture;

char* encode_name=NULL;
char* encode_index_name=NULL;
int encode_bitrate=0;

//**************************************************************************//
//             Input media streaming & demultiplexer:
//**************************************************************************//

#include "stream.c"
#include "demuxer.c"
#include "demux_avi.c"
#include "demux_mpg.c"

demuxer_t *demuxer=NULL;
demux_stream_t *d_audio=NULL;
demux_stream_t *d_video=NULL;

// MPEG video stream parser:
#include "parse_es.c"

static const int frameratecode2framerate[16] = {
   0, 24000*10000/1001, 24*10000,25*10000, 30000*10000/1001, 30*10000,50*10000,60000*10000/1001,
  60*10000, 0,0,0,0,0,0,0
};

//**************************************************************************//
//             Audio codecs:
//**************************************************************************//

//int mp3_read(char *buf,int size){
int mplayer_audio_read(char *buf,int size){
  int len;
  len=demux_read_data(d_audio,buf,size);
  return len;
}

static void ac3_fill_buffer(uint8_t **start,uint8_t **end){
    int len=ds_get_packet(d_audio,(char**)start);
    //printf("<ac3:%d>\n",len);
    if(len<0)
          *start = *end = NULL;
    else
          *end = *start + len;
}

#include "alaw.c"

#include "xa/xa_gsm.h"

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

// MPEG video codec process controller:
#ifdef HAVE_CODECCTRL
#include "codecctrl.c"
#endif

//**************************************************************************//

static vo_functions_t *video_out=NULL;

static int play_in_bg=0;

void exit_player(char* how){
  if(how) printf("\nExiting... (%s)\n",how);
  printf("max framesize was %d bytes\n",max_framesize);
  // restore terminal:
  getch2_disable();
#ifdef HAVE_CODECCTRL
  if(child_pid){
    // MPEG
      // printf("\n\n");
      //send_cmd(data_fifo,0);usleep(50000); // EOF request
    DEBUG_SIG { printf("Sending TERM signal to CTRL...\n");DEBUG_SIGNALS_SLEEP}
      kill(child_pid,SIGTERM);
      usleep(10000);  // kill & wait 10ms
    DEBUG_SIG { printf("Closing PIPEs...\n");DEBUG_SIGNALS_SLEEP}
      close(control_fifo);
      close(data_fifo);
    DEBUG_SIG { printf("Freeing shmem...\n");DEBUG_SIGNALS_SLEEP}
      if(videobuffer) shmem_free(videobuffer);
    DEBUG_SIG { printf("Exiting...\n");DEBUG_SIGNALS_SLEEP}
  } else
#endif
  {
  	// AVI
	video_out->uninit();
  }
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

int main(int argc,char* argv[], char *envp[]){
char* filename=NULL; //"MI2-Trailer.avi";
int i;
int seek_to_sec=0;
int seek_to_byte=0;
int f; // filedes
int stream_type;
stream_t* stream=NULL;
int file_format=DEMUXER_TYPE_UNKNOWN;
int has_audio=1; // audio format  0=none  1=mpeg 2=pcm 3=ac3 4=win32 5=alaw 6=msgsm
int has_video=0; // video format  0=none  1=mpeg 2=win32 3=OpenDivX
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
float default_fps=25;
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
int screen_size_x=SCREEN_SIZE_X;
int screen_size_y=SCREEN_SIZE_Y;
int screen_size_xy=0;
// movie info:
int movie_size_x=0;
int movie_size_y=0;
int out_fmt=0;
char *dsp="/dev/dsp";
int force_ni=0;
char *homedir;
char conffile[100];
char confdir[100];
int conffile_fd;
#include "cfg-mplayer.h"

  printf("%s",banner_text);

if (parse_config_file(conf, "/etc/mplayer.conf") < 0)
  exit(1);
if ((homedir = getenv("HOME")) == NULL) {
  printf("Can't find HOME dir\n");
} else {
  snprintf(confdir, 100, "%s/.mplayer", homedir);
  mkdir(confdir, 0777);
  snprintf(conffile, 100, "%s/config", confdir);
  if ((conffile_fd = open(conffile, O_CREAT | O_EXCL | O_WRONLY, 0644)) != -1) {
    write(conffile_fd, default_config, strlen(default_config));
    close(conffile_fd);
  }
  if (parse_config_file(conf, conffile) < 0)
    exit(1);
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


if(!filename){
  if(vcd_track) filename="/dev/cdrom"; 
  else {
    printf("%s",help_text); exit(0);
  }
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
  stream_type=STREAMTYPE_FILE;
}

stream=new_stream(f,stream_type);

//============ Open & Sync stream and detect file format ===============

if(has_audio==0) audio_id=-2; // do NOT read audio packets...

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
//      printf("!!! ASF files not (yet) supported !!!\n");exit(1);
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
//          file_format=DEMUXER_TYPE_AVI_NI; // HACK!!!!
          demuxer->type=DEMUXER_TYPE_AVI_NI; // HACK!!!!
	  pts_from_bps=1; // force BPS sync!
        }
      }
  } else {
      // no index
      if(force_ni){
          printf("Using NON-INTERLEAVED Broken AVI file-format!\n");
//          file_format=DEMUXER_TYPE_AVI_NI; // HACK!!!!
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
  has_video=2;
  // Decide audio format:
  if(audio_format)
    has_audio=audio_format; // override type
  else if(has_audio)
    switch(((WAVEFORMATEX *)&avi_header.wf_ext)->wFormatTag){
      case 0:
        has_audio=0;break; // disable/no audio
      case 6:
        avi_header.audio_seekable=1;
        has_audio=5;break; // aLaw
      case 0x31:
      case 0x32:
        has_audio=6;break; // MS-GSM
      case 0x50:
#ifdef DEFAULT_MPG123
      case 0x55:
#endif
        avi_header.audio_seekable=1;
        has_audio=1;break; // MPEG
      case 0x01:
        avi_header.audio_seekable=1;
        has_audio=2;break; // PCM
      case 0x2000:
        avi_header.audio_seekable=1;
        has_audio=3;break; // AC3
      default:
        avi_header.audio_seekable=0;
        has_audio=4;       // Win32/ACM
    }
  if(verbose) printf("detected AVI audio format: %d\n",has_audio);
  if(has_audio==4){
    if(!avi_header.audio_codec) avi_header.audio_codec=get_auds_codec_name();
    if(!avi_header.audio_codec) has_audio=0; // unknown win32 codec
    if(verbose) printf("win32 audio codec: '%s'\n",avi_header.audio_codec);
  }
  if(has_audio){
    if(verbose) printf("AVI: Searching for audio stream (id:%d)\n",d_audio->id);
    if(!ds_fill_buffer(d_audio)){
      printf("AVI: No Audio stream found...  ->nosound\n");
      has_audio=0;
    }
  }
  default_fps=(float)avi_header.video.dwRate/(float)avi_header.video.dwScale;
  break;
 }
 case DEMUXER_TYPE_ASF: {
  //---- ASF header:
  read_asf_header();
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream,avi_header.movi_start);
  avi_header.idx_pos=0;
#if 0
  if(avi_header.idx_size>0){
    // decide index format:
    if(avi_header.idx[0].dwChunkOffset<avi_header.movi_start)
      avi_header.idx_offset=avi_header.movi_start-4;
    else
      avi_header.idx_offset=0;
    if(verbose) printf("ASF index offset: %d\n",avi_header.idx_offset);
  }
#endif
  demuxer->endpos=avi_header.movi_end;
  if(!ds_fill_buffer(d_video)){
    printf("ASF: missing video stream!? contact the author, it may be a bug :(\n");
    exit(1);
  }
  has_video=2;
  // Decide audio format:
  if(audio_format)
    has_audio=audio_format; // override type
  else if(has_audio)
    switch(((WAVEFORMATEX *)&avi_header.wf_ext)->wFormatTag){
      case 0:
        has_audio=0;break; // disable/no audio
      case 6:
        avi_header.audio_seekable=1;
        has_audio=5;break; // aLaw
      case 0x31:
      case 0x32:
        has_audio=6;break; // MS-GSM
      case 0x50:
#ifdef DEFAULT_MPG123
      case 0x55:
#endif
        avi_header.audio_seekable=1;
        has_audio=1;break; // MPEG
      case 0x01:
        avi_header.audio_seekable=1;
        has_audio=2;break; // PCM
      case 0x2000:
        avi_header.audio_seekable=1;
        has_audio=3;break; // AC3
      default:
        avi_header.audio_seekable=0;
        has_audio=4;       // Win32/ACM
    }
  if(verbose) printf("detected ASF audio format: %d\n",has_audio);
  if(has_audio==4){
    if(!avi_header.audio_codec) avi_header.audio_codec=get_auds_codec_name();
    if(!avi_header.audio_codec) has_audio=0; // unknown win32 codec
    if(verbose) printf("win32 audio codec: '%s'\n",avi_header.audio_codec);
  }
  if(has_audio){
    if(verbose) printf("ASF: Searching for audio stream (id:%d)\n",d_audio->id);
    if(!ds_fill_buffer(d_audio)){
      printf("ASF: No Audio stream found...  ->nosound\n");
      has_audio=0;
    }
  }
  break;
 }
 case DEMUXER_TYPE_MPEG_ES: {
  demuxer->audio->type=0;
  has_audio=0; // ES streams has no audio channel
  has_video=1; // mpeg video
  break;
 }
 case DEMUXER_TYPE_MPEG_PS: {
  if(has_audio)
  if(!ds_fill_buffer(d_audio)){
    printf("MPEG: No Audio stream found...  ->nosound\n");
    has_audio=0;
  } else {
    has_audio=d_audio->type;
  }
  if(verbose) printf("detected MPG-PS audio format: %d\n",has_audio);
  has_video=1; // mpeg video
  break;
 }
} // switch(file_format)

if(verbose) printf("file successfully opened  (has_audio=%d)\n",has_audio);

fflush(stdout);

//================== Init VIDEO (codec & libvo) ==========================

if(has_video==2){
  if(avi_header.video.fccHandler==mmioFOURCC('d', 'v', 'x', '1')) has_video=3;
  if(avi_header.video.fccHandler==mmioFOURCC('d', 'i', 'v', 'x')) has_video=3;
  if(avi_header.bih.biCompression==mmioFOURCC('d', 'v', 'x', '1')) has_video=3;
  if(avi_header.bih.biCompression==mmioFOURCC('d', 'i', 'v', 'x')) has_video=3;
//  if(avi_header.bih.biCompression==mmioFOURCC('D', 'I', 'V', 'X')) has_video=3; // Gabucino
}

switch(has_video){
 case 2: {
   if(!avi_header.video_codec) avi_header.video_codec=get_vids_codec_name();
   if(verbose)
     printf("win32 video codec: '%s' %s%s%s\n",avi_header.video_codec,
       avi_header.yuv_supported?"[YUV]":"",
       avi_header.yuv_hack_needed?"[hack]":"",
       avi_header.flipped?"[FLIP]":""
     );
   if(!avi_header.video_codec) exit(1); // unknown video codec
   if(avi_header.yuv_supported && video_out->query_format(IMGFMT_YUY2)) out_fmt=IMGFMT_YUY2; else
   if(avi_header.no_32bpp_support && video_out->query_format(IMGFMT_BGR|32)) out_fmt=IMGFMT_BGR|24; else
   if(video_out->query_format(IMGFMT_BGR|15)) out_fmt=IMGFMT_BGR|16; else
   if(video_out->query_format(IMGFMT_BGR|16)) out_fmt=IMGFMT_BGR|16; else
   if(video_out->query_format(IMGFMT_BGR|24)) out_fmt=IMGFMT_BGR|24; else
   if(video_out->query_format(IMGFMT_BGR|32)) out_fmt=IMGFMT_BGR|32; else {
     printf("Sorry, selected video_out device is incompatible with this codec.\n");
     printf("(It can't show 24bpp or 32bpp RGB images. Try to run X at 24/32bpp!)\n");
//     printf("(cannot convert between YUY2, YV12 and RGB colorspace formats)\n");
     exit(1);
   }
   //if(verbose) printf("AVI out_fmt=%X\n",out_fmt);
   if(verbose) if(out_fmt==IMGFMT_YUY2) printf("Using YUV/YUY2 video output format!\n");
   if(!init_video_codec(out_fmt)) exit(1);
   if(verbose) printf("INFO: Win32 video codec init OK!\n");
   if(out_fmt==(IMGFMT_BGR|16)) out_fmt=IMGFMT_BGR|15; // fix bpp
   
   // calculating video bitrate:
   avi_header.bitrate=avi_header.movi_end-avi_header.movi_start-avi_header.idx_size*8;
   if(avi_header.audio.fccType) avi_header.bitrate-=avi_header.audio.dwLength;
   if(verbose) printf("AVI video length=%d\n",avi_header.bitrate);
   avi_header.bitrate=((float)avi_header.bitrate/(float)avi_header.video.dwLength)
                     *((float)avi_header.video.dwRate/(float)avi_header.video.dwScale);
//   default_fps=(float)avi_header.video.dwRate/(float)avi_header.video.dwScale;
   printf("VIDEO:  [%.4s]  %dx%d  %dbpp  %4.2f fps  %5.1f kbps (%4.1f kbyte/s)\n",
    &avi_header.bih.biCompression,
    avi_header.bih.biWidth,
    avi_header.bih.biHeight,
    avi_header.bih.biBitCount,
    default_fps,
    avi_header.bitrate*0.008f,
    avi_header.bitrate/1024.0f );

   // display info:
   movie_size_x=avi_header.o_bih.biWidth;
   movie_size_y=abs(avi_header.o_bih.biHeight);
   break;
 }
 case 3: {  // OpenDivX
   out_fmt=IMGFMT_YV12;
   if(!video_out->query_format(out_fmt)) {
     printf("Sorry, selected video_out device is incompatible with this codec!\n");
     exit(1);
   }

   if(verbose) printf("OpenDivX video codec\n");
   { DEC_PARAM dec_param;
     DEC_SET dec_set;
	dec_param.x_dim = avi_header.bih.biWidth;
	dec_param.y_dim = avi_header.bih.biHeight;
	dec_param.color_depth = 32;
	decore(0x123, DEC_OPT_INIT, &dec_param, NULL);
	dec_set.postproc_level = divx_quality;
	decore(0x123, DEC_OPT_SETPP, &dec_set, NULL);
   }
   if(verbose) printf("INFO: OpenDivX video codec init OK!\n");
   
   // calculating video bitrate:
   avi_header.bitrate=avi_header.movi_end-avi_header.movi_start-avi_header.idx_size*8;
   if(avi_header.audio.fccType) avi_header.bitrate-=avi_header.audio.dwLength;
   if(verbose) printf("AVI video length=%d\n",avi_header.bitrate);
   avi_header.bitrate=((float)avi_header.bitrate/(float)avi_header.video.dwLength)
                     *((float)avi_header.video.dwRate/(float)avi_header.video.dwScale);
//   default_fps=(float)avi_header.video.dwRate/(float)avi_header.video.dwScale;
   printf("VIDEO:  [%.4s]  %dx%d  %dbpp  %4.2f fps  %5.1f kbps (%4.1f kbyte/s)\n",
    &avi_header.bih.biCompression,
    avi_header.bih.biWidth,
    avi_header.bih.biHeight,
    avi_header.bih.biBitCount,
    default_fps,
    avi_header.bitrate*0.008f,
    avi_header.bitrate/1024.0f );

   // display info:
//   movie_size_x=avi_header.bih.biWidth+(divx_quality?0:64);
   movie_size_x=avi_header.bih.biWidth;
   movie_size_y=abs(avi_header.bih.biHeight);
   break;
 }
 case 1: {
   out_fmt=IMGFMT_YV12;
   if(!video_out->query_format(out_fmt)) {
     printf("Sorry, selected video_out device is incompatible with this codec!\n");
     exit(1);
   }
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
   if(verbose) printf("FOUND!\n");
   // allocate some shared memory for the video packet buffer:
   videobuffer=shmem_alloc(VIDEOBUFFER_SIZE);
   if(!videobuffer){ printf("Cannot allocate shared memory\n");exit(0);}
   // init libmpeg2:
   mpeg2_init();
#ifdef MPEG12_POSTPROC
   picture->pp_options=divx_quality;
#else
   if(divx_quality){
       printf("WARNING! You requested image postprocessing for an MPEG 1/2 video,\n");
       printf("         but compiled MPlayer without MPEG 1/2 postprocessing support!\n");
       printf("         #define MPEG12_POSTPROC in config.h, and recompile libmpeg2!\n");
   }
#endif
   if(verbose)  printf("mpeg2_init() ok\n");
   // ========= Read & process sequence header & extension ============
   videobuf_len=0;
   if(!read_video_packet(d_video)){ printf("FATAL: Cannot read sequence header!\n");return 1;}
   if(header_process_sequence_header (picture, &videobuffer[4])) {
     printf ("bad sequence header!\n"); return 1;
   }
   if(sync_video_packet(d_video)==0x1B5){ // next packet is seq. ext.
    videobuf_len=0;
    if(!read_video_packet(d_video)){ printf("FATAL: Cannot read sequence header extension!\n");return 1;}
    if(header_process_extension (picture, &videobuffer[4])) {
      printf ("bad sequence header extension!\n"); return 1;
    }
   }
   default_fps=frameratecode2framerate[picture->frame_rate_code]*0.0001f;
   if(verbose) printf("mpeg bitrate: %d (%X)\n",picture->bitrate,picture->bitrate);
   printf("VIDEO:  %s  %dx%d  (aspect %d)  %4.2f fps  %5.1f kbps (%4.1f kbyte/s)\n",
    picture->mpeg1?"MPEG1":"MPEG2",
    picture->display_picture_width,picture->display_picture_height,
    picture->aspect_ratio_information,
    default_fps,
    picture->bitrate*0.5f,
    picture->bitrate/16.0f );
   // display info:
//   movie_size_x=picture->coded_picture_width;
   movie_size_x=picture->display_picture_width;
   movie_size_y=picture->display_picture_height;
   break;
 }
}

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
     write_avi_header_1(encode_file,mmioFOURCC('d', 'i', 'v', 'x'),default_fps,movie_size_x,movie_size_y);
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

// ========== Init display (movie_size_x*movie_size_y/out_fmt) ============

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
       screen_size_x=screen_size_xy*movie_size_x;
       screen_size_y=screen_size_xy*movie_size_y;
     } else {
       screen_size_x=screen_size_xy;
       screen_size_y=screen_size_xy*movie_size_y/movie_size_x;
     }
   } else {
     if(screen_size_x<=8) screen_size_x*=movie_size_x;
     if(screen_size_y<=8) screen_size_y*=movie_size_y;
   }
   if(verbose) printf("Destination size: %d x %d  out_fmt=%0X\n",
                      screen_size_x,screen_size_y,out_fmt);

   if(verbose) printf("video_out->init(%dx%d->%dx%d,fs=%d,'%s',0x%X)\n",
                      movie_size_x,movie_size_y,
                      screen_size_x,screen_size_y,
                      fullscreen,title,out_fmt);

   if(video_out->init(movie_size_x,movie_size_y,
                      screen_size_x,screen_size_y,
                      fullscreen,title,out_fmt)){
     printf("FATAL: Cannot initialize video driver!\n");exit(1);
   }
   if(verbose) printf("INFO: Video OUT driver init OK!\n");

   fflush(stdout);
   
  
if(has_video==1){
   //================== init mpeg codec ===================
   mpeg2_allocate_image_buffers (picture);
   if(verbose) printf("INFO: mpeg2_init_video() OK!\n");
#ifdef HAVE_CODECCTRL
   // ====== Init MPEG codec process ============
   make_pipe(&control_fifo,&control_fifo2);
   make_pipe(&data_fifo2,&data_fifo);
   // ====== Let's FORK() !!! ===================
   if((child_pid=fork())==0)
     mpeg_codec_controller(video_out); // this one is running in a new process!!!!
   signal(SIGPIPE,SIG_IGN);  // Ignore "Broken pipe" signal (codec restarts)
#endif
}

//================== MAIN: ==========================
{
char* a_buffer=NULL;
int a_buffer_len=0;
int a_buffer_size=0;
int audio_fd=-1;
int pcm_bswap=0;
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
ac3_frame_t *ac3_frame=NULL;
float num_frames=0;      // number of frames played
//int real_num_frames=0;   // number of frames readed
double video_time_usage=0;
double vout_time_usage=0;
double audio_time_usage=0;
int grab_frames=0;

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

  if(verbose) printf("Initializing audio codec...\n");

MP3_bps=2;
pcm_bswap=0;
a_buffer_size=16384;        // default size, maybe not enough for Win32/ACM

if(has_audio==4){
  // Win32 ACM audio codec:
  if(init_audio_codec()){
    MP3_channels=avi_header.wf.nChannels;
    MP3_samplerate=avi_header.wf.nSamplesPerSec;
    if(a_buffer_size<avi_header.audio_out_minsize+OUTBURST)
        a_buffer_size=avi_header.audio_out_minsize+OUTBURST;
  } else {
    printf("Could not load/initialize Win32/ACM AUDIO codec (missing DLL file?)\n");
    if((((WAVEFORMATEX *)&avi_header.wf_ext)->wFormatTag)==0x55){
      printf("Audio format is MP3 -> fallback to internal mp3lib/mpg123\n");
      has_audio=1;  // fallback to mp3lib
    } else
      has_audio=0;  // nosound
  }
}

// allocate audio out buffer:
a_buffer=malloc(a_buffer_size);
memset(a_buffer,0,a_buffer_size);
a_buffer_len=0;

if(has_audio==4){
    int ret=acm_decode_audio(a_buffer,a_buffer_size);
    if(ret<0){
        printf("ACM error %d -> switching to nosound...\n",ret);
        has_audio=0;
    } else {
        a_buffer_len=ret;
        printf("ACM decoding test: %d bytes\n",ret);
    }
}

if(has_audio==2){
  if(file_format==DEMUXER_TYPE_AVI){
    // AVI PCM Audio:
    WAVEFORMATEX *h=(WAVEFORMATEX*)&avi_header.wf_ext;
    MP3_channels=h->nChannels;
    MP3_samplerate=h->nSamplesPerSec;
    MP3_bps=(h->wBitsPerSample+7)/8;
  } else {
    // DVD PCM audio:
    MP3_channels=2;
    MP3_samplerate=48000;
    pcm_bswap=1;
  }
} else
if(has_audio==3){
  // Dolby AC3 audio:
  ac3_config.fill_buffer_callback = ac3_fill_buffer;
  ac3_config.num_output_ch = 2;
  ac3_config.flags = 0;
#ifdef HAVE_MMX
  ac3_config.flags |= AC3_MMX_ENABLE;
#endif
#ifdef HAVE_3DNOW
  ac3_config.flags |= AC3_3DNOW_ENABLE;
#endif
  ac3_init();
  ac3_frame = ac3_decode_frame();
  if(ac3_frame){
    MP3_samplerate=ac3_frame->sampling_rate;
    MP3_channels=2;
  } else has_audio=0; // bad frame -> disable audio
} else
if(has_audio==5){
  // aLaw audio codec:
  Gen_aLaw_2_Signed(); // init table
  MP3_channels=((WAVEFORMATEX*)&avi_header.wf_ext)->nChannels;
  MP3_samplerate=((WAVEFORMATEX*)&avi_header.wf_ext)->nSamplesPerSec;
} else
if(has_audio==6){
  // MS-GSM audio codec:
  GSM_Init();
  MP3_channels=((WAVEFORMATEX*)&avi_header.wf_ext)->nChannels;
  MP3_samplerate=((WAVEFORMATEX*)&avi_header.wf_ext)->nSamplesPerSec;
}
// must be here for Win32->mp3lib fallbacks
if(has_audio==1){
  // MPEG Audio:
  MP3_Init();
  MP3_samplerate=MP3_channels=0;
//  printf("[\n");
  a_buffer_len=MP3_DecodeFrame(a_buffer,-1);
//  printf("]\n");
  MP3_channels=2; // hack
}

if(verbose) printf("Audio: type: %d  samplerate=%d  channels=%d  bps=%d\n",has_audio,MP3_samplerate,MP3_channels,MP3_bps);

if(!MP3_channels || !MP3_samplerate){
  printf("Unknown/missing audio format, using nosound\n");
  has_audio=0;
}

if(has_audio){
#ifdef USE_XMMP_AUDIO
  xmm_Init( &xmm );
  xmm.cSound = (XMM_PluginSound *)xmm_PluginRegister( XMMP_AUDIO_DRIVER );
  if( xmm.cSound ){
    pSound = xmm.cSound->Init( &xmm );
    if( pSound && pSound->Start( pSound, MP3_samplerate, MP3_channels,
                ( MP3_bps == 2 ) ?  XMM_SOUND_FMT_S16LE : XMM_SOUND_FMT_U8 )){
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
    buffer_delay=audio_buffer_size/(float)(MP3_samplerate*MP3_channels*MP3_bps);
  }
#else
  int r;
  r=(MP3_bps==2)?AFMT_S16_LE:AFMT_U8;ioctl (audio_fd, SNDCTL_DSP_SETFMT, &r);
  r=MP3_channels-1; ioctl (audio_fd, SNDCTL_DSP_STEREO, &r);
  r=MP3_samplerate; if(ioctl (audio_fd, SNDCTL_DSP_SPEED, &r)==-1)
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
      write(audio_fd,&a_buffer[a_buffer_len],OUTBURST);
      audio_buffer_size+=OUTBURST;
    }
    if(audio_buffer_size==0){
        printf("\n   ***  Your audio driver DOES NOT support select()  ***\n");
          printf("Recompile mplayer with #undef HAVE_AUDIO_SELECT in config.h !\n\n");
        exit_player("audio_init");
    }
#endif
  }
  buffer_delay=audio_buffer_size/(float)(MP3_samplerate*MP3_channels*MP3_bps);
#endif
  a_frame=-(buffer_delay);
  printf("Audio buffer size: %d bytes, delay: %5.3fs\n",audio_buffer_size,buffer_delay);
}

} // has_audio

if(has_audio){
  printf("Audio: type: %d  samplerate: %d  channels: %d  bps: %d\n",has_audio,MP3_samplerate,MP3_channels,MP3_bps);
} else {
  printf("Audio: no sound\n");
  if(verbose) printf("Freeing %d unused audio chunks\n",d_audio->packs);
  ds_free_packs(d_audio); // free buffered chunks
  d_audio->id=-2;         // do not read audio chunks
  if(a_buffer) free(a_buffer);
  alsa=1; MP3_samplerate=76800;MP3_bps=MP3_channels=2; // fake, required for timer
}

  current_module=NULL;

//==================== START PLAYING =======================

if(file_format==DEMUXER_TYPE_AVI){
  a_pts=d_audio->pts-(buffer_delay+audio_delay);
  audio_delay-=(float)(avi_header.audio.dwInitialFrames-avi_header.video.dwInitialFrames)/default_fps;
//  audio_delay-=(float)(avi_header.audio.dwInitialFrames-avi_header.video.dwInitialFrames)/default_fps;
  printf("AVI Initial frame delay: %5.3f\n",(float)(avi_header.audio.dwInitialFrames-avi_header.video.dwInitialFrames)/default_fps);
  printf("v: audio_delay=%5.3f  buffer_delay=%5.3f  a_pts=%5.3f  a_frame=%5.3f\n",
           audio_delay,buffer_delay,a_pts,a_frame);
  printf("START:  a_pts=%5.3f  v_pts=%5.3f  \n",d_audio->pts,d_video->pts);
  delay_corrected=0; // has to correct PTS diffs
  d_video->pts=0;d_audio->pts=0; // PTS is outdated now!
}
if(force_fps) default_fps=force_fps;

printf("Start playing...\n");fflush(stdout);

#if 0
     // ACM debug code
{   DWORD srcsize=0;
    DWORD dstsize=16384*8;
    int ret=acmStreamSize(avi_header.srcstream,dstsize, &srcsize, ACM_STREAMSIZEF_DESTINATION);
    printf("acmStreamSize %d -> %d (err=%d)\n",dstsize,srcsize,ret);
}
#endif

InitTimer();

while(!eof){

/*========================== PLAY AUDIO ============================*/

while(has_audio){
  unsigned int t=GetTimer();
  current_module="decode_audio";   // Enter AUDIO decoder module
  // Update buffer if needed
  while(a_buffer_len<OUTBURST && !d_audio->eof){
    switch(has_audio){
      case 1: // MPEG layer 2 or 3
        a_buffer_len+=MP3_DecodeFrame(&a_buffer[a_buffer_len],-1);
        MP3_channels=2; // hack
        break;
      case 2: // PCM
      { int i=demux_read_data(d_audio,&a_buffer[a_buffer_len],OUTBURST);
        if(pcm_bswap){
          int j;
          if(i&3){ printf("Warning! pcm_audio_size&3 !=0  (%d)\n",i);i&=~3; }
          for(j=0;j<i;j+=2){
            char x=a_buffer[a_buffer_len+j];
            a_buffer[a_buffer_len+j]=a_buffer[a_buffer_len+j+1];
            a_buffer[a_buffer_len+j+1]=x;
          }
        }
        a_buffer_len+=i;
        break;
      }
      case 5:  // aLaw decoder
      { int l=demux_read_data(d_audio,&a_buffer[a_buffer_len],OUTBURST/2);
        unsigned short *d=(unsigned short *) &a_buffer[a_buffer_len];
        unsigned char *s=&a_buffer[a_buffer_len];
        a_buffer_len+=2*l;
        while(l>0){
          --l;
          d[l]=xa_alaw_2_sign[s[l]];
        }
        break;
      }
      case 6:  // MS-GSM decoder
      { unsigned char buf[65]; // 65 bytes / frame
            while(a_buffer_len<OUTBURST){
                if(demux_read_data(d_audio,buf,65)!=65) break; // EOF
                XA_MSGSM_Decoder(buf,(unsigned short *) &a_buffer[a_buffer_len]); // decodes 65 byte -> 320 short
//  		XA_GSM_Decoder(buf,(unsigned short *) &a_buffer[a_buffer_len]); // decodes 33 byte -> 160 short
                a_buffer_len+=2*320;
            }
        break;
      }
      case 3: // AC3 decoder
        //printf("{1:%d}",avi_header.idx_pos);fflush(stdout);
        if(!ac3_frame) ac3_frame=ac3_decode_frame();
        //printf("{2:%d}",avi_header.idx_pos);fflush(stdout);
        if(ac3_frame){
          memcpy(&a_buffer[a_buffer_len],ac3_frame->audio_data,256 * 6 *MP3_channels*MP3_bps);
          a_buffer_len+=256 * 6 *MP3_channels*MP3_bps;
          ac3_frame=NULL;
        }
        //printf("{3:%d}",avi_header.idx_pos);fflush(stdout);
        break;
      case 4:
      { int ret=acm_decode_audio(&a_buffer[a_buffer_len],a_buffer_size-a_buffer_len);
        if(ret>0) a_buffer_len+=ret;
        break;
      }
    }
  }
  current_module=NULL;   // Leave AUDIO decoder module
  t=GetTimer()-t;
  audio_time_usage+=t*0.000001;

  // Play sound from the buffer:

  if(a_buffer_len>=OUTBURST){ // if not EOF
#ifdef USE_XMMP_AUDIO
    pSound->Write( pSound, a_buffer, OUTBURST );
#else
#ifdef SIMULATE_ALSA
    fake_ALSA_write(audio_fd,a_buffer,OUTBURST); // for testing purposes
#else
    write(audio_fd,a_buffer,OUTBURST);
#endif
#endif
    a_buffer_len-=OUTBURST;
    memcpy(a_buffer,&a_buffer[OUTBURST],a_buffer_len);
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
         a_frame+=OUTBURST/(float)(MP3_samplerate*MP3_channels*MP3_bps);
         a_pts+=OUTBURST/(float)(MP3_samplerate*MP3_channels*MP3_bps);
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

  a_frame+=OUTBURST/(float)(MP3_samplerate*MP3_channels*MP3_bps);
  a_pts+=OUTBURST/(float)(MP3_samplerate*MP3_channels*MP3_bps);

  if(alsa){
    // Use system timer for sync, not audio card/driver
    time_frame+=OUTBURST/(float)(MP3_samplerate*MP3_channels*MP3_bps);
    time_frame-=GetRelativeTime();
//    printf("time_frame=%5.3f\n",time_frame);
    if(time_frame<-0.1 || time_frame>0.1){
      time_frame=0;
    } else {
//      if(time_frame>0.01) usleep(1000000*(time_frame-0.01)); // sleeping
//      if(time_frame>0.019) usleep(1000000*(time_frame-0.019)); // sleeping
//      if(time_frame>0.001) usleep(1000000*(time_frame)); // sleeping
//      if(time_frame>0.02) usleep(1000000*(time_frame)); // sleeping if >20ms
        while(time_frame>0.007){
//            printf("TIMER  %8.3f -> ",time_frame*1000);
//            if(time_frame>0.021)
//                usleep(time_frame-0.12);
//            else
                usleep(1000);
            time_frame-=GetRelativeTime();
//            printf("%8.3f    \n",time_frame*1000);
        }
      }

  }


/*========================== PLAY VIDEO ============================*/

if(1)
  while(v_frame<a_frame || force_redraw){

    current_module="decode_video";

  //--------------------  Decode a frame: -----------------------
switch(has_video){
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
	//dec_frame.bmp = video_out;
	dec_frame.render_flag = 1;
	decore(0x123, 0, &dec_frame, NULL);
      t2=GetTimer();t=t2-t;video_time_usage+=t*0.000001f;

      if(opendivx_src[0]){
        video_out->draw_slice(opendivx_src,opendivx_stride,
                            movie_size_x,movie_size_y,0,0);
//        video_out->flip_page();
        opendivx_src[0]=NULL;
      }

      t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;

      ++num_frames;
      v_frame+=1.0f/default_fps; //(float)avi_header.video.dwScale/(float)avi_header.video.dwRate;
    
    break;
  }
  case 2: {
    HRESULT ret;
    char* start=NULL;
    unsigned int t=GetTimer();
    unsigned int t2;
    float pts1=d_video->pts;
    int in_size=ds_get_packet(d_video,&start);
    float pts2=d_video->pts;
    if(in_size<0){ eof=1;break;}
    if(in_size>max_framesize) max_framesize=in_size;
    
//    printf("frame len = %5.4f\n",pts2-pts1);

//if(in_size>0){
      avi_header.bih.biSizeImage = in_size;
      ret = ICDecompress(avi_header.hic, ICDECOMPRESS_NOTKEYFRAME, 
//      ret = ICDecompress(avi_header.hic, ICDECOMPRESS_NOTKEYFRAME|(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL), 
                        &avi_header.bih,   start,
                        &avi_header.o_bih, avi_header.our_out_buffer);
      if(ret){ printf("Error decompressing frame, err=%d\n",ret);break; }
//}

      t2=GetTimer();t=t2-t;video_time_usage+=t*0.000001f;
        video_out->draw_frame((uint8_t **)&avi_header.our_out_buffer);
//        video_out->flip_page();
      t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;

      ++num_frames;
      
      if(file_format==DEMUXER_TYPE_ASF){
        float d=pts2-pts1;
        if(d>0 && d<0.2) v_frame+=d;
      } else
        v_frame+=1.0f/default_fps; //(float)avi_header.video.dwScale/(float)avi_header.video.dwRate;
      //v_pts+=1.0f/default_fps;   //(float)avi_header.video.dwScale/(float)avi_header.video.dwRate;

    break;
  }
  case 1: {
#ifndef HAVE_CODECCTRL

        int in_frame=0;
        videobuf_len=0;
        while(videobuf_len<VIDEOBUFFER_SIZE-MAX_VIDEO_PACKET_SIZE){
          int i=sync_video_packet(d_video);
          if(in_frame){
            if(i<0x101 || i>=0x1B0){  // not slice code -> end of frame
              // send END OF FRAME code:
#if 1
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
        ++dbg_es_sent;
        //if(videobuf_len>4) 
        //my_write(data_fifo,(char*) &videobuf_len,4);
        
        { int t=0;
          int x;
          float l;
          t-=GetTimer();
          mpeg2_decode_data(video_out, videobuffer, videobuffer+videobuf_len);
          t+=GetTimer();
          //*** CMD=0 : Frame completed ***
          //send_cmd(control_fifo2,0); // FRAME_COMPLETED command
          x=frameratecode2framerate[picture->frame_rate_code]; //fps
          ++dbg_es_rcvd;
          l=(100+picture->repeat_count)*0.01f;
          num_frames+=l;
          picture->repeat_count=0;
          video_time_usage+=t*0.000001;
          if(x && !force_fps) default_fps=x*0.0001f;
          if(!force_redraw){
            // increase video timers:
            v_frame+=l/default_fps;
            v_pts+=l/default_fps;
          }
        }
        //if(eof) break;

#else
    while(1){
      int x;
      while(1){
        x=-1; // paranoia
        if(4==read(control_fifo,&x,4)) break;  // status/command
        usleep(5000); // do not eat 100% CPU (waiting for codec restart)
      }
      if(x==0x3030303){
        //*** CMD=3030303 : Video packet requested ***
        // read a single compressed frame:
        int in_frame=0;
        videobuf_len=0;
        while(videobuf_len<VIDEOBUFFER_SIZE-MAX_VIDEO_PACKET_SIZE){
          int i=sync_video_packet(d_video);
          if(in_frame){
            if(i<0x101 || i>=0x1B0){  // not slice code -> end of frame
              // send END OF FRAME code:
#if 1
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
        ++dbg_es_sent;
        //if(videobuf_len>4) 
        my_write(data_fifo,(char*) &videobuf_len,4);
        if(eof) break;
      } else
      if(x==0){
        //*** CMD=0 : Frame completed ***
        int l=100;
        int t=0;
        read(control_fifo,&x,4); // FPS
        read(control_fifo,&l,4); // Length*100
        read(control_fifo,&t,4); // Time*1000000
        //printf("+++ FRAME COMPLETED fps=%d  len=%d  time=%d\n",x,l,t);
        ++dbg_es_rcvd;
        num_frames+=l/100.0f;
        video_time_usage+=t*0.000001;
        if(x && !force_fps) default_fps=x*0.0001f;
        if(!force_redraw){
          // increase video timers:
          v_frame+=l*0.01f/default_fps;
          v_pts+=l*0.01f/default_fps;
        }
        break; // frame OK.
      } else
      if(x==0x22222222){
        //*** CMD=22222222 : codec reset/restart ***
        read(control_fifo,&codec_pid,4); // PID
        printf("\nVideo codec started... (pid %d)\n",codec_pid);
        send_cmd(data_fifo,0x22222222);  // send response (syncword)
        dbg_es_sent=dbg_es_rcvd=0;
        //printf("  [ReSync-VIDEO]       \n");
        while(1){
          int id=sync_video_packet(d_video);
          if(id==0x100 || id>=0x1B0) break; // header chunk
          if(!id || !skip_video_packet(d_video)){ eof=1; break;} // EOF
        }
        if(eof) break;
        max_pts_correction=0.2;
      } else
        printf("invalid cmd: 0x%X\n",x);
    }
#endif
    break;
  }
} // switch
//------------------------ frame decoded. --------------------

    current_module="flip_page";

    video_out->flip_page();

    current_module=NULL;

    if(eof) break;
    if(force_redraw) --force_redraw;

//    printf("A:%6.1f  V:%6.1f  A-V:%7.3f  frame=%5.2f   \r",d_audio->pts,d_video->pts,d_audio->pts-d_video->pts,a_frame);
//    fflush(stdout);

#if 1
/*================ A-V TIMESTAMP CORRECTION: =========================*/
  if(has_audio){
    if(pts_from_bps && (file_format==DEMUXER_TYPE_AVI)){
//      a_pts=(float)ds_tell(d_audio)/((WAVEFORMATEX*)avi_header.wf_ext)->nAvgBytesPerSec-(buffer_delay+audio_delay);
      a_pts=(float)ds_tell(d_audio)/((WAVEFORMATEX*)avi_header.wf_ext)->nAvgBytesPerSec-(buffer_delay);
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
      if(!delay_corrected){
#if 0
        printf("Initial PTS delay: %5.3f sec\n",x);
        delay_corrected=1;
        audio_delay+=x;
        a_pts-=x;
#endif
      } else
      {
        printf("A:%6.1f  V:%6.1f  A-V:%7.3f",a_pts,v_pts,x);
        x*=0.5f;
        if(x<-max_pts_correction) x=-max_pts_correction; else
        if(x> max_pts_correction) x= max_pts_correction;
        max_pts_correction=default_max_pts_correction;
        a_frame+=x; c_total+=x;
#if 0
        printf("  ct:%7.3f  a=%d v=%d  \r",c_total,
        d_audio->pos,d_video->pos);
#else
        printf("  ct:%7.3f  %3d  %2d%%  %2d%%  %3.1f%% %d \r",c_total,
        (int)num_frames,
        (v_frame>0.5)?(int)(100.0*video_time_usage/(double)v_frame):0,
        (v_frame>0.5)?(int)(100.0*vout_time_usage/(double)v_frame):0,
        (v_frame>0.5)?(100.0*audio_time_usage/(double)v_frame):0,
        dbg_es_sent-dbg_es_rcvd 
        );
#endif
        fflush(stdout);
//        printf("\n");
      }
      //force_fps+=1*force_fps*x;
//      printf("  ct:%7.3f  fps:%5.2f nf:%2.1f/%d  t:%d.%03d\r",c_total,default_fps,num_frames,real_num_frames,codec_time_usage_sec,codec_time_usage/1000);fflush(stdout);
      frame_corr_num=0; frame_correction=0;
    }
    if(frame_corr_num>=0) frame_correction+=a_pts-v_pts;
  } else {
    // No audio:
    if(d_video->pts) v_pts=d_video->pts;
    if(frame_corr_num==5){
//      printf("A: ---   V:%6.1f   \r",v_pts);
      printf("V:%6.1f  %3d  %2d%%  %2d%%  %3.1f%% %d \r",v_pts,
        (int)num_frames,
        (v_frame>0.5)?(int)(100.0*video_time_usage/(double)v_frame):0,
        (v_frame>0.5)?(int)(100.0*vout_time_usage/(double)v_frame):0,
        (v_frame>0.5)?(100.0*audio_time_usage/(double)v_frame):0,
        dbg_es_sent-dbg_es_rcvd);

      fflush(stdout);
      frame_corr_num=0;
    }
  }
  ++frame_corr_num;
#endif

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
      rel_seek_secs+=10;break;
    case KEY_LEFT:
      rel_seek_secs-=10;break;
    // seek 1 min
    case KEY_UP:
      rel_seek_secs+=60;break;
    case KEY_DOWN:
      rel_seek_secs-=60;break;
    // delay correction:
    case '+':
      buffer_delay+=0.1;  // increase audio buffer size
      a_frame-=0.1;
      break;
    case '-':
      buffer_delay-=0.1;  // decrease audio buffer size
      a_frame+=0.1;
      break;
    // quit
    case KEY_ESC: // ESC
    case KEY_ENTER: // ESC
    case 'q': exit_player("Quit");
    case 'g': grab_frames=2;break;
    // restart codec
#ifdef HAVE_CODECCTRL
    case 'k': kill(codec_pid,SIGKILL);break;
//    case 'k': kill(child_pid,SIGKILL);break;
#endif
    // pause
    case 'p':
    case ' ':
      printf("\n------ PAUSED -------\r");fflush(stdout);
      while(
#ifdef HAVE_LIRC
          lirc_mp_getinput()<=0 &&
#endif
          getch2(20)<=0 && mplayer_get_key()<=0){
	  video_out->check_events();
      }
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
    
//    printf("a_buffer_len=%d  \n",a_buffer_len);
    a_buffer_len=0;

switch(file_format){

  case DEMUXER_TYPE_AVI: {
  //================= seek in AVI ==========================
    int rel_seek_frames=rel_seek_secs*default_fps;
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
            v_pts+=(float)avi_header.video.dwScale/(float)avi_header.video.dwRate;
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
            v_pts-=(float)avi_header.video.dwScale/(float)avi_header.video.dwRate;
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
            avi_video_pts+=(float)avi_header.video.dwScale/(float)avi_header.video.dwRate;
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
        curr_audio_pos=(avi_video_pts) * ((WAVEFORMATEX*)avi_header.wf_ext)->nAvgBytesPerSec;
        if(curr_audio_pos<0)curr_audio_pos=0;
#if 1
        curr_audio_pos&=~15; // requires for PCM formats!!!
#else
        curr_audio_pos/=((WAVEFORMATEX*)avi_header.wf_ext)->nBlockAlign;
        curr_audio_pos*=((WAVEFORMATEX*)avi_header.wf_ext)->nBlockAlign;
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

          if(!avi_header.audio_seekable){
#if 0
//             curr_audio_pos=apos; // selected audio codec can't seek in chunk
             skip_audio_secs=(float)skip_audio_bytes/(float)((WAVEFORMATEX*)avi_header.wf_ext)->nAvgBytesPerSec;
             //printf("Seek_AUDIO: %d bytes --> %5.3f secs\n",skip_audio_bytes,skip_audio_secs);
             skip_audio_bytes=0;
#else
             int d=skip_audio_bytes % ((WAVEFORMATEX*)avi_header.wf_ext)->nBlockAlign;
             skip_audio_bytes-=d;
//             curr_audio_pos-=d;
             skip_audio_secs=(float)d/(float)((WAVEFORMATEX*)avi_header.wf_ext)->nAvgBytesPerSec;
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
              avi_video_pts-=(float)avi_header.video.dwScale/(float)avi_header.video.dwRate;
            }
            ++i;
          }
          
      }

      if(verbose) printf("SEEK: idx=%d  (a:%d v:%d)  v.skip=%d  a.skip=%d/%4.3f  \n",
        avi_header.idx_pos,audio_chunk_pos,video_chunk_pos,
        skip_video_frames,skip_audio_bytes,skip_audio_secs);

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

        switch(has_audio){
        case 1:
          MP3_DecodeFrame(NULL,-2); // resync
          MP3_DecodeFrame(NULL,-2); // resync
          MP3_DecodeFrame(NULL,-2); // resync
          break;
        case 3:
          ac3_bitstream_reset();    // reset AC3 bitstream buffer
    //      if(verbose){ printf("Resyncing AC3 audio...");fflush(stdout);}
          ac3_frame=ac3_decode_frame(); // resync
    //      if(verbose) printf(" OK!\n");
          break;
        case 4:
          a_in_buffer_len=0;        // reset ACM audio buffer
          break;
        }

        // re-sync PTS (MPEG-PS only!!!)
        if(file_format==DEMUXER_TYPE_MPEG_PS)
        if(d_video->pts && d_audio->pts){
          if (d_video->pts < d_audio->pts){
          
          } else {
            while(d_video->pts > d_audio->pts){
              switch(has_audio){
                case 1: MP3_DecodeFrame(NULL,-2);break; // skip MPEG frame
                case 3: ac3_frame=ac3_decode_frame();break; // skip AC3 frame
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



} // while(!eof)

//printf("\nEnd of file.\n");
exit_player("End of file");
}
return 1;
}
