// AVI & MPEG Player    v0.18   (C) 2000-2001. by A'rpi/ESP-team

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "version.h"
#include "config.h"

#ifdef __FreeBSD__
#include <sys/cdrio.h>
#endif

#ifdef sun
#define	DEFAULT_CDROM_DEVICE	"/vol/dev/aliases/cdrom0"
#else
#define DEFAULT_CDROM_DEVICE    "/dev/cdrom"
#endif

#include "cfgparser.h"
#include "cfg-mplayer-def.h"

#include "subreader.h"

#include "libvo/video_out.h"
#include "libvo/sub.h"

#include "libao2/audio_out.h"

#include "libmpeg2/mpeg2.h"
#include "libmpeg2/mpeg2_internal.h"

#include "loader.h"
#include "wine/avifmt.h"

#include "codec-cfg.h"

#include "dvdauth.h"
#include "spudec.h"

extern char* win32_codec_name;  // must be set before calling DrvOpen() !!!

#include "linux/getch2.h"
#include "linux/keycodes.h"
#include "linux/timer.h"
#include "linux/shmem.h"

#ifdef HAVE_LIRC
#include "lirc_mp.h"
#endif

#include "help_mp.h"

#ifdef STREAMING
#include "url.h"
#include "network.h"
static URL_t* url;
#endif


#define DEBUG if(0)
#ifdef HAVE_GUI
 int nogui=1;
#endif
int verbose=0;

#define ABS(x) (((x)>=0)?(x):(-(x)))

void find_sub(subtitle* subtitles,int key);

static int
usec_sleep(int usec_delay)
{
#if	1
    struct timespec ts;
    ts.tv_sec  =  usec_delay / 1000000;
    ts.tv_nsec = (usec_delay % 1000000) * 1000;
    return nanosleep(&ts, NULL);
#else
    return usleep(usec_delay);
#endif
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

//**************************************************************************//
//**************************************************************************//
//             Input media streaming & demultiplexer:
//**************************************************************************//

static int max_framesize=0;

#include "stream.h"
#include "demuxer.h"

#include "stheader.h"

char* encode_name=NULL;
char* encode_index_name=NULL;
int encode_bitrate=0;

extern int asf_packetsize; // for seeking

extern float avi_audio_pts;
extern float avi_video_pts;
extern float avi_video_ftime;
extern int skip_video_frames;

void read_avi_header(demuxer_t *demuxer,int index_mode);
demux_stream_t* demux_avi_select_stream(demuxer_t *demux,unsigned int id);

int asf_check_header(demuxer_t *demuxer);
int read_asf_header(demuxer_t *demuxer);

demuxer_t* demux_open(stream_t *stream,int file_format);
int demux_seek(demuxer_t *demuxer,float rel_seek_secs,int flags);

// MPEG video stream parser:
#include "parse_es.h"

extern int num_elementary_packets100; // for MPEG-ES fileformat detection
extern int num_elementary_packets101;
extern int num_elementary_packetsPES;

extern picture_t *picture;	// exported from libmpeg2/decode.c

int frameratecode2framerate[16] = {
  0,
  // Official mpeg1/2 framerates:
  24000*10000/1001, 24*10000,25*10000, 30000*10000/1001, 30*10000,50*10000,60000*10000/1001, 60*10000,
  // libmpeg3's "Unofficial economy rates":
  1*10000,5*10000,10*10000,12*10000,15*10000,0,0
};

//**************************************************************************//
//**************************************************************************//

// Common FIFO functions, and keyboard/event FIFO code
#include "fifo.c"

//**************************************************************************//

static vo_functions_t *video_out=NULL;
static ao_functions_t *audio_out=NULL;

float c_total=0;

double video_time_usage=0;
double vout_time_usage=0;
static double audio_time_usage=0;
static int total_time_usage_start=0;
static int benchmark=0;

static int play_in_bg=0;

extern void avi_fixate();

#ifdef HAVE_GUI
 #include "../Gui/mplayer/psignal.h"
 #define GUI_MSG(x) if ( !nogui ) { mplSendMessage( x ); usec_sleep( 10000 ); }
#else
 #define GUI_MSG(x)
#endif

// options:
int osd_level=2;
int divx_quality=0;
char *seek_to_sec=NULL;
int seek_to_byte=0;
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

//#ifdef ALSA_TIMER
//int alsa=1;
//#else
//int alsa=0;
//#endif

// streaming:
int audio_id=-1;
int video_id=-1;
int dvdsub_id=-1;
int vcd_track=0;
char *stream_dump_name=NULL;
int stream_dump_type=0;
int index_mode=-1;  // -1=untouched  0=don't use index  1=use (geneate) index
int force_ni=0;

float default_max_pts_correction=-1;//0.01f;
float max_pts_correction=0;//default_max_pts_correction;
#ifdef AVI_SYNC_BPS
int pts_from_bps=1;
#else
int pts_from_bps=0;
#endif

float force_fps=0;
int force_srate=0;
float audio_delay=0;
int frame_dropping=0; // option  0=no drop  1= drop vo  2= drop decode
int play_n_frames=-1;

// screen info:
char* video_driver=NULL; //"mga"; // default
char* audio_driver=NULL;
int fullscreen=0;
int vidmode=0;
int softzoom=0;
int flip=-1;
int screen_size_x=0;//SCREEN_SIZE_X;
int screen_size_y=0;//SCREEN_SIZE_Y;
int screen_size_xy=0;

// sub:
char *font_name=NULL;
float font_factor=0.75;
char *sub_name=NULL;
float sub_delay=0;
float sub_fps=0;
int   sub_auto = 1;
/*DSP!!char *dsp=NULL;*/

float rel_seek_secs=0;
float initial_pts_delay=0;

extern char *vo_subdevice;
extern char *ao_subdevice;

void exit_player(char* how){
 total_time_usage_start=GetTimer()-total_time_usage_start;

#ifdef HAVE_GUI
 if ( !nogui )
  {
   if ( how != NULL )
    {
     if ( !strcmp( how,"Quit" ) ) mplSendMessage( mplEndOfFile );
     if ( !strcmp( how,"End of file" ) ) mplSendMessage( mplEndOfFile );
     if ( !strcmp( how,"audio_init" ) ) mplSendMessage( mplAudioError );
    }
    else mplSendMessage( mplUnknowError );
  }
#endif

  if(how) printf("\nExiting... (%s)\n",how);
  if(verbose) printf("max framesize was %d bytes\n",max_framesize);
  if(benchmark){
      double tot=video_time_usage+vout_time_usage+audio_time_usage;
      double total_time_usage=(float)total_time_usage_start*0.000001;
      printf("BENCHMARKs: V:%8.3fs VO:%8.3fs A:%8.3fs Sys:%8.3fs = %8.3fs\n",
          video_time_usage,vout_time_usage,audio_time_usage,
	  total_time_usage-tot,total_time_usage);
      if(total_time_usage>0.0)
      printf("BENCHMARK%%: V:%8.4f%% VO:%8.4f%% A:%8.4f%% Sys:%8.4f%% = %8.4f%%\n",
          100.0*video_time_usage/total_time_usage,
	  100.0*vout_time_usage/total_time_usage,
	  100.0*audio_time_usage/total_time_usage,
	  100.0*(total_time_usage-tot)/total_time_usage,
	  100.0);
  }
  // restore terminal:
  #ifdef HAVE_GUI
   if ( nogui )
  #endif
     getch2_disable();
  if(video_out) video_out->uninit();
  if(audio_out) audio_out->uninit();
  if(encode_name) avi_fixate();
#ifdef HAVE_LIRC
  #ifdef HAVE_GUI
   if ( nogui )
  #endif
  lirc_mp_cleanup();
#endif
  exit(1);
}

char* current_module=NULL; // for debugging

void exit_sighandler(int x){
  static int sig_count=0;
  ++sig_count;
  if(sig_count==2) exit(1);
  if(sig_count>2){
    // can't stop :(
    kill(getpid(),SIGKILL);
  }
  fprintf(stderr,"\nMPlayer interrupted by signal %d in module: %s \n",x,
      current_module?current_module:"unknown"
  );
  #ifdef HAVE_GUI
   if ( !nogui )
    {
     mplShMem->items.error.signal=x;
     strcpy( mplShMem->items.error.module,current_module?current_module:"unknown" );
    }
  #endif
  exit_player(NULL);
}

extern int vcd_get_track_end(int fd,int track);
extern void write_avi_header_1(FILE *f,int fcc,float fps,int width,int height);

// dec_audio.c:
extern int init_audio(sh_audio_t *sh_audio);
extern int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen);
extern void resync_audio_stream(sh_audio_t *sh_audio);
extern void skip_audio_frame(sh_audio_t *sh_audio);

// dec_video.c:
extern int init_video(sh_video_t *sh_video);
extern int decode_video(vo_functions_t *video_out,sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame);

#include "mixer.h"
#include "cfg-mplayer.h"

void parse_cfgfiles( void )
{
char *conffile;
int conffile_fd;
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
}

#ifndef HAVE_GUI
 int main(int argc,char* argv[], char *envp[]){
#else
 int mplayer(int argc,char* argv[], char *envp[]){
#endif

static subtitle* subtitles=NULL;

static demuxer_t *demuxer=NULL;

static demux_stream_t *d_audio=NULL;
static demux_stream_t *d_video=NULL;
static demux_stream_t *d_dvdsub=NULL;

static sh_audio_t *sh_audio=NULL;
static sh_video_t *sh_video=NULL;

char* filename=NULL; //"MI2-Trailer.avi";
stream_t* stream=NULL;
int file_format=DEMUXER_TYPE_UNKNOWN;
//
int delay_corrected=1;
#ifdef VCD_CACHE
int vcd_cache_size=128;
#endif
#ifdef __FreeBSD__
int bsize = VCD_SECTOR_SIZE;
#endif
char* title="MPlayer";

// movie info:
int out_fmt=0;

int osd_visible=100;
int osd_function=OSD_PLAY;
int osd_last_pts=-303;

//float a_frame=0;    // Audio

float rel_seek_secs=0;

int i;
int f; // filedes

  printf("%s",banner_text);

#ifdef HAVE_GUI
  if ( nogui )
   {
#endif
    parse_cfgfiles();
    if (parse_command_line(conf, argc, argv, envp, &filename) < 0) exit(1);

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
#ifdef HAVE_GUI
   }
#endif

if(!filename){
  if(vcd_track) filename=DEFAULT_CDROM_DEVICE;
  else {
    printf("%s",help_text); exit(0);
  }
}

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
  if(!video_out){
    fprintf(stderr,"Invalid video output driver name: %s\nUse '-vo help' to get a list of available video drivers.\n",video_driver);
    return 0;
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
    fprintf(stderr,"Invalid audio output driver name: %s\nUse '-ao help' to get a list of available audio drivers.\n",audio_driver);
    return 0;
  }
/*DSP!!  if(dsp) audio_out->control(AOCONTROL_SET_DEVICE,(int)dsp);*/

// check codec.conf
if(!parse_codec_cfg(get_path("codecs.conf"))){
  if(!parse_codec_cfg(DATADIR"/codecs.conf")){
    printf("(copy/link DOCS/codecs.conf to ~/.mplayer/codecs.conf)\n");
    GUI_MSG( mplCodecConfNotFound )
    exit(1);
  }
}

// check font
  if(font_name){
       vo_font=read_font_desc(font_name,font_factor,verbose>1);
       if(!vo_font) fprintf(stderr,"Can't load font: %s\n",font_name);
  } else {
      // try default:
       vo_font=read_font_desc(get_path("font/font.desc"),font_factor,verbose>1);
       if(!vo_font)
       vo_font=read_font_desc(DATADIR"/font/font.desc",font_factor,verbose>1);
  }

// check .sub
  if(sub_name){
       subtitles=sub_read_file(sub_name);
       if(!subtitles) fprintf(stderr,"Can't load subtitles: %s\n",sub_name);
  } else {
    if ( sub_auto )
      {
       // auto load sub file ...
       subtitles=sub_read_file( sub_filename( get_path("sub/"), filename ) );
      }
      if ( subtitles == NULL ) subtitles=sub_read_file(get_path("default.sub")); // try default:
  }


if(vcd_track){
//============ Open VideoCD track ==============
  int ret,ret2;
  f=open(filename,O_RDONLY);
  if(f<0){ fprintf(stderr,"CD-ROM Device '%s' not found!\n",filename);return 1; }
  vcd_read_toc(f);
  ret2=vcd_get_track_end(f,vcd_track);
  if(ret2<0){ fprintf(stderr,"Error selecting VCD track! (get)\n");return 1;}
  ret=vcd_seek_to_track(f,vcd_track);
  if(ret<0){ fprintf(stderr,"Error selecting VCD track! (seek)\n");return 1;}
  seek_to_byte+=ret;
  if(verbose) printf("VCD start byte position: 0x%X  end: 0x%X\n",seek_to_byte,ret2);
#ifdef VCD_CACHE
  vcd_cache_init(vcd_cache_size);
#endif
#ifdef __FreeBSD__
  if (ioctl (f, CDRIOCSETBLOCKSIZE, &bsize) == -1) {
        perror ( "Error in CDRIOCSETBLOCKSIZE");
  }
#endif
  stream=new_stream(f,STREAMTYPE_VCD);
  stream->start_pos=ret;
  stream->end_pos=ret2;
} else {
//============ Open plain FILE ============
  int len;
  if(!strcmp(filename,"-")){
      // read from stdin
      printf("Reading from stdin...\n");
      f=0; // 0=stdin
      stream=new_stream(f,STREAMTYPE_STREAM);
  } else {
#ifdef STREAMING
      url = url_new(filename);
      if(url==NULL) {
       // failed to create a new URL, so it's not an URL (or a malformed URL)
#endif
       f=open(filename,O_RDONLY);
       if(f<0){ fprintf(stderr,"File not found: '%s'\n",filename);return 1; }
       len=lseek(f,0,SEEK_END); lseek(f,0,SEEK_SET);
       stream=new_stream(f,STREAMTYPE_FILE);
       stream->end_pos=len;
#ifdef STREAMING
      } else {
        file_format=autodetectProtocol( url, &f );
        if( file_format==DEMUXER_TYPE_UNKNOWN ) { 
          fprintf(stderr,"Unable to open URL: %s\n", filename);
          url_free(url);
          return 1; 
        } else {
          f=streaming_start( &url, f, file_format );
          if(f<0){ fprintf(stderr,"Unable to open URL: %s\n", url->url); return 1; }
          printf("Connected to server: %s\n", url->hostname );
        }
        stream=new_stream(f,STREAMTYPE_STREAM);
      }
#endif
  }
}

#ifdef HAVE_LIBCSS
  if (dvdimportkey) {
    if (dvd_import_key(dvdimportkey)) {
	fprintf(stderr,"Error processing DVD KEY.\n");
        GUI_MSG( mplErrorDVDKeyProcess )
	exit(1);
    }
    printf("DVD command line requested key is stored for descrambling.\n");
  }
  if (dvd_auth_device) {
//  if (dvd_auth(dvd_auth_device,f)) {
    if (dvd_auth(dvd_auth_device,filename)) {
        GUI_MSG( mplErrorDVDAuth )
        exit(0);
      } 
    printf("DVD auth sequence seems to be OK.\n");
  }
#endif

//============ Open & Sync stream and detect file format ===============

if(!has_audio) audio_id=-2; // do NOT read audio packets...

demuxer=demux_open(stream,file_format);
if(!demuxer) exit(1); // ERROR

file_format=demuxer->file_format;

d_audio=demuxer->audio;
d_video=demuxer->video;
d_dvdsub=demuxer->sub;

// DUMP STREAMS:
if(stream_dump_type){
  FILE *f;
  demux_stream_t *ds=NULL;
  // select stream to dump
  switch(stream_dump_type){
  case 1: ds=d_audio;break;
  case 2: ds=d_video;break;
  case 3: ds=d_dvdsub;break;
  }
  if(!ds){        
      fprintf(stderr,"dump: FATAL: selected stream missing!\n");
      exit(1);
  }
  // disable other streams:
  if(d_audio && d_audio!=ds) {ds_free_packs(d_audio); d_audio->id=-2; }
  if(d_video && d_video!=ds) {ds_free_packs(d_video); d_video->id=-2; }
  if(d_dvdsub && d_dvdsub!=ds) {ds_free_packs(d_dvdsub); d_dvdsub->id=-2; }
  // let's dump it!
  f=fopen(stream_dump_name?stream_dump_name:"stream.dump","wb");
  if(!f){ fprintf(stderr,"Can't open dump file!!!\n");exit(1); }
  while(!ds->eof){
    unsigned char* start;
    int in_size=ds_get_packet(ds,&start);
    if( (file_format==DEMUXER_TYPE_AVI || file_format==DEMUXER_TYPE_ASF)
	&& stream_dump_type==2) fwrite(&in_size,1,4,f);
    if(in_size>0) fwrite(start,in_size,1,f);
  }
  fclose(f);
  printf("core dumped :)\n");
  exit(1);
}

sh_audio=d_audio->sh;
sh_video=d_video->sh;

if(sh_video){

  if(!video_read_properties(sh_video)) exit(1); // couldn't read header?

  printf("[V] filefmt:%d  fourcc:0x%X  size:%dx%d  fps:%5.2f  ftime:=%6.4f\n",
   file_format,sh_video->format, sh_video->disp_w,sh_video->disp_h,
   sh_video->fps,sh_video->frametime
  );

  if(!sh_video->fps && !force_fps){
    fprintf(stderr,"FPS not specified (or invalid) in the header! Use the -fps option!\n");
    exit(1);
  }

}

fflush(stdout);

if(!sh_video){
    fprintf(stderr,"Sorry, no video stream... it's unplayable yet\n");
    exit(1);
}

//================== Init AUDIO (codec) ==========================
if(sh_audio){
  // Go through the codec.conf and find the best codec...
  sh_audio->codec=NULL;
  if(audio_family!=-1) printf("Trying to force audio codec driver family %d ...\n",audio_family);
  while(1){
    sh_audio->codec=find_codec(sh_audio->format,NULL,sh_audio->codec,1);
    if(!sh_audio->codec){
      if(audio_family!=-1) {
        sh_audio->codec=NULL; /* re-search */
        printf("Can't find audio codec for forced driver family, fallback to other drivers.\n");
        audio_family=-1;
        continue;      
      }
      printf("Can't find codec for audio format 0x%X !\n",sh_audio->format);
      printf("*** Try to upgrade %s from DOCS/codecs.conf\n",get_path("codecs.conf"));
      printf("*** If it's still not OK, then read DOCS/CODECS!\n");
      sh_audio=NULL;
      break;
    }
    if(audio_codec && strcmp(sh_audio->codec->name,audio_codec)) continue;
    else if(audio_family!=-1 && sh_audio->codec->driver!=audio_family) continue;
    printf("%s audio codec: [%s] drv:%d (%s)\n",audio_codec?"Forcing":"Detected",sh_audio->codec->name,sh_audio->codec->driver,sh_audio->codec->info);
    break;
  }
}

if(sh_audio){
  if(verbose) printf("Initializing audio codec...\n");
  if(!init_audio(sh_audio)){
    printf("Couldn't initialize audio codec! -> nosound\n");
    sh_audio=0;
  } else {
    printf("AUDIO: srate=%d  chans=%d  bps=%d  sfmt=0x%X  ratio: %d->%d\n",sh_audio->samplerate,sh_audio->channels,sh_audio->samplesize,
        sh_audio->sample_format,sh_audio->i_bps,sh_audio->o_bps);
  }
}

//================== Init VIDEO (codec & libvo) ==========================

// Go through the codec.conf and find the best codec...
sh_video->codec=NULL;
if(video_family!=-1) printf("Trying to force video codec driver family %d ...\n",video_family);
while(1){
  sh_video->codec=find_codec(sh_video->format,
    sh_video->bih?((unsigned int*) &sh_video->bih->biCompression):NULL,sh_video->codec,0);
  if(!sh_video->codec){
    if(video_family!=-1) {
      sh_video->codec=NULL; /* re-search */
      printf("Can't find video codec for forced driver family, fallback to other drivers.\n");
      video_family=-1;
      continue;      
    }
    printf("Can't find codec for video format 0x%X !\n",sh_video->format);
      printf("*** Try to upgrade %s from DOCS/codecs.conf\n",get_path("codecs.conf"));
      printf("*** If it's still not OK, then read DOCS/CODECS!\n");
    #ifdef HAVE_GUI
     if ( !nogui )
      {
       mplShMem->items.videodata.format=sh_video->format;
       mplSendMessage( mplCantFindCodecForVideoFormat );
       usec_sleep( 10000 );
      }
    #endif
    exit(1);
  }
  // is next line needed anymore? - atmos ::
  if(!allow_dshow && sh_video->codec->driver==4) continue; // skip DShow
  else if(video_codec && strcmp(sh_video->codec->name,video_codec)) continue;
  else if(video_family!=-1 && sh_video->codec->driver!=video_family) continue;
  break;
}

printf("%s video codec: [%s] drv:%d (%s)\n",video_codec?"Forcing":"Detected",sh_video->codec->name,sh_video->codec->driver,sh_video->codec->info);

for(i=0;i<CODECS_MAX_OUTFMT;i++){
    int ret;
    out_fmt=sh_video->codec->outfmt[i];
    if(out_fmt==0xFFFFFFFF) continue;
    ret=video_out->query_format(out_fmt);
    if(verbose) printf("vo_debug: query(%s) returned 0x%X\n",vo_format_name(out_fmt),ret);
    if(ret) break;
}
if(i>=CODECS_MAX_OUTFMT){
    fprintf(stderr,"Sorry, selected video_out device is incompatible with this codec.\n");
    GUI_MSG( mplIncompatibleVideoOutDevice )
    exit(1);
}
sh_video->outfmtidx=i;

if(flip==-1){
    // autodetect flipping
    flip=0;
    if(sh_video->codec->outflags[i]&CODECS_FLAG_FLIP)
      if(!(sh_video->codec->outflags[i]&CODECS_FLAG_NOFLIP))
         flip=1;
}

if(verbose) printf("vo_debug1: out_fmt=%s\n",vo_format_name(out_fmt));

if(!init_video(sh_video)){
     fprintf(stderr,"FATAL: Couldn't initialize video codec :(\n");
     GUI_MSG( mplUnknowError )
     exit(1);
}

// ================== Init output files for encoding ===============
   if(encode_name){
     // encode file!!!
     FILE *encode_file=fopen(encode_name,"rb");
     if(encode_file){
       fclose(encode_file);
       fprintf(stderr,"File already exists: %s (don't overwrite your favourite AVI!)\n",encode_name);
       return 0;
     }
     encode_file=fopen(encode_name,"wb");
     if(!encode_file){
       fprintf(stderr,"Cannot create file for encoding\n");
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
     sh_audio=0; // disable audio !!!!!
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
     printf("VO: [%s] %dx%d => %dx%d %s %s%s%s%s\n",info->short_name,
         sh_video->disp_w,sh_video->disp_h,
         screen_size_x,screen_size_y,
	 vo_format_name(out_fmt),
         fullscreen?"fs ":"",
         vidmode?"vm ":"",
         softzoom?"zoom ":"",
         (flip==1)?"flip ":""
//         fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3)
     );
    printf("VO: Description: %s\n"
           "VO: Author: %s\n",
        info->name,
        info->author	
     );
    if(strlen(info->comment) > 0)
        printf("VO: Comment: %s\n", info->comment);
   }

   if(verbose) printf("video_out->init(%dx%d->%dx%d,flags=%d,'%s',0x%X)\n",
                      sh_video->disp_w,sh_video->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      title,out_fmt);

   #ifdef HAVE_GUI
    if ( !nogui )
     {
      mplShMem->items.videodata.width=sh_video->disp_w;
      mplShMem->items.videodata.height=sh_video->disp_h;
      mplSendMessage( mplSetVideoData );
     }
   #endif

   if(video_out->init(sh_video->disp_w,sh_video->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      title,out_fmt)){
     fprintf(stderr,"FATAL: Cannot initialize video driver!\n");
     GUI_MSG( mplCantInitVideoDriver )
     exit(1);
   }
   if(verbose) printf("INFO: Video OUT driver init OK!\n");

   fflush(stdout);
   
//================== MAIN: ==========================
{

float frame_correction=0; // average of A-V timestamp differences
int frame_corr_num=0;   //
//float v_frame=0;    // Video
float time_frame=0; // Timer
int eof=0;
int force_redraw=0;
//float num_frames=0;      // number of frames played
int grab_frames=0;
char osd_text_buffer[64];
int drop_frame=0;
int drop_frame_cnt=0;

#ifdef HAVE_LIRC
 #ifdef HAVE_GUI
  if ( nogui )
 #endif
  lirc_mp_setup();
#endif

  #ifdef HAVE_GUI
   if ( nogui )
    {
  #endif
#ifdef USE_TERMCAP
  load_termcap(NULL); // load key-codes
#endif
  if(f) getch2_enable();
  #ifdef HAVE_GUI
   }
  #endif 

  //========= Catch terminate signals: ================
  // terminate requests:
  signal(SIGTERM,exit_sighandler); // kill
  signal(SIGHUP,exit_sighandler);  // kill -HUP  /  xterm closed

  #ifdef HAVE_GUI
   if ( nogui )
  #endif
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

if(sh_audio){
  
  const ao_info_t *info=audio_out->info;
  printf("AO: [%s] %iHz %s %s\n"
         "AO: Description: %s\n"
         "AO: Author: %s\n",
      info->short_name,
      force_srate?force_srate:sh_audio->samplerate,
      sh_audio->channels>1?"Stereo":"Mono",
      audio_out_format_name(sh_audio->sample_format),
      info->name,
      info->author	
   );
  if(strlen(info->comment) > 0)
      printf("AO: Comment: %s\n", info->comment);

  if(!audio_out->init(force_srate?force_srate:sh_audio->samplerate,
      sh_audio->channels,sh_audio->sample_format,0)){
    printf("couldn't open/init audio device -> NOSOUND\n");
    sh_audio=0; audio_out=NULL;
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
  if(sh_audio) sh_audio->timer=0;

if(!sh_audio){
  printf("Audio: no sound\n");
  if(verbose) printf("Freeing %d unused audio chunks\n",d_audio->packs);
  ds_free_packs(d_audio); // free buffered chunks
  d_audio->id=-2;         // do not read audio chunks
  if(sh_audio) if(sh_audio->a_buffer) free(sh_audio->a_buffer);
  if(audio_out){ audio_out->uninit(); audio_out=NULL;} // close device
}

  current_module=NULL;

//==================== START PLAYING =======================

if(file_format==DEMUXER_TYPE_AVI && sh_audio){
  //a_pts=d_audio->pts;
  if(verbose) printf("Initial frame delay  A: %d  V: %d\n",(int)sh_audio->audio.dwInitialFrames,(int)sh_video->video.dwInitialFrames);
  if(!pts_from_bps){
    float x=(float)(sh_audio->audio.dwInitialFrames-sh_video->video.dwInitialFrames)*sh_video->frametime;
    audio_delay-=x;
    if(verbose) printf("AVI Initial frame delay: %5.3f\n",x);
  }
  if(verbose){
//    printf("v: audio_delay=%5.3f  buffer_delay=%5.3f  a_pts=%5.3f  sh_audio->timer=%5.3f\n",
//             audio_delay,audio_buffer_delay,a_pts,sh_audio->timer);
    printf("START:  a_pts=%5.3f  v_pts=%5.3f  \n",d_audio->pts,d_video->pts);
  }
  delay_corrected=0; // has to correct PTS diffs
  d_video->pts=0;d_audio->pts=0; // PTS is outdated now!
} else {
  pts_from_bps=0; // it must be 0 for mpeg/asf !
}
if(force_fps){
  sh_video->fps=force_fps;
  sh_video->frametime=1.0f/sh_video->fps;
  printf("FPS forced to be %5.3f  (ftime: %5.3f)\n",sh_video->fps,sh_video->frametime);
}

printf("Start playing...\n");fflush(stdout);

InitTimer();

total_time_usage_start=GetTimer();

while(!eof){

    if(play_n_frames>=0){
      --play_n_frames;
      if(play_n_frames<0) exit_player("Requested number of frames played");
    }

/*========================== PLAY AUDIO ============================*/
while(sh_audio){
  unsigned int t;
  int playsize=audio_out->get_space();
  
  if(!playsize) break; // buffer is full, do not block here!!!
  
  if(playsize>MAX_OUTBURST) playsize=MAX_OUTBURST; // we shouldn't exceed it!
  //if(playsize>outburst) playsize=outburst;

  // Update buffer if needed
  t=GetTimer();
  current_module="decode_audio";   // Enter AUDIO decoder module
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

if(1)
  while(1){
  
    float frame_time=1;
    float pts1=d_video->pts;
    int blit_frame=0;

    current_module="decode_video";
    
  //--------------------  Decode a frame: -----------------------

  if(file_format==DEMUXER_TYPE_MPEG_ES || file_format==DEMUXER_TYPE_MPEG_PS){
        int in_frame=0;
        float newfps;
        videobuf_len=0;
        while(videobuf_len<VIDEOBUFFER_SIZE-MAX_VIDEO_PACKET_SIZE){
          int i=sync_video_packet(d_video);
	  void* buffer=&videobuffer[videobuf_len+4];
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
	  if(sh_video->codec->driver!=1){
	    // if not libmpeg2:
	    switch(i){
	      case 0x1B3: header_process_sequence_header (picture, buffer);break;
	      case 0x1B5: header_process_extension (picture, buffer);break;
	    }
	  }
        }
        
        if(videobuf_len>max_framesize) max_framesize=videobuf_len; // debug
        //printf("--- SEND %d bytes\n",videobuf_len);
	if(grab_frames==1){
	      FILE *f=fopen("grab.mpg","ab");
	      fwrite(videobuffer,videobuf_len-4,1,f);
	      fclose(f);
	}

    blit_frame=decode_video(video_out,sh_video,videobuffer,videobuf_len,drop_frame);

    // get mpeg fps:
    newfps=frameratecode2framerate[picture->frame_rate_code]*0.0001f;
    if(ABS(sh_video->fps-newfps)>0.01f) if(!force_fps){
            printf("Warning! FPS changed %5.3f -> %5.3f  (%f) [%d]  \n",sh_video->fps,newfps,sh_video->fps-newfps,picture->frame_rate_code);
            sh_video->fps=newfps;
            sh_video->frametime=10000.0f/(float)frameratecode2framerate[picture->frame_rate_code];
    }

    // fix mpeg2 frametime:
    frame_time=(100+picture->repeat_count)*0.01f;
    picture->repeat_count=0;

  } else {
      // frame-based file formats: (AVI,ASF,MOV)
    unsigned char* start=NULL;
    int in_size=ds_get_packet(d_video,&start);
    if(in_size<0){ eof=1;break;}
    if(in_size>max_framesize) max_framesize=in_size;
    blit_frame=decode_video(video_out,sh_video,start,in_size,drop_frame);
  }

//------------------------ frame decoded. --------------------

    // Increase video timers:
    sh_video->num_frames+=frame_time;
    frame_time*=sh_video->frametime;
    if(file_format==DEMUXER_TYPE_ASF && !force_fps){
        // .ASF files has no fixed FPS - just frame durations!
        float d=d_video->pts-pts1;
        if(d>=0 && d<5) frame_time=d;
        if(d>0){
          if(verbose)
            if((int)sh_video->fps==1000)
              printf("\rASF framerate: %d fps             \n",(int)(1.0f/d));
          sh_video->frametime=d; // 1ms
          sh_video->fps=1.0f/d;
        }
    }
    sh_video->timer+=frame_time;
    time_frame+=frame_time;  // for nosound

    if(file_format==DEMUXER_TYPE_MPEG_PS) d_video->pts+=frame_time;
    
    if(verbose>1) printf("*** ftime=%5.3f ***\n",frame_time);

    if(drop_frame){

      if(sh_audio && !d_audio->eof){
          int delay=audio_out->get_delay();
          if(verbose>1)printf("delay=%d\n",delay);
          time_frame=sh_video->timer;
          time_frame-=sh_audio->timer-(float)delay/(float)sh_audio->o_bps;
	  if(time_frame>-2*frame_time) {
	    drop_frame=0; // stop dropping frames
	    if (verbose>0) printf("\nstop frame drop %.2f\n", time_frame);
	  }else{
	    ++drop_frame_cnt;
	    if (verbose > 0 && drop_frame_cnt%10 == 0)
	      printf("\nstill dropping, %.2f\n", time_frame);
	  }
      }

    } else {
      // It's time to sleep...
      current_module="sleep";

      time_frame-=GetRelativeTime(); // reset timer

      if(sh_audio && !d_audio->eof){
          int delay=audio_out->get_delay();
          if(verbose>1)printf("delay=%d\n",delay);
          time_frame=sh_video->timer;
          time_frame-=sh_audio->timer-(float)delay/(float)sh_audio->o_bps;
          // we are out of time... drop next frame!
	  if(time_frame<-2*frame_time){
	      drop_frame=frame_dropping; // tricky!
	      ++drop_frame_cnt;
	      if (verbose>0) printf("\nframe drop %d, %.2f\n", drop_frame, time_frame);
	  }
      } else {
          if( (time_frame<-3*frame_time || time_frame>3*frame_time) || benchmark)
	      time_frame=0;
	  
      }

//      if(verbose>1)printf("sleep: %5.3f  a:%6.3f  v:%6.3f  \n",time_frame,sh_audio->timer,sh_video->timer);
      
      while(time_frame>0.005){
          if(time_frame<=0.020)
             usec_sleep(10000); // sleeps 1 clock tick (10ms)!
          else
             usec_sleep(1000000*(time_frame-0.002));
          time_frame-=GetRelativeTime();
      }

        current_module="flip_page";
        if(blit_frame) video_out->flip_page();
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
      // PTS = (audio position)/(bytes per sec)
//      a_pts=(ds_tell(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
      a_pts=(ds_tell(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->wf->nAvgBytesPerSec;
      delay_corrected=1; // hack
      v_pts=d_video->pack_no*(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
      if(verbose)printf("%5.3f|",v_pts-d_video->pts);
    } else {
      if(!delay_corrected && d_audio->pts){
        float x=d_audio->pts-d_video->pts-(delay+audio_delay);
        float y=-(delay+audio_delay);
        printf("Initial PTS delay: %5.3f sec  (calculated: %5.3f)\n",x,y);
	initial_pts_delay+=x;
        audio_delay+=x;
        delay_corrected=1;
        if(verbose)
        printf("v: audio_delay=%5.3f  buffer_delay=%5.3f  a.pts=%5.3f  v.pts=%5.3f\n",
               audio_delay,delay,d_audio->pts,d_video->pts);
      }
      // PTS = (last timestamp) + (bytes after last timestamp)/(bytes per sec)
      a_pts=d_audio->pts;
      a_pts+=(ds_tell_pts(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
      v_pts=d_video->pts-frame_time;
    }

    if(verbose>1)printf("### A:%8.3f (%8.3f)  V:%8.3f  A-V:%7.4f  \n",a_pts,a_pts-audio_delay-delay,v_pts,(a_pts-delay-audio_delay)-v_pts);

    if(frame_corr_num==1){
      float x=frame_correction;
      if(delay_corrected){
//        printf("A:%6.1f  V:%6.1f  A-V:%7.3f",a_pts-audio_delay-delay,v_pts,x);
        printf("A:%6.1f (%6.1f)  V:%6.1f  A-V:%7.3f",a_pts,a_pts-audio_delay-delay,v_pts,x);
        x*=0.1f;
        if(x<-max_pts_correction) x=-max_pts_correction; else
        if(x> max_pts_correction) x= max_pts_correction;
        if(default_max_pts_correction>=0)
          max_pts_correction=default_max_pts_correction;
        else
          max_pts_correction=sh_video->frametime*0.10; // +-10% of time
        sh_audio->timer+=x; c_total+=x;
        printf(" ct:%7.3f  %3d  %2d%% %2d%% %4.1f%% %d\r",c_total,
        (int)sh_video->num_frames,
        (sh_video->timer>0.5)?(int)(100.0*video_time_usage/(double)sh_video->timer):0,
        (sh_video->timer>0.5)?(int)(100.0*vout_time_usage/(double)sh_video->timer):0,
        (sh_video->timer>0.5)?(100.0*audio_time_usage/(double)sh_video->timer):0
        ,drop_frame_cnt
        );
        fflush(stdout);
      }
      frame_corr_num=0; frame_correction=0;
    }
    
    if(frame_corr_num>=0) frame_correction+=(a_pts-delay-audio_delay)-v_pts;
    
  } else {
    // No audio:
    //if(d_video->pts)
    float v_pts=d_video->pts;
    if(frame_corr_num==5){
//      printf("A: ---   V:%6.1f   \r",v_pts);
      printf("V:%6.1f  %3d  %2d%%  %2d%%  %3.1f%% \r",v_pts,
        (int)sh_video->num_frames,
        (sh_video->timer>0.5)?(int)(100.0*video_time_usage/(double)sh_video->timer):0,
        (sh_video->timer>0.5)?(int)(100.0*vout_time_usage/(double)sh_video->timer):0,
        (sh_video->timer>0.5)?(100.0*audio_time_usage/(double)sh_video->timer):0
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
      if (audio_out && sh_audio)
         audio_out->pause();	// pause audio, keep data if possible
#ifdef HAVE_GUI
      if ( nogui )
        {
#endif
         while(
#ifdef HAVE_LIRC
             lirc_mp_getinput()<=0 &&
#endif
             (!f || getch2(20)<=0) && mplayer_get_key()<=0){
	     video_out->check_events();
             if(!f) usec_sleep(1000); // do not eat the CPU
         }
         osd_function=OSD_PLAY;
#ifdef HAVE_GUI
        } else while( osd_function != OSD_PLAY ) usec_sleep( 1000 );
#endif
      if (audio_out && sh_audio)
        audio_out->resume();	// resume audio
  }


    if(!force_redraw) break;
  } //  while(sh_video->timer<sh_audio->timer || force_redraw)


//================= Keyboard events, SEEKing ====================

{ int c;
  while(
#ifdef HAVE_LIRC
      (c=lirc_mp_getinput())>0 ||
#endif
      (f && (c=getch2(0))>0) || (c=mplayer_get_key())>0) switch(c){
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
    case 'z':
      sub_delay -= 0.1;
      break;
    case 'x':
      sub_delay += 0.1;
      break;
    case '*':
    case '/': {
        float mixer_l, mixer_r;
        mixer_getvolume( &mixer_l,&mixer_r );
        if(c=='*'){
            mixer_l++; if ( mixer_l > 100 ) mixer_l = 100;
            mixer_r++; if ( mixer_r > 100 ) mixer_r = 100;
        } else {
            mixer_l--; if ( mixer_l < 0 ) mixer_l = 0;
            mixer_r--; if ( mixer_r < 0 ) mixer_r = 0;
        }
        mixer_setvolume( mixer_l,mixer_r );

        if(osd_level){
          osd_visible=sh_video->fps; // 1 sec
          vo_osd_progbar_type=OSD_VOLUME;
          vo_osd_progbar_value=(mixer_l+mixer_r)*5/4;
          //printf("volume: %d\n",vo_osd_progbar_value);
        }
      }
      break; 
    case 'm':
      mixer_usemaster=!mixer_usemaster;
      break;
    case 'd':
      frame_dropping=(frame_dropping+1)%3;
      printf("== drop: %d ==  \n",frame_dropping);
      break;
  }
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
  
  if(rel_seek_secs)
  if(demux_seek(demuxer,rel_seek_secs,0)){
      // success:

      if(sh_audio){
        current_module="audio_reset";
        audio_out->reset(); // stop audio, throwing away buffered data
        current_module=NULL;
      }

        // Set OSD:
      if(osd_level){
        int len=((demuxer->movi_end-demuxer->movi_start)>>8);
        if(len>0){
          osd_visible=sh_video->fps; // 1 sec
          vo_osd_progbar_type=0;
          vo_osd_progbar_value=(demuxer->filepos-demuxer->movi_start)/len;
        }
      }
      
      c_total=0; // kell ez?
      max_pts_correction=0.1;
      frame_corr_num=0; // -5   FIXME!!!
      frame_correction=0;
      force_redraw=5;
      audio_time_usage=0; video_time_usage=0; vout_time_usage=0;
  
  }
  rel_seek_secs=0;
  
} // keyboard event handler

//================= Update OSD ====================
{ if(osd_level>=2){
      int pts=d_video->pts;
      if(pts==osd_last_pts-1) ++pts; else osd_last_pts=pts;
      vo_osd_text=osd_text_buffer;
      sprintf(vo_osd_text,"%c %02d:%02d:%02d",osd_function,pts/3600,(pts/60)%60,pts%60);
  } else {
      vo_osd_text=NULL;
  }
//  for(i=1;i<=11;i++) osd_text_buffer[10+i]=i;osd_text_buffer[10+i]=0;
//  vo_osd_text=osd_text_buffer;
  
  // find sub
  if(subtitles && d_video->pts>0){
      int pts=d_video->pts;
      if(sub_fps==0) sub_fps=sh_video->fps;
      current_module="find_sub";
      find_sub(subtitles,sub_uses_time?(100*(pts+sub_delay)):((pts+sub_delay)*sub_fps)); // FIXME! frame counter...
      current_module=NULL;
  }
  
  // DVD sub:
  { unsigned char* packet=NULL;
    int len=ds_get_packet_sub(d_dvdsub,&packet);
    if(len>=2){
      int len2;
      len2=(packet[0]<<8)+packet[1];
      if(verbose) printf("\rDVD sub: %d / %d  \n",len,len2);
      if(len==len2)
        spudec_decode(packet,len);
      else
        printf("fragmented dvd-subs not yet supported!!!\n");
    } else if(len>=0) {
      printf("invalud dvd sub\n");
    }
  }
  
}

} // while(!eof)

exit_player("End of file");
}
return 1;
}
