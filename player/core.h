/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_MP_CORE_H
#define MPLAYER_MP_CORE_H

#include <stdbool.h>

#include "libmpv/client.h"

#include "common/common.h"
#include "options/options.h"
#include "sub/osd.h"
#include "demux/timeline.h"

// definitions used internally by the core player code

enum stop_play_reason {
    KEEP_PLAYING = 0,   // must be 0, numeric values of others do not matter
    AT_END_OF_FILE,     // file has ended, prepare to play next
                        // also returned on unrecoverable playback errors
    PT_NEXT_ENTRY,      // prepare to play next entry in playlist
    PT_CURRENT_ENTRY,   // prepare to play mpctx->playlist->current
    PT_STOP,            // stop playback, clear playlist
    PT_RELOAD_DEMUXER,  // restart playback, but keep stream open
    PT_QUIT,            // stop playback, quit player
    PT_ERROR,           // play next playlist entry (due to an error)
};

enum mp_osd_seek_info {
    OSD_SEEK_INFO_BAR           = 1,
    OSD_SEEK_INFO_TEXT          = 2,
    OSD_SEEK_INFO_CHAPTER_TEXT  = 4,
    OSD_SEEK_INFO_EDITION       = 8,
    OSD_SEEK_INFO_CURRENT_FILE  = 16,
};


enum {
    // other constants
    MAX_OSD_LEVEL = 3,
    OSD_LEVEL_INVISIBLE = 4,
    OSD_BAR_SEEK = 256,

    MAX_NUM_VO_PTS = 100,
};

enum seek_type {
    MPSEEK_NONE = 0,
    MPSEEK_RELATIVE,
    MPSEEK_ABSOLUTE,
    MPSEEK_FACTOR,
};

enum seek_precision {
    MPSEEK_DEFAULT = 0,
    // The following values are numerically sorted by increasing precision
    MPSEEK_KEYFRAME,
    MPSEEK_EXACT,
    MPSEEK_VERY_EXACT,
};

struct track {
    enum stream_type type;

    // Currently used for decoding.
    bool selected;

    // The type specific ID, also called aid (audio), sid (subs), vid (video).
    // For UI purposes only; this ID doesn't have anything to do with any
    // IDs coming from demuxers or container files.
    int user_tid;

    int demuxer_id; // same as stream->demuxer_id. -1 if not set.
    int ff_index; // same as stream->ff_index, or 0.

    char *title;
    bool default_track;
    bool attached_picture;
    char *lang;

    // If this track is from an external file (e.g. subtitle file).
    bool is_external;
    bool no_default;            // pretend it's not external for auto-selection
    char *external_filename;
    bool auto_loaded;

    // If the track's stream changes with the timeline (ordered chapters).
    bool under_timeline;

    // Does not change with under_timeline, but it useless for most purposes.
    struct sh_stream *original_stream;

    // Value can change if under_timeline==true.
    struct demuxer *demuxer;
    // Invariant: !stream || stream->demuxer == demuxer
    struct sh_stream *stream;

    // For external subtitles, which are read fully on init. Do not attempt
    // to read packets from them.
    bool preloaded;
};

/* Note that playback can be paused, stopped, etc. at any time. While paused,
 * playback restart is still active, because you want seeking to work even
 * if paused.
 * The main purpose of distinguishing these states is proper reinitialization
 * of A/V sync.
 */
enum playback_status {
    // code may compare status values numerically
    STATUS_SYNCING,     // seeking for a position to resume
    STATUS_FILLING,     // decoding more data (so you start with full buffers)
    STATUS_READY,       // buffers full, playback can be started any time
    STATUS_PLAYING,     // normal playback
    STATUS_DRAINING,    // decoding has ended; still playing out queued buffers
    STATUS_EOF,         // playback has ended, or is disabled
};

#define NUM_PTRACKS 2

typedef struct MPContext {
    bool initialized;
    bool autodetach;
    struct mpv_global *global;
    struct MPOpts *opts;
    struct mp_log *log;
    struct m_config *mconfig;
    struct input_ctx *input;
    struct mp_client_api *clients;
    struct mp_dispatch_queue *dispatch;
    struct mp_cancel *playback_abort;

    struct mp_log *statusline;
    struct osd_state *osd;
    char *term_osd_text;
    char *term_osd_status;
    char *term_osd_subs;
    char *term_osd_contents;
    char *last_window_title;

    int add_osd_seek_info; // bitfield of enum mp_osd_seek_info
    double osd_visible; // for the osd bar only
    int osd_function;
    double osd_function_visible;
    double osd_msg_visible;
    double osd_msg_next_duration;
    double osd_last_update;
    bool osd_force_update;
    char *osd_msg_text;
    bool osd_show_pos;
    struct osd_progbar_state osd_progbar;

    struct playlist *playlist;
    struct playlist_entry *playing; // currently playing file
    char *filename; // immutable copy of playing->filename (or NULL)
    char *stream_open_filename;
    enum stop_play_reason stop_play;
    bool playback_initialized; // playloop can be run/is running
    int error_playing;

    // Return code to use with PT_QUIT
    int quit_custom_rc;
    bool has_quit_custom_rc;
    char **resume_defaults;

    // Global file statistics
    int files_played;       // played without issues (even if stopped by user)
    int files_errored;      // played, but errors happened at one point
    int files_broken;       // couldn't be played at all

    // Current file statistics
    int64_t shown_vframes, shown_aframes;

    struct stream *stream; // stream that was initially opened
    struct demuxer **sources; // all open demuxers
    int num_sources;

    struct timeline *tl;
    struct timeline_part *timeline;
    int num_timeline_parts;
    int timeline_part;
    struct demux_chapter *chapters;
    int num_chapters;
    double video_offset;

    struct demuxer *demuxer; // can change with timeline
    struct mp_tags *filtered_tags;

    struct track **tracks;
    int num_tracks;

    char *track_layout_hash;

    // Selected tracks. NULL if no track selected.
    // There can be NUM_PTRACKS of the same STREAM_TYPE selected at once.
    // Currently, this is used for the secondary subtitle track only.
    struct track *current_track[NUM_PTRACKS][STREAM_TYPE_COUNT];

    struct dec_video *d_video;
    struct dec_audio *d_audio;
    struct dec_sub *d_sub[2];

    // Uses: accessing metadata (consider ordered chapters case, where the main
    // demuxer defines metadata), or special purpose demuxers like TV.
    struct demuxer *master_demuxer;
    struct demuxer *track_layout;   // complication for ordered chapters

    struct mixer *mixer;
    struct ao *ao;
    struct mp_audio *ao_decoder_fmt; // for weak gapless audio check
    struct mp_audio_buffer *ao_buffer;  // queued audio; passed to ao_play() later

    struct vo *video_out;
    // next_frame[0] is the next frame, next_frame[1] the one after that.
    struct mp_image *next_frame[2];
    struct mp_image *saved_frame;   // for hrseek_lastframe

    enum playback_status video_status, audio_status;
    bool restart_complete;
    /* Set if audio should be timed to start with video frame after seeking,
     * not set when e.g. playing cover art */
    bool sync_audio_to_video;
    bool hrseek_active;     // skip all data until hrseek_pts
    bool hrseek_framedrop;  // allow decoder to drop frames before hrseek_pts
    bool hrseek_lastframe;  // drop everything until last frame reached
    double hrseek_pts;
    // AV sync: the next frame should be shown when the audio out has this
    // much (in seconds) buffered data left. Increased when more data is
    // written to the ao, decreased when moving to the next video frame.
    double delay;
    // AV sync: time in seconds until next frame should be shown
    double time_frame;
    // How much video timing has been changed to make it match the audio
    // timeline. Used for status line information only.
    double total_avsync_change;
    // Total number of dropped frames that were dropped by decoder.
    int dropped_frames_total;
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
    // Mostly unused; for proper audio resync on speed changes.
    double video_next_pts;
    // As video_pts, but is not reset when seeking away. (For the very short
    // period of time until a new frame is decoded and shown.)
    double last_vo_pts;
    // Video PTS, or audio PTS if video has ended.
    double playback_pts;

    int last_chapter;

    // History of video frames timestamps that were queued in the VO
    // This includes even skipped frames during hr-seek
    double vo_pts_history_pts[MAX_NUM_VO_PTS];
    // Whether the PTS at vo_pts_history[n] is after a seek reset
    uint64_t vo_pts_history_seek[MAX_NUM_VO_PTS];
    uint64_t vo_pts_history_seek_ts;
    uint64_t backstep_start_seek_ts;
    bool backstep_active;

    double next_heartbeat;
    double last_idle_tick;
    double next_cache_update;

    double sleeptime;      // number of seconds to sleep before next iteration

    double mouse_timer;
    unsigned int mouse_event_ts;
    bool mouse_cursor_visible;

    // used to prevent hanging in some error cases
    double start_timestamp;

    // Timestamp from the last time some timing functions read the
    // current time, in microseconds.
    // Used to turn a new time value to a delta from last time.
    int64_t last_time;

    // Used to communicate the parameters of a seek between parts
    struct seek_params {
        enum seek_type type;
        enum seek_precision exact;
        double amount;
        bool immediate; // disable seek delay logic
    } seek;

    /* Heuristic for relative chapter seeks: keep track which chapter
     * the user wanted to go to, even if we aren't exactly within the
     * boundaries of that chapter due to an inaccurate seek. */
    int last_chapter_seek;
    double last_chapter_pts;

    /* Subtitle renderer. This is separate, because we want to keep fonts
     * loaded across ordered chapters, instead of reloading and rescanning
     * them on each transition. (Both of these objects contain this state.)
     */
    struct ass_renderer *ass_renderer;
    struct ass_library *ass_library;
    struct mp_log *ass_log;

    int last_dvb_step;

    bool paused;
    // step this many frames, then pause
    int step_frames;
    // Counted down each frame, stop playback if 0 is reached. (-1 = disable)
    int max_frames;
    bool playing_msg_shown;

    bool paused_for_cache;
    double cache_stop_time, cache_wait_time;

    // Set after showing warning about decoding being too slow for realtime
    // playback rate. Used to avoid showing it multiple times.
    bool drop_message_shown;

    struct screenshot_ctx *screenshot_ctx;
    struct command_ctx *command_ctx;
    struct encode_lavc_context *encode_lavc_ctx;
    struct mp_nav_state *nav_state;

    struct mp_ipc_ctx *ipc_ctx;

    struct mpv_opengl_cb_context *gl_cb_ctx;
} MPContext;

// audio.c
void reset_audio_state(struct MPContext *mpctx);
void reinit_audio_chain(struct MPContext *mpctx);
int reinit_audio_filters(struct MPContext *mpctx);
double playing_audio_pts(struct MPContext *mpctx);
void fill_audio_out_buffers(struct MPContext *mpctx, double endpts);
double written_audio_pts(struct MPContext *mpctx);
void clear_audio_output_buffers(struct MPContext *mpctx);
void set_playback_speed(struct MPContext *mpctx, double new_speed);
void uninit_audio_out(struct MPContext *mpctx);
void uninit_audio_chain(struct MPContext *mpctx);

// configfiles.c
void mp_parse_cfgfiles(struct MPContext *mpctx);
void mp_load_auto_profiles(struct MPContext *mpctx);
void mp_get_resume_defaults(struct MPContext *mpctx);
void mp_load_playback_resume(struct MPContext *mpctx, const char *file);
void mp_write_watch_later_conf(struct MPContext *mpctx);
struct playlist_entry *mp_check_playlist_resume(struct MPContext *mpctx,
                                                struct playlist *playlist);

// discnav.c
void mp_nav_init(struct MPContext *mpctx);
void mp_nav_reset(struct MPContext *mpctx);
void mp_nav_destroy(struct MPContext *mpctx);
void mp_nav_user_input(struct MPContext *mpctx, char *command);
void mp_handle_nav(struct MPContext *mpctx);
int mp_nav_in_menu(struct MPContext *mpctx);
bool mp_nav_mouse_on_button(struct MPContext *mpctx);

// loadfile.c
void uninit_player(struct MPContext *mpctx, unsigned int mask);
struct track *mp_add_external_file(struct MPContext *mpctx, char *filename,
                                   enum stream_type filter);
void mp_switch_track(struct MPContext *mpctx, enum stream_type type,
                     struct track *track);
void mp_switch_track_n(struct MPContext *mpctx, int order,
                       enum stream_type type, struct track *track);
void mp_deselect_track(struct MPContext *mpctx, struct track *track);
void mp_mark_user_track_selection(struct MPContext *mpctx, int order,
                                  enum stream_type type);
struct track *mp_track_by_tid(struct MPContext *mpctx, enum stream_type type,
                              int tid);
double timeline_set_from_time(struct MPContext *mpctx, double pts, bool *need_reset);
void add_demuxer_tracks(struct MPContext *mpctx, struct demuxer *demuxer);
bool mp_remove_track(struct MPContext *mpctx, struct track *track);
struct playlist_entry *mp_next_file(struct MPContext *mpctx, int direction,
                                    bool force);
void mp_set_playlist_entry(struct MPContext *mpctx, struct playlist_entry *e);
void mp_play_files(struct MPContext *mpctx);
void update_demuxer_properties(struct MPContext *mpctx);
void print_track_list(struct MPContext *mpctx);
void reselect_demux_streams(struct MPContext *mpctx);
void prepare_playlist(struct MPContext *mpctx, struct playlist *pl);
void autoload_external_files(struct MPContext *mpctx);
struct track *select_track(struct MPContext *mpctx, enum stream_type type,
                           int tid, int ffid, char **langs);

// main.c
int mp_initialize(struct MPContext *mpctx, char **argv);
struct MPContext *mp_create(void);
void mp_destroy(struct MPContext *mpctx);
void mp_print_version(struct mp_log *log, int always);
void wakeup_playloop(void *ctx);

// misc.c
double get_start_time(struct MPContext *mpctx);
double get_main_demux_pts(struct MPContext *mpctx);
double get_track_video_offset(struct MPContext *mpctx, struct track *track);
double rel_time_to_abs(struct MPContext *mpctx, struct m_rel_time t);
double get_play_end_pts(struct MPContext *mpctx);
double get_relative_time(struct MPContext *mpctx);
void merge_playlist_files(struct playlist *pl);
float mp_get_cache_percent(struct MPContext *mpctx);
bool mp_get_cache_idle(struct MPContext *mpctx);
void update_window_title(struct MPContext *mpctx, bool force);
void error_on_track(struct MPContext *mpctx, struct track *track);
void stream_dump(struct MPContext *mpctx);
int mpctx_run_reentrant(struct MPContext *mpctx, void (*thread_fn)(void *arg),
                        void *thread_arg);
struct mpv_global *create_sub_global(struct MPContext *mpctx);

// osd.c
void set_osd_bar(struct MPContext *mpctx, int type,
                 double min, double max, double neutral, double val);
bool set_osd_msg(struct MPContext *mpctx, int level, int time,
                 const char* fmt, ...) PRINTF_ATTRIBUTE(4,5);
void set_osd_function(struct MPContext *mpctx, int osd_function);
void set_osd_subtitle(struct MPContext *mpctx, const char *text);
void get_current_osd_sym(struct MPContext *mpctx, char *buf, size_t buf_size);
void set_osd_bar_chapters(struct MPContext *mpctx, int type);

// playloop.c
void mp_wait_events(struct MPContext *mpctx, double sleeptime);
void mp_process_input(struct MPContext *mpctx);
void reset_playback_state(struct MPContext *mpctx);
void pause_player(struct MPContext *mpctx);
void unpause_player(struct MPContext *mpctx);
void add_step_frame(struct MPContext *mpctx, int dir);
void queue_seek(struct MPContext *mpctx, enum seek_type type, double amount,
                enum seek_precision exact, bool immediate);
bool mp_seek_chapter(struct MPContext *mpctx, int chapter);
double get_time_length(struct MPContext *mpctx);
double get_current_time(struct MPContext *mpctx);
double get_playback_time(struct MPContext *mpctx);
int get_percent_pos(struct MPContext *mpctx);
double get_current_pos_ratio(struct MPContext *mpctx, bool use_range);
int get_current_chapter(struct MPContext *mpctx);
char *chapter_display_name(struct MPContext *mpctx, int chapter);
char *chapter_name(struct MPContext *mpctx, int chapter);
double chapter_start_time(struct MPContext *mpctx, int chapter);
int get_chapter_count(struct MPContext *mpctx);
double get_cache_buffering_percentage(struct MPContext *mpctx);
void execute_queued_seek(struct MPContext *mpctx);
void run_playloop(struct MPContext *mpctx);
void mp_idle(struct MPContext *mpctx);
void idle_loop(struct MPContext *mpctx);
void handle_force_window(struct MPContext *mpctx, bool reconfig);
void add_frame_pts(struct MPContext *mpctx, double pts);
void seek_to_last_frame(struct MPContext *mpctx);

// scripting.c
struct mp_scripting {
    const char *file_ext;   // e.g. "lua"
    int (*load)(struct mpv_handle *client, const char *filename);
};
void mp_load_scripts(struct MPContext *mpctx);

// sub.c
void reset_subtitle_state(struct MPContext *mpctx);
void uninit_stream_sub_decoders(struct demuxer *demuxer);
void reinit_subs(struct MPContext *mpctx, int order);
void uninit_sub(struct MPContext *mpctx, int order);
void uninit_sub_all(struct MPContext *mpctx);
void update_osd_msg(struct MPContext *mpctx);
void update_subtitles(struct MPContext *mpctx);
void uninit_sub_renderer(struct MPContext *mpctx);
void update_osd_sub_state(struct MPContext *mpctx, int order,
                          struct osd_sub_state *out_state);

// video.c
void reset_video_state(struct MPContext *mpctx);
int reinit_video_chain(struct MPContext *mpctx);
int reinit_video_filters(struct MPContext *mpctx);
void write_video(struct MPContext *mpctx, double endpts);
void mp_force_video_refresh(struct MPContext *mpctx);
void uninit_video_out(struct MPContext *mpctx);
void uninit_video_chain(struct MPContext *mpctx);

#endif /* MPLAYER_MP_CORE_H */
