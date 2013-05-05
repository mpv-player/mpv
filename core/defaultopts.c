#include <stddef.h>

#include "config.h"
#include "defaultopts.h"
#include "core/options.h"
#include "audio/mixer.h"

void set_default_mplayer_options(struct MPOpts *opts)
{
    *opts = (const struct MPOpts){
        .audio_driver_list = NULL,
        .audio_decoders = "-spdif:*", // never select spdif by default
        .video_decoders = NULL,
        .fixed_vo = 1,
        .softvol = SOFTVOL_AUTO,
        .softvol_max = 200,
        .mixer_init_volume = -1,
        .mixer_init_mute = -1,
        .volstep = 3,
        .ao_buffersize = -1,
        .vo = {
            .video_driver_list = NULL,
            .cursor_autohide_delay = 1000,
            .monitor_pixel_aspect = 1.0,
            .panscanrange = 1.0,
            .fs = false,
            .screen_id = -1,
            .fsscreen_id = -1,
            .stop_screensaver = 1,
            .nomouse_input = 0,
            .fsmode = 0,
            .panscan = 0.0f,
            .keepaspect = 1,
            .border = 1,
            .colorkey = 0x0000ff00, // default colorkey is green
                        // (0xff000000 means that colorkey has been disabled)
            .WinID = -1,
        },
        .wintitle = "mpv - ${media-title}",
        .heartbeat_interval = 30.0,
        .gamma_gamma = 1000,
        .gamma_brightness = 1000,
        .gamma_contrast = 1000,
        .gamma_saturation = 1000,
        .gamma_hue = 1000,
        .osd_level = 1,
        .osd_duration = 1000,
        .osd_bar_align_y = 0.5,
        .osd_bar_w = 75.0,
        .osd_bar_h = 3.125,
        .loop_times = -1,
        .ordered_chapters = 1,
        .chapter_merge_threshold = 100,
        .load_config = 1,
        .position_resume = 1,
        .stream_cache_min_percent = 20.0,
        .stream_cache_seek_min_percent = 50.0,
        .stream_cache_pause = 10.0,
        .chapterrange = {-1, -1},
        .edition_id = -1,
        .default_max_pts_correction = -1,
        .user_correct_pts = -1,
        .initial_audio_sync = 1,
        .term_osd = 2,
        .consolecontrols = 1,
        .doubleclick_time = 300,
        .play_frames = -1,
        .keep_open = 0,
        .audio_id = -1,
        .video_id = -1,
        .sub_id = -1,
        .audio_display = 1,
        .sub_visibility = 1,
        .extension_parsing = 1,
        .audio_output_channels = 2,
        .audio_output_format = -1,  // AF_FORMAT_UNKNOWN
        .playback_speed = 1.,
        .drc_level = 1.,
        .movie_aspect = -1.,
        .sub_auto = 1,
        .osd_bar_visible = 1,
#ifdef CONFIG_ASS
        .ass_enabled = 1,
#endif
        .sub_scale = 1,
        .ass_vsfilter_aspect_compat = 1,
        .ass_style_override = 1,
        .use_embedded_fonts = 1,

        .hwdec_codecs = "all",

        .lavc_param = {
            .workaround_bugs = 1, // autodetect
            .error_concealment = 3,
        },
        .input = {
             .key_fifo_size = 7,
             .ar_delay = 200,
             .ar_rate = 40,
             .use_joystick = 1,
             .use_lirc = 1,
             .use_lircc = 1,
             .default_bindings = 1,
         }
    };
}
