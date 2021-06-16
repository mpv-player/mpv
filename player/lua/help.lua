local mp = require 'mp'
local utils = require 'mp.utils'
local options = require 'mp.options'

local o = {
    show_unkown = false,       -- Show unknown commands in pretty view.
    show_prefixes = true,      -- Show command prefixes for raw commands.

    timeout = 8,               -- OSD timeout.
    margin = 0.05,             -- OSD margin as a ratio of screen size.
    horizontal_offset = 0.5,   -- Horizontal offset of OSD as a ratio of screen size.

    max_lines = 30,            -- Maximum number of lines shown on OSD. Font size is adjusted accordingly but will not go below min_font_size.
    min_font_size = 16,        -- Minimum OSD font size, subject to display-hidpi-scale property. Reduces number of lines shown if necessary.
    group_font_ratio = 1.5,    -- OSD group font size is adjusted by this ratio.
    font_border_ratio = 0.075, -- OSD font border size as a ratio of the font size.
    font_shadow_ratio = 0,     -- OSD font shadow size as a ratio of the font size.

    -- Additional ass styles to apply.
    group_ass_style = '{\\b1}',
    key_ass_style = '',
    prefix_ass_style = '{\\1c&H777777&}',
    command_ass_style = '{\\1c&FFFFFF&}',
    argument_ass_style = '{\\1c&Hababab&}',
    page_ass_style = '',

    debug = false
}
options.read_options(o)

-- Verbs to use to describe property commands.
-- First column for positive action (add >=0, cycle, cycle-values, multiply >=1, set yes),
-- second column for the reverse (add <0, cycle down, cycle-values "!reverse", multiply <1, set no).
-- If the third column is provided, cycle and cycle-values will always use
-- that verb instead of deciding by cycle direction.
local verbs = {
    du = { 'down', 'up', 'up/down' },
    edc = { 'enable', 'disable', 'cycle' },
    edt = { 'enable', 'disable', 'toggle' },
    id = { 'increase', 'decrease' },
    lr = { 'left', 'right', 'left/right '},
    np = { 'next', 'previous', },
    rl = { 'right', 'left', 'right/left '},
    sp = { 'start', 'pause', 'toggle' },
    ud = { 'up', 'down', 'down/up' },
}

local commands = {
    ['ab-loop'] = { text = 'Cycle A-B loop states', group = 'Playback' },
    ['frame-back-step'] = { text = 'Go back by one frame and pause', group = 'Playback' },
    ['frame-step'] = { text = 'Play one frame and pause', group = 'Playback' },
    ['playlist-clear'] = { text = 'Clear playlist', group = 'Playlist' },
    ['playlist-next'] = { text = 'Go to next playlist entry', group = 'Playlist' },
    ['playlist-prev'] = { text = 'Go to previous playlist entry', group = 'Playlist' },
    ['playlist-shuffle'] = { text = 'Shuffle playlist', group = 'Playlist' },
    ['playlist-unshuffle'] = { text = 'Undo last shuffle playlist', group = 'Playlist' },
    ['quit-watch-later'] = { text = 'Exit and remember playback position', group = 'Player' },
    ['quit'] = { text = 'Exit', group = 'Player' },
    ['revert-seek'] = { text = 'Undo last seek', group = 'Seek' },
    ['screenshot-to-file'] = { text = 'Take a screenshot', group = 'Player' },
    ['screenshot'] = { text = 'Take a screenshot', group = 'Player' },
    ['show-progress'] = { text = 'Show playback progress', group = 'Playback' },
    ['stop'] = { text = 'Stop playback and clear playlist', group = 'Playback' },
    ['sub-seek'] = { text = 'Seek to %s subtitle', verb = verbs.np, group = 'Seek' },
    ['sub-step'] = { text = 'Change timing to %s subtitle', verb = verbs.np, group = 'Subtitles' },
    ['write-watch-later-config'] = { text = 'Remember playback position', group = 'Player' },
}

local properties = {
    ['ao-mute'] = { text = '%s mute', verb = verbs.edt, group = 'Audio' },
    ['ao-volume'] = { text = '%s volume', verb = verbs.id, group = 'Audio' },
    ['audio-delay'] = { text = '%s audio delay', verb = verbs.id, group = 'Audio' },
    ['audio'] = { text = '%s audio track', verb = verbs.np, group = 'Audio' },
    ['brightness'] = { text = '%s brightness', verb = verbs.id, group = 'Video' },
    ['chapter'] = { text = '%s chapter', verb = verbs.np, group = 'Playlist' },
    ['contrast'] = { text = '%s contrast', verb = verbs.id, group = 'Video' },
    ['deinterlace'] = { text = '%s deinterlacer', verb = verbs.edt, group = 'Video' },
    ['edition'] = { text = '%s edition', verb = verbs.np, group = 'Playlist' },
    ['fullscreen'] = { text = '%s fullscreen', verb = verbs.edt, group = 'Player' },
    ['gamma'] = { text = '%s gamma', verb = verbs.id, group = 'Video' },
    ['hwdec'] = { text = '%s hardware video decoding', verb = verbs.edc, group = 'Video' },
    ['interpolation'] = { text = '%s video interpolation', verb = verbs.edt, group = 'Video' },
    ['loop-file'] = { text = '%s infinite looping', verb = verbs.edt, group = 'Playback' },
    ['mute'] = { text = '%s mute', verb = verbs.edt, group = 'Audio' },
    ['ontop'] = { text = '%s stay on top', verb = verbs.edt, group = 'Player' },
    ['osd-level'] = { text = 'Cycle OSD state', group = 'Player' },
    ['panscan'] = { text = '%s pan-and-scan range', verb = verbs.id, group = 'Video' },
    ['pause'] = { text = '%s playback', verb = verbs.sp, group = 'Playback' },
    ['saturation'] = { text = '%s saturation', verb = verbs.id, group = 'Video' },
    ['secondary-sid'] = { text = '%s secondary subtitles track', verb = verbs.np, group = 'Subtitles' },
    ['secondary-sub-visibility'] = { text = '%s secondary subtitle visibility', verb = verbs.edt, group = 'Subtitles' },
    ['speed'] = { text = '%s playback speed', verb = verbs.id, group = 'Playback' },
    ['sub-ass-override'] = { text = '%s ASS subtitle style overrides', verb = verbs.edc, group = 'Subtitles' },
    ['sub-ass-vsfilter-aspect-compat'] = { text = '%s subtitle VSFilter aspect compatibility mode', verb = verbs.edt, group = 'Subtitles' } ,
    ['sub-delay'] = { text = '%s subtitle delay', verb = verbs.id, group = 'Subtitles' },
    ['sub-pos'] = { text = 'Move subtitles %s', verb = verbs.du, group = 'Subtitles' },
    ['sub-scale'] = { text = '%s subtitle size', verb = verbs.id, group = 'Subtitles' },
    ['sub-visibility'] = { text = '%s subtitle visibility', verb = verbs.edt, group = 'Subtitles' },
    ['sub'] = { text = '%s subtitle track', verb = verbs.np, group = 'Subtitles' },
    ['video-aspect-override'] = { text = 'Override video aspect ratio', group = 'Video' },
    ['video-pan-x'] = { text = 'Move video %s', verb = verbs.rl, group = 'Video' },
    ['video-pan-y'] = { text = 'Move video %s', verb = verbs.du, group = 'Video' },
    ['video-zoom'] = { text = '%s video zoom', verb = verbs.id, group = 'Video' },
    ['video'] = { text = '%s video track', verb = verbs.np, group = 'Video' },
    ['volume'] = { text = '%s volume', verb = verbs.id, group = 'Audio' },
    ['window-scale'] = { text = '%s window size', verb = verbs.id, group = 'Video' },
}

local script_bindings = {
    [mp.get_script_name() .. '/show-help'] = { text = 'Show input bindings', group = 'Other' },
    [mp.get_script_name() .. '/show-help-raw'] = { text = 'Show raw input bindings', group = 'Other' },
    ['acompressor/acompressor-decrease-attack'] = { text = 'Decrease attack', sort_key = 'attack', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-decrease-knee'] = { text = 'Decrease knee', sort_key = 'knee', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-decrease-makeup'] = { text = 'Decrease makeup', sort_key = 'makeup', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-decrease-ratio'] = { text = 'Decrease ratio', sort_key = 'ratio', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-decrease-release'] = { text = 'Decrease release', sort_key = 'release', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-decrease-threshold'] = { text = 'Decrease threshold', sort_key = 'threshold', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-increase-attack'] = { text = 'Increase attack', sort_key = 'attack', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-increase-knee'] = { text = 'Increase knee', sort_key = 'knee', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-increase-makeup'] = { text = 'Increase makeup', sort_key = 'makeup', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-increase-ratio'] = { text = 'Increase ratio', sort_key = 'ratio', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-increase-release'] = { text = 'Increase release', sort_key = 'release', group = 'Dynamic Audio Compressor' },
    ['acompressor/acompressor-increase-threshold'] = { text = 'Increase threshold', sort_key = 'threshold', group = 'Dynamic Audio Compressor' },
    ['acompressor/toggle-acompressor'] = { text = 'Toggle dynamic range compression', group = 'Dynamic Audio Compressor' },
    ['autocrop/toggle_crop'] = { text = 'Toggle auto crop', group = 'Video' },
    ['autodeint/autodeint'] = { text = 'Start automatic deinterlace', group = 'Video' },
    ['console/enable'] = { text = 'Enable console', group = 'Other' },
    ['cycle-deinterlace-pullup/cycle-deinterlace-pullup'] = { text = 'Cycle deinterlacer/pullup', group = 'Video' },
    ['osc/visibility'] = { text = 'Cycle OSC visibilty', group = 'Other' },
    ['stats/display-stats-toggle'] = { text = 'Toggle stats', group = 'Other' },
    ['stats/display-stats'] = { text = 'Show stats', group = 'Other' },
}
local script_bindings_seen = {}

local group_weights = {
    Player = 10,
    Playback = 20,
    Seek = 30,
    Playlist = 40,
    Audio = 60,
    Subtitles = 70,
    Video = 70,
    ['Dynamic Audio Compressor'] = 80,
    Other = 100
}

local prefixes = {
    ['no-osd'] = 'osd',
    ['osd-auto'] = 'osd',
    ['osd-bar'] = 'osd',
    ['osd-msg'] = 'osd',
    ['osd-msg-bar'] = 'osd',
    ['raw'] = 'raw',
    ['expand-properties'] = 'raw',
    ['async'] = 'sync',
    ['sync'] = 'sync',
    ['repeatable'] = 'repeatable'
}

local property_commands = {
    set = true,
    add = true,
    cycle = true,
    multiply = true,
    ['cycle-values'] = true
}

local osd_page = 0
local osd_pages = {}
local osd_pretty = true

local osd = mp.create_osd_overlay('ass-events')
local osd_timer = mp.add_timeout(o.timeout, function()
    osd_page = 0
    osd:remove()
end)
osd_timer:kill()

local osd_bindings = {}
local console_bindings = {}

local osc_margin_top = 0
local osc_margin_bottom = 0

local function ass_escape(str)
    str = str:gsub('\\', '\\\239\187\191')
    str = str:gsub('{', '\\{')
    str = str:gsub('}', '\\}')
    str = str:gsub('\n', '\239\187\191\\N')
    return str
end

local function parse_fraction(value)
    local v = tonumber(value)
    if v then
        return v
    end

    local a, b = value:match('([^/:]+)[/:]([^/:]+)')
    a = tonumber(a)
    b = tonumber(b)
    if a and b then
        return a / b
    end
end

local function read_token(cmd, i)
    i = cmd:find('[^%s]', i)
    if not i then
        return true -- End of string.
    end

    local c = cmd:sub(i, i)
    if c == ';' then
        return true, i + 1 -- End of one command.
    elseif c == '"' then

        local e
        local s = i + 1
        local quotes_needed = false

        repeat
            s, e = cmd:find('["%s\\]', s)
            if not e then
                return false -- Unbalanced quotes.
            end

            c = cmd:sub(s, e)
            if c == '\\' then
                s = e + 2
                quotes_needed = true
            elseif c:find('%s') then
                s = cmd:find('[^%s]', e + 1)
                quotes_needed = true
            end
        until c == '"'

        -- Any argument parsing for command description will not need
        -- unquoted text, so we just do easy unquoting for arguments
        -- that didn't need quoting in the first place.
        -- This also makes re-quoting for command normalization unnecessary.
        if quotes_needed or cmd:find('^!', i + 1) then
            return cmd:sub(i, e), e + 1
        else
            return cmd:sub(i + 1, e - 1), e + 1
        end

    elseif c == '!' then

        local q = cmd:sub(i + 1, i + 1)
        if not q then
            return false -- Unbalanced quotes.
        end

        local s, e = cmd:find(q .. '!', i + 2, true)
        if not s then
            return false -- Unbalanced quotes.
        end

        -- See comment above regarding unquoting.
        local escaped = cmd:sub(i + 2, e - 2)
        if escaped:find('["%s\\]') or escaped:find('^!') then
            escaped = string.format('%q', escaped)
        end
        return escaped, e + 1

    else
        local s, e = cmd:find('[^%s;]*', i)
        return cmd:sub(s, e), e + 1
    end
end

local function parse_command(cmd)
    local i = 1
    local current_cmd
    local cmd_list = {}

    while i do
        local token

        token, i = read_token(cmd, i)
        if not current_cmd then
            current_cmd = {arguments = {}}
        end

        if not token then
            return -- Malformed command.
        elseif token == true then
            if not current_cmd.command then
                break -- Empty command / end of command list.
            elseif current_cmd.command ~= 'ignore' then
                current_cmd.command = current_cmd.command:gsub('_', '-')
                table.insert(cmd_list, current_cmd)
            end
            current_cmd = nil
        else
            if not current_cmd.command then
                current_cmd[prefixes[token] or 'command'] = token
            else
                table.insert(current_cmd.arguments, token)
            end
        end
    end

    return #cmd_list > 0 and cmd_list or nil
end

local function normalize_command(cmd_list)
    local ass = {}
    local normalized = {}

    for _, cmd in ipairs(cmd_list) do
        local n = {}
        local ass_n = {}
        if o.show_prefixes then
            table.insert(n, cmd.osd)
            table.insert(n, cmd.raw)
            table.insert(n, cmd.sync)
            table.insert(n, cmd.repeatable)

            local ass_p = {}
            table.insert(ass_p, cmd.osd)
            table.insert(ass_p, cmd.raw)
            table.insert(ass_p, cmd.sync)
            table.insert(ass_p, cmd.repeatable)
            if #ass_p > 0 then
                table.insert(ass_n, o.prefix_ass_style .. table.concat(ass_p, ' '))
            end
        end

        table.insert(n, cmd.command)
        table.insert(ass_n, o.command_ass_style .. ass_escape(cmd.command))

        for _, arg in ipairs(cmd.arguments) do
            table.insert(n, arg)
            table.insert(ass_n, o.argument_ass_style .. ass_escape(arg))
        end

        table.insert(ass, table.concat(ass_n, ' '))
        table.insert(normalized, table.concat(n, ' '))
    end

    return table.concat(normalized, '; '), table.concat(ass, '; ')
end

local function describe_seek(cmd)
    local target = tonumber(cmd.arguments[1])
    if not target then
        return 'Seek'
    end

    local flags = cmd.arguments[2] or ''
    local percent = string.find(flags, '-percent', 1, true)
    local absolute = string.find(flags, 'absolute', 1, true)

    if absolute then
        if percent then
            return string.format('Seek to %d%%', target)
        else
            local d = target < 0 and '-' or ''
            target = math.abs(target)
            local h = math.floor(target / 3600)
            local m = math.floor(target / 60 - h * 60)
            local s = math.floor(target - h * 3600 - m * 60)
            return string.format('Seek to %s%02d:%02d:%02d', d, h, m, s)
        end
    else
        local verb = target < 0 and 'backward' or 'forward'
        target = math.abs(target)

        if percent then
            target = string.format('%d%%', target)
        else
            local m = math.floor(target / 60)
            local s = math.floor(target - m * 60)
            if m > 0 then
                if s > 0 then
                    target = string.format('%d minute%s %d second%s', m, m ~= 1 and 's' or '', s, s ~= 1 and 's' or '')
                else
                    target = string.format('%d minute%s', m, m ~= 1 and 's' or '')
                end
            else
                target = string.format('%d second%s', target, s ~= 1 and 's' or '')
            end
        end

        return string.format('Seek %s by %s', verb, target)
    end
end

local function get_verb(d, cmd, property, value)
    if cmd.command == 'set' then
        if value == 'yes' then
            return d.verb[1]
        elseif value == 'no' then
            return d.verb[2]
        else
            local default_value = mp.get_property('option-info/' .. property .. '/default-value')

            if default_value then
                if value ~= default_value then
                    value = parse_fraction(value)
                    default_value = tonumber(default_value)
                end

                if value == default_value then
                    return 'reset'
                end
            end

            return 'set'
        end
    elseif d.verb[3] and (cmd.command == 'cycle' or cmd.command == 'cycle-values') then
        return d.verb[3]
    else
        value = parse_fraction(value)
        if cmd.command == 'multiply' then
            value = value - 1
        end
        return d.verb[(value < 0) and 2 or 1]
    end
end

local function describe_single_command(b, cmd)
    local value
    local property

    local d = commands[cmd.command]
    if d then
        value = cmd.arguments[1]
    elseif cmd.command == 'seek' then
        b.cmd = describe_seek(cmd)
        b.group = 'Seek'
        b.sort_key = cmd.arguments[1]
        return true
    elseif cmd.command == 'script-binding' then
        d = script_bindings[cmd.arguments[1]]
    elseif property_commands[cmd.command] then
        value = cmd.arguments[2]
        property = cmd.arguments[1]

        if cmd.command == 'cycle-values' then
            value = property == '"!reverse"' and -1 or 1
            property = cmd.arguments[value == 1 and 1 or 2]
        elseif cmd.command == 'cycle' then
            value = cmd.arguments[2] == 'down' and -1 or 1
        end

        d = properties[property]
    end

    if d then
        local text = d.text:format(d.verb and get_verb(d, cmd, property, value))
        b.cmd = text:gsub('^%l', string.upper)
        b.group = d.group
        b.sort_key = property or d.sort_key
        return true
    end
end

local function describe_command_list(b)
    if osd_pretty and b.comment then
        local json = utils.parse_json(b.comment, true)
        if json then
            b.cmd = json.text
            b.group = json.group or 'Other'
            b.group_weight = json.group_weight
            b.sort_key = json.sort_key
            return
        end
    end

    b.cmd = parse_command(b.cmd)
    if not b.cmd then
        return
    end

    if osd_pretty then
        local text
        local count = 0
        local single_cmd
        local show_text = false

        -- We can only really describe a single command from a command list.
        -- If the only other commands are print/show-text, we can assume they
        -- contain sensible text to describe the command as fallback.

        for _, c in ipairs(b.cmd) do
            if c.command == 'print-text' and not show_text then
                text = c.arguments[1]
            elseif c.command == 'show-text' then
                text = c.arguments[1]
                show_text = true
            else
                count = count + 1
                single_cmd = c
            end
        end

        if single_cmd and count == 1 and describe_single_command(b, single_cmd) then
            return
        elseif text then
            b.cmd = text:find('^"') and text:sub(2, #text - 1) or text
            b.group = 'Other'
            return
        end
    end

    if o.show_unkown or not osd_pretty then
        b.cmd, b.ass = normalize_command(b.cmd)
        return
    end

    b.cmd = nil
end

local function format_console(bindings, max_key_size)
    local group
    local lines = {}

    for _, b in ipairs(bindings) do
        if osd_pretty and group ~= b.group then
            group = b.group
            table.insert(lines, b.group .. ':')
        end

        table.insert(lines, string.format('%' .. max_key_size .. 's: %s', b.key, b.cmd))
    end

    console_bindings[osd_pretty] = table.concat(lines, '\n')
end

local function format_osd_pages()
    local margin_top = osd.res_y * (o.margin + osc_margin_top)
    local margin_bottom = osd.res_y * (o.margin + osc_margin_bottom)

    local max_lines = o.max_lines
    local height = osd.res_y - margin_top - margin_bottom
    local font_size = height / max_lines

    local scale = mp.get_property_native('display-hidpi-scale', 1.0)
    if font_size < scale * o.min_font_size then
        max_lines = math.ceil(height / (scale * o.min_font_size))
        font_size = height / max_lines
    end
    max_lines = max_lines - 2 -- Account for page indicator and empty line.

    local style = function(an, line, extra_ass, fs_factor)
        local x_offset = (an == 9 and -1) or (an == 7 and 1) or 0
        return string.format('\n{\\r\\q2\\fs%f\\bord%f\\shad%f\\an%d\\pos(%f,%f)}%s',
                             font_size * (fs_factor or 1),
                             font_size * o.font_border_ratio,
                             font_size * o.font_shadow_ratio,
                             an,
                             (1 + 0.01 * x_offset) * osd.res_x * o.horizontal_offset,
                             margin_top + font_size * line, extra_ass)
    end

    local line = 0
    local key_ass = {}
    local cmd_ass = {}
    local group_ass = {}
    local group

    osd_pages[osd_pretty] = {}
    local bindings = osd_bindings[osd_pretty]

    for i, b in ipairs(bindings) do
        local new_style = (not osd_pretty and line == 0)

        if osd_pretty and (group ~= b.group or line == 0) then
            group = b.group
            new_style = true
            line = (line == 0) and 1 or (line + 2)
            table.insert(group_ass, style(2, line, o.group_ass_style, o.group_font_ratio) .. ass_escape(b.group))
        end

        if new_style then
            table.insert(key_ass, style(9, line, o.key_ass_style))
            table.insert(cmd_ass, style(7, line, o.command_ass_style))
        end

        table.insert(key_ass, ass_escape(b.key) .. '\\N')
        table.insert(cmd_ass, (b.ass or ass_escape(b.cmd)) .. '\\N')
        line = line + 1

        -- Force a new page if we exceeded the quota or if it would orphan the group header.
        if line >= max_lines or i >= #bindings or
        (osd_pretty and max_lines - line < 3 and group ~= bindings[i + 1].group) then
            table.insert(osd_pages[osd_pretty], {table.concat(group_ass), table.concat(key_ass), table.concat(cmd_ass)})
            key_ass = {}
            cmd_ass = {}
            group_ass = {}
            group = nil
            line = 0
        end
    end

    for i, p in pairs(osd_pages[osd_pretty]) do
        if o.debug then
            local debug_rect = function(ass, x, y, w, h, c, a, t)
                table.insert(ass, string.format('\n{\\fs%f\\c&H%s&\\shad0\\alpha%s\\bord0\\pos(0,0)\\p1}', h, c, a))
                table.insert(ass, string.format('m %f %f l %f %f %f %f %f %f{\\p0}', x, y, x + w, y, x + w, y + h, x, y + h))
                if t then
                    table.insert(ass, string.format('\n{\\fs%f\\c&H%s&\\shad0\\bord0\\an5\\pos(%f,%f)}%s', h, c, x + w / 2, y + h / 2, t))
                end
            end

            -- OSC margins (shared script property)
            local m_top = osd.res_y * osc_margin_top
            local m_bottom = osd.res_y * osc_margin_bottom
            debug_rect(p, 0, 0, osd.res_x, m_top, '00FFFF', 'C8', '')
            debug_rect(p, 0, osd.res_y - m_bottom, osd.res_x, m_bottom, '00FFFF', 'C8', '')

            -- Margins (help.lua)
            debug_rect(p, 0, m_top, osd.res_x, margin_top - m_top, 'FF00', 'C8', string.format('%dx%d (%d)', osd.res_x, osd.res_y, max_lines))
            debug_rect(p, 0, osd.res_y - margin_bottom, osd.res_x, margin_bottom - m_bottom, 'FF00', 'C8', string.format('%dx%d (%d)', osd.res_x, osd.res_y, max_lines))

            for l = 0, max_lines + 1 do
                local c = (l == max_lines or l == max_lines + 1) and 'FF0000' --[[Page indicator and separator line]] or 'FF'
                debug_rect(p, 0, margin_top + font_size * l, osd.res_x, font_size, c, l % 2 == 0 and 'AF' or 'CF', l)
            end
        end

        table.insert(p, style(2, max_lines + 2, o.page_ass_style))
        table.insert(p, string.format('Page %d of %d', i, #osd_pages[osd_pretty]))
        osd_pages[osd_pretty][i] = table.concat(p)
    end
end

local function get_bindings()
    local keys = {}
    for _, b in pairs(mp.get_property_native('input-bindings', {})) do
        if b.section ~= 'encode' and b.priority >= 0 and (not keys[b.key] or keys[b.key].priority < b.priority) then
            keys[b.key] = b
        end
    end

    local bindings = {}
    for _, b in pairs(keys) do
        describe_command_list(b)
        if b.cmd and b.cmd ~= '' then
            b.group_weight = b.group_weight or (group_weights[b.group] or math.huge)
            b.group = b.group or 'Unknown'
            b.sort_key = b.sort_key or ''
            table.insert(bindings, b)
        end
    end

    table.sort(bindings, function(a, b)
        if a.group_weight ~= b.group_weight then
            return a.group_weight < b.group_weight
        elseif a.group ~= b.group then
            return a.group < b.group
        elseif a.sort_key ~= b.sort_key then
            return a.sort_key < b.sort_key
        elseif a.cmd ~= b.cmd then
            return a.cmd < b.cmd
        else
            return a.key < b.key
        end
    end)

    local i = 1
    local max_key_size = 0

    while i <= #bindings do
        local b = bindings[i]
        b.key = {b.key}

        i = i + 1
        while i <= #bindings do
            if b.cmd ~= bindings[i].cmd then
                break
            end

            table.insert(b.key, bindings[i].key)
            table.remove(bindings, i)
        end

        b.key = table.concat(b.key, ', ')
        max_key_size = math.max(max_key_size, #b.key)
    end

    osd_pages[osd_pretty] = nil
    osd_bindings[osd_pretty] = bindings
    format_console(bindings, max_key_size)
end

local function update_osd(advance_page)
    if not console_bindings[osd_pretty] then
        get_bindings()
    end
    if not osd_pages[osd_pretty] then
        format_osd_pages()
    end

    if advance_page then
        osd_page = osd_page % #osd_pages[osd_pretty] + 1
    else
        osd_page = math.min(osd_page, #osd_pages[osd_pretty])
    end

    osd.data = osd_pages[osd_pretty][osd_page] or 'No input bindings found'
    osd:update()
end

local function show_help(pretty)
    local update_console = not osd_timer:is_enabled()
    if osd_pretty ~= pretty then
        osd_page = 0
        osd_pretty = pretty
        update_console = true
    end

    update_osd(true)
    if update_console then
        local text = console_bindings[osd_pretty]
        mp.msg.info(#text > 0 and text or 'No input bindings found')
    end

    osd_timer:kill()
    osd_timer:resume()
end

mp.observe_property('osd-dimensions', 'native', function(_, value)
    if osd.res_x ~= value.w or osd.res_y ~= value.h then
        osd.res_x = value.w or 1280
        osd.res_y = value.h or 720
        osd_pages = {}

        if osd_timer:is_enabled() then
            update_osd(false)
        end
    end
end)

mp.observe_property('shared-script-properties', 'native', function(_, value)
    if value['osc-margins'] then
        osc_margin_top, osc_margin_bottom = value['osc-margins']:match('[^,]+,[^,]+,([^,]+),([^,]+)')
        osc_margin_top = tonumber(osc_margin_top) or 0
        osc_margin_bottom = tonumber(osc_margin_bottom) or 0
        osd_pages = {}
    elseif osc_margin_top ~= 0 or osc_margin_bottom ~= 0 then
        osc_margin_top = 0
        osc_margin_bottom = 0
        osd_pages = {}
    end

    for k, v in pairs(value) do
        local script = k:match('^help%-(.+)')
        if script and not script_bindings_seen[script] then
            osd_pages = {}
            osd_bindings = {}
            console_bindings = {}
            script_bindings_seen[script] = true
            for c, d in pairs(utils.parse_json(v)) do
                script_bindings[script .. '/' .. c] = d
            end
        end
    end

    if osd_timer:is_enabled() and #osd_pages == 0 then
        update_osd(false)
    end
end)

mp.add_key_binding('h', 'show-help', function() show_help(true) end)
mp.add_key_binding('H', 'show-help-raw', function() show_help(false) end)
