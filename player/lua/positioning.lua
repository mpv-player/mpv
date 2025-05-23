--[[
This file is part of mpv.

mpv is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

mpv is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
]]

local options = {
    toggle_align_to_cursor = false,
    suppress_osd = false,
}

require "mp.options".read_options(options, nil, function () end)

local function clamp(value, min, max)
    return math.min(math.max(value, min), max)
end

mp.add_key_binding(nil, "pan-x", function (t)
    if t.arg == nil or t.arg == "" then
        mp.osd_message("Usage: script-binding positioning/pan-x <amount>")
        return
    end

    if t.event == "up" then
        return
    end

    local dims = mp.get_property_native("osd-dimensions")
    local width = dims.w - dims.ml - dims.mr
    -- 1 video-align shifts the OSD by (width - osd-width) / 2 pixels, so the
    -- equation to find how much video-align to add to offset the OSD by
    -- osd-width is:
    -- x/1 = osd-width / ((width - osd-width) / 2)
    local amount = t.arg * t.scale * 2 * dims.w / (width - dims.w)
    mp.commandv("add", "video-align-x", clamp(amount, -2, 2))
end, { complex = true, scalable = true })

mp.add_key_binding(nil, "pan-y", function (t)
    if t.arg == nil or t.arg == "" then
        mp.osd_message("Usage: script-binding positioning/pan-y <amount>")
        return
    end

    if t.event == "up" then
        return
    end

    local dims = mp.get_property_native("osd-dimensions")
    local height = dims.h - dims.mt - dims.mb
    local amount = t.arg * t.scale * 2 * dims.h / (height - dims.h)
    mp.commandv("add", "video-align-y", clamp(amount, -2, 2))
end, { complex = true, scalable = true })

mp.add_key_binding(nil, "drag-to-pan", function (t)
    if t.event == "up" then
        mp.remove_key_binding("drag-to-pan-mouse-move")
        return
    end

    local dims = mp.get_property_native("osd-dimensions")
    local old_mouse_pos = mp.get_property_native("mouse-pos")
    local old_align_x = mp.get_property_native("video-align-x")
    local old_align_y = mp.get_property_native("video-align-y")

    mp.add_forced_key_binding("MOUSE_MOVE", "drag-to-pan-mouse-move", function ()
        local mouse_pos = mp.get_property_native("mouse-pos")
        -- 1 video-align shifts the OSD by (dimension - osd_dimension) / 2 pixels,
        -- so the equation to find how much video-align to add to offset the OSD
        -- by the difference in mouse position is:
        -- x/1 = (mouse_pos - old_mouse_pos) / ((dimension - osd_dimension) / 2)
        local align = old_align_x + 2 * (mouse_pos.x - old_mouse_pos.x)
                      / (dims.ml + dims.mr)
        mp.set_property("video-align-x", clamp(align, -1, 1))
        align = old_align_y + 2 * (mouse_pos.y - old_mouse_pos.y)
                / (dims.mt + dims.mb)
        mp.set_property("video-align-y", clamp(align, -1, 1))
    end)
end, { complex = true })


local align_to_cursor_bound = false

local function align_to_cursor(_, mouse_pos)
    local dims = mp.get_property_native("osd-dimensions")
    local align = (mouse_pos.x * 2 - dims.w) / dims.w
    mp.set_property("video-align-x", clamp(align, -1, 1))
    align = (mouse_pos.y * 2 - dims.h) / dims.h
    mp.set_property("video-align-y", clamp(align, -1, 1))
end

mp.add_key_binding(nil, "align-to-cursor", function (t)
    if options.toggle_align_to_cursor == false then
        if t.event == "down" then
            mp.observe_property("mouse-pos", "native", align_to_cursor)
        else
            mp.unobserve_property(align_to_cursor)
        end

        return
    end

    if t.event ~= "up" then
        return
    end

    if align_to_cursor_bound then
        mp.unobserve_property(align_to_cursor)
    else
        mp.observe_property("mouse-pos", "native", align_to_cursor)
    end
    align_to_cursor_bound = not align_to_cursor_bound
end, { complex = true })


mp.add_key_binding(nil, "cursor-centric-zoom", function (t)
    if t.arg == nil or t.arg == "" then
        mp.osd_message("Usage: script-binding positioning/cursor-centric-zoom <amount>")
        return
    end

    local amount = t.arg * t.scale

    local command = (options.suppress_osd and "no-osd " or "") ..
                    "add video-zoom " .. amount .. ";"

    local x, y
    local touch_positions = mp.get_property_native("touch-pos")
    if touch_positions[1] then
        for _, position in pairs(touch_positions) do
            x = x + position.x
            y = y + position.y
        end
        x = x / #touch_positions
        y = y / #touch_positions
    else
        local mouse_pos = mp.get_property_native("mouse-pos")
        x = mouse_pos.x
        y = mouse_pos.y
    end

    local dims = mp.get_property_native("osd-dimensions")
    local width = (dims.w - dims.ml - dims.mr) * 2^amount
    local height = (dims.h - dims.mt - dims.mb) * 2^amount

    local old_cursor_ml = dims.ml - x
    local cursor_ml = old_cursor_ml * 2^amount
    local ml = cursor_ml + x
    -- video/out/aspect.c:src_dst_split_scaling() defines ml as:
    -- ml = (osd-width - width) * (video-align-x + 1) / 2 + pan-x * width
    -- So video-align-x is:
    local align = 2 * (ml - mp.get_property_native("video-pan-x") * width)
                  / (dims.w - width) - 1
    command = command .. "no-osd set video-align-x " .. clamp(align, -1, 1) .. ";"

    local mt = (dims.mt - y) * 2^amount + y
    align = 2 * (mt - mp.get_property_native("video-pan-y") * height)
            / (dims.h - height) - 1
    command = command .. "no-osd set video-align-y " .. clamp(align, -1, 1)

    mp.command(command)
end, { complex = true, scalable = true })
