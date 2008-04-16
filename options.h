#ifndef MPLAYER_OPTIONS_H
#define MPLAYER_OPTIONS_H

typedef struct MPOpts {
    char **video_driver_list;
    char **audio_driver_list;
    int fixed_vo;
    int correct_pts;
    int user_correct_pts;
} MPOpts;

#endif
