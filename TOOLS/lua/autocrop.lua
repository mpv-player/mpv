-- This script uses the lavfi cropdetect filter to automatically
-- insert a crop filter with appropriate parameters for the currently
-- playing video.
--
-- It registers the key-binding "C" (shift+c), which when pressed,
-- inserts the filter vf=lavfi=cropdetect. After 1 second, it then
-- inserts the filter vf=crop=w:h:x:y, where w,h,x,y are determined
-- from the vf-metadata gathered by cropdetect. The cropdetect filter
-- is removed immediately after the crop filter is inserted as it is
-- no longer needed.
--
-- If the "C" key is pressed again, the crop filter is removed
-- restoring playback to its original state.
--
-- Since the crop parameters are determined from the 1 second of video
-- between inserting the cropdetect and crop filters, the "C" key
-- should be pressed at a position in the video where the crop region
-- is unambiguous (i.e., not a black frame, black background title
-- card, or dark scene).
--
-- The default delay between insertion of the cropdetect and
-- crop filters may be overridden by adding
--
-- --script-opts=autocrop.detect_seconds=<number of seconds>
--
-- to mpv's arguments. This may be desirable to allow cropdetect more
-- time to collect data.
require "mp.msg"

script_name = mp.get_script_name()
cropdetect_label = string.format("%s-cropdetect", script_name)
crop_label = string.format("%s-crop", script_name)

-- number of seconds to gather cropdetect data
detect_seconds = tonumber(mp.get_opt(string.format("%s.detect_seconds", script_name)))
if not detect_seconds then
    detect_seconds = 1
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

function autocrop_start()
    -- exit if cropdetection is already in progress
    if timer then
        mp.msg.warn("already cropdetecting!")
        return
    end

    -- if there's a crop filter, remove it and exit
    if del_filter_if_present(crop_label) then
        return
    end

    -- insert the cropdetect filter
    ret=mp.command(
        string.format(
            'vf add @%s:cropdetect=limit=%f:round=2:reset=0',
            cropdetect_label, 24/255
        )
    )
    -- wait to gather data
    timer=mp.add_timeout(detect_seconds, do_crop)
end

function do_crop()
    -- get the metadata
    local cropdetect_metadata = mp.get_property_native(
        string.format("vf-metadata/%s", cropdetect_label)
    )

    -- use it to crop if its valid
    if cropdetect_metadata then
        if cropdetect_metadata["lavfi.cropdetect.w"]
            and cropdetect_metadata["lavfi.cropdetect.h"]
            and cropdetect_metadata["lavfi.cropdetect.x"]
            and cropdetect_metadata["lavfi.cropdetect.y"]
        then
            mp.command(string.format("vf add @%s:lavfi-crop=w=%s:h=%s:x=%s:y=%s",
                                     crop_label,
                                     cropdetect_metadata["lavfi.cropdetect.w"],
                                     cropdetect_metadata["lavfi.cropdetect.h"],
                                     cropdetect_metadata["lavfi.cropdetect.x"],
                                     cropdetect_metadata["lavfi.cropdetect.y"]))
        else
            mp.msg.error(
                "Got empty crop data. You might need to increase detect_seconds."
            )
        end
    else
        mp.msg.error(
            "No crop data. Was the cropdetect filter successfully inserted?"
        )
        mp.msg.error(
            "Does your version of ffmpeg/libav support AVFrame metadata?"
        )
    end
    -- remove the cropdetect filter
    del_filter_if_present(cropdetect_label)
    timer=nil
end

mp.add_key_binding("C", "auto_crop", autocrop_start)
