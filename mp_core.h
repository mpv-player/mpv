// definitions used internally by the core player code

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


#define SUB_SOURCE_SUBS 0
#define SUB_SOURCE_VOBSUB 1
#define SUB_SOURCE_DEMUX 2
#define SUB_SOURCES 3


#define PT_NEXT_ENTRY 1
#define PT_PREV_ENTRY -1
#define PT_NEXT_SRC 2
#define PT_PREV_SRC -2
#define PT_UP_NEXT 3
#define PT_UP_PREV -3


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

#define MAX_OSD_LEVEL 3
#define MAX_TERM_OSD_LEVEL 1


typedef struct MPContext {
    int osd_show_percentage;
    int osd_function;
    ao_functions_t *audio_out;
    play_tree_t *playtree;
    play_tree_iter_t *playtree_iter;
    int eof;
    int play_tree_step;

    stream_t *stream;
    demuxer_t *demuxer;
    sh_audio_t *sh_audio;
    sh_video_t *sh_video;
    demux_stream_t *d_audio;
    demux_stream_t *d_video;
    demux_stream_t *d_sub;
    mixer_t mixer;
    vo_functions_t *video_out;
    // Frames buffered in the vo ready to flip. Currently always 0 or 1.
    // This is really a vo variable but currently there's no suitable vo
    // struct.
    int num_buffered_frames;

    // AV sync: the next frame should be shown when the audio out has this
    // much (in seconds) buffered data left. Increased when more data is
    // written to the ao, decreased when moving to the next frame.
    // In the audio-only case used as a timer since the last seek
    // by the audio CPU usage meter.
    double delay;

    float begin_skip; ///< start time of the current skip while on edlout mode
    // audio is muted if either EDL or user activates mute
    short edl_muted; ///< Stores whether EDL is currently in muted mode.
    short user_muted; ///< Stores whether user wanted muted mode.

    int global_sub_size; // this encompasses all subtitle sources
    int global_sub_pos; // this encompasses all subtitle sources
    int set_of_sub_pos;
    int set_of_sub_size;
    int global_sub_indices[SUB_SOURCES];
#ifdef USE_ASS
    // set_of_ass_tracks[i] contains subtitles from set_of_subtitles[i]
    // parsed by libass or NULL if format unsupported
    ass_track_t* set_of_ass_tracks[MAX_SUBTITLE_FILES];
#endif
    sub_data* set_of_subtitles[MAX_SUBTITLE_FILES];

    int file_format;

#ifdef HAS_DVBIN_SUPPORT
    int last_dvb_step;
    int dvbin_reopen;
#endif

    int was_paused;
} MPContext;


// Most of these should not be globals
extern int abs_seek_pos;
extern float rel_seek_secs;
extern FILE *edl_fd;
extern int file_filter;
// These appear in options list
extern float playback_speed;
extern int osd_duration;
extern int term_osd;
extern int fixed_vo;
extern int ass_enabled;
extern int fixed_vo;
extern int forced_subs_only;

// These were listed as externs in mplayer.c, should be in some other header
extern int vo_gamma_gamma;
extern int vo_gamma_brightness;
extern int vo_gamma_contrast;
extern int vo_gamma_saturation;
extern int vo_gamma_hue;



int build_afilter_chain(sh_audio_t *sh_audio, ao_data_t *ao_data);
void uninit_player(unsigned int mask);
void reinit_audio_chain(void);
void init_vo_spudec(void);
void set_osd_bar(int type,const char* name,double min,double max,double val);
void set_osd_msg(int id, int level, int time, const char* fmt, ...);
double playing_audio_pts(sh_audio_t *sh_audio, demux_stream_t *d_audio,
			 ao_functions_t *audio_out);
void exit_player_with_rc(const char* how, int rc);
char *get_path(const char *filename);
void rm_osd_msg(int id);
void add_subtitles(char *filename, float fps, int silent);
void mplayer_put_key(int code);
int reinit_video_chain(void);
