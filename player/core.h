/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_MP_CORE_H
#define MPLAYER_MP_CORE_H

#include <stdbool.h>
#include <pthread.h>

#include "osdep/atomic.h"

#include "libmpv/client.h"

#include "common/common.h"
#include "filters/filter.h"
#include "filters/f_output_chain.h"
#include "options/options.h"
#include "sub/osd.h"
#include "audio/aframe.h"
#include "video/mp_image.h"
#include "video/out/vo.h"

// definitions used internally by the core player code

enum stop_play_reason {
    KEEP_PLAYING = 0,   // playback of a file is actually going on
                        // must be 0, numeric values of others do not matter
    AT_END_OF_FILE,     // file has ended, prepare to play next
                        // also returned on unrecoverable playback errors
    PT_NEXT_ENTRY,      // prepare to play next entry in playlist
    PT_CURRENT_ENTRY,   // prepare to play mpctx->playlist->current
    PT_STOP,            // stop playback / idle mode
    PT_QUIT,            // stop playback, quit player
    PT_ERROR,           // play next playlist entry (due to an error)
};

enum mp_osd_seek_info {
    OSD_SEEK_INFO_BAR           = 1,
    OSD_SEEK_INFO_TEXT          = 2,
    OSD_SEEK_INFO_CHAPTER_TEXT  = 4,
    OSD_SEEK_INFO_CURRENT_FILE  = 8,
};


enum {
    // other constants
    OSD_LEVEL_INVISIBLE = 4,
    OSD_BAR_SEEK = 256,

    MAX_NUM_VO_PTS = 100,
};

enum seek_type {
    MPSEEK_NONE = 0,
    MPSEEK_RELATIVE,
    MPSEEK_ABSOLUTE,
    MPSEEK_FACTOR,
    MPSEEK_BACKSTEP,
};

enum seek_precision {
    // The following values are numerically sorted by increasing precision
    MPSEEK_DEFAULT = 0,
    MPSEEK_KEYFRAME,
    MPSEEK_EXACT,
    MPSEEK_VERY_EXACT,
};

enum seek_flags {
    MPSEEK_FLAG_DELAY = 1 << 0, // give player chance to coalesce multiple seeks
    MPSEEK_FLAG_NOFLUSH = 1 << 1, // keeping remaining data for seamless loops
};

struct seek_params {
    enum seek_type type;
    enum seek_precision exact;
    double amount;
    unsigned flags; // MPSEEK_FLAG_*
};

enum video_sync {
    VS_DEFAULT = 0,
    VS_DISP_RESAMPLE,
    VS_DISP_RESAMPLE_VDROP,
    VS_DISP_RESAMPLE_NONE,
    VS_DISP_ADROP,
    VS_DISP_VDROP,
    VS_DISP_NONE,
    VS_NONE,
};

#define VS_IS_DISP(x) ((x) == VS_DISP_RESAMPLE ||       \
                       (x) == VS_DISP_RESAMPLE_VDROP || \
                       (x) == VS_DISP_RESAMPLE_NONE ||  \
                       (x) == VS_DISP_ADROP ||          \
                       (x) == VS_DISP_VDROP ||          \
                       (x) == VS_DISP_NONE)

// Information about past video frames that have been sent to the VO.
struct frame_info {
    double pts;
    double duration;        // PTS difference to next frame
    double approx_duration; // possibly fixed/smoothed out duration
    double av_diff;         // A/V diff at time of scheduling
    int num_vsyncs;         // scheduled vsyncs, if using display-sync
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
    bool default_track, forced_track, dependent_track;
    bool visual_impaired_track, hearing_impaired_track;
    bool image;
    bool attached_picture;
    char *lang;

    // If this track is from an external file (e.g. subtitle file).
    bool is_external;
    bool no_default;            // pretend it's not external for auto-selection
    bool no_auto_select;
    char *external_filename;
    bool auto_loaded;

    struct demuxer *demuxer;
    // Invariant: !stream || stream->demuxer == demuxer
    struct sh_stream *stream;

    // Current subtitle state (or cached state if selected==false).
    struct dec_sub *d_sub;

    // Current decoding state (NULL if selected==false)
    struct mp_decoder_wrapper *dec;

    // Where the decoded result goes to (one of them is not NULL if active)
    struct vo_chain *vo_c;
    struct ao_chain *ao_c;
    struct mp_pin *sink;

    // For stream recording (remuxing mode).
    struct mp_recorder_sink *remux_sink;
};

// Summarizes video filtering and output.
struct vo_chain {
    struct mp_log *log;

    struct mp_output_chain *filter;

    struct vo *vo;

    struct track *track;
    struct mp_pin *filter_src;
    struct mp_pin *dec_src;

    // - video consists of a single picture, which should be shown only once
    // - do not sync audio to video in any way
    bool is_coverart;
    // - video consists of sparse still images
    bool is_sparse;

    bool underrun;
    bool underrun_signaled;
};

// Like vo_chain, for audio.
struct ao_chain {
    struct mp_log *log;
    struct MPContext *mpctx;

    bool spdif_passthrough, spdif_failed;

    struct mp_output_chain *filter;

    struct ao *ao;
    struct mp_async_queue *ao_queue;
    struct mp_filter *queue_filter;
    struct mp_filter *ao_filter;
    double ao_resume_time;

    bool out_eof;
    double last_out_pts;

    double start_pts;
    bool start_pts_known;

    struct track *track;
    struct mp_pin *filter_src;
    struct mp_pin *dec_src;

    double delay;
    bool untimed_throttle;

    bool ao_underrun;   // last known AO state
    bool underrun;      // for cache pause logic
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
    STATUS_READY,       // buffers full, playback can be started any time
    STATUS_PLAYING,     // normal playback
    STATUS_DRAINING,    // decoding has ended; still playing out queued buffers
    STATUS_EOF,         // playback has ended, or is disabled
};

const char *mp_status_str(enum playback_status st);

extern const int num_ptracks[STREAM_TYPE_COUNT];

// Maximum of all num_ptracks[] values.
#define MAX_PTRACKS 2

typedef struct MPContext {
    bool initialized;
    bool is_cli;
    struct mpv_global *global;
    struct MPOpts *opts;
    struct mp_log *log;
    struct stats_ctx *stats;
    struct m_config *mconfig;
    struct input_ctx *input;
    struct mp_client_api *clients;
    struct mp_dispatch_queue *dispatch;
    struct mp_cancel *playback_abort;
    // Number of asynchronous tasks that still need to finish until MPContext
    // destruction is ok. It's implied that the async tasks call
    // mp_wakeup_core() each time this is decremented.
    // As using an atomic+wakeup would be racy, this is a normal integer, and
    // mp_dispatch_lock must be called to change it.
    int64_t outstanding_async;

    struct mp_thread_pool *thread_pool; // for coarse I/O, often during loading

    struct mp_log *statusline;
    struct osd_state *osd;
    char *term_osd_text;
    char *term_osd_status;
    char *term_osd_subs;
    char *term_osd_contents;
    char *term_osd_title;
    char *last_window_title;
    struct voctrl_playback_state vo_playback_state;

    int add_osd_seek_info; // bitfield of enum mp_osd_seek_info
    double osd_visible; // for the osd bar only
    int osd_function;
    double osd_function_visible;
    double osd_msg_visible;
    double osd_msg_next_duration;
    double osd_last_update;
    bool osd_force_update, osd_idle_update;
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

    // Global file statistics
    int files_played;       // played without issues (even if stopped by user)
    int files_errored;      // played, but errors happened at one point
    int files_broken;       // couldn't be played at all

    // Current file statistics
    int64_t shown_vframes, shown_aframes;

    struct demux_chapter *chapters;
    int num_chapters;

    struct demuxer *demuxer;
    struct mp_tags *filtered_tags;

    struct track **tracks;
    int num_tracks;

    char *track_layout_hash;

    // Selected tracks. NULL if no track selected.
    // There can be num_ptracks[type] of the same STREAM_TYPE selected at once.
    // Currently, this is used for the secondary subtitle track only.
    struct track *current_track[MAX_PTRACKS][STREAM_TYPE_COUNT];

    struct mp_filter *filter_root;

    struct mp_filter *lavfi;
    char *lavfi_graph;

    struct ao *ao;
    struct mp_aframe *ao_filter_fmt; // for weak gapless audio check
    struct ao_chain *ao_chain;

    struct vo_chain *vo_chain;

    struct vo *video_out;
    // next_frame[0] is the next frame, next_frame[1] the one after that.
    // The +1 is for adding 1 additional frame in backstep mode.
    struct mp_image *next_frames[VO_MAX_REQ_FRAMES + 1];
    int num_next_frames;
    struct mp_image *saved_frame;   // for hrseek_lastframe and hrseek_backstep

    enum playback_status video_status, audio_status;
    bool restart_complete;
    int play_dir;
    // Factors to multiply with opts->playback_speed to get the total audio or
    // video speed (usually 1.0, but can be set to by the sync code).
    double speed_factor_v, speed_factor_a;
    // Redundant values set from opts->playback_speed and speed_factor_*.
    // update_playback_speed() updates them from the other fields.
    double audio_speed, video_speed;
    bool display_sync_active;
    int display_sync_drift_dir;
    // Timing error (in seconds) due to rounding on vsync boundaries
    double display_sync_error;
    // Number of mistimed frames.
    int mistimed_frames_total;
    bool hrseek_active;     // skip all data until hrseek_pts
    bool hrseek_lastframe;  // drop everything until last frame reached
    bool hrseek_backstep;   // go to frame before seek target
    double hrseek_pts;
    struct seek_params current_seek;
    bool ab_loop_clip;      // clip to the "b" part of an A-B loop if available
    // AV sync: the next frame should be shown when the audio out has this
    // much (in seconds) buffered data left. Increased when more data is
    // written to the ao, decreased when moving to the next video frame.
    double delay;
    // AV sync: time in seconds until next frame should be shown
    double time_frame;
    // How much video timing has been changed to make it match the audio
    // timeline. Used for status line information only.
    double total_avsync_change;
    // A-V sync difference when last frame was displayed. Kept to display
    // the same value if the status line is updated at a time where no new
    // video frame is shown.
    double last_av_difference;
    /* timestamp of video frame currently visible on screen
     * (or at least queued to be flipped by VO) */
    double video_pts;
    // Last seek target.
    double last_seek_pts;
    // Frame duration field from demuxer. Only used for duration of the last
    // video frame.
    double last_frame_duration;
    // Video PTS, or audio PTS if video has ended.
    double playback_pts;
    // For logging only.
    double logged_async_diff;

    int last_chapter;

    // Past timestamps etc.
    // The newest frame is at index 0.
    struct frame_info *past_frames;
    int num_past_frames;

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

    struct seek_params seek;

    /* Heuristic for relative chapter seeks: keep track which chapter
     * the user wanted to go to, even if we aren't exactly within the
     * boundaries of that chapter due to an inaccurate seek. */
    int last_chapter_seek;
    double last_chapter_pts;

    bool paused;            // internal pause state
    bool playback_active;   // not paused, restarting, loading, unloading
    bool in_playloop;

    // step this many frames, then pause
    int step_frames;
    // Counted down each frame, stop playback if 0 is reached. (-1 = disable)
    int max_frames;
    bool playing_msg_shown;

    bool paused_for_cache;
    bool demux_underrun;
    double cache_stop_time;
    int cache_buffer;
    double cache_update_pts;

    // Set after showing warning about decoding being too slow for realtime
    // playback rate. Used to avoid showing it multiple times.
    bool drop_message_shown;

    struct mp_recorder *recorder;

    char *cached_watch_later_configdir;

    struct screenshot_ctx *screenshot_ctx;
    struct command_ctx *command_ctx;
    struct encode_lavc_context *encode_lavc_ctx;

    struct mp_ipc_ctx *ipc_ctx;

    int64_t builtin_script_ids[5];

    pthread_mutex_t abort_lock;

    // --- The following fields are protected by abort_lock
    struct mp_abort_entry **abort_list;
    int num_abort_list;
    bool abort_all; // during final termination

    // --- Owned by MPContext
    pthread_t open_thread;
    bool open_active; // open_thread is a valid thread handle, all setup
    atomic_bool open_done;
    // --- All fields below are immutable while open_active is true.
    //     Otherwise, they're owned by MPContext.
    struct mp_cancel *open_cancel;
    char *open_url;
    char *open_format;
    int open_url_flags;
    bool open_for_prefetch;
    // --- All fields below are owned by open_thread, unless open_done was set
    //     to true.
    struct demuxer *open_res_demuxer;
    int open_res_error;
} MPContext;

// Contains information about an asynchronous work item, how it can be aborted,
// and when. All fields are protected by MPContext.abort_lock.
struct mp_abort_entry {
    // General conditions.
    bool coupled_to_playback;   // trigger when playback is terminated
    // Actual trigger to abort the work. Pointer immutable, owner may access
    // without holding the abort_lock.
    struct mp_cancel *cancel;
    // For client API.
    struct mpv_handle *client;  // non-NULL if done by a client API user
    int client_work_type;       // client API type, e.h. MPV_EVENT_COMMAND_REPLY
    uint64_t client_work_id;    // client API user reply_userdata value
                                // (only valid if client_work_type set)
};

// audio.c
void reset_audio_state(struct MPContext *mpctx);
void reinit_audio_chain(struct MPContext *mpctx);
int init_audio_decoder(struct MPContext *mpctx, struct track *track);
int reinit_audio_filters(struct MPContext *mpctx);
double playing_audio_pts(struct MPContext *mpctx);
void fill_audio_out_buffers(struct MPContext *mpctx);
double written_audio_pts(struct MPContext *mpctx);
void clear_audio_output_buffers(struct MPContext *mpctx);
void update_playback_speed(struct MPContext *mpctx);
void uninit_audio_out(struct MPContext *mpctx);
void uninit_audio_chain(struct MPContext *mpctx);
int init_audio_decoder(struct MPContext *mpctx, struct track *track);
void reinit_audio_chain_src(struct MPContext *mpctx, struct track *track);
void audio_update_volume(struct MPContext *mpctx);
void audio_update_balance(struct MPContext *mpctx);
void reload_audio_output(struct MPContext *mpctx);
void audio_start_ao(struct MPContext *mpctx);

// configfiles.c
void mp_parse_cfgfiles(struct MPContext *mpctx);
void mp_load_auto_profiles(struct MPContext *mpctx);
void mp_load_playback_resume(struct MPContext *mpctx, const char *file);
void mp_write_watch_later_conf(struct MPContext *mpctx);
void mp_delete_watch_later_conf(struct MPContext *mpctx, const char *file);
struct playlist_entry *mp_check_playlist_resume(struct MPContext *mpctx,
                                                struct playlist *playlist);

// loadfile.c
void mp_abort_playback_async(struct MPContext *mpctx);
void mp_abort_add(struct MPContext *mpctx, struct mp_abort_entry *abort);
void mp_abort_remove(struct MPContext *mpctx, struct mp_abort_entry *abort);
void mp_abort_recheck_locked(struct MPContext *mpctx,
                             struct mp_abort_entry *abort);
void mp_abort_trigger_locked(struct MPContext *mpctx,
                             struct mp_abort_entry *abort);
void uninit_player(struct MPContext *mpctx, unsigned int mask);
int mp_add_external_file(struct MPContext *mpctx, char *filename,
                         enum stream_type filter, struct mp_cancel *cancel,
                         bool cover_art);
void mark_track_selection(struct MPContext *mpctx, int order,
                          enum stream_type type, int value);
#define FLAG_MARK_SELECTION 1
void mp_switch_track(struct MPContext *mpctx, enum stream_type type,
                     struct track *track, int flags);
void mp_switch_track_n(struct MPContext *mpctx, int order,
                       enum stream_type type, struct track *track, int flags);
void mp_deselect_track(struct MPContext *mpctx, struct track *track);
struct track *mp_track_by_tid(struct MPContext *mpctx, enum stream_type type,
                              int tid);
void add_demuxer_tracks(struct MPContext *mpctx, struct demuxer *demuxer);
bool mp_remove_track(struct MPContext *mpctx, struct track *track);
struct playlist_entry *mp_next_file(struct MPContext *mpctx, int direction,
                                    bool force, bool mutate);
void mp_set_playlist_entry(struct MPContext *mpctx, struct playlist_entry *e);
void mp_play_files(struct MPContext *mpctx);
void update_demuxer_properties(struct MPContext *mpctx);
void print_track_list(struct MPContext *mpctx, const char *msg);
void reselect_demux_stream(struct MPContext *mpctx, struct track *track,
                           bool refresh_only);
void prepare_playlist(struct MPContext *mpctx, struct playlist *pl);
void autoload_external_files(struct MPContext *mpctx, struct mp_cancel *cancel);
struct track *select_default_track(struct MPContext *mpctx, int order,
                                   enum stream_type type);
void prefetch_next(struct MPContext *mpctx);
void close_recorder(struct MPContext *mpctx);
void close_recorder_and_error(struct MPContext *mpctx);
void open_recorder(struct MPContext *mpctx, bool on_init);
void update_lavfi_complex(struct MPContext *mpctx);

// main.c
int mp_initialize(struct MPContext *mpctx, char **argv);
struct MPContext *mp_create(void);
void mp_destroy(struct MPContext *mpctx);
void mp_print_version(struct mp_log *log, int always);
void mp_update_logging(struct MPContext *mpctx, bool preinit);
void issue_refresh_seek(struct MPContext *mpctx, enum seek_precision min_prec);

// misc.c
double rel_time_to_abs(struct MPContext *mpctx, struct m_rel_time t);
double get_play_end_pts(struct MPContext *mpctx);
double get_play_start_pts(struct MPContext *mpctx);
bool get_ab_loop_times(struct MPContext *mpctx, double t[2]);
void merge_playlist_files(struct playlist *pl);
void update_vo_playback_state(struct MPContext *mpctx);
void update_window_title(struct MPContext *mpctx, bool force);
void error_on_track(struct MPContext *mpctx, struct track *track);
int stream_dump(struct MPContext *mpctx, const char *source_filename);
double get_track_seek_offset(struct MPContext *mpctx, struct track *track);

// osd.c
void set_osd_bar(struct MPContext *mpctx, int type,
                 double min, double max, double neutral, double val);
bool set_osd_msg(struct MPContext *mpctx, int level, int time,
                 const char* fmt, ...) PRINTF_ATTRIBUTE(4,5);
void set_osd_function(struct MPContext *mpctx, int osd_function);
void term_osd_set_subs(struct MPContext *mpctx, const char *text);
void get_current_osd_sym(struct MPContext *mpctx, char *buf, size_t buf_size);
void set_osd_bar_chapters(struct MPContext *mpctx, int type);

// playloop.c
void mp_wait_events(struct MPContext *mpctx);
void mp_set_timeout(struct MPContext *mpctx, double sleeptime);
void mp_wakeup_core(struct MPContext *mpctx);
void mp_wakeup_core_cb(void *ctx);
void mp_core_lock(struct MPContext *mpctx);
void mp_core_unlock(struct MPContext *mpctx);
double get_relative_time(struct MPContext *mpctx);
void reset_playback_state(struct MPContext *mpctx);
void set_pause_state(struct MPContext *mpctx, bool user_pause);
void update_internal_pause_state(struct MPContext *mpctx);
void update_core_idle_state(struct MPContext *mpctx);
void add_step_frame(struct MPContext *mpctx, int dir);
void queue_seek(struct MPContext *mpctx, enum seek_type type, double amount,
                enum seek_precision exact, int flags);
double get_time_length(struct MPContext *mpctx);
double get_start_time(struct MPContext *mpctx, int dir);
double get_current_time(struct MPContext *mpctx);
double get_playback_time(struct MPContext *mpctx);
int get_percent_pos(struct MPContext *mpctx);
double get_current_pos_ratio(struct MPContext *mpctx, bool use_range);
int get_current_chapter(struct MPContext *mpctx);
char *chapter_display_name(struct MPContext *mpctx, int chapter);
char *chapter_name(struct MPContext *mpctx, int chapter);
double chapter_start_time(struct MPContext *mpctx, int chapter);
int get_chapter_count(struct MPContext *mpctx);
int get_cache_buffering_percentage(struct MPContext *mpctx);
void execute_queued_seek(struct MPContext *mpctx);
void run_playloop(struct MPContext *mpctx);
void mp_idle(struct MPContext *mpctx);
void idle_loop(struct MPContext *mpctx);
int handle_force_window(struct MPContext *mpctx, bool force);
void seek_to_last_frame(struct MPContext *mpctx);
void update_screensaver_state(struct MPContext *mpctx);
void update_ab_loop_clip(struct MPContext *mpctx);
bool get_internal_paused(struct MPContext *mpctx);

// scripting.c
struct mp_script_args {
    const struct mp_scripting *backend;
    struct MPContext *mpctx;
    struct mp_log *log;
    struct mpv_handle *client;
    const char *filename;
    const char *path;
};
struct mp_scripting {
    const char *name;       // e.g. "lua script"
    const char *file_ext;   // e.g. "lua"
    bool no_thread;         // don't run load() on dedicated thread
    int (*load)(struct mp_script_args *args);
};
bool mp_load_scripts(struct MPContext *mpctx);
void mp_load_builtin_scripts(struct MPContext *mpctx);
int64_t mp_load_user_script(struct MPContext *mpctx, const char *fname);

// sub.c
void reset_subtitle_state(struct MPContext *mpctx);
void reinit_sub(struct MPContext *mpctx, struct track *track);
void reinit_sub_all(struct MPContext *mpctx);
void uninit_sub(struct MPContext *mpctx, struct track *track);
void uninit_sub_all(struct MPContext *mpctx);
void update_osd_msg(struct MPContext *mpctx);
bool update_subtitles(struct MPContext *mpctx, double video_pts);

// video.c
int video_get_colors(struct vo_chain *vo_c, const char *item, int *value);
int video_set_colors(struct vo_chain *vo_c, const char *item, int value);
void reset_video_state(struct MPContext *mpctx);
int init_video_decoder(struct MPContext *mpctx, struct track *track);
void reinit_video_chain(struct MPContext *mpctx);
void reinit_video_chain_src(struct MPContext *mpctx, struct track *track);
int reinit_video_filters(struct MPContext *mpctx);
void write_video(struct MPContext *mpctx);
void mp_force_video_refresh(struct MPContext *mpctx);
void uninit_video_out(struct MPContext *mpctx);
void uninit_video_chain(struct MPContext *mpctx);
double calc_average_frame_duration(struct MPContext *mpctx);
int init_video_decoder(struct MPContext *mpctx, struct track *track);

#endif /* MPLAYER_MP_CORE_H */
