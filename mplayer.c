// AVI & MPEG Player    v0.18   (C) 2000-2001. by A'rpi/ESP-team

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <signal.h>
#include <time.h>
#include <fcntl.h>

#include "version.h"
#include "config.h"

#include "mp_msg.h"

#define HELP_MP_DEFINE_STATIC
#include "help_mp.h"

#include "cfgparser.h"
#include "cfg-mplayer-def.h"

#ifdef USE_SUB
#include "subreader.h"
void find_sub(subtitle* subtitles,int key);
#endif

#ifdef USE_LIBVO2
#include "libvo2/libvo2.h"
#else
#include "libvo/video_out.h"
extern void* mDisplay; // Display* mDisplay;
#endif

#ifdef USE_LIBVO2
#include "libvo2/sub.h"
#else
#include "libvo/sub.h"
#endif

#include "libao2/audio_out.h"

#include "libmpeg2/mpeg2.h"
#include "libmpeg2/mpeg2_internal.h"

#include "codec-cfg.h"

#include "dvdauth.h"
#include "spudec.h"

#include "linux/getch2.h"
#include "linux/keycodes.h"
#include "linux/timer.h"
#include "linux/shmem.h"

#include "cpudetect.h"

#ifdef HAVE_LIRC
#include "lirc_mp.h"
#endif

#ifdef HAVE_NEW_GUI
#include "Gui/mplayer/play.h"
#endif

int verbose=0;
int quiet=0;

#define ABS(x) (((x)>=0)?(x):(-(x)))

#ifdef TARGET_LINUX
#include <linux/rtc.h>
#endif

#ifdef USE_TV
#include "libmpdemux/tv.h"

extern int tv_param_on;
extern tvi_handle_t *tv_handler;
#endif

//**************************************************************************//
//             Config file
//**************************************************************************//

static int cfg_inc_verbose(struct config *conf){ ++verbose; return 0;}

static int cfg_include(struct config *conf, char *filename){
	return parse_config_file(conf, filename);
}

#include "get_path.c"

//**************************************************************************//
//**************************************************************************//
//             Input media streaming & demultiplexer:
//**************************************************************************//

static int max_framesize=0;

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "parse_es.h"

#include "dec_audio.h"
#include "dec_video.h"

#if 0
extern picture_t *picture;	// exported from libmpeg2/decode.c

int frameratecode2framerate[16] = {
  0,
  // Official mpeg1/2 framerates:
  24000*10000/1001, 24*10000,25*10000, 30000*10000/1001, 30*10000,50*10000,60000*10000/1001, 60*10000,
  // libmpeg3's "Unofficial economy rates":
  1*10000,5*10000,10*10000,12*10000,15*10000,0,0
};
#endif

//**************************************************************************//
//**************************************************************************//

// Common FIFO functions, and keyboard/event FIFO code
#include "fifo.c"

//**************************************************************************//

#ifdef USE_LIBVO2
static vo2_handle_t *video_out=NULL;
#else
static vo_functions_t *video_out=NULL;
#endif
static ao_functions_t *audio_out=NULL;

// benchmark:
double video_time_usage=0;
double vout_time_usage=0;
static double audio_time_usage=0;
static int total_time_usage_start=0;
static int benchmark=0;

// static int play_in_bg=0;

// options:
static int auto_quality=0;
static int output_quality=0;

int use_gui=0;

int osd_level=2;

// seek:
char *seek_to_sec=NULL;
off_t seek_to_byte=0;
off_t step_sec=0;
int loop_times=-1;
float rel_seek_secs=0;
int abs_seek_pos=0;

// codecs:
int has_audio=1;
char *audio_codec=NULL; // override audio codec
char *video_codec=NULL; // override video codec
int audio_family=-1;     // override audio codec family 
int video_family=-1;     // override video codec family 

// IMHO this stuff is no longer of use, or is there a special
// reason why dshow should be completely disabled? - atmos ::
// yes, people without working c++ compiler can disable it - A'rpi
#ifdef USE_DIRECTSHOW
int allow_dshow=1;
#else
int allow_dshow=0;
#endif

// streaming:
static int audio_id=-1;
static int video_id=-1;
static int dvdsub_id=-1;
static int vcd_track=0;

// cache2:
static int stream_cache_size=0;
#ifdef USE_STREAM_CACHE
extern int cache_fill_status;
#else
#define cache_fill_status 0
#endif

// dump:
static char *stream_dump_name=NULL;
static int stream_dump_type=0;

// A-V sync:
static float default_max_pts_correction=-1;//0.01f;
static float max_pts_correction=0;//default_max_pts_correction;
static float c_total=0;
static float audio_delay=0;

static int dapsync=0;
static int softsleep=0;

static float force_fps=0;
static int force_srate=0;
static int frame_dropping=0; // option  0=no drop  1= drop vo  2= drop decode
static int play_n_frames=-1;

// screen info:
char* video_driver=NULL; //"mga"; // default
char* audio_driver=NULL;
static int fullscreen=0;
static int vidmode=0;
static int softzoom=0;
static int flip=-1;
static int screen_size_x=0;//SCREEN_SIZE_X;
static int screen_size_y=0;//SCREEN_SIZE_Y;
static int screen_size_xy=0;
static float movie_aspect=0.0;

char* playlist_file;

// sub:
char *font_name=NULL;
float font_factor=0.75;
char *sub_name=NULL;
float sub_delay=0;
float sub_fps=0;
int   sub_auto = 1;
/*DSP!!char *dsp=NULL;*/

extern char *vo_subdevice;
extern char *ao_subdevice;

static stream_t* stream=NULL;

static char* current_module=NULL; // for debugging

static unsigned int inited_flags=0;
#define INITED_VO 1
#define INITED_AO 2
#define INITED_GUI 4
#define INITED_GETCH2 8
#define INITED_LIRC 16
#define INITED_STREAM 64
#define INITED_ALL 0xFFFF

void uninit_player(unsigned int mask){
  mask=inited_flags&mask;
  if(mask&INITED_VO){
    inited_flags&=~INITED_VO;
    current_module="uninit_vo";
#ifdef USE_LIBVO2
    vo2_close(video_out);
#else
    video_out->uninit();
#endif
  }

  if(mask&INITED_AO){
    inited_flags&=~INITED_AO;
    current_module="uninit_ao";
    audio_out->uninit();
  }

  if(mask&INITED_GETCH2){
    inited_flags&=~INITED_GETCH2;
    current_module="uninit_getch2";
  // restore terminal:
    getch2_disable();
  }

#ifdef HAVE_NEW_GUI
  if(mask&INITED_GUI){
    inited_flags&=~INITED_GUI;
    current_module="uninit_gui";
    mplDone();
  }
#endif

  if(mask&INITED_STREAM){
    inited_flags&=~INITED_STREAM;
    current_module="uninit_stream";
    if(stream) free_stream(stream);
    stream=NULL;
  }

#ifdef HAVE_LIRC
  if(mask&INITED_LIRC){
    inited_flags&=~INITED_LIRC;
    current_module="uninit_lirc";
    lirc_mp_cleanup();
  }
#endif

  current_module=NULL;

}

void exit_player(char* how){
 total_time_usage_start=GetTimer()-total_time_usage_start;

  uninit_player(INITED_ALL);

  current_module="exit_player";

  if(how) mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_Exiting,how);
  mp_msg(MSGT_CPLAYER,MSGL_V,"max framesize was %d bytes\n",max_framesize);
  if(benchmark){
      double tot=video_time_usage+vout_time_usage+audio_time_usage;
      double total_time_usage=(float)total_time_usage_start*0.000001;
      mp_msg(MSGT_CPLAYER,MSGL_INFO,"BENCHMARKs: V:%8.3fs VO:%8.3fs A:%8.3fs Sys:%8.3fs = %8.3fs\n",
          video_time_usage,vout_time_usage,audio_time_usage,
	  total_time_usage-tot,total_time_usage);
      if(total_time_usage>0.0)
      mp_msg(MSGT_CPLAYER,MSGL_INFO,"BENCHMARK%%: V:%8.4f%% VO:%8.4f%% A:%8.4f%% Sys:%8.4f%% = %8.4f%%\n",
          100.0*video_time_usage/total_time_usage,
	  100.0*vout_time_usage/total_time_usage,
	  100.0*audio_time_usage/total_time_usage,
	  100.0*(total_time_usage-tot)/total_time_usage,
	  100.0);
  }

  exit(1);
}

void exit_sighandler(int x){
  static int sig_count=0;
  ++sig_count;
  if(sig_count==2) exit(1);
  if(sig_count>2){
    // can't stop :(
    kill(getpid(),SIGKILL);
  }
  mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_IntBySignal,x,
      current_module?current_module:"unknown"
  );
  exit_player(NULL);
}

//extern void write_avi_header_1(FILE *f,int fcc,float fps,int width,int height);

#include "mixer.h"
#include "cfg-mplayer.h"

void parse_cfgfiles( void )
{
char *conffile;
int conffile_fd;
if (parse_config_file(conf, "/etc/mplayer.conf") < 0)
  exit(1);
if ((conffile = get_path("")) == NULL) {
  mp_msg(MSGT_CPLAYER,MSGL_WARN,MSGTR_NoHomeDir);
} else {
  mkdir(conffile, 0777);
  free(conffile);
  if ((conffile = get_path("config")) == NULL) {
    mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_GetpathProblem);
  } else {
    if ((conffile_fd = open(conffile, O_CREAT | O_EXCL | O_WRONLY, 0666)) != -1) {
      mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_CreatingCfgFile, conffile);
      write(conffile_fd, default_config, strlen(default_config));
      close(conffile_fd);
    }
    if (parse_config_file(conf, conffile) < 0)
      exit(1);
    free(conffile);
  }
}
}

int main(int argc,char* argv[], char *envp[]){

#ifdef USE_SUB
static subtitle* subtitles=NULL;
#endif

static demuxer_t *demuxer=NULL;

static demux_stream_t *d_audio=NULL;
static demux_stream_t *d_video=NULL;
static demux_stream_t *d_dvdsub=NULL;

static sh_audio_t *sh_audio=NULL;
static sh_video_t *sh_video=NULL;

// for multifile support:
char **filenames=NULL;
int num_filenames=0;
int curr_filename=0;

char* filename=NULL; //"MI2-Trailer.avi";
int file_format=DEMUXER_TYPE_UNKNOWN;
//
int delay_corrected=1;
char* title="MPlayer";

// movie info:
int out_fmt=0;

int osd_visible=100;
int osd_function=OSD_PLAY;
int osd_last_pts=-303;

int v_bright=50;
int v_cont=50;
int v_hue=50;
int v_saturation=50;

int vo_flags=0;

int rtc_fd=-1;

//float a_frame=0;    // Audio

int i;
int use_stdin=0; //int f; // filedes

int gui_no_filename=0;

  mp_msg_init(MSGL_STATUS);

  mp_msg(MSGT_CPLAYER,MSGL_INFO,"%s",banner_text);

  /* Test for cpu capabilities (and corresponding OS support) for optimizing */
#ifdef ARCH_X86
  GetCpuCaps(&gCpuCaps);
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"CPUflags: Type: %d MMX: %d MMX2: %d 3DNow: %d 3DNow2: %d SSE: %d SSE2: %d\n",
      gCpuCaps.cpuType,gCpuCaps.hasMMX,gCpuCaps.hasMMX2,
      gCpuCaps.has3DNow, gCpuCaps.has3DNowExt,
      gCpuCaps.hasSSE, gCpuCaps.hasSSE2);
#endif

  if ( argv[0] )
    if(!strcmp(argv[0],"gmplayer") ||
      (strrchr(argv[0],'/') && !strcmp(strrchr(argv[0],'/'),"/gmplayer") ) )
          use_gui=1;

    parse_cfgfiles();
    num_filenames=parse_command_line(conf, argc, argv, envp, &filenames);

   if(playlist_file!=NULL)
   {
    FILE *playlist_f;
    char *playlist_linebuffer = (char*)malloc(256);
    char *playlist_line;    
    if(!strcmp(playlist_file,"-"))
    {
      playlist_f = fopen("/dev/stdin","r");
    }
    else
      playlist_f = fopen(playlist_file,"r");
    if(playlist_f != NULL)
    {
      while(!feof(playlist_f))
      {
        memset(playlist_linebuffer,0,255);
        fgets(playlist_linebuffer,255,playlist_f);
        if(strlen(playlist_linebuffer)==0)
          break;
        playlist_linebuffer[strlen(playlist_linebuffer)-1] = 0;
        playlist_line = (char*)malloc(strlen(playlist_linebuffer)+1);
        memset(playlist_line,0,strlen(playlist_linebuffer)+1);
        strcpy(playlist_line,playlist_linebuffer);
        if (!(filenames = (char **) realloc(filenames, sizeof(*filenames) * (num_filenames + 2))))
          exit(3);
        filenames[num_filenames++] = playlist_line;
      }
      fclose(playlist_f);
    }
}


    if(num_filenames<0) exit(1); // error parsing cmdline

#ifndef HAVE_NEW_GUI
    if(use_gui){
      mp_msg(MSGT_CPLAYER,MSGL_WARN,MSGTR_NoGui);
      use_gui=0;
    }
#else
    if(use_gui && !vo_init()){
      mp_msg(MSGT_CPLAYER,MSGL_WARN,MSGTR_GuiNeedsX);
      use_gui=0;
    }
#endif

#ifndef USE_LIBVO2
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
#endif
    if(audio_driver && strcmp(audio_driver,"help")==0){
      printf("Available audio output drivers:\n");
      i=0;
      while (audio_out_drivers[i]) {
        const ao_info_t *info = audio_out_drivers[i++]->info;
	printf("\t%s\t%s\n", info->short_name, info->name);
      }
      printf("\n");
      exit(0);
    }

// check codec.conf
if(!parse_codec_cfg(get_path("codecs.conf"))){
  if(!parse_codec_cfg(DATADIR"/codecs.conf")){
    mp_msg(MSGT_CPLAYER,MSGL_HINT,MSGTR_CopyCodecsConf);
//    printf("Exit.\n");
//    exit(0);  // From unknown reason a hangup occurs here :((((((
    kill(getpid(),SIGTERM);
    usleep(20000);
    kill(getpid(),SIGKILL);
  }
}

    if(audio_codec && strcmp(audio_codec,"help")==0){
      printf("Available audio codecs:\n");
      list_codecs(1);
      printf("\n");
      exit(0);
    }
    if(video_codec && strcmp(video_codec,"help")==0){
      printf("Available video codecs:\n");
      list_codecs(0);
      printf("\n");
      exit(0);
    }


    if(!num_filenames && !vcd_track && !dvd_title && !tv_param_on){
      if(!use_gui){
	// no file/vcd/dvd -> show HELP:
	printf("%s",help_text);
	exit(0);
      } else gui_no_filename=1;
    }

    // Many users forget to include command line in bugreports...
    if(verbose){
      printf("CommandLine:");
      for(i=1;i<argc;i++)printf(" '%s'",argv[i]);
      printf("\n");
      printf("num_filenames: %d\n",num_filenames);
    }

    mp_msg_init(verbose+MSGL_STATUS);

//------ load global data first ------


// check font
#ifdef USE_OSD
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

#ifdef USE_SUB
// check .sub
  if(sub_name){
       int l=strlen(sub_name);
       if ((l>4) && ((0==strcmp(&sub_name[l-4],".utf"))
		   ||(0==strcmp(&sub_name[l-4],".UTF"))))
	  sub_utf8=1;
       subtitles=sub_read_file(sub_name);
       if(!subtitles || sub_num == 0) mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CantLoadSub,sub_name);
  }
#endif

  // It's time to init the GUI code: (and fork() the GTK process)
#ifdef HAVE_NEW_GUI
  if(use_gui){
       appInit( argc,argv,envp,(void*)mDisplay );
       inited_flags|=INITED_GUI;
       mplShMem->Playing= (gui_no_filename) ? 0 : 1;
  }
#endif

#ifdef HAVE_LIRC
  lirc_mp_setup();
  inited_flags|=INITED_LIRC;
#endif

#ifdef TARGET_LINUX
    if ((rtc_fd = open("/dev/rtc", O_RDONLY)) < 0)
	perror ("Linux RTC init: open");
    else {
	unsigned long irqp;

	/* if (ioctl(rtc_fd, RTC_IRQP_SET, _) < 0) { */
	if (ioctl(rtc_fd, RTC_IRQP_READ, &irqp) < 0) {
    	    perror ("Linux RTC init: ioctl (rtc_irqp_read)");
    	    close (rtc_fd);
    	    rtc_fd = -1;
	} else if (ioctl(rtc_fd, RTC_PIE_ON, 0) < 0) {
	    /* variable only by the root */
    	    perror ("Linux RTC init: ioctl (rtc_pie_on)");
    	    close (rtc_fd);
	    rtc_fd = -1;
	} else
	    printf("Using Linux's hardware RTC timing (%ldHz)\n", irqp);
    }
    if(rtc_fd<0)
#endif
	printf("Using %s timing\n",softsleep?"software":"usleep()");

#ifdef USE_TERMCAP
  if ( !use_gui ) load_termcap(NULL); // load key-codes
#endif

// ========== Init keyboard FIFO (connection to libvo) ============
make_pipe(&keyb_fifo_get,&keyb_fifo_put);

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

// ******************* Now, let's see the per-file stuff ********************

    curr_filename=0;
play_next_file:
    filename=(num_filenames>0)?filenames[curr_filename]:NULL;

#ifdef HAVE_NEW_GUI
    if ( use_gui ) {
      if(filename && !mplShMem->FilenameChanged) strcpy( mplShMem->Filename,filename );
//      mplShMem->Playing= (gui_no_filename) ? 0 : 1;
      while(mplShMem->Playing!=1){
	usleep(20000);
	EventHandling();
      }
      if(mplShMem->FilenameChanged){
        filename=mplShMem->Filename;
      }
    }
#endif

    if(filename) mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_Playing, filename);

#ifdef USE_SUB
// check .sub
  if(!sub_name){
      if(sub_auto && filename) { // auto load sub file ...
         subtitles=sub_read_file( sub_filename( get_path("sub/"), filename ) );
      }
      if(!subtitles) subtitles=sub_read_file(get_path("default.sub")); // try default
  }
#endif

  if(subtitles && stream_dump_type==3) list_sub_file(subtitles);
  if(subtitles && stream_dump_type==4) dump_mpsub(subtitles);

    stream=NULL;
    demuxer=NULL;
    d_audio=NULL;
    d_video=NULL;
    sh_audio=NULL;
    sh_video=NULL;
    
#ifdef USE_LIBVO2
    current_module="vo2_new";
    video_out=vo2_new(video_driver);
    current_module=NULL;
#else
// check video_out driver name:
    if (video_driver)
	if ((i = strcspn(video_driver, ":")) > 0)
	{
	    size_t i2 = strlen(video_driver);

	    if (video_driver[i] == ':')
	    {
		vo_subdevice = malloc(i2-i);
		if (vo_subdevice != NULL)
		    strncpy(vo_subdevice, (char *)(video_driver+i+1), i2-i);
		video_driver[i] = '\0';
	    }
//	    printf("video_driver: %s, subdevice: %s\n", video_driver, vo_subdevice);
	}
  if(!video_driver)
    video_out=video_out_drivers[0];
  else
  for (i=0; video_out_drivers[i] != NULL; i++){
    const vo_info_t *info = video_out_drivers[i]->get_info ();
    if(strcmp(info->short_name,video_driver) == 0){
      video_out = video_out_drivers[i];break;
    }
  }
#endif
  if(!video_out){
    mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_InvalidVOdriver,video_driver?video_driver:"?");
    exit_player(MSGTR_Exit_error);
  }

// check audio_out driver name:
    if (audio_driver)
	if ((i = strcspn(audio_driver, ":")) > 0)
	{
	    size_t i2 = strlen(audio_driver);

	    if (audio_driver[i] == ':')
	    {
		ao_subdevice = malloc(i2-i);
		if (ao_subdevice != NULL)
		    strncpy(ao_subdevice, (char *)(audio_driver+i+1), i2-i);
		audio_driver[i] = '\0';
	    }
//	    printf("audio_driver: %s, subdevice: %s\n", audio_driver, ao_subdevice);
	}
  if(!audio_driver)
    audio_out=audio_out_drivers[0];
  else
  for (i=0; audio_out_drivers[i] != NULL; i++){
    const ao_info_t *info = audio_out_drivers[i]->info;
    if(strcmp(info->short_name,audio_driver) == 0){
      audio_out = audio_out_drivers[i];break;
    }
  }
  if (!audio_out){
    mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_InvalidAOdriver,audio_driver);
    exit_player(MSGTR_Exit_error);
  }
/*DSP!!  if(dsp) audio_out->control(AOCONTROL_SET_DEVICE,(int)dsp);*/


  current_module="open_stream";
  stream=open_stream(filename,vcd_track,&file_format);
  if(!stream) goto goto_next_file;//  exit_player(MSGTR_Exit_error); // error...
  inited_flags|=INITED_STREAM;
  stream->start_pos+=seek_to_byte;

  if(stream_cache_size) stream_enable_cache(stream,stream_cache_size*1024);

  use_stdin=filename && (!strcmp(filename,"-"));

#ifdef HAVE_LIBCSS
  current_module="libcss";
  if (dvdimportkey) {
    if (dvd_import_key(dvdimportkey)) {
	mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_ErrorDVDkey);
	exit_player(MSGTR_Exit_error);
    }
    mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_CmdlineDVDkey);
  }
  if (dvd_auth_device) {
//  if (dvd_auth(dvd_auth_device,f)) {
    if (dvd_auth(dvd_auth_device,filename)) {
	mp_msg(MSGT_CPLAYER,MSGL_FATAL,"Error in DVD auth...\n");
	exit_player(MSGTR_Exit_error);
      } 
    mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_DVDauthOk);
  }
#endif

//============ Open & Sync stream and detect file format ===============

if(!has_audio) audio_id=-2; // do NOT read audio packets...

current_module="demux_open";

demuxer=demux_open(stream,file_format,audio_id,video_id,dvdsub_id);
if(!demuxer) goto goto_next_file; // exit_player(MSGTR_Exit_error); // ERROR

//file_format=demuxer->file_format;

d_audio=demuxer->audio;
d_video=demuxer->video;
d_dvdsub=demuxer->sub;

// DUMP STREAMS:
if(stream_dump_type){
  FILE *f;
  demux_stream_t *ds=NULL;
  current_module="dump";
  // select stream to dump
  switch(stream_dump_type){
  case 1: ds=d_audio;break;
  case 2: ds=d_video;break;
  case 3:
  case 4: ds=d_dvdsub;break;
  }
  if(!ds){        
      mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_DumpSelectedSteramMissing);
      exit_player(MSGTR_Exit_error);
  }
  // disable other streams:
  if(d_audio && d_audio!=ds) {ds_free_packs(d_audio); d_audio->id=-2; }
  if(d_video && d_video!=ds) {ds_free_packs(d_video); d_video->id=-2; }
  if(d_dvdsub && d_dvdsub!=ds) {ds_free_packs(d_dvdsub); d_dvdsub->id=-2; }
  // let's dump it!
  f=fopen(stream_dump_name?stream_dump_name:"stream.dump","wb");
  if(!f){
    mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_CantOpenDumpfile);
    exit_player(MSGTR_Exit_error);
  }
  while(!ds->eof){
    unsigned char* start;
    int in_size=ds_get_packet(ds,&start);
    if( (demuxer->file_format==DEMUXER_TYPE_AVI || demuxer->file_format==DEMUXER_TYPE_ASF || demuxer->file_format==DEMUXER_TYPE_MOV)
	&& stream_dump_type==2) fwrite(&in_size,1,4,f);
    if(in_size>0) fwrite(start,in_size,1,f);
  }
  fclose(f);
  mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_CoreDumped);
  exit_player(MSGTR_Exit_eof);
}

sh_audio=d_audio->sh;
sh_video=d_video->sh;

current_module="video_read_properties";

if(sh_video){

  if(!video_read_properties(sh_video)) goto goto_next_file; // exit_player(MSGTR_Exit_error); // couldn't read header?

  mp_msg(MSGT_CPLAYER,MSGL_INFO,"[V] filefmt:%d  fourcc:0x%X  size:%dx%d  fps:%5.2f  ftime:=%6.4f\n",
   demuxer->file_format,sh_video->format, sh_video->disp_w,sh_video->disp_h,
   sh_video->fps,sh_video->frametime
  );

  if(!sh_video->fps && !force_fps){
    mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_FPSnotspecified);
    goto goto_next_file; //  exit_player(MSGTR_Exit_error);
  }

}

fflush(stdout);

if(!sh_video){
    mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_NoVideoStream);
    goto goto_next_file; // exit_player(MSGTR_Exit_error);
}

//================== Init AUDIO (codec) ==========================

current_module="init_audio_codec";

if(sh_audio){
  // Go through the codec.conf and find the best codec...
  sh_audio->codec=NULL;
  if(audio_family!=-1) mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_TryForceAudioFmt,audio_family);
  while(1){
    sh_audio->codec=find_codec(sh_audio->format,NULL,sh_audio->codec,1);
    if(!sh_audio->codec){
      if(audio_family!=-1) {
        sh_audio->codec=NULL; /* re-search */
        mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CantFindAfmtFallback);
        audio_family=-1;
        continue;      
      }
      mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CantFindAudioCodec,sh_audio->format);
      mp_msg(MSGT_CPLAYER,MSGL_HINT, MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
      sh_audio=d_audio->sh=NULL;
      break;
    }
    if(audio_codec && strcmp(sh_audio->codec->name,audio_codec)) continue;
    else if(audio_family!=-1 && sh_audio->codec->driver!=audio_family) continue;
    mp_msg(MSGT_CPLAYER,MSGL_INFO,"%s audio codec: [%s] drv:%d (%s)\n",audio_codec?"Forcing":"Detected",sh_audio->codec->name,sh_audio->codec->driver,sh_audio->codec->info);
    break;
  }
}

if(sh_audio){
  mp_msg(MSGT_CPLAYER,MSGL_V,"Initializing audio codec...\n");
  if(!init_audio(sh_audio)){
    mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CouldntInitAudioCodec);
    sh_audio=d_audio->sh=NULL;
#ifdef HAVE_NEW_GUI
    if ( use_gui ) mplShMem->AudioType=0;
#endif
  } else {
    mp_msg(MSGT_CPLAYER,MSGL_INFO,"AUDIO: srate=%d  chans=%d  bps=%d  sfmt=0x%X  ratio: %d->%d\n",sh_audio->samplerate,sh_audio->channels,sh_audio->samplesize,
        sh_audio->sample_format,sh_audio->i_bps,sh_audio->o_bps);
#ifdef HAVE_NEW_GUI
    if ( use_gui ) mplShMem->AudioType=sh_audio->channels;
#endif
  }
}

//================== Init VIDEO (codec & libvo) ==========================

current_module="init_video_codec";

// Go through the codec.conf and find the best codec...
sh_video->codec=NULL;
if(video_family!=-1) mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_TryForceVideoFmt,video_family);
while(1){
  sh_video->codec=find_codec(sh_video->format,
    sh_video->bih?((unsigned int*) &sh_video->bih->biCompression):NULL,sh_video->codec,0);
  if(!sh_video->codec){
    if(video_family!=-1) {
      sh_video->codec=NULL; /* re-search */
      mp_msg(MSGT_CPLAYER,MSGL_WARN,MSGTR_CantFindVfmtFallback);
      video_family=-1;
      continue;      
    }
    mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CantFindVideoCodec,sh_video->format);
    mp_msg(MSGT_CPLAYER,MSGL_HINT, MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
    goto goto_next_file; // exit_player(MSGTR_Exit_error);
  }
  // is next line needed anymore? - atmos ::
  if(!allow_dshow && sh_video->codec->driver==VFM_DSHOW) continue; // skip DShow
  else if(video_codec && strcmp(sh_video->codec->name,video_codec)) continue;
  else if(video_family!=-1 && sh_video->codec->driver!=video_family) continue;
  break;
}

mp_msg(MSGT_CPLAYER,MSGL_INFO,"%s video codec: [%s] drv:%d (%s)\n",video_codec?"Forcing":"Detected",sh_video->codec->name,sh_video->codec->driver,sh_video->codec->info);

for(i=0;i<CODECS_MAX_OUTFMT;i++){
//    int ret;
    out_fmt=sh_video->codec->outfmt[i];
    if(out_fmt==0xFFFFFFFF) continue;
#ifdef USE_LIBVO2
    vo_flags=vo2_query_format(video_out);
#else
    vo_flags=video_out->query_format(out_fmt);
#endif
    mp_msg(MSGT_CPLAYER,MSGL_DBG2,"vo_debug: query(%s) returned 0x%X\n",vo_format_name(out_fmt),vo_flags);
    if(vo_flags) break;
}
if(i>=CODECS_MAX_OUTFMT){
    mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_VOincompCodec);
    goto goto_next_file; // exit_player(MSGTR_Exit_error);
}
sh_video->outfmtidx=i;

if(flip==-1){
    // autodetect flipping
    flip=0;
    if(sh_video->codec->outflags[i]&CODECS_FLAG_FLIP)
      if(!(sh_video->codec->outflags[i]&CODECS_FLAG_NOFLIP))
         flip=1;
}

mp_msg(MSGT_CPLAYER,MSGL_DBG2,"vo_debug1: out_fmt=%s\n",vo_format_name(out_fmt));

if(!init_video(sh_video)){
     mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_CouldntInitVideoCodec);
     goto goto_next_file; // exit_player(MSGTR_Exit_error);
}

if(auto_quality>0){
    // Auto quality option enabled
    output_quality=get_video_quality_max(sh_video);
    if(auto_quality>output_quality) auto_quality=output_quality;
    else output_quality=auto_quality;
    mp_msg(MSGT_CPLAYER,MSGL_V,"AutoQ: setting quality to %d\n",output_quality);
    set_video_quality(sh_video,output_quality);
}

// ========== Init display (sh_video->disp_w*sh_video->disp_h/out_fmt) ============

current_module="init_libvo";

#if 0 /* was X11_FULLSCREEN hack -> moved to libvo/vo_xv.c where it belongs ::atmos */
   if(fullscreen){
     if(vo_init()){
       //if(verbose) printf("X11 running at %dx%d depth: %d\n",vo_screenwidth,vo_screenheight,vo_depthonscreen);
     }
     if(!screen_size_xy) screen_size_xy=vo_screenwidth; // scale with asp.ratio
   }
#endif
  // Set default VGA 1:1 aspect as fallback ::atmos
  if(movie_aspect) sh_video->aspect = movie_aspect; // cmdline overrides autodetect
//  if(!sh_video->aspect) sh_video->aspect=1.0;

  if(screen_size_xy||screen_size_x||screen_size_y){
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
  } else {
    // check source format aspect, calculate prescale ::atmos
    screen_size_x=sh_video->disp_w;
    screen_size_y=sh_video->disp_h;
    if(sh_video->aspect>0.01){
      mp_msg(MSGT_CPLAYER,MSGL_INFO,"Movie-Aspect is %.2f:1 - prescaling to correct movie aspect.\n",
             sh_video->aspect);
      screen_size_x=(int)((float)sh_video->disp_h*sh_video->aspect);
      screen_size_x+=screen_size_x%2; // round
      if(screen_size_x<sh_video->disp_w){
        screen_size_x=sh_video->disp_w;
        screen_size_y=(int)((float)sh_video->disp_w*(1.0/sh_video->aspect));
        screen_size_y+=screen_size_y%2; // round
      }
    } else {
      mp_msg(MSGT_CPLAYER,MSGL_INFO,"Movie-Aspect is undefined - no prescaling applied.\n");
    }
  }

#ifndef USE_LIBVO2
   { const vo_info_t *info = video_out->get_info();
     mp_msg(MSGT_CPLAYER,MSGL_INFO,"VO: [%s] %dx%d => %dx%d %s %s%s%s%s\n",info->short_name,
         sh_video->disp_w,sh_video->disp_h,
         screen_size_x,screen_size_y,
	 vo_format_name(out_fmt),
         fullscreen?"fs ":"",
         vidmode?"vm ":"",
         softzoom?"zoom ":"",
         (flip==1)?"flip ":""
//         fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3)
     );
    mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Description: %s\n",info->name);
    mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Author: %s\n", info->author);
    if(strlen(info->comment) > 0)
        mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Comment: %s\n", info->comment);
   }
#endif

#ifdef HAVE_NEW_GUI
   if ( use_gui )
    {
     mplResizeToMovieSize( sh_video->disp_w,sh_video->disp_h );
     moviewidth=screen_size_x=sh_video->disp_w;
     movieheight=screen_size_y=sh_video->disp_h;
     mplShMem->StreamType=stream->type;
     mplSetFileName( filename );
    }
#endif

   mp_msg(MSGT_CPLAYER,MSGL_V,"video_out->init(%dx%d->%dx%d,flags=%d,'%s',0x%X)\n",
                      sh_video->disp_w,sh_video->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      title,out_fmt);

#ifdef USE_LIBVO2
   if(!vo2_start(video_out,
               sh_video->disp_w,sh_video->disp_h,out_fmt,0,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3) )){
#else
   if(video_out->init(sh_video->disp_w,sh_video->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      title,out_fmt)){
#endif
     mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_CannotInitVO);
     goto goto_next_file; // exit_player(MSGTR_Exit_error);
   }
   inited_flags|=INITED_VO;
   mp_msg(MSGT_CPLAYER,MSGL_V,"INFO: Video OUT driver init OK!\n");

   fflush(stdout);
   
//================== MAIN: ==========================
{

//int frame_corr_num=0;   //
//float v_frame=0;    // Video
float time_frame=0; // Timer
int eof=0;
int force_redraw=0;
//float num_frames=0;      // number of frames played
int grab_frames=0;
char osd_text_buffer[64];
int drop_frame=0;
int drop_frame_cnt=0;
int too_slow_frame_cnt=0;
int too_fast_frame_cnt=0;
// for auto-quality:
float AV_delay=0; // average of A-V timestamp differences
double cvideo_base_vtime;
double cvideo_base_vframe;
double vdecode_time;
unsigned int lastframeout_ts;
float time_frame_corr_avg=0;

//================ SETUP AUDIO ==========================
  current_module="setup_audio";

if(sh_audio){
  
  const ao_info_t *info=audio_out->info;
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"AO: [%s] %iHz %s %s\n",
      info->short_name,
      force_srate?force_srate:sh_audio->samplerate,
      sh_audio->channels>1?"Stereo":"Mono",
      audio_out_format_name(sh_audio->sample_format)
   );
   mp_msg(MSGT_CPLAYER,MSGL_V,"AO: Description: %s\nAO: Author: %s\n",
      info->name,
      info->author	
   );
   if(strlen(info->comment) > 0)
      mp_msg(MSGT_CPLAYER,MSGL_V,"AO: Comment: %s\n", info->comment);
  if(!audio_out->init(force_srate?force_srate:sh_audio->samplerate,
      sh_audio->channels,sh_audio->sample_format,0)){
    mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CannotInitAO);
    sh_audio=d_audio->sh=NULL;
  } else {
    inited_flags|=INITED_AO;
  }

//  printf("Audio buffer size: %d bytes, delay: %5.3fs\n",audio_buffer_size,audio_buffer_delay);

  // fixup audio buffer size:
//  if(outburst<MAX_OUTBURST){
//    sh_audio->a_buffer_size=sh_audio->audio_out_minsize+outburst;
//    printf("Audio out buffer size reduced to %d bytes\n",sh_audio->a_buffer_size);
//  }

//  sh_audio->timer=-(audio_buffer_delay);
}

  sh_video->timer=0;
  if(sh_audio) sh_audio->timer=-audio_delay;

if(!sh_audio){
  mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_NoSound);
  if(verbose) mp_msg(MSGT_CPLAYER,MSGL_V,"Freeing %d unused audio chunks\n",d_audio->packs);
  ds_free_packs(d_audio); // free buffered chunks
  d_audio->id=-2;         // do not read audio chunks
  if(audio_out) uninit_player(INITED_AO); // close device
}

  current_module=NULL;

if(demuxer->file_format!=DEMUXER_TYPE_AVI) pts_from_bps=0; // it must be 0 for mpeg/asf!
if(force_fps){
  sh_video->fps=force_fps;
  sh_video->frametime=1.0f/sh_video->fps;
  mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_FPSforced,sh_video->fps,sh_video->frametime);
}

//==================== START PLAYING =======================

mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_StartPlaying);fflush(stdout);

if(!use_stdin){
  getch2_enable();  // prepare stdin for hotkeys...
  inited_flags|=INITED_GETCH2;
}

InitTimer();

total_time_usage_start=GetTimer();

while(!eof){
//    unsigned int aq_total_time=GetTimer();
    float aq_sleep_time=0;

    if(play_n_frames>=0){
      --play_n_frames;
      if(play_n_frames<0) exit_player(MSGTR_Exit_frames);
    }

  vo_pts=sh_video->timer*90000.0;

/*========================== PLAY AUDIO ============================*/
while(sh_audio){
  unsigned int t;
  int playsize;
  
  ao_pts=sh_audio->timer*90000.0;
  playsize=audio_out->get_space();
  
  if(!playsize) break; // buffer is full, do not block here!!!
  
  if(playsize>MAX_OUTBURST) playsize=MAX_OUTBURST; // we shouldn't exceed it!
  //if(playsize>outburst) playsize=outburst;

  // Update buffer if needed
  current_module="decode_audio";   // Enter AUDIO decoder module
  t=GetTimer();
  while(sh_audio->a_buffer_len<playsize && !d_audio->eof){
    int ret=decode_audio(sh_audio,&sh_audio->a_buffer[sh_audio->a_buffer_len],
        playsize-sh_audio->a_buffer_len,sh_audio->a_buffer_size-sh_audio->a_buffer_len);
    if(ret>0) sh_audio->a_buffer_len+=ret; else break;
  }
  current_module=NULL;   // Leave AUDIO decoder module
  t=GetTimer()-t;audio_time_usage+=t*0.000001;
  
  if(playsize>sh_audio->a_buffer_len) playsize=sh_audio->a_buffer_len;
  
  playsize=audio_out->play(sh_audio->a_buffer,playsize,0);

  if(playsize>0){
      sh_audio->a_buffer_len-=playsize;
      memcpy(sh_audio->a_buffer,&sh_audio->a_buffer[playsize],sh_audio->a_buffer_len);
      sh_audio->timer+=playsize/(float)(sh_audio->o_bps);
  }

  break;
} // if(sh_audio)

/*========================== UPDATE TIMERS ============================*/
#if 0
  if(alsa){
    // Use system timer for sync, not audio card/driver
    time_frame-=GetRelativeTime();
    if(time_frame<-0.1 || time_frame>0.1){
      time_frame=0;
    } else {
        while(time_frame>0.022){
            usec_sleep(time_frame-0.022);
            time_frame-=GetRelativeTime();
        }
        while(time_frame>0.007){
            usec_sleep(1000);	// sleeps 1 clock tick (10ms)!
            time_frame-=GetRelativeTime();
        }
    }
  }
#endif

/*========================== PLAY VIDEO ============================*/

cvideo_base_vframe=sh_video->timer;
cvideo_base_vtime=video_time_usage;

if(1)
  while(1){
  
    float frame_time=0;
    int blit_frame=0;
    
    //--------------------  Decode a frame: -----------------------
    vdecode_time=video_time_usage;
    {   unsigned char* start=NULL;
	int in_size;
	// get it!
	current_module="video_read_frame";
        in_size=video_read_frame(sh_video,&frame_time,&start,force_fps);
	if(in_size<0){ eof=1; break; }
	if(in_size>max_framesize) max_framesize=in_size; // stats
	// decode:
	current_module="decode_video";
//	printf("Decode! %p  %d  \n",start,in_size);
	blit_frame=decode_video(video_out,sh_video,start,in_size,drop_frame);
    }
    vdecode_time=video_time_usage-vdecode_time;
    //------------------------ frame decoded. --------------------
    
//------------------------ add OSD to frame contents ---------
#ifndef USE_LIBVO2
    current_module="draw_osd";
    video_out->draw_osd();
#endif

    current_module="av_sync";

    sh_video->timer+=frame_time;
    time_frame+=frame_time;  // for nosound

    mp_dbg(MSGT_AVSYNC,MSGL_DBG2,"*** ftime=%5.3f ***\n",frame_time);

    if(drop_frame){

      if(sh_audio && !d_audio->eof){
          int delay=audio_out->get_delay();
          mp_dbg(MSGT_AVSYNC,MSGL_DBG2,"delay=%d\n",delay);
          time_frame=sh_video->timer;
          time_frame-=sh_audio->timer-(float)delay/(float)sh_audio->o_bps;
	  if(time_frame>-2*frame_time) {
	    drop_frame=0; // stop dropping frames
	    mp_msg(MSGT_AVSYNC,MSGL_DBG2,"\nstop frame drop %.2f\n", time_frame);
	  }else{
	    ++drop_frame_cnt;
	    if (verbose > 0 && drop_frame_cnt%10 == 0)
	      mp_msg(MSGT_AVSYNC,MSGL_DBG2,"\nstill dropping, %.2f\n", time_frame);
	  }
      }
#ifdef HAVE_NEW_GUI
      if(use_gui) EventHandling();
#endif
      video_out->check_events(); // check events AST
    } else {
      // It's time to sleep...
      current_module="sleep";

      time_frame-=GetRelativeTime(); // reset timer

      if(sh_audio && !d_audio->eof){
          int delay=audio_out->get_delay();
          mp_dbg(MSGT_AVSYNC,MSGL_DBG2,"delay=%d\n",delay);

if(!dapsync){

	      /* Arpi's AV-sync */

          time_frame=sh_video->timer;
          time_frame-=sh_audio->timer-(float)delay/(float)sh_audio->o_bps;
          // we are out of time... drop next frame!
	  if(time_frame<-2*frame_time){
	      static int drop_message=0;
	      drop_frame=frame_dropping; // tricky!
	      ++drop_frame_cnt;
	      if(drop_frame_cnt>50 && AV_delay>0.5 && !drop_message){
	          drop_message=1;
	          mp_msg(MSGT_AVSYNC,MSGL_WARN,MSGTR_SystemTooSlow);
	      }
	      mp_msg(MSGT_AVSYNC,MSGL_DBG2,"\nframe drop %d, %.2f\n", drop_frame, time_frame);
	  }

} else {

	      /* DaP's AV-sync */

    	      float SH_AV_delay;
	      /* SH_AV_delay = sh_video->timer - (sh_audio->timer - (float)((float)delay + sh_audio->a_buffer_len) / (float)sh_audio->o_bps); */
	      SH_AV_delay = sh_video->timer - (sh_audio->timer - (float)((float)delay) / (float)sh_audio->o_bps);
	      // printf ("audio slp req: %.3f TF: %.3f delta: %.3f (v: %.3f a: %.3f) | ", i, time_frame,
	      //	    i - time_frame, sh_video->timer, sh_audio->timer - (float)((float)delay / (float)sh_audio->o_bps));
	      if(SH_AV_delay<-2*frame_time){
		  static int drop_message=0;
	          drop_frame=frame_dropping; // tricky!
	          ++drop_frame_cnt;
		  if(drop_frame_cnt>50 && AV_delay>0.5 && !drop_message){
	    	      drop_message=1;
	    	      mp_msg(MSGT_AVSYNC,MSGL_WARN,MSGTR_SystemTooSlow);
	         }
		printf ("A-V SYNC: FRAMEDROP (SH_AV_delay=%.3f)!\n", SH_AV_delay);
	        mp_msg(MSGT_AVSYNC,MSGL_DBG2,"\nframe drop %d, %.2f\n", drop_frame, time_frame);
	        /* go into unlimited-TF cycle */
    		time_frame = SH_AV_delay;
	      } else {
#define	SL_CORR_AVG_LEN	125
	        /* don't adjust under framedropping */
	        time_frame_corr_avg = (time_frame_corr_avg * (SL_CORR_AVG_LEN - 1) +
	    				(SH_AV_delay - time_frame)) / SL_CORR_AVG_LEN;
#define	UNEXP_CORR_MAX	0.1	/* limit of unexpected correction between two frames (percentage) */
#define	UNEXP_CORR_WARN	1.0	/* warn limit of A-V lag (percentage) */
	        time_frame += time_frame_corr_avg;
	        // printf ("{%.3f - %.3f}\n", i - time_frame, frame_time + frame_time_corr_avg);
	        if (SH_AV_delay - time_frame < (frame_time + time_frame_corr_avg) * UNEXP_CORR_MAX &&
		    SH_AV_delay - time_frame > (frame_time + time_frame_corr_avg) * -UNEXP_CORR_MAX)
		    time_frame = SH_AV_delay;
	        else {
		    if (SH_AV_delay - time_frame > (frame_time + time_frame_corr_avg) * UNEXP_CORR_WARN ||
		        SH_AV_delay - time_frame < (frame_time + time_frame_corr_avg) * -UNEXP_CORR_WARN)
		        printf ("WARNING: A-V SYNC LAG TOO LARGE: %.3f {%.3f - %.3f} (too little UNEXP_CORR_MAX?)\n",
		  	    SH_AV_delay - time_frame, SH_AV_delay, time_frame);
		        time_frame += (frame_time + time_frame_corr_avg) * ((SH_AV_delay > time_frame) ?
		    		      UNEXP_CORR_MAX : -UNEXP_CORR_MAX);
	        }
	      }	/* /start dropframe */

}

      } else {
          // NOSOUND:
          if( (time_frame<-3*frame_time || time_frame>3*frame_time) || benchmark)
	      time_frame=0;
	  
      }

//      if(verbose>1)printf("sleep: %5.3f  a:%6.3f  v:%6.3f  \n",time_frame,sh_audio->timer,sh_video->timer);

      aq_sleep_time+=time_frame;

#ifdef HAVE_NEW_GUI
      if(use_gui){
	EventHandling();
      }
#endif

if(!(vo_flags&256)){ // flag 256 means: libvo driver does its timing (dvb card)

#ifdef TARGET_LINUX
    if(rtc_fd>=0){
	// -------- RTC -----------
        while (time_frame > 0.000) {
	    unsigned long long rtc_ts;
	    if (read (rtc_fd, &rtc_ts, sizeof(rtc_ts)) <= 0)
		    perror ("read (rtc_fd)");
    	    time_frame-=GetRelativeTime();
	}
    } else
#endif
    {
	// -------- USLEEP + SOFTSLEEP -----------
	float min=softsleep?0.021:0.005;
        while(time_frame>min){
          if(time_frame<=0.020)
             usec_sleep(0); // sleeps 1 clock tick (10ms)!
          else
             usec_sleep(1000000*(time_frame-0.020));
          time_frame-=GetRelativeTime();
        }
	if(softsleep){
	    if(time_frame<0) printf("Warning! softsleep underflow!\n");
	    while(time_frame>0) time_frame-=GetRelativeTime(); // burn the CPU
	}
    }

}

        current_module="flip_page";
#ifdef USE_LIBVO2
        if(blit_frame) vo2_flip(video_out,0);
#else
	video_out->check_events();
        if(blit_frame){
	   unsigned int t2=GetTimer();

	   float j;
#define	FRAME_LAG_WARN	0.2
	   j = ((float)t2 - lastframeout_ts) / 1000000;
	   lastframeout_ts = GetTimer();
	   if (j < frame_time + frame_time * -FRAME_LAG_WARN)
		too_fast_frame_cnt++;
		/* printf ("PANIC: too fast frame (%.3f)!\n", j); */
	   else if (j > frame_time + frame_time * FRAME_LAG_WARN)
		too_slow_frame_cnt++;
		/* printf ("PANIC: too slow frame (%.3f)!\n", j); */

	   video_out->flip_page();
	   t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;
	}
#endif
//        usec_sleep(50000); // test only!

    }

    current_module=NULL;
    
    if(eof) break;
    if(force_redraw){
      --force_redraw;
      if(!force_redraw) osd_function=OSD_PLAY;
      continue;
    }

//    printf("A:%6.1f  V:%6.1f  A-V:%7.3f  frame=%5.2f   \r",d_audio->pts,d_video->pts,d_audio->pts-d_video->pts,sh_audio->timer);
//    fflush(stdout);

#if 1
/*================ A-V TIMESTAMP CORRECTION: =========================*/
  if(sh_audio){
    float a_pts=0;
    float v_pts=0;

    // unplayed bytes in our and soundcard/dma buffer:
    int delay_bytes=audio_out->get_delay()+sh_audio->a_buffer_len;
    float delay=(float)delay_bytes/(float)sh_audio->o_bps;

    if(pts_from_bps){
#if 1
        unsigned int samples=(sh_audio->audio.dwSampleSize)?
          ((ds_tell(d_audio)-sh_audio->a_in_buffer_len)/sh_audio->audio.dwSampleSize) :
          (d_audio->pack_no); // <- used for VBR audio
        a_pts=samples*(float)sh_audio->audio.dwScale/(float)sh_audio->audio.dwRate;
#else
      if(sh_audio->audio.dwSampleSize)
        a_pts=(ds_tell(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->wf->nAvgBytesPerSec;
      else  // VBR:
        a_pts=d_audio->pack_no*(float)sh_audio->audio.dwScale/(float)sh_audio->audio.dwRate;
#endif
//      v_pts=d_video->pack_no*(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
//      printf("V_PTS: PTS: %8.3f BPS: %8.3f  \n",d_video->pts,v_pts);
      delay_corrected=1;
    } else {
      // PTS = (last timestamp) + (bytes after last timestamp)/(bytes per sec)
      a_pts=d_audio->pts;
      if(!delay_corrected) if(a_pts) delay_corrected=1;
      //printf("*** %5.3f ***\n",a_pts);
      a_pts+=(ds_tell_pts(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
//      v_pts=d_video->pts-frame_time;
//      v_pts=d_video->pts;
    }
    v_pts=d_video->pts;

      mp_dbg(MSGT_AVSYNC,MSGL_DBG2,"### A:%8.3f (%8.3f)  V:%8.3f  A-V:%7.4f  \n",a_pts,a_pts-audio_delay-delay,v_pts,(a_pts-delay-audio_delay)-v_pts);

      if(delay_corrected){
        float x;
	AV_delay=(a_pts-delay-audio_delay)-v_pts;
        x=AV_delay*0.1f;
        if(x<-max_pts_correction) x=-max_pts_correction; else
        if(x> max_pts_correction) x= max_pts_correction;
        if(default_max_pts_correction>=0)
          max_pts_correction=default_max_pts_correction;
        else
          max_pts_correction=sh_video->frametime*0.10; // +-10% of time
        sh_audio->timer+=x; c_total+=x;
        if(!quiet) mp_msg(MSGT_AVSYNC,MSGL_STATUS,"A:%6.1f V:%6.1f A-V:%7.3f ct:%7.3f  %3d/%3d  %2d%% %2d%% %4.1f%% %d %d %d%%\r",
	  a_pts-audio_delay-delay,v_pts,AV_delay,c_total,
          (int)sh_video->num_frames,(int)sh_video->num_frames_decoded,
          (sh_video->timer>0.5)?(int)(100.0*video_time_usage/(double)sh_video->timer):0,
          (sh_video->timer>0.5)?(int)(100.0*vout_time_usage/(double)sh_video->timer):0,
          (sh_video->timer>0.5)?(100.0*audio_time_usage/(double)sh_video->timer):0
          ,drop_frame_cnt
	  ,output_quality
	  ,cache_fill_status
        );
        fflush(stdout);
      }
    
  } else {
    // No audio:
    
    if(!quiet)
      mp_msg(MSGT_AVSYNC,MSGL_STATUS,"V:%6.1f  %3d  %2d%% %2d%% %4.1f%% %d %d %d%%\r",d_video->pts,
        (int)sh_video->num_frames,
        (sh_video->timer>0.5)?(int)(100.0*video_time_usage/(double)sh_video->timer):0,
        (sh_video->timer>0.5)?(int)(100.0*vout_time_usage/(double)sh_video->timer):0,
        (sh_video->timer>0.5)?(100.0*audio_time_usage/(double)sh_video->timer):0
          ,drop_frame_cnt
	  ,output_quality
	  ,cache_fill_status
        );

      fflush(stdout);

  }
#endif

/*Output quality adjustments:*/
if(auto_quality>0){
#if 0
  /*If we took a long time decoding this frame, downgrade the quality.*/
  if(output_quality>0&&
     (video_time_usage-cvideo_base_vtime)*sh_video->timer>=
     (0.95*sh_video->timer-(vout_time_usage+audio_time_usage))*
     (sh_video->timer-cvideo_base_vframe-frame_correction)){
    output_quality>>=1;
    mp_msg(MSGT_AUTOQ,MSGL_DBG2,"Downgrading quality to %i.\n",output_quality);
    set_video_quality(sh_video,output_quality);
  } else
  /*If we had plenty of extra time, upgrade the quality.*/
  if(output_quality<auto_quality&&
     vdecode_time<0.5*frame_time&&
     (video_time_usage-cvideo_base_vtime)*sh_video->timer<
     (0.67*sh_video->timer-(vout_time_usage+audio_time_usage))*
     (sh_video->timer-cvideo_base_vframe-frame_correction)){
    output_quality++;
    mp_msg(MSGT_AUTOQ,MSGL_DBG2,"Upgrading quality to %i.\n",output_quality);
    set_video_quality(sh_video,output_quality);
  }
#else
//  float total=0.000001f * (GetTimer()-aq_total_time);
//  if(output_quality<auto_quality && aq_sleep_time>0.05f*total)
  if(output_quality<auto_quality && aq_sleep_time>0)
      ++output_quality;
  else
//  if(output_quality>0 && aq_sleep_time<-0.05f*total)
  if(output_quality>1 && aq_sleep_time<0)
      --output_quality;
  else
  if(output_quality>0 && aq_sleep_time<-0.050f) // 50ms
      output_quality=0;
//  printf("total: %8.6f  sleep: %8.6f  q: %d\n",(0.000001f*aq_total_time),aq_sleep_time,output_quality);
  set_video_quality(sh_video,output_quality);
#endif
}

#ifdef USE_OSD
  if(osd_visible){
    if (!--osd_visible){ vo_osd_progbar_type=-1; // disable
       if (osd_function != OSD_PAUSE)
	   osd_function = OSD_PLAY;
    }
  }
#endif

  if(osd_function==OSD_PAUSE){
      int gui_pause_flag=0; // gany!
      mp_msg(MSGT_CPLAYER,MSGL_STATUS,"\n------ PAUSED -------\r");fflush(stdout);
#ifdef HAVE_NEW_GUI
      if(use_gui) mplShMem->Playing=2;
#endif
      if (audio_out && sh_audio)
         audio_out->pause();	// pause audio, keep data if possible
         while(
#ifdef HAVE_LIRC
             lirc_mp_getinput()<=0 &&
#endif
             (use_stdin || getch2(20)<=0) && mplayer_get_key()<=0){
#ifndef USE_LIBVO2
	     video_out->check_events();
#endif
#ifdef HAVE_NEW_GUI
             if(use_gui){
		EventHandling();
		if(mplShMem->Playing!=2 || (rel_seek_secs || abs_seek_pos))
		  { gui_pause_flag=1; break; } // end of pause or seek
             }
#endif
             if(use_stdin) usec_sleep(1000); // do not eat the CPU
         }
         osd_function=OSD_PLAY;
      if (audio_out && sh_audio)
        audio_out->resume();	// resume audio
      (void)GetRelativeTime();	// keep TF around FT in next cycle
#ifdef HAVE_NEW_GUI
      if(use_gui && !gui_pause_flag) mplShMem->Playing=1; // play from keyboard
#endif
  }


    if(!force_redraw) break;
  } //  while(sh_video->timer<sh_audio->timer || force_redraw)

// skip some seconds... added by fly

if(step_sec>0) {
	osd_function=OSD_FFW;
	rel_seek_secs+=step_sec;
}

//================= Keyboard events, SEEKing ====================

{ int c;
  while(
#ifdef HAVE_LIRC
      (c=lirc_mp_getinput())>0 ||
#endif
      (!use_stdin && (c=getch2(0))>0) || (c=mplayer_get_key())>0) switch(c){
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
    // seek 10 min
    case KEY_PAGE_UP:
      rel_seek_secs+=600;break;
    case KEY_PAGE_DOWN:
      rel_seek_secs-=600;break;
    // delay correction:
    case '+':
      audio_delay+=0.1;  // increase audio buffer delay
      if(sh_audio) sh_audio->timer-=0.1;
      break;
    case '-':
      audio_delay-=0.1;  // decrease audio buffer delay
      if(sh_audio) sh_audio->timer+=0.1;
      break;
    // quit
    case KEY_ESC: // ESC
    case 'q': exit_player(MSGTR_Exit_quit);
    case '>':
	if(curr_filename>=num_filenames-1)
		break;
    case KEY_ENTER: // ESC
      eof=1;  // jump to next file
      break;
    case '<':
	if(curr_filename < 1)
		break;
        curr_filename-=2;
	eof=1;
      break;
    case 'g': grab_frames=2;break;
    // pause
    case 'p':
    case ' ':
      osd_function=OSD_PAUSE;
      break;
    case 'o':  // toggle OSD
      osd_level=(osd_level+1)%3;
      break;
    case 'z':
      sub_delay -= 0.1;
      break;
    case 'x':
      sub_delay += 0.1;
      break;
    case '9':
    case '0':
    case '*':
    case '/': {
        if(c=='*' || c=='0'){
               mixer_incvolume();
        } else {
               mixer_decvolume();
        }

#ifdef USE_OSD
        if(osd_level){
          osd_visible=sh_video->fps; // 1 sec
          vo_osd_progbar_type=OSD_VOLUME;
          vo_osd_progbar_value=(mixer_getbothvolume()*256.0)/100.0;
          //printf("volume: %d\n",vo_osd_progbar_value);
        }
#endif
      }
      break; 
    case 'm':
      mixer_usemaster=!mixer_usemaster;
      break;

#if 0  // change to 1 for absolute seeking tests
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
	rel_seek_secs=0.99*(float)(c-'1')/7.0;
	abs_seek_pos=3;
	printf("ABS seek to %5.3f   \n",rel_seek_secs);
	break;
#else
    // Contrast:
    case '1':
    case '2':
        if(c=='2'){
	    if ( ++v_cont > 100 ) v_cont = 100;
        } else {
    	    if ( --v_cont < 0 ) v_cont = 0;	    
        }
	if(set_video_colors(sh_video,"Contrast",v_cont)){
#ifdef USE_OSD
    		if(osd_level){
            	    osd_visible=sh_video->fps; // 1 sec
	    	    vo_osd_progbar_type=OSD_CONTRAST;
            	    vo_osd_progbar_value=((v_cont)<<8)/100;
		}
#endif
	}
	break;

    // Brightness:
    case '3':
    case '4':
        if(c=='4'){
	    if ( ++v_bright > 100 ) v_bright = 100;
        } else {
    	    if ( --v_bright < 0 ) v_bright = 0;	    
        }
	if(set_video_colors(sh_video,"Brightness",v_bright)){
#ifdef USE_OSD
    		if(osd_level){
            	    osd_visible=sh_video->fps; // 1 sec
	    	    vo_osd_progbar_type=OSD_BRIGHTNESS;
            	    vo_osd_progbar_value=((v_bright)<<8)/100;
		}
#endif
	}
	break;

    // Hue:
    case '5':
    case '6':
        if(c=='6'){
	    if ( ++v_hue > 100 ) v_hue = 100;
        } else {
    	    if ( --v_hue < 0 ) v_hue = 0;	    
        }
	if(set_video_colors(sh_video,"Hue",v_hue)){
#ifdef USE_OSD
    		if(osd_level){
            	    osd_visible=sh_video->fps; // 1 sec
	    	    vo_osd_progbar_type=OSD_HUE;
            	    vo_osd_progbar_value=((v_hue)<<8)/100;
		}
#endif
	}
	break;

    // Saturation:
    case '7':
    case '8':
        if(c=='8'){
	    if ( ++v_saturation > 100 ) v_saturation = 100;
        } else {
    	    if ( --v_saturation < 0 ) v_saturation = 0;	    
        }
	if(set_video_colors(sh_video,"Saturation",v_saturation)){
#ifdef USE_OSD
    		if(osd_level){
            	    osd_visible=sh_video->fps; // 1 sec
	    	    vo_osd_progbar_type=OSD_SATURATION;
            	    vo_osd_progbar_value=((v_saturation)<<8)/100;
		}
#endif
	}
	break;
#endif

    case 'd':
      frame_dropping=(frame_dropping+1)%3;
      mp_msg(MSGT_CPLAYER,MSGL_V,"== drop: %d ==  \n",frame_dropping);
      break;
      
#ifdef USE_TV
    case 'h':
     if (tv_param_on == 1)
        tv_step_channel(tv_handler, TV_CHANNEL_HIGHER);
     break;
    case 'l':
     if (tv_param_on == 1)
        tv_step_channel(tv_handler, TV_CHANNEL_LOWER);
     break;
    case 'n':
     if (tv_param_on == 1)
	 tv_step_norm(tv_handler);
     break;
    case 'b':
     if (tv_param_on == 1)
        tv_step_chanlist(tv_handler);
     break;
#endif
  }
} // keyboard event handler

  if (seek_to_sec) {
    int a,b; float d;
    
    if (sscanf(seek_to_sec, "%d:%d:%f", &a,&b,&d)==3)
	rel_seek_secs += 3600*a +60*b +d ;
    else if (sscanf(seek_to_sec, "%d:%f", &a, &d)==2)
	rel_seek_secs += 60*a +d;
    else if (sscanf(seek_to_sec, "%f", &d)==1)
	rel_seek_secs += d;

     seek_to_sec = NULL;
  }
  
  /* Looping. */
  if(eof==5 && loop_times>-1) {

    if(loop_times!=0) {
      
      loop_times--;
      
      if(loop_times==0)
        loop_times=-1;
      
    }
    
    eof=0;
    abs_seek_pos=1;

    mp_msg(MSGT_CPLAYER,MSGL_V,"loop_times = %d, eof = 0\n", loop_times);
    
  }

if(rel_seek_secs || abs_seek_pos){
  current_module="seek";
  if(demux_seek(demuxer,rel_seek_secs,abs_seek_pos)){
      // success:

      if(sh_audio){
	if(verbose){
	    float a_pts=d_audio->pts;
            a_pts+=(ds_tell_pts(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	    mp_msg(MSGT_AVSYNC,MSGL_V,"SEEK: A: %5.3f  V: %5.3f  A-V: %5.3f   \n",a_pts,d_video->pts,a_pts-d_video->pts);
	}
        mp_msg(MSGT_AVSYNC,MSGL_STATUS,"A:%6.1f  V:%6.1f  A-V:%7.3f  ct: ?   \r",d_audio->pts,d_video->pts,0.0f);
      } else {
        mp_msg(MSGT_AVSYNC,MSGL_STATUS,"A: ---   V:%6.1f   \r",d_video->pts);
      }
      fflush(stdout);

      if(sh_audio){
        current_module="seek_audio_reset";
        audio_out->reset(); // stop audio, throwing away buffered data
      }

#ifdef USE_OSD
        // Set OSD:
      if(osd_level){
        int len=((demuxer->movi_end-demuxer->movi_start)>>8);
        if (len>0){
	   osd_visible=sh_video->fps; // 1 sec
	   vo_osd_progbar_type=0;
	   vo_osd_progbar_value=(demuxer->filepos-demuxer->movi_start)/len;
	}
      }
#endif
      
      c_total=0;
      max_pts_correction=0.1;
      osd_visible=sh_video->fps; // to rewert to PLAY pointer after 1 sec
      audio_time_usage=0; video_time_usage=0; vout_time_usage=0;
      drop_frame_cnt=0;
      too_slow_frame_cnt=0;
      too_fast_frame_cnt=0;
  }
  rel_seek_secs=0;
  abs_seek_pos=0;
  current_module=NULL;
}

#ifdef HAVE_NEW_GUI
      if(use_gui){
	if(demuxer->file_format==DEMUXER_TYPE_AVI && sh_video->video.dwLength>2){
	  // get pos from frame number / total frames
	  mplShMem->Position=(float)d_video->pack_no*100.0f/sh_video->video.dwLength;
	} else {
	  // get pos from file position / filesize
          int len=((demuxer->movi_end-demuxer->movi_start));
	  int pos=(demuxer->file_format==DEMUXER_TYPE_AVI)?demuxer->filepos:d_video->pos;
	  mplShMem->Position=(len<=0)?0:((float)(pos-demuxer->movi_start) / len * 100.0f);
	}
	mplShMem->TimeSec=d_video->pts; 
	if(mplShMem->Playing==0) break; // STOP
	if(mplShMem->Playing==2) osd_function=OSD_PAUSE;
	if ( mplShMem->VolumeChanged ) 
	 {
	  mixer_setvolume( mplShMem->Volume,mplShMem->Volume );
	  mplShMem->VolumeChanged=0;
#ifdef USE_OSD
          if ( osd_level )
	   {
            osd_visible=sh_video->fps; // 1 sec
            vo_osd_progbar_type=OSD_VOLUME;
            vo_osd_progbar_value=( ( mplShMem->Volume ) * 256.0 ) / 100.0;
           }
#endif
	 } 
	mplShMem->Volume=(float)mixer_getbothvolume();
      }
#endif


//================= Update OSD ====================
#ifdef USE_OSD
  if(osd_level>=2){
      int pts=d_video->pts;
      if(pts==osd_last_pts-1) ++pts; else osd_last_pts=pts;
      vo_osd_text=osd_text_buffer;
      sprintf(vo_osd_text,"%c %02d:%02d:%02d",osd_function,pts/3600,(pts/60)%60,pts%60);
  } else {
      vo_osd_text=NULL;
  }
//  for(i=1;i<=11;i++) osd_text_buffer[10+i]=i;osd_text_buffer[10+i]=0;
//  vo_osd_text=osd_text_buffer;
#endif
  
#ifdef USE_SUB
  // find sub
  if(subtitles && d_video->pts>0){
      float pts=d_video->pts;
      if(sub_fps==0) sub_fps=sh_video->fps;
      current_module="find_sub";
      find_sub(subtitles,sub_uses_time?(100*(pts+sub_delay)):((pts+sub_delay)*sub_fps)); // FIXME! frame counter...
      current_module=NULL;
  }
#endif
  
  // DVD sub:
  { unsigned char* packet=NULL;
    int len=ds_get_packet_sub(d_dvdsub,&packet);
    if(len>=2){
      int len2;
      len2=(packet[0]<<8)+packet[1];
      mp_msg(MSGT_CPLAYER,MSGL_V,"\rDVD sub: %d / %d  \n",len,len2);
      if(len==len2)
        spudec_decode(packet,len);
      else
        mp_msg(MSGT_CPLAYER,MSGL_V,"fragmented dvd-subs not yet supported!!!\n");
    } else if(len>=0) {
      mp_msg(MSGT_CPLAYER,MSGL_V,"invalid dvd sub\n");
    }
  }
  
} // while(!eof)

mp_msg(MSGT_GLOBAL,MSGL_V,"EOF code: %d  \n",eof);

}


if(curr_filename+1<num_filenames || use_gui){
    // partial uninit:

  uninit_player(INITED_ALL-(INITED_GUI+INITED_LIRC));

}

goto_next_file:  // don't jump here after ao/vo/getch initialization!

#ifdef HAVE_NEW_GUI
      if(use_gui) 
       {
        mplStop();
       }	
#endif

if(use_gui || ++curr_filename<num_filenames){

  current_module="uninit_vcodec";
  if(sh_video) uninit_video(sh_video);

  current_module="free_demuxer";
  if(demuxer) free_demuxer(demuxer);

  current_module="free_stream";
  if(stream) free_stream(stream);

  video_out=NULL;
  audio_out=NULL;

  goto play_next_file;
}

exit_player(MSGTR_Exit_eof);

return 1;
}
