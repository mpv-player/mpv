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

#include "mpvcore/options.h"
#include "demux/demux.h"

// definitions used internally by the core player code

#define INITIALIZED_VO      1
#define INITIALIZED_AO      2
#define INITIALIZED_GETCH2  8
#define INITIALIZED_PLAYBACK 16
#define INITIALIZED_STREAM  64
#define INITIALIZED_DEMUXER 512
#define INITIALIZED_ACODEC  1024
#define INITIALIZED_VCODEC  2048
#define INITIALIZED_SUB     4096
#define INITIALIZED_ALL     0xFFFF


enum stop_play_reason {
    KEEP_PLAYING = 0,   // must be 0, numeric values of others do not matter
    AT_END_OF_FILE,     // file has ended, prepare to play next
                        // also returned on unrecoverable playback errors
    PT_NEXT_ENTRY,      // prepare to play next entry in playlist
    PT_CURRENT_ENTRY,   // prepare to play mpctx->playlist->current
    PT_STOP,            // stop playback, clear playlist
    PT_RESTART,         // restart previous file
    PT_QUIT,            // stop playback, quit player
};

enum exit_reason {
  EXIT_NONE,
  EXIT_QUIT,
  EXIT_PLAYED,
  EXIT_ERROR,
  EXIT_NOTPLAYED,
  EXIT_SOMENOTPLAYED
};

struct timeline_part {
    double start;
    double source_start;
    struct demuxer *source;
};

struct chapter {
    double start;
    char *name;
};

enum mp_osd_seek_info {
    OSD_SEEK_INFO_BAR           = 1,
    OSD_SEEK_INFO_TEXT          = 2,
    OSD_SEEK_INFO_CHAPTER_TEXT  = 4,
    OSD_SEEK_INFO_EDITION       = 8,
};

enum seek_type {
    MPSEEK_NONE = 0,
    MPSEEK_RELATIVE,
    MPSEEK_ABSOLUTE,
    MPSEEK_FACTOR,
};

struct track {
    enum stream_type type;
    // The type specific ID, also called aid (audio), sid (subs), vid (video).
    // For UI purposes only; this ID doesn't have anything to do with any
    // IDs coming from demuxers or container files.
    int user_tid;

    // Same as stream->demuxer_id. -1 if not set.
    int demuxer_id;

    char *title;
    bool default_track;
    bool attached_picture;
    char *lang;

    // If this track is from an external file (e.g. subtitle file).
    bool is_external;
    char *external_filename;
    bool auto_loaded;

    // If the track's stream changes with the timeline (ordered chapters).
    bool under_timeline;

    // Value can change if under_timeline==true.
    struct demuxer *demuxer;
    // Invariant: !stream || stream->demuxer == demuxer
    struct sh_stream *stream;

    // For external subtitles, which are read fully on init. Do not attempt
    // to read packets from them.
    bool preloaded;
};

enum {
    MAX_NUM_VO_PTS = 100,
};

typedef struct MPContext {
    struct mpv_global *global;
    struct MPOpts *opts;
    struct mp_log *log;
    struct m_config *mconfig;
    struct input_ctx *input;
    struct osd_state *osd;
    struct mp_osd_msg *osd_msg_stack;
    char *terminal_osd_text;

    int add_osd_seek_info; // bitfield of enum mp_osd_seek_info
    double osd_visible; // for the osd bar only
    int osd_function;
    double osd_function_visible;
    double osd_last_update;

    struct playlist *playlist;
    char *filename; // currently playing file
    struct mp_resolve_result *resolve_result;
    enum stop_play_reason stop_play;
    unsigned int initialized_flags;  // which subsystems have been initialized

    // Return code to use with PT_QUIT
    enum exit_reason quit_player_rc;
    int quit_custom_rc;
    bool has_quit_custom_rc;
    bool error_playing;

    struct demuxer **sources;
    int num_sources;

    struct timeline_part *timeline;
    int num_timeline_parts;
    int timeline_part;
    // NOTE: even if num_chapters==0, chapters being not NULL signifies presence
    //       of chapter metadata
    struct chapter *chapters;
    int num_chapters;
    double video_offset;

    struct stream *stream;
    struct demuxer *demuxer;

    struct track **tracks;
    int num_tracks;

    // Selected tracks. NULL if no track selected.
    struct track *current_track[STREAM_TYPE_COUNT];

    struct sh_stream *sh[STREAM_TYPE_COUNT];
    struct sh_audio *sh_audio;          // same as sh[STREAM_AUDIO]->audio
    struct sh_video *sh_video;          // same as sh[STREAM_VIDEO]->video
    struct sh_sub *sh_sub;              // same as sh[STREAM_SUB]->sub

    // Uses: accessing metadata (consider ordered chapters case, where the main
    // demuxer defines metadata), or special purpose demuxers like TV.
    struct demuxer *master_demuxer;

    struct mixer *mixer;
    struct ao *ao;
    struct vo *video_out;

    /* We're starting playback from scratch or after a seek. Show first
     * video frame immediately and reinitialize sync. */
    bool restart_playback;
    /* Set if audio should be timed to start with video frame after seeking,
     * not set when e.g. playing cover art */
    bool sync_audio_to_video;
    /* After playback restart (above) or audio stream change, adjust audio
     * stream by cutting samples or adding silence at the beginning to make
     * audio playback position match video position. */
    bool syncing_audio;
    bool hrseek_active;
    bool hrseek_framedrop;
    double hrseek_pts;
    // AV sync: the next frame should be shown when the audio out has this
    // much (in seconds) buffered data left. Increased when more data is
    // written to the ao, decreased when moving to the next frame.
    // In the audio-only case used as a timer since the last seek
    // by the audio CPU usage meter.
    double delay;
    // AV sync: time until next frame should be shown
    double time_frame;
    // How long the last vo flip() call took. Used to adjust timing with
    // the goal of making flip() calls finish (rather than start) at the
    // specified time.
    double last_vo_flip_duration;
    // How much video timing has been changed to make it match the audio
    // timeline. Used for status line information only.
    double total_avsync_change;
    // Total number of dropped frames that were "approved" to be dropped.
    // Actual dropping depends on --framedrop and decoder internals.
    int drop_frame_cnt;
    // Number of frames dropped in a row.
    int dropped_frames;
    // A-V sync difference when last frame was displayed. Kept to display
    // the same value if the status line is updated at a time where no new
    // video frame is shown.
    double last_av_difference;
    /* timestamp of video frame currently visible on screen
     * (or at least queued to be flipped by VO) */
    double video_pts;
    double last_seek_pts;
    // As video_pts, but is not reset when seeking away. (For the very short
    // period of time until a new frame is decoded and shown.)
    double last_vo_pts;
    // Video PTS, or audio PTS if video has ended.
    double playback_pts;
    // Used to determine whether the video filter chain was rebuilt.
    long last_vf_reconfig_count;

    // History of video frames timestamps that were queued in the VO
    // This includes even skipped frames during hr-seek
    double vo_pts_history_pts[MAX_NUM_VO_PTS];
    // Whether the PTS at vo_pts_history[n] is after a seek reset
    uint64_t vo_pts_history_seek[MAX_NUM_VO_PTS];
    uint64_t vo_pts_history_seek_ts;
    uint64_t backstep_start_seek_ts;
    bool backstep_active;

    double audio_delay;

    double last_heartbeat;
    double last_metadata_update;

    double mouse_timer;
    unsigned int mouse_event_ts;
    bool mouse_cursor_visible;

    // used to prevent hanging in some error cases
    double start_timestamp;

    // Timestamp from the last time some timing functions read the
    // current time, in (occasionally wrapping) microseconds. Used
    // to turn a new time value to a delta from last time.
    int64_t last_time;

    // Used to communicate the parameters of a seek between parts
    struct seek_params {
        enum seek_type type;
        double amount;
        int exact;  // -1 = disable, 0 = default, 1 = enable
        // currently not set by commands, only used internally by seek()
        int direction; // -1 = backward, 0 = default, 1 = forward
    } seek;

    /* Heuristic for relative chapter seeks: keep track which chapter
     * the user wanted to go to, even if we aren't exactly within the
     * boundaries of that chapter due to an inaccurate seek. */
    int last_chapter_seek;
    double last_chapter_pts;

    struct ass_library *ass_library;

    int last_dvb_step;
    int dvbin_reopen;

    bool paused;
    // step this many frames, then pause
    int step_frames;
    // Counted down each frame, stop playback if 0 is reached. (-1 = disable)
    int max_frames;
    bool playing_msg_shown;

    bool paused_for_cache;

    // Set after showing warning about decoding being too slow for realtime
    // playback rate. Used to avoid showing it multiple times.
    bool drop_message_shown;

    struct screenshot_ctx *screenshot_ctx;

    char *track_layout_hash;

    struct encode_lavc_context *encode_lavc_ctx;
} MPContext;


// should not be global
extern FILE *edl_fd;
// These appear in options list
extern int forced_subs_only;

void uninit_player(struct MPContext *mpctx, unsigned int mask);
void reinit_audio_chain(struct MPContext *mpctx);
double playing_audio_pts(struct MPContext *mpctx);
struct track *mp_add_subtitles(struct MPContext *mpctx, char *filename);
int reinit_video_chain(struct MPContext *mpctx);
int reinit_video_filters(struct MPContext *mpctx);
int reinit_audio_filters(struct MPContext *mpctx);
void pause_player(struct MPContext *mpctx);
void unpause_player(struct MPContext *mpctx);
void add_step_frame(struct MPContext *mpctx, int dir);
void queue_seek(struct MPContext *mpctx, enum seek_type type, double amount,
                int exact);
bool mp_seek_chapter(struct MPContext *mpctx, int chapter);
double get_time_length(struct MPContext *mpctx);
double get_start_time(struct MPContext *mpctx);
double get_current_time(struct MPContext *mpctx);
int get_percent_pos(struct MPContext *mpctx);
double get_current_pos_ratio(struct MPContext *mpctx, bool use_range);
int get_current_chapter(struct MPContext *mpctx);
char *chapter_display_name(struct MPContext *mpctx, int chapter);
char *chapter_name(struct MPContext *mpctx, int chapter);
double chapter_start_time(struct MPContext *mpctx, int chapter);
int get_chapter_count(struct MPContext *mpctx);
void mp_switch_track(struct MPContext *mpctx, enum stream_type type,
                     struct track *track);
struct track *mp_track_by_tid(struct MPContext *mpctx, enum stream_type type,
                              int tid);
bool mp_remove_track(struct MPContext *mpctx, struct track *track);
struct playlist_entry *mp_next_file(struct MPContext *mpctx, int direction,
                                    bool force);
int mp_get_cache_percent(struct MPContext *mpctx);
void mp_write_watch_later_conf(struct MPContext *mpctx);
void mp_set_playlist_entry(struct MPContext *mpctx, struct playlist_entry *e);
struct playlist_entry *mp_resume_playlist(struct playlist *pl);
void mp_force_video_refresh(struct MPContext *mpctx);

void mp_print_version(int always);

// timeline/tl_matroska.c
void build_ordered_chapter_timeline(struct MPContext *mpctx);
// timeline/tl_edl.c
void build_edl_timeline(struct MPContext *mpctx);
// timeline/tl_cue.c
void build_cue_timeline(struct MPContext *mpctx);

#endif /* MPLAYER_MP_CORE_H */
