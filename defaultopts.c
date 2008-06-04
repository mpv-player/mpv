#include <stddef.h>

#include "config.h"
#include "defaultopts.h"
#include "options.h"

void set_default_mplayer_options(struct MPOpts *opts)
{
    *opts = (const struct MPOpts){
        .audio_driver_list = NULL,
        .video_driver_list = NULL,
        .fixed_vo = 0,
        .monitor_pixel_aspect = 1.0,
        .vo_panscanrange = 1.0,
        .vo_gamma_gamma = 1000,
        .vo_gamma_brightness = 1000,
        .vo_gamma_contrast = 1000,
        .vo_gamma_saturation = 1000,
        .vo_gamma_hue = 1000,
        .loop_times = -1,
        .user_correct_pts = -1,
        .key_fifo_size = 7,
        .doubleclick_time = 300,
        .audio_id = -1,
        .video_id = -1,
        .sub_id = -2,
        .playback_speed = 1.,
        .movie_aspect = -1.,
        .flip = -1,
        .vd_use_slices = 1,
        .lavc_param = (struct lavc_param){
            .workaround_bugs = 1, // autodetect
            .error_resilience = 2,
            .error_concealment = 3,
            .threads = 1,
        },
        .input = (struct input_conf){
             .config_file = "input.conf",
             .ar_delay = 100,
             .ar_rate = 8,
             .use_joystick = 1,
             .use_lirc = 1,
             .use_lircc = 1,
#ifdef HAVE_APPLE_REMOTE
             .use_ar = 1,
#else
             .use_ar = 0,
#endif
         }
    };
}

void set_default_mencoder_options(struct MPOpts *opts)
{
    set_default_mplayer_options(opts);
    opts->user_correct_pts = 0;
}
