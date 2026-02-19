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
    menu_conf_path = "~~/menu.conf",
    max_playlist_items = 25,
    populate_menu_data = true,
}

require "mp.options".read_options(options, nil, function () end)

local platform = mp.get_property("platform")

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

local function to_map(t)
    local map = {}

    for _, value in pairs(t) do
        map[value] = true
    end

    return map
end

local function format_playlist_entry(entry, show)
    local item = entry.title

    if not item or show ~= "title" then
        item = entry.filename

        if not item:find("://") then
            item = select(2, utils.split_path(item))
        end

        if entry.title and show == "both" then
            item = entry.title .. " (" .. item .. ")"
        end
    end

    return item
end

mp.add_key_binding(nil, "select-playlist", function ()
    local playlist = {}
    local default_item
    local show = mp.get_property_native("osd-playlist-entry")

    for i, entry in ipairs(mp.get_property_native("playlist")) do
        playlist[i] = format_playlist_entry(entry, show)

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

local function format_track_type(track)
    return (track.image
                and "Image"
                or track.type:sub(1, 1):upper() .. track.type:sub(2)) ..
           ": "
end

local function format_flags(track)
    local flags = ""

    for _, flag in ipairs({
        "default", "forced", "dependent", "visual-impaired", "hearing-impaired",
        "original", "commentary", "image", "external"
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
        tracks[i] = format_track_type(track) .. format_track(track)
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

local function format_edition(edition)
    return edition.title or ("Edition " .. edition.id + 1)
end

mp.add_key_binding(nil, "select-edition", function ()
    local edition_list = mp.get_property_native("edition-list")

    if edition_list == nil or #edition_list < 2 then
        show_warning("No available editions.")
        return
    end

    local editions = {}
    local default_item = mp.get_property_native("current-edition")

    for i, edition in ipairs(edition_list) do
        editions[i] = format_edition(edition)
    end

    input.select({
        prompt = "Select an edition:",
        items = editions,
        default_item = default_item > -1 and default_item + 1,
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
    local sub_content = {}

    -- Strip HTML and ASS tags and process subtitles
    for line in r.stdout:gmatch("[^\n]+") do
        -- Clean up tags
        local sub_line = line:gsub("<.->", "")                -- Strip HTML tags
                             :gsub("\\h+", " ")               -- Replace '\h' tag
                             :gsub("{[\\=].-}", "")           -- Remove ASS formatting
                             :gsub(".-]", "", 1)              -- Remove time info prefix
                             :gsub("^%s*(.-)%s*$", "%1")      -- Strip whitespace
                             :gsub("^m%s[mbl%s%-%d%.]+$", "") -- Remove graphics code

        if sub.codec == "text" or (sub_line ~= "" and sub_line:match("^%s+$") == nil) then
            local sub_time = line:match("%d+") * 60 + line:match(":([%d%.]*)")
            local time_seconds = math.floor(sub_time)
            sub_content[time_seconds] = sub_content[time_seconds] or {}
            sub_content[time_seconds][sub_line] = true
        end
    end

    -- Process all timestamps and content into selectable subtitle list
    for time_seconds, contents in pairs(sub_content) do
        for sub_line in pairs(contents) do
            sub_times[#sub_times + 1] = time_seconds
            sub_lines[#sub_lines + 1] = format_time(time_seconds, duration) .. " " .. sub_line
        end
    end

    -- Generate time -> subtitle mapping
    local time_to_lines = {}
    for i = 1, #sub_times do
        local time = sub_times[i]
        local line = sub_lines[i]

        if not time_to_lines[time] then
            time_to_lines[time] = {}
        end
        table.insert(time_to_lines[time], line)
    end

    -- Sort by timestamp
    local sorted_sub_times = {}
    for i = 1, #sub_times do
        sorted_sub_times[i] = sub_times[i]
    end
    table.sort(sorted_sub_times)

    -- Use a helper table to avoid duplicates
    local added_times = {}

    -- Rebuild sub_lines and sub_times based on the sorted timestamps
    local sorted_sub_lines = {}
    for _, sub_time in ipairs(sorted_sub_times) do
        -- Iterate over all subtitle content for this timestamp
        if not added_times[sub_time] then
            added_times[sub_time] = true
            for _, line in ipairs(time_to_lines[sub_time]) do
                table.insert(sorted_sub_lines, line)
            end
        end
    end

    -- Use the sorted subtitle list
    sub_lines = sorted_sub_lines
    sub_times = sorted_sub_times

    -- Get the default item (last subtitle before current time position)
    for i, sub_time in ipairs(sub_times) do
        if sub_time <= time_pos then
            default_item = i
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

local function format_audio_device(device)
    return device.name .. " (" .. device.description .. ")"
end

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
        items[i] = format_audio_device(device)

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

local function get_active_bindings()
    local bindings = {}

    for _, binding in pairs(mp.get_property_native("input-bindings")) do
        if binding.priority >= 0 and (
               bindings[binding.key] == nil or
               (bindings[binding.key].is_weak and not binding.is_weak) or
               (binding.is_weak == bindings[binding.key].is_weak and
                binding.priority > bindings[binding.key].priority)
        ) and not binding.section:find("^input_forced_")
          -- OSC sections
          and binding.section ~= "input"
          and binding.section ~= "window-controls" then
            bindings[binding.key] = binding
        end
    end

    return bindings
end

mp.add_key_binding(nil, "select-binding", function ()
    local items = {}
    for _, binding in pairs(get_active_bindings()) do
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

local function system_open(path)
    local args
    if platform == "windows" then
        args = {"rundll32", "url.dll,FileProtocolHandler", path}
    elseif platform == "darwin" then
        args = {"open", path}
    else
        args = {"gio", "open", path}
    end

    mp.commandv("run", unpack(args))
end

local function edit_config_file(filename)
    if not mp.get_property_bool("config") then
        show_warning("Editing config files with --no-config is not supported.")
        return
    end

    local path = mp.find_config_file(filename)

    if not path then
        path = mp.command_native({"expand-path", "~~/" .. filename})
        local file_handle, error_message = io.open(path, "w")

        if not file_handle then
            show_error(error_message)
            return
        end

        file_handle:close()
    end

    system_open(path)
end

mp.add_key_binding(nil, "edit-config-file", function ()
    edit_config_file("mpv.conf")
end)

mp.add_key_binding(nil, "edit-input-conf", function ()
    edit_config_file("input.conf")
end)

mp.add_key_binding(nil, "open-docs", function ()
    system_open("https://mpv.io/manual/")
end)

mp.add_key_binding(nil, "open-chat", function ()
    system_open("https://web.libera.chat/#mpv")
end)

mp.add_key_binding(nil, "menu", function ()
    local sub_track_count = 0
    local audio_track_count = 0
    local video_track_count = 0
    local text_sub_selected = false
    local is_disc = mp.get_property("current-demuxer") == "disc"

    local image_sub_codecs = to_map({"dvb_subtitle", "dvd_subtitle", "hdmv_pgs_subtitle"})

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
        {"Playback statistics", "script-binding stats/display-page-1-toggle", true},
        {"File information", "script-binding stats/display-page-5-toggle",
         mp.get_property("filename")},
        {"Edit config file", "script-binding select/edit-config-file", true},
        {"Edit key bindings", "script-binding select/edit-input-conf", true},
        {"Help", "script-binding stats/display-page-4-toggle", true},
        {"Online documentation", "script-binding select/open-docs", true},
        {"Support", "script-binding select/open-chat", true},
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
        items = labels,
        keep_open = true,
        submit = function (i)
            mp.command(commands[i])

            if not commands[i]:find("^script%-binding select/select") then
                input.terminate()
            end
        end,
    })
end)


local menu = {} -- contains wrappers of menu_data's items
local menu_data = {}
local observed_properties = {}
local property_cache = {}
local active_bindings = {}
local property_set = {}
local property_items = {}
local have_dirty_items = false
local current_item

local function on_property_change(name, value)
    property_cache[name] = value

    if property_items[name] then
        for item, _ in pairs(property_items[name]) do
            item.dirty = true
        end
        have_dirty_items = true
    end
end

local none_getters = {
    -- Only retrieve playlist when playlist-count is not huge to not kill
    -- performance. playlist is observed with type none to receive every
    -- MP_EVENT_CHANGE_PLAYLIST.
    playlist = function ()
        if mp.get_property_native("playlist-count") < 100 then
            return mp.get_property_native("playlist")
        end
    end,
}

local function on_none_property_change(name)
    on_property_change(name, none_getters[name]())
end

function _G.get(name, default)
    if not observed_properties[name] then
        local result, err = (none_getters[name] or mp.get_property_native)(name)

        if err == "property not found" and not property_set(name:match("^([^/]+)")) then
            mp.msg.error("Property '" .. name .. "' was not found.")
            return default
        end

        observed_properties[name] = true
        property_cache[name] = result

        if none_getters[name] then
            mp.observe_property(name, "none", on_none_property_change)
        else
            mp.observe_property(name, "native", on_property_change)
        end
    end

    if current_item then
        if not property_items[name] then
            property_items[name] = {}
        end

        property_items[name][current_item] = true
    end

    if property_cache[name] == nil then
        return default
    end

    return property_cache[name]
end

local function magic_get(name)
    return get(name:gsub("_", "-"), nil)
end

local evil_magic = {}
setmetatable(evil_magic, {
    __index = function(_, key)
        if _G[key] ~= nil then
            return _G[key]
        end

        return magic_get(key)
    end,
})

_G.p = {}
setmetatable(p, {
    __index = function(_, key)
        return magic_get(key)
    end,
})

local function compile_condition(chunk, chunkname)
    chunk = "return " .. chunk
    chunkname = 'Menu entry "' .. chunkname .. '"'

    local compiled_chunk, err

    -- luacheck: push
    -- luacheck: ignore setfenv loadstring
    if setfenv then -- lua 5.1
        compiled_chunk, err = loadstring(chunk, chunkname)
        if compiled_chunk then
            setfenv(compiled_chunk, evil_magic)
        end
    else -- lua 5.2
        compiled_chunk, err = load(chunk, chunkname, "t", evil_magic)
    end
    -- luacheck: pop

    if not compiled_chunk then
        mp.msg.error(chunkname .. " : " .. err)
        compiled_chunk = function() return false end
    end

    return compiled_chunk
end

local function evaluate_condition(chunk, chunkname)
    local status, result
    status, result = pcall(chunk)

    if not status then
        mp.msg.verbose(chunkname .. " error on evaluating: " .. result)
        return false
    end

    return not not result
end

local function toggle_state(states, state, add)
    for i, existing_state in ipairs(states) do
        if existing_state == state then
            if add then
                return
            end

            table.remove(states, i)
        end
    end

    if add then
        states[#states + 1] = state
    end
end

local function on_idle()
    if not have_dirty_items then
        return
    end

    have_dirty_items = false

    for _, item in pairs(menu) do
        if item.dirty then
            item:update()
            item.dirty = false
        end
    end

    mp.set_property_native("menu-data", menu_data)
end

-- quote string and escape it in JSON-style
local function quote(str)
    return utils.format_json(str or "")
end

local function clamp_submenu(submenu, max, cmd)
    if #submenu <= max then
       return submenu
    end

    local mid = 1
    for i, item in pairs(submenu) do
        if item.state then
            mid = i
            break
        end
    end

    local offset = math.floor(max / 2)
    local first = mid + 1 - offset
    local last = mid + offset

    if first < 1 then
        first = 1
        last = max
    end

    if last > #submenu then
        first = math.max(#submenu - max + 1, 1)
        last = #submenu
    end

    local clamped = {}

    if first > 1 then
        clamped[1] = {
            title = "…",
            cmd = cmd,
            shortcut = first - 1 .. " more",
        }
    end

    for i = first, last do
        clamped[#clamped + 1] = submenu[i]
    end

    if last < #submenu then
        clamped[#clamped + 1] = {
            title = "…",
            cmd = cmd,
            shortcut = #submenu - last .. " more",
        }
    end

    return clamped
end

local function playlist()
    local show = get("osd-playlist-entry")

    if not get("playlist") then
        return
    end

    local items = {}

    for i, entry in ipairs(get("playlist")) do
        items[i] = {
            title = format_playlist_entry(entry, show):gsub("&", "&&"),
            cmd = "playlist-play-index " .. (i - 1)
        }

        if entry.current then
            items[i].state = {"checked"}
        end
    end

    return clamp_submenu(items, options.max_playlist_items,
                         "script-binding select/select-playlist")
end

local function tracks(property, track_type)
    local items = {}
    local track_list = get("track-list")
    local last_type

    local positions = {video = 0, audio = 1, sub = 2}
    table.sort(track_list, function (i, j)
        if i.type ~= j.type then
            return positions[i.type] < positions[j.type]
        end

        return i.id < j.id
    end)

    for _, track in ipairs(track_list) do
        if not track_type or track.type == track_type then
            if last_type and track.type ~= last_type then
                items[#items + 1] = { type = "separator" }
            end
            last_type = track.type

            items[#items + 1] = {
                title = (track_type and "" or format_track_type(track)) ..
                        -- Remove the circles since checkmarks are already added.
                        format_track(track):sub(5):gsub("&", "&&"),
                cmd = "set " .. (property or track.type) .. " " .. track.id,
            }

            if track.selected then
                items[#items].cmd = "set " .. (property or track.type) .. " no"
                items[#items].state = {"checked"}
            end
        end
    end

    return items
end

local function chapters()
    local items = {}
    local current_chapter = get("chapter", -1)
    local duration = mp.get_property_native("duration", math.huge)

    for i, chapter in ipairs(get("chapter-list")) do
        items[i] = {
            title = chapter.title:gsub("&", "&&"),
            cmd = "set chapter " .. (i - 1),
            shortcut = format_time(chapter.time, duration),
        }

        if i == current_chapter + 1 then
            items[i].state = {"checked"}
        end
    end

    return items
end

local function editions()
    local items = {}
    local current_edition = get("current-edition", -1)

    for i, edition in ipairs(get("edition-list", {})) do
        items[i] = {
            title = format_edition(edition):gsub("&", "&&"),
            cmd = "set edition " .. (i - 1),
        }

        if i == current_edition + 1 then
            items[i].state = {"checked"}
        end
    end

    return items
end

local function audio_devices()
    local items = {}
    local selected_device = get("audio-device")

    for i, device in ipairs(get("audio-device-list")) do
        items[i] = {
            title = format_audio_device(device):gsub("&", "&&"),
            cmd = "set audio-device " .. quote(device.name),
        }

        if device.name == selected_device then
            items[i].state = {"checked"}
        end
    end

    return items
end

local function profiles()
    local builtin_profiles = to_map({
        "box", "builtin-pseudo-gui", "default", "encoding", "fast", "gpu-hq",
        "high-quality", "libmpv", "low-latency", "osd-box", "pseudo-gui",
        "sub-box", "sw-fast"
    })

    local user_profiles = {}
    for i, profile in pairs(get("profile-list")) do
        if not builtin_profiles[profile.name] then
            user_profiles[i] = profile.name
        end
    end
    table.sort(user_profiles)

    local items = {}

    for i, profile in ipairs({
        "fast", "high-quality", "osd-box", "sub-box", "box",
    }) do
        items[i] = {
            title = profile,
            cmd = "apply-profile " .. profile,
        }
    end

    if user_profiles[1] then
        items[#items + 1] = { type = "separator" }
    end

    for _, profile in ipairs(user_profiles) do
        items[#items + 1] = {
            title = profile:gsub("&", "&&"),
            cmd = "apply-profile " .. quote(profile),
        }
    end

    return items
end

local builtin_submenus = {
    ["$playlist"] = playlist,
    ["$tracks"] = function () return tracks() end,
    ["$video-tracks"] = function () return tracks("video", "video") end,
    ["$audio-tracks"] = function () return tracks("audio", "audio") end,
    ["$sub-tracks"] = function () return tracks("sub", "sub") end,
    ["$secondary-sub-tracks"] = function () return tracks("secondary-sid", "sub") end,
    ["$chapters"] = chapters,
    ["$editions"] = editions,
    ["$audio-devices"] = audio_devices,
    ["$profiles"] = profiles,
}

local submenu_commands = {
    ["$playlist"] = "script-binding select/select-playlist",
    ["$tracks"] = "script-binding select/select-track",
    ["$video-tracks"] = "script-binding select/select-vid",
    ["$audio-tracks"] = "script-binding select/select-aid",
    ["$sub-tracks"] = "script-binding select/select-sid",
    ["$secondary-sub-tracks"] = "script-binding select/select-secondary-sid",
    ["$chapters"] = "script-binding select/select-chapter",
    ["$editions"] = "script-binding select/select-edition",
    ["$audio-devices"] = "script-binding select/select-audio-device",
}

local function get_shortcut(cmd)
    local shortcuts = {}
    local uncommon_keys = to_map({
        "MBTN_BACK", "MBTN_FORWARD", "POWER", "PLAY", "PAUSE", "PLAYPAUSE",
        "PLAYONLY", "PAUSEONLY", "STOP", "FORWARD", "REWIND", "NEXT", "PREV",
        "VOLUME_UP", "VOLUME_DOWN", "MUTE", "CLOSE_WIN",
    })

    for _, binding in pairs(active_bindings) do
        if binding.cmd == cmd and not uncommon_keys[binding.key]
           and not binding.key:find("KP_") then
            shortcuts[#shortcuts + 1] = binding.key
        end
    end

    return table.concat(shortcuts, ",")
end

local function update_builtin_submenu(item)
    item.item.submenu = builtin_submenus[item.builtin_submenu]()

    -- With huge playlists, make the menu item open the playlist menu
    -- instead to not kill performance.
    if not item.item.submenu then
        item.item.cmd = submenu_commands[item.builtin_submenu]
        item.item.type = nil
        return
    end

    item.item.cmd = nil
    item.item.type = "submenu"

    local min = item.builtin_submenu == "$editions" and 2 or 1
    item.item.state = #item.item.submenu < min and {"disabled"} or {}
end

local function update_state(item)
    for state, compiled_condition in pairs(item.compiled_conditions) do
        toggle_state(item.item.state, state,
                     evaluate_condition(compiled_condition, item.item.title))
    end
end

local function parse_menu_item(line)
    local tokens = {}
    local separator = "\t+"
    for token in line:gmatch("(.-)" .. separator) do
        tokens[#tokens + 1] = token
    end
    tokens[#tokens + 1] = line:gsub(".*" .. separator, "")

    if tokens[1] == "" then
        return { type = "separator" }
    end

    local item = {
        item = {
            title = tokens[1],
            state = {},
        },
        compiled_conditions = {},
    }

    current_item = item

    if builtin_submenus[tokens[2]] then
        item.builtin_submenu = tokens[2]
        item.item.shortcut = get_shortcut(submenu_commands[item.builtin_submenu])
        item.update = update_builtin_submenu
        item:update()
        return item
    end

    local state_start = 3
    for _, state in pairs({"checked", "disabled", "hidden"}) do
        if not tokens[2] or tokens[2]:find("^%s*" .. state .. "=") then
            state_start = 2
            break
        end
    end

    if state_start == 2 then
        item.item.type = "submenu"
        item.item.submenu = {}
    else
        item.item.cmd = tokens[2]
        item.item.shortcut = get_shortcut(tokens[2])
    end

    for i = state_start, #tokens do
        local state, condition = tokens[i]:match("(%S-)=(.*)")

        if not state then
            return false
        end

        item.compiled_conditions[state] = compile_condition(condition, tokens[1])
        if evaluate_condition(item.compiled_conditions[state], tokens[1]) then
            table.insert(item.item.state, state)
        end
    end

    item.update = update_state

    return item
end

local function get_menu_conf()
    local menu_conf
    local file_handle = io.open(mp.command_native({"expand-path", options.menu_conf_path}))
    if file_handle then
        menu_conf = file_handle:read("*a")
        file_handle:close()
    else
        menu_conf = mp.get_property("default-menu")
    end

    local lines = {}
    for line in menu_conf:gmatch("(.-)\r?\n") do
        lines[#lines + 1] = line
    end

    return lines
end

local function parse_menu_conf(_, vo_configured)
    if not vo_configured then
        return
    end

    mp.unobserve_property(parse_menu_conf)

    property_set = to_map(mp.get_property_native("property-list"))
    active_bindings = get_active_bindings()

    local lines = get_menu_conf()
    local last_leading_whitespace = ""
    local menus_by_depth = { [""] = menu_data }

    for i, line in ipairs(lines) do
        local leading_whitespace = line:match("^%s*")
        local item = parse_menu_item(line:gsub("^%s*", ""))

        if not item then
            show_error("menu.conf is malformed: " .. line ..
                       " contains tabs not used as separators")
            return
        end

        if item.item then
            menu[#menu + 1] = item
            item = item.item
        end

        if #leading_whitespace > #last_leading_whitespace then
            local last_menu = menus_by_depth[last_leading_whitespace]

            if not last_menu[#last_menu].submenu then
                show_error("menu.conf is malformed: " .. line ..
                           " has leading whitespace but no parent menu was defined")
                return
            end

            menus_by_depth[leading_whitespace] = last_menu[#last_menu].submenu
        end

        if line == "" then
            -- Determine the depth of the separator from the next line.
            if lines[i + 1] then -- ignore newlines at the end
                table.insert(menus_by_depth[lines[i + 1]:match("%s*")], item)
            end
        else
            table.insert(menus_by_depth[leading_whitespace], item)
            last_leading_whitespace = leading_whitespace
        end
    end

    property_set = nil
    active_bindings = nil
    current_item = nil

    mp.register_idle(on_idle)
end

if options.populate_menu_data then
    mp.observe_property("vo-configured", "native", parse_menu_conf)
end

mp.add_key_binding(nil, "context-menu", function (info)
    if info.event == "repeat" then
        return
    end

    if not info.is_mouse then
        if info.event == "down" or info.event == "press" then
            mp.command(mp.get_property_native("load-context-menu")
                           and "script-message-to context_menu open"
                           or "context-menu")
        end
    elseif mp.get_property_native("load-context-menu") then
        mp.commandv("script-message-to", "context_menu",
                    info.event == "down" and "open" or "select")
    elseif info.event == (platform == "windows" and "up" or "down") then
        mp.command("context-menu")
    end
end, { complex = true })
