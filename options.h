#ifndef MPLAYER_OPTIONS_H
#define MPLAYER_OPTIONS_H

typedef struct MPOpts {
    char **video_driver_list;
    char **audio_driver_list;
    int fixed_vo;
    int vo_ontop;
    int vo_screenwidth;
    int vo_screenheight;
    int vo_dbpp;
    int correct_pts;
    int loop_times;
    int user_correct_pts;
} MPOpts;

#endif
