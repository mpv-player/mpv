-- This script cycles between deinterlacing, pullup (inverse
-- telecine), and both filters off. It uses the "deinterlace" property
-- so that a hardware deinterlacer will be used if available.
--
-- It overrides the default deinterlace toggle keybinding "D"
-- (shift+d), so that rather than merely cycling the "deinterlace" property
-- between on and off, it adds a "pullup" step to the cycle.
--
-- It provides OSD feedback as to the actual state of the two filters
-- after each cycle step/keypress.
--
-- Note: if hardware decoding is enabled, pullup filter will likely
-- fail to insert.
--
-- TODO: It might make sense to use hardware assisted vdpaupp=pullup,
-- if available, but I don't have hardware to test it. Patch welcome.

script_name = mp.get_script_name()
pullup_label = string.format("%s-pullup", script_name)

function pullup_on()
    for i,vf in pairs(mp.get_property_native('vf')) do
        if vf['label'] == pullup_label then
            return "yes"
        end
    end
    return "no"
end

function do_cycle()
    if pullup_on() == "yes" then
        -- if pullup is on remove it
        mp.command(string.format("vf del @%s:pullup", pullup_label))
        return
    elseif mp.get_property("deinterlace") == "yes" then
        -- if deinterlace is on, turn it off and insert pullup filter
        mp.set_property("deinterlace", "no")
        mp.command(string.format("vf add @%s:pullup", pullup_label))
        return
    else
        -- if neither is on, turn on deinterlace
        mp.set_property("deinterlace", "yes")
        return
    end
end

function cycle_deinterlace_pullup_handler()
    do_cycle()
    -- independently determine current state and give user feedback
    mp.osd_message(string.format("deinterlace: %s\n"..
                                     "pullup: %s",
                                 mp.get_property("deinterlace"),
                                 pullup_on()))
end

mp.add_key_binding("D", "cycle-deinterlace-pullup", cycle_deinterlace_pullup_handler)
