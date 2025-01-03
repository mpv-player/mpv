-- This script automatically loads playlist entries before and after the
-- currently played file. It does so by scanning the directory a file is
-- located in when starting playback. It sorts the directory entries
-- alphabetically, and adds entries before and after the current file to
-- the internal playlist. (It stops if it would add an already existing
-- playlist entry at the same position - this makes it "stable".)
-- Add at most 5000 * 2 files when starting a file (before + after).

--[[
To configure this script use file autoload.conf in directory script-opts (the "script-opts"
directory must be in the mpv configuration directory, typically ~/.config/mpv/).

Option `ignore_patterns` is a comma-separated list of patterns (see lua.org/pil/20.2.html).
Additionally to the standard lua patterns, you can also escape commas with `%`,
for example, the option `bak%,x%,,another` will be resolved as patterns `bak,x,` and `another`.
But it does not mean you need to escape all lua patterns twice,
so the option `bak%%,%.mp4,` will be resolved as two patterns `bak%%` and `%.mp4`.

Example configuration would be:

disabled=no
images=no
videos=yes
audio=yes
additional_image_exts=list,of,ext
additional_video_exts=list,of,ext
additional_audio_exts=list,of,ext
ignore_hidden=yes
same_type=yes
directory_mode=recursive
ignore_patterns=^~,^bak-,%.bak$

--]]

local MAX_ENTRIES = 5000
local MAX_DIR_STACK = 20

local msg = require 'mp.msg'
local options = require 'mp.options'
local utils = require 'mp.utils'

local o = {
    disabled = false,
    images = true,
    videos = true,
    audio = true,
    additional_image_exts = "",
    additional_video_exts = "",
    additional_audio_exts = "",
    ignore_hidden = true,
    same_type = false,
    directory_mode = "auto",
    ignore_patterns = ""
}

local function Set(t)
    local set = {}
    for _, v in pairs(t) do set[v] = true end
    return set
end

local EXTENSIONS_VIDEO_DEFAULT = Set {
    '3g2', '3gp', 'avi', 'flv', 'm2ts', 'm4v', 'mj2', 'mkv', 'mov',
    'mp4', 'mpeg', 'mpg', 'ogv', 'rmvb', 'webm', 'wmv', 'y4m'
}

local EXTENSIONS_AUDIO_DEFAULT = Set {
    'aiff', 'ape', 'au', 'flac', 'm4a', 'mka', 'mp3', 'oga', 'ogg',
    'ogm', 'opus', 'wav', 'wma'
}

local EXTENSIONS_IMAGES_DEFAULT = Set {
    'avif', 'bmp', 'gif', 'j2k', 'jp2', 'jpeg', 'jpg', 'jxl', 'png',
    'svg', 'tga', 'tif', 'tiff', 'webp'
}

local EXTENSIONS, EXTENSIONS_VIDEO, EXTENSIONS_AUDIO, EXTENSIONS_IMAGES

local function SetUnion(a, b)
    for k in pairs(b) do a[k] = true end
    return a
end

-- Returns first and last positions in string or past-to-end indices
local function FindOrPastTheEnd(string, pattern, start_at)
    local pos1, pos2 = string:find(pattern, start_at)
    return pos1 or #string + 1,
           pos2 or #string + 1
end

local function Split(list)
    local set = {}

    local item_pos = 1
    local item = ""

    while item_pos <= #list do
        local pos1, pos2 = FindOrPastTheEnd(list, "%%*,", item_pos)

        local pattern_length = pos2 - pos1
        local is_comma_escaped = pattern_length % 2

        local pos_before_escape = pos1 - 1
        local item_escape_count = pattern_length - is_comma_escaped

        item = item .. string.sub(list, item_pos, pos_before_escape + item_escape_count)

        if is_comma_escaped == 1 then
            item = item .. ","
        else
            set[item] = true
            item = ""
        end

        item_pos = pos2 + 1
    end

    set[item] = true

    -- exclude empty items
    set[""] = nil

    return set
end

local function split_option_exts(video, audio, image)
    if video then o.additional_video_exts = Split(o.additional_video_exts) end
    if audio then o.additional_audio_exts = Split(o.additional_audio_exts) end
    if image then o.additional_image_exts = Split(o.additional_image_exts) end
end

local function split_patterns()
    o.ignore_patterns = Split(o.ignore_patterns)
end

local function create_extensions()
    EXTENSIONS = {}
    EXTENSIONS_VIDEO = {}
    EXTENSIONS_AUDIO = {}
    EXTENSIONS_IMAGES = {}
    if o.videos then
        SetUnion(SetUnion(EXTENSIONS_VIDEO, EXTENSIONS_VIDEO_DEFAULT), o.additional_video_exts)
        SetUnion(EXTENSIONS, EXTENSIONS_VIDEO)
    end
    if o.audio then
        SetUnion(SetUnion(EXTENSIONS_AUDIO, EXTENSIONS_AUDIO_DEFAULT), o.additional_audio_exts)
        SetUnion(EXTENSIONS, EXTENSIONS_AUDIO)
    end
    if o.images then
        SetUnion(SetUnion(EXTENSIONS_IMAGES, EXTENSIONS_IMAGES_DEFAULT), o.additional_image_exts)
        SetUnion(EXTENSIONS, EXTENSIONS_IMAGES)
    end
end

local function validate_directory_mode()
    if o.directory_mode ~= "recursive" and o.directory_mode ~= "lazy"
       and o.directory_mode ~= "ignore" then
        o.directory_mode = nil
    end
end

options.read_options(o, nil, function(list)
    split_option_exts(list.additional_video_exts, list.additional_audio_exts,
                      list.additional_image_exts)
    if list.videos or list.additional_video_exts or
        list.audio or list.additional_audio_exts or
        list.images or list.additional_image_exts then
        create_extensions()
    end
    if list.directory_mode then
        validate_directory_mode()
    end
    if list.ignore_patterns then
        split_patterns()
    end
end)

split_option_exts(true, true, true)
split_patterns()
create_extensions()
validate_directory_mode()

local function add_files(files)
    local oldcount = mp.get_property_number("playlist-count", 1)
    for i = 1, #files do
        mp.commandv("loadfile", files[i][1], "append")
        mp.commandv("playlist-move", oldcount + i - 1, files[i][2])
    end
end

local function get_extension(path)
    return path:match("%.([^%.]+)$") or "nomatch"
end

local function is_ignored(file)
    for pattern in pairs(o.ignore_patterns) do
        if file:match(pattern) then
            return true
        end
    end
    return false
end

-- alphanum sorting for humans in Lua
-- http://notebook.kulchenko.com/algorithms/alphanumeric-natural-sorting-for-humans-in-lua

local function alphanumsort(filenames)
    local function padnum(n, d)
        return #d > 0 and ("%03d%s%.12f"):format(#n, n, tonumber(d) / (10 ^ #d))
            or ("%03d%s"):format(#n, n)
    end

    local tuples = {}
    for i, f in ipairs(filenames) do
        tuples[i] = {f:lower():gsub("0*(%d+)%.?(%d*)", padnum), f}
    end
    table.sort(tuples, function(a, b)
        return a[1] == b[1] and #b[2] < #a[2] or a[1] < b[1]
    end)
    for i, tuple in ipairs(tuples) do filenames[i] = tuple[2] end
    return filenames
end

local autoloaded
local added_entries = {}
local autoloaded_dir

local function scan_dir(path, current_file, dir_mode, separator, dir_depth, total_files, extensions)
    if dir_depth == MAX_DIR_STACK then
        return
    end
    msg.trace("scanning: " .. path)
    local files = utils.readdir(path, "files") or {}
    local dirs = dir_mode ~= "ignore" and utils.readdir(path, "dirs") or {}
    local prefix = path == "." and "" or path

    local function filter(t, iter)
        for i = #t, 1, -1 do
            if not iter(t[i]) then
                table.remove(t, i)
            end
        end
    end

    filter(files, function(v)
        -- Always accept current file
        local current = prefix .. v == current_file
        if current then
            return true
        end
        if o.ignore_hidden and v:match("^%.") then
            return false
        end
        if is_ignored(v) then
            return false
        end

        local ext = get_extension(v)
        return ext and extensions[ext:lower()]
    end)
    filter(dirs, function(d)
        return not (o.ignore_hidden and d:match("^%."))
    end)
    alphanumsort(files)
    alphanumsort(dirs)

    for i, file in ipairs(files) do
        files[i] = prefix .. file
    end

    local function append(t1, t2)
        local t1_size = #t1
        for i = 1, #t2 do
            t1[t1_size + i] = t2[i]
        end
    end

    append(total_files, files)
    if dir_mode == "recursive" then
        for _, dir in ipairs(dirs) do
            scan_dir(prefix .. dir .. separator, current_file, dir_mode,
                     separator, dir_depth + 1, total_files, extensions)
        end
    else
        for i, dir in ipairs(dirs) do
            dirs[i] = prefix .. dir
        end
        append(total_files, dirs)
    end
end

local function find_and_add_entries()
    local aborted = mp.get_property_native("playback-abort")
    if aborted then
        msg.debug("stopping: playback aborted")
        return
    end

    local path = mp.get_property("path", "")
    local dir, filename = utils.split_path(path)
    msg.trace(("dir: %s, filename: %s"):format(dir, filename))
    if o.disabled then
        msg.debug("stopping: autoload disabled")
        return
    elseif #dir == 0 then
        msg.debug("stopping: not a local path")
        return
    end

    local pl_count = mp.get_property_number("playlist-count", 1)
    local this_ext = get_extension(filename)
    -- check if this is a manually made playlist
    if pl_count > 1 and autoloaded == nil then
        msg.debug("stopping: manually made playlist")
        return
    elseif pl_count == 1 then
        autoloaded = true
        autoloaded_dir = dir
        added_entries = {}
    end

    local extensions
    if o.same_type then
        if EXTENSIONS_VIDEO[this_ext:lower()] then
            extensions = EXTENSIONS_VIDEO
        elseif EXTENSIONS_AUDIO[this_ext:lower()] then
            extensions = EXTENSIONS_AUDIO
        elseif EXTENSIONS_IMAGES[this_ext:lower()] then
            extensions = EXTENSIONS_IMAGES
        end
    else
        extensions = EXTENSIONS
    end
    if not extensions then
        msg.debug("stopping: no matched extensions list")
        return
    end

    local pl = mp.get_property_native("playlist", {})
    local pl_current = mp.get_property_number("playlist-pos-1", 1)
    msg.trace(("playlist-pos-1: %s, playlist: %s"):format(pl_current,
        utils.to_string(pl)))

    local files = {}
    scan_dir(autoloaded_dir, path,
             o.directory_mode or mp.get_property("directory-mode", "lazy"),
             mp.get_property_native("platform") == "windows" and "\\" or "/",
             0, files, extensions)

    if next(files) == nil then
        msg.debug("no other files or directories in directory")
        return
    end

    -- Find the current pl entry (dir+"/"+filename) in the sorted dir list
    local current
    for i = 1, #files do
        if files[i] == path then
            current = i
            break
        end
    end
    if not current then
        msg.debug("current file not found in directory")
        return
    end
    msg.trace("current file position in files: "..current)

    -- treat already existing playlist entries, independent of how they got added
    -- as if they got added by autoload
    for _, entry in ipairs(pl) do
        added_entries[entry.filename] = true
    end

    local append = {[-1] = {}, [1] = {}}
    for direction = -1, 1, 2 do -- 2 iterations, with direction = -1 and +1
        for i = 1, MAX_ENTRIES do
            local pos = current + i * direction
            local file = files[pos]
            if file == nil or file[1] == "." then
                break
            end

            -- skip files that are/were already in the playlist
            if not added_entries[file] then
                if direction == -1 then
                    msg.verbose("Prepending " .. file)
                    table.insert(append[-1], 1, {file, pl_current + i * direction + 1})
                else
                    msg.verbose("Adding " .. file)
                    if pl_count > 1 then
                        table.insert(append[1], {file, pl_current + i * direction - 1})
                    else
                        mp.commandv("loadfile", file, "append")
                    end
                end
                added_entries[file] = true
            end
        end
        if pl_count == 1 and direction == -1 and #append[-1] > 0 then
            local load = append[-1]
            for i = 1, #load do
                mp.commandv("loadfile", load[i][1], "append")
            end
            mp.commandv("playlist-move", 0, current)
        end
    end

    if pl_count > 1 then
        add_files(append[1])
        add_files(append[-1])
    end
end

mp.register_event("start-file", find_and_add_entries)
