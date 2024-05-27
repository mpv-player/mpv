--[[
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
--]]
local options = {
    -- Whether to automatically apply crop at the start of playback. If you
    -- don't want to crop automatically, add
    -- script-opts-append=autocrop-auto=no to mpv.conf.
    auto = true,
    -- Delay before starting crop in auto mode. You can try to increase this
    -- value to avoid dark scenes or fade ins at beginning. Automatic cropping
    -- will not occur if the value is larger than the remaining playback time.
    auto_delay = 4,
    -- Black threshold for cropdetect. Smaller values will generally result in
    -- less cropping. See limit of
    -- https://ffmpeg.org/ffmpeg-filters.html#cropdetect
    detect_limit = "24/255",
    -- The value which the width/height should be divisible by. Smaller
    -- values have better detection accuracy. If you have problems with
    -- other filters, you can try to set it to 4 or 16. See round of
    -- https://ffmpeg.org/ffmpeg-filters.html#cropdetect
    detect_round = 2,
    -- The ratio of the minimum clip size to the original. A number from 0 to
    -- 1. If the picture is over cropped, try adjusting this value.
    detect_min_ratio = 0.5,
    -- How long to gather cropdetect data. Increasing this may be desirable to
    -- allow cropdetect more time to collect data.
    detect_seconds = 1,
    -- Whether the OSD shouldn't be used when cropdetect and video-crop are
    -- applied and removed.
    suppress_osd = false,
}

require "mp.options".read_options(options)

local cropdetect_label = mp.get_script_name() .. "-cropdetect"

local timers = {
    auto_delay = nil,
    detect_crop = nil
}

local hwdec_backup

local command_prefix = options.suppress_osd and 'no-osd' or ''

local function is_enough_time(seconds)
    -- Plus 1 second for deviation.
    local time_needed = seconds + 1
    local playtime_remaining = mp.get_property_native("playtime-remaining")

    return playtime_remaining and time_needed < playtime_remaining
end

local function is_cropable(time_needed)
    if mp.get_property_native('current-tracks/video/image') ~= false then
        mp.msg.warn("autocrop only works for videos.")
        return false
    end

    if not is_enough_time(time_needed) then
        mp.msg.warn("Not enough time to detect crop.")
        return false
    end

    return true
end

local function remove_cropdetect()
    for _, filter in pairs(mp.get_property_native("vf")) do
        if filter.label == cropdetect_label then
            mp.command(
                string.format("%s vf remove @%s", command_prefix, filter.label))

            return
        end
    end
end

local function restore_hwdec()
    if hwdec_backup then
        mp.set_property("hwdec", hwdec_backup)
        hwdec_backup = nil
    end
end

local function cleanup()
    remove_cropdetect()

    -- Kill all timers.
    for index, timer in pairs(timers) do
        if timer then
            timer:kill()
            timers[index] = nil
        end
    end

    restore_hwdec()
end

local function apply_crop(meta)
    -- Verify if it is necessary to crop.
    local is_effective = meta.w and meta.h and meta.x and meta.y and
                         (meta.x > 0 or meta.y > 0
                         or meta.w < meta.max_w or meta.h < meta.max_h)

    -- Verify it is not over cropped.
    local is_excessive = false
    if is_effective and (meta.w < meta.min_w or meta.h < meta.min_h) then
        mp.msg.info("The area to be cropped is too large.")
        mp.msg.info("You might need to decrease detect_min_ratio.")
        is_excessive = true
    end

    if not is_effective or is_excessive then
        -- Clear any existing crop.
        mp.command(string.format("%s set file-local-options/video-crop ''", command_prefix))
        return
    end

    -- Apply crop.
    mp.command(string.format("%s set file-local-options/video-crop %sx%s+%s+%s",
                             command_prefix, meta.w, meta.h, meta.x, meta.y))
end

local function detect_end()
    -- Get the metadata and remove the cropdetect filter.
    local cropdetect_metadata = mp.get_property_native(
        "vf-metadata/" .. cropdetect_label)
    remove_cropdetect()

    -- Remove the timer of detect crop.
    if timers.detect_crop then
        timers.detect_crop:kill()
        timers.detect_crop = nil
    end

    restore_hwdec()

    local meta

    -- Verify the existence of metadata.
    if cropdetect_metadata then
        meta = {
            w = cropdetect_metadata["lavfi.cropdetect.w"],
            h = cropdetect_metadata["lavfi.cropdetect.h"],
            x = cropdetect_metadata["lavfi.cropdetect.x"],
            y = cropdetect_metadata["lavfi.cropdetect.y"],
        }
    else
        mp.msg.error("No crop data.")
        mp.msg.info("Was the cropdetect filter successfully inserted?")
        mp.msg.info("Does your version of FFmpeg support AVFrame metadata?")
        return
    end

    -- Verify that the metadata meets the requirements and convert it.
    if meta.w and meta.h and meta.x and meta.y then
        local width = mp.get_property_native("width")
        local height = mp.get_property_native("height")

        meta = {
            w = tonumber(meta.w),
            h = tonumber(meta.h),
            x = tonumber(meta.x),
            y = tonumber(meta.y),
            min_w = width * options.detect_min_ratio,
            min_h = height * options.detect_min_ratio,
            max_w = width,
            max_h = height
        }
    else
        mp.msg.error("Got empty crop data.")
        mp.msg.info("You might need to increase detect_seconds.")
    end

    apply_crop(meta)
end

local function detect_crop()
    local time_needed = options.detect_seconds

    if not is_cropable(time_needed) then
        return
    end

    local hwdec_current = mp.get_property("hwdec-current")
    if hwdec_current:find("-copy$") == nil and hwdec_current ~= "no" and
       hwdec_current ~= "crystalhd" and hwdec_current ~= "rkmpp" then
        hwdec_backup = mp.get_property("hwdec")
        mp.set_property("hwdec", "no")
    end

    -- Insert the cropdetect filter.
    local limit = options.detect_limit
    local round = options.detect_round

    mp.command(
        string.format(
            '%s vf pre @%s:cropdetect=limit=%s:round=%d:reset=0',
            command_prefix, cropdetect_label, limit, round
        )
    )

    -- Wait to gather data.
    timers.detect_crop = mp.add_timeout(time_needed, detect_end)
end

local function on_start()

    -- Clean up at the beginning.
    cleanup()

    -- If auto is not true, exit.
    if not options.auto then
        return
    end

    -- If it is the beginning, wait for detect_crop
    -- after auto_delay seconds, otherwise immediately.
    local playback_time = mp.get_property_native("playback-time")
    local is_delay_needed = playback_time
        and options.auto_delay > playback_time

    if is_delay_needed then

        -- Verify if there is enough time for autocrop.
        local time_needed = options.auto_delay + options.detect_seconds

        if not is_cropable(time_needed) then
            return
        end

        timers.auto_delay = mp.add_timeout(time_needed,
            function()
                detect_crop()

                -- Remove the timer of auto delay.
                timers.auto_delay:kill()
                timers.auto_delay = nil
            end
        )
    else
        detect_crop()
    end
end

local function on_toggle()

    -- If it is during auto_delay, kill the timer.
    if timers.auto_delay then
        timers.auto_delay:kill()
        timers.auto_delay = nil
    end

    -- Cropped => Remove it.
    if mp.get_property("video-crop") ~= "" then
        mp.command(string.format("%s set file-local-options/video-crop ''", command_prefix))
        return
    end

    -- Detecting => Leave it.
    if timers.detect_crop then
        mp.msg.warn("Already cropdetecting!")
        return
    end

    -- Neither => Detect crop.
    detect_crop()
end

mp.add_key_binding("C", "toggle_crop", on_toggle)
mp.register_event("end-file", cleanup)
mp.register_event("file-loaded", on_start)
