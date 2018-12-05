#ifndef MPLAYER_OPTIONS_H
#define MPLAYER_OPTIONS_H

#include <stdbool.h>
#include <stdint.h>
#include "m_option.h"
#include "common/common.h"

typedef struct mp_vo_opts {
    struct m_obj_settings *video_driver_list;

    int taskbar_progress;
    int snap_window;
    int ontop;
    int ontop_level;
    int fullscreen;
    int border;
    int fit_border;
    int all_workspaces;

    int screen_id;
    int fsscreen_id;
    char *winname;
    int x11_netwm;
    int x11_bypass_compositor;
    int native_keyrepeat;

    float panscan;
    float zoom;
    float pan_x, pan_y;
    float align_x, align_y;
    int unscaled;

    struct m_geometry geometry;
    struct m_geometry autofit;
    struct m_geometry autofit_larger;
    struct m_geometry autofit_smaller;
    double window_scale;

    int keepaspect;
    int keepaspect_window;
    int hidpi_window_scale;
    int native_fs;

    int64_t WinID;

    float force_monitor_aspect;
    float monitor_pixel_aspect;
    int force_window_position;

    char *mmcss_profile;

    double override_display_fps;
    double timing_offset;

    // vo_drm
    struct sws_opts *sws_opts;
    // vo_drm
    struct drm_opts *drm_opts;
} mp_vo_opts;

// Subtitle options needed by the subtitle decoders/renderers.
struct mp_subtitle_opts {
    int sub_visibility;
    int sub_pos;
    float sub_delay;
    float sub_fps;
    float sub_speed;
    int forced_subs_only;
    int stretch_dvd_subs;
    int stretch_image_subs;
    int image_subs_video_res;
    int sub_fix_timing;
    int sub_scale_by_window;
    int sub_scale_with_window;
    int ass_scale_with_window;
    struct osd_style_opts *sub_style;
    float sub_scale;
    float sub_gauss;
    int sub_gray;
    int sub_filter_SDH;
    int sub_filter_SDH_harder;
    int ass_enabled;
    float ass_line_spacing;
    int ass_use_margins;
    int sub_use_margins;
    int ass_vsfilter_aspect_compat;
    int ass_vsfilter_color_compat;
    int ass_vsfilter_blur_compat;
    int use_embedded_fonts;
    char **ass_force_style_list;
    char *ass_styles_file;
    int ass_style_override;
    int ass_hinting;
    int ass_shaper;
    int ass_justify;
    int sub_clear_on_seek;
    int teletext_page;
};

struct mp_osd_render_opts {
    float osd_bar_align_x;
    float osd_bar_align_y;
    float osd_bar_w;
    float osd_bar_h;
    float osd_scale;
    int osd_scale_by_window;
    struct osd_style_opts *osd_style;
    int force_rgba_osd;
};

typedef struct MPOpts {
    int property_print_help;
    int use_terminal;
    char *dump_stats;
    int verbose;
    int msg_really_quiet;
    char **msg_levels;
    int msg_color;
    int msg_module;
    int msg_time;
    char *log_file;

    int operation_mode;

    char **reset_options;
    char **script_files;
    char **script_opts;
    int lua_load_osc;
    int lua_load_ytdl;
    char *lua_ytdl_format;
    char **lua_ytdl_raw_options;
    int lua_load_stats;

    int auto_load_scripts;

    int audio_exclusive;
    int ao_null_fallback;
    int audio_stream_silence;
    float audio_wait_open;
    int force_vo;
    float softvol_volume;
    int rgain_mode;
    float rgain_preamp;         // Set replaygain pre-amplification
    int rgain_clip;             // Enable/disable clipping prevention
    float rgain_fallback;
    int softvol_mute;
    float softvol_max;
    int gapless_audio;

    mp_vo_opts *vo;
    struct ao_opts *ao_opts;

    char *wintitle;
    char *media_title;

    struct mp_csp_equalizer_opts *video_equalizer;

    int stop_screensaver;
    int cursor_autohide_delay;
    int cursor_autohide_fs;

    int video_rotate;

    char *audio_decoders;
    char *video_decoders;
    char *audio_spdif;

    struct mp_subtitle_opts *subs_rend;
    struct mp_osd_render_opts *osd_rend;

    int osd_level;
    int osd_duration;
    int osd_fractions;
    int osd_on_seek;
    int video_osd;

    int untimed;
    char *stream_dump;
    char *record_file;
    int stop_playback_on_init_failure;
    int loop_times;
    int loop_file;
    int shuffle;
    int ordered_chapters;
    char *ordered_chapters_files;
    int chapter_merge_threshold;
    double chapter_seek_threshold;
    char *chapter_file;
    int load_unsafe_playlists;
    int merge_files;
    int quiet;
    int load_config;
    char *force_configdir;
    int use_filedir_conf;
    int hls_bitrate;
    int chapterrange[2];
    int edition_id;
    int correct_pts;
    int initial_audio_sync;
    int video_sync;
    double sync_max_video_change;
    double sync_max_audio_change;
    double sync_audio_drop_size;
    int hr_seek;
    float hr_seek_demuxer_offset;
    int hr_seek_framedrop;
    float audio_delay;
    float default_max_pts_correction;
    int autosync;
    int frame_dropping;
    int video_latency_hacks;
    int term_osd;
    int term_osd_bar;
    char *term_osd_bar_chars;
    char *playing_msg;
    char *osd_playing_msg;
    char *status_msg;
    char *osd_status_msg;
    char *osd_msg[3];
    int player_idle_mode;
    int consolecontrols;
    int playlist_pos;
    struct m_rel_time play_start;
    struct m_rel_time play_end;
    struct m_rel_time play_length;
    int rebase_start_time;
    int play_frames;
    double ab_loop[2];
    double step_sec;
    int position_resume;
    int position_save_on_quit;
    int write_filename_in_watch_later_config;
    int ignore_path_in_watch_later_config;
    char *watch_later_directory;
    int pause;
    int keep_open;
    int keep_open_pause;
    double image_display_duration;
    char *lavfi_complex;
    int stream_id[2][STREAM_TYPE_COUNT];
    char **stream_lang[STREAM_TYPE_COUNT];
    int stream_auto_sel;
    int audio_display;
    char **display_tags;

    char **audio_files;
    char *demuxer_name;
    int demuxer_thread;
    double demux_termination_timeout;
    int prefetch_open;
    char *audio_demuxer_name;
    char *sub_demuxer_name;

    int cache_pause;
    int cache_pause_initial;
    float cache_pause_wait;

    struct image_writer_opts *screenshot_image_opts;
    char *screenshot_template;
    char *screenshot_directory;

    double force_fps;
    int index_mode;

    struct m_channels audio_output_channels;
    int audio_output_format;
    int force_srate;
    double playback_speed;
    int pitch_correction;
    struct m_obj_settings *vf_settings, *vf_defs;
    struct m_obj_settings *af_settings, *af_defs;
    struct filter_opts *filter_opts;
    float movie_aspect;
    int aspect_method;
    char **sub_name;
    char **sub_paths;
    char **audiofile_paths;
    char **external_files;
    int autoload_files;
    int sub_auto;
    int audiofile_auto;
    int osd_bar_visible;

    int w32_priority;

    struct tv_params *tv_params;
    struct pvr_params *stream_pvr_opts;
    struct cdda_params *stream_cdda_opts;
    struct dvb_params *stream_dvb_opts;
    struct stream_lavf_params *stream_lavf_opts;

    char *cdrom_device;
    char *bluray_device;

    double mf_fps;
    char *mf_type;

    struct demux_rawaudio_opts *demux_rawaudio;
    struct demux_rawvideo_opts *demux_rawvideo;
    struct demux_lavf_opts *demux_lavf;
    struct demux_mkv_opts *demux_mkv;

    struct demux_opts *demux_opts;

    struct vd_lavc_params *vd_lavc_params;
    struct ad_lavc_params *ad_lavc_params;

    struct input_opts *input_opts;

    // may be NULL if encoding is not compiled-in
    struct encode_opts *encode_opts;

    char *ipc_path;
    char *input_file;

    int wingl_dwm_flush;

    struct mp_resample_opts *resample_opts;

    struct gl_video_opts *gl_video_opts;
    struct angle_opts *angle_opts;
    struct opengl_opts *opengl_opts;
    struct vulkan_opts *vulkan_opts;
    struct spirv_opts *spirv_opts;
    struct d3d11_opts *d3d11_opts;
    struct d3d11va_opts *d3d11va_opts;
    struct cocoa_opts *cocoa_opts;
    struct macos_opts *macos_opts;
    struct android_opts *android_opts;
    struct dvd_opts *dvd_opts;
    struct vaapi_opts *vaapi_opts;

    int cuda_device;
} MPOpts;

struct dvd_opts {
    int angle;
    int speed;
    char *device;
};

struct filter_opts {
    int deinterlace;
};

extern const m_option_t mp_opts[];
extern const struct MPOpts mp_default_opts;
extern const struct m_sub_options vo_sub_opts;
extern const struct m_sub_options dvd_conf;
extern const struct m_sub_options mp_subtitle_sub_opts;
extern const struct m_sub_options mp_osd_render_sub_opts;
extern const struct m_sub_options filter_conf;
extern const struct m_sub_options resample_conf;

#endif
