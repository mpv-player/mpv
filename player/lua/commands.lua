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
    persist_history = false,
    history_path = "~~state/command_history.txt",
    remember_input = true,
}

local input = require "mp.input"
local utils = require "mp.utils"

local styles = {
    -- Colors are stolen from base16 Eighties by Chris Kempson
    -- and converted to BGR as is required by ASS.
    -- 2d2d2d 393939 515151 697374
    -- 939fa0 c8d0d3 dfe6e8 ecf0f2
    -- 7a77f2 5791f9 66ccff 99cc99
    -- cccc66 cc9966 cc99cc 537bd2

    debug = "{\\1c&Ha09f93&}",
    v = "{\\1c&H99cc99&}",
    warn = "{\\1c&H66ccff&}",
    error = "{\\1c&H7a77f2&}",
    fatal = "{\\1c&H5791f9&}",
    completion = "{\\1c&Hcc99cc&}",
}
for key, style in pairs(styles) do
    styles[key] = style .. "{\\3c&H111111&}"
end

local terminal_styles = {
    debug = "\027[90m",
    v = "\027[32m",
    warn = "\027[33m",
    error = "\027[31m",
    fatal = "\027[91m",
}

local platform = mp.get_property("platform")
local path_separator = platform == "windows" and "\\" or "/"
local completion_pos
local completion_append
local last_text
local last_cursor_position

local cache = {}


local function get_commands()
    if cache["commands"] then
        return cache["commands"]
    end

    cache["commands"] = mp.get_property_native("command-list")
    table.sort(cache["commands"], function(c1, c2)
        return c1.name < c2.name
    end)

    return cache["commands"]
end

local function help_command(param)
    local output = ""
    if param == "" then
        output = "Available commands:\n"
        for _, cmd in ipairs(get_commands()) do
            output = output  .. "  " .. cmd.name
        end
        output = output .. "\n"
        output = output .. 'Use "help command" to show information about a command.\n'
        output = output .. "ESC or Ctrl+d exits the console.\n"
    else
        local cmd = nil
        for _, curcmd in ipairs(get_commands()) do
            if curcmd.name:find(param, 1, true) then
                cmd = curcmd
                if curcmd.name == param then
                    break -- exact match
                end
            end
        end

        if not cmd then
            input.log('No command matches "' .. param .. '"!', styles.error,
                      terminal_styles.error)
            return
        end

        output = output .. 'Command "' .. cmd.name .. '"\n'

        for _, arg in ipairs(cmd.args) do
            output = output .. "    " .. arg.name .. " (" .. arg.type .. ")"
            if arg.optional then
                output = output .. " (optional)"
            end
            output = output .. "\n"
        end

        if cmd.vararg then
            output = output .. "This command supports variable arguments.\n"
        end
    end

    input.log(output:sub(1, -2))
end

local function submit(line)
    -- match "help [<text>]", return <text> or "", strip all whitespace
    local help = line:match("^%s*help%s+(.-)%s*$") or
                 (line:match("^%s*help$") and "")

    if help then
        help_command(help)
    elseif line ~= "" then
        mp.command(line)
    end
end

local function opened()
    mp.enable_messages("terminal-default")
end

local function closed(text, cursor_position)
    mp.enable_messages("silent:terminal-default")

    if options.remember_input then
        last_text = text
        last_cursor_position = cursor_position
    end
end

local function command_list()
    local cmds = {}
    for i, command in ipairs(get_commands()) do
        cmds[i] = command.name
    end

    return cmds
end

local function property_list()
    if cache["property-list"] then
        return cache["property-list"]
    end

    cache["property-list"] = mp.get_property_native("property-list")

    for _, sub_property in pairs({"video", "audio", "sub", "sub2"}) do
        table.insert(cache["property-list"], "current-tracks/" .. sub_property)
    end

    for _, sub_property in pairs({"text", "text-primary"}) do
        table.insert(cache["property-list"], "clipboard/" .. sub_property)
    end

    return cache["property-list"]
end

local function profile_list()
    local profiles = {}

    for i, profile in ipairs(mp.get_property_native("profile-list")) do
        profiles[i] = profile.name
    end

    return profiles
end

local function option_info(option, prop, default)
    local key = "option-info/" .. option
    cache[key] = cache[key] or mp.get_property_native(key, false)

    if not cache[key] then
        return default
    end

    if prop then
        if cache[key][prop] ~= nil then
            return cache[key][prop]
        end
        return default
    end

    return cache[key]
end

local function list_option_list()
    if cache["option-list"] then
        return cache["option-list"]
    end
    cache["option-list"] = {}

    -- Don't log errors for renamed and removed properties.
    -- (Just mp.enable_messages("fatal") still logs them to the terminal.)
    local msg_level_backup = mp.get_property("msg-level")
    mp.set_property("msg-level", msg_level_backup == "" and "cplayer=no"
                                 or msg_level_backup .. ",cplayer=no")

    for _, option in pairs(mp.get_property_native("options")) do
        if option_info(option, "type", ""):find(" list$") then
            table.insert(cache["option-list"], option)
        end
    end

    mp.set_property("msg-level", msg_level_backup)

    return cache["option-list"]
end

local function list_option_action_list(option)
    local type = option_info(option, "type")

    if type == "Key/value list" then
        return {"add", "append", "set", "remove"}
    end

    if type == "String list" or type == "Object settings list" then
        return {"add", "append", "clr", "pre", "set", "remove", "toggle"}
    end
end

local function list_option_value_list(option)
    local values = mp.get_property_native(option)

    if type(values) ~= "table" then
        return
    end

    if type(values[1]) ~= "table" then
        return values
    end

    for i, value in ipairs(values) do
        values[i] = value.label and "@" .. value.label or value.name
    end

    return values
end

local function has_file_argument(candidate_command)
    local key = "command-file/" .. candidate_command
    if cache[key] ~= nil then
        return cache[key]
    end

    for _, command in pairs(get_commands()) do
        if command.name == candidate_command then
            cache[key] = command.args[1] and
                         (command.args[1].name == "filename" or command.args[1].name == "url")
            return cache[key]
        end
    end
end

local function file_list(directory)
    if directory == "" then
        directory = "."
    else
        directory = mp.command_native({"expand-path", directory})
    end

    local files = utils.readdir(directory, "files") or {}

    for _, dir in pairs(utils.readdir(directory, "dirs") or {}) do
        files[#files + 1] = dir .. path_separator
    end

    return files
end

local function handle_file_completion(before_cur)
    local directory, last_component_pos =
        before_cur:sub(completion_pos):match("(.-)()[^" .. path_separator .."]*$")

    completion_pos = completion_pos + last_component_pos - 1

    -- Don"t use completion_append for file completion to not add quotes after
    -- directories whose entries you may want to complete afterwards.
    completion_append = ""

    return file_list(directory)
end

local function handle_choice_completion(option, before_cur)
    local info = option_info(option, nil, {})

    if info.type == "Flag" then
        return { "no", "yes" }, before_cur
    end

    if info["expects-file"] then
        return handle_file_completion(before_cur)
    end

    -- Fix completing the empty value for --dscale and --cscale.
    if info.choices and info.choices[1] == "" and completion_append == "" then
        info.choices[1] = '""'
    end

    return info.choices
end

local function command_flags_at_1st_argument_list(command)
    local flags = {
        ["playlist-next"] = {"weak", "force"},
        ["playlist-play-index"] = {"current", "none"},
        ["playlist-remove"] = {"current"},
        ["rescan-external-files"] = {"reselect", "keep-selection"},
        ["revert-seek"] = {"mark", "mark-permanent"},
        ["screenshot"] = {"subtitles", "video", "window", "each-frame"},
        ["stop"] = {"keep-playlist"},
    }
    flags["playlist-prev"] = flags["playlist-next"]
    flags["screenshot-raw"] = flags.screenshot

    return flags[command]
end

local function command_flags_at_2nd_argument_list(command)
    local flags = {
        ["apply-profile"] = {"apply", "restore"},
        ["frame-step"] = {"play", "seek", "mute"},
        ["loadfile"] = {"replace", "append", "append-play", "insert-next",
                        "insert-next-play", "insert-at", "insert-at-play"},
        ["screenshot-to-file"] = {"subtitles", "video", "window", "each-frame"},
        ["screenshot-raw"] = {"bgr0", "bgra", "rgba", "rgba64"},
        ["seek"] = {"relative", "absolute", "absolute-percent",
                    "relative-percent", "keyframes", "exact"},
        ["sub-add"] = {"select", "auto", "cached"},
        ["sub-seek"] = {"primary", "secondary"},
    }
    flags.loadlist = flags.loadfile
    flags["audio-add"] = flags["sub-add"]
    flags["video-add"] = flags["sub-add"]
    flags["sub-step"] = flags["sub-seek"]

    return flags[command]
end

local function handle_flags(command, arg_index, flags)
    for _, cmd in pairs(get_commands()) do
        if cmd.name == command then
            if cmd.args[arg_index] and cmd.args[arg_index].type == "Flags" then
                break
            else
                return
            end
        end
    end

    local plus_pos = flags:find("%+[^%+]*$")

    if plus_pos then
        completion_pos = completion_pos + plus_pos
    end
end

local function executable_list()
    local executable_map = {}
    local path = os.getenv("PATH") or ""
    local separator = platform == "windows" and ";" or ":"
    local exts = {}

    for ext in (os.getenv("PATHEXT") or ""):gmatch("[^;]+") do
        exts[ext:lower()] = true
    end

    for directory in path:gmatch("[^" .. separator .. "]+") do
        for _, executable in pairs(utils.readdir(directory, "files") or {}) do
            if not next(exts) or exts[(executable:match("%.%w+$") or ""):lower()] then
                executable_map[executable] = true
            end
        end
    end

    local executables = {}
    for executable, _ in pairs(executable_map) do
        executables[#executables + 1] = executable
    end

    return executables
end

local function filter_label_list(type)
    local values = {"all"}

    for _, value in pairs(mp.get_property_native(type)) do
        if value.label then
            values[#values + 1] = value.label
        end
    end

    return values
end

local function complete(before_cur)
    local tokens = {}
    local first_useful_token_index = 1
    local completions

    local begin_new_token = true
    local last_quote
    for pos, char in before_cur:gmatch("()(.)") do
        if char:find("[%s;]") and not last_quote then
            begin_new_token = true
            if char == ";" then
                first_useful_token_index = #tokens + 1
            end
        elseif begin_new_token then
            tokens[#tokens + 1] = { text = char, pos = pos }
            last_quote = char:match("['\"]")
            begin_new_token = false
        else
            tokens[#tokens].text = tokens[#tokens].text .. char
            if char == last_quote then
                last_quote = nil
            end
        end
    end

    completion_append = last_quote or ""

    -- Strip quotes from tokens.
    for _, token in pairs(tokens) do
        if token.text:find('^"') then
            token.text = token.text:sub(2):gsub('"$', "")
            token.pos = token.pos + 1
        elseif token.text:find("^'") then
            token.text = token.text:sub(2):gsub('"$', "")
            token.pos = token.pos + 1
        end
    end

    -- Skip command prefixes because it is not worth lumping them together with
    -- command completions when they are useless for interactive usage.
    local command_prefixes = {
        ["osd-auto"] = true, ["no-osd"] = true, ["osd-bar"] = true,
        ["osd-msg"] = true, ["osd-msg-bar"] = true, ["raw"] = true,
        ["expand-properties"] = true, ["repeatable"] = true,
        ["nonrepeatable"] = true, ["nonscalable"] = true,
        ["async"] = true, ["sync"] = true
    }

    -- Add an empty token if the cursor is after whitespace or ; to simplify
    -- comparisons.
    if before_cur == "" or before_cur:find("[%s;]$") then
        tokens[#tokens + 1] = { text = "", pos = #before_cur + 1 }
    end

    while tokens[first_useful_token_index] and
          command_prefixes[tokens[first_useful_token_index].text] do
        if first_useful_token_index == #tokens then
            return
        end

        first_useful_token_index = first_useful_token_index + 1
    end

    completion_pos = tokens[#tokens].pos

    local add_actions = {
        ["add"] = true, ["append"] = true, ["pre"] = true, ["set"] = true
    }

    local first_useful_token = tokens[first_useful_token_index]

    local property_pos = before_cur:match("${[=>]?()[%w_/-]*$")
    if property_pos then
        completion_pos = property_pos
        completions = property_list()
        completion_append = "}"
    elseif #tokens == first_useful_token_index then
        completions = command_list()
        completions[#completions + 1] = "help"
    elseif #tokens == first_useful_token_index + 1 then
        if first_useful_token.text == "set" or
           first_useful_token.text == "add" or
           first_useful_token.text == "cycle" or
           first_useful_token.text == "cycle-values" or
           first_useful_token.text == "multiply" then
            completions = property_list()
        elseif first_useful_token.text == "help" then
            completions = command_list()
        elseif first_useful_token.text == "apply-profile" then
            completions = profile_list()
        elseif first_useful_token.text == "change-list" then
            completions = list_option_list()
        elseif first_useful_token.text == "run" then
            completions = executable_list()
        elseif first_useful_token.text == "vf" or
               first_useful_token.text == "af" then
            completions = list_option_action_list(first_useful_token.text)
        elseif first_useful_token.text == "vf-command" or
               first_useful_token.text == "af-command" then
            completions = filter_label_list(first_useful_token.text:sub(1,2))
        elseif has_file_argument(first_useful_token.text) then
            completions = handle_file_completion(before_cur)
        else
            completions = command_flags_at_1st_argument_list(first_useful_token.text)
            handle_flags(first_useful_token.text, 1, tokens[#tokens].text)
        end
    elseif first_useful_token.text == "cycle-values" then
        completions = handle_choice_completion(tokens[first_useful_token_index + 1].text,
                                               before_cur)
    elseif first_useful_token.text == "run" then
        completions = handle_file_completion(before_cur)
    elseif #tokens == first_useful_token_index + 2 then
        if first_useful_token.text == "set" then
            completions = handle_choice_completion(tokens[first_useful_token_index + 1].text,
                                                   before_cur)
        elseif first_useful_token.text == "change-list" then
            completions = list_option_action_list(tokens[first_useful_token_index + 1].text)
        elseif first_useful_token.text == "vf" or
               first_useful_token.text == "af" then
            if add_actions[tokens[first_useful_token_index + 1].text] then
                completions = handle_choice_completion(first_useful_token.text, before_cur)
            elseif tokens[first_useful_token_index + 1].text == "remove" then
                completions = list_option_value_list(first_useful_token.text)
            end
        else
            completions = command_flags_at_2nd_argument_list(first_useful_token.text)
            handle_flags(first_useful_token.text, 2, tokens[#tokens].text)
        end
    elseif #tokens == first_useful_token_index + 3 then
        if first_useful_token.text == "change-list" then
            if add_actions[tokens[first_useful_token_index + 2].text] then
                completions = handle_choice_completion(tokens[first_useful_token_index + 1].text,
                                                       before_cur)
            elseif tokens[first_useful_token_index + 2].text == "remove" then
                completions = list_option_value_list(tokens[first_useful_token_index + 1].text)
            end
        elseif first_useful_token.text == "dump-cache" then
            completions = handle_file_completion(before_cur)
        end
    end

    return completions or {}, completion_pos, completion_append
end

local function open(text, cursor_position, keep_open)
    input.get({
        prompt = ">",
        submit = submit,
        keep_open = keep_open,
        opened = opened,
        closed = closed,
        complete = complete,
        autoselect_completion = true,
        default_text = text,
        cursor_position = cursor_position,
        history_path = options.persist_history
                       and mp.command_native({"expand-path", options.history_path}) or nil,
    })
end

mp.add_key_binding(nil, "open", function ()
    open(last_text, last_cursor_position, true)
end)

-- Open the console with passed text and cursor_position arguments
mp.register_script_message("type", open)

mp.register_event("log-message", function(e)
    -- Ignore log messages from the OSD because of paranoia, since writing them
    -- to the OSD could generate more messages in an infinite loop.
    if e.prefix:sub(1, 3) == "osd" then return end

    -- Ignore messages output by this script.
    if e.prefix == mp.get_script_name() then return end

    -- Ignore buffer overflow warning messages. Overflowed log messages would
    -- have been offscreen anyway.
    if e.prefix == "overflow" then return end

    -- Filter out trace-level log messages, even if the terminal-default log
    -- level includes them. These aren"t too useful for an on-screen display
    -- without scrollback and they include messages that are generated from the
    -- OSD display itself.
    if e.level == "trace" then return end

    -- Avoid logging debug messages infinitely.
    if e.prefix == "cplayer" and
       e.text:find('^Run command: script%-message%-to, flags=64, args=%[target="console"') then
        return
    end

    -- Use color for debug/v/warn/error/fatal messages.
    input.log("[" .. e.prefix .. "] " .. e.text:sub(1, -2), styles[e.level],
              terminal_styles[e.level])
end)

-- Enable log messages. In silent mode, mpv will queue log messages in a buffer
-- until enable_messages is called again without the silent: prefix.
mp.enable_messages("silent:terminal-default")

require "mp.options".read_options(options)
