-- This script uses the lavfi idet filter to automatically insert the
-- appropriate deinterlacing filter based on a short section of the
-- currently playing video.
--
-- It registers the key-binding ctrl+d, which when pressed, inserts
-- the filter vf=lavfi=idet. After 4 second, it examines the results
-- to determine whether the content progressive or interlaced and the
-- interlacing field dominance. It immediately sets the latter within
-- mpv.
--
-- If the content is judged not to be progressive, it may be either
-- interlaced or telecined. To determine which one, the video is
-- seeked back to the detection starting point with the "pullup"
-- filter inserted. After another four seconds, if idet thinks that
-- the pulled up viedo is "progressive", then pullup is the correct
-- filter and this script leaves it in places while removing the idet
-- filter. Otherwise, the content plain interlaced and mpv's deinterlace
-- property is set. This will usually insert the yadif deinterlacing filter.
--
-- The default detection time may be overridden by adding
--
-- --script-opts=autodeint.detect_seconds=<number of seconds>
--
-- to mpv's arguments. This may be desirable to allow idet more
-- time to collect data.
--
-- To see counts of the various types of frames for each detection phase,
-- the verbosity can be increased with
--
-- --msg-level autodeint=v
--
-- This script requires a recent version of ffmpeg for which the idet
-- filter adds the required metadata.

require "mp.msg"

script_name = mp.get_script_name()
detect_label = string.format("%s-detect", script_name)
pullup_label = string.format("%s", script_name)

-- number of seconds to gather cropdetect data
detect_seconds = tonumber(mp.get_opt(string.format("%s.detect_seconds", script_name)))
if not detect_seconds then
    detect_seconds = 4
end

function del_filter_if_present(label)
    -- necessary because mp.command('vf del @label:filter') raises an
    -- error if the filter doesn't exist
    local vfs = mp.get_property_native("vf")

    for i,vf in pairs(vfs) do
        if vf["label"] == label then
            table.remove(vfs, i)
            mp.set_property_native("vf", vfs)
            return true
        end
    end
    return false
end

function start_detect()
    -- exit if detection is already in progress
    if timer then
        mp.msg.warn("already detecting!")
        return
    end

    mp.set_property("deinterlace","no")
    del_filter_if_present(pullup_label)

    percent_pos = mp.get_property("percent-pos")

    -- insert the detection filter
    local cmd = string.format('vf add @%s:lavfi=graph="idet"', detect_label)
    if not mp.command(cmd) then
        mp.msg.error("failed to insert detection filter")
        return
    end

    -- wait to gather data
    timer = mp.add_timeout(detect_seconds, judge_field_dominance)
end

function stop_detect()
    del_filter_if_present(detect_label)
    timer = nil
    mp.set_property("playback-pos", percent_pos)
end

progressive, interlaced_tff, interlaced_bff, interlaced = 0, 1, 2, 3, 4

function judge()
    -- get the metadata
    local result = mp.get_property_native(string.format("vf-metadata/%s", detect_label))
    num_tff          = tonumber(result["lavfi.idet.multiple.tff"])
    num_bff          = tonumber(result["lavfi.idet.multiple.bff"])
    num_progressive  = tonumber(result["lavfi.idet.multiple.progressive"])
    num_undetermined = tonumber(result["lavfi.idet.multiple.undetermined"])
    num_interlaced   = num_tff + num_bff
    num_determined   = num_interlaced + num_progressive

    mp.msg.verbose("progressive    = "..num_progressive)
    mp.msg.verbose("interlaced-tff = "..num_tff)
    mp.msg.verbose("interlaced-bff = "..num_bff)
    mp.msg.verbose("undetermined   = "..num_undetermined)

    if num_determined < num_undetermined then
        mp.msg.warn("majority undetermined frames")
    end
    if num_progressive > 20*num_interlaced then
        return progressive
    elseif num_tff > 10*num_bff then
        return interlaced_tff
    elseif num_bff > 10*num_tff then
        return interlaced_bff
    else
        return interlaced
    end
end

function judge_field_dominance()
    verdict = judge()
    if verdict == progressive then
        stop_detect()
        mp.msg.info("progressive: doing nothing")
        return
    elseif verdict == interlaced_tff then
        mp.set_property("field-dominance", "top")
    elseif verdict == interlaced_bff then
        mp.set_property("field-dominance", "bottom")
    elseif verdict == interlaced then
        mp.set_property("field-dominance", "auto")
    end

    local cmd = string.format("vf pre @%s:pullup", pullup_label)
    if not mp.command(cmd) then
        mp.msg.error("failed to insert pullup filter")
        stop_detect()
        return
    end

    -- redo the detection with the pullup filter to see if it's telecined
    mp.set_property("percent-pos", percent_pos)
    timer = mp.add_timeout(detect_seconds, judge_ivtc)
end

function judge_ivtc()
    verdict = judge()

    if verdict == progressive then
        mp.msg.info(string.format("telecinied with %s field dominance: using pullup", mp.get_property("field-dominance")))
        stop_detect()
    else
        mp.msg.info(string.format("interlaced with %s field dominance: setting deinterlace property", mp.get_property("field-dominance")))
        del_filter_if_present(pullup_label)
        mp.set_property("deinterlace","yes")
        stop_detect()
    end

    return
end

mp.add_key_binding("ctrl+d", script_name, start_detect)
