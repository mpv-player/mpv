
/// \file
/// \ingroup Properties Command2Property OSDMsgStack

#include <stdio.h>
#include <stdlib.h>
#include "config.h"

#ifdef WIN32
#define _UWIN 1  /*disable Non-underscored versions of non-ANSI functions as otherwise int eof would conflict with eof()*/
#include <windows.h>
#endif
#include <string.h>
#include <unistd.h>

// #include <sys/mman.h>
#include <sys/types.h>
#ifndef __MINGW32__
#include <sys/ioctl.h>
#include <sys/wait.h>
#else
#define	SIGHUP	1	/* hangup */
#define	SIGQUIT	3	/* quit */
#define	SIGKILL	9	/* kill (cannot be caught or ignored) */
#define	SIGBUS	10	/* bus error */
#define	SIGPIPE	13	/* broken pipe */
extern int mp_input_win32_slave_cmd_func(int fd,char* dest,int size);
#endif

#include <sys/time.h>
#include <sys/stat.h>

#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <limits.h>

#include <errno.h>

#include "version.h"

#include "mp_msg.h"

#define HELP_MP_DEFINE_STATIC
#include "help_mp.h"

#include "m_option.h"
#include "m_config.h"
#include "m_property.h"

#include "cfg-mplayer-def.h"

#include "subreader.h"

#include "libvo/video_out.h"

#include "libvo/font_load.h"
#include "libvo/sub.h"

#ifdef HAVE_X11
#include "libvo/x11_common.h"
#endif

#include "libao2/audio_out.h"

#include "codec-cfg.h"

#include "edl.h"

#include "spudec.h"
#include "vobsub.h"

#include "osdep/getch2.h"
#include "osdep/timer.h"

#include "cpudetect.h"

#ifdef HAVE_NEW_GUI
#include "Gui/interface.h"
#endif

#include "input/input.h"

int slave_mode=0;
int player_idle_mode=0;
int quiet=0;
int enable_mouse_movements=0;

#ifdef WIN32
char * proc_priority=NULL;
#endif

#define ROUND(x) ((int)((x)<0 ? (x)-0.5 : (x)+0.5))

#ifdef HAVE_RTC
#ifdef __linux__
#include <linux/rtc.h>
#else
#include <rtc.h>
#define RTC_IRQP_SET RTCIO_IRQP_SET
#define RTC_PIE_ON   RTCIO_PIE_ON
#endif /* __linux__ */
#endif /* HAVE_RTC */

#ifdef USE_TV
#include "stream/tv.h"
#endif
#ifdef USE_RADIO
#include "stream/stream_radio.h"
#endif

#ifdef HAS_DVBIN_SUPPORT
#include "stream/dvbin.h"
static int last_dvb_step = 1;
static int dvbin_reopen = 0;
#include "stream/cache2.h"
#endif

//**************************************************************************//
//             Playtree
//**************************************************************************//
#include "playtree.h"
#include "playtreeparser.h"

#ifdef HAVE_NEW_GUI
extern int import_playtree_playlist_into_gui(play_tree_t* my_playtree, m_config_t* config);
extern int import_initial_playtree_into_gui(play_tree_t* my_playtree, m_config_t* config, int enqueue);
#endif

play_tree_t* playtree;
play_tree_iter_t* playtree_iter = NULL;
static int play_tree_step = 1;

#define PT_NEXT_ENTRY 1
#define PT_PREV_ENTRY -1
#define PT_NEXT_SRC 2
#define PT_PREV_SRC -2
#define PT_UP_NEXT 3
#define PT_UP_PREV -3

//**************************************************************************//
//             Config
//**************************************************************************//
#include "parser-cfg.h"
#include "parser-mpcmd.h"

m_config_t* mconfig;

//**************************************************************************//
//             Config file
//**************************************************************************//

static int cfg_inc_verbose(m_option_t *conf){ ++verbose; return 0;}

static int cfg_include(m_option_t *conf, char *filename){
	return m_config_parse_config_file(mconfig, filename);
}

#include "get_path.c"

//**************************************************************************//
//             XScreensaver
//**************************************************************************//

#ifdef HAVE_X11
void xscreensaver_heartbeat(void);
#endif

//**************************************************************************//
//**************************************************************************//
//             Input media streaming & demultiplexer:
//**************************************************************************//

static int max_framesize=0;

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
//#include "parse_es.h"
#include "libmpdemux/matroska.h"

#ifdef USE_DVDREAD
#include "stream/stream_dvd.h"
#endif

#ifdef USE_DVDNAV
#include "stream/stream_dvdnav.h"
#endif

#include "libmpcodecs/dec_audio.h"
#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/mp_image.h"
#include "libmpcodecs/vf.h"
#include "libmpcodecs/vd.h"

//**************************************************************************//

static void reinit_audio_chain(void);

//**************************************************************************//
//**************************************************************************//

// Common FIFO functions, and keyboard/event FIFO code
#include "fifo.c"
int noconsolecontrols=0;
//**************************************************************************//

vo_functions_t *video_out=NULL;
ao_functions_t *audio_out=NULL;

int fixed_vo=0;
int eof=0;

// benchmark:
double video_time_usage=0;
double vout_time_usage=0;
static double audio_time_usage=0;
static int total_time_usage_start=0;
static int total_frame_cnt=0;
static int drop_frame_cnt=0; // total number of dropped frames
int benchmark=0;

// options:
       int auto_quality=0;
static int output_quality=0;

float playback_speed=1.0;

int use_gui=0;

#ifdef HAVE_NEW_GUI
int enqueue=0;
#endif

static int list_properties = 0;

#define MAX_OSD_LEVEL 3
#define MAX_TERM_OSD_LEVEL 1

int osd_level=1;
int osd_level_saved=-1;
// if nonzero, hide current OSD contents when GetTimerMS() reaches this
unsigned int osd_visible;
static int osd_function=OSD_PLAY;
static int osd_show_percentage = 0;
static int osd_duration = 1000;

static int term_osd = 1;
static char* term_osd_esc = "\x1b[A\r\x1b[K";
static char* playing_msg = NULL;
// seek:
static char *seek_to_sec=NULL;
static off_t seek_to_byte=0;
static off_t step_sec=0;
static int loop_times=-1;
static int loop_seek=0;

static m_time_size_t end_at = { .type = END_AT_NONE, .pos = 0 };

// A/V sync:
       int autosync=0; // 30 might be a good default value.

// may be changed by GUI:  (FIXME!)
float rel_seek_secs=0;
int abs_seek_pos=0;

// codecs:
char **audio_codec_list=NULL; // override audio codec
char **video_codec_list=NULL; // override video codec
char **audio_fm_list=NULL;    // override audio codec family 
char **video_fm_list=NULL;    // override video codec family 

// demuxer:
extern char *demuxer_name; // override demuxer
extern char *audio_demuxer_name; // override audio demuxer
extern char *sub_demuxer_name; // override sub demuxer

// streaming:
int audio_id=-1;
int video_id=-1;
int dvdsub_id=-2;
int vobsub_id=-1;
char* audio_lang=NULL;
char* dvdsub_lang=NULL;
static char* spudec_ifo=NULL;
char* filename=NULL; //"MI2-Trailer.avi";
int forced_subs_only=0;
int file_filter=1;

// cache2:
       int stream_cache_size=-1;
#ifdef USE_STREAM_CACHE
extern int cache_fill_status;

float stream_cache_min_percent=20.0;
float stream_cache_seek_min_percent=50.0;
#else
#define cache_fill_status 0
#endif

// dump:
static char *stream_dump_name="stream.dump";
       int stream_dump_type=0;

// A-V sync:
static float default_max_pts_correction=-1;//0.01f;
static float max_pts_correction=0;//default_max_pts_correction;
static float c_total=0;
       float audio_delay=0;
static int ignore_start=0;

static int softsleep=0;

       float force_fps=0;
static int force_srate=0;
static int audio_output_format=-1; // AF_FORMAT_UNKNOWN
       int frame_dropping=0; // option  0=no drop  1= drop vo  2= drop decode
static int play_n_frames=-1;
static int play_n_frames_mf=-1;

// screen info:
char** video_driver_list=NULL;
char** audio_driver_list=NULL;

extern char *vo_subdevice;
extern char *ao_subdevice;

// codec outfmt flags (defined in libmpcodecs/vd.c)
extern int vo_flags;

// sub:
char *font_name=NULL;
#ifdef HAVE_FONTCONFIG
extern int font_fontconfig;
#endif
float font_factor=0.75;
char **sub_name=NULL;
float sub_delay=0;
float sub_fps=0;
int   sub_auto = 1;
char *vobsub_name=NULL;
/*DSP!!char *dsp=NULL;*/
int   subcc_enabled=0;
int suboverlap_enabled = 1;

sub_data* set_of_subtitles[MAX_SUBTITLE_FILES];
int set_of_sub_size = 0;
int set_of_sub_pos = -1;
double sub_last_pts = -303;

int global_sub_size = 0; // this encompasses all subtitle sources
int global_sub_pos = -1; // this encompasses all subtitle sources
#define SUB_SOURCE_SUBS 0
#define SUB_SOURCE_VOBSUB 1
#define SUB_SOURCE_DEMUX 2
#define SUB_SOURCES 3
int global_sub_indices[SUB_SOURCES];

#ifdef USE_ASS
#include "libass/ass.h"
#include "libass/ass_mp.h"

// set_of_ass_tracks[i] contains subtitles from set_of_subtitles[i] parsed by libass
// or NULL if format unsupported
ass_track_t* set_of_ass_tracks[MAX_SUBTITLE_FILES];
ass_track_t* ass_track = 0; // current track to render
#endif

extern int mp_msg_levels[MSGT_MAX];
extern int mp_msg_level_all;

static stream_t* stream=NULL;
static demuxer_t *demuxer=NULL;
static sh_audio_t *sh_audio=NULL;
static sh_video_t *sh_video=NULL;
static demux_stream_t *d_audio=NULL;
static demux_stream_t *d_video=NULL;
static demux_stream_t *d_dvdsub=NULL;

char* current_module=NULL; // for debugging

extern int vo_gamma_gamma;
extern int vo_gamma_brightness;
extern int vo_gamma_contrast;
extern int vo_gamma_saturation;
extern int vo_gamma_hue;

// ---

#ifdef HAVE_MENU
#include "m_struct.h"
#include "libmenu/menu.h"
extern void vf_menu_pause_update(struct vf_instance_s* vf);
extern vf_info_t vf_info_menu;
static vf_info_t* libmenu_vfs[] = {
  &vf_info_menu,
  NULL
};
static vf_instance_t* vf_menu = NULL;
static int use_menu = 0;
static char* menu_cfg = NULL;
static char* menu_root = "main";
#endif


#ifdef HAVE_RTC
static int nortc = 1;
static char* rtc_device;
#endif

edl_record_ptr edl_records = NULL; ///< EDL entries memory area
edl_record_ptr next_edl_record = NULL; ///< only for traversing edl_records
short user_muted = 0; ///< Stores whether user wanted muted mode.
short edl_muted  = 0; ///< Stores whether EDL is currently in muted mode.
short edl_decision = 0; ///< 1 when an EDL operation has been made.
FILE* edl_fd = NULL; ///< fd to write to when in -edlout mode.
float begin_skip = MP_NOPTS_VALUE; ///< start time of the current skip while on edlout mode
int use_filedir_conf;

static unsigned int inited_flags=0;
#define INITED_VO 1
#define INITED_AO 2
#define INITED_GUI 4
#define INITED_GETCH2 8
#define INITED_SPUDEC 32
#define INITED_STREAM 64
#define INITED_INPUT    128
#define INITED_VOBSUB  256
#define INITED_DEMUXER 512
#define INITED_ACODEC  1024
#define INITED_VCODEC  2048
#define INITED_ALL 0xFFFF

#include "metadata.h"

#define mp_basename2(s) (strrchr(s,'/')==NULL?(char*)s:(strrchr(s,'/')+1))

static int is_valid_metadata_type (metadata_t type) {
  switch (type)
  {
  /* check for valid video stream */
  case META_VIDEO_CODEC:
  case META_VIDEO_BITRATE:
  case META_VIDEO_RESOLUTION:
  {
    if (!sh_video)
      return 0;
    break;
  }

  /* check for valid audio stream */
  case META_AUDIO_CODEC:
  case META_AUDIO_BITRATE:
  case META_AUDIO_SAMPLES:
  {
    if (!sh_audio)
      return 0;
    break;
  }

  /* check for valid demuxer */
  case META_INFO_TITLE:
  case META_INFO_ARTIST:
  case META_INFO_ALBUM:
  case META_INFO_YEAR:
  case META_INFO_COMMENT:
  case META_INFO_TRACK:
  case META_INFO_GENRE:
  {
    if (!demuxer)
      return 0;
    break;
  }

  default:
    break;
  }

  return 1;
}

static char *get_demuxer_info (char *tag) {
  char **info = demuxer->info;
  int n;

  if (!info || !tag)
    return NULL;

  for (n = 0; info[2*n] != NULL ; n++)
    if (!strcmp (info[2*n], tag))
      break;

  return info[2*n+1] ? strdup (info[2*n+1]) : NULL;
}

char *get_metadata (metadata_t type) {
  char *meta = NULL;

  if (!is_valid_metadata_type (type))
    return NULL;
  
  switch (type)
  {
  case META_NAME:
  {
    return strdup (mp_basename2 (filename));
  }
    
  case META_VIDEO_CODEC:
  {
    if (sh_video->format == 0x10000001)
      meta = strdup ("mpeg1");
    else if (sh_video->format == 0x10000002)
      meta = strdup ("mpeg2");
    else if (sh_video->format == 0x10000004)
      meta = strdup ("mpeg4");
    else if (sh_video->format == 0x10000005)
      meta = strdup ("h264");
    else if (sh_video->format >= 0x20202020)
    {
      meta = (char *) malloc (8);
      sprintf (meta, "%.4s", (char *) &sh_video->format);
    }
    else
    {
      meta = (char *) malloc (8);
      sprintf (meta, "0x%08X", sh_video->format);
    }
    return meta;
  }
  
  case META_VIDEO_BITRATE:
  {
    meta = (char *) malloc (16);
    sprintf (meta, "%d kbps", (int) (sh_video->i_bps * 8 / 1024));
    return meta;
  }
  
  case META_VIDEO_RESOLUTION:
  {
    meta = (char *) malloc (16);
    sprintf (meta, "%d x %d", sh_video->disp_w, sh_video->disp_h);
    return meta;
  }

  case META_AUDIO_CODEC:
  {
    if (sh_audio->codec && sh_audio->codec->name)
      meta = strdup (sh_audio->codec->name);
    return meta;
  }
  
  case META_AUDIO_BITRATE:
  {
    meta = (char *) malloc (16);
    sprintf (meta, "%d kbps", (int) (sh_audio->i_bps * 8/1000));
    return meta;
  }
  
  case META_AUDIO_SAMPLES:
  {
    meta = (char *) malloc (16);
    sprintf (meta, "%d Hz, %d ch.", sh_audio->samplerate, sh_audio->channels);
    return meta;
  }

  /* check for valid demuxer */
  case META_INFO_TITLE:
    return get_demuxer_info ("Title");
  
  case META_INFO_ARTIST:
    return get_demuxer_info ("Artist");

  case META_INFO_ALBUM:
    return get_demuxer_info ("Album");

  case META_INFO_YEAR:
    return get_demuxer_info ("Year");

  case META_INFO_COMMENT:
    return get_demuxer_info ("Comment");

  case META_INFO_TRACK:
    return get_demuxer_info ("Track");

  case META_INFO_GENRE:
    return get_demuxer_info ("Genre");

  default:
    break;
  }

  return meta;
}

static void uninit_player(unsigned int mask){
  mask=inited_flags&mask;

  mp_msg(MSGT_CPLAYER,MSGL_DBG2,"\n*** uninit(0x%X)\n",mask);

  if(mask&INITED_ACODEC){
    inited_flags&=~INITED_ACODEC;
    current_module="uninit_acodec";
    if(sh_audio) uninit_audio(sh_audio);
#ifdef HAVE_NEW_GUI
    guiGetEvent(guiSetAfilter, (char *)NULL);
#endif
    sh_audio=NULL;
  }

  if(mask&INITED_VCODEC){
    inited_flags&=~INITED_VCODEC;
    current_module="uninit_vcodec";
    if(sh_video) uninit_video(sh_video);
    sh_video=NULL;
#ifdef HAVE_MENU
    vf_menu=NULL;
#endif
  }
 
  if(mask&INITED_DEMUXER){
    inited_flags&=~INITED_DEMUXER;
    current_module="free_demuxer";
    if(demuxer){
	stream=demuxer->stream;
	free_demuxer(demuxer);
    }
    demuxer=NULL;
  }

  // kill the cache process:
  if(mask&INITED_STREAM){
    inited_flags&=~INITED_STREAM;
    current_module="uninit_stream";
    if(stream) free_stream(stream);
    stream=NULL;
  }

  if(mask&INITED_VO){
    inited_flags&=~INITED_VO;
    current_module="uninit_vo";
    video_out->uninit();
    video_out=NULL;
  }

  // Must be after libvo uninit, as few vo drivers (svgalib) have tty code.
  if(mask&INITED_GETCH2){
    inited_flags&=~INITED_GETCH2;
    current_module="uninit_getch2";
    mp_msg(MSGT_CPLAYER,MSGL_DBG2,"\n[[[uninit getch2]]]\n");
  // restore terminal:
    getch2_disable();
  }

  if(mask&INITED_VOBSUB){
    inited_flags&=~INITED_VOBSUB;
    current_module="uninit_vobsub";
    if(vo_vobsub) vobsub_close(vo_vobsub);
    vo_vobsub=NULL;
  }

  if (mask&INITED_SPUDEC){
    inited_flags&=~INITED_SPUDEC;
    current_module="uninit_spudec";
    spudec_free(vo_spudec);
    vo_spudec=NULL;
  }

  if(mask&INITED_AO){
    inited_flags&=~INITED_AO;
    current_module="uninit_ao";
    audio_out->uninit(eof?0:1); audio_out=NULL;
  }

#ifdef HAVE_NEW_GUI
  if(mask&INITED_GUI){
    inited_flags&=~INITED_GUI;
    current_module="uninit_gui";
    guiDone();
  }
#endif

  if(mask&INITED_INPUT){
    inited_flags&=~INITED_INPUT;
    current_module="uninit_input";
    mp_input_uninit();
  }

  current_module=NULL;
}

static void exit_player_with_rc(const char* how, int rc){

  uninit_player(INITED_ALL);
#ifdef HAVE_X11
#ifdef HAVE_NEW_GUI
  if ( !use_gui )
#endif
  vo_uninit();	// Close the X11 connection (if any is open).
#endif

#ifdef HAVE_FREETYPE
  current_module="uninit_font";
  if (vo_font) free_font_desc(vo_font);
  vo_font = NULL;
  done_freetype();
#endif
  free_osd_list();

#ifdef USE_ASS
  ass_library_done(ass_library);
#endif

  current_module="exit_player";

// free mplayer config
  if(mconfig)
    m_config_free(mconfig);
  
  if(playtree)
    play_tree_free(playtree, 1);


  if(edl_records != NULL) free(edl_records); // free mem allocated for EDL
  if(how) mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_ExitingHow,how);
  mp_msg(MSGT_CPLAYER,MSGL_DBG2,"max framesize was %d bytes\n",max_framesize);

  exit(rc);
}

void exit_player(const char* how){
  exit_player_with_rc(how, 1);
}

#ifndef __MINGW32__
static void child_sighandler(int x){  
  pid_t pid;
  while((pid=waitpid(-1,NULL,WNOHANG)) > 0);
}
#endif

#ifdef CRASH_DEBUG
static char *prog_path;
static int crash_debug = 0;
#endif

static void exit_sighandler(int x){
  static int sig_count=0;
#ifdef CRASH_DEBUG
  if (!crash_debug || x != SIGTRAP)
#endif
  ++sig_count;
  if(inited_flags==0 && sig_count>1) exit(1);
  if(sig_count==5)
    {
      /* We're crashing bad and can't uninit cleanly :( 
       * by popular request, we make one last (dirty) 
       * effort to restore the user's 
       * terminal. */
      getch2_disable();
      exit(1);
    }
  if(sig_count==6) exit(1);
  if(sig_count>6){
    // can't stop :(
#ifndef __MINGW32__
    kill(getpid(),SIGKILL);
#endif
  }
  mp_msg(MSGT_CPLAYER,MSGL_FATAL,"\n" MSGTR_IntBySignal,x,
      current_module?current_module:"unknown"
  );
  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SIGNAL=%d\n", x);
  if(sig_count<=1)
  switch(x){
  case SIGINT:
  case SIGQUIT:
  case SIGTERM:
  case SIGKILL:
      break;  // killed from keyboard (^C) or killed [-9]
  case SIGILL:
#ifdef RUNTIME_CPUDETECT
      mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_Exit_SIGILL_RTCpuSel);
#else
      mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_Exit_SIGILL);
#endif
  case SIGFPE:
  case SIGSEGV:
      mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_Exit_SIGSEGV_SIGFPE);
  default:
      mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_Exit_SIGCRASH);
#ifdef CRASH_DEBUG
      if (crash_debug) {
        int gdb_pid;
        char spid[20];
        snprintf(spid, 19, "%i", getpid());
        spid[19] = 0;
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "Forking...\n");
        gdb_pid = fork();
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "Forked...\n");
        if (gdb_pid == 0) { // We are the child
          if (execlp("gdb", "gdb", prog_path, spid, NULL) == -1)
            mp_msg(MSGT_CPLAYER, MSGL_ERR, "Couldn't start gdb\n");
        } else if (gdb_pid < 0) 
          mp_msg(MSGT_CPLAYER, MSGL_ERR, "Couldn't fork\n");
        else {
          waitpid(gdb_pid, NULL, 0);
        }
        if (x == SIGTRAP) return;
      }
#endif  
  }
  exit_player(NULL);
}

extern void mp_input_register_options(m_config_t* cfg);

#include "mixer.h"
mixer_t mixer;
/// step size of mixer changes
int volstep = 3;

#include "cfg-mplayer.h"

static void parse_cfgfiles( m_config_t* conf )
{
char *conffile;
int conffile_fd;
if (m_config_parse_config_file(conf, MPLAYER_CONFDIR "/mplayer.conf") < 0)
  exit_player(NULL);
if ((conffile = get_path("")) == NULL) {
  mp_msg(MSGT_CPLAYER,MSGL_WARN,MSGTR_NoHomeDir);
} else {
#ifdef __MINGW32__
  mkdir(conffile);
#else
  mkdir(conffile, 0777);
#endif
  free(conffile);
  if ((conffile = get_path("config")) == NULL) {
    mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_GetpathProblem);
  } else {
    if ((conffile_fd = open(conffile, O_CREAT | O_EXCL | O_WRONLY, 0666)) != -1) {
      mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_CreatingCfgFile, conffile);
      write(conffile_fd, default_config, strlen(default_config));
      close(conffile_fd);
    }
    if (m_config_parse_config_file(conf, conffile) < 0)
      exit_player(NULL);
    free(conffile);
  }
}
}

void load_per_file_config (m_config_t* conf, const char *const file)
{
    char *confpath;
    char cfg[strlen(file)+10];
    struct stat st;
    char *name;

    sprintf (cfg, "%s.conf", file);
    
    if (use_filedir_conf && !stat (cfg, &st))
    {
	mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_LoadingConfig, cfg);
	m_config_parse_config_file (conf, cfg);
	return;
    }

    if ((name = strrchr (cfg, '/')) == NULL)
	name = cfg;
    else
	name++;

    if ((confpath = get_path (name)) != NULL)
    {
	if (!stat (confpath, &st))
	{
	    mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_LoadingConfig, confpath);
	    m_config_parse_config_file (conf, confpath);
	}

	free (confpath);
    }
}

/* When libmpdemux performs a blocking operation (network connection or
 * cache filling) if the operation fails we use this function to check
 * if it was interrupted by the user.
 * The function returns a new value for eof. */
static int libmpdemux_was_interrupted(int eof) {
  mp_cmd_t* cmd;
  if((cmd = mp_input_get_cmd(0,0,0)) != NULL) {
       switch(cmd->id) {
       case MP_CMD_QUIT:
	 exit_player_with_rc(MSGTR_Exit_quit, (cmd->nargs > 0)? cmd->args[0].v.i : 0);
       case MP_CMD_PLAY_TREE_STEP: {
	 eof = (cmd->args[0].v.i > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
	 play_tree_step = (cmd->args[0].v.i == 0) ? 1 : cmd->args[0].v.i;
       } break;
       case MP_CMD_PLAY_TREE_UP_STEP: {
	 eof = (cmd->args[0].v.i > 0) ? PT_UP_NEXT : PT_UP_PREV;
       } break;	  
       case MP_CMD_PLAY_ALT_SRC_STEP: {
	 eof = (cmd->args[0].v.i > 0) ?  PT_NEXT_SRC : PT_PREV_SRC;
       } break;
       }
       mp_cmd_free(cmd);
  }
  return eof;
}

#define mp_basename(s) (strrchr(s,'\\')==NULL?(mp_basename2(s)):(strrchr(s,'\\')+1))

int playtree_add_playlist(play_tree_t* entry)
{
  play_tree_add_bpf(entry,filename);

#ifdef HAVE_NEW_GUI
  if (use_gui) {
    if (entry) {
      import_playtree_playlist_into_gui(entry, mconfig);
      play_tree_free_list(entry,1);
    }
  } else
#endif
  {
  if(!entry) {      
    entry = playtree_iter->tree;
    if(play_tree_iter_step(playtree_iter,1,0) != PLAY_TREE_ITER_ENTRY) {
        return PT_NEXT_ENTRY;
    }
    if(playtree_iter->tree == entry ) { // Loop with a single file
      if(play_tree_iter_up_step(playtree_iter,1,0) != PLAY_TREE_ITER_ENTRY) {
	return PT_NEXT_ENTRY;
      }
    }
    play_tree_remove(entry,1,1);
    return PT_NEXT_SRC;
  }
  play_tree_insert_entry(playtree_iter->tree,entry);
  play_tree_set_params_from(entry,playtree_iter->tree);
  entry = playtree_iter->tree;
  if(play_tree_iter_step(playtree_iter,1,0) != PLAY_TREE_ITER_ENTRY) {
    return PT_NEXT_ENTRY;
  }      
  play_tree_remove(entry,1,1);
  }
  return PT_NEXT_SRC;
}

int sub_source(void)
{
    int source = -1;
    int top = -1;
    int i;
    for (i = 0; i < SUB_SOURCES; i++) {
        int j = global_sub_indices[i];
        if ((j >= 0) && (j > top) && (global_sub_pos >= j)) {
            source = i;
            top = j;
        }
    }
    return source;
}

sub_data* subdata = NULL;
static subtitle* vo_sub_last = NULL;

void add_subtitles(char *filename, float fps, int silent)
{
    sub_data *subd;
#ifdef USE_ASS
    ass_track_t *asst = 0;
#endif

    if (filename == NULL || set_of_sub_size >= MAX_SUBTITLE_FILES) {
	return;
    }

    subd = sub_read_file(filename, fps);
#ifdef USE_ASS
    if (ass_enabled)
#ifdef USE_ICONV
        asst = ass_read_file(ass_library, filename, sub_cp);
#else
        asst = ass_read_file(ass_library, filename, 0);
#endif
    if (ass_enabled && subd && !asst)
        asst = ass_read_subdata(ass_library, subd, fps);

    if (!asst && !subd && !silent)
#else
    if(!subd && !silent) 
#endif
        mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_CantLoadSub, filename);
    
#ifdef USE_ASS
    if (!asst && !subd) return;
    set_of_ass_tracks[set_of_sub_size] = asst;
#else
    if (!subd) return;
#endif
    set_of_subtitles[set_of_sub_size] = subd;
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_FILE_SUB_ID=%d\n", set_of_sub_size);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_FILE_SUB_FILENAME=%s\n", filename);
    ++set_of_sub_size;
    mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_AddedSubtitleFile, set_of_sub_size, filename);
}

// FIXME: if/when the GUI calls this, global sub numbering gets (potentially) broken.
void update_set_of_subtitles(void)
    // subdata was changed, set_of_sub... have to be updated.
{
    int i;
    if (set_of_sub_size > 0 && subdata == NULL) { // *subdata was deleted
        for (i = set_of_sub_pos + 1; i < set_of_sub_size; ++i)
            set_of_subtitles[i-1] = set_of_subtitles[i];
        set_of_subtitles[set_of_sub_size-1] = NULL;
        --set_of_sub_size;
        if (set_of_sub_size > 0) subdata = set_of_subtitles[set_of_sub_pos=0];
    }
    else if (set_of_sub_size > 0 && subdata != NULL) { // *subdata was changed
        set_of_subtitles[set_of_sub_pos] = subdata;
    }
    else if (set_of_sub_size <= 0 && subdata != NULL) { // *subdata was added
        set_of_subtitles[set_of_sub_pos=set_of_sub_size] = subdata;
        ++set_of_sub_size;
    }
}

void init_vo_spudec(void) {
  if (vo_spudec)
    spudec_free(vo_spudec);
  inited_flags &= ~INITED_SPUDEC;
  vo_spudec = NULL;
  if (spudec_ifo) {
    unsigned int palette[16], width, height;
    current_module="spudec_init_vobsub";
    if (vobsub_parse_ifo(NULL,spudec_ifo, palette, &width, &height, 1, -1, NULL) >= 0)
      vo_spudec=spudec_new_scaled(palette, width, height);
  }

#ifdef USE_DVDREAD
  if (vo_spudec==NULL && stream->type==STREAMTYPE_DVD) {
    current_module="spudec_init_dvdread";
    vo_spudec=spudec_new_scaled(((dvd_priv_t *)(stream->priv))->cur_pgc->palette,
                                sh_video->disp_w, sh_video->disp_h);
  }
#endif

#ifdef USE_DVDNAV
  if (vo_spudec==NULL && stream->type==STREAMTYPE_DVDNAV) {
    unsigned int *palette = mp_dvdnav_get_spu_clut(stream);
    current_module="spudec_init_dvdnav";
    vo_spudec=spudec_new_scaled(palette, sh_video->disp_w, sh_video->disp_h);
  }
#endif

  if ((vo_spudec == NULL) && (demuxer->type == DEMUXER_TYPE_MATROSKA) &&
      (d_dvdsub->sh != NULL) && (((sh_sub_t *)d_dvdsub->sh)->type == 'v')) {
    sh_sub_t *mkv_sh_sub = (sh_sub_t *)d_dvdsub->sh;
    current_module = "spudec_init_matroska";
    vo_spudec =
      spudec_new_scaled_vobsub(mkv_sh_sub->palette, mkv_sh_sub->colors,
                             mkv_sh_sub->custom_colors, mkv_sh_sub->width,
                               mkv_sh_sub->height);
    forced_subs_only = mkv_sh_sub->forced_subs_only;
  }

  if (vo_spudec==NULL) {
    sh_sub_t *sh = (sh_sub_t *)d_dvdsub->sh;
    unsigned int *palette = NULL;
    if (sh && sh->has_palette)
      palette = sh->palette;
    current_module="spudec_init_normal";
    vo_spudec=spudec_new_scaled(palette, sh_video->disp_w, sh_video->disp_h);
    spudec_set_font_factor(vo_spudec,font_factor);
  }

  if (vo_spudec!=NULL)
    inited_flags|=INITED_SPUDEC;
}

/*
 * In Mac OS X the SDL-lib is built upon Cocoa. The easiest way to
 * make it all work is to use the builtin SDL-bootstrap code, which 
 * will be done automatically by replacing our main() if we include SDL.h.
 */
#if defined(SYS_DARWIN) && defined(HAVE_SDL)
#include <SDL.h>
#endif

/**
 * \brief append a formatted string
 * \param buf buffer to print into
 * \param pos position of terminating 0 in buf
 * \param len maximum number of characters in buf, not including terminating 0
 * \param format printf format string
 */
static void saddf(char *buf, unsigned *pos, int len, const char *format, ...)
{
  va_list va;
  va_start(va, format);
  *pos += vsnprintf(&buf[*pos], len - *pos, format, va);
  va_end(va);
  if (*pos >= len ) {
    buf[len] = 0;
    *pos = len;
  }
}

/**
 * \brief append time in the hh:mm:ss.f format
 * \param buf buffer to print into
 * \param pos position of terminating 0 in buf
 * \param len maximum number of characters in buf, not including terminating 0
 * \param time time value to convert/append
 */
static void sadd_hhmmssf(char *buf, unsigned *pos, int len, float time) {
  long tenths = 10 * time;
  int f1 = tenths % 10;
  int ss = (tenths /  10) % 60;
  int mm = (tenths / 600) % 60;
  int hh = tenths / 36000;
  if (time <= 0) {
    saddf(buf, pos, len, "unknown");
    return;
  }
  if (hh > 0)
    saddf(buf, pos, len, "%2d:", hh);
  if (hh > 0 || mm > 0)
    saddf(buf, pos, len, "%02d:", mm);
  saddf(buf, pos, len, "%02d.%1d", ss, f1);
}

/**
 * \brief print the status line
 * \param a_pos audio position
 * \param a_v A-V desynchronization
 * \param corr amount out A-V synchronization
 */
static void print_status(float a_pos, float a_v, float corr)
{
  int width;
  char *line;
  unsigned pos = 0;
  get_screen_size();
  if (screen_width > 0)
    width = screen_width;
  else
  width = 80;
#ifdef WIN32
  /* Windows command line is broken (MinGW's rxvt works, but we
   * should not depend on that). */
  width--;
#endif
  line = malloc(width + 1); // one additional char for the terminating null
  
  // Audio time
  if (sh_audio) {
    saddf(line, &pos, width, "A:%6.1f ", a_pos);
    if (!sh_video) {
      float len = demuxer_get_time_length(demuxer);
      saddf(line, &pos, width, "(");
      sadd_hhmmssf(line, &pos, width, a_pos);
      saddf(line, &pos, width, ") of %.1f (", len);
      sadd_hhmmssf(line, &pos, width, len);
      saddf(line, &pos, width, ") ");
    }
  }

  // Video time
  if (sh_video)
    saddf(line, &pos, width, "V:%6.1f ", sh_video->pts);

  // A-V sync
  if (sh_audio && sh_video)
    saddf(line, &pos, width, "A-V:%7.3f ct:%7.3f ", a_v, corr);

  // Video stats
  if (sh_video)
    saddf(line, &pos, width, "%3d/%3d ",
      (int)sh_video->num_frames,
      (int)sh_video->num_frames_decoded);

  // CPU usage
  if (sh_video) {
    if (sh_video->timer > 0.5)
      saddf(line, &pos, width, "%2d%% %2d%% %4.1f%% ",
        (int)(100.0*video_time_usage*playback_speed/(double)sh_video->timer),
        (int)(100.0*vout_time_usage*playback_speed/(double)sh_video->timer),
        (100.0*audio_time_usage*playback_speed/(double)sh_video->timer));
    else
      saddf(line, &pos, width, "??%% ??%% ??,?%% ");
  } else if (sh_audio) {
    if (sh_audio->delay > 0.5)
      saddf(line, &pos, width, "%4.1f%% ",
        100.0*audio_time_usage/(double)sh_audio->delay);
    else
      saddf(line, &pos, width, "??,?%% ");
  }

  // VO stats
  if (sh_video) 
    saddf(line, &pos, width, "%d %d ", drop_frame_cnt, output_quality);

#ifdef USE_STREAM_CACHE
  // cache stats
  if (stream_cache_size > 0)
    saddf(line, &pos, width, "%d%% ", cache_fill_status);
#endif

  // other
  if (playback_speed != 1)
    saddf(line, &pos, width, "%4.2fx ", playback_speed);

  // end
  if (erase_to_end_of_line) {
    line[pos] = 0;
    mp_msg(MSGT_AVSYNC, MSGL_STATUS, "%s%s\r", line, erase_to_end_of_line);
  } else {
    memset(&line[pos], ' ', width - pos);
    line[width] = 0;
    mp_msg(MSGT_AVSYNC, MSGL_STATUS, "%s\r", line);
  }
  free(line);
}

/**
 * \brief build a chain of audio filters that converts the input format
 * to the ao's format, taking into account the current playback_speed.
 * \param sh_audio describes the requested input format of the chain.
 * \param ao_data describes the requested output format of the chain.
 */
static int build_afilter_chain(sh_audio_t *sh_audio, ao_data_t *ao_data)
{
  int new_srate;
  int result;
  if (!sh_audio)
  {
#ifdef HAVE_NEW_GUI
    guiGetEvent(guiSetAfilter, (char *)NULL);
#endif
    mixer.afilter = NULL;
    return 0;
  }
  new_srate = sh_audio->samplerate * playback_speed;
  if (new_srate != ao_data->samplerate) {
    // limits are taken from libaf/af_resample.c
    if (new_srate < 8000)
      new_srate = 8000;
    if (new_srate > 192000)
      new_srate = 192000;
    playback_speed = (float)new_srate / (float)sh_audio->samplerate;
  }
  result =  init_audio_filters(sh_audio, new_srate,
           sh_audio->channels, sh_audio->sample_format,
           &ao_data->samplerate, &ao_data->channels, &ao_data->format,
           ao_data->outburst * 4, ao_data->buffersize);
  mixer.afilter = sh_audio->afilter;
#ifdef HAVE_NEW_GUI
  guiGetEvent(guiSetAfilter, (char *)sh_audio->afilter);
#endif
  return result;
}

/**
 * \brief Log the currently displayed subtitle to a file
 * 
 * Logs the current or last displayed subtitle together with filename
 * and time information to ~/.mplayer/subtitle_log
 *
 * Intended purpose is to allow convenient marking of bogus subtitles
 * which need to be fixed while watching the movie.
 */

static void log_sub(void){
    char *fname;
    FILE *f;
    int i;

    if (subdata == NULL || vo_sub_last == NULL) return;
    fname = get_path("subtitle_log");
    f = fopen(fname, "a");
    if (!f) return;
    fprintf(f, "----------------------------------------------------------\n");
    if (subdata->sub_uses_time) {
	fprintf(f, "N: %s S: %02ld:%02ld:%02ld.%02ld E: %02ld:%02ld:%02ld.%02ld\n", filename, 
		vo_sub_last->start/360000, (vo_sub_last->start/6000)%60,
		(vo_sub_last->start/100)%60, vo_sub_last->start%100,
		vo_sub_last->end/360000, (vo_sub_last->end/6000)%60,
		(vo_sub_last->end/100)%60, vo_sub_last->end%100);
    } else {
	fprintf(f, "N: %s S: %ld E: %ld\n", filename, vo_sub_last->start, vo_sub_last->end);
    }
    for (i = 0; i < vo_sub_last->lines; i++) {
	fprintf(f, "%s\n", vo_sub_last->text[i]);
    }
    fclose(f);
}

/// \defgroup OSDMsgStack OSD message stack
///
///@{

#define OSD_MSG_TV_CHANNEL              0
#define OSD_MSG_TEXT                    1
#define OSD_MSG_SUB_DELAY               2
#define OSD_MSG_SPEED                   3
#define OSD_MSG_OSD_STATUS              4
#define OSD_MSG_BAR                     5
#define OSD_MSG_PAUSE                   6
#define OSD_MSG_RADIO_CHANNEL           7
/// Base id for messages generated from the commmand to property bridge.
#define OSD_MSG_PROPERTY                0x100


typedef struct mp_osd_msg mp_osd_msg_t;
struct mp_osd_msg {
    /// Previous message on the stack.
    mp_osd_msg_t* prev;
    /// Message text.
    char msg[64];
    int  id,level,started;
    /// Display duration in ms.
    unsigned  time;
};

/// OSD message stack.
static mp_osd_msg_t* osd_msg_stack = NULL;

/**
 *  \brief Add a message on the OSD message stack
 * 
 *  If a message with the same id is already present in the stack
 *  it is pulled on top of the stack, otherwise a new message is created.
 *  
 */

static void set_osd_msg(int id, int level, int time, const char* fmt, ...) {
    mp_osd_msg_t *msg,*last=NULL;
    va_list va;
    int r;
   
    // look if the id is already in the stack
    for(msg = osd_msg_stack ; msg && msg->id != id ;
	last = msg, msg = msg->prev);
    // not found: alloc it
    if(!msg) {
        msg = calloc(1,sizeof(mp_osd_msg_t));
        msg->prev = osd_msg_stack;
        osd_msg_stack = msg;
    } else if(last) { // found, but it's not on top of the stack
        last->prev = msg->prev;
        msg->prev = osd_msg_stack;
        osd_msg_stack = msg;
    }
    // write the msg
    va_start(va,fmt);
    r = vsnprintf(msg->msg, 64, fmt, va);
    va_end(va);
    if(r >= 64) msg->msg[63] = 0;
    // set id and time
    msg->id = id;
    msg->level = level;
    msg->time = time;
    
}

/**
 *  \brief Remove a message from the OSD stack
 * 
 *  This function can be used to get rid of a message right away.
 * 
 */

static void rm_osd_msg(int id) {
    mp_osd_msg_t *msg,*last=NULL;
    
    // Search for the msg
    for(msg = osd_msg_stack ; msg && msg->id != id ;
	last = msg, msg = msg->prev);
    if(!msg) return;

    // Detach it from the stack and free it
    if(last)
        last->prev = msg->prev;
    else
        osd_msg_stack = msg->prev;
    free(msg);
}

/**
 *  \brief Remove all messages from the OSD stack
 * 
 */

static void clear_osd_msgs(void) {
    mp_osd_msg_t* msg = osd_msg_stack, *prev = NULL;
    while(msg) {
        prev = msg->prev;
        free(msg);
        msg = prev;
    }
    osd_msg_stack = NULL;
}

/**
 *  \brief Get the current message from the OSD stack.
 * 
 *  This function decrements the message timer and destroys the old ones.
 *  The message that should be displayed is returned (if any).
 *  
 */

static mp_osd_msg_t* get_osd_msg(void) {
    mp_osd_msg_t *msg,*prev,*last = NULL;
    static unsigned last_update = 0;
    unsigned now = GetTimerMS();
    unsigned diff;
    char hidden_dec_done = 0;
    
    if(!last_update) last_update = now;
    diff = now >= last_update ? now - last_update : 0;
    
    last_update = now;
    
    // Look for the first message in the stack with high enough level.
    for(msg = osd_msg_stack ; msg ; last = msg, msg = prev) {
        prev = msg->prev;
        if(msg->level > osd_level && hidden_dec_done) continue;
        // The message has a high enough level or it is the first hidden one
        // in both cases we decrement the timer or kill it.
        if(!msg->started || msg->time > diff) {
            if(msg->started) msg->time -= diff;
            else msg->started = 1;
            // display it
            if(msg->level <= osd_level) return msg;
            hidden_dec_done = 1;
            continue;
        }
        // kill the message
        free(msg);
        if(last) {
            last->prev = prev;
            msg = last;
        } else {
            osd_msg_stack = prev;
            msg = NULL;
        }
    }
    // Nothing found
    return NULL;
}

/**
 * \brief Display the OSD bar.
 *
 * Display the OSD bar or fall back on a simple message.
 *
 */

static void set_osd_bar(int type,const char* name,double min,double max,double val) {
    
    if(osd_level < 1) return;
    
    if(sh_video) {
        osd_visible = (GetTimerMS() + 1000) | 1;
        vo_osd_progbar_type = type;
        vo_osd_progbar_value = 256*(val-min)/(max-min);
        vo_osd_changed(OSDTYPE_PROGBAR);
        return;
    }
    
    set_osd_msg(OSD_MSG_BAR,1,osd_duration,"%s: %d %%",
                name,ROUND(100*(val-min)/(max-min)));
}


/**
 * \brief Update the OSD message line.
 *
 * This function displays the current message on the vo OSD or on the term.
 * If the stack is empty and the OSD level is high enough the timer
 * is displayed (only on the vo OSD).
 * 
 */

static void update_osd_msg(void) {
    mp_osd_msg_t *msg;
    static char osd_text[64] = "";
    static char osd_text_timer[64];
    
    // we need some mem for vo_osd_text
    vo_osd_text = (unsigned char*)osd_text;
    
    // Look if we have a msg
    if((msg = get_osd_msg())) {
        if(strcmp(osd_text,msg->msg)) {
            strncpy((char*)osd_text, msg->msg, 63);
            if(sh_video) vo_osd_changed(OSDTYPE_OSD); else 
            if(term_osd) mp_msg(MSGT_CPLAYER,MSGL_STATUS,"%s%s\n",term_osd_esc,msg->msg);
        }
        return;
    }
        
    if(sh_video) {
        // fallback on the timer
        if(osd_level>=2) {
            int len = demuxer_get_time_length(demuxer);
            int percentage = -1;
            char percentage_text[10];
            int pts = demuxer_get_current_time(demuxer);
            
            if (osd_show_percentage)
                percentage = demuxer_get_percent_pos(demuxer);
            
            if (percentage >= 0)
                snprintf(percentage_text, 9, " (%d%%)", percentage);
            else
                percentage_text[0] = 0;
            
            if (osd_level == 3) 
                snprintf(osd_text_timer, 63,
                         "%c %02d:%02d:%02d / %02d:%02d:%02d%s",
                         osd_function,pts/3600,(pts/60)%60,pts%60,
                         len/3600,(len/60)%60,len%60,percentage_text);
            else
                snprintf(osd_text_timer, 63, "%c %02d:%02d:%02d%s",
                         osd_function,pts/3600,(pts/60)%60,
                         pts%60,percentage_text);
        } else
            osd_text_timer[0]=0;
        
        // always decrement the percentage timer
        if(osd_show_percentage)
            osd_show_percentage--;
        
        if(strcmp(osd_text,osd_text_timer)) {
            strncpy(osd_text, osd_text_timer, 63);
            vo_osd_changed(OSDTYPE_OSD);
        }
        return;
    }
        
    // Clear the term osd line
    if(term_osd && osd_text[0]) {
        osd_text[0] = 0;
        printf("%s\n",term_osd_esc);
    }
}

///@}
// OSDMsgStack

/// \defgroup Properties
///@{

/// \defgroup GeneralProperties General properties
/// \ingroup Properties
///@{

/// OSD level (RW)
static int mp_property_osdlevel(m_option_t* prop,int action,void* arg) {
    return m_property_choice(prop,action,arg,&osd_level);
}

/// Playback speed (RW)
static int mp_property_playback_speed(m_option_t* prop,int action,void* arg) {
    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop,*(float*)arg);
        playback_speed = *(float*)arg;
        build_afilter_chain(sh_audio, &ao_data);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        playback_speed += (arg ? *(float*)arg : 0.1) *
            (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        M_PROPERTY_CLAMP(prop,playback_speed);
        build_afilter_chain(sh_audio, &ao_data);
        return M_PROPERTY_OK;
    }
    return m_property_float_range(prop,action,arg,&playback_speed);
}

/// filename with path (RO)
static int mp_property_path(m_option_t* prop,int action,void* arg) {
    return m_property_string_ro(prop,action,arg,filename);
}

/// filename without path (RO)
static int mp_property_filename(m_option_t* prop,int action,void* arg) {
    char* f;
    if(!filename) return M_PROPERTY_UNAVAILABLE;
    if(((f = strrchr(filename,'/')) || (f = strrchr(filename,'\\'))) && f[1])
        f++;
    else
        f = filename;
    return m_property_string_ro(prop,action,arg,f);
}

/// Demuxer name (RO)
static int mp_property_demuxer(m_option_t* prop,int action,void* arg) {
    if(!demuxer) return M_PROPERTY_UNAVAILABLE;
    return m_property_string_ro(prop,action,arg,(char*)demuxer->desc->name);
}

/// Position in the stream (RW)
static int mp_property_stream_pos(m_option_t* prop,int action,void* arg) {
    if (!demuxer || !demuxer->stream) return M_PROPERTY_UNAVAILABLE;
    if (!arg) return M_PROPERTY_ERROR;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t*)arg = stream_tell(demuxer->stream);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        M_PROPERTY_CLAMP(prop,*(off_t*)arg);
        stream_seek(demuxer->stream, *(off_t*)arg);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Stream start offset (RO)
static int mp_property_stream_start(m_option_t* prop,int action,void* arg) {
    if (!demuxer || !demuxer->stream) return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t*)arg = demuxer->stream->start_pos;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Stream end offset (RO)
static int mp_property_stream_end(m_option_t* prop,int action,void* arg) {
    if (!demuxer || !demuxer->stream) return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t*)arg = demuxer->stream->end_pos;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Stream length (RO)
static int mp_property_stream_length(m_option_t* prop,int action,void* arg) {
    if (!demuxer || !demuxer->stream) return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t*)arg = demuxer->stream->end_pos - demuxer->stream->start_pos;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Media length in seconds (RO)
static int mp_property_length(m_option_t* prop,int action,void* arg) {
    double len;
    
    if(!demuxer ||
       !(int)(len = demuxer_get_time_length(demuxer)))
        return M_PROPERTY_UNAVAILABLE;
    
    switch(action) {
    case M_PROPERTY_PRINT:
        if(!arg) return M_PROPERTY_ERROR;
        else {
            int h, m, s = len;
            h = s/3600;
            s -= h*3600;
            m = s/60;
            s -= m*60;
            *(char**)arg = malloc(20);
            if(h > 0) sprintf(*(char**)arg,"%d:%02d:%02d",h,m,s);
            else if(m > 0) sprintf(*(char**)arg,"%d:%02d",m,s);
            else sprintf(*(char**)arg,"%d",s);
            return M_PROPERTY_OK;
        }
        break;
    }
    return m_property_double_ro(prop,action,arg,len);
}

///@}

/// \defgroup AudioProperties Audio properties
/// \ingroup Properties
///@{

/// Volume (RW)
static int mp_property_volume(m_option_t* prop,int action,void* arg) {

    if(!sh_audio) return M_PROPERTY_UNAVAILABLE;
    
    switch(action) {
    case M_PROPERTY_GET:
        if(!arg) return M_PROPERTY_ERROR;
        mixer_getbothvolume(&mixer,arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:{
        float vol;
        if(!arg) return M_PROPERTY_ERROR;
        mixer_getbothvolume(&mixer,&vol);
        return m_property_float_range(prop,action,arg,&vol);
    }        
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
    case M_PROPERTY_SET:
        break;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

    if (edl_muted) return M_PROPERTY_DISABLED;
    user_muted = 0;

    switch(action) {
   case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop,*(float*)arg);
        mixer_setvolume(&mixer,*(float*)arg,*(float*)arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
        if(arg && *(float*)arg <= 0)
            mixer_decvolume(&mixer);
        else
            mixer_incvolume(&mixer);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_DOWN:
        if(arg && *(float*)arg <= 0)
            mixer_incvolume(&mixer);
        else
            mixer_decvolume(&mixer);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Mute (RW)
static int mp_property_mute(m_option_t* prop,int action,void* arg) {
    
    if(!sh_audio) return M_PROPERTY_UNAVAILABLE;
    
    switch(action) {
    case M_PROPERTY_SET:
        if(edl_muted) return M_PROPERTY_DISABLED;
        if(!arg) return M_PROPERTY_ERROR;
        if((!!*(int*)arg) != mixer.muted)
            mixer_mute(&mixer);
        user_muted = mixer.muted;
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if(edl_muted) return M_PROPERTY_DISABLED;
        mixer_mute(&mixer);
        user_muted = mixer.muted;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if(!arg) return M_PROPERTY_ERROR;
        if(edl_muted) {
            *(char**)arg = strdup(MSGTR_EnabledEdl);
            return M_PROPERTY_OK;
        }
    default:
        return m_property_flag(prop,action,arg,&mixer.muted);

    }
}

/// Audio delay (RW)
static int mp_property_audio_delay(m_option_t* prop,int action,void* arg) {
    if(!(sh_audio && sh_video)) return M_PROPERTY_UNAVAILABLE;
    switch(action) {
    case M_PROPERTY_SET:
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if(!arg) return M_PROPERTY_ERROR;
        else {
            float delay = audio_delay;
            m_property_delay(prop,action,arg,&audio_delay);
            if(sh_audio) sh_audio->delay -= audio_delay-delay;
        }
        return M_PROPERTY_OK;
    default:
        return m_property_delay(prop,action,arg,&audio_delay);
    }
}

/// Audio codec tag (RO)
static int mp_property_audio_format(m_option_t* prop,int action,void* arg) {
    if(!sh_audio) return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop,action,arg,sh_audio->format);
}

/// Audio bitrate (RO)
static int mp_property_audio_bitrate(m_option_t* prop,int action,void* arg) {
    if(!sh_audio) return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop,action,arg,sh_audio->i_bps);
}

/// Samplerate (RO)
static int mp_property_samplerate(m_option_t* prop,int action,void* arg) {
    if(!sh_audio) return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop,action,arg,sh_audio->samplerate);
}

/// Number of channels (RO)
static int mp_property_channels(m_option_t* prop,int action,void* arg) {
    if(!sh_audio) return M_PROPERTY_UNAVAILABLE;
    switch(action) {
    case M_PROPERTY_PRINT:
        if(!arg) return M_PROPERTY_ERROR;
        switch(sh_audio->channels) {
        case 1: *(char**)arg = strdup("mono"); break;
        case 2: *(char**)arg = strdup("stereo"); break;
        default:
            *(char**)arg = malloc(32);
            sprintf(*(char**)arg,"%d channels",sh_audio->channels);
        }
        return M_PROPERTY_OK;
    }
    return m_property_int_ro(prop,action,arg,sh_audio->channels);
}

/// Selected audio id (RW)
static int mp_property_audio(m_option_t* prop,int action,void* arg) {
    int current_id = -1, tmp;

    switch(action) {
    case M_PROPERTY_GET:
        if(!sh_audio) return M_PROPERTY_UNAVAILABLE;
        if(!arg) return M_PROPERTY_ERROR;
        *(int*)arg = audio_id;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if(!sh_audio) return M_PROPERTY_UNAVAILABLE;
        if(!arg) return M_PROPERTY_ERROR;

        if (audio_id < 0)
            *(char**)arg = strdup(MSGTR_Disabled);
        else {
            char lang[40] = MSGTR_Unknown;
            if (demuxer->type == DEMUXER_TYPE_MATROSKA)
                demux_mkv_get_audio_lang(demuxer, audio_id, lang, 9);
#ifdef USE_DVDREAD
            else {
                int code = dvd_lang_from_aid(stream, audio_id);
                if (code) {
                    lang[0] = code >> 8;
                    lang[1] = code;
                    lang[2] = 0;
                }
            }
#endif
            *(char**)arg = malloc(64);
            snprintf(*(char**)arg, 64, "(%d) %s", audio_id, lang);
        }
        return M_PROPERTY_OK;

    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_SET:
        if(action==M_PROPERTY_SET && arg)
            tmp = *((int*)arg);
        else
            tmp = -1;
        current_id = demuxer->audio->id;
        audio_id = demuxer_switch_audio(demuxer, tmp);
        if(audio_id == -2 || (audio_id > -1 && demuxer->audio->id != current_id && current_id != -2))
          uninit_player(INITED_AO | INITED_ACODEC);
        if(audio_id > -1 && demuxer->audio->id != current_id) {
          sh_audio_t *sh2;
          sh2 = demuxer->a_streams[demuxer->audio->id];
          if(sh2) {
            sh2->ds = demuxer->audio;
            sh_audio = sh2;
            reinit_audio_chain();
          }
        }
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_TRACK=%d\n", audio_id);
        return M_PROPERTY_OK;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

}

static int reinit_video_chain(void);
/// Selected video id (RW)
static int mp_property_video(m_option_t* prop,int action,void* arg) {
    int current_id = -1, tmp;

    switch(action) {
    case M_PROPERTY_GET:
        if(!sh_video) return M_PROPERTY_UNAVAILABLE;
        if(!arg) return M_PROPERTY_ERROR;
        *(int*)arg = video_id;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if(!sh_video) return M_PROPERTY_UNAVAILABLE;
        if(!arg) return M_PROPERTY_ERROR;

        if (video_id < 0)
            *(char**)arg = strdup(MSGTR_Disabled);
        else {
            char lang[40] = MSGTR_Unknown;
            *(char**)arg = malloc(64);
            snprintf(*(char**)arg, 64, "(%d) %s", video_id, lang);
        }
        return M_PROPERTY_OK;

    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_SET:
        current_id = demuxer->video->id;
        if(action==M_PROPERTY_SET && arg)
            tmp = *((int*)arg);
        else
            tmp = -1;
        video_id = demuxer_switch_video(demuxer, tmp);
        if(video_id == -2 || (video_id > -1 && demuxer->video->id != current_id && current_id != -2))
          uninit_player(INITED_VCODEC | (fixed_vo && video_id != -2 ? 0 : INITED_VO));
        if(video_id > -1 && demuxer->video->id != current_id) {
          sh_video_t *sh2;
          sh2 = demuxer->v_streams[demuxer->video->id];
          if(sh2) {
            sh2->ds = demuxer->video;
            sh_video = sh2;
            reinit_video_chain();
          }
        }
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_TRACK=%d\n", video_id);
        return M_PROPERTY_OK;

    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
}

static int mp_property_program(m_option_t* prop,int action,void* arg) {
    demux_program_t prog;

    switch(action) {
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_SET:
        if(action==M_PROPERTY_SET && arg)
            prog.progid = *((int*)arg);
        else
            prog.progid = -1;
        if(demux_control(demuxer, DEMUXER_CTRL_IDENTIFY_PROGRAM, &prog) == DEMUXER_CTRL_NOTIMPL)
            return M_PROPERTY_ERROR;

        mp_property_do("switch_audio", M_PROPERTY_SET, &prog.aid);
        mp_property_do("switch_video", M_PROPERTY_SET, &prog.vid);
        return M_PROPERTY_OK;

    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
}

///@}

/// \defgroup VideoProperties Video properties
/// \ingroup Properties
///@{

/// Fullscreen state (RW)
static int mp_property_fullscreen(m_option_t* prop,int action,void* arg) {

    if(!video_out) return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop,*(int*)arg);
        if(vo_fs == !!*(int*)arg) return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
#ifdef HAVE_NEW_GUI
        if(use_gui) guiGetEvent(guiIEvent,(char*)MP_CMD_GUI_FULLSCREEN);
        else
#endif
        if(vo_config_count) video_out->control(VOCTRL_FULLSCREEN, 0);
        return M_PROPERTY_OK;
    default:
        return m_property_flag(prop,action,arg,&vo_fs);
    }
}

static int mp_property_deinterlace(m_option_t* prop,int action,void* arg) {
    int deinterlace;
    vf_instance_t *vf;
    if (!sh_video || !sh_video->vfilter) return M_PROPERTY_UNAVAILABLE;
    vf = sh_video->vfilter;
    switch(action) {
    case M_PROPERTY_GET:
        if(!arg) return M_PROPERTY_ERROR;
        vf->control(vf, VFCTRL_GET_DEINTERLACE, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop,*(int*)arg);
        vf->control(vf, VFCTRL_SET_DEINTERLACE, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        vf->control(vf, VFCTRL_GET_DEINTERLACE, &deinterlace);
        deinterlace = !deinterlace;
        vf->control(vf, VFCTRL_SET_DEINTERLACE, &deinterlace);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Panscan (RW)
static int mp_property_panscan(m_option_t* prop,int action,void* arg) {

    if(!video_out || video_out->control(VOCTRL_GET_PANSCAN,NULL ) != VO_TRUE)
        return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop,*(float*)arg);
        vo_panscan = *(float*)arg;
        video_out->control(VOCTRL_SET_PANSCAN,NULL);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        vo_panscan += (arg ? *(float*)arg : 0.1) *
            (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        if(vo_panscan > 1) vo_panscan = 1;
        else if(vo_panscan < 0) vo_panscan = 0;
        video_out->control(VOCTRL_SET_PANSCAN,NULL);
        return M_PROPERTY_OK;
    default:
        return m_property_float_range(prop,action,arg,&vo_panscan);
    }
}

/// Helper to set vo flags.
/** \ingroup PropertyImplHelper
 */
static int mp_property_vo_flag(m_option_t* prop,int action,void* arg,
                               int vo_ctrl,int* vo_var) {

    if(!video_out) return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop,*(int*)arg);
        if(*vo_var == !!*(int*)arg) return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if(vo_config_count) video_out->control(vo_ctrl, 0);
        return M_PROPERTY_OK;
    default:
        return m_property_flag(prop,action,arg,vo_var);
    }
}

/// Window always on top (RW)
static int mp_property_ontop(m_option_t* prop,int action,void* arg) {
    return mp_property_vo_flag(prop,action,arg,VOCTRL_ONTOP,&vo_ontop);
}

/// Display in the root window (RW)
static int mp_property_rootwin(m_option_t* prop,int action,void* arg) {
    return mp_property_vo_flag(prop,action,arg,VOCTRL_ROOTWIN,&vo_rootwin);
}

/// Show window borders (RW)
static int mp_property_border(m_option_t* prop,int action,void* arg) {
    return mp_property_vo_flag(prop,action,arg,VOCTRL_BORDER,&vo_border);
}

/// Framedropping state (RW)
static int mp_property_framedropping(m_option_t* prop,int action,void* arg) {

    if(!sh_video) return M_PROPERTY_UNAVAILABLE;
    
    switch(action) {
    case M_PROPERTY_PRINT:
        if(!arg) return M_PROPERTY_ERROR;
        *(char**)arg = strdup(frame_dropping == 1 ? MSGTR_Enabled :
                              (frame_dropping == 2 ? MSGTR_HardFrameDrop  : MSGTR_Disabled));
        return M_PROPERTY_OK;
    default:
        return m_property_choice(prop,action,arg,&frame_dropping);
    }
}

/// Color settings, try to use vf/vo then fall back on TV. (RW)
static int mp_property_gamma(m_option_t* prop,int action,void* arg) {
    int* gamma = prop->priv, r;

    if(!sh_video) return M_PROPERTY_UNAVAILABLE;

    if(gamma[0] == 1000) {
        gamma[0] = 0;
        get_video_colors (sh_video, prop->name, gamma);
    }

    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop,*(int*)arg);
        *gamma = *(int*)arg;
        r = set_video_colors(sh_video, prop->name, *gamma);
        if(r <= 0) break;
        return r;
    case M_PROPERTY_GET:
        if(!arg) return M_PROPERTY_ERROR;
        r = get_video_colors (sh_video, prop->name, arg);
        if(r <= 0) break;
        return r;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        *gamma += (arg ? *(int*)arg : 1) *
            (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        M_PROPERTY_CLAMP(prop,*gamma);
        r = set_video_colors(sh_video, prop->name, *gamma);
        if(r <= 0) break;
        return r;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
    
#ifdef USE_TV
    if(demuxer->type == DEMUXER_TYPE_TV) {
        int l = strlen(prop->name);
        char tv_prop[3+l+1];
        sprintf(tv_prop,"tv_%s",prop->name);
        return mp_property_do(tv_prop,action,arg);
    }
#endif
    
    return M_PROPERTY_UNAVAILABLE;
}

/// VSync (RW)
static int mp_property_vsync(m_option_t* prop,int action,void* arg) {
    return m_property_flag(prop,action,arg,&vo_vsync);
}

/// Video codec tag (RO)
static int mp_property_video_format(m_option_t* prop,int action,void* arg) {
    if(!sh_video) return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop,action,arg,sh_video->format);
}

/// Video bitrate (RO)
static int mp_property_video_bitrate(m_option_t* prop,int action,void* arg) {
    if(!sh_video) return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop,action,arg,sh_video->i_bps);
}

/// Video display width (RO)
static int mp_property_width(m_option_t* prop,int action,void* arg) {
    if(!sh_video) return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop,action,arg,sh_video->disp_w);
}

/// Video display height (RO)
static int mp_property_height(m_option_t* prop,int action,void* arg) {
    if(!sh_video) return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop,action,arg,sh_video->disp_h);
}

/// Video fps (RO)
static int mp_property_fps(m_option_t* prop,int action,void* arg) {
    if(!sh_video) return M_PROPERTY_UNAVAILABLE;
    return m_property_float_ro(prop,action,arg,sh_video->fps);
}

/// Video aspect (RO)
static int mp_property_aspect(m_option_t* prop,int action,void* arg) {
    if(!sh_video) return M_PROPERTY_UNAVAILABLE;
    return m_property_float_ro(prop,action,arg,sh_video->aspect);
}

///@}

/// \defgroup SubProprties Subtitles properties
/// \ingroup Properties
///@{

/// Text subtitle position (RW)
static int mp_property_sub_pos(m_option_t* prop,int action,void* arg) {
    if(!sh_video) return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        vo_osd_changed(OSDTYPE_SUBTITLE);
    default:
        return m_property_int_range(prop,action,arg,&sub_pos);
    }
}

/// Selected subtitles (RW)
static int mp_property_sub(m_option_t* prop,int action,void* arg) {
    int source = -1, reset_spu = 0;
    char* sub_name;

    if(global_sub_size <= 0) return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_GET:
        if(!arg) return M_PROPERTY_ERROR;
        *(int*)arg = global_sub_pos;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if(!arg) return M_PROPERTY_ERROR;
        *(char**)arg = malloc(64);
        (*(char**)arg)[63] = 0;
        sub_name = 0;
        if(subdata)
            sub_name = subdata->filename;
#ifdef USE_ASS
        if (ass_track && ass_track->name)
            sub_name = ass_track->name;
#endif
        if(sub_name) {
            char *tmp,*tmp2;
            tmp = sub_name;
            if ((tmp2 = strrchr(tmp, '/')))
                tmp = tmp2+1;

            snprintf(*(char**)arg, 63, "(%d) %s%s",
                     set_of_sub_pos + 1,
                     strlen(tmp) < 20 ? "" : "...",
                     strlen(tmp) < 20 ? tmp : tmp+strlen(tmp)-19);
            return M_PROPERTY_OK;
        }

#ifdef USE_DVDNAV
        if(stream->type==STREAMTYPE_DVDNAV) {
            if(vo_spudec && dvdsub_id >= 0) {
                unsigned char lang[3];
                if(dvdnav_lang_from_sid(stream, dvdsub_id, lang)) {
                    snprintf(*(char**)arg, 63, "(%d) %s", dvdsub_id, lang);
                    return M_PROPERTY_OK;
                }
            }
        }
#endif

        if (demuxer->type == DEMUXER_TYPE_MATROSKA && dvdsub_id >= 0) {
            char lang[40] = MSGTR_Unknown;
            demux_mkv_get_sub_lang(demuxer, dvdsub_id, lang, 9);
            snprintf(*(char**)arg, 63, "(%d) %s", dvdsub_id, lang);
            return M_PROPERTY_OK;
        }
#ifdef HAVE_OGGVORBIS
        if (demuxer->type == DEMUXER_TYPE_OGG && d_dvdsub && dvdsub_id >= 0) {
            char *lang = demux_ogg_sub_lang(demuxer, dvdsub_id);
            if (!lang) lang = MSGTR_Unknown;
            snprintf(*(char**)arg, 63, "(%d) %s",
                     dvdsub_id, lang);
            return M_PROPERTY_OK;
        }
#endif
        if (vo_vobsub && vobsub_id >= 0) {
            const char *language = MSGTR_Unknown;
            language = vobsub_get_id(vo_vobsub, (unsigned int) vobsub_id);
            snprintf(*(char**)arg, 63, "(%d) %s",
                     vobsub_id, language ? language : MSGTR_Unknown);
            return M_PROPERTY_OK;
        }
#ifdef USE_DVDREAD
        if (vo_spudec && stream->type == STREAMTYPE_DVD && dvdsub_id >= 0) {
            char lang[3];
            int code = dvd_lang_from_sid(stream, dvdsub_id);
                lang[0] = code >> 8;
                lang[1] = code;
                lang[2] = 0;
            snprintf(*(char**)arg, 63, "(%d) %s",
                     dvdsub_id, lang);
            return M_PROPERTY_OK;
        }
#endif
        if (dvdsub_id >= 0) {
            snprintf(*(char**)arg, 63, "(%d) %s", dvdsub_id, MSGTR_Unknown);
            return M_PROPERTY_OK;
        }
        snprintf(*(char**)arg, 63, MSGTR_Disabled);
        return M_PROPERTY_OK;

    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        if(*(int*)arg < -1) *(int*)arg = -1;
        else if(*(int*)arg >= global_sub_size) *(int*)arg = global_sub_size-1;
        global_sub_pos = *(int*)arg;
        break;
    case M_PROPERTY_STEP_UP:
        global_sub_pos += 2;
        global_sub_pos = (global_sub_pos % (global_sub_size+1)) - 1;
        break;
    case M_PROPERTY_STEP_DOWN:
        global_sub_pos += global_sub_size+1;
        global_sub_pos = (global_sub_pos % (global_sub_size+1)) - 1;
        break;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

    if (global_sub_pos >= 0)
        source = sub_source();

    mp_msg(MSGT_CPLAYER, MSGL_DBG3,
           "subtitles: %d subs, (v@%d s@%d d@%d), @%d, source @%d\n",
           global_sub_size, global_sub_indices[SUB_SOURCE_VOBSUB],
           global_sub_indices[SUB_SOURCE_SUBS],
           global_sub_indices[SUB_SOURCE_DEMUX],
           global_sub_pos, source);

    set_of_sub_pos = -1;
    subdata = NULL;
    vo_sub_last = vo_sub = NULL;

    vobsub_id = -1;
    dvdsub_id = -1;
    if (d_dvdsub) {
        if(d_dvdsub->id > -2) reset_spu = 1;
        d_dvdsub->id = -2;
    }
#ifdef USE_ASS
    ass_track = 0;
#endif

    if (source == SUB_SOURCE_VOBSUB) {
        vobsub_id = global_sub_pos - global_sub_indices[SUB_SOURCE_VOBSUB];
    } else if (source == SUB_SOURCE_SUBS) {
        set_of_sub_pos = global_sub_pos - global_sub_indices[SUB_SOURCE_SUBS];
#ifdef USE_ASS
        if (ass_enabled && set_of_ass_tracks[set_of_sub_pos])
            ass_track = set_of_ass_tracks[set_of_sub_pos];
        else 
#endif
        {
            subdata = set_of_subtitles[set_of_sub_pos];
            vo_osd_changed(OSDTYPE_SUBTITLE);
        }
    } else if (source == SUB_SOURCE_DEMUX) {
        dvdsub_id = global_sub_pos - global_sub_indices[SUB_SOURCE_DEMUX];
        if (d_dvdsub) {
#ifdef USE_DVDREAD
            if (vo_spudec && stream->type == STREAMTYPE_DVD) {
                d_dvdsub->id = dvdsub_id;
                spudec_reset(vo_spudec);
            }
#endif

#ifdef USE_DVDNAV
            if (vo_spudec && stream->type == STREAMTYPE_DVDNAV) {
                d_dvdsub->id = dvdsub_id;
                spudec_reset(vo_spudec);
            }
#endif
            if (stream->type != STREAMTYPE_DVD && stream->type != STREAMTYPE_DVDNAV) {
              int i = 0;
              for (d_dvdsub->id = 0; d_dvdsub->id < MAX_S_STREAMS; d_dvdsub->id++) {
                if (demuxer->s_streams[d_dvdsub->id]) {
                  if (i == dvdsub_id) break;
                  i++;
                }
              }
              d_dvdsub->sh = demuxer->s_streams[d_dvdsub->id];
            }
            if (demuxer->type == DEMUXER_TYPE_MATROSKA) {
                d_dvdsub->id = demux_mkv_change_subs(demuxer, dvdsub_id);
            }
            if (d_dvdsub->sh && d_dvdsub->id >= 0) {
                sh_sub_t *sh = d_dvdsub->sh;
                if (sh->type == 'v')
                    init_vo_spudec();
#ifdef USE_ASS
                else if (ass_enabled && sh->type == 'a')
                    ass_track = sh->ass_track;
#endif
            }
        }
    } else { // off
        vo_osd_changed(OSDTYPE_SUBTITLE);
        if(vo_spudec) vo_osd_changed(OSDTYPE_SPU);
    }
#ifdef USE_DVDREAD
    if (vo_spudec && (stream->type == STREAMTYPE_DVD || stream->type == STREAMTYPE_DVDNAV) && dvdsub_id < 0 && reset_spu) {
        dvdsub_id = -2;
        d_dvdsub->id = dvdsub_id;
        spudec_reset(vo_spudec);
    }
#endif

    return M_PROPERTY_OK;
}

/// Subtitle delay (RW)
static int mp_property_sub_delay(m_option_t* prop,int action,void* arg) {
    if(!sh_video) return M_PROPERTY_UNAVAILABLE;
    return m_property_delay(prop,action,arg,&sub_delay);
}

/// Alignment of text subtitles (RW) 
static int mp_property_sub_alignment(m_option_t* prop,int action,void* arg) {
    char* name[] = { MSGTR_Top, MSGTR_Center, MSGTR_Bottom };

    if(!sh_video || global_sub_pos < 0 || sub_source() != SUB_SOURCE_SUBS)
        return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_PRINT:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop,sub_alignment);
        *(char**)arg = strdup(name[sub_alignment]);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        vo_osd_changed(OSDTYPE_SUBTITLE);
    default:
        return m_property_choice(prop,action,arg,&sub_alignment);
    }
}

/// Subtitle visibility (RW)
static int mp_property_sub_visibility(m_option_t* prop,int action,void* arg) {
    if(!sh_video) return M_PROPERTY_UNAVAILABLE;
    
    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        vo_osd_changed(OSDTYPE_SUBTITLE);
        if(vo_spudec) vo_osd_changed(OSDTYPE_SPU);
    default:
        return m_property_flag(prop,action,arg,&sub_visibility);
    }
}

/// Show only forced subtitles (RW)
static int mp_property_sub_forced_only(m_option_t* prop,int action,void* arg) {
    if(!vo_spudec) return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        m_property_flag(prop,action,arg,&forced_subs_only);
        spudec_set_forced_subs_only(vo_spudec,forced_subs_only);
        return M_PROPERTY_OK;
    default:
        return m_property_flag(prop,action,arg,&forced_subs_only);
    }

}

///@}

/// \defgroup TVProperties TV properties
/// \ingroup Properties
///@{

#ifdef USE_TV

/// TV color settings (RW)
static int mp_property_tv_color(m_option_t* prop,int action,void* arg) {
    int r,val;
    tvi_handle_t* tvh = demuxer->priv;
    if(demuxer->type != DEMUXER_TYPE_TV || !tvh) return M_PROPERTY_UNAVAILABLE;
    
    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop,*(int*)arg);
        return tv_set_color_options(tvh,(int)prop->priv,*(int*)arg);
    case M_PROPERTY_GET:
        return tv_get_color_options(tvh,(int)prop->priv,arg);
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if((r = tv_get_color_options(tvh,(int)prop->priv,&val)) >= 0) {
            if(!r) return M_PROPERTY_ERROR;
            val += (arg ? *(int*)arg : 1) *
                (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
            M_PROPERTY_CLAMP(prop,val);
            return tv_set_color_options(tvh,(int)prop->priv,val);
        }
        return M_PROPERTY_ERROR;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

#endif

///@}

/// All properties available in MPlayer.
/** \ingroup Properties
 */
static m_option_t mp_properties[] = {
    // General
    { "osdlevel", mp_property_osdlevel, CONF_TYPE_INT,
      M_OPT_RANGE, 0, 3, NULL },
    { "speed", mp_property_playback_speed, CONF_TYPE_FLOAT,
      M_OPT_RANGE, 0.01, 100.0, NULL },
    { "filename", mp_property_filename, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "path", mp_property_path, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "demuxer", mp_property_demuxer, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "stream_pos", mp_property_stream_pos, CONF_TYPE_POSITION,
      M_OPT_MIN, 0, 0, NULL },
    { "stream_start", mp_property_stream_start, CONF_TYPE_POSITION,
      M_OPT_MIN, 0, 0, NULL },
    { "stream_end", mp_property_stream_end, CONF_TYPE_POSITION,
      M_OPT_MIN, 0, 0, NULL },
    { "stream_length", mp_property_stream_length, CONF_TYPE_POSITION,
      M_OPT_MIN, 0, 0, NULL },
    { "length", mp_property_length, CONF_TYPE_DOUBLE,
      0, 0, 0, NULL },

    // Audio
    { "volume", mp_property_volume, CONF_TYPE_FLOAT,
      M_OPT_RANGE, 0, 100, NULL },
    { "mute", mp_property_mute, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "audio_delay", mp_property_audio_delay, CONF_TYPE_FLOAT,
      M_OPT_RANGE, -100, 100, NULL },
    { "audio_format", mp_property_audio_format, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "audio_bitrate", mp_property_audio_bitrate, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "samplerate", mp_property_samplerate, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "channels", mp_property_channels, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "switch_audio", mp_property_audio, CONF_TYPE_INT,
      CONF_RANGE, -2, MAX_A_STREAMS-1, NULL },

    // Video
    { "fullscreen", mp_property_fullscreen, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "deinterlace", mp_property_deinterlace, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "ontop", mp_property_ontop, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "rootwin", mp_property_rootwin, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "border", mp_property_border, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "framedropping", mp_property_framedropping, CONF_TYPE_INT,
      M_OPT_RANGE, 0, 2, NULL },
    { "gamma", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, &vo_gamma_gamma },
    { "brightness", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, &vo_gamma_brightness },
    { "contrast", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, &vo_gamma_contrast },
    { "saturation", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, &vo_gamma_saturation },
    { "hue", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, &vo_gamma_hue },
    { "panscan", mp_property_panscan, CONF_TYPE_FLOAT,
      M_OPT_RANGE, 0, 1, NULL },
    { "vsync", mp_property_vsync, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "video_format", mp_property_video_format, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "video_bitrate", mp_property_video_bitrate, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "width", mp_property_width, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "height", mp_property_height, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "fps", mp_property_fps, CONF_TYPE_FLOAT,
      0, 0, 0, NULL },
    { "aspect", mp_property_aspect, CONF_TYPE_FLOAT,
      0, 0, 0, NULL },
    { "switch_video", mp_property_video, CONF_TYPE_INT,
      CONF_RANGE, -2, MAX_V_STREAMS-1, NULL },
    { "switch_program", mp_property_program, CONF_TYPE_INT,
      CONF_RANGE, -1, 65535, NULL },

    // Subs
    { "sub", mp_property_sub, CONF_TYPE_INT,
      M_OPT_MIN, -1, 0, NULL },
    { "sub_delay", mp_property_sub_delay, CONF_TYPE_FLOAT,
      0, 0, 0, NULL },
    { "sub_pos", mp_property_sub_pos, CONF_TYPE_INT,
      M_OPT_RANGE, 0, 100, NULL },
    { "sub_alignment", mp_property_sub_alignment, CONF_TYPE_INT,
      M_OPT_RANGE, 0, 2, NULL },
    { "sub_visibility", mp_property_sub_visibility, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "sub_forced_only", mp_property_sub_forced_only, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    
#ifdef USE_TV
    { "tv_brightness", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, (void*)TV_COLOR_BRIGHTNESS },
    { "tv_contrast", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, (void*)TV_COLOR_CONTRAST },
    { "tv_saturation", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, (void*)TV_COLOR_SATURATION },
    { "tv_hue", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, (void*)TV_COLOR_HUE },
#endif

    { NULL, NULL, NULL, 0, 0, 0, NULL }
};

m_option_t*  mp_property_find(const char* name) {
    return m_option_list_find(mp_properties,name);
}

int mp_property_do(const char* name,int action, void* val) {
    m_option_t* p = mp_property_find(name);
    if(!p) return M_PROPERTY_UNAVAILABLE;
    return m_property_do(p,action,val);
}

///@}
// Properties group


/**
 * \defgroup Command2Property Command to property bridge
 * 
 * It is used to handle most commands that just set a property
 * and optionally display something on the OSD.
 * Two kinds of commands are handled: adjust or toggle.
 *
 * Adjust commands take 1 or 2 parameters: <value> <abs>
 * If <abs> is non-zero the property is set to the given value
 * otherwise it is adjusted.
 *
 * Toggle commands take 0 or 1 parameters. With no parameter
 * or a value less than the property minimum it just steps the
 * property to its next value. Otherwise it sets it to the given
 * value.
 *
 *@{
 */

/// List of the commands that can be handled by setting a property.
static struct  {
    /// property name
    const char* name;
    /// cmd id
    int cmd;
    /// set/adjust or toggle command
    int toggle;
    /// progressbar type
    int osd_progbar;
    /// osd msg id if it must be shared
    int osd_id;
    /// osd msg template
    const char* osd_msg;
} set_prop_cmd[] = {
    // audio
    { "volume", MP_CMD_VOLUME, 0, OSD_VOLUME, -1, MSGTR_Volume },
    { "mute", MP_CMD_MUTE, 1, 0, -1, MSGTR_MuteStatus },
    { "audio_delay", MP_CMD_AUDIO_DELAY, 0, 0, -1, MSGTR_AVDelayStatus },
    { "switch_audio", MP_CMD_SWITCH_AUDIO, 1, 0, -1, MSGTR_OSDAudio },
    // video
    { "fullscreen", MP_CMD_VO_FULLSCREEN, 1, 0, -1, NULL },
    { "panscan", MP_CMD_PANSCAN, 0, OSD_PANSCAN, -1, MSGTR_Panscan },
    { "ontop", MP_CMD_VO_ONTOP, 1, 0, -1, MSGTR_OnTopStatus },
    { "rootwin", MP_CMD_VO_ROOTWIN, 1, 0, -1, MSGTR_RootwinStatus },
    { "border", MP_CMD_VO_BORDER, 1, 0, -1, MSGTR_BorderStatus },
    { "framedropping", MP_CMD_FRAMEDROPPING, 1, 0, -1, MSGTR_FramedroppingStatus },
    { "gamma", MP_CMD_GAMMA, 0, OSD_BRIGHTNESS, -1, MSGTR_Gamma },
    { "brightness", MP_CMD_BRIGHTNESS, 0, OSD_BRIGHTNESS, -1, MSGTR_Brightness },
    { "contrast", MP_CMD_CONTRAST, 0, OSD_CONTRAST, -1, MSGTR_Contrast },
    { "saturation", MP_CMD_SATURATION, 0, OSD_SATURATION, -1, MSGTR_Saturation },
    { "hue", MP_CMD_HUE, 0, OSD_HUE, -1, MSGTR_Hue },
    { "vsync", MP_CMD_SWITCH_VSYNC, 1, 0, -1, MSGTR_VSyncStatus },
    // subs
    { "sub", MP_CMD_SUB_SELECT, 1, 0, -1, MSGTR_SubSelectStatus },
    { "sub_pos", MP_CMD_SUB_POS, 0, 0, -1, MSGTR_SubPosStatus },
    { "sub_alignment", MP_CMD_SUB_ALIGNMENT, 1, 0, -1, MSGTR_SubAlignStatus },
    { "sub_delay", MP_CMD_SUB_DELAY, 0, 0, OSD_MSG_SUB_DELAY, MSGTR_SubDelayStatus },
    { "sub_visibility", MP_CMD_SUB_VISIBILITY, 1, 0, -1, MSGTR_SubVisibleStatus },
    { "sub_forced_only", MP_CMD_SUB_FORCED_ONLY, 1, 0, -1, MSGTR_SubForcedOnlyStatus },
#ifdef USE_TV
    { "tv_brightness", MP_CMD_TV_SET_BRIGHTNESS, 0, OSD_BRIGHTNESS, -1, MSGTR_Brightness },
    { "tv_hue", MP_CMD_TV_SET_HUE, 0, OSD_HUE, -1, MSGTR_Hue },
    { "tv_saturation", MP_CMD_TV_SET_SATURATION, 0, OSD_SATURATION, -1, MSGTR_Saturation },
    { "tv_contrast", MP_CMD_TV_SET_CONTRAST, 0, OSD_CONTRAST, -1, MSGTR_Contrast },
#endif
    { NULL, 0, 0, 0, -1, NULL }
};

/// Handle commands that set a property.
static int set_property_command(mp_cmd_t* cmd) {
    int i,r;
    m_option_t* prop;
    
    // look for the command
    for(i = 0 ; set_prop_cmd[i].name ; i++)
        if(set_prop_cmd[i].cmd == cmd->id) break;
    if(!set_prop_cmd[i].name) return 0;
     
    // get the property
    prop = mp_property_find(set_prop_cmd[i].name);
    if(!prop) return 0;
    
    // toggle command
    if(set_prop_cmd[i].toggle) {
        // set to value
        if(cmd->nargs > 0 && cmd->args[0].v.i >= prop->min)
            r = m_property_do(prop,M_PROPERTY_SET,&cmd->args[0].v.i);
        else
            r = m_property_do(prop,M_PROPERTY_STEP_UP,NULL);
    } else if(cmd->args[1].v.i) //set
            r = m_property_do(prop,M_PROPERTY_SET,&cmd->args[0].v);
    else // adjust
        r = m_property_do(prop,M_PROPERTY_STEP_UP,&cmd->args[0].v);
 
    if(r <= 0) return 1;
    
    if(set_prop_cmd[i].osd_progbar) {
        if(prop->type == CONF_TYPE_INT) {
            if(m_property_do(prop,M_PROPERTY_GET,&r) > 0)
                set_osd_bar(set_prop_cmd[i].osd_progbar,
                            set_prop_cmd[i].osd_msg,
                            prop->min,prop->max,r);
        } else if(prop->type == CONF_TYPE_FLOAT) {
            float f;
            if(m_property_do(prop,M_PROPERTY_GET,&f) > 0)
                set_osd_bar(set_prop_cmd[i].osd_progbar,set_prop_cmd[i].osd_msg,
                            prop->min,prop->max,f);
        } else
            mp_msg(MSGT_CPLAYER,MSGL_ERR, "Property use an unsupported type.\n");
        return 1;
    }
    
    if(set_prop_cmd[i].osd_msg) {
        char* val = m_property_print(prop);
        if(val) {
            set_osd_msg(set_prop_cmd[i].osd_id >= 0 ? set_prop_cmd[i].osd_id :
                        OSD_MSG_PROPERTY+i,1,osd_duration,
                        set_prop_cmd[i].osd_msg,val);
            free(val);
        }
    }
    return 1;
}

static void reinit_audio_chain(void) {
if(sh_audio){
  current_module="init_audio_codec";
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"==========================================================================\n");
  if(!init_best_audio_codec(sh_audio,audio_codec_list,audio_fm_list)){
    sh_audio=d_audio->sh=NULL; // failed to init :(
    d_audio->id = -2;
    return;
  } else
    inited_flags|=INITED_ACODEC;
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"==========================================================================\n");


  //const ao_info_t *info=audio_out->info;
  current_module="af_preinit";
  ao_data.samplerate=force_srate;
  ao_data.channels=0;
  ao_data.format=audio_output_format;
#if 1
  // first init to detect best values
  if(!preinit_audio_filters(sh_audio,
        // input:
        (int)(sh_audio->samplerate*playback_speed),
	sh_audio->channels, sh_audio->sample_format,
	// output:
	&ao_data.samplerate, &ao_data.channels, &ao_data.format)){
      mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_AudioFilterChainPreinitError);
      exit_player(MSGTR_Exit_error);
  }
#endif  
  current_module="ao2_init";
  if(!(audio_out=init_best_audio_out(audio_driver_list,
      0, // plugin flag
      ao_data.samplerate,
      ao_data.channels,
      ao_data.format,0))){
    // FAILED:
    mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CannotInitAO);
    uninit_player(INITED_ACODEC); // close codec
    sh_audio=d_audio->sh=NULL; // -> nosound
    d_audio->id = -2;
    return;
  } else {
    // SUCCESS:
    inited_flags|=INITED_AO;
    mp_msg(MSGT_CPLAYER,MSGL_INFO,"AO: [%s] %dHz %dch %s (%d bytes per sample)\n",
      audio_out->info->short_name,
      ao_data.samplerate, ao_data.channels,
      af_fmt2str_short(ao_data.format),
      af_fmt2bits(ao_data.format)/8 );
    mp_msg(MSGT_CPLAYER,MSGL_V,"AO: Description: %s\nAO: Author: %s\n",
      audio_out->info->name, audio_out->info->author);
    if(strlen(audio_out->info->comment) > 0)
      mp_msg(MSGT_CPLAYER,MSGL_V,"AO: Comment: %s\n", audio_out->info->comment);
    // init audio filters:
#if 1
    current_module="af_init";
    if(!build_afilter_chain(sh_audio, &ao_data)) {
      mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_NoMatchingFilter);
//      mp_msg(MSGT_CPLAYER,MSGL_ERR,"Couldn't find matching filter / ao format! -> NOSOUND\n");
//      uninit_player(INITED_ACODEC|INITED_AO); // close codec & ao
//      sh_audio=d_audio->sh=NULL; // -> nosound
    }
#endif
  }
  mixer.audio_out = audio_out;
  mixer.volstep = volstep;
}
}


///@}
// Command2Property


// Return pts value corresponding to the end point of audio written to the
// ao so far.
static double written_audio_pts(sh_audio_t *sh_audio, demux_stream_t *d_audio)
{
    // first calculate the end pts of audio that has been output by decoder
    double a_pts = sh_audio->pts;
    if (a_pts != MP_NOPTS_VALUE)
	// Good, decoder supports new way of calculating audio pts.
	// sh_audio->pts is the timestamp of the latest input packet with
	// known pts that the decoder has decoded. sh_audio->pts_bytes is
	// the amount of bytes the decoder has written after that timestamp.
	a_pts += sh_audio->pts_bytes / (double) sh_audio->o_bps;
    else {
	// Decoder doesn't support new way of calculating pts (or we're
	// being called before it has decoded anything with known timestamp).
	// Use the old method of audio pts calculation: take the timestamp
	// of last packet with known pts the decoder has read data from,
	// and add amount of bytes read after the beginning of that packet
	// divided by input bps. This will be inaccurate if the input/output
	// ratio is not constant for every audio packet or if it is constant
	// but not accurately known in sh_audio->i_bps.

	a_pts = d_audio->pts;
	// ds_tell_pts returns bytes read after last timestamp from
	// demuxing layer, decoder might use sh_audio->a_in_buffer for bytes
	// it has read but not decoded
	if (sh_audio->i_bps)
	    a_pts += (ds_tell_pts(d_audio) - sh_audio->a_in_buffer_len) /
		(double)sh_audio->i_bps;
    }
    // Now a_pts hopefully holds the pts for end of audio from decoder.
    // Substract data in buffers between decoder and audio out.

    // Decoded but not filtered
    a_pts -= sh_audio->a_buffer_len / (double)sh_audio->o_bps;

    // Data that was ready for ao but was buffered because ao didn't fully
    // accept everything to internal buffers yet
    a_pts -= sh_audio->a_out_buffer_len * playback_speed / (double)ao_data.bps;

    return a_pts;
}

// Return pts value corresponding to currently playing audio.
static double playing_audio_pts(sh_audio_t *sh_audio, demux_stream_t *d_audio,
				ao_functions_t *audio_out)
{
    return written_audio_pts(sh_audio, d_audio) - playback_speed *
	audio_out->get_delay();
}

static void update_subtitles(void)
{
    // find sub
    if (subdata) {
	double pts = sh_video->pts;
	if (sub_fps==0) sub_fps = sh_video->fps;
	current_module = "find_sub";
	if (pts > sub_last_pts || pts < sub_last_pts-1.0) {
	    find_sub(subdata, (pts+sub_delay) *
		     (subdata->sub_uses_time ? 100. : sub_fps)); 
	    if (vo_sub) vo_sub_last = vo_sub;
	    // FIXME! frame counter...
	    sub_last_pts = pts;
	}
    }

    // DVD sub:
    if (vo_config_count && vo_spudec) {
	unsigned char* packet=NULL;
	int len, timestamp;
	current_module = "spudec";
	spudec_heartbeat(vo_spudec, 90000*sh_video->timer);
	/* Get a sub packet from the DVD or a vobsub and make a timestamp
	 * relative to sh_video->timer */
	while(1) {
	    // Vobsub
	    len = 0;
	    if (vo_vobsub) {
		if (sh_video->pts+sub_delay >= 0) {
		    len = vobsub_get_packet(vo_vobsub, sh_video->pts+sub_delay,
					    (void**)&packet, &timestamp);
		    if (len > 0) {
			timestamp -= (sh_video->pts + sub_delay - sh_video->timer)*90000;
			mp_dbg(MSGT_CPLAYER,MSGL_V,"\rVOB sub: len=%d v_pts=%5.3f v_timer=%5.3f sub=%5.3f ts=%d \n",len,sh_video->pts,sh_video->timer,timestamp / 90000.0,timestamp);
		    }
		}
	    } else {
		// DVD sub
		len = ds_get_packet_sub(d_dvdsub, (unsigned char**)&packet);
		if (len > 0) {
		    // XXX This is wrong, sh_video->pts can be arbitrarily
		    // much behind demuxing position. Unfortunately using
		    // d_video->pts which would have been the simplest
		    // improvement doesn't work because mpeg specific hacks
		    // in video.c set d_video->pts to 0.
		    float x = d_dvdsub->pts - sh_video->pts;
		    if (x > -20 && x < 20) // prevent missing subs on pts reset
			timestamp = 90000*(sh_video->timer + d_dvdsub->pts
					   + sub_delay - sh_video->pts);
		    else timestamp = 90000*(sh_video->timer + sub_delay);
		    mp_dbg(MSGT_CPLAYER, MSGL_V, "\rDVD sub: len=%d  "
			   "v_pts=%5.3f  s_pts=%5.3f  ts=%d \n", len,
			   sh_video->pts, d_dvdsub->pts, timestamp);
		}
	    }
	    if (len<=0 || !packet) break;
	    if (timestamp >= 0)
		spudec_assemble(vo_spudec, packet, len, timestamp);
	}

	if (spudec_changed(vo_spudec))
	    vo_osd_changed(OSDTYPE_SPU);
    }
    current_module=NULL;
}

static int generate_video_frame(sh_video_t *sh_video, demux_stream_t *d_video)
{
    unsigned char *start;
    int in_size;
    int hit_eof=0;
    double pts;

    while (1) {
	void *decoded_frame;
	current_module = "decode video";
	// XXX Time used in this call is not counted in any performance
	// timer now, OSD is not updated correctly for filter-added frames
	if (vf_output_queued_frame(sh_video->vfilter))
	    break;
	current_module = "video_read_frame";
	in_size = ds_get_packet_pts(d_video, &start, &pts);
	if (in_size < 0) {
	    // try to extract last frames in case of decoder lag
	    in_size = 0;
	    pts = 1e300;
	    hit_eof = 1;
	}
	if (in_size > max_framesize)
	    max_framesize = in_size;
	if (pts == MP_NOPTS_VALUE)
	    mp_msg(MSGT_CPLAYER, MSGL_ERR, "pts value from demuxer MISSING\n");
	current_module = "decode video";
	decoded_frame = decode_video(sh_video, start, in_size, 0, pts);
	if (decoded_frame) {
	    update_subtitles();
	    update_osd_msg();
	    current_module = "filter video";
	    if (filter_video(sh_video, decoded_frame, sh_video->pts))
		break;
	}
	if (hit_eof)
	    return 0;
    }
    return 1;
}

static void rescale_input_coordinates(int ix, int iy, double *dx, double *dy) {
    //remove the borders, if any, and rescale to the range [0,1],[0,1]
    if(vo_fs) {  //we are in full-screen mode
        if(vo_screenwidth > vo_dwidth) //there are borders along the x axis
            ix -= (vo_screenwidth - vo_dwidth) / 2;
        if(vo_screenheight > vo_dheight) //there are borders along the y axis (usual way)
            iy -= (vo_screenheight - vo_dheight) / 2;

        if(ix < 0 || ix > vo_dwidth)  {*dx = *dy = -1.0; return; }  //we are on one of the borders
        if(iy < 0 || iy > vo_dheight) {*dx = *dy = -1.0; return; } //we are on one of the borders
    }

    *dx = (double) ix / (double) vo_dwidth;
    *dy = (double) iy / (double) vo_dheight;

    mp_msg(MSGT_CPLAYER,MSGL_V, "\r\nrescaled coordinates: %.3lf, %.3lf, screen (%d x %d), vodisplay: (%d, %d), fullscreen: %d\r\n",
        *dx, *dy, vo_screenwidth, vo_screenheight,  vo_dwidth, vo_dheight, vo_fs);
}

#ifdef HAVE_RTC
    int rtc_fd = -1;
#endif

static float timing_sleep(float time_frame)
{
#ifdef HAVE_RTC
    if (rtc_fd >= 0){
	// -------- RTC -----------
	current_module="sleep_rtc";
        while (time_frame > 0.000) {
	    unsigned long rtc_ts;
	    if (read(rtc_fd, &rtc_ts, sizeof(rtc_ts)) <= 0)
		mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_LinuxRTCReadError, strerror(errno));
    	    time_frame -= GetRelativeTime();
	}
    } else
#endif
#ifdef SYS_DARWIN
	{
	    current_module = "sleep_darwin";
	    while (time_frame > 0.005) {
		usec_sleep(1000000*time_frame);
		time_frame -= GetRelativeTime();
	    }
	}
#else
    {
	// assume kernel HZ=100 for softsleep, works with larger HZ but with
	// unnecessarily high CPU usage
	float margin = softsleep ? 0.011 : 0;
	current_module = "sleep_timer";
	while (time_frame > margin) {
	    usec_sleep(1000000 * (time_frame - margin));
	    time_frame -= GetRelativeTime();
	}
	if (softsleep){
	    current_module = "sleep_soft";
	    if (time_frame < 0)
		mp_msg(MSGT_AVSYNC, MSGL_WARN, MSGTR_SoftsleepUnderflow);
	    while (time_frame > 0)
		time_frame-=GetRelativeTime(); // burn the CPU
	}
    }
#endif /* SYS_DARWIN */
    return time_frame;
}

static void adjust_sync_and_print_status(int between_frames, float timing_error)
{
    current_module="av_sync";

    if(sh_audio){
	double a_pts, v_pts;

	if (autosync)
	    /*
	     * If autosync is enabled, the value for delay must be calculated
	     * a bit differently.  It is set only to the difference between
	     * the audio and video timers.  Any attempt to include the real
	     * or corrected delay causes the pts_correction code below to
	     * try to correct for the changes in delay which autosync is
	     * trying to measure.  This keeps the two from competing, but still
	     * allows the code to correct for PTS drift *only*.  (Using a delay
	     * value here, even a "corrected" one, would be incompatible with
	     * autosync mode.)
	     */
	    a_pts = written_audio_pts(sh_audio, d_audio) - sh_audio->delay;
	else
	    a_pts = playing_audio_pts(sh_audio, d_audio, audio_out);

	v_pts = sh_video->pts;

	{
	    static int drop_message=0;
	    double AV_delay = a_pts - audio_delay - v_pts;
	    double x;
	    if (AV_delay>0.5 && drop_frame_cnt>50 && drop_message==0){
		++drop_message;
		mp_msg(MSGT_AVSYNC,MSGL_WARN,MSGTR_SystemTooSlow);
	    }
	    if (autosync)
		x = AV_delay*0.1f;
	    else
		/* Do not correct target time for the next frame if this frame
		 * was late not because of wrong target time but because the
		 * target time could not be met */
		x = (AV_delay + timing_error * playback_speed) * 0.1f;
	    if (x < -max_pts_correction)
		x = -max_pts_correction;
	    else if (x> max_pts_correction)
		x = max_pts_correction;
	    if (default_max_pts_correction >= 0)
		max_pts_correction = default_max_pts_correction;
	    else
		max_pts_correction = sh_video->frametime*0.10; // +-10% of time
	    if (!between_frames) {
		sh_audio->delay+=x;
		c_total+=x;
	    }
	    if(!quiet)
		print_status(a_pts - audio_delay, AV_delay, c_total);
	}
    
    } else {
	// No audio:
    
	if (!quiet)
	    print_status(0, 0, 0);
    }
}

static int fill_audio_out_buffers(void)
{
    unsigned int t;
    double tt;
    int playsize;
    int playflags=0;
    int audio_eof=0;
    int bytes_to_write;

    current_module="play_audio";

    while (1) {
	// all the current uses of ao_data.pts seem to be in aos that handle
	// sync completely wrong; there should be no need to use ao_data.pts
	// in get_space()
	ao_data.pts = ((sh_video?sh_video->timer:0)+sh_audio->delay)*90000.0;
	bytes_to_write = audio_out->get_space();
	if (sh_video || bytes_to_write >= ao_data.outburst)
	    break;

	// handle audio-only case:
	// this is where mplayer sleeps during audio-only playback
	// to avoid 100% CPU use
	usec_sleep(10000); // Wait a tick before retry
    }

    while (bytes_to_write) {
	playsize = bytes_to_write;
	if (playsize > MAX_OUTBURST)
	    playsize = MAX_OUTBURST;
	bytes_to_write -= playsize;

	// Fill buffer if needed:
	current_module="decode_audio";
	t = GetTimer();
	while (sh_audio->a_out_buffer_len < playsize) {
	    int buflen = sh_audio->a_out_buffer_len;
	    int ret = decode_audio(sh_audio, &sh_audio->a_out_buffer[buflen],
				   playsize - buflen, // min bytes
				   sh_audio->a_out_buffer_size - buflen // max
				   );
	    if (ret <= 0) { // EOF?
		if (d_audio->eof) {
		    audio_eof = 1;
		    if (sh_audio->a_out_buffer_len == 0)
			return 0;
		}
		break;
	    }
	    sh_audio->a_out_buffer_len += ret;
	}
	t = GetTimer() - t;
	tt = t*0.000001f; audio_time_usage+=tt;
	if (playsize > sh_audio->a_out_buffer_len) {
	    playsize = sh_audio->a_out_buffer_len;
	    if (audio_eof)
		playflags |= AOPLAY_FINAL_CHUNK;
	}
	if (!playsize)
	    break;

	// play audio:  
	current_module="play_audio";

	// Is this pts value actually useful for the aos that access it?
	// They're obviously badly broken in the way they handle av sync;
	// would not having access to this make them more broken?
	ao_data.pts = ((sh_video?sh_video->timer:0)+sh_audio->delay)*90000.0;
	playsize = audio_out->play(sh_audio->a_out_buffer, playsize, playflags);

	if (playsize > 0) {
	    sh_audio->a_out_buffer_len -= playsize;
	    memmove(sh_audio->a_out_buffer, &sh_audio->a_out_buffer[playsize],
		    sh_audio->a_out_buffer_len);
	    sh_audio->delay += playback_speed*playsize/(double)ao_data.bps;
	}
	else if (audio_eof && audio_out->get_delay() < .04) {
	    // Sanity check to avoid hanging in case current ao doesn't output
	    // partial chunks and doesn't check for AOPLAY_FINAL_CHUNK
	    mp_msg(MSGT_CPLAYER, MSGL_WARN, "Audio output truncated at end.\n");
	    sh_audio->a_out_buffer_len = 0;
	}
    }
    return 1;
}

static int sleep_until_update(float *time_frame, float *aq_sleep_time)
{
    int frame_time_remaining = 0;
    current_module="calc_sleep_time";

    *time_frame -= GetRelativeTime(); // reset timer

    if (sh_audio && !d_audio->eof) {
	float delay = audio_out->get_delay();
	mp_dbg(MSGT_AVSYNC, MSGL_DBG2, "delay=%f\n", delay);

	if (autosync) {
	    /*
	     * Adjust this raw delay value by calculating the expected
	     * delay for this frame and generating a new value which is
	     * weighted between the two.  The higher autosync is, the
	     * closer to the delay value gets to that which "-nosound"
	     * would have used, and the longer it will take for A/V
	     * sync to settle at the right value (but it eventually will.)
	     * This settling time is very short for values below 100.
	     */
	    float predicted = sh_audio->delay / playback_speed + *time_frame;
	    float difference = delay - predicted;
	    delay = predicted + difference / (float)autosync;
	}

	*time_frame = delay - sh_audio->delay / playback_speed;

	// delay = amount of audio buffered in soundcard/driver
	if (delay > 0.25) delay=0.25; else
	if (delay < 0.10) delay=0.10;
	if (*time_frame > delay*0.6) {
	    // sleep time too big - may cause audio drops (buffer underrun)
	    frame_time_remaining = 1;
	    *time_frame = delay*0.5;
	}
    } else {
	// If we're lagging more than 200 ms behind the right playback rate,
	// don't try to "catch up".
	// If benchmark is set always output frames as fast as possible
	// without sleeping.
	if (*time_frame < -0.2 || benchmark)
	    *time_frame = 0;
    }

    *aq_sleep_time += *time_frame;


    //============================== SLEEP: ===================================

    // flag 256 means: libvo driver does its timing (dvb card)
    if (*time_frame > 0.001 && !(vo_flags&256))
	*time_frame = timing_sleep(*time_frame);
    return frame_time_remaining;
}

static int reinit_video_chain(void) {
    //================== Init VIDEO (codec & libvo) ==========================
    if(!fixed_vo || !(inited_flags&INITED_VO)){
    current_module="preinit_libvo";

    //shouldn't we set dvideo->id=-2 when we fail?
    vo_config_count=0;
    //if((video_out->preinit(vo_subdevice))!=0){
    if(!(video_out=init_best_video_out(video_driver_list))){
      mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_ErrorInitializingVODevice);
      goto err_out;
    }
    sh_video->video_out=video_out;
    inited_flags|=INITED_VO;
  }

  current_module="init_video_filters";
  {
    char* vf_arg[] = { "_oldargs_", (char*)video_out , NULL };
    sh_video->vfilter=(void*)vf_open_filter(NULL,"vo",vf_arg);
  }
#ifdef HAVE_MENU
  if(use_menu) {
    char* vf_arg[] = { "_oldargs_", menu_root, NULL };
    vf_menu = vf_open_plugin(libmenu_vfs,sh_video->vfilter,"menu",vf_arg);
    if(!vf_menu) {
      mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CantOpenLibmenuFilterWithThisRootMenu,menu_root);
      use_menu = 0;
    }
  }
  if(vf_menu)
    sh_video->vfilter=(void*)vf_menu;
#endif

#ifdef USE_ASS
  if(ass_enabled) {
    int i;
    int insert = 1;
    if (vf_settings)
      for (i = 0; vf_settings[i].name; ++i)
        if (strcmp(vf_settings[i].name, "ass") == 0) {
          insert = 0;
          break;
        }
    if (insert) {
      extern vf_info_t vf_info_ass;
      vf_info_t* libass_vfs[] = {&vf_info_ass, NULL};
      char* vf_arg[] = {"auto", "1", NULL};
      vf_instance_t* vf_ass = vf_open_plugin(libass_vfs,sh_video->vfilter,"ass",vf_arg);
      if (vf_ass)
        sh_video->vfilter=(void*)vf_ass;
      else
        mp_msg(MSGT_CPLAYER,MSGL_ERR, "ASS: cannot add video filter\n");
    }
  }
#endif

  sh_video->vfilter=(void*)append_filters(sh_video->vfilter);

#ifdef USE_ASS
  if (ass_enabled)
    ((vf_instance_t *)sh_video->vfilter)->control(sh_video->vfilter, VFCTRL_INIT_EOSD, ass_library);
#endif

  current_module="init_video_codec";

  mp_msg(MSGT_CPLAYER,MSGL_INFO,"==========================================================================\n");
  init_best_video_codec(sh_video,video_codec_list,video_fm_list);
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"==========================================================================\n");

  if(!sh_video->inited){
    if(!fixed_vo) uninit_player(INITED_VO);
    goto err_out;
  }

  inited_flags|=INITED_VCODEC;

  if (sh_video->codec)
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_CODEC=%s\n", sh_video->codec->name);

  sh_video->last_pts = MP_NOPTS_VALUE;
  sh_video->num_buffered_pts = 0;
  sh_video->next_frame_time = 0;

  if(auto_quality>0){
    // Auto quality option enabled
    output_quality=get_video_quality_max(sh_video);
    if(auto_quality>output_quality) auto_quality=output_quality;
    else output_quality=auto_quality;
    mp_msg(MSGT_CPLAYER,MSGL_V,"AutoQ: setting quality to %d.\n",output_quality);
    set_video_quality(sh_video,output_quality);
  }

  // ========== Init display (sh_video->disp_w*sh_video->disp_h/out_fmt) ============

  current_module="init_vo";

  return 1;

err_out:
  sh_video = d_video->sh = NULL;
  return 0;
}

static double update_video(int *blit_frame)
{
    //--------------------  Decode a frame: -----------------------
    double frame_time;
    *blit_frame = 0; // Don't blit if we hit EOF
    if (!correct_pts) {
	unsigned char* start=NULL;
	void *decoded_frame;
	int drop_frame=0;
	int in_size;

	current_module = "video_read_frame";
	frame_time = sh_video->next_frame_time;
	in_size = video_read_frame(sh_video, &sh_video->next_frame_time,
				   &start, force_fps);
	if (in_size < 0)
	    return -1;
	if (in_size > max_framesize)
	    max_framesize = in_size; // stats
	sh_video->timer += frame_time;
	if (sh_audio)
	    sh_audio->delay -= frame_time;
	// video_read_frame can change fps (e.g. for ASF video)
	vo_fps = sh_video->fps;
	// check for frame-drop:
	current_module = "check_framedrop";
	if (sh_audio && !d_audio->eof) {
	    static int dropped_frames;
	    float delay = playback_speed*audio_out->get_delay();
	    float d = delay-sh_audio->delay;
	    // we should avoid dropping too many frames in sequence unless we
	    // are too late. and we allow 100ms A-V delay here:
	    if (d < -dropped_frames*frame_time-0.100 &&
				osd_function != OSD_PAUSE) {
		drop_frame = frame_dropping;
		++drop_frame_cnt;
		++dropped_frames;
	    } else
		drop_frame = dropped_frames = 0;
	    ++total_frame_cnt;
	}
	update_subtitles();
	update_osd_msg();
	current_module = "decode_video";
	decoded_frame = decode_video(sh_video, start, in_size, drop_frame,
				     sh_video->pts);
	current_module = "filter_video";
	*blit_frame = (decoded_frame && filter_video(sh_video, decoded_frame,
						    sh_video->pts));
    }
    else {
	if (!generate_video_frame(sh_video, d_video))
	    return -1;
	((vf_instance_t *)sh_video->vfilter)->control(sh_video->vfilter,
					    VFCTRL_GET_PTS, &sh_video->pts);
	if (sh_video->pts == MP_NOPTS_VALUE) {
	    mp_msg(MSGT_CPLAYER, MSGL_ERR, "pts after filters MISSING\n");
	    sh_video->pts = sh_video->last_pts;
	}
	if (sh_video->last_pts == MP_NOPTS_VALUE)
	    sh_video->last_pts= sh_video->pts;
	else if (sh_video->last_pts >= sh_video->pts) {
	    sh_video->last_pts = sh_video->pts;
	    mp_msg(MSGT_CPLAYER, MSGL_WARN, "pts value <= previous");
	}
	frame_time = sh_video->pts - sh_video->last_pts;
	sh_video->last_pts = sh_video->pts;
	sh_video->timer += frame_time;
	if(sh_audio)
	    sh_audio->delay -= frame_time;
	*blit_frame = 1;
    }
    return frame_time;
}

void pause_loop(void)
{
    mp_cmd_t* cmd;
    if (!quiet) {
        // Small hack to display the pause message on the OSD line.
        // The pause string is: "\n == PAUSE == \r" so we need to
        // take the first and the last char out
	if (term_osd && !sh_video) {
	    char msg[128] = MSGTR_Paused;
	    int mlen = strlen(msg);
	    msg[mlen-1] = '\0';
	    set_osd_msg(OSD_MSG_PAUSE, 1, 0, "%s", msg+1);
	    update_osd_msg();
	} else
	    mp_msg(MSGT_CPLAYER,MSGL_STATUS,MSGTR_Paused);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_PAUSED\n");
	fflush(stdout);
    }
#ifdef HAVE_NEW_GUI
    if (use_gui)
	guiGetEvent(guiCEvent, (char *)guiSetPause);
#endif
    if (video_out && sh_video && vo_config_count)
	video_out->control(VOCTRL_PAUSE, NULL);

    if (audio_out && sh_audio)
	audio_out->pause();	// pause audio, keep data if possible

    while ( (cmd = mp_input_get_cmd(20, 1, 1)) == NULL) {
	if (sh_video && video_out && vo_config_count)
	    video_out->check_events();
#ifdef HAVE_NEW_GUI
	if (use_gui) {
	    guiEventHandling();
	    guiGetEvent(guiReDraw, NULL);
	    if (guiIntfStruct.Playing!=2 || (rel_seek_secs || abs_seek_pos))
		break;
	}
#endif
#ifdef HAVE_MENU
	if (vf_menu)
	    vf_menu_pause_update(vf_menu);
#endif
	usec_sleep(20000);
    }
    if (cmd && cmd->id == MP_CMD_PAUSE) {
	cmd = mp_input_get_cmd(0,1,0);
	mp_cmd_free(cmd);
    }
    osd_function=OSD_PLAY;
    if (audio_out && sh_audio)
        audio_out->resume();	// resume audio
    if (video_out && sh_video && vo_config_count)
        video_out->control(VOCTRL_RESUME, NULL);	// resume video
    (void)GetRelativeTime();	// ignore time that passed during pause
#ifdef HAVE_NEW_GUI
    if (use_gui) {
	if (guiIntfStruct.Playing == guiSetStop)
	    eof = 1;
	else
	    guiGetEvent(guiCEvent, (char *)guiSetPlay);
    }
#endif
}


int main(int argc,char* argv[]){


char * mem_ptr;

int file_format=DEMUXER_TYPE_UNKNOWN;

// movie info:

/* Flag indicating whether MPlayer should exit without playing anything. */
int opt_exit = 0;

//float a_frame=0;    // Audio

int i;
char *tmp;

int gui_no_filename=0;

  srand((int) time(NULL)); 

  InitTimer();
  
  mp_msg_init();

  mp_msg(MSGT_CPLAYER,MSGL_INFO, "MPlayer " VERSION " (C) 2000-2006 MPlayer Team\n");
  /* Test for CPU capabilities (and corresponding OS support) for optimizing */
  GetCpuCaps(&gCpuCaps);
#ifdef ARCH_X86
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"CPUflags:  MMX: %d MMX2: %d 3DNow: %d 3DNow2: %d SSE: %d SSE2: %d\n",
      gCpuCaps.hasMMX,gCpuCaps.hasMMX2,
      gCpuCaps.has3DNow, gCpuCaps.has3DNowExt,
      gCpuCaps.hasSSE, gCpuCaps.hasSSE2);
#ifdef RUNTIME_CPUDETECT
  mp_msg(MSGT_CPLAYER,MSGL_INFO, MSGTR_CompiledWithRuntimeDetection);
#else
  mp_msg(MSGT_CPLAYER,MSGL_INFO, MSGTR_CompiledWithCPUExtensions);
#ifdef HAVE_MMX
  mp_msg(MSGT_CPLAYER,MSGL_INFO," MMX");
#endif
#ifdef HAVE_MMX2
  mp_msg(MSGT_CPLAYER,MSGL_INFO," MMX2");
#endif
#ifdef HAVE_3DNOW
  mp_msg(MSGT_CPLAYER,MSGL_INFO," 3DNow");
#endif
#ifdef HAVE_3DNOWEX
  mp_msg(MSGT_CPLAYER,MSGL_INFO," 3DNowEx");
#endif
#ifdef HAVE_SSE
  mp_msg(MSGT_CPLAYER,MSGL_INFO," SSE");
#endif
#ifdef HAVE_SSE2
  mp_msg(MSGT_CPLAYER,MSGL_INFO," SSE2");
#endif
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"\n");
#endif /* RUNTIME_CPUDETECT */
#endif /* ARCH_X86 */

#if defined(WIN32) && defined(USE_WIN32DLL)
  set_path_env();
#endif /*WIN32 && USE_WIN32DLL*/

#ifdef USE_TV
  tv_param_immediate = 1;
#endif

  if (argc > 1 && argv[1] &&
      (!strcmp(argv[1], "-gui") || !strcmp(argv[1], "-nogui"))) {
    use_gui = !strcmp(argv[1], "-gui");
  } else
  if ( argv[0] )
  {
    char *base = strrchr(argv[0], '/');
    if (!base)
      base = strrchr(argv[0], '\\');
    if (!base)
      base = argv[0];
    if(strstr(base, "gmplayer"))
          use_gui=1;
  }

    mconfig = m_config_new();
    m_config_register_options(mconfig,mplayer_opts);
    // TODO : add something to let modules register their options
    mp_input_register_options(mconfig);
    parse_cfgfiles(mconfig);

#ifdef HAVE_NEW_GUI
    if ( use_gui ) cfg_read();
#endif

    playtree = m_config_parse_mp_command_line(mconfig, argc, argv);
    if(playtree == NULL)
      opt_exit = 1;
    else {
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
    }
	
#if defined(WIN32) && defined(HAVE_NEW_GUI)
    void *runningmplayer = FindWindow("MPlayer GUI for Windows", "MPlayer for Windows");
    if(runningmplayer && filename && use_gui){
        COPYDATASTRUCT csData;
        char file[MAX_PATH];
        char *filepart = filename;
        if(GetFullPathName(filename, MAX_PATH, file, &filepart)){
            csData.dwData = 0;
            csData.cbData = strlen(file)*2;
            csData.lpData = file;
            SendMessage(runningmplayer, WM_COPYDATA, (WPARAM)runningmplayer, (LPARAM)&csData);
        }
    }
#endif

#ifdef WIN32
	if(proc_priority){
		int i;
        	for(i=0; priority_presets_defs[i].name; i++){
        		if(strcasecmp(priority_presets_defs[i].name, proc_priority) == 0)
				break;
		}
		mp_msg(MSGT_CPLAYER,MSGL_STATUS,"Setting process priority: %s\n",
				priority_presets_defs[i].name);
		SetPriorityClass(GetCurrentProcess(), priority_presets_defs[i].prio);
	}
#endif	
#ifndef HAVE_NEW_GUI
    if(use_gui){
      mp_msg(MSGT_CPLAYER,MSGL_WARN,MSGTR_NoGui);
      use_gui=0;
    }
#else
#ifndef WIN32
    if(use_gui && !vo_init()){
      mp_msg(MSGT_CPLAYER,MSGL_WARN,MSGTR_GuiNeedsX);
      use_gui=0;
    }
#endif
    if (use_gui && playtree_iter){
      char cwd[PATH_MAX+2];
      // Free Playtree and Playtree-Iter as it's not used by the GUI.
      play_tree_iter_free(playtree_iter);
      playtree_iter=NULL;
      
      if (getcwd(cwd, PATH_MAX) != (char *)NULL)
      {
	  strcat(cwd, "/");
          // Prefix relative paths with current working directory
          play_tree_add_bpf(playtree, cwd);
      }      
      // Import initital playtree into GUI.
      import_initial_playtree_into_gui(playtree, mconfig, enqueue);
    }
#endif /* HAVE_NEW_GUI */

    if(video_driver_list && strcmp(video_driver_list[0],"help")==0){
      list_video_out();
      opt_exit = 1;
    }

    if(audio_driver_list && strcmp(audio_driver_list[0],"help")==0){
      list_audio_out();
      opt_exit = 1;
    }

/* Check codecs.conf. */
if(!codecs_file || !parse_codec_cfg(codecs_file)){
  if(!parse_codec_cfg(mem_ptr=get_path("codecs.conf"))){
    if(!parse_codec_cfg(MPLAYER_CONFDIR "/codecs.conf")){
      if(!parse_codec_cfg(NULL)){
	mp_msg(MSGT_CPLAYER,MSGL_HINT,MSGTR_CopyCodecsConf);
        exit_player_with_rc(NULL, 0);
      }
      mp_msg(MSGT_CPLAYER,MSGL_V,MSGTR_BuiltinCodecsConf);
    }
  }
  free( mem_ptr ); // release the buffer created by get_path()
}

#if 0
    if(video_codec_list){
	int i;
	video_codec=video_codec_list[0];
	for(i=0;video_codec_list[i];i++)
	    mp_msg(MSGT_FIXME,MSGL_FIXME,"vc#%d: '%s'\n",i,video_codec_list[i]);
    }
#endif
    if(audio_codec_list && strcmp(audio_codec_list[0],"help")==0){
      mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_AvailableAudioCodecs);
      mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_CODECS\n");
      list_codecs(1);
      mp_msg(MSGT_FIXME, MSGL_FIXME, "\n");
      opt_exit = 1;
    }
    if(video_codec_list && strcmp(video_codec_list[0],"help")==0){
      mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_AvailableVideoCodecs);
      mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_CODECS\n");
      list_codecs(0);
      mp_msg(MSGT_FIXME, MSGL_FIXME, "\n");
      opt_exit = 1;
    }
    if(video_fm_list && strcmp(video_fm_list[0],"help")==0){
      vfm_help();
      mp_msg(MSGT_FIXME, MSGL_FIXME, "\n");
      opt_exit = 1;
    }
    if(audio_fm_list && strcmp(audio_fm_list[0],"help")==0){
      afm_help();
      mp_msg(MSGT_FIXME, MSGL_FIXME, "\n");
      opt_exit = 1;
    }
    if(af_cfg.list && strcmp(af_cfg.list[0],"help")==0){
      af_help();
      printf("\n");
      opt_exit = 1;
    }
#ifdef HAVE_X11
    if(vo_fstype_list && strcmp(vo_fstype_list[0],"help")==0){
      fstype_help();
      mp_msg(MSGT_FIXME, MSGL_FIXME, "\n");
      opt_exit = 1;
    }
#endif
    if((demuxer_name && strcmp(demuxer_name,"help")==0) ||
       (audio_demuxer_name && strcmp(audio_demuxer_name,"help")==0) ||
       (sub_demuxer_name && strcmp(sub_demuxer_name,"help")==0)){
      demuxer_help();
      mp_msg(MSGT_CPLAYER, MSGL_INFO, "\n");
      opt_exit = 1;
    }
    if(list_properties) {
      m_properties_print_help_list(mp_properties);
      opt_exit = 1;
    }

    if(opt_exit)
      exit_player(NULL);

    if (player_idle_mode && use_gui) {
        mp_msg(MSGT_CPLAYER, MSGL_FATAL, MSGTR_NoIdleAndGui);
        exit_player_with_rc(NULL, 1);
    }

    if(!filename && !player_idle_mode){
      if(!use_gui){
	// no file/vcd/dvd -> show HELP:
	mp_msg(MSGT_CPLAYER, MSGL_INFO, help_text);
        exit_player_with_rc(NULL, 0);
      } else gui_no_filename=1;
    }

    /* Display what configure line was used */
    mp_msg(MSGT_CPLAYER, MSGL_V, "Configuration: " CONFIGURATION "\n");

    // Many users forget to include command line in bugreports...
    if( mp_msg_test(MSGT_CPLAYER,MSGL_V) ){
      mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_CommandLine);
      for(i=1;i<argc;i++)mp_msg(MSGT_CPLAYER, MSGL_INFO," '%s'",argv[i]);
      mp_msg(MSGT_CPLAYER, MSGL_INFO, "\n");
    }

//------ load global data first ------

// check font
#ifdef HAVE_FREETYPE
  init_freetype();
#endif
#ifdef HAVE_FONTCONFIG
  if(!font_fontconfig)
  {
#endif
#ifdef HAVE_BITMAP_FONT
  if(font_name){
       vo_font=read_font_desc(font_name,font_factor,verbose>1);
       if(!vo_font) mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CantLoadFont,font_name);
  } else {
      // try default:
       vo_font=read_font_desc( mem_ptr=get_path("font/font.desc"),font_factor,verbose>1);
       free(mem_ptr); // release the buffer created by get_path()
       if(!vo_font)
       vo_font=read_font_desc(MPLAYER_DATADIR "/font/font.desc",font_factor,verbose>1);
  }
#endif
#ifdef HAVE_FONTCONFIG
  }
#endif

  vo_init_osd();

#ifdef USE_ASS
  ass_library = ass_init();
#endif

#ifdef HAVE_RTC
  if(!nortc)
  {
    // seteuid(0); /* Can't hurt to try to get root here */
    if ((rtc_fd = open(rtc_device ? rtc_device : "/dev/rtc", O_RDONLY)) < 0)
	mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_RTCDeviceNotOpenable,
	    rtc_device ? rtc_device : "/dev/rtc", strerror(errno));
     else {
	unsigned long irqp = 1024; /* 512 seemed OK. 128 is jerky. */

	if (ioctl(rtc_fd, RTC_IRQP_SET, irqp) < 0) {
    	    mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_LinuxRTCInitErrorIrqpSet, irqp, strerror(errno));
    	    mp_msg(MSGT_CPLAYER, MSGL_HINT, MSGTR_IncreaseRTCMaxUserFreq, irqp);
   	    close (rtc_fd);
    	    rtc_fd = -1;
	} else if (ioctl(rtc_fd, RTC_PIE_ON, 0) < 0) {
	    /* variable only by the root */
    	    mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_LinuxRTCInitErrorPieOn, strerror(errno));
    	    close (rtc_fd);
	    rtc_fd = -1;
	} else
	    mp_msg(MSGT_CPLAYER, MSGL_V, MSGTR_UsingRTCTiming, irqp);
    }
  }
#ifdef HAVE_NEW_GUI
// breaks DGA and SVGAlib and VESA drivers:  --A'rpi
// and now ? -- Pontscho
    if(use_gui) setuid( getuid() ); // strongly test, please check this.
#endif
    if(rtc_fd<0)
#endif /* HAVE_RTC */
      mp_msg(MSGT_CPLAYER, MSGL_V, "Using %s timing\n",
	     softsleep?"software":timer_name);

#ifdef USE_TERMCAP
  if ( !use_gui ) load_termcap(NULL); // load key-codes
#endif

// ========== Init keyboard FIFO (connection to libvo) ============

// Init input system
current_module = "init_input";
mp_input_init(use_gui);
#if 0
make_pipe(&keyb_fifo_get,&keyb_fifo_put);

if(keyb_fifo_get > 0)
  mp_input_add_key_fd(keyb_fifo_get,1,NULL,NULL);
#else
  mp_input_add_key_fd(-1,0,mplayer_get_key,NULL);
#endif
if(slave_mode)
#ifndef __MINGW32__
   mp_input_add_cmd_fd(0,1,NULL,NULL);
#else
  mp_input_add_cmd_fd(0,0,mp_input_win32_slave_cmd_func,NULL);
#endif
else if(!noconsolecontrols)
#ifndef HAVE_NO_POSIX_SELECT
  mp_input_add_key_fd(0,1,NULL,NULL);
#else
  mp_input_add_key_fd(0,0,NULL,NULL);
#endif

inited_flags|=INITED_INPUT;
current_module = NULL;

#ifdef HAVE_MENU
 if(use_menu) {
   if(menu_cfg && menu_init(menu_cfg))
     mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_MenuInitialized, menu_cfg);
   else {
     menu_cfg = get_path("menu.conf");
     if(menu_init(menu_cfg))
       mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_MenuInitialized, menu_cfg);
     else {
       if(menu_init(MPLAYER_CONFDIR "/menu.conf"))
         mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_MenuInitialized, MPLAYER_CONFDIR"/menu.conf");
       else {
         mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_MenuInitFailed);
         use_menu = 0;
       }
     }
   }
 }
#endif
  
  /// Catch signals
#ifndef __MINGW32__
  signal(SIGCHLD,child_sighandler);
#endif

#ifdef CRASH_DEBUG
  prog_path = argv[0];
#endif
  //========= Catch terminate signals: ================
  // terminate requests:
  signal(SIGTERM,exit_sighandler); // kill
  signal(SIGHUP,exit_sighandler);  // kill -HUP  /  xterm closed

  signal(SIGINT,exit_sighandler);  // Interrupt from keyboard

  signal(SIGQUIT,exit_sighandler); // Quit from keyboard
  signal(SIGPIPE,exit_sighandler); // Some window managers cause this
#ifdef ENABLE_SIGHANDLER
  // fatal errors:
  signal(SIGBUS,exit_sighandler);  // bus error
  signal(SIGSEGV,exit_sighandler); // segfault
  signal(SIGILL,exit_sighandler);  // illegal instruction
  signal(SIGFPE,exit_sighandler);  // floating point exc.
  signal(SIGABRT,exit_sighandler); // abort()
#ifdef CRASH_DEBUG
  if (crash_debug)
    signal(SIGTRAP,exit_sighandler);
#endif
#endif

#ifdef HAVE_NEW_GUI
  if(use_gui){
       guiInit();
       inited_flags|=INITED_GUI;
       guiGetEvent( guiCEvent,(char *)((gui_no_filename) ? 0 : 1) );
  }
#endif

// ******************* Now, let's see the per-file stuff ********************

play_next_file:

  // init global sub numbers
  global_sub_size = 0;
  { int i; for (i = 0; i < SUB_SOURCES; i++) global_sub_indices[i] = -1; }

  if (filename) load_per_file_config (mconfig, filename);

// We must enable getch2 here to be able to interrupt network connection
// or cache filling
if(!noconsolecontrols && !slave_mode){
  if(inited_flags&INITED_GETCH2)
    mp_msg(MSGT_CPLAYER,MSGL_WARN,MSGTR_Getch2InitializedTwice);
  else
    getch2_enable();  // prepare stdin for hotkeys...
  inited_flags|=INITED_GETCH2;
  mp_msg(MSGT_CPLAYER,MSGL_DBG2,"\n[[[init getch2]]]\n");
}

// =================== GUI idle loop (STOP state) ===========================
#ifdef HAVE_NEW_GUI
    if ( use_gui ) {
      file_format=DEMUXER_TYPE_UNKNOWN;
      guiGetEvent( guiSetDefaults,0 );
      while ( guiIntfStruct.Playing != 1 )
       {
        mp_cmd_t* cmd;                                                                                   
	usec_sleep(20000);
	guiEventHandling();
	guiGetEvent( guiReDraw,NULL );
	if ( (cmd = mp_input_get_cmd(0,0,0)) != NULL) guiGetEvent( guiIEvent,(char *)cmd->id );
       } 
      guiGetEvent( guiSetParameters,NULL );
      if ( guiIntfStruct.StreamType == STREAMTYPE_STREAM )
       {
        play_tree_t * entry = play_tree_new();
        play_tree_add_file( entry,guiIntfStruct.Filename );
        if ( playtree ) play_tree_free_list( playtree->child,1 );
         else playtree=play_tree_new();
        play_tree_set_child( playtree,entry );
        if(playtree)
	 {
	  playtree_iter = play_tree_iter_new(playtree,mconfig);
	  if(playtree_iter)
	   {
	    if(play_tree_iter_step(playtree_iter,0,0) != PLAY_TREE_ITER_ENTRY)
	     {
	      play_tree_iter_free(playtree_iter);
	      playtree_iter = NULL;
	     }
	    filename = play_tree_iter_get_file(playtree_iter,1);
	   }
         }
       } 
    }
#endif /* HAVE_NEW_GUI */

while (player_idle_mode && !filename) {
    play_tree_t * entry = NULL;
    mp_cmd_t * cmd;
    while (!(cmd = mp_input_get_cmd(0,1,0))) { // wait for command
        if (video_out && vo_config_count) video_out->check_events();
        usec_sleep(20000);
    }
    switch (cmd->id) {
        case MP_CMD_LOADFILE:
            // prepare a tree entry with the new filename
            entry = play_tree_new();
            play_tree_add_file(entry, cmd->args[0].v.s);
            // The entry is added to the main playtree after the switch().
            break;
        case MP_CMD_LOADLIST:
            entry = parse_playlist_file(cmd->args[0].v.s);
            break;
        case MP_CMD_QUIT:
            exit_player_with_rc(MSGTR_Exit_quit, (cmd->nargs > 0)? cmd->args[0].v.i : 0);
            break;
    }

    mp_cmd_free(cmd);

    if (entry) { // user entered a command that gave a valid entry
        if (playtree) // the playtree is always a node with one child. let's clear it
            play_tree_free_list(playtree->child, 1);
        else playtree=play_tree_new(); // .. or make a brand new playtree

        if (!playtree) continue; // couldn't make playtree! wait for next command

        play_tree_set_child(playtree, entry);

        /* Make iterator start at the top the of tree. */
        playtree_iter = play_tree_iter_new(playtree, mconfig);
        if (!playtree_iter) continue;

        // find the first real item in the tree
        if (play_tree_iter_step(playtree_iter,0,0) != PLAY_TREE_ITER_ENTRY) {
            // no items!
            play_tree_iter_free(playtree_iter);
            playtree_iter = NULL;
            continue; // wait for next command
        }
        filename = play_tree_iter_get_file(playtree_iter, 1);
    }
}
//---------------------------------------------------------------------------

    if(filename) mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_Playing, filename);

if (edl_filename) {
    if (edl_records) free_edl(edl_records);
    next_edl_record = edl_records = edl_parse_file();
}
if (edl_output_filename) {
    if (edl_fd) fclose(edl_fd);
    if ((edl_fd = fopen(edl_output_filename, "w")) == NULL)
    {
        mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_EdlCantOpenForWrite,
               edl_output_filename);
    }
}

//==================== Open VOB-Sub ============================

    current_module="vobsub";
    if (vobsub_name){
      vo_vobsub=vobsub_open(vobsub_name,spudec_ifo,1,&vo_spudec);
      if(vo_vobsub==NULL)
        mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CantLoadSub,vobsub_name);
    }else if(sub_auto && filename && (strlen(filename)>=5)){
      /* try to autodetect vobsub from movie filename ::atmos */
      char *buf = malloc((strlen(filename)-3)),*psub;
      memset(buf,0,strlen(filename)-3); // make sure string is terminated
      strncpy(buf, filename, strlen(filename)-4); 
      vo_vobsub=vobsub_open(buf,spudec_ifo,0,&vo_spudec);
      /* try from ~/.mplayer/sub */
      if(!vo_vobsub && (psub = get_path( "sub/" ))) {
          char *bname;
          int l;
          bname = strrchr(buf,'/');
#ifdef WIN32
          if(!bname) bname = strrchr(buf,'\\');
#endif
          if(bname) bname++;
          else bname = buf;
          l = strlen(psub) + strlen(bname) + 1;
          psub = realloc(psub,l);
          strcat(psub,bname);
          vo_vobsub=vobsub_open(psub,spudec_ifo,0,&vo_spudec);
          free(psub);          
      }
      free(buf);
    }
    if(vo_vobsub){
      inited_flags|=INITED_VOBSUB;
      vobsub_set_from_lang(vo_vobsub, dvdsub_lang);
      // check if vobsub requested only to display forced subtitles
      forced_subs_only=vobsub_get_forced_subs_flag(vo_vobsub);

      // setup global sub numbering
      global_sub_indices[SUB_SOURCE_VOBSUB] = global_sub_size; // the global # of the first vobsub.
      global_sub_size += vobsub_get_indexes_count(vo_vobsub);
    }

//============ Open & Sync STREAM --- fork cache2 ====================

  stream=NULL;
  demuxer=NULL;
  if (d_audio) {
    //free_demuxer_stream(d_audio);
    d_audio=NULL;
  }
  if (d_video) {
    //free_demuxer_stream(d_video);
    d_video=NULL;
  }
  sh_audio=NULL;
  sh_video=NULL;

  current_module="open_stream";
  stream=open_stream(filename,0,&file_format);
  if(!stream) { // error...
    eof = libmpdemux_was_interrupted(PT_NEXT_ENTRY);
    goto goto_next_file;
  }
  inited_flags|=INITED_STREAM;

#ifdef HAVE_NEW_GUI
  if ( use_gui ) guiGetEvent( guiSetStream,(char *)stream );
#endif

  if(file_format == DEMUXER_TYPE_PLAYLIST) {
    play_tree_t* entry;
    // Handle playlist
    current_module="handle_playlist";
    mp_msg(MSGT_CPLAYER,MSGL_V,"Parsing playlist %s...\n",filename);
    entry = parse_playtree(stream,0);
    eof=playtree_add_playlist(entry);
    goto goto_next_file;
  }
  stream->start_pos+=seek_to_byte;

if(stream_dump_type==5){
  unsigned char buf[4096];
  int len;
  FILE *f;
  current_module="dumpstream";
  if(stream->type==STREAMTYPE_STREAM && stream->fd<0){
    mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_DumpstreamFdUnavailable);
    exit_player(MSGTR_Exit_error);
  }
  stream_reset(stream);
  stream_seek(stream,stream->start_pos);
  f=fopen(stream_dump_name,"wb");
  if(!f){
    mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_CantOpenDumpfile);
    exit_player(MSGTR_Exit_error);
  }
  while(!stream->eof){
      len=stream_read(stream,buf,4096);
      if(len>0) {
        if(fwrite(buf,len,1,f) != 1) {
          mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_ErrorWritingFile,stream_dump_name);
          exit_player(MSGTR_Exit_error);
        }
      }
  }
  if(fclose(f)) {
    mp_msg(MSGT_MENCODER,MSGL_FATAL,MSGTR_ErrorWritingFile,stream_dump_name);
    exit_player(MSGTR_Exit_error);
  }
  mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_CoreDumped);
  exit_player_with_rc(MSGTR_Exit_eof, 0);
}

#ifdef USE_DVDREAD
if(stream->type==STREAMTYPE_DVD){
  current_module="dvd lang->id";
  if(audio_id==-1) audio_id=dvd_aid_from_lang(stream,audio_lang);
  if(dvdsub_lang && dvdsub_id==-2) dvdsub_id=-1;
  if(dvdsub_lang && dvdsub_id==-1) dvdsub_id=dvd_sid_from_lang(stream,dvdsub_lang);
  // setup global sub numbering
  global_sub_indices[SUB_SOURCE_DEMUX] = global_sub_size; // the global # of the first demux-specific sub.
  global_sub_size += dvd_number_of_subs(stream);
  current_module=NULL;
}
#endif

#ifdef USE_DVDNAV
if(stream->type==STREAMTYPE_DVDNAV){
  current_module="dvdnav lang->id";
  if(audio_id==-1) audio_id=dvdnav_aid_from_lang(stream,audio_lang);
  if(dvdsub_lang && dvdsub_id==-2) dvdsub_id=-1;
  if(dvdsub_lang && dvdsub_id==-1) dvdsub_id=dvdnav_sid_from_lang(stream,dvdsub_lang);
  // setup global sub numbering
  global_sub_indices[SUB_SOURCE_DEMUX] = global_sub_size; // the global # of the first demux-specific sub.
  global_sub_size += dvdnav_number_of_subs(stream);
  current_module=NULL;
}
#endif

// CACHE2: initial prefill: 20%  later: 5%  (should be set by -cacheopts)
goto_enable_cache:
if(stream_cache_size>0){
  current_module="enable_cache";
  if(!stream_enable_cache(stream,stream_cache_size*1024,
                          stream_cache_size*1024*(stream_cache_min_percent / 100.0),
                          stream_cache_size*1024*(stream_cache_seek_min_percent / 100.0)))
    if((eof = libmpdemux_was_interrupted(PT_NEXT_ENTRY))) goto goto_next_file;
}

//============ Open DEMUXERS --- DETECT file type =======================
current_module="demux_open";

demuxer=demux_open(stream,file_format,audio_id,video_id,dvdsub_id,filename);

// HACK to get MOV Reference Files working

if (demuxer && demuxer->type==DEMUXER_TYPE_PLAYLIST)
{ 
  unsigned char* playlist_entry;
  play_tree_t *list = NULL, *entry = NULL;

  current_module="handle_demux_playlist";
  while (ds_get_packet(demuxer->video,&playlist_entry)>0)
  {	 
    char *temp, *bname;
    
    mp_msg(MSGT_CPLAYER,MSGL_V,"Adding file %s to element entry.\n",playlist_entry);

    bname=mp_basename(playlist_entry);
    if ((strlen(bname)>10) && !strncmp(bname,"qt",2) && !strncmp(bname+3,"gateQT",6))
        continue;

    if (!strncmp(bname,mp_basename(filename),strlen(bname))) // ignoring self-reference
        continue;

    entry = play_tree_new();
    
    if (filename && !strcmp(mp_basename(playlist_entry),playlist_entry)) // add reference path of current file
    {
      temp=malloc((strlen(filename)-strlen(mp_basename(filename))+strlen(playlist_entry)+1));
      if (temp)
      {
	strncpy(temp, filename, strlen(filename)-strlen(mp_basename(filename)));
	temp[strlen(filename)-strlen(mp_basename(filename))]='\0';
	strcat(temp, playlist_entry);
	play_tree_add_file(entry,temp);
	mp_msg(MSGT_CPLAYER,MSGL_V,"Resolving reference to %s.\n",temp);
	free(temp);
      }
    }
    else
      play_tree_add_file(entry,playlist_entry);
    
    if(!list)
      list = entry;
    else
      play_tree_append_entry(list,entry);
  }
  free_demuxer(demuxer);
  demuxer = NULL;

  if (list)
  {
    entry = play_tree_new();
    play_tree_set_child(entry,list);
    eof=playtree_add_playlist(entry);
    goto goto_next_file;
  }
}

if(!demuxer) 
{
#if 0
  play_tree_t* entry;
  // Handle playlist
  current_module="handle_playlist";
  switch(stream->type){
  case STREAMTYPE_VCD:
  case STREAMTYPE_DVD:
  case STREAMTYPE_CDDA:
  case STREAMTYPE_VCDBINCUE:
    // don't try to parse raw media as playlist, it's unlikely
    goto goto_next_file;
  }
  mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_FallingBackOnPlaylist,filename);
  stream_reset(stream);
  stream_seek(stream,stream->start_pos);
  entry = parse_playtree(stream,0);
  if(!entry)
    mp_msg(MSGT_DEMUXER,MSGL_ERR,MSGTR_FormatNotRecognized);
  else
    eof=playtree_add_playlist(entry);
#endif
  goto goto_next_file;
}
inited_flags|=INITED_DEMUXER;

if (stream->type != STREAMTYPE_DVD && stream->type != STREAMTYPE_DVDNAV) {
  int i;
  // setup global sub numbering
  global_sub_indices[SUB_SOURCE_DEMUX] = global_sub_size; // the global # of the first demux-specific sub.
  for (i = 0; i < MAX_S_STREAMS; i++)
    if (demuxer->s_streams[i])
      global_sub_size++;
}

current_module="demux_open2";

//file_format=demuxer->file_format;

d_audio=demuxer->audio;
d_video=demuxer->video;
d_dvdsub=demuxer->sub;

// DUMP STREAMS:
if((stream_dump_type)&&(stream_dump_type<4)){
  FILE *f;
  demux_stream_t *ds=NULL;
  current_module="dump";
  // select stream to dump
  switch(stream_dump_type){
  case 1: ds=d_audio;break;
  case 2: ds=d_video;break;
  case 3: ds=d_dvdsub;break;
  }
  if(!ds){        
      mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_DumpSelectedStreamMissing);
      exit_player(MSGTR_Exit_error);
  }
  // disable other streams:
  if(d_audio && d_audio!=ds) {ds_free_packs(d_audio); d_audio->id=-2; }
  if(d_video && d_video!=ds) {ds_free_packs(d_video); d_video->id=-2; }
  if(d_dvdsub && d_dvdsub!=ds) {ds_free_packs(d_dvdsub); d_dvdsub->id=-2; }
  // let's dump it!
  f=fopen(stream_dump_name,"wb");
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
  exit_player_with_rc(MSGTR_Exit_eof, 0);
}

sh_audio=d_audio->sh;
sh_video=d_video->sh;

if(sh_video){

  current_module="video_read_properties";
  if(!video_read_properties(sh_video)) {
    mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_CannotReadVideoProperties);
    sh_video=d_video->sh=NULL;
  } else {
    mp_msg(MSGT_CPLAYER,MSGL_V,"[V] filefmt:%d  fourcc:0x%X  size:%dx%d  fps:%5.2f  ftime:=%6.4f\n",
	   demuxer->file_format,sh_video->format, sh_video->disp_w,sh_video->disp_h,
	   sh_video->fps,sh_video->frametime
	   );

    /* need to set fps here for output encoders to pick it up in their init */
    if(force_fps){
      sh_video->fps=force_fps;
      sh_video->frametime=1.0f/sh_video->fps;
    }
    vo_fps = sh_video->fps;

    if(!sh_video->fps && !force_fps){
      mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_FPSnotspecified);
      sh_video=d_video->sh=NULL;
    }
  }

}

fflush(stdout);

if(!sh_video && !sh_audio){
    mp_msg(MSGT_CPLAYER,MSGL_FATAL, MSGTR_NoStreamFound);
#ifdef HAS_DVBIN_SUPPORT
	if((stream->type == STREAMTYPE_DVB) && stream->priv)
	{
	  dvb_priv_t *priv = (dvb_priv_t*) stream->priv;
	  if(priv->is_on)
	  {
		int dir;
		int v = last_dvb_step;
		if(v > 0)
			dir = DVB_CHANNEL_HIGHER;
		else
			dir = DVB_CHANNEL_LOWER;
			
		if(dvb_step_channel(priv, dir))
			eof = dvbin_reopen = 1;
	  }
	}
#endif	
    goto goto_next_file; // exit_player(MSGTR_Exit_error);
}

/* display clip info */
demux_info_print(demuxer);

//================== Read SUBTITLES (DVD & TEXT) ==========================
if(vo_spudec==NULL && sh_video &&
     (stream->type==STREAMTYPE_DVD || stream->type == STREAMTYPE_DVDNAV || d_dvdsub->id >= 0)){
  init_vo_spudec();
}

// Apply current settings for forced subs
if (vo_spudec!=NULL)
  spudec_set_forced_subs_only(vo_spudec,forced_subs_only);

if(sh_video) {
// after reading video params we should load subtitles because
// we know fps so now we can adjust subtitle time to ~6 seconds AST
// check .sub
  current_module="read_subtitles_file";
  if(sub_name){
    for (i = 0; sub_name[i] != NULL; ++i) 
        add_subtitles (sub_name[i], sh_video->fps, 0); 
  } 
  if(sub_auto) { // auto load sub file ...
    char *psub = get_path( "sub/" );
    char **tmp = sub_filenames((psub ? psub : ""), filename);
    int i = 0;
    free(psub); // release the buffer created by get_path() above
    while (tmp[i]) {
        add_subtitles (tmp[i], sh_video->fps, 0);
        free(tmp[i++]);
    }
    free(tmp);
  }
  if (set_of_sub_size > 0)  {
      // setup global sub numbering
      global_sub_indices[SUB_SOURCE_SUBS] = global_sub_size; // the global # of the first sub.
      global_sub_size += set_of_sub_size;
  }
}

if (global_sub_size) {
  // find the best sub to use
  if (vobsub_id >= 0) {
    // if user asks for a vobsub id, use that first.
    global_sub_pos = global_sub_indices[SUB_SOURCE_VOBSUB] + vobsub_id;
  } else if (dvdsub_id >= 0 && global_sub_indices[SUB_SOURCE_DEMUX] >= 0) {
    // if user asks for a dvd sub id, use that next.
    global_sub_pos = global_sub_indices[SUB_SOURCE_DEMUX] + dvdsub_id;
  } else if (global_sub_indices[SUB_SOURCE_SUBS] >= 0) {
    // if there are text subs to use, use those.  (autosubs come last here)
    global_sub_pos = global_sub_indices[SUB_SOURCE_SUBS];
/*
  } else if (global_sub_indices[SUB_SOURCE_DEMUX] >= 0) {
    // if nothing else works, get subs from the demuxer.
    global_sub_pos = global_sub_indices[SUB_SOURCE_DEMUX];
*/
  } else {
    // nothing worth doing automatically.
    global_sub_pos = -1;
  }
  // rather than duplicate code, use the SUB_SELECT handler to init the right one.
  global_sub_pos--;
  mp_property_do("sub",M_PROPERTY_STEP_UP,NULL);
  if(subdata)
    switch (stream_dump_type) {
        case 3: list_sub_file(subdata); break;
        case 4: dump_mpsub(subdata, sh_video->fps); break;
        case 6: dump_srt(subdata, sh_video->fps); break;
        case 7: dump_microdvd(subdata, sh_video->fps); break;
        case 8: dump_jacosub(subdata, sh_video->fps); break;
        case 9: dump_sami(subdata, sh_video->fps); break;
    }
}

  mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_FILENAME=%s\n", filename);
  mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_DEMUXER=%s\n", demuxer->desc->name);
  if (sh_video) {
    /* Assume FOURCC if all bytes >= 0x20 (' ') */
    if (sh_video->format >= 0x20202020)
	mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_VIDEO_FORMAT=%.4s\n", (char *)&sh_video->format);
    else
	mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_VIDEO_FORMAT=0x%08X\n", sh_video->format);
    mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_VIDEO_BITRATE=%d\n", sh_video->i_bps*8);
    mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_VIDEO_WIDTH=%d\n", sh_video->disp_w);
    mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_VIDEO_HEIGHT=%d\n", sh_video->disp_h);
    mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_VIDEO_FPS=%5.3f\n", sh_video->fps);
    mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_VIDEO_ASPECT=%1.4f\n", sh_video->aspect);
  }
  if (sh_audio) {
    /* Assume FOURCC if all bytes >= 0x20 (' ') */
    if (sh_audio->format >= 0x20202020)
      mp_msg(MSGT_IDENTIFY,MSGL_INFO, "ID_AUDIO_FORMAT=%.4s\n", (char *)&sh_audio->format);
    else
      mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_AUDIO_FORMAT=%d\n", sh_audio->format);
    mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_AUDIO_BITRATE=%d\n", sh_audio->i_bps*8);
    mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_AUDIO_RATE=%d\n", sh_audio->samplerate);
    mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_AUDIO_NCH=%d\n", sh_audio->channels);
  }
  mp_msg(MSGT_IDENTIFY,MSGL_INFO,"ID_LENGTH=%.2lf\n", demuxer_get_time_length(demuxer));

if(!sh_video) goto main; // audio-only

if(!reinit_video_chain()) {
  if(!sh_video){
    if(!sh_audio) goto goto_next_file;
    goto main; // exit_player(MSGTR_Exit_error);
  }
}

   if(vo_flags & 0x08 && vo_spudec)
      spudec_set_hw_spu(vo_spudec,video_out);

#ifdef HAVE_FREETYPE
   force_load_font = 1;
#endif

//================== MAIN: ==========================
main:
current_module="main";

    if(playing_msg) {
        char* msg = m_properties_expand_string(mp_properties,playing_msg);
        mp_msg(MSGT_CPLAYER,MSGL_INFO,"%s",msg);
        free(msg);
    }
        

// Disable the term OSD in verbose mode
if(verbose) term_osd = 0;
fflush(stdout);

{
//int frame_corr_num=0;   //
//float v_frame=0;    // Video
float time_frame=0; // Timer
//float num_frames=0;      // number of frames played
int grab_frames=0;

int frame_time_remaining=0; // flag
int blit_frame=0;
int was_paused=0;

// Make sure old OSD does not stay around,
// e.g. with -fixed-vo and same-resolution files
clear_osd_msgs();
update_osd_msg();

//================ SETUP AUDIO ==========================

if(sh_audio){
  reinit_audio_chain();
  if (sh_audio && sh_audio->codec)
    mp_msg(MSGT_IDENTIFY,MSGL_INFO, "ID_AUDIO_CODEC=%s\n", sh_audio->codec->name);
}

current_module="av_init";

if(sh_video){
  sh_video->timer=0;
  if (! ignore_start)
    audio_delay += sh_video->stream_delay;
}
if(sh_audio){
  if (! ignore_start)
    audio_delay -= sh_audio->stream_delay;
  sh_audio->delay=-audio_delay;
}

if(!sh_audio){
  mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_NoSound);
  mp_msg(MSGT_CPLAYER,MSGL_V,"Freeing %d unused audio chunks.\n",d_audio->packs);
  ds_free_packs(d_audio); // free buffered chunks
  //d_audio->id=-2;         // do not read audio chunks
  //uninit_player(INITED_AO); // close device
}
if(!sh_video){
   mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_Video_NoVideo);
   mp_msg(MSGT_CPLAYER,MSGL_V,"Freeing %d unused video chunks.\n",d_video->packs);
   ds_free_packs(d_video);
   d_video->id=-2;
   //if(!fixed_vo) uninit_player(INITED_VO);
}

if (!sh_video && !sh_audio)
    goto goto_next_file;

//if(demuxer->file_format!=DEMUXER_TYPE_AVI) pts_from_bps=0; // it must be 0 for mpeg/asf!
if(force_fps && sh_video){
  vo_fps = sh_video->fps=force_fps;
  sh_video->frametime=1.0f/sh_video->fps;
  mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_FPSforced,sh_video->fps,sh_video->frametime);
}

#ifdef HAVE_NEW_GUI
if ( use_gui ) {
    if ( sh_audio ) guiIntfStruct.AudioType=sh_audio->channels; else guiIntfStruct.AudioType=0;
    if ( !sh_video && sh_audio ) guiGetEvent( guiSetAudioOnly,(char *)1 ); else guiGetEvent( guiSetAudioOnly,(char *)0 );
    guiGetEvent( guiSetFileFormat,(char *)demuxer->file_format );
    if ( guiGetEvent( guiSetValues,(char *)sh_video ) ) goto goto_next_file;
    guiGetEvent( guiSetDemuxer,(char *)demuxer );
}
#endif

//==================== START PLAYING =======================

if(loop_times>1) loop_times--; else
if(loop_times==1) loop_times = -1;

mp_msg(MSGT_CPLAYER,MSGL_INFO,MSGTR_StartPlaying);fflush(stdout);

total_time_usage_start=GetTimer();
audio_time_usage=0; video_time_usage=0; vout_time_usage=0;
total_frame_cnt=0; drop_frame_cnt=0; // fix for multifile fps benchmark
play_n_frames=play_n_frames_mf;

if(play_n_frames==0){
  eof=PT_NEXT_ENTRY; goto goto_next_file;
}

while(!eof){
    float aq_sleep_time=0;
if(!sh_audio && d_audio->sh) {
  sh_audio = d_audio->sh;
  sh_audio->ds = d_audio;
  reinit_audio_chain();
}

/*========================== PLAY AUDIO ============================*/

if (sh_audio)
    if (!fill_audio_out_buffers())
	// at eof, all audio at least written to ao
	if (!sh_video)
	    eof = PT_NEXT_ENTRY;


if(!sh_video) {
  // handle audio-only case:
  double a_pos=0;
  if(!quiet || end_at.type == END_AT_TIME )
    a_pos = playing_audio_pts(sh_audio, d_audio, audio_out);

  if(!quiet)
    print_status(a_pos, 0, 0);

  if(end_at.type == END_AT_TIME && end_at.pos < a_pos)
    eof = PT_NEXT_ENTRY;
  update_osd_msg();

} else {

/*========================== PLAY VIDEO ============================*/

  vo_pts=sh_video->timer*90000.0;
  vo_fps=sh_video->fps;

  if (!frame_time_remaining) {
      double frame_time = update_video(&blit_frame);
      mp_dbg(MSGT_AVSYNC,MSGL_DBG2,"*** ftime=%5.3f ***\n",frame_time);
      if (sh_video->vf_inited < 0) {
	  mp_msg(MSGT_CPLAYER,MSGL_FATAL, MSGTR_NotInitializeVOPorVO);
	  eof = 1; goto goto_next_file;
      }
      if (frame_time < 0)
	  eof = 1;
      else
	  time_frame += frame_time / playback_speed;  // for nosound
  }

// ==========================================================================
    
//    current_module="draw_osd";
//    if(vo_config_count) video_out->draw_osd();

#ifdef HAVE_NEW_GUI
    if(use_gui) guiEventHandling();
#endif

    current_module="vo_check_events";
    if (vo_config_count) video_out->check_events();

#ifdef HAVE_X11
    if (stop_xscreensaver) {
	current_module = "stop_xscreensaver";
	xscreensaver_heartbeat();
    }
#endif

    frame_time_remaining = sleep_until_update(&time_frame, &aq_sleep_time);

//====================== FLIP PAGE (VIDEO BLT): =========================

        current_module="flip_page";
        if (!frame_time_remaining && blit_frame) {
	   unsigned int t2=GetTimer();

	   if(vo_config_count) video_out->flip_page();

	   vout_time_usage += (GetTimer() - t2) * 0.000001;
        }
//====================== A-V TIMESTAMP CORRECTION: =========================

  adjust_sync_and_print_status(frame_time_remaining, time_frame);

//============================ Auto QUALITY ============================

/*Output quality adjustments:*/
if(auto_quality>0){
  current_module="autoq";
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
}

 if (play_n_frames >= 0 && !frame_time_remaining && blit_frame) {
     --play_n_frames;
     if (play_n_frames <= 0) eof = PT_NEXT_ENTRY;
 }


// FIXME: add size based support for -endpos
 if (end_at.type == END_AT_TIME &&
         !frame_time_remaining && end_at.pos <= sh_video->pts)
     eof = PT_NEXT_ENTRY;

} // end if(sh_video)

#ifdef USE_DVDNAV
 if (stream->type == STREAMTYPE_DVDNAV) {
   nav_highlight_t hl;
   mp_dvdnav_get_highlight (stream, &hl);
   osd_set_nav_box (hl.sx, hl.sy, hl.ex, hl.ey);
   vo_osd_changed (OSDTYPE_DVDNAV);
 }
#endif
 
//============================ Handle PAUSE ===============================

  current_module="pause";

  if(osd_visible){
      // 36000000 means max timed visibility is 1 hour into the future, if
      // the difference is greater assume it's wrapped around from below 0
    if (osd_visible - GetTimerMS() > 36000000) {
       osd_visible = 0;
       vo_osd_progbar_type=-1; // disable
       vo_osd_changed(OSDTYPE_PROGBAR);
       if (osd_function != OSD_PAUSE)
	   osd_function = OSD_PLAY;
    }
  }

  if (osd_function == OSD_PAUSE) {
      pause_loop();
      was_paused = 1;
  }

// handle -sstep
if(step_sec>0) {
	osd_function=OSD_FFW;
	rel_seek_secs+=step_sec;
}

//================= EDL =========================================

 if( next_edl_record ) { // Are we (still?) doing EDL?
  if ( !sh_video ) {
    mp_msg( MSGT_CPLAYER, MSGL_ERR, MSGTR_EdlNOsh_video );
    free_edl(edl_records);
    next_edl_record = NULL; 
    edl_records = NULL;
  } else {
   if( sh_video->pts >= next_edl_record->start_sec ) {
     if( next_edl_record->action == EDL_SKIP ) {
       osd_function = OSD_FFW;
       abs_seek_pos = 0;
       rel_seek_secs = next_edl_record->length_sec;
       mp_msg(MSGT_CPLAYER, MSGL_DBG4, "EDL_SKIP: start [%f], stop [%f], length [%f]\n", next_edl_record->start_sec, next_edl_record->stop_sec, next_edl_record->length_sec );
       edl_decision = 1;
     } else if( next_edl_record->action == EDL_MUTE ) {
       edl_muted = !edl_muted;
       if ((user_muted | edl_muted) != mixer.muted) mixer_mute(&mixer);
       mp_msg(MSGT_CPLAYER, MSGL_DBG4, "EDL_MUTE: [%f]\n", next_edl_record->start_sec );
     }
     next_edl_record=next_edl_record->next;
   }
  }
 }

//================= Keyboard events, SEEKing ====================

  current_module="key_events";

{
  mp_cmd_t* cmd;
  int brk_cmd = 0;
  while( !brk_cmd && (cmd = mp_input_get_cmd(0,0,0)) != NULL) {
   if(!set_property_command(cmd))
    switch(cmd->id) {
    case MP_CMD_SEEK : {
      float v;
      int abs;
      if(sh_video)
        osd_show_percentage = sh_video->fps;
      v = cmd->args[0].v.f;
      abs = (cmd->nargs > 1) ? cmd->args[1].v.i : 0;
      if(abs==2) { /* Absolute seek to a specific timestamp in seconds */
        abs_seek_pos = 1;
	if(sh_video)
	  osd_function= (v > sh_video->pts) ? OSD_FFW : OSD_REW;
	rel_seek_secs = v;
      }
      else if(abs) { /* Absolute seek by percentage */
	abs_seek_pos = 3;
	if(sh_video)
	  osd_function= OSD_FFW;   // Direction isn't set correctly
	rel_seek_secs = v/100.0;
      }
      else {
	rel_seek_secs+= v;
	osd_function= (v > 0) ? OSD_FFW : OSD_REW;
      }
      brk_cmd = 1;
    } break;
    case MP_CMD_SET_PROPERTY: {
        m_option_t* prop = mp_property_find(cmd->args[0].v.s);
        if(!prop) mp_msg(MSGT_CPLAYER,MSGL_WARN,"Unknown property: '%s'\n",cmd->args[0].v.s);
        else if(m_property_parse(prop,cmd->args[1].v.s) <= 0)
            mp_msg(MSGT_CPLAYER,MSGL_WARN,"Failed to set property '%s' to '%s'.\n",
                   cmd->args[0].v.s,cmd->args[1].v.s);
        
    } break;
    case MP_CMD_STEP_PROPERTY: {
        m_option_t* prop = mp_property_find(cmd->args[0].v.s);
        float arg = cmd->args[1].v.f;
        if(!prop) mp_msg(MSGT_CPLAYER,MSGL_WARN, "Unknown property: '%s'\n",cmd->args[0].v.s);
        else if(m_property_do(prop,M_PROPERTY_STEP_UP, arg ? &arg : NULL) <= 0)
            mp_msg(MSGT_CPLAYER,MSGL_WARN, "Failed to increment property '%s' by %f.\n",cmd->args[0].v.s, arg);
    } break;
    case MP_CMD_GET_PROPERTY: {
        m_option_t* prop;
        void* val;
        prop = mp_property_find(cmd->args[0].v.s);
        if(!prop) {
            mp_msg(MSGT_CPLAYER,MSGL_WARN,"Unknown property: '%s'\n",cmd->args[0].v.s);
            break;
        }
        /* Use m_option_print directly to get easily parseable values. */
        val = calloc(1,prop->type->size);
        if(m_property_do(prop,M_PROPERTY_GET,val) <= 0) {
            mp_msg(MSGT_CPLAYER,MSGL_WARN,"Failed to get value of property '%s'.\n",
                   cmd->args[0].v.s);
            break;
        }
        tmp = m_option_print(prop,val);
        if(!tmp || tmp == (char*)-1) {
            mp_msg(MSGT_CPLAYER,MSGL_WARN,"Failed to print value of property '%s'.\n",
                   cmd->args[0].v.s);
            break;
        }
        mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_%s=%s\n",cmd->args[0].v.s,tmp);
        free(tmp);
    } break;
    case MP_CMD_EDL_MARK:
      if( edl_fd ) {
	float v = sh_video ? sh_video->pts :
	    playing_audio_pts(sh_audio, d_audio, audio_out);

	if(begin_skip == MP_NOPTS_VALUE)
	{
	  begin_skip = v; 
	  mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_EdloutStartSkip);
	}else{
	  if(begin_skip > v)
	  {
	    mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdloutBadStop);
	  }else{
	    fprintf(edl_fd, "%f %f %d\n", begin_skip, v, 0);
	    mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_EdloutEndSkip);
	  }
	  begin_skip = MP_NOPTS_VALUE;
	}
      }
      break;
    case MP_CMD_SWITCH_RATIO : {
      if (cmd->nargs == 0 || cmd->args[0].v.f == -1)
	movie_aspect = (float) sh_video->disp_w / sh_video->disp_h;
      else
	movie_aspect = cmd->args[0].v.f;
      mpcodecs_config_vo (sh_video, sh_video->disp_w, sh_video->disp_h, 0);
    } break;
    case MP_CMD_SPEED_INCR : {
      float v = cmd->args[0].v.f;
      playback_speed += v;
      build_afilter_chain(sh_audio, &ao_data);
      set_osd_msg(OSD_MSG_SPEED,1,osd_duration,MSGTR_OSDSpeed, playback_speed);
    } break;
    case MP_CMD_SPEED_MULT : {
      float v = cmd->args[0].v.f;
      playback_speed *= v;
      build_afilter_chain(sh_audio, &ao_data);
      set_osd_msg(OSD_MSG_SPEED,1,osd_duration,MSGTR_OSDSpeed, playback_speed);
    } break;
    case MP_CMD_SPEED_SET : {
      float v = cmd->args[0].v.f;
      playback_speed = v;
      build_afilter_chain(sh_audio, &ao_data);
      set_osd_msg(OSD_MSG_SPEED,1,osd_duration,MSGTR_OSDSpeed, playback_speed);
    } break;
    case MP_CMD_FRAME_STEP :
    case MP_CMD_PAUSE : {
      cmd->pausing = 1;
      brk_cmd = 1;
    } break;
    case MP_CMD_FILE_FILTER : {
      file_filter = cmd->args[0].v.i;
      break;
    }
    case MP_CMD_QUIT : {
      exit_player_with_rc(MSGTR_Exit_quit, (cmd->nargs > 0)? cmd->args[0].v.i : 0);
    }
    case MP_CMD_GRAB_FRAMES : {
      grab_frames=2;
    } break;
    case MP_CMD_PLAY_TREE_STEP : {
      int n = cmd->args[0].v.i == 0 ? 1 : cmd->args[0].v.i;
      int force = cmd->args[1].v.i;

#ifdef HAVE_NEW_GUI
     if (use_gui) {
	int i=0;
        if (n>0)
	  for (i=0;i<n;i++)
	    mplNext();
        else
	  for (i=0;i<-1*n;i++)
	    mplPrev();
     } else
#endif
     {
      if(!force && playtree_iter) {
	play_tree_iter_t* i = play_tree_iter_new_copy(playtree_iter);
	
	if(play_tree_iter_step(i,n,0) == PLAY_TREE_ITER_ENTRY)
	  eof = (n > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
	play_tree_iter_free(i);
      } else
	eof = (n > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
      if(eof)
	play_tree_step = n;
      brk_cmd = 1;
     }
    } break;
    case MP_CMD_PLAY_TREE_UP_STEP : {
      int n = cmd->args[0].v.i > 0 ? 1 : -1;
      int force = cmd->args[1].v.i;

      if(!force && playtree_iter) {
	play_tree_iter_t* i = play_tree_iter_new_copy(playtree_iter);
	if(play_tree_iter_up_step(i,n,0) == PLAY_TREE_ITER_ENTRY)
	  eof = (n > 0) ? PT_UP_NEXT : PT_UP_PREV;
	play_tree_iter_free(i);
      } else
	eof = (n > 0) ? PT_UP_NEXT : PT_UP_PREV;
      brk_cmd = 1;
    } break;
    case MP_CMD_PLAY_ALT_SRC_STEP : {
      if(playtree_iter && playtree_iter->num_files > 1) {
	int v = cmd->args[0].v.i;
	if(v > 0 && playtree_iter->file < playtree_iter->num_files)
	  eof = PT_NEXT_SRC;
	else if(v < 0 && playtree_iter->file > 1)
	  eof = PT_PREV_SRC;
      }
      brk_cmd = 1;
    } break;
    case MP_CMD_SUB_STEP : {
    if (sh_video) {
      int movement = cmd->args[0].v.i;
      step_sub(subdata, sh_video->pts, movement);
#ifdef USE_ASS
      if (ass_track)
        sub_delay += ass_step_sub(ass_track, (sh_video->pts + sub_delay) * 1000 + .5, movement) / 1000.;
#endif
      set_osd_msg(OSD_MSG_SUB_DELAY,1,osd_duration,
                  MSGTR_OSDSubDelay, ROUND(sub_delay*1000));
    }
    } break;
    case MP_CMD_SUB_LOG : {
	log_sub();
    } break;
    case MP_CMD_OSD :  {
	int v = cmd->args[0].v.i;
	int max = (term_osd && !sh_video) ? MAX_TERM_OSD_LEVEL : MAX_OSD_LEVEL;
	if(osd_level > max) osd_level = max;
	if(v < 0)
	  osd_level=(osd_level+1)%(max+1);
	else
	  osd_level= v > max ? max : v;
	/* Show OSD state when disabled, but not when an explicit
	   argument is given to the OSD command, i.e. in slave mode. */
	if (v == -1 && osd_level <= 1)
	  set_osd_msg(OSD_MSG_OSD_STATUS,0,osd_duration,
                      MSGTR_OSDosd, osd_level ? MSGTR_OSDenabled : MSGTR_OSDdisabled);
	else
	  rm_osd_msg(OSD_MSG_OSD_STATUS);
    } break;
    case MP_CMD_OSD_SHOW_TEXT :  {
      set_osd_msg(OSD_MSG_TEXT,cmd->args[2].v.i,
                  (cmd->args[1].v.i < 0 ? osd_duration : cmd->args[1].v.i),
                  "%-.63s",cmd->args[0].v.s);
    } break;
    case MP_CMD_OSD_SHOW_PROPERTY_TEXT : {
      char* txt = m_properties_expand_string(mp_properties,cmd->args[0].v.s);
      /* if no argument supplied take default osd_duration, else <arg> ms. */
      if(txt) {
        set_osd_msg(OSD_MSG_TEXT,cmd->args[2].v.i,
                    (cmd->args[1].v.i < 0 ? osd_duration : cmd->args[1].v.i),
                    "%-.63s",txt);
        free(txt);
      }
    } break;
    case MP_CMD_LOADFILE : {
      play_tree_t* e = play_tree_new();
      play_tree_add_file(e,cmd->args[0].v.s);

      if (cmd->args[1].v.i) // append
        play_tree_append_entry(playtree, e);
      else {
      // Go back to the starting point.
      while(play_tree_iter_up_step(playtree_iter,0,1) != PLAY_TREE_ITER_END)
	/* NOP */;
      play_tree_free_list(playtree->child,1);
      play_tree_set_child(playtree,e);
      play_tree_iter_step(playtree_iter,0,0);
      eof = PT_NEXT_SRC;
      }
      brk_cmd = 1;
    } break;
    case MP_CMD_LOADLIST : {
      play_tree_t* e = parse_playlist_file(cmd->args[0].v.s);
      if(!e)
	mp_msg(MSGT_CPLAYER,MSGL_ERR,MSGTR_PlaylistLoadUnable,cmd->args[0].v.s);
      else {
	if (cmd->args[1].v.i) // append
	  play_tree_append_entry(playtree, e);
	else {
	// Go back to the starting point.
	while(play_tree_iter_up_step(playtree_iter,0,1) != PLAY_TREE_ITER_END)
	  /* NOP */;
	play_tree_free_list(playtree->child,1);
	play_tree_set_child(playtree,e);
	play_tree_iter_step(playtree_iter,0,0);
	eof = PT_NEXT_SRC;	
	}
      }
      brk_cmd = 1;
    } break;
#ifdef USE_RADIO
    case MP_CMD_RADIO_STEP_CHANNEL :  {
      if (demuxer->stream->type==STREAMTYPE_RADIO) {
          int v = cmd->args[0].v.i;
          if(v > 0)
              radio_step_channel(demuxer->stream, RADIO_CHANNEL_HIGHER);
          else
              radio_step_channel(demuxer->stream, RADIO_CHANNEL_LOWER);
          if (radio_get_channel_name(demuxer->stream)) {
              set_osd_msg(OSD_MSG_RADIO_CHANNEL,1,osd_duration,
                      MSGTR_OSDChannel, radio_get_channel_name(demuxer->stream));
          }
      }
    } break;
    case MP_CMD_RADIO_SET_CHANNEL :  {
        if (demuxer->stream->type== STREAMTYPE_RADIO) {
            radio_set_channel(demuxer->stream, cmd->args[0].v.s);
            if (radio_get_channel_name(demuxer->stream)) {
                set_osd_msg(OSD_MSG_RADIO_CHANNEL,1,osd_duration,
                      MSGTR_OSDChannel, radio_get_channel_name(demuxer->stream));
            }
        }
    } break;
    case MP_CMD_RADIO_SET_FREQ :  {
      if (demuxer->stream->type==  STREAMTYPE_RADIO)
        radio_set_freq(demuxer->stream, cmd->args[0].v.f);
    } break;
    case MP_CMD_RADIO_STEP_FREQ :
      if (demuxer->stream->type == STREAMTYPE_RADIO){
        radio_step_freq(demuxer->stream, cmd->args[0].v.f);
      }
    break;
#endif
#ifdef USE_TV
    case MP_CMD_TV_SET_FREQ :  {
      if (file_format == DEMUXER_TYPE_TV)
        tv_set_freq((tvi_handle_t*)(demuxer->priv), cmd->args[0].v.f * 16.0);
    } break;
    case MP_CMD_TV_SET_NORM :  {
      if (file_format == DEMUXER_TYPE_TV)
        tv_set_norm((tvi_handle_t*)(demuxer->priv), cmd->args[0].v.s);
    } break;
    case MP_CMD_TV_STEP_CHANNEL :  {
      if (file_format == DEMUXER_TYPE_TV) {
	int v = cmd->args[0].v.i;
	if(v > 0){
	  tv_step_channel((tvi_handle_t*)(demuxer->priv), TV_CHANNEL_HIGHER);
	} else {
	  tv_step_channel((tvi_handle_t*)(demuxer->priv), TV_CHANNEL_LOWER);
	}
        if (tv_channel_list) {
          set_osd_msg(OSD_MSG_TV_CHANNEL,1,osd_duration,
                      MSGTR_OSDChannel, tv_channel_current->name);
          //vo_osd_changed(OSDTYPE_SUBTITLE);
        }
      }
    } 
#ifdef HAS_DVBIN_SUPPORT
	if((stream->type == STREAMTYPE_DVB) && stream->priv)
	{
	  dvb_priv_t *priv = (dvb_priv_t*) stream->priv;
	  if(priv->is_on)
	  {
		int dir;
		int v = cmd->args[0].v.i;
	    
		last_dvb_step = v;	
		if(v > 0)
			dir = DVB_CHANNEL_HIGHER;
		else
			dir = DVB_CHANNEL_LOWER;
			
			
		if(dvb_step_channel(priv, dir))
			eof = dvbin_reopen = 1;
	  }
	}
#endif /* HAS_DVBIN_SUPPORT */
    break;
    case MP_CMD_TV_SET_CHANNEL :  {
      if (file_format == DEMUXER_TYPE_TV) {
	tv_set_channel((tvi_handle_t*)(demuxer->priv), cmd->args[0].v.s);
	if (tv_channel_list) {
		set_osd_msg(OSD_MSG_TV_CHANNEL,1,osd_duration,
		            MSGTR_OSDChannel, tv_channel_current->name);
		//vo_osd_changed(OSDTYPE_SUBTITLE);
	}
      }
    } break;
#ifdef HAS_DVBIN_SUPPORT	
  case MP_CMD_DVB_SET_CHANNEL:  
  {
	if((stream->type == STREAMTYPE_DVB) && stream->priv)
	{
	  dvb_priv_t *priv = (dvb_priv_t*) stream->priv;
	  if(priv->is_on)
	  {
		if(priv->list->current <= cmd->args[0].v.i)
		    last_dvb_step = 1;
		else
		    last_dvb_step = -1;

  		if(dvb_set_channel(priv, cmd->args[1].v.i, cmd->args[0].v.i))
		  eof = dvbin_reopen = 1;
	  }
	}
  }
  break;
#endif /* HAS_DVBIN_SUPPORT	*/
    case MP_CMD_TV_LAST_CHANNEL :  {
      if (file_format == DEMUXER_TYPE_TV) {
	tv_last_channel((tvi_handle_t*)(demuxer->priv));
	if (tv_channel_list) {
		set_osd_msg(OSD_MSG_TV_CHANNEL,1,osd_duration,
                            MSGTR_OSDChannel, tv_channel_current->name);
		//vo_osd_changed(OSDTYPE_SUBTITLE);
	}
      }
    } break;
    case MP_CMD_TV_STEP_NORM :  {
      if (file_format == DEMUXER_TYPE_TV)
	tv_step_norm((tvi_handle_t*)(demuxer->priv));
    } break;
    case MP_CMD_TV_STEP_CHANNEL_LIST :  {
      if (file_format == DEMUXER_TYPE_TV)
	tv_step_chanlist((tvi_handle_t*)(demuxer->priv));
    } break;
#endif /* USE_TV */
    case MP_CMD_SUB_LOAD:
    {
      if (sh_video) {
        int n = set_of_sub_size;
        add_subtitles(cmd->args[0].v.s, sh_video->fps, 0);
        if (n != set_of_sub_size) {
          if (global_sub_indices[SUB_SOURCE_SUBS] < 0)
            global_sub_indices[SUB_SOURCE_SUBS] = global_sub_size;
          ++global_sub_size;
        }
      }
    } break;
    case MP_CMD_SUB_REMOVE:
    {
      if (sh_video) {
        int v = cmd->args[0].v.i;
        sub_data *subd;
        if (v < 0) {
          for (v = 0; v < set_of_sub_size; ++v) {
            subd = set_of_subtitles[v];
            mp_msg(MSGT_CPLAYER, MSGL_STATUS, MSGTR_RemovedSubtitleFile, v + 1, subd->filename);
            sub_free(subd);
            set_of_subtitles[v] = NULL;
          }
          global_sub_indices[SUB_SOURCE_SUBS] = -1;
          global_sub_size -= set_of_sub_size;
          set_of_sub_size = 0;
          if (set_of_sub_pos >= 0) {
            global_sub_pos = -2;
            vo_sub_last = vo_sub = NULL;
            vo_osd_changed(OSDTYPE_SUBTITLE);
            vo_update_osd(sh_video->disp_w, sh_video->disp_h);
            mp_input_queue_cmd(mp_input_parse_cmd("sub_select"));
          }
        }
        else if (v < set_of_sub_size) {
          subd = set_of_subtitles[v];
          mp_msg(MSGT_CPLAYER, MSGL_STATUS, MSGTR_RemovedSubtitleFile, v + 1, subd->filename);
          sub_free(subd);
          if (set_of_sub_pos == v) {
            global_sub_pos = -2;
            vo_sub_last = vo_sub = NULL;
            vo_osd_changed(OSDTYPE_SUBTITLE);
            vo_update_osd(sh_video->disp_w, sh_video->disp_h);
            mp_input_queue_cmd(mp_input_parse_cmd("sub_select"));
          }
          else if (set_of_sub_pos > v) {
            --set_of_sub_pos;
            --global_sub_pos;
          }
          while (++v < set_of_sub_size)
            set_of_subtitles[v - 1] = set_of_subtitles[v];
          --set_of_sub_size;
          --global_sub_size;
          if (set_of_sub_size <= 0)
            global_sub_indices[SUB_SOURCE_SUBS] = -1;
          set_of_subtitles[set_of_sub_size] = NULL;
        }
      }
    } break;
    case MP_CMD_GET_SUB_VISIBILITY:
	{
	if (sh_video) {
		mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_SUB_VISIBILITY=%d\n", sub_visibility);
	}
	} break;
    case MP_CMD_SCREENSHOT :
      if(vo_config_count){
	mp_msg(MSGT_CPLAYER,MSGL_INFO,"sending VFCTRL_SCREENSHOT!\n");
	if(CONTROL_OK!=((vf_instance_t *)sh_video->vfilter)->control(sh_video->vfilter, VFCTRL_SCREENSHOT, &cmd->args[0].v.i))
	video_out->control(VOCTRL_SCREENSHOT, NULL);
      }
      break;
    case MP_CMD_VF_CHANGE_RECTANGLE:
	set_rectangle(sh_video, cmd->args[0].v.i, cmd->args[1].v.i);
	break;
	
    case MP_CMD_GET_TIME_LENGTH : {
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_LENGTH=%.2lf\n", demuxer_get_time_length(demuxer));
    } break;

    case MP_CMD_GET_FILENAME : {
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_FILENAME='%s'\n", get_metadata (META_NAME));
    } break;

    case MP_CMD_GET_VIDEO_CODEC : {
        char *inf = get_metadata (META_VIDEO_CODEC);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_VIDEO_CODEC='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_VIDEO_BITRATE : {
        char *inf = get_metadata (META_VIDEO_BITRATE);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_VIDEO_BITRATE='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_VIDEO_RESOLUTION : {
        char *inf = get_metadata (META_VIDEO_RESOLUTION);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_VIDEO_RESOLUTION='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_AUDIO_CODEC : {
        char *inf = get_metadata (META_AUDIO_CODEC);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_AUDIO_CODEC='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_AUDIO_BITRATE : {
        char *inf = get_metadata (META_AUDIO_BITRATE);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_AUDIO_BITRATE='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_AUDIO_SAMPLES : {
        char *inf = get_metadata (META_AUDIO_SAMPLES);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_AUDIO_SAMPLES='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_META_TITLE : {
        char *inf = get_metadata (META_INFO_TITLE);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_META_TITLE='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_META_ARTIST : {
        char *inf = get_metadata (META_INFO_ARTIST);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_META_ARTIST='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_META_ALBUM : {
        char *inf = get_metadata (META_INFO_ALBUM);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_META_ALBUM='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_META_YEAR : {
        char *inf = get_metadata (META_INFO_YEAR);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_META_YEAR='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_META_COMMENT : {
        char *inf = get_metadata (META_INFO_COMMENT);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_META_COMMENT='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_META_TRACK : {
        char *inf = get_metadata (META_INFO_TRACK);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_META_TRACK='%s'\n", inf);
        free (inf);
    } break;

    case MP_CMD_GET_META_GENRE : {
        char *inf = get_metadata (META_INFO_GENRE);
        if (!inf) inf = strdup ("");
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_META_GENRE='%s'\n", inf);
        free (inf);
    } break;

	case MP_CMD_GET_VO_FULLSCREEN : {
	if(video_out && vo_config_count)
		mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_VO_FULLSCREEN=%d\n", vo_fs);
	} break;
    
    case MP_CMD_GET_PERCENT_POS : {
	mp_msg(MSGT_GLOBAL,MSGL_INFO, "ANS_PERCENT_POSITION=%d\n", demuxer_get_percent_pos(demuxer));
    } break;
    case MP_CMD_GET_TIME_POS : {
      float pos = 0;
      if (sh_video)
        pos = sh_video->pts;
      else
      if (sh_audio && audio_out)
        pos = playing_audio_pts(sh_audio, d_audio, audio_out);
      mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_TIME_POSITION=%.1f\n", pos);
    } break;
    case MP_CMD_RUN : {
#ifndef __MINGW32__
        if(!fork()) {
          execl("/bin/sh","sh","-c",cmd->args[0].v.s,NULL);
          exit(0);
        }
#endif
    } break;
    case MP_CMD_KEYDOWN_EVENTS : {
		mplayer_put_key(cmd->args[0].v.i);
    } break;
    case MP_CMD_SEEK_CHAPTER : {
        int seek = cmd->args[0].v.i;
        int abs = (cmd->nargs > 1) ? cmd->args[1].v.i : 0;
        int chap;
        float next_pts = 0;
        int num_chapters;
        char *chapter_name;

        rel_seek_secs = 0;
        abs_seek_pos = 0;
        chap = demuxer_seek_chapter(demuxer, seek, abs, &next_pts, &num_chapters, &chapter_name);
        if(chap != -1) {
            if(next_pts > -1.0) {
                abs_seek_pos = 1;
                rel_seek_secs = next_pts;
            }
            if(chapter_name) {
                set_osd_msg(OSD_MSG_TEXT, 1, osd_duration, MSGTR_OSDChapter,
                chap+1, chapter_name);
                free(chapter_name);
            }
        } else {
            if (seek > 0)
                rel_seek_secs = 1000000000.;
            else
                set_osd_msg(OSD_MSG_TEXT, 1, osd_duration, MSGTR_OSDChapter, 0, MSGTR_Unknown);
        }
        break;
    } break;
    case MP_CMD_SET_MOUSE_POS: {
        int button = -1, pointer_x, pointer_y;
        double dx, dy;
        pointer_x = cmd->args[0].v.i;
        pointer_y = cmd->args[1].v.i;
        rescale_input_coordinates(pointer_x, pointer_y, &dx, &dy);
#ifdef USE_DVDNAV
        if(stream->type == STREAMTYPE_DVDNAV && dx > 0.0 && dy > 0.0) {
            pointer_x = (int) (dx * (double) sh_video->disp_w);
            pointer_y = (int) (dy * (double) sh_video->disp_h);
            mp_dvdnav_update_mouse_pos(stream, pointer_x, pointer_y, &button);
            if(button>0) set_osd_msg(OSD_MSG_TEXT, 1, osd_duration, "Selected button number %d", button);
        }
#endif
        break;
    }
#ifdef USE_DVDNAV
    case MP_CMD_DVDNAV: {
      int button = -1;
      if(stream->type != STREAMTYPE_DVDNAV) break;

      if(mp_dvdnav_handle_input(stream, cmd->args[0].v.i, &button)) {
          uninit_player(INITED_ALL-(INITED_STREAM|INITED_INPUT|(fixed_vo ? INITED_VO : 0)));
          goto goto_enable_cache;
      } else if(button>0) set_osd_msg(OSD_MSG_TEXT, 1, osd_duration, "Selected button number %d", button);
      break;
    }
#endif
    default : {
#ifdef HAVE_NEW_GUI
      if ( ( use_gui )&&( cmd->id > MP_CMD_GUI_EVENTS ) ) guiGetEvent( guiIEvent,(char *)cmd->id );
       else
#endif
      mp_msg(MSGT_CPLAYER, MSGL_V, "Received unknown cmd %s\n",cmd->name);
    }
    }
    switch (cmd->pausing) {
      case 1: // "pausing"
        osd_function = OSD_PAUSE;
        break;
      case 3: // "pausing_toggle"
        was_paused = !was_paused;
        // fall through
      case 2: // "pausing_keep"
        if (was_paused) osd_function = OSD_PAUSE;
    }
    mp_cmd_free(cmd);
  }
}
  was_paused = 0;

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
  
  if (end_at.type != END_AT_NONE) {
    if(end_at.type == END_AT_SIZE) {
      mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_MPEndposNoSizeBased);
      end_at.type = END_AT_NONE;
    } else {
      end_at.pos += rel_seek_secs;
    }
  }

  /* Looping. */
  if(eof==1 && loop_times>=0) {
    int l = loop_times;
    play_tree_iter_step(playtree_iter,0,0);
    loop_times = l;
    mp_msg(MSGT_CPLAYER,MSGL_V,"loop_times = %d, eof = %d\n", loop_times,eof);

    if(loop_times>1) loop_times--; else
    if(loop_times==1) loop_times=-1;
    play_n_frames=play_n_frames_mf;
    eof=0;
    abs_seek_pos=3; rel_seek_secs=0; // seek to start of movie (0%)
    loop_seek = 1;
  }

if(rel_seek_secs || abs_seek_pos){
  current_module="seek";
  if(demux_seek(demuxer,rel_seek_secs,audio_delay,abs_seek_pos)){
      // success:
      /* FIXME there should be real seeking for vobsub */
      if(sh_video) sh_video->pts=d_video->pts;
      if (vo_vobsub)
	//vobsub_reset(vo_vobsub);
	vobsub_seek(vo_vobsub,sh_video->pts);
      fflush(stdout);

      if(sh_video){
	 current_module="seek_video_reset";
         resync_video_stream(sh_video);
         if(vo_config_count) video_out->control(VOCTRL_RESET,NULL);
	 sh_video->num_buffered_pts = 0;
	 sh_video->last_pts = MP_NOPTS_VALUE;
      }
      
      if(sh_audio){
        current_module="seek_audio_reset";
        audio_out->reset(); // stop audio, throwing away buffered data
        sh_audio->a_buffer_len = 0;
        sh_audio->a_out_buffer_len = 0;
      }
        // Set OSD:
      if(!loop_seek){
	if( !edl_decision )
          set_osd_bar(0,"Position",0,100,demuxer_get_percent_pos(demuxer));
      }

      if(sh_video) {
	c_total=0;
	max_pts_correction=0.1;
	osd_visible=(GetTimerMS() + 1000) | 1; // to revert to PLAY pointer after 1 sec
	audio_time_usage=0; video_time_usage=0; vout_time_usage=0;
	drop_frame_cnt=0;

        if(vo_spudec) spudec_reset(vo_spudec);
      }
  }
/*
 * We saw a seek, have to rewind the EDL operations stack
 * and find the next EDL action to take care of.
 */

edl_muted = 0;
next_edl_record = edl_records;

while (next_edl_record)
{
    /* Trying to remember if we need to mute/unmute first;
     * prior EDL implementation lacks this.
     */
  
    if (next_edl_record->start_sec >= sh_video->pts)
        break;

    if (next_edl_record->action == EDL_MUTE) edl_muted = !edl_muted;
    next_edl_record = next_edl_record->next;

}
if ((user_muted | edl_muted) != mixer.muted) mixer_mute(&mixer);

  rel_seek_secs=0;
  abs_seek_pos=0;
  frame_time_remaining=0;
  current_module=NULL;
  loop_seek=0;
}

#ifdef HAVE_NEW_GUI
      if(use_gui){
        guiEventHandling();
	if(demuxer->file_format==DEMUXER_TYPE_AVI && sh_video && sh_video->video.dwLength>2){
	  // get pos from frame number / total frames
	  guiIntfStruct.Position=(float)d_video->pack_no*100.0f/sh_video->video.dwLength;
	} else {
	 off_t len = ( demuxer->movi_end - demuxer->movi_start );
	 off_t pos = ( demuxer->file_format == DEMUXER_TYPE_AUDIO?stream->pos:demuxer->filepos );
	 guiIntfStruct.Position=(len <= 0? 0.0f : ( pos - demuxer->movi_start ) * 100.0f / len );
	}
	if ( sh_video ) guiIntfStruct.TimeSec=sh_video->pts;
	  else if ( sh_audio ) guiIntfStruct.TimeSec=sh_audio->delay;
	guiIntfStruct.LengthInSec=demuxer_get_time_length(demuxer);
	guiGetEvent( guiReDraw,NULL );
	guiGetEvent( guiSetVolume,NULL );
	if(guiIntfStruct.Playing==0) break; // STOP
	if(guiIntfStruct.Playing==2) osd_function=OSD_PAUSE;
        if ( guiIntfStruct.DiskChanged || guiIntfStruct.NewPlay ) goto goto_next_file;
#ifdef USE_DVDREAD
        if ( stream->type == STREAMTYPE_DVD )
	 {
	  dvd_priv_t * dvdp = stream->priv;
	  guiIntfStruct.DVD.current_chapter=dvd_chapter_from_cell(dvdp,guiIntfStruct.DVD.current_title-1, dvdp->cur_cell)+1;
	 }
#endif
      }
#endif /* HAVE_NEW_GUI */

} // while(!eof)

mp_msg(MSGT_GLOBAL,MSGL_V,"EOF code: %d  \n",eof);

#ifdef HAS_DVBIN_SUPPORT
if(dvbin_reopen)
{
  eof = 0;
  uninit_player(INITED_ALL-(INITED_STREAM|INITED_INPUT));
  cache_uninit(stream);
  dvbin_reopen = 0;
  goto goto_enable_cache;
}
#endif
}

goto_next_file:  // don't jump here after ao/vo/getch initialization!

mp_msg(MSGT_CPLAYER,MSGL_INFO,"\n");

if(benchmark){
  double tot=video_time_usage+vout_time_usage+audio_time_usage;
  double total_time_usage;
  total_time_usage_start=GetTimer()-total_time_usage_start;
  total_time_usage = (float)total_time_usage_start*0.000001;
  mp_msg(MSGT_CPLAYER,MSGL_INFO,"\nBENCHMARKs: VC:%8.3fs VO:%8.3fs A:%8.3fs Sys:%8.3fs = %8.3fs\n",
	 video_time_usage,vout_time_usage,audio_time_usage,
	 total_time_usage-tot,total_time_usage);
  if(total_time_usage>0.0)
    mp_msg(MSGT_CPLAYER,MSGL_INFO,"BENCHMARK%%: VC:%8.4f%% VO:%8.4f%% A:%8.4f%% Sys:%8.4f%% = %8.4f%%\n",
	   100.0*video_time_usage/total_time_usage,
	   100.0*vout_time_usage/total_time_usage,
	   100.0*audio_time_usage/total_time_usage,
	   100.0*(total_time_usage-tot)/total_time_usage,
	   100.0);
  if(total_frame_cnt && frame_dropping)
    mp_msg(MSGT_CPLAYER,MSGL_INFO,"BENCHMARKn: disp: %d (%3.2f fps)  drop: %d (%d%%)  total: %d (%3.2f fps)\n",
	total_frame_cnt-drop_frame_cnt,
	(total_time_usage>0.5)?((total_frame_cnt-drop_frame_cnt)/total_time_usage):0,
	drop_frame_cnt,
	100*drop_frame_cnt/total_frame_cnt,
	total_frame_cnt,
	(total_time_usage>0.5)?(total_frame_cnt/total_time_usage):0);
  
}

// time to uninit all, except global stuff:
uninit_player(INITED_ALL-(INITED_GUI+INITED_INPUT+(fixed_vo?INITED_VO:0)));

  if ( set_of_sub_size > 0 ) 
   {
    current_module="sub_free";
    for (i = 0; i < set_of_sub_size; ++i) {
        sub_free( set_of_subtitles[i] );
#ifdef USE_ASS
        if ( set_of_ass_tracks[i] )
            ass_free_track( set_of_ass_tracks[i] );
#endif
    }
    set_of_sub_size = 0;
   }
    vo_sub_last = vo_sub=NULL;
    subdata=NULL;
#ifdef USE_ASS
    ass_track = NULL;
#endif

if(eof == PT_NEXT_ENTRY || eof == PT_PREV_ENTRY) {
  eof = eof == PT_NEXT_ENTRY ? 1 : -1;
  if(play_tree_iter_step(playtree_iter,play_tree_step,0) == PLAY_TREE_ITER_ENTRY) {
    eof = 1;
  } else {
    play_tree_iter_free(playtree_iter);
    playtree_iter = NULL;
  }
  play_tree_step = 1;
} else if (eof == PT_UP_NEXT || eof == PT_UP_PREV) {
  eof = eof == PT_UP_NEXT ? 1 : -1;
  if ( playtree_iter ) {
    if(play_tree_iter_up_step(playtree_iter,eof,0) == PLAY_TREE_ITER_ENTRY) {
     eof = 1;
    } else {
      play_tree_iter_free(playtree_iter);
      playtree_iter = NULL;
    }
   }
} else { // NEXT PREV SRC
     eof = eof == PT_PREV_SRC ? -1 : 1;
}

if(eof == 0) eof = 1;

while(playtree_iter != NULL) {
  filename = play_tree_iter_get_file(playtree_iter,eof);
  if(filename == NULL) {
    if( play_tree_iter_step(playtree_iter,eof,0) != PLAY_TREE_ITER_ENTRY) {
      play_tree_iter_free(playtree_iter);
      playtree_iter = NULL;
    };
  } else
    break;
} 

#ifdef HAVE_NEW_GUI
 if( use_gui && !playtree_iter ) 
  {
#ifdef USE_DVDREAD
   if ( !guiIntfStruct.DiskChanged ) 
#endif
   mplEnd();
  }	
#endif

if(use_gui || playtree_iter != NULL || player_idle_mode){
  if (!playtree_iter) filename = NULL;
  eof = 0;
  goto play_next_file;
}


exit_player_with_rc(MSGTR_Exit_eof, 0);

return 1;
}
