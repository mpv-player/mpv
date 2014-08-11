-- Rebuild the terminal status line as a lua script
-- Be aware that this will require more cpu power!

-- Add a string to the status line
function atsl(s)
    newStatus = newStatus .. s
end

function update_status_line()
    -- Reset the status line
    newStatus = ""

    if mp.get_property_bool("pause") then
        atsl("(Paused) ")
    elseif mp.get_property_bool("paused-for-cache") then
        atsl("(Buffering) ")
    end

    if mp.get_property("vid") ~= "no" then
        atsl("A")
    end
    if mp.get_property("aid") ~= "no" then
        atsl("V")
    end

    atsl(": ")

    atsl(mp.get_property_osd("time-pos"))

    atsl(" / ");
    atsl(mp.get_property_osd("length"));

    atsl(" (")
    atsl(mp.get_property_osd("percent-pos", -1))
    atsl("%)")

    local r = mp.get_property_number("speed", -1)
    if r ~= 1 then
        atsl(string.format(" x%4.2f", r))
    end

    r = mp.get_property_number("avsync", nil)
    if r ~= nil then
        atsl(string.format(" A-V: %7.3f", r))
    end

    r = mp.get_property("total-avsync-change", 0)
    if math.abs(r) > 0.05 then
        atsl(string.format(" ct:%7.3f", r))
    end

    r = mp.get_property_number("drop-frame-count", -1)
    if r > 0 then
        atsl(" Late: ")
        atsl(r)
    end

    r = mp.get_property_number("cache", 0)
    if r > 0 then
        atsl(string.format(" Cache: %d%% ", r))
    end

    -- Set the new status line
    mp.set_property("options/term-status-msg", newStatus)
end

-- Register the event
mp.register_event("tick", update_status_line)

