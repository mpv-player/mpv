"""
This script uses the lavfi cropdetect filter and the video-crop property to
automatically crop the currently playing video with appropriate parameters.

It automatically crops the video when playback starts.

You can also manually crop the video by pressing the "C" (shift+c) key.
Pressing it again undoes the crop.

The workflow is as follows: First, it inserts the cropdetect filter. After
<detect_seconds> (default is 1) seconds, it then sets video-crop based on the
vf-metadata values gathered by cropdetect. The cropdetect filter is removed
after video-crop is set as it is no longer needed.

Since the crop parameters are determined from the 1 second of video between
inserting the cropdetect filter and setting video-crop, the "C" key should be
pressed at a position in the video where the crop region is unambiguous (i.e.,
not a black frame, black background title card, or dark scene).

If non-copy-back hardware decoding is in use, hwdec is temporarily disabled for
the duration of cropdetect as the filter would fail otherwise.

These are the default options. They can be overridden by adding
script-opts-append=autocrop-<parameter>=<value> to mpv.conf.
"""
from mpvclient import mpv  # type: ignore

options = dict(
    # Whether to automatically apply crop at the start of playback. If you
    # don't want to crop automatically, add
    # script-opts-append=autocrop-auto=no to mpv.conf.
    auto = True,
    # Delay before starting crop in auto mode. You can try to increase this
    # value to avoid dark scenes or fade ins at beginning. Automatic cropping
    # will not occur if the value is larger than the remaining playback time.
    auto_delay = 4,
    # Black threshold for cropdetect. Smaller values will generally result in
    # less cropping. See limit of
    # https://ffmpeg.org/ffmpeg-filters.html#cropdetect
    detect_limit = "24/255",
    # The value which the width/height should be divisible by. Smaller
    # values have better detection accuracy. If you have problems with
    # other filters, you can try to set it to 4 or 16. See round of
    # https://ffmpeg.org/ffmpeg-filters.html#cropdetect
    detect_round = 2,
    # The ratio of the minimum clip size to the original. A number from 0 to
    # 1. If the picture is over cropped, try adjusting this value.
    detect_min_ratio = 0.5,
    # How long to gather cropdetect data. Increasing this may be desirable to
    # allow cropdetect more time to collect data.
    detect_seconds = 1,
    # Whether the OSD shouldn't be used when cropdetect and video-crop are
    # applied and removed.
    suppress_osd = False,
)

mpv.options.read_options(options)

cropdetect_label = mpv.name + "-cropdetect"

hwdec_backup = None

command_prefix = options["suppress_osd"] and "no-osd" or ""


def is_enough_time(seconds):
    # Plus 1 second for deviation.
    time_needed = seconds + 1
    playtime_remaining = mpv.get_property_node("playtime-remaining")
    return playtime_remaining and time_needed < playtime_remaining


def is_cropable(time_needed):
    if mpv.get_property_node('current-tracks/video/image'):
        mpv.warn("autocrop only works for videos.")
        return False

    if not is_enough_time(time_needed):
        mpv.warn("Not enough time to detect crop.")
        return False

    return True


def remove_cropdetect():
    for filter in mpv.get_property_node("vf"):
        if filter["label"] == cropdetect_label:
            mpv.command_string(f"{command_prefix} vf remove @{filter["label"]}")
            return


def restore_hwdec():
    global hwdec_backup
    if hwdec_backup:
        mpv.set_property_string("hwdec", hwdec_backup)
        hwdec_backup = None


def cleanup():
    remove_cropdetect()
    # Kill all timers.
    mpv.clear_timers()

    restore_hwdec()


def apply_crop(meta):
    # Verify if it is necessary to crop.
    is_effective = meta["w"] and meta["h"] and meta["x"] and meta["y"] and \
                         (meta["x"] > 0 or meta["y"] > 0
                         or meta["w"] < meta["max_w"] or meta["h"] < meta["max_h"])

    # Verify it is not over cropped.
    is_excessive = False
    if is_effective and (meta["w"] < meta["min_w"] or meta["h"] < meta["min_h"]):
        mpv.info("The area to be cropped is too large.")
        mpv.info("You might need to decrease detect_min_ratio.")
        is_excessive = True

    if not is_effective or is_excessive:
        # Clear any existing crop.
        mpv.command_string(f"{command_prefix} set file-local-options/video-crop ''")
        return

    # Apply crop.
    mpv.command_string("%s set file-local-options/video-crop %sx%s+%s+%s" %
                             (command_prefix, meta["w"], meta["h"], meta["x"], meta["y"]))


def detect_end():
    # Get the metadata and remove the cropdetect filter.
    cropdetect_metadata = mpv.get_property_node("vf-metadata/" + cropdetect_label)
    remove_cropdetect()

    # Remove the timer of detect_crop.
    mpv.clear_timer("detect_crop")

    restore_hwdec()

    # Verify the existence of metadata.
    if cropdetect_metadata:
        meta = dict(
            w = cropdetect_metadata["lavfi.cropdetect.w"],
            h = cropdetect_metadata["lavfi.cropdetect.h"],
            x = cropdetect_metadata["lavfi.cropdetect.x"],
            y = cropdetect_metadata["lavfi.cropdetect.y"],
        )
    else:
        mpv.error("No crop data.")
        mpv.info("Was the cropdetect filter successfully inserted?")
        mpv.info("Does your version of FFmpeg support AVFrame metadata?")
        return

    # Verify that the metadata meets the requirements and convert it.
    if meta["w"] and meta["h"] and meta["x"] and meta["y"]:
        width = mpv.get_property_node("width")
        height = mpv.get_property_node("height")

        meta = dict(
            w = int(meta["w"]),
            h = int(meta["h"]),
            x = int(meta["x"]),
            y = int(meta["y"]),
            min_w = width * options["detect_min_ratio"],
            min_h = height * options["detect_min_ratio"],
            max_w = width,
            max_h = height,
        )
    else:
        mpv.error("Got empty crop data.")
        mpv.info("You might need to increase detect_seconds.")

    apply_crop(meta)


def detect_crop():
    time_needed = options["detect_seconds"]

    if not is_cropable(time_needed):
        return

    hwdec_current = mpv.get_property_string("hwdec-current")

    if not hwdec_current.endswith("-copy") and hwdec_current not in ["no", "crystalhd", "rkmpp"]:
        hwdec_backup = mpv.get_property_string("hwdec")
        mpv.set_property_string("hwdec", "no")

    # Insert the cropdetect filter.
    limit = options["detect_limit"]
    round = options["detect_round"]

    mpv.command_string('%s vf pre @%s:cropdetect=limit=%s:round=%d:reset=0' % (
        command_prefix, cropdetect_label, limit, round))

    # Wait to gather data.
    mpv.add_timeout(time_needed, detect_end, name="detect_crop")


def on_start():

    # Clean up at the beginning.
    cleanup()

    # If auto is not true, exit.
    if not options["auto"]:
        return

    # If it is the beginning, wait for detect_crop
    # after auto_delay seconds, otherwise immediately.
    playback_time = mpv.get_property_node("playback-time")
    is_delay_needed = playback_time and options["auto_delay"] > playback_time

    if is_delay_needed:

        # Verify if there is enough time for autocrop.
        time_needed = options["auto_delay"] + options["detect_seconds"]

        if not is_cropable(time_needed):
            return

        def auto_delay():
            detect_crop()

            mpv.clear_timer("auto_delay")

        mpv.add_timeout(time_needed, auto_delay, name="auto_delay")
    else:
        detect_crop()


@mpv.add_binding(key="C", name="toggle_crop")
def on_toggle():

    # If it is during auto_delay, kill the timer.
    if "auto_delay" in mpv.timers:
        mpv.clear_timer("auto_delay")

    # Cropped => Remove it.
    if mpv.get_property_string("video-crop") != "":
        mpv.command_string("%s set file-local-options/video-crop ''" % command_prefix)
        return

    # Detecting => Leave it.
    if "detect_crop" in mpv.timers:
        mpv.warn("Already cropdetecting!")
        return

    # Neither => Detect crop.
    detect_crop()


mpv.register_event(mpv.MPV_EVENT_END_FILE)(cleanup)
mpv.register_event(mpv.MPV_EVENT_FILE_LOADED)(on_start)
