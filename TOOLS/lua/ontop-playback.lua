--makes mpv disable ontop when pausing and re-enable it again when resuming playback
--please note that this won't do anything if ontop was not enabled before pausing

local was_ontop = mp.get_property_native("ontop")

function f_load(event)
    if was_ontop then
        mp.set_property_native("ontop", true)
    end
end

mp.register_event("file-loaded", f_load)

mp.observe_property("pause", "bool", function(name, value)

local ontop = mp.get_property_native("ontop")
local is_idle = mp.get_property_native("playback-abort")
    if value then
        mp.set_property_native("ontop", false)
        was_ontop = ontop
    else
        if is_idle then
            mp.set_property_native("ontop", false)
            was_ontop = ontop
        else
            if was_ontop then
                mp.set_property_native("ontop", true)
            end
        end
    end
end)
