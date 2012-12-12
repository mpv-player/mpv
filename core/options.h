#ifndef MPLAYER_OPTIONS_H
#define MPLAYER_OPTIONS_H

#include "core/m_option.h"

typedef struct MPOpts {
    char **video_driver_list;
    char **audio_driver_list;
    int fixed_vo;
    int vo_ontop;
    char *mixer_device;
    char *mixer_channel;
    int softvol;
    float mixer_init_volume;
    int mixer_init_mute;
    float softvol_max;
    int gapless_audio;
    int ao_buffersize;
    int screen_size_x;
    int screen_size_y;
    int vo_screenwidth;
    int vo_screenheight;
    int force_window_position;
    char *vo_winname;
    char *vo_wintitle;
    float force_monitor_aspect;
    float monitor_pixel_aspect;
    int vidmode;
    int fullscreen;
    int vo_dbpp;
    float vo_panscanrange;
    int requested_colorspace;
    int requested_input_range;
    int requested_output_range;
    int cursor_autohide_delay;

    // ranges -100 - 100, 1000 if the vo default should be used
    int vo_gamma_gamma;
    int vo_gamma_brightness;
    int vo_gamma_contrast;
    int vo_gamma_saturation;
    int vo_gamma_hue;

    int osd_level;
    int osd_duration;
    int osd_fractions;
    char *vobsub_name;
    int untimed;
    int loop_times;
    int ordered_chapters;
    int chapter_merge_threshold;
    int quiet;
    int noconfig;
    char *codecs_file;
    int stream_cache_size;
    float stream_cache_min_percent;
    float stream_cache_seek_min_percent;
    int stream_cache_pause;
    int chapterrange[2];
    int edition_id;
    int correct_pts;
    int user_correct_pts;
    int user_pts_assoc_mode;
    int initial_audio_sync;
    int hr_seek;
    float hr_seek_demuxer_offset;
    int autosync;
    int softsleep;
    int frame_dropping;
    int term_osd;
    char *term_osd_esc;
    char *playing_msg;
    char *status_msg;
    int player_idle_mode;
    int consolecontrols;
    int doubleclick_time;
    int list_properties;
    struct m_rel_time play_start;
    struct m_rel_time play_end;
    struct m_rel_time play_length;
    int start_paused;
    int keep_open;
    int audio_id;
    int video_id;
    int sub_id;
    char **audio_lang;
    char **sub_lang;
    int audio_display;
    int sub_visibility;
    char *quvi_format;

    char *audio_stream;
    int audio_stream_cache;
    char *sub_stream;
    char *demuxer_name;
    char *audio_demuxer_name;
    char *sub_demuxer_name;
    int extension_parsing;

    struct image_writer_opts *screenshot_image_opts;
    char *screenshot_template;

    int audio_output_channels;
    int audio_output_format;
    float playback_speed;
    float drc_level;
    struct m_obj_settings *vf_settings;
    float movie_aspect;
    float screen_size_xy;
    int flip;
    int vd_use_slices;
    int vd_use_dr1;
    char **sub_name;
    char **sub_paths;
    int sub_auto;
    struct osd_style_opts *osd_style;
    float sub_scale;
    float sub_gauss;
    int sub_gray;
    int ass_enabled;
    float ass_line_spacing;
    int ass_use_margins;
    int ass_vsfilter_aspect_compat;
    int use_embedded_fonts;
    char **ass_force_style_list;
    char *ass_styles_file;
    int ass_style_override;
    int ass_hinting;
    struct lavc_param {
        int workaround_bugs;
        int error_resilience;
        int error_concealment;
        int gray;
        int vstats;
        int idct_algo;
        int debug;
        int vismv;
        int skip_top;
        int skip_bottom;
        int fast;
        char *lowres_str;
        char *skip_loop_filter_str;
        char *skip_idct_str;
        char *skip_frame_str;
        int threads;
        int bitexact;
        char *avopt;
    } lavc_param;

    struct lavfdopts {
        unsigned int probesize;
        int probescore;
        unsigned int analyzeduration;
        char *format;
        char *cryptokey;
        char *avopt;
    } lavfdopts;

    struct input_conf {
        char *config_file;
        int key_fifo_size;
        unsigned int ar_delay;
        unsigned int ar_rate;
        char *js_dev;
        char *ar_dev;
        char *in_file;
        int use_joystick;
        int use_lirc;
        int use_lircc;
        int use_ar; // apple remote
        int default_bindings;
        int test;
    } input;

    struct encode_output_conf {
        char *file;
        char *format;
        char **fopts;
        float fps;
        char *vcodec;
        char **vopts;
        char *acodec;
        char **aopts;
        int harddup;
        float voffset;
        float aoffset;
        int copyts;
        int rawts;
        int autofps;
        int neverdrop;
        int video_first;
        int audio_first;
    } encode_output;
} MPOpts;

#endif
