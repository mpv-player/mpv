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

local options = {
    history_date_format = "%Y-%m-%d %H:%M:%S",
    hide_history_duplicates = true,
}

require "mp.options".read_options(options, nil, function () end)

local function show_warning(message)
    mp.msg.warn(message)
    if mp.get_property_native("vo-configured") then
        mp.osd_message(message)
    end
end

local function show_error(message)
    mp.msg.error(message)
    if mp.get_property_native("vo-configured") then
        mp.osd_message(message)
    end
end

mp.add_key_binding(nil, "select-playlist", function ()
    local playlist = {}
    local default_item
    local show = mp.get_property_native("osd-playlist-entry")
    local trailing_slash_pattern = mp.get_property("platform") == "windows"
                                   and "[/\\]+$" or "/+$"

    for i, entry in ipairs(mp.get_property_native("playlist")) do
        playlist[i] = entry.title
        if not playlist[i] or show ~= "title" then
            playlist[i] = entry.filename
            if not playlist[i]:find("://") then
                playlist[i] = select(2, utils.split_path(
                    playlist[i]:gsub(trailing_slash_pattern, "")))
            end
        end
        if entry.title and show == "both" then
            playlist[i] = string.format("%s (%s)", entry.title, playlist[i])
        end

        if entry.playing then
            default_item = i
        end
    end

    if #playlist == 0 then
        show_warning("The playlist is empty.")
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

local function format_flags(track)
    local flags = ""

    for _, flag in ipairs({
        "default", "forced", "dependent", "visual-impaired", "hearing-impaired",
        "image", "external"
    }) do
        if track[flag] then
            flags = flags .. flag .. " "
        end
    end

    if flags == "" then
        return ""
    end

    return " [" .. flags:sub(1, -2) .. "]"
end

local function format_track(track)
    local bitrate = track["demux-bitrate"] or track["hls-bitrate"]

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
             "ch " or "") ..
            (track["codec-profile"] and track.type == "audio"
             and track["codec-profile"] .. " " or "") ..
            (track["demux-samplerate"] and track["demux-samplerate"] / 1000 ..
             " kHz " or "") ..
            (bitrate and string.format("%.0f", bitrate / 1000) ..
             " kbps " or "")
        ):sub(1, -2) .. ")" .. format_flags(track)
end

mp.add_key_binding(nil, "select-track", function ()
    local tracks = {}

    for i, track in ipairs(mp.get_property_native("track-list")) do
        tracks[i] = (track.image and "Image" or
                     track.type:sub(1, 1):upper() .. track.type:sub(2)) .. ": " ..
                    format_track(track)
    end

    if #tracks == 0 then
        show_warning("No available tracks.")
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

local function select_track(property, type, prompt, warning)
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
        show_warning(warning)
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

mp.add_key_binding(nil, "select-sid", function ()
    select_track("sid", "sub", "Select a subtitle:", "No available subtitles.")
end)

mp.add_key_binding(nil, "select-secondary-sid", function ()
    select_track("secondary-sid", "sub", "Select a secondary subtitle:",
                 "No available subtitles.")
end)

mp.add_key_binding(nil, "select-aid", function ()
    select_track("aid", "audio", "Select an audio track:",
                 "No available audio tracks.")
end)

mp.add_key_binding(nil, "select-vid", function ()
    select_track("vid", "video", "Select a video track:",
                 "No available video tracks.")
end)

local function format_time(t, duration)
    local fmt = math.max(t, duration) >= 60 * 60 and "%H:%M:%S" or "%M:%S"
    return mp.format_time(t, fmt)
end

mp.add_key_binding(nil, "select-chapter", function ()
    local chapters = {}
    local default_item = mp.get_property_native("chapter")

    if default_item == nil then
        show_warning("No available chapters.")
        return
    end

    local duration = mp.get_property_native("duration", math.huge)

    for i, chapter in ipairs(mp.get_property_native("chapter-list")) do
        chapters[i] = format_time(chapter.time, duration) .. " " .. chapter.title
    end

    input.select({
        prompt = "Select a chapter:",
        items = chapters,
        default_item = default_item > -1 and default_item + 1,
        submit = function (chapter)
            mp.set_property("chapter", chapter - 1)
        end,
    })
end)

mp.add_key_binding(nil, "select-edition", function ()
    local edition_list = mp.get_property_native("edition-list")

    if edition_list == nil or #edition_list < 2 then
        show_warning("No available editions.")
        return
    end

    local editions = {}

    for i, edition in ipairs(edition_list) do
        editions[i] = edition.title or ("Edition " .. edition.id + 1)
    end

    input.select({
        prompt = "Select an edition:",
        items = editions,
        default_item = mp.get_property_native("current-edition") + 1,
        submit = function (edition)
            mp.set_property("edition", edition - 1)
        end,
    })
end)

mp.add_key_binding(nil, "select-subtitle-line", function ()
    local sub = mp.get_property_native("current-tracks/sub")

    if sub == nil then
        show_warning("No subtitle is loaded.")
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
    local delay = mp.get_property_native("sub-delay")
    local time_pos = mp.get_property_native("time-pos") - delay
    local duration = mp.get_property_native("duration", math.huge)

    -- Strip HTML and ASS tags.
    for line in r.stdout:gsub("<.->", ""):gsub("{\\.-}", ""):gmatch("[^\n]+") do
        -- ffmpeg outputs LRCs with minutes > 60 instead of adding hours.
        sub_times[#sub_times + 1] = line:match("%d+") * 60 + line:match(":([%d%.]*)")
        sub_lines[#sub_lines + 1] = format_time(sub_times[#sub_times], duration) ..
                                    " " .. line:gsub(".*]", "", 1)

        if sub_times[#sub_times] <= time_pos then
            default_item = #sub_times
        end
    end

    input.select({
        prompt = "Select a line to seek to:",
        items = sub_lines,
        default_item = default_item,
        submit = function (index)
            -- Add an offset to seek to the correct line while paused without a
            -- video track.
            if mp.get_property_native("current-tracks/video/image") ~= false then
                delay = delay + 0.1
            end

            mp.commandv("seek", sub_times[index] + delay, "absolute")
        end,
    })
end)

mp.add_key_binding(nil, "select-audio-device", function ()
    local devices = mp.get_property_native("audio-device-list")
    local items = {}
    -- This is only useful if an --audio-device has been explicitly set,
    -- otherwise its value is just auto and there is no current-audio-device
    -- property.
    local selected_device = mp.get_property("audio-device")
    local default_item

    if #devices == 0 then
        show_warning("No available audio devices.")
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

local function format_history_entry(entry)
    local status
    status, entry.time = pcall(os.date, options.history_date_format, entry.time)

    if not status then
        mp.msg.warn(entry.time)
    end

    local item = "(" .. entry.time .. ") "

    if entry.title then
        return item .. entry.title .. " (" .. entry.path .. ")"
    end

    if entry.path:find("://") then
        return item .. entry.path
    end

    local directory, filename = utils.split_path(entry.path)

    return item .. filename .. " (" .. directory .. ")"
end

mp.add_key_binding(nil, "select-watch-history", function ()
    local history_file_path = mp.command_native(
        {"expand-path", mp.get_property("watch-history-path")})
    local history_file, error_message = io.open(history_file_path)
    if not history_file then
        show_warning(mp.get_property_native("save-watch-history")
                     and error_message
                     or "Enable --save-watch-history to jump to recently played files.")
        return
    end

    local all_entries = {}
    local line_num = 1
    for line in history_file:lines() do
        local entry = utils.parse_json(line)
        if entry and entry.path then
            all_entries[#all_entries + 1] = entry
        else
            mp.msg.warn(history_file_path .. ": Parse error at line " .. line_num)
        end
        line_num = line_num + 1
    end
    history_file:close()

    local entries = {}
    local items = {}
    local seen = {}

    for i = #all_entries, 1, -1 do
        local entry = all_entries[i]
        if not seen[entry.path] or not options.hide_history_duplicates then
            seen[entry.path] = true
            entries[#entries + 1] = entry
            items[#items + 1] = format_history_entry(entry)
        end
    end

    items[#items+1] = "Clear history"

    input.select({
        prompt = "Select a file:",
        items = items,
        submit = function (i)
            if entries[i] then
                mp.commandv("loadfile", entries[i].path)
                return
            end

            error_message = select(2, os.remove(history_file_path))
            if error_message then
                show_error(error_message)
            else
                mp.osd_message("History cleared.")
            end
        end,
    })
end)

mp.add_key_binding(nil, "select-watch-later", function ()
    local watch_later_dir = mp.get_property("current-watch-later-dir")

    if not watch_later_dir then
        show_warning("No watch later files found.")
        return
    end

    local watch_later_files = {}

    for i, file in ipairs(utils.readdir(watch_later_dir, "files") or {}) do
        watch_later_files[i] = watch_later_dir .. "/" .. file
    end

    if #watch_later_files == 0 then
        show_warning("No watch later files found.")
        return
    end

    local files = {}
    for _, watch_later_file in pairs(watch_later_files) do
        local file_handle = io.open(watch_later_file)
        if file_handle then
            local line = file_handle:read()
            if line and line ~= "# redirect entry" and line:find("^#") then
                files[#files + 1] = {line:sub(3), utils.file_info(watch_later_file).mtime}
            end
            file_handle:close()
        end
    end

    if #files == 0 then
        show_warning(mp.get_property_native("write-filename-in-watch-later-config")
            and "No watch later files found."
            or "Enable --write-filename-in-watch-later-config to select recent files.")
        return
    end

    table.sort(files, function (i, j)
        return i[2] > j[2]
    end)

    local items = {}
    for i, file in ipairs(files) do
        items[i] = os.date("(%Y-%m-%d) ", file[2]) .. file[1]
    end

    input.select({
        prompt = "Select a file:",
        items = items,
        submit = function (i)
            mp.commandv("loadfile", files[i][1])
        end,
    })
end)

mp.add_key_binding(nil, "select-binding", function ()
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

mp.add_key_binding(nil, "show-properties", function ()
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

mp.add_key_binding(nil, "menu", function ()
    local sub_track_count = 0
    local audio_track_count = 0
    local video_track_count = 0
    local text_sub_selected = false
    local is_disc = mp.get_property("current-demuxer") == "disc"

    local image_sub_codecs = {["dvd_subtitle"] = true, ["hdmv_pgs_subtitle"] = true}

    for _, track in pairs(mp.get_property_native("track-list")) do
        if track.type == "sub" then
            sub_track_count = sub_track_count + 1

            if track["main-selection"] == 0 and not image_sub_codecs[track.codec] then
                text_sub_selected = true
            end
        elseif track.type == "audio" then
            audio_track_count = audio_track_count + 1
        elseif track.type == "video" then
            video_track_count = video_track_count + 1
        end
    end

    local menu = {
        {"Subtitles", "script-binding select/select-sid", sub_track_count > 0},
        {"Secondary subtitles", "script-binding select/select-secondary-sid", sub_track_count > 1},
        {"Subtitle lines", "script-binding select/select-subtitle-line", text_sub_selected},
        {"Audio tracks", "script-binding select/select-aid", audio_track_count > 1},
        {"Video tracks", "script-binding select/select-vid", video_track_count > 1},
        {"Playlist", "script-binding select/select-playlist",
         mp.get_property_native("playlist-count") > 1},
        {"Chapters", "script-binding select/select-chapter", mp.get_property("chapter")},
        {is_disc and "Titles" or "Editions", "script-binding select/select-edition",
         mp.get_property_native("edition-list/count", 0) > 1},
        {"Audio devices", "script-binding select/select-audio-device", audio_track_count > 0},
        {"Key bindings", "script-binding select/select-binding", true},
        {"History", "script-binding select/select-watch-history", true},
        {"Watch later", "script-binding select/select-watch-later", true},
        {"Stats for nerds", "script-binding stats/display-page-1-toggle", true},
        {"File info", "script-binding stats/display-page-5-toggle", mp.get_property("filename")},
        {"Help", "script-binding stats/display-page-4-toggle", true},
    }

    local labels = {}
    local commands = {}

    for _, entry in ipairs(menu) do
        if entry[3] then
            labels[#labels + 1] = entry[1]
            commands[#commands + 1] = entry[2]
        end
    end

    input.select({
        prompt = "",
        items = labels,
        keep_open = true,
        submit = function (i)
            mp.command(commands[i])

            if not commands[i]:find("^script%-binding select/") then
                input.terminate()
            end
        end,
    })
end)
