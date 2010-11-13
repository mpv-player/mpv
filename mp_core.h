/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_MP_CORE_H
#define MPLAYER_MP_CORE_H

#include <stdbool.h>

#include "options.h"
#include "mixer.h"
#include "subreader.h"

// definitions used internally by the core player code

#define INITIALIZED_VO      1
#define INITIALIZED_AO      2

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


enum stop_play_reason {
    KEEP_PLAYING = 0,  // must be 0, numeric values of others do not matter
    AT_END_OF_FILE,
    PT_NEXT_ENTRY,
    PT_PREV_ENTRY,
    PT_NEXT_SRC,
    PT_PREV_SRC,
    PT_UP_NEXT,
    PT_UP_PREV,
    PT_STOP,
};

enum exit_reason {
  EXIT_NONE,
  EXIT_QUIT,
  EXIT_EOF,
  EXIT_ERROR
};

struct content_source {
    struct stream *stream;
    struct demuxer *demuxer;
};

struct timeline_part {
    double start;
    double source_start;
    struct content_source *source;
};

struct chapter {
    double start;
    char *name;
};

typedef struct MPContext {
    struct MPOpts opts;
    struct m_config *mconfig;
    struct vo_x11_state *x11_state;
    struct mp_fifo *key_fifo;
    struct input_ctx *input;
    struct osd_state *osd;

    bool add_osd_seek_info;
    // if nonzero, hide current OSD contents when GetTimerMS() reaches this
    unsigned int osd_show_percentage_until;
    unsigned int osd_visible;

    int osd_function;
    const ao_functions_t *audio_out;
    struct play_tree *playtree;
    struct play_tree_iter *playtree_iter;
    char *filename; // currently playing file
    enum stop_play_reason stop_play;
    int play_tree_step;
    unsigned int initialized_flags;  // which subsystems have been initialized

    struct content_source *sources;
    int num_sources;
    struct timeline_part *timeline;
    int num_timeline_parts;
    int timeline_part;
    struct chapter *chapters;
    int num_chapters;
    double video_offset;

    struct stream *stream;
    struct demuxer *demuxer;
    struct sh_audio *sh_audio;
    struct sh_video *sh_video;
    struct demux_stream *d_audio;
    struct demux_stream *d_video;
    struct demux_stream *d_sub;
    mixer_t mixer;
    struct vo *video_out;

    /* We're starting playback from scratch or after a seek. Show first
     * video frame immediately and reinitialize sync. */
    bool restart_playback;
    /* After playback restart (above) or audio stream change, adjust audio
     * stream by cutting samples or adding silence at the beginning to make
     * audio playback position match video position. */
    bool syncing_audio;
    // AV sync: the next frame should be shown when the audio out has this
    // much (in seconds) buffered data left. Increased when more data is
    // written to the ao, decreased when moving to the next frame.
    // In the audio-only case used as a timer since the last seek
    // by the audio CPU usage meter.
    double delay;
    // AV sync: time until next frame should be shown
    float time_frame;
    // How long the last vo flip() call took. Used to adjust timing with
    // the goal of making flip() calls finish (rather than start) at the
    // specified time.
    float last_vo_flip_duration;
    // How much video timing has been changed to make it match the audio
    // timeline. Used for status line information only.
    double total_avsync_change;
    // A-V sync difference when last frame was displayed. Kept to display
    // the same value if the status line is updated at a time where no new
    // video frame is shown.
    double last_av_difference;

    // Timestamp from the last time some timing functions read the
    // current time, in (occasionally wrapping) microseconds. Used
    // to turn a new time value to a delta from last time.
    unsigned int last_time;

    // Used to communicate the parameters of a seek between parts
    double rel_seek_secs;
    int abs_seek_pos;

    /* Heuristic for relative chapter seeks: keep track which chapter
     * the user wanted to go to, even if we aren't exactly within the
     * boundaries of that chapter due to an inaccurate seek. */
    int last_chapter_seek;
    double last_chapter_pts;

    float begin_skip; ///< start time of the current skip while on edlout mode
    // audio is muted if either EDL or user activates mute
    short edl_muted; ///< Stores whether EDL is currently in muted mode.
    short user_muted; ///< Stores whether user wanted muted mode.

    int global_sub_size; // this encompasses all subtitle sources
    int global_sub_pos; // this encompasses all subtitle sources
    int set_of_sub_pos;
    int set_of_sub_size;
    int sub_counts[SUB_SOURCES];
    // set_of_ass_tracks[i] contains subtitles from set_of_subtitles[i]
    // parsed by libass or NULL if format unsupported
    struct ass_track *set_of_ass_tracks[MAX_SUBTITLE_FILES];
    sub_data* set_of_subtitles[MAX_SUBTITLE_FILES];

    int file_format;

    int last_dvb_step;
    int dvbin_reopen;

    int paused;
    // step this many frames, then pause
    int step_frames;

    // Set after showing warning about decoding being too slow for realtime
    // playback rate. Used to avoid showing it multiple times.
    bool drop_message_shown;

#ifdef CONFIG_DVDNAV
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
void exit_player_with_rc(struct MPContext *mpctx, enum exit_reason how, int rc);
void add_subtitles(struct MPContext *mpctx, char *filename, float fps, int noerr);
int reinit_video_chain(struct MPContext *mpctx);
void pause_player(struct MPContext *mpctx);
void unpause_player(struct MPContext *mpctx);
void add_step_frame(struct MPContext *mpctx);
int seek_chapter(struct MPContext *mpctx, int chapter, double *seek_pts,
                 char **chapter_name);
double get_time_length(struct MPContext *mpctx);
double get_current_time(struct MPContext *mpctx);
int get_percent_pos(struct MPContext *mpctx);
int get_current_chapter(struct MPContext *mpctx);
char *chapter_display_name(struct MPContext *mpctx, int chapter);

#endif /* MPLAYER_MP_CORE_H */
