#ifndef MPLAYER_MP_CORE_H
#define MPLAYER_MP_CORE_H

#include "mp_osd.h"
#include "libao2/audio_out.h"
#include "playtree.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "mixer.h"
#include "libvo/video_out.h"
#include "subreader.h"

// definitions used internally by the core player code

#define INITIALIZED_VO      1
#define INITIALIZED_AO      2
#define INITIALIZED_GUI     4
#define INITIALIZED_GETCH2  8
#define INITIALIZED_SPUDEC  32
#define INITIALIZED_STREAM  64
#define INITIALIZED_INPUT   128
#define INITIALIZED_VOBSUB  256
#define INITIALIZED_DEMUXER 512
#define INITIALIZED_ACODEC  1024
#define INITIALIZED_VCODEC  2048
#define INITIALIZED_ALL     0xFFFF


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
#define PT_STOP 4

typedef enum {
  EXIT_NONE,
  EXIT_QUIT,
  EXIT_EOF,
  EXIT_ERROR
} exit_reason_t;

typedef struct MPContext {
    int osd_show_percentage;
    int osd_function;
    const ao_functions_t *audio_out;
    play_tree_t *playtree;
    play_tree_iter_t *playtree_iter;
    int eof;
    int play_tree_step;
    int loop_times;

    stream_t *stream;
    demuxer_t *demuxer;
    sh_audio_t *sh_audio;
    sh_video_t *sh_video;
    demux_stream_t *d_audio;
    demux_stream_t *d_video;
    demux_stream_t *d_sub;
    mixer_t mixer;
    const vo_functions_t *video_out;
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
#ifdef CONFIG_ASS
    // set_of_ass_tracks[i] contains subtitles from set_of_subtitles[i]
    // parsed by libass or NULL if format unsupported
    ass_track_t* set_of_ass_tracks[MAX_SUBTITLE_FILES];
#endif
    sub_data* set_of_subtitles[MAX_SUBTITLE_FILES];

    int file_format;

#ifdef CONFIG_DVBIN
    int last_dvb_step;
    int dvbin_reopen;
#endif

    int was_paused;

#ifdef CONFIG_DVDNAV
    struct mp_image_s *nav_smpi; ///< last decoded dvdnav video image
    unsigned char *nav_buffer;   ///< last read dvdnav video frame
    unsigned char *nav_start;    ///< pointer to last read video buffer
    int            nav_in_size;  ///< last read size
#endif
} MPContext;


// Most of these should not be globals
extern int abs_seek_pos;
extern float rel_seek_secs;
extern FILE *edl_fd;
extern int file_filter;
// These appear in options list
extern float playback_speed;
extern int fixed_vo;
extern int forced_subs_only;


int build_afilter_chain(sh_audio_t *sh_audio, ao_data_t *ao_data);
void uninit_player(unsigned int mask);
void reinit_audio_chain(void);
void init_vo_spudec(void);
double playing_audio_pts(sh_audio_t *sh_audio, demux_stream_t *d_audio,
			 const ao_functions_t *audio_out);
void exit_player_with_rc(exit_reason_t how, int rc);
void add_subtitles(char *filename, float fps, int noerr);
int reinit_video_chain(void);

#endif /* MPLAYER_MP_CORE_H */
