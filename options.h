#ifndef MPLAYER_OPTIONS_H
#define MPLAYER_OPTIONS_H

typedef struct MPOpts {
    char **video_driver_list;
    char **audio_driver_list;
    int fixed_vo;
    int vo_ontop;
    int screen_size_x;
    int screen_size_y;
    int vo_screenwidth;
    int vo_screenheight;
    int vidmode;
    int fullscreen;
    int vo_dbpp;
    int correct_pts;
    int loop_times;
    int user_correct_pts;
    int audio_id;
    int video_id;
    int sub_id;
    float playback_speed;
    int softzoom;
    int flip;
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
    } lavc_param;
} MPOpts;

#endif
