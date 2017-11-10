-- This script uses the lavfi idet filter to automatically insert the
-- appropriate deinterlacing filter based on a short section of the
-- currently playing video.
--
-- It registers the key-binding ctrl+d, which when pressed, inserts the filters
-- ``vf=idet,lavfi-pullup,idet``. After 4 seconds, it removes these
-- filters and decides whether the content is progressive, interlaced, or
-- telecined and the interlacing field dominance.
--
-- Based on this information, it may set mpv's ``deinterlace`` property (which
-- usually inserts the yadif filter), or insert the ``pullup`` filter if the
-- content is telecined.  It also sets field dominance with lavfi setfield.
--
-- OPTIONS:
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
-- --msg-level=autodeint=v

require "mp.msg"

script_name = mp.get_script_name()
detect_label = string.format("%s-detect", script_name)
pullup_label = string.format("%s", script_name)
dominance_label = string.format("%s-dominance", script_name)
ivtc_detect_label = string.format("%s-ivtc-detect", script_name)

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

local function add_vf(label, filter)
    return mp.command(('vf add @%s:%s'):format(label, filter))
end

function start_detect()
    -- exit if detection is already in progress
    if timer then
        mp.msg.warn("already detecting!")
        return
    end

    mp.set_property("deinterlace","no")
    del_filter_if_present(pullup_label)
    del_filter_if_present(dominance_label)

    -- insert the detection filters
    if not (add_vf(detect_label, 'idet') and
            add_vf(dominance_label, 'setfield=mode=auto') and
            add_vf(pullup_label, 'lavfi-pullup') and
            add_vf(ivtc_detect_label, 'idet')) then
        mp.msg.error("failed to insert detection filters")
        return
    end

    -- wait to gather data
    timer = mp.add_timeout(detect_seconds, select_filter)
end

function stop_detect()
    del_filter_if_present(detect_label)
    del_filter_if_present(ivtc_detect_label)
    timer = nil
end

progressive, interlaced_tff, interlaced_bff, interlaced = 0, 1, 2, 3, 4

function judge(label)
    -- get the metadata
    local result = mp.get_property_native(string.format("vf-metadata/%s", label))
    local num_tff          = tonumber(result["lavfi.idet.multiple.tff"])
    local num_bff          = tonumber(result["lavfi.idet.multiple.bff"])
    local num_progressive  = tonumber(result["lavfi.idet.multiple.progressive"])
    local num_undetermined = tonumber(result["lavfi.idet.multiple.undetermined"])
    local num_interlaced   = num_tff + num_bff
    local num_determined   = num_interlaced + num_progressive

    mp.msg.verbose(label.." progressive    = "..num_progressive)
    mp.msg.verbose(label.." interlaced-tff = "..num_tff)
    mp.msg.verbose(label.." interlaced-bff = "..num_bff)
    mp.msg.verbose(label.." undetermined   = "..num_undetermined)

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

function select_filter()
    -- handle the first detection filter results
    local verdict = judge(detect_label)
    local ivtc_verdict = judge(ivtc_detect_label)
    local dominance = "auto"
    if verdict == progressive then
        mp.msg.info("progressive: doing nothing")
        stop_detect()
        del_filter_if_present(dominance_label)
        del_filter_if_present(pullup_label)
        return
    else
        if verdict == interlaced_tff then
            dominance = "tff"
            add_vf(dominance_label, 'setfield=mode='..dominance)
        elseif verdict == interlaced_bff then
            dominance = "bff"
            add_vf(dominance_label, 'setfield=mode='..dominance)
        else
            del_filter_if_present(dominance_label)
        end
    end

    -- handle the ivtc detection filter results
    if ivtc_verdict == progressive then
        mp.msg.info(string.format("telecined with %s field dominance: using pullup", dominance))
        stop_detect()
    else
        mp.msg.info(string.format("interlaced with %s field dominance: setting deinterlace property", dominance))
        del_filter_if_present(pullup_label)
        mp.set_property("deinterlace","yes")
        stop_detect()
    end
end

mp.add_key_binding("ctrl+d", script_name, start_detect)
