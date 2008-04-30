#ifndef MPLAYER_MP_CORE_H
#define MPLAYER_MP_CORE_H

#include "options.h"
#include "mixer.h"
#include "subreader.h"

// definitions used internally by the core player code

#define INITIALIZED_VO      1
#define INITIALIZED_AO      2
#define INITIALIZED_GUI     4
#define INITIALIZED_GETCH2  8
#define INITIALIZED_SPUDEC  32
#define INITIALIZED_STREAM  64
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


typedef struct MPContext {
    struct MPOpts opts;
    struct m_config *mconfig;
    struct vo_x11_state *x11_state;
    struct mp_fifo *key_fifo;
    struct input_ctx *input;
    int osd_show_percentage;
    int osd_function;
    const ao_functions_t *audio_out;
    struct play_tree *playtree;
    struct play_tree_iter *playtree_iter;
    char *filename; // currently playing file
    int eof;
    int play_tree_step;
    unsigned int initialized_flags;  // which subsystems have been initialized

    struct stream *stream;
    struct demuxer *demuxer;
    struct sh_audio *sh_audio;
    struct sh_video *sh_video;
    struct demux_stream *d_audio;
    struct demux_stream *d_video;
    struct demux_stream *d_sub;
    mixer_t mixer;
    struct vo *video_out;
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

    // Timestamp from the last time some timing functions read the
    // current time, in (occasionally wrapping) microseconds. Used
    // to turn a new time value to a delta from last time.
    unsigned int last_time;

    // Used to communicate the parameters of a seek between parts
    float rel_seek_secs;
    int abs_seek_pos;

    float begin_skip; ///< start time of the current skip while on edlout mode
    // audio is muted if either EDL or user activates mute
    short edl_muted; ///< Stores whether EDL is currently in muted mode.
    short user_muted; ///< Stores whether user wanted muted mode.

    int global_sub_size; // this encompasses all subtitle sources
    int global_sub_pos; // this encompasses all subtitle sources
    int set_of_sub_pos;
    int set_of_sub_size;
    int global_sub_indices[SUB_SOURCES];
    // set_of_ass_tracks[i] contains subtitles from set_of_subtitles[i]
    // parsed by libass or NULL if format unsupported
    struct ass_track_s *set_of_ass_tracks[MAX_SUBTITLE_FILES];
    sub_data* set_of_subtitles[MAX_SUBTITLE_FILES];

    int file_format;

    int last_dvb_step;
    int dvbin_reopen;

    int was_paused;

#ifdef USE_DVDNAV
    struct mp_image *nav_smpi; ///< last decoded dvdnav video image
    unsigned char *nav_buffer;   ///< last read dvdnav video frame
    unsigned char *nav_start;    ///< pointer to last read video buffer
    int            nav_in_size;  ///< last read size
#endif
} MPContext;


// Most of these should not be globals
extern FILE *edl_fd;
extern int file_filter;
// These appear in options list
extern int forced_subs_only;

struct ao_data;
int build_afilter_chain(struct MPContext *mpctx, struct sh_audio *sh_audio, struct ao_data *ao_data);
void uninit_player(struct MPContext *mpctx, unsigned int mask);
void reinit_audio_chain(struct MPContext *mpctx);
void init_vo_spudec(struct MPContext *mpctx);
double playing_audio_pts(struct MPContext *mpctx);
void exit_player_with_rc(struct MPContext *mpctx, const char* how, int rc);
void add_subtitles(struct MPContext *mpctx, char *filename, float fps, int noerr);
int reinit_video_chain(struct MPContext *mpctx);

#endif /* MPLAYER_MP_CORE_H */
