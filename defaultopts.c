#include <stddef.h>

#include "config.h"
#include "defaultopts.h"
#include "options.h"

void set_default_mplayer_options(struct MPOpts *opts)
{
    *opts = (const struct MPOpts){
        .audio_driver_list = NULL,
        .video_driver_list = NULL,
        .fixed_vo = 1,
        .ao_buffersize = -1,
        .monitor_pixel_aspect = 1.0,
        .vo_panscanrange = 1.0,
        .vo_gamma_gamma = 1000,
        .vo_gamma_brightness = 1000,
        .vo_gamma_contrast = 1000,
        .vo_gamma_saturation = 1000,
        .vo_gamma_hue = 1000,
        .osd_level = 1,
        .osd_duration = 1000,
        .stream_dump_name = "stream.dump",
        .loop_times = -1,
        .ordered_chapters = 1,
        .chapter_merge_threshold = 100,
        .stream_cache_min_percent = 20.0,
        .stream_cache_seek_min_percent = 50.0,
        .chapterrange = {-1, -1},
        .edition_id = -1,
        .user_correct_pts = -1,
        .initial_audio_sync = 1,
        .term_osd = 1,
        .term_osd_esc = "\x1b[A\r\x1b[K",
        .key_fifo_size = 7,
        .consolecontrols = 1,
        .doubleclick_time = 300,
        .audio_id = -1,
        .video_id = -1,
        .sub_id = -1,
        .extension_parsing = 1,
        .audio_output_channels = 2,
        .audio_output_format = -1,  // AF_FORMAT_UNKNOWN
        .playback_speed = 1.,
        .drc_level = 1.,
        .movie_aspect = -1.,
        .flip = -1,
        .vd_use_slices = 1,
#ifdef CONFIG_ASS
        .ass_enabled = 1,
#endif

        .lavc_param = {
            .workaround_bugs = 1, // autodetect
            .error_resilience = 2,
            .error_concealment = 3,
        },
        .input = {
             .config_file = "input.conf",
             .ar_delay = 100,
             .ar_rate = 8,
             .use_joystick = 1,
             .use_lirc = 1,
             .use_lircc = 1,
#ifdef CONFIG_APPLE_REMOTE
             .use_ar = 1,
#endif
             .default_bindings = 1,
         }
    };
}
