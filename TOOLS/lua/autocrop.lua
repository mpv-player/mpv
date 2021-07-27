--[[
This script uses the lavfi cropdetect filter to automatically
insert a crop filter with appropriate parameters for the
currently playing video.

It will automatically crop the video, when playback starts.

Also It registers the key-binding "C" (shift+c). You can manually
crop the video by pressing the "C" (shift+c) key.

If the "C" key is pressed again, the crop filter is removed
restoring playback to its original state.

The workflow is as follows: First, it inserts the filter
vf=lavfi=cropdetect. After <detect_seconds> (default is 1)
seconds, it then inserts the filter vf=crop=w:h:x:y, where
w,h,x,y are determined from the vf-metadata gathered by
cropdetect. The cropdetect filter is removed immediately after
the crop filter is inserted as it is no longer needed.

Since the crop parameters are determined from the 1 second of
video between inserting the cropdetect and crop filters, the "C"
key should be pressed at a position in the video where the crop
region is unambiguous (i.e., not a black frame, black background
title card, or dark scene).

The default options can be overridden by adding
script-opts-append=autocrop-<parameter>=<value> into mpv.conf

List of available parameters (For default values, see <options>)：

auto: bool - Whether to automatically apply crop at the start of
    playback. If you don't want to crop automatically, set it to
    false or add "script-opts-append=autocrop-auto=no" into
    mpv.conf.

auto_delay: seconds - Delay before starting crop in auto mode.
    You can try to increase this value to avoid dark scene or
    fade in at beginning. Automatic cropping will not occur if
    the value is larger than the remaining playback time.

detect_limit: number[0-255] - Black threshold for cropdetect.
    Smaller values will generally result in less cropping.
    See limit of https://ffmpeg.org/ffmpeg-filters.html#cropdetect

detect_round: number[2^n] -  The value which the width/height
    should be divisible by. Smaller values ​​have better detection
    accuracy. If you have problems with other filters,
    you can try to set it to 4 or 16.
    See round of https://ffmpeg.org/ffmpeg-filters.html#cropdetect

detect_min_ratio: number[0.0-1.0] - The ratio of the minimum clip
    size to the original. If the picture is over cropped or under
    cropped, try adjusting this value.

detect_seconds: seconds - How long to gather cropdetect data.
    Increasing this may be desirable to allow cropdetect more
    time to collect data.

suppress_osd: bool - Whether the OSD shouldn't be used when filters
    are applied and removed.
--]]

require "mp.msg"
require 'mp.options'

local options = {
    auto = true,
    auto_delay = 4,
    detect_limit = "24/255",
    detect_round = 2,
    detect_min_ratio = 0.5,
    detect_seconds = 1,
    suppress_osd = false,
}
read_options(options)

local label_prefix = mp.get_script_name()
local labels = {
    crop = string.format("%s-crop", label_prefix),
    cropdetect = string.format("%s-cropdetect", label_prefix)
}

timers = {
    auto_delay = nil,
    detect_crop = nil
}

local command_prefix = options.suppress_osd and 'no-osd' or ''

function is_filter_present(label)
    local filters = mp.get_property_native("vf")
    for index, filter in pairs(filters) do
        if filter["label"] == label then
            return true
        end
    end
    return false
end

function is_enough_time(seconds)

    -- Plus 1 second for deviation.
    local time_needed = seconds + 1
    local playtime_remaining = mp.get_property_native("playtime-remaining")

    return playtime_remaining and time_needed < playtime_remaining
end

function is_cropable()
    for _, track in pairs(mp.get_property_native('track-list')) do
        if track.type == 'video' and track.selected then
            return not track.albumart
        end
    end

    return false
end

function remove_filter(label)
    if is_filter_present(label) then
        mp.command(string.format('%s vf remove @%s', command_prefix, label))
        return true
    end
    return false
end

function cleanup()

    -- Remove all existing filters.
    for key, value in pairs(labels) do
        remove_filter(value)
    end

    -- Kill all timers.
    for index, timer in pairs(timers) do
        if timer then
            timer:kill()
            timer = nil
        end
    end
end

function detect_crop()

    -- If it's not cropable, exit.
    if not is_cropable() then
        mp.msg.warn("autocrop only works for videos.")
        return
    end

    -- Verify if there is enough time to detect crop.
    local time_needed = options.detect_seconds

    if not is_enough_time(time_needed) then
        mp.msg.warn("Not enough time to detect crop.")
        return
    end

    -- Insert the cropdetect filter.
    local limit = options.detect_limit
    local round = options.detect_round

    mp.command(
        string.format(
            '%s vf pre @%s:cropdetect=limit=%s:round=%d:reset=0',
            command_prefix, labels.cropdetect, limit, round
        )
    )

    -- Wait to gather data.
    timers.detect_crop = mp.add_timeout(time_needed, detect_end)
end

function detect_end()

    -- Get the metadata and remove the cropdetect filter.
    local cropdetect_metadata =
        mp.get_property_native(
            string.format("vf-metadata/%s",
            labels.cropdetect
        )
    )
    remove_filter(labels.cropdetect)

    -- Remove the timer of detect crop.
    if timers.detect_crop then
        timers.detect_crop:kill()
        timers.detect_crop = nil
    end

    local meta = {}

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
        mp.msg.info("Does your version of ffmpeg/libav support AVFrame metadata?")
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
        return
    end

    apply_crop(meta)
end

function apply_crop(meta)

    -- Verify if it is necessary to crop.
    local is_effective = meta.x > 0 or meta.y > 0
        or meta.w < meta.max_w or meta.h < meta.max_h

    if not is_effective then
        mp.msg.info("No area detected for cropping.")
        return
    end

    -- Verify it is not over cropped.
    local is_excessive = meta.w < meta.min_w and meta.h < meta.min_h

    if is_excessive then
        mp.msg.info("The area to be cropped is too large.")
        mp.msg.info("You might need to decrease detect_min_ratio.")
        return
    end

    -- Remove existing crop.
    remove_filter(labels.crop)

    -- Apply crop.
    mp.command(
        string.format("%s vf pre @%s:lavfi-crop=w=%s:h=%s:x=%s:y=%s",
            command_prefix, labels.crop, meta.w, meta.h, meta.x, meta.y
        )
    )
end

function on_start()

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

        if not is_enough_time(time_needed) then
            mp.msg.warn("Not enough time for autocrop.")
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

function on_toggle()

    -- If it is during auto_delay, kill the timer.
    if timers.auto_delay then
        timers.auto_delay:kill()
        timers.auto_delay = nil
    end

    -- Cropped => Remove it.
    if remove_filter(labels.crop) then
        return
    end

    -- Detecting => Leave it.
    if timers.detect_crop then
        mp.msg.warn("Already cropdetecting!")
        return
    end

    -- Neither => Do delectcrop.
    detect_crop()
end

mp.add_key_binding("C", "toggle_crop", on_toggle)
mp.register_event("end-file", cleanup)
mp.register_event("file-loaded", on_start)
