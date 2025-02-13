#ifndef MPLAYER_OPTIONS_H
#define MPLAYER_OPTIONS_H

#include <stdbool.h>
#include <stdint.h>
#include "m_option.h"
#include "common/common.h"

typedef struct mp_vo_opts {
    struct m_obj_settings *video_driver_list;

    bool taskbar_progress;
    bool snap_window;
    int drag_and_drop;
    bool ontop;
    int ontop_level;
    bool fullscreen;
    bool border;
    bool title_bar;
    bool all_workspaces;
    bool window_minimized;
    bool window_maximized;
    int focus_on;

    int screen_id;
    char *screen_name;
    int fsscreen_id;
    char *fsscreen_name;
    char *winname;
    char *appid;
    int x11_netwm;
    int x11_bypass_compositor;
    int x11_present;
    bool x11_wid_title;
    bool cursor_passthrough;
    bool native_keyrepeat;
    bool input_ime;

    int wl_configure_bounds;
    int wl_content_type;
    bool wl_disable_vsync;
    int wl_edge_pixels_pointer;
    int wl_edge_pixels_touch;
    bool wl_present;

    float panscan;
    float zoom;
    float pan_x, pan_y;
    float align_x, align_y;
    float scale_x, scale_y;
    float margin_x[2];
    float margin_y[2];
    int unscaled;

    struct m_geometry geometry;
    struct m_geometry autofit;
    struct m_geometry autofit_larger;
    struct m_geometry autofit_smaller;
    double window_scale;

    bool auto_window_resize;
    bool recenter;
    bool keepaspect;
    bool keepaspect_window;
    bool hidpi_window_scale;
    bool native_fs;
    bool native_touch;
    bool show_in_taskbar;

    int64_t WinID;

    float force_monitor_aspect;
    float monitor_pixel_aspect;
    bool force_render;
    bool force_window_position;

    int backdrop_type;
    int window_affinity;
    char *mmcss_profile;
    int window_corners;

    double display_fps_override;
    double timing_offset;
    int video_sync;

    struct m_geometry android_surface_size;

    int swapchain_depth;  // max number of images to render ahead

    struct m_geometry video_crop;
} mp_vo_opts;

// Subtitle options needed by the subtitle decoders/renderers.
struct mp_subtitle_opts {
    float sub_fps;
    float sub_speed;
    bool sub_forced_events_only;
    bool stretch_dvd_subs;
    bool stretch_image_subs;
    bool image_subs_video_res;
    bool sub_fix_timing;
    bool sub_stretch_durations;
    bool sub_scale_by_window;
    bool sub_scale_with_window;
    bool ass_scale_with_window;
    struct osd_style_opts *sub_style;
    float sub_scale;
    bool sub_scale_signs;
    float sub_gauss;
    bool sub_gray;
    bool ass_enabled;
    float sub_line_spacing;
    bool ass_use_margins;
    bool sub_use_margins;
    int ass_vsfilter_color_compat;
    bool sub_vsfilter_bidi_compat;
    int ass_use_video_data;
    double ass_video_aspect;
    bool use_embedded_fonts;
    char **ass_style_override_list;
    char *ass_styles_file;
    int sub_hinting;
    int sub_shaper;
    double ass_prune_delay;
    bool ass_justify;
    bool sub_clear_on_seek;
    int teletext_page;
    bool sub_past_video_end;
    char **sub_avopts;
};

// Options for both primary and secondary subs.
struct mp_subtitle_shared_opts {
    float sub_delay[2];
    float sub_pos[2];
    bool sub_visibility[2];
    int ass_style_override[2];
};

struct mp_osd_render_opts {
    float osd_scale;
    bool osd_scale_by_window;
    struct m_color osd_selected_color;
    struct m_color osd_selected_outline_color;
    struct osd_style_opts *osd_style;
    struct osd_bar_style_opts *osd_bar_style;
    bool force_rgba_osd;
};

typedef struct MPOpts {
    bool property_print_help;
    bool use_terminal;
    char *dump_stats;
    int verbose;
    bool msg_really_quiet;
    char **msg_levels;
    bool msg_color;
    bool msg_module;
    bool msg_time;
    char *log_file;

    int operation_mode;

    char **reset_options;
    char **script_files;
    char **script_opts;
    bool js_memory_report;
    bool lua_load_osc;
    bool lua_load_ytdl;
    char *lua_ytdl_format;
    char **lua_ytdl_raw_options;
    bool lua_load_stats;
    bool lua_load_console;
    int lua_load_auto_profiles;
    bool lua_load_select;

    bool auto_load_scripts;

    bool audio_exclusive;
    bool ao_null_fallback;
    bool audio_stream_silence;
    float audio_wait_open;
    int force_vo;
    float softvol_volume;
    int rgain_mode;
    float rgain_preamp;         // Set replaygain pre-amplification
    bool rgain_clip;             // Enable/disable clipping prevention
    float rgain_fallback;
    bool softvol_mute;
    float softvol_max;
    float softvol_gain;
    float softvol_gain_min;
    float softvol_gain_max;
    int gapless_audio;

    mp_vo_opts *vo;
    struct ao_opts *ao_opts;

    char *wintitle;
    char *media_title;

    struct mp_csp_equalizer_opts *video_equalizer;

    int stop_screensaver;
    int cursor_autohide_delay;
    bool cursor_autohide_fs;

    struct mp_subtitle_opts *subs_rend;
    struct mp_subtitle_shared_opts *subs_shared;
    struct mp_sub_filter_opts *subs_filt;
    struct mp_osd_render_opts *osd_rend;

    int osd_level;
    int osd_duration;
    bool osd_fractions;
    int osd_on_seek;
    bool video_osd;

    bool untimed;
    char *stream_dump;
    bool stop_playback_on_init_failure;
    int loop_times;
    int loop_file;
    bool shuffle;
    bool ordered_chapters;
    char *ordered_chapters_files;
    int chapter_merge_threshold;
    double chapter_seek_threshold;
    char *chapter_file;
    bool merge_files;
    bool quiet;
    bool load_config;
    char *force_configdir;
    bool use_filedir_conf;
    int hls_bitrate;
    int edition_id;
    bool initial_audio_sync;
    double sync_max_video_change;
    double sync_max_audio_change;
    int sync_max_factor;
    int hr_seek;
    float hr_seek_demuxer_offset;
    bool hr_seek_framedrop;
    float audio_delay;
    float default_max_pts_correction;
    int autosync;
    int frame_dropping;
    bool video_latency_hacks;
    int term_osd;
    bool term_osd_bar;
    char *term_osd_bar_chars;
    char *term_title;
    char *playing_msg;
    char *osd_playing_msg;
    int osd_playing_msg_duration;
    char *status_msg;
    char *osd_status_msg;
    char *osd_msg[3];
    int playlist_entry_name;
    int player_idle_mode;
    char **input_commands;
    bool consolecontrols;
    int playlist_pos;
    struct m_rel_time play_start;
    struct m_rel_time play_end;
    struct m_rel_time play_length;
    int play_dir;
    bool rebase_start_time;
    int play_frames;
    double ab_loop[2];
    int ab_loop_count;
    double step_sec;
    bool position_resume;
    bool position_check_mtime;
    bool position_save_on_quit;
    bool write_filename_in_watch_later_config;
    bool ignore_path_in_watch_later_config;
    char *watch_later_dir;
    char **watch_later_options;
    bool save_watch_history;
    char *watch_history_path;
    bool pause;
    int keep_open;
    bool keep_open_pause;
    double image_display_duration;
    char *lavfi_complex;
    int stream_id[2][STREAM_TYPE_COUNT];
    char **stream_lang[STREAM_TYPE_COUNT];
    bool stream_auto_sel;
    int subs_with_matching_audio;
    bool subs_match_os_language;
    int subs_fallback;
    int subs_fallback_forced;
    int audio_display;
    char **display_tags;

    char **audio_files;
    char *demuxer_name;
    bool demuxer_thread;
    double demux_termination_timeout;
    bool demuxer_cache_wait;
    bool prefetch_open;
    char *audio_demuxer_name;
    char *sub_demuxer_name;

    bool cache_pause;
    bool cache_pause_initial;
    float cache_pause_wait;

    struct image_writer_opts *screenshot_image_opts;
    char *screenshot_template;
    char *screenshot_dir;
    bool screenshot_sw;

    struct m_channels audio_output_channels;
    int audio_output_format;
    int force_srate;
    double playback_speed;
    double playback_pitch;
    bool pitch_correction;
    struct m_obj_settings *vf_settings;
    struct m_obj_settings *af_settings;
    struct filter_opts *filter_opts;
    struct dec_wrapper_opts *dec_wrapper;
    char **sub_name;
    char **sub_paths;
    char **audiofile_paths;
    char **coverart_files;
    char **external_files;
    bool autoload_files;
    int sub_file_priority;
    int audio_file_priority;
    int video_file_priority;
    int sub_auto;
    char **sub_auto_exts;
    int audiofile_auto;
    char **audio_exts;
    int coverart_auto;
    char **image_exts;
    char **coverart_whitelist;
    char **video_exts;
    char **archive_exts;
    char **playlist_exts;
    bool osd_bar_visible;

    int w32_priority;
    bool media_controls;

    struct bluray_opts *stream_bluray_opts;
    struct cdda_opts *stream_cdda_opts;
    struct dvb_opts *stream_dvb_opts;
    struct lavf_opts *stream_lavf_opts;

    char *bluray_device;

    struct demux_rawaudio_opts *demux_rawaudio;
    struct demux_rawvideo_opts *demux_rawvideo;
    struct demux_playlist_opts *demux_playlist;
    struct demux_lavf_opts *demux_lavf;
    struct demux_mkv_opts *demux_mkv;

    struct demux_opts *demux_opts;
    struct demux_cache_opts *demux_cache_opts;
    struct stream_opts *stream_opts;

    struct vd_lavc_params *vd_lavc_params;
    struct ad_lavc_params *ad_lavc_params;

    struct hwdec_opts *hwdec_opts;

    struct input_opts *input_opts;

    struct clipboard_opts *clipboard_opts;

    struct encode_opts *encode_opts;

    char *ipc_path;
    char *ipc_client;

    struct mp_resample_opts *resample_opts;

    struct ra_ctx_opts *ra_ctx_opts;
    struct gl_video_opts *gl_video_opts;
    struct gl_next_opts *gl_next_opts;
    struct angle_opts *angle_opts;
    struct opengl_opts *opengl_opts;
    struct vulkan_opts *vulkan_opts;
    struct vulkan_display_opts *vulkan_display_opts;
    struct spirv_opts *spirv_opts;
    struct d3d11_opts *d3d11_opts;
    struct d3d11va_opts *d3d11va_opts;
    struct macos_opts *macos_opts;
    struct drm_opts *drm_opts;
    struct wingl_opts *wingl_opts;
    struct cuda_opts *cuda_opts;
    struct dvd_opts *dvd_opts;
    struct vaapi_opts *vaapi_opts;
    struct sws_opts *sws_opts;
    struct zimg_opts *zimg_opts;
    struct egl_opts *egl_opts;

    int cuda_device;
} MPOpts;

struct cuda_opts {
    int cuda_device;
};

struct filter_opts {
    int deinterlace;
    int field_parity;
};

extern const struct m_sub_options vo_sub_opts;
extern const struct m_sub_options cuda_conf;
extern const struct m_sub_options mp_subtitle_sub_opts;
extern const struct m_sub_options mp_subtitle_shared_sub_opts;
extern const struct m_sub_options mp_osd_render_sub_opts;
extern const struct m_sub_options filter_conf;
extern const struct m_sub_options resample_conf;
extern const struct m_sub_options stream_conf;
extern const struct m_sub_options dec_wrapper_conf;
extern const struct m_sub_options mp_opt_root;

#endif
