#ifndef MPLAYER_OPTIONS_H
#define MPLAYER_OPTIONS_H

#include <stdbool.h>
#include <stdint.h>
#include "m_option.h"

typedef struct mp_vo_opts {
    struct m_obj_settings *video_driver_list, *vo_defs;

    int ontop;
    int fullscreen;
    int border;
    int all_workspaces;

    int screen_id;
    int fsscreen_id;
    int fs_black_out_screens;
    char *winname;
    int x11_netwm;
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
    float window_scale;

    int keepaspect;
    int keepaspect_window;

    int64_t WinID;

    float force_monitor_aspect;
    float monitor_pixel_aspect;
    int force_window_position;

    struct sws_opts *sws_opts;
} mp_vo_opts;

struct mp_cache_opts {
    int size;
    int def_size;
    int initial;
    int seek_min;
    char *file;
    int file_max;
};

typedef struct MPOpts {
    int use_terminal;
    char *dump_stats;
    int verbose;
    char **msg_levels;
    int msg_color;
    int msg_module;
    int msg_time;
    char *log_file;

    char **reset_options;
    char **script_files;
    char **script_opts;
    int lua_load_osc;
    int lua_load_ytdl;
    char *lua_ytdl_format;
    char **lua_ytdl_raw_options;

    int auto_load_scripts;

    struct m_obj_settings *audio_driver_list, *ao_defs;
    char *audio_device;
    char *audio_client_name;
    int force_vo;
    int softvol;
    float mixer_init_volume;
    int mixer_init_mute;
    char *mixer_restore_volume_data;
    float softvol_max;
    int gapless_audio;
    double audio_buffer;

    mp_vo_opts vo;
    int allow_win_drag;

    char *wintitle;
    char *media_title;
    int force_rgba_osd;

    // ranges -100 - 100, 1000 if the vo default should be used
    int gamma_gamma;
    int gamma_brightness;
    int gamma_contrast;
    int gamma_saturation;
    int gamma_hue;

    int stop_screensaver;
    int cursor_autohide_delay;
    int cursor_autohide_fs;

    int video_rotate;
    int video_stereo_mode;

    char *audio_decoders;
    char *video_decoders;

    int osd_level;
    int osd_duration;
    int osd_fractions;
    int untimed;
    char *stream_capture;
    char *stream_dump;
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
    int network_rtsp_transport;
    int hls_bitrate;
    struct mp_cache_opts stream_cache;
    int chapterrange[2];
    int edition_id;
    int correct_pts;
    int user_pts_assoc_mode;
    int initial_audio_sync;
    int hr_seek;
    float hr_seek_demuxer_offset;
    int hr_seek_framedrop;
    float audio_delay;
    float default_max_pts_correction;
    int autosync;
    int frame_dropping;
    double frame_drop_fps;
    int term_osd;
    int term_osd_bar;
    char *term_osd_bar_chars;
    char *playing_msg;
    char *osd_playing_msg;
    char *status_msg;
    char *osd_status_msg;
    char *osd_msg[3];
    char *heartbeat_cmd;
    float heartbeat_interval;
    int player_idle_mode;
    int consolecontrols;
    struct m_rel_time play_start;
    struct m_rel_time play_end;
    struct m_rel_time play_length;
    int play_frames;
    double ab_loop[2];
    double step_sec;
    int position_resume;
    int position_save_on_quit;
    int write_filename_in_watch_later_config;
    int ignore_path_in_watch_later_config;
    int pause;
    int keep_open;
    int audio_id;
    int video_id;
    int sub_id;
    int sub2_id;
    int audio_id_ff;
    int video_id_ff;
    int sub_id_ff;
    char **audio_lang;
    char **sub_lang;
    int audio_display;
    char **display_tags;
    int sub_visibility;
    int sub_pos;
    float sub_delay;
    float sub_fps;
    float sub_speed;
    int forced_subs_only;
    int stretch_dvd_subs;

    int sub_fix_timing;
    char *sub_cp;

    char **audio_files;
    char *demuxer_name;
    int demuxer_thread;
    int demuxer_min_packs;
    int demuxer_min_bytes;
    double demuxer_min_secs;
    char *audio_demuxer_name;
    char *sub_demuxer_name;

    double demuxer_min_secs_cache;
    int cache_pausing;

    struct image_writer_opts *screenshot_image_opts;
    char *screenshot_template;

    double force_fps;
    int index_mode;

    struct mp_chmap audio_output_channels;
    int audio_output_format;
    int force_srate;
    int dtshd;
    double playback_speed;
    int pitch_correction;
    struct m_obj_settings *vf_settings, *vf_defs;
    struct m_obj_settings *af_settings, *af_defs;
    int deinterlace;
    float movie_aspect;
    int field_dominance;
    char **sub_name;
    char **sub_paths;
    int sub_auto;
    int audiofile_auto;
    int use_text_osd;
    int osd_bar_visible;
    float osd_bar_align_x;
    float osd_bar_align_y;
    float osd_bar_w;
    float osd_bar_h;
    float osd_scale;
    int osd_scale_by_window;
    int sub_scale_by_window;
    int sub_scale_with_window;
    int ass_scale_with_window;
    struct osd_style_opts *osd_style;
    struct osd_style_opts *sub_text_style;
    float sub_scale;
    float sub_gauss;
    int sub_gray;
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
    int sub_clear_on_seek;

    int hwdec_api;
    char *hwdec_codecs;

    int w32_priority;

    int slave_mode;

    int network_cookies_enabled;
    char *network_cookies_file;
    char *network_useragent;
    char *network_referrer;
    char **network_http_header_fields;
    int network_tls_verify;
    char *network_tls_ca_file;
    double network_timeout;

    struct tv_params *tv_params;
    struct pvr_params *stream_pvr_opts;
    struct cdda_params *stream_cdda_opts;
    struct dvb_params *stream_dvb_opts;
    struct stream_lavf_params *stream_lavf_opts;

    char *cdrom_device;
    int dvd_title;
    int dvd_angle;
    int dvd_speed;
    char *dvd_device;
    int bluray_angle;
    char *bluray_device;

    double mf_fps;
    char *mf_type;

    struct demux_rawaudio_opts *demux_rawaudio;
    struct demux_rawvideo_opts *demux_rawvideo;
    struct demux_lavf_opts *demux_lavf;
    struct demux_mkv_opts *demux_mkv;

    struct vd_lavc_params *vd_lavc_params;
    struct ad_lavc_params *ad_lavc_params;

    struct input_opts *input_opts;

    // may be NULL if encoding is not compiled-in
    struct encode_opts *encode_opts;

    char *ipc_path;
    char *input_file;
} MPOpts;

extern const m_option_t mp_opts[];
extern const struct MPOpts mp_default_opts;

#endif
