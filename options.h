#ifndef MPLAYER_OPTIONS_H
#define MPLAYER_OPTIONS_H

typedef struct MPOpts {
    char **video_driver_list;
    char **audio_driver_list;
    int fixed_vo;
    int vo_ontop;
    char *mixer_device;
    char *mixer_channel;
    int softvol;
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
    int auto_quality;
    int untimed;
    int loop_times;
    int ordered_chapters;
    int chapter_merge_threshold;
    int quiet;
    int noconfig;
    float stream_cache_min_percent;
    float stream_cache_seek_min_percent;
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
    int term_osd;
    char *term_osd_esc;
    char *playing_msg;
    int player_idle_mode;
    int consolecontrols;
    int doubleclick_time;
    int list_properties;
    double seek_to_sec;
    int start_paused;
    int audio_id;
    int video_id;
    int sub_id;
    char **audio_lang;
    char **sub_lang;
    int sub_visibility;
    int hr_mp3_seek;
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
    int softzoom;
    float movie_aspect;
    float screen_size_xy;
    int flip;
    int vd_use_slices;
    char **sub_name;
    char **sub_paths;
    int sub_auto;
    int ass_enabled;
    float ass_font_scale;
    float ass_line_spacing;
    int ass_top_margin;
    int ass_bottom_margin;
    int ass_use_margins;
    int ass_vsfilter_aspect_compat;
    int use_embedded_fonts;
    char **ass_force_style_list;
    char *ass_color;
    char *ass_border_color;
    char *ass_styles_file;
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
    } input;
} MPOpts;

#endif
