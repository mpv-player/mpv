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

local utils = require "mp.utils"
local input = require "mp.input"

local function show_error(message)
    mp.msg.error(message)
    if mp.get_property_native("vo-configured") then
        mp.osd_message(message)
    end
end

mp.add_forced_key_binding(nil, "select-playlist", function ()
    local playlist = {}
    local default_item

    for i, entry in ipairs(mp.get_property_native("playlist")) do
        playlist[i] = select(2, utils.split_path(entry.filename))

        if entry.playing then
            default_item = i
        end
    end

    if #playlist == 0 then
        show_error("The playlist is empty.")
        return
    end

    input.select({
        prompt = "Select a playlist entry:",
        items = playlist,
        default_item = default_item,
        submit = function (index)
            mp.commandv("playlist-play-index", index - 1)
        end,
    })
end)

local function format_track(track)
    return (track.selected and "●" or "○") ..
        (track.title and " " .. track.title or "") ..
        " (" .. (
            (track.lang and track.lang .. " " or "") ..
            (track.codec and track.codec .. " " or "") ..
            (track["demux-w"] and track["demux-w"] .. "x" .. track["demux-h"]
             .. " " or "") ..
            (track["demux-fps"] and not track.image
             and string.format("%.4f", track["demux-fps"]):gsub("%.?0*$", "") ..
             " fps " or "") ..
            (track["demux-channel-count"] and track["demux-channel-count"] ..
             " ch " or "") ..
            (track["codec-profile"] and track.type == "audio"
             and track["codec-profile"] .. " " or "") ..
            (track["demux-samplerate"] and track["demux-samplerate"] / 1000 ..
             " kHz " or "") ..
            (track.external and "external " or "")
        ):sub(1, -2) .. ")"
end

mp.add_forced_key_binding(nil, "select-track", function ()
    local tracks = {}

    for i, track in ipairs(mp.get_property_native("track-list")) do
        tracks[i] = track.type:sub(1, 1):upper() .. track.type:sub(2) .. ": " ..
                    format_track(track)
    end

    if #tracks == 0 then
        show_error("No available tracks.")
        return
    end

    input.select({
        prompt = "Select a track:",
        items = tracks,
        submit = function (id)
            local track = mp.get_property_native("track-list/" .. id - 1)
            if track then
                mp.set_property(track.type, track.selected and "no" or track.id)
            end
        end,
    })
end)

local function select_track(property, type, prompt, error)
    local tracks = {}
    local items = {}
    local default_item
    local track_id = mp.get_property_native(property)

    for _, track in ipairs(mp.get_property_native("track-list")) do
        if track.type == type then
            tracks[#tracks + 1] = track
            items[#items + 1] = format_track(track)

            if track.id == track_id then
                default_item = #items
            end
        end
    end

    if #items == 0 then
        show_error(error)
        return
    end

    input.select({
        prompt = prompt,
        items = items,
        default_item = default_item,
        submit = function (id)
            mp.set_property(property, tracks[id].selected and "no" or tracks[id].id)
        end,
    })
end

mp.add_forced_key_binding(nil, "select-sid", function ()
    select_track("sid", "sub", "Select a subtitle:", "No available subtitles.")
end)

mp.add_forced_key_binding(nil, "select-secondary-sid", function ()
    select_track("secondary-sid", "sub", "Select a secondary subtitle:",
                 "No available subtitles.")
end)

mp.add_forced_key_binding(nil, "select-aid", function ()
    select_track("aid", "audio", "Select an audio track:",
                 "No available audio tracks.")
end)

mp.add_forced_key_binding(nil, "select-vid", function ()
    select_track("vid", "video", "Select a video track:",
                 "No available video tracks.")
end)

local function format_time(t, duration)
    local h = math.floor(t / (60 * 60))
    t = t - (h * 60 * 60)
    local m = math.floor(t / 60)
    local s = t - (m * 60)

    if duration >= 60 * 60 or h > 0 then
        return string.format("%.2d:%.2d:%.2d", h, m, s)
    end

    return string.format("%.2d:%.2d", m, s)
end

mp.add_forced_key_binding(nil, "select-chapter", function ()
    local chapters = {}
    local default_item = mp.get_property_native("chapter")

    if default_item == nil then
        show_error("No available chapters.")
        return
    end

    local duration = mp.get_property_native("duration", math.huge)

    for i, chapter in ipairs(mp.get_property_native("chapter-list")) do
        chapters[i] = format_time(chapter.time, duration) .. " " .. chapter.title
    end

    input.select({
        prompt = "Select a chapter:",
        items = chapters,
        default_item = default_item + 1,
        submit = function (chapter)
            mp.set_property("chapter", chapter - 1)
        end,
    })
end)

mp.add_forced_key_binding(nil, "select-subtitle-line", function ()
    local sub = mp.get_property_native("current-tracks/sub")

    if sub == nil then
        show_error("No subtitle is loaded.")
        return
    end

    if sub.external and sub["external-filename"]:find("^edl://") then
        sub["external-filename"] = sub["external-filename"]:match('https?://.*')
                                   or sub["external-filename"]
    end

    local r = mp.command_native({
        name = "subprocess",
        capture_stdout = true,
        args = sub.external
            and {"ffmpeg", "-loglevel", "error", "-i", sub["external-filename"],
                 "-f", "lrc", "-map_metadata", "-1", "-fflags", "+bitexact", "-"}
            or {"ffmpeg", "-loglevel", "error", "-i", mp.get_property("path"),
                "-map", "s:" .. sub["id"] - 1, "-f", "lrc", "-map_metadata",
                "-1", "-fflags", "+bitexact", "-"}
    })

    if r.error_string == "init" then
        show_error("Failed to extract subtitles: ffmpeg not found.")
        return
    elseif r.status ~= 0 then
        show_error("Failed to extract subtitles.")
        return
    end

    local sub_lines = {}
    local sub_times = {}
    local default_item
    local sub_start = mp.get_property_native("sub-start",
                                             mp.get_property_native("time-pos"))
    local duration = mp.get_property_native("duration", math.huge)

    -- Strip HTML and ASS tags.
    for line in r.stdout:gsub("<.->", ""):gsub("{\\.-}", ""):gmatch("[^\n]+") do
        -- ffmpeg outputs LRCs with minutes > 60 instead of adding hours.
        sub_times[#sub_times + 1] = line:match("%d+") * 60 + line:match(":([%d%.]*)")
        sub_lines[#sub_lines + 1] = format_time(sub_times[#sub_times], duration) ..
                                    " " .. line:gsub(".*]", "", 1)

        if sub_times[#sub_times] <= sub_start then
            default_item = #sub_times
        end
    end

    -- Handle sub-start of embedded subs being slightly earlier than
    -- ffmpeg's timestamps.
    sub_start = mp.get_property_native("sub-start")
    if sub_start and default_item and sub_times[default_item] < sub_start and
       sub_lines[default_item + 1] then
        default_item = default_item + 1
    end

    input.select({
        prompt = "Select a line to seek to:",
        items = sub_lines,
        default_item = default_item,
        submit = function (index)
            -- Add an offset to seek to the correct line while paused without a
            -- video track.
            local offset = mp.get_property_native("current-tracks/video/image") == false
                           and 0 or .09
            mp.commandv("seek", sub_times[index] + offset, "absolute")
        end,
    })
end)

mp.add_forced_key_binding(nil, "select-audio-device", function ()
    local devices = mp.get_property_native("audio-device-list")
    local items = {}
    -- This is only useful if an --audio-device has been explicitly set,
    -- otherwise its value is just auto and there is no current-audio-device
    -- property.
    local selected_device = mp.get_property("audio-device")
    local default_item

    if #devices == 0 then
        show_error("No available audio devices.")
        return
    end

    for i, device in ipairs(devices) do
        items[i] = device.name .. " (" .. device.description .. ")"

        if device.name == selected_device then
            default_item = i
        end
    end

    input.select({
        prompt = "Select an audio device:",
        items = items,
        default_item = default_item,
        submit = function (id)
            mp.set_property("audio-device", devices[id].name)
        end,
    })
end)

mp.add_forced_key_binding(nil, "select-binding", function ()
    local bindings = {}

    for _, binding in pairs(mp.get_property_native("input-bindings")) do
        if binding.priority >= 0 and (
               bindings[binding.key] == nil or
               (bindings[binding.key].is_weak and not binding.is_weak) or
               (binding.is_weak == bindings[binding.key].is_weak and
                binding.priority > bindings[binding.key].priority)
        ) then
            bindings[binding.key] = binding
        end
    end

    local items = {}
    for _, binding in pairs(bindings) do
        if binding.cmd ~= "ignore" then
            items[#items + 1] = binding.key .. " " .. binding.cmd
        end
    end

    table.sort(items)

    input.select({
        prompt = "Select a binding:",
        items = items,
        submit = function (i)
            mp.command(items[i]:gsub("^.- ", ""))
        end,
    })
end)

local properties = {}

local function add_property(property, value)
    value = value or mp.get_property_native(property)

    if type(value) == "table" and next(value) then
        for key, val in pairs(value) do
            add_property(property .. "/" .. key, val)
        end
    else
        properties[#properties + 1] = property .. ": " .. utils.to_string(value)
    end
end

mp.add_forced_key_binding(nil, "show-properties", function ()
    properties = {}

    -- Don't log errors for renamed and removed properties.
    local msg_level_backup = mp.get_property("msg-level")
    mp.set_property("msg-level", msg_level_backup == "" and "cplayer=no"
                                 or msg_level_backup .. ",cplayer=no")

    for _, property in pairs(mp.get_property_native("property-list")) do
        add_property(property)
    end

    mp.set_property("msg-level", msg_level_backup)

    add_property("current-tracks/audio")
    add_property("current-tracks/video")
    add_property("current-tracks/sub")
    add_property("current-tracks/sub2")

    table.sort(properties)

    input.select({
        prompt = "Inspect a property:",
        items = properties,
        submit = function (i)
            if mp.get_property_native("vo-configured") then
                mp.commandv("expand-properties", "show-text",
                            (#properties[i] > 100 and
                             "${osd-ass-cc/0}{\\fs9}${osd-ass-cc/1}" or "") ..
                            "$>" .. properties[i], 20000)
            else
                mp.msg.info(properties[i])
            end
        end,
    })
end)
