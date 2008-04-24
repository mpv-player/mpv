#include <stddef.h>
#include "defaultopts.h"
#include "options.h"

void set_default_mplayer_options(struct MPOpts *opts)
{
    *opts = (const struct MPOpts){
        .audio_driver_list = NULL,
        .video_driver_list = NULL,
        .fixed_vo = 0,
        .loop_times = -1,
        .user_correct_pts = -1,
        .audio_id = -1,
        .video_id = -1,
        .sub_id = -2,
        .playback_speed = 1.,
        .lavc_param = (struct lavc_param){
            .workaround_bugs = 1, // autodetect
            .error_resilience = 2,
            .error_concealment = 3,
            .threads = 1,
        },
    };
}

void set_default_mencoder_options(struct MPOpts *opts)
{
    set_default_mplayer_options(opts);
    opts->user_correct_pts = 0;
}
