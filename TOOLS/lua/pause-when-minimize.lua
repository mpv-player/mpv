-- This script pauses playback when minimizing the window, and resumes playback
-- if it's brought back again. If the player was already paused when minimizing,
-- then try not to mess with the pause state.

local did_minimize = false

mp.observe_property("window-minimized", "bool", function(_, value)
    local pause = mp.get_property_native("pause")
    if value == true then
        if pause == false then
            mp.set_property_native("pause", true)
            did_minimize = true
        end
    elseif value == false then
        if did_minimize and (pause == true) then
            mp.set_property_native("pause", false)
        end
        did_minimize = false
    end
end)
