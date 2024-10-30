-- Copyright (C) 2019 the mpv developers
--
-- Permission to use, copy, modify, and/or distribute this software for any
-- purpose with or without fee is hereby granted, provided that the above
-- copyright notice and this permission notice appear in all copies.
--
-- THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
-- WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
-- MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
-- SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
-- WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
-- OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
-- CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

local utils = require 'mp.utils'
local assdraw = require 'mp.assdraw'

local function detect_platform()
    local platform = mp.get_property_native('platform')
    if platform == 'darwin' or platform == 'windows' then
        return platform
    elseif os.getenv('WAYLAND_DISPLAY') then
        return 'wayland'
    end
    return 'x11'
end

local platform = detect_platform()

-- Default options
local opts = {
    font = "",
    font_size = 24,
    border_size = 1.5,
    scale_with_window = "auto",
    case_sensitive = platform ~= 'windows' and true or false,
    history_dedup = true,
    font_hw_ratio = 'auto',
}

-- Apply user-set options
require 'mp.options'.read_options(opts)

local styles = {
    -- Colors are stolen from base16 Eighties by Chris Kempson
    -- and converted to BGR as is required by ASS.
    -- 2d2d2d 393939 515151 697374
    -- 939fa0 c8d0d3 dfe6e8 ecf0f2
    -- 7a77f2 5791f9 66ccff 99cc99
    -- cccc66 cc9966 cc99cc 537bd2

    debug = '{\\1c&Ha09f93&}',
    v = '{\\1c&H99cc99&}',
    warn = '{\\1c&H66ccff&}',
    error = '{\\1c&H7a77f2&}',
    fatal = '{\\1c&H5791f9&}',
    suggestion = '{\\1c&Hcc99cc&}',
    selected_suggestion = '{\\1c&H2fbdfa&\\b1}',
    default_item = '{\\1c&H2fbdfa&}',
    disabled = '{\\1c&Hcccccc&}',
}

local terminal_styles = {
    debug = '\027[90m',
    v = '\027[32m',
    warn = '\027[33m',
    error = '\027[31m',
    fatal = '\027[91m',
    selected_suggestion = '\027[7m',
    default_item = '\027[1m',
    disabled = '\027[38;5;8m',
}

local repl_active = false
local osd_msg_active = false
local insert_mode = false
local pending_update = false
local line = ''
local cursor = 1
local default_prompt = '>'
local prompt = default_prompt
local bottom_left_margin = 6
local default_id = 'default'
local id = default_id
local histories = {[id] = {}}
local history = histories[id]
local history_pos = 1
local searching_history = false
local log_buffers = {[id] = {}}
local key_bindings = {}
local dont_bind_up_down = false
local global_margins = { t = 0, b = 0 }
local input_caller

local suggestion_buffer = {}
local selected_suggestion_index
local completion_pos
local completion_append
local path_separator = platform == 'windows' and '\\' or '/'
local completion_old_line
local completion_old_cursor

local selectable_items
local matches = {}
local selected_match = 1
local first_match_to_print = 1
local default_item

local set_active


local function get_font()
    if opts.font ~= '' then
        return opts.font
    end

    if selectable_items and not searching_history then
        return
    end

    -- Pick a better default font for Windows and macOS
    if platform == 'windows' then
        return 'Consolas'
    end

    if platform == 'darwin' then
        return 'Menlo'
    end

    return 'monospace'
end


-- Naive helper function to find the next UTF-8 character in 'str' after 'pos'
-- by skipping continuation bytes. Assumes 'str' contains valid UTF-8.
local function next_utf8(str, pos)
    if pos > str:len() then return pos end
    repeat
        pos = pos + 1
    until pos > str:len() or str:byte(pos) < 0x80 or str:byte(pos) > 0xbf
    return pos
end

-- As above, but finds the previous UTF-8 character in 'str' before 'pos'
local function prev_utf8(str, pos)
    if pos <= 1 then return pos end
    repeat
        pos = pos - 1
    until pos <= 1 or str:byte(pos) < 0x80 or str:byte(pos) > 0xbf
    return pos
end

local function len_utf8(str)
    local len = 0
    local pos = 1
    while pos <= str:len() do
        pos = next_utf8(str, pos)
        len = len + 1
    end
    return len
end


-- Functions to calculate the font width.
local width_length_ratio = 0.5
local osd_width, osd_height = 100, 100

---Update osd resolution if valid
local function update_osd_resolution()
    local dim = mp.get_property_native('osd-dimensions')
    if not dim or dim.w == 0 or dim.h == 0 then
        return
    end
    osd_width = dim.w
    osd_height = dim.h
end

local text_osd = mp.create_osd_overlay('ass-events')
text_osd.compute_bounds, text_osd.hidden = true, true

local function measure_bounds(ass_text)
    update_osd_resolution()
    text_osd.res_x, text_osd.res_y = osd_width, osd_height
    text_osd.data = ass_text
    local res = text_osd:update()
    return res.x0, res.y0, res.x1, res.y1
end

---Measure text width and normalize to a font size of 1
---text has to be ass safe
local function normalized_text_width(text, size, horizontal)
    local align, rotation = horizontal and 7 or 1, horizontal and 0 or -90
    local template = '{\\pos(0,0)\\rDefault\\blur0\\bord0\\shad0\\q2\\an%s\\fs%s\\fn%s\\frz%s}%s'
    size = size / 0.8
    local width
    -- Limit to 5 iterations
    local repetitions_left = 5
    for i = 1, repetitions_left do
        size = size * 0.8
        local ass = assdraw.ass_new()
        ass.text = template:format(align, size, get_font(), rotation, text)
        local _, _, x1, y1 = measure_bounds(ass.text)
        -- Check if nothing got clipped
        if x1 and x1 < osd_width and y1 < osd_height then
            width = horizontal and x1 or y1
            break
        end
        if i == repetitions_left then
            width = 0
        end
    end
    return width / size, horizontal and osd_width or osd_height
end

local function fit_on_osd(text)
    local estimated_width = #text * width_length_ratio
    if osd_width >= osd_height then
        -- Fill the osd as much as possible, bigger is more accurate.
        return math.min(osd_width / estimated_width, osd_height), true
    else
        return math.min(osd_height / estimated_width, osd_width), false
    end
end

local measured_font_hw_ratio = nil
local function get_font_hw_ratio()
    local font_hw_ratio = tonumber(opts.font_hw_ratio)
    if font_hw_ratio then
        return font_hw_ratio
    end
    if not measured_font_hw_ratio then
        local alphabet = 'abcdefghijklmnopqrstuvwxyz'
        local text = alphabet:rep(3)
        update_osd_resolution()
        local size, horizontal = fit_on_osd(text)
        local normalized_width = normalized_text_width(text, size * 0.9, horizontal)
        measured_font_hw_ratio = #text / normalized_width * 0.95
    end
    return measured_font_hw_ratio
end


-- Escape a string for verbatim display on the OSD
local function ass_escape(str)
    return mp.command_native({'escape-ass', str})
end

local function should_scale()
    return opts.scale_with_window == "yes" or
           (opts.scale_with_window == "auto" and mp.get_property_native("osd-scale-by-window"))
end

local function scale_factor()
    local height = mp.get_property_native('osd-height')

    if should_scale() and height > 0 then
        return height / 720
    end

    return mp.get_property_native('display-hidpi-scale', 1)
end

local function get_scaled_osd_dimensions()
    local dims = mp.get_property_native('osd-dimensions')
    local scale = scale_factor()

    return dims.w / scale, dims.h /scale
end

local function calculate_max_log_lines()
    if not mp.get_property_native('vo-configured')
       or not mp.get_property_native('video-osd') then
        -- Subtract 1 for the input line and for each line in the status line.
        -- This does not detect wrapped lines.
        return mp.get_property_native('term-size/h', 24) - 2 -
               select(2, mp.get_property('term-status-msg'):gsub('\\n', ''))
    end

    return math.floor((select(2, get_scaled_osd_dimensions())
                       * (1 - global_margins.t - global_margins.b)
                       - bottom_left_margin)
                      / opts.font_size
                      -- Subtract 1 for the input line and 0.5 for the empty
                      -- line between the log and the input line.
                      - 1.5)
end

-- Takes a list of strings, a max width in characters and
-- optionally a max row count.
-- The result contains at least one column.
-- Rows are cut off from the top if rows_max is specified.
-- returns a string containing the formatted table and the row count
local function format_table(list, width_max, rows_max)
    if #list == 0 then
        return '', 0
    end

    local spaces_min = 2
    local spaces_max = 8
    local list_size = #list
    local column_count = 1
    local row_count = list_size
    local column_widths
    -- total width without spacing
    local width_total = 0

    local list_widths = {}
    for i, item in ipairs(list) do
        list_widths[i] = len_utf8(item)
    end

    -- use as many columns as possible
    for columns = 2, list_size do
        local rows_lower_bound = math.min(rows_max, math.ceil(list_size / columns))
        local rows_upper_bound = math.min(rows_max, list_size,
                                          math.ceil(list_size / (columns - 1) - 1))
        for rows = rows_upper_bound, rows_lower_bound, -1 do
            local cw = {}
            width_total = 0

            -- find out width of each column
            for column = 1, columns do
                local width = 0
                for row = 1, rows do
                    local i = row + (column - 1) * rows
                    local item_width = list_widths[i]
                    if not item_width then break end
                    if width < item_width then
                        width = item_width
                    end
                end
                cw[column] = width
                width_total = width_total + width
                if width_total + (columns - 1) * spaces_min > width_max then
                    break
                end
            end

            if width_total + (columns - 1) * spaces_min <= width_max then
                row_count = rows
                column_count = columns
                column_widths = cw
            else
                break
            end
        end
        if width_total + (columns - 1) * spaces_min > width_max then
            break
        end
    end

    local spaces = math.floor((width_max - width_total) / (column_count - 1))
    spaces = math.max(spaces_min, math.min(spaces_max, spaces))
    local spacing = column_count > 1
                    and ass_escape(string.format('%' .. spaces .. 's', ' '))
                    or ''

    local rows = {}
    for row = 1, row_count do
        local columns = {}
        for column = 1, column_count do
            local i = row + (column - 1) * row_count
            if i > #list then break end
            -- more then 99 leads to 'invalid format (width or precision too long)'
            local format_string = column == column_count and '%s'
                                  or '%-' .. math.min(column_widths[column], 99) .. 's'
            columns[column] = ass_escape(string.format(format_string, list[i]))

            if i == selected_suggestion_index then
                columns[column] = styles.selected_suggestion .. columns[column]
                                  .. '{\\b0}'.. styles.suggestion
            end
        end
        -- first row is at the bottom
        rows[row_count - row + 1] = table.concat(columns, spacing)
    end
    return table.concat(rows, ass_escape('\n')), row_count
end

local function fuzzy_find(needle, haystacks)
    local result = require 'mp.fzy'.filter(needle, haystacks)
    if line ~= '' then -- Prevent table.sort() from reordering the items.
        table.sort(result, function (i, j)
            return i[3] > j[3]
        end)
    end
    for i, value in ipairs(result) do
        result[i] = value[1]
    end
    return result
end

local function populate_log_with_matches()
    if not selectable_items or selected_match == 0 then
        return
    end

    log_buffers[id] = {}
    local log = log_buffers[id]

    local max_log_lines = calculate_max_log_lines()
    local print_counter = false

    if #matches > max_log_lines then
        print_counter = true
        max_log_lines = max_log_lines - 1
    end

    if selected_match < first_match_to_print then
        first_match_to_print = selected_match
    elseif selected_match > first_match_to_print + max_log_lines - 1 then
        first_match_to_print = selected_match - max_log_lines + 1
    end

    local last_match_to_print  = math.min(first_match_to_print + max_log_lines - 1,
                                          #matches)

    if print_counter then
        log[1] = {
            text = '',
            style = styles.disabled .. selected_match .. '/' .. #matches ..
                    ' {\\fs' .. opts.font_size * 0.75 .. '}[' ..
                    first_match_to_print .. '-' .. last_match_to_print .. ']',
            terminal_style = terminal_styles.disabled .. selected_match .. '/' ..
                             #matches .. ' [' .. first_match_to_print .. '-' ..
                             last_match_to_print .. ']',
        }
    end

    for i = first_match_to_print, last_match_to_print do
        local style = ''
        local terminal_style = ''

        if i == selected_match then
            style = styles.selected_suggestion
            terminal_style = terminal_styles.selected_suggestion
        end
        if matches[i].index == default_item then
            style = style .. styles.default_item
            terminal_style = terminal_style .. terminal_styles.default_item
        end

        log[#log + 1] = {
            text = matches[i].text,
            style = style,
            terminal_style = terminal_style,
        }
    end
end

local function print_to_terminal()
    -- Clear the log after closing the console.
    if not repl_active then
        if osd_msg_active then
            mp.osd_message('')
        end
        osd_msg_active = false
        return
    end

    populate_log_with_matches()

    local log = ''
    local clip = selectable_items and mp.get_property('term-clip-cc') or ''
    for _, log_line in ipairs(log_buffers[id]) do
        log = log .. clip .. log_line.terminal_style .. log_line.text .. '\027[0m\n'
    end

    local suggestions = ''
    for i, suggestion in ipairs(suggestion_buffer) do
        if i == selected_suggestion_index then
            suggestions = suggestions .. terminal_styles.selected_suggestion ..
                          suggestion .. '\027[0m'
        else
            suggestions = suggestions .. suggestion
        end
        suggestions = suggestions .. (i < #suggestion_buffer and '\t' or '\n')
    end

    local before_cur = line:sub(1, cursor - 1)
    local after_cur = line:sub(cursor)
    -- Ensure there is a character with inverted colors to print.
    if after_cur == '' then
        after_cur = ' '
    end

    mp.osd_message(log .. suggestions .. prompt .. ' ' .. before_cur ..
                  '\027[7m' .. after_cur:sub(1, 1) .. '\027[0m' ..
                   after_cur:sub(2), 999)
    osd_msg_active = true
end

-- Render the REPL and console as an ASS OSD
local function update()
    pending_update = false

    -- Unlike vo-configured, current-vo doesn't become falsy while switching VO,
    -- which would print the log to the OSD.
    if not mp.get_property('current-vo') or not mp.get_property_native('video-osd') then
        print_to_terminal()
        return
    end

    -- Clear the OSD if the console was being printed to the terminal
    if osd_msg_active then
        mp.osd_message('')
        osd_msg_active = false
    end

    -- Clear the OSD if the REPL is not active
    if not repl_active then
        mp.set_osd_ass(0, 0, '')
        return
    end

    local screenx, screeny = get_scaled_osd_dimensions()

    local coordinate_top = math.floor(global_margins.t * screeny + 0.5)
    local clipping_coordinates = '0,' .. coordinate_top .. ',' ..
                                 screenx .. ',' .. screeny
    local ass = assdraw.ass_new()
    local has_shadow = mp.get_property('osd-border-style'):find('box$') == nil
    local font = get_font()
    local style = '{\\r' ..
                  '\\1a&H00&\\3a&H00&\\1c&Heeeeee&\\3c&H111111&' ..
                  (has_shadow and '\\4a&H99&\\4c&H000000&' or '') ..
                  (font and '\\fn' .. font or '') ..
                  '\\fs' .. opts.font_size ..
                  '\\bord' .. opts.border_size .. '\\xshad0\\yshad1\\fsp0' ..
                  (selectable_items and '\\q2' or '\\q1') ..
                  '\\clip(' .. clipping_coordinates .. ')}'
    -- Create the cursor glyph as an ASS drawing. ASS will draw the cursor
    -- inline with the surrounding text, but it sets the advance to the width
    -- of the drawing. So the cursor doesn't affect layout too much, make it as
    -- thin as possible and make it appear to be 1px wide by giving it 0.5px
    -- horizontal borders.
    local cheight = opts.font_size * 8
    local cglyph = '{\\rDefault' ..
                   (mp.get_property_native('focused') == false
                    and '\\alpha&HFF&' or '\\1a&H44&\\3a&H44&\\4a&H99&') ..
                   '\\1c&Heeeeee&\\3c&Heeeeee&\\4c&H000000&' ..
                   '\\xbord0.5\\ybord0\\xshad0\\yshad1\\p4\\pbo24}' ..
                   'm 0 0 l 1 0 l 1 ' .. cheight .. ' l 0 ' .. cheight ..
                   '{\\p0}'
    local before_cur = ass_escape(line:sub(1, cursor - 1))
    local after_cur = ass_escape(line:sub(cursor))

    -- Render log messages as ASS.
    -- This will render at most screeny / font_size - 1 messages.

    local lines_max = calculate_max_log_lines()
    local suggestion_ass = ''
    if next(suggestion_buffer) then
        -- Estimate how many characters fit in one line
        local width_max = math.floor((screenx - bottom_left_margin -
                                     mp.get_property_native('osd-margin-x') * 2 * screeny / 720)
                                     / opts.font_size * get_font_hw_ratio())

        local suggestions, rows = format_table(suggestion_buffer, width_max, lines_max)
        lines_max = lines_max - rows
        suggestion_ass = style .. styles.suggestion .. suggestions .. '\\N'
    end

    populate_log_with_matches()

    local log_ass = ''
    local log_buffer = log_buffers[id]
    local log_messages = #log_buffer
    local log_max_lines = math.max(0, lines_max)
    if log_max_lines < log_messages then
        log_messages = log_max_lines
    end
    for i = #log_buffer - log_messages + 1, #log_buffer do
        log_ass = log_ass .. style .. log_buffer[i].style ..
                  ass_escape(log_buffer[i].text) .. '\\N'
    end

    ass:new_event()
    ass:an(1)
    ass:pos(bottom_left_margin, screeny - bottom_left_margin - global_margins.b * screeny)
    ass:append(log_ass .. '\\N')
    ass:append(suggestion_ass)
    ass:append(style .. ass_escape(prompt) .. ' ' .. before_cur)
    ass:append(cglyph)
    ass:append(style .. after_cur)

    -- Redraw the cursor with the REPL text invisible. This will make the
    -- cursor appear in front of the text.
    ass:new_event()
    ass:an(1)
    ass:pos(bottom_left_margin, screeny - bottom_left_margin - global_margins.b * screeny)
    ass:append(style .. '{\\alpha&HFF&}' .. ass_escape(prompt) .. ' ' .. before_cur)
    ass:append(cglyph)
    ass:append(style .. '{\\alpha&HFF&}' .. after_cur)

    mp.set_osd_ass(screenx, screeny, ass.text)
end

local update_timer = nil
update_timer = mp.add_periodic_timer(0.05, function()
    if pending_update then
        update()
    else
        update_timer:kill()
    end
end)
update_timer:kill()

-- Add a line to the log buffer (which is limited to 100 lines)
local function log_add(text, style, terminal_style)
    local log_buffer = log_buffers[id]
    log_buffer[#log_buffer + 1] = {
        text = text,
        style = style or '',
        terminal_style = terminal_style or '',
    }
    if #log_buffer > 100 then
        table.remove(log_buffer, 1)
    end

    if repl_active then
        if not update_timer:is_enabled() then
            update()
            update_timer:resume()
        else
            pending_update = true
        end
    end
end

-- Add a line to the history and deduplicate
local function history_add(text)
    if opts.history_dedup then
        -- More recent entries are more likely to be repeated
        for i = #history, 1, -1 do
            if history[i] == text then
                table.remove(history, i)
                break
            end
        end
    end

    history[#history + 1] = text
end

local function handle_edit()
    if selectable_items then
        matches = {}
        selected_match = 1

        for i, match in ipairs(fuzzy_find(line, selectable_items)) do
            matches[i] = { index = match, text = selectable_items[match] }
        end
    end

    suggestion_buffer = {}
    update()

    if input_caller then
        mp.commandv('script-message-to', input_caller, 'input-event', 'edited',
                    utils.format_json({line}))
    end
end

-- Insert a character at the current cursor position (any_unicode)
local function handle_char_input(c)
    if insert_mode then
        line = line:sub(1, cursor - 1) .. c .. line:sub(next_utf8(line, cursor))
    else
        line = line:sub(1, cursor - 1) .. c .. line:sub(cursor)
    end
    cursor = cursor + #c
    handle_edit()
end

-- Remove the character behind the cursor (Backspace)
local function handle_backspace()
    if cursor <= 1 then return end
    local prev = prev_utf8(line, cursor)
    line = line:sub(1, prev - 1) .. line:sub(cursor)
    cursor = prev
    handle_edit()
end

-- Remove the character in front of the cursor (Del)
local function handle_del()
    if cursor > line:len() then return end
    line = line:sub(1, cursor - 1) .. line:sub(next_utf8(line, cursor))
    handle_edit()
end

-- Toggle insert mode (Ins)
local function handle_ins()
    insert_mode = not insert_mode
end

-- Move the cursor to the next character (Right)
local function next_char()
    cursor = next_utf8(line, cursor)
    suggestion_buffer = {}
    update()
end

-- Move the cursor to the previous character (Left)
local function prev_char()
    cursor = prev_utf8(line, cursor)
    suggestion_buffer = {}
    update()
end

-- Clear the current line (Ctrl+C)
local function clear()
    line = ''
    cursor = 1
    insert_mode = false
    history_pos = #history + 1
    handle_edit()
end

-- Close the REPL if the current line is empty, otherwise delete the next
-- character (Ctrl+D)
local function maybe_exit()
    if line == '' then
        set_active(false)
    else
        handle_del()
    end
end

local function help_command(param)
    local cmdlist = mp.get_property_native('command-list')
    table.sort(cmdlist, function(c1, c2)
        return c1.name < c2.name
    end)
    local output = ''
    if param == '' then
        output = 'Available commands:\n'
        for _, cmd in ipairs(cmdlist) do
            output = output  .. '  ' .. cmd.name
        end
        output = output .. '\n'
        output = output .. 'Use "help command" to show information about a command.\n'
        output = output .. "ESC or Ctrl+d exits the console.\n"
    else
        local cmd = nil
        for _, curcmd in ipairs(cmdlist) do
            if curcmd.name:find(param, 1, true) then
                cmd = curcmd
                if curcmd.name == param then
                    break -- exact match
                end
            end
        end
        if not cmd then
            log_add('No command matches "' .. param .. '"!', styles.error,
                    terminal_styles.error)
            return
        end
        output = output .. 'Command "' .. cmd.name .. '"\n'
        for _, arg in ipairs(cmd.args) do
            output = output .. '    ' .. arg.name .. ' (' .. arg.type .. ')'
            if arg.optional then
                output = output .. ' (optional)'
            end
            output = output .. '\n'
        end
        if cmd.vararg then
            output = output .. 'This command supports variable arguments.\n'
        end
    end
    log_add(output:sub(1, -2))
end

local function unbind_mouse()
    mp.remove_key_binding('_console_mouse_move')
    mp.remove_key_binding('_console_mbtn_left')
end

-- Run the current command and clear the line (Enter)
local function handle_enter()
    if searching_history then
        searching_history = false
        selectable_items = nil
        line = #matches > 0 and matches[selected_match].text or ''
        cursor = #line + 1
        log_buffers[id] = {}
        update()
        unbind_mouse()
        return
    end

    if line == '' and input_caller == nil then
        return
    end
    if history[#history] ~= line and line ~= '' then
        history_add(line)
    end

    if selectable_items then
        if #matches > 0 then
            mp.commandv('script-message-to', input_caller, 'input-event', 'submit',
                        utils.format_json({matches[selected_match].index}))
        end
        set_active(false)
    elseif input_caller then
        mp.commandv('script-message-to', input_caller, 'input-event', 'submit',
                    utils.format_json({line}))
    else
        -- match "help [<text>]", return <text> or "", strip all whitespace
        local help = line:match('^%s*help%s+(.-)%s*$') or
                     (line:match('^%s*help$') and '')
        if help then
            help_command(help)
        else
            mp.command(line)
        end
    end

    clear()
end

local function determine_hovered_item()
    local height = select(2, get_scaled_osd_dimensions())
    local y = mp.get_property_native('mouse-pos').y / scale_factor()
              - global_margins.t * height
    -- Calculate how many lines could be printed without decreasing them for
    -- the input line and OSC.
    local max_lines = height / opts.font_size
    local clicked_line = math.ceil(y / height * max_lines)

    local offset = first_match_to_print - 1
    local min_line = 1
    max_lines = calculate_max_log_lines()

    -- Subtract 1 line for the position counter.
    if first_match_to_print > 1 or offset + max_lines < #matches then
        min_line = 2
        offset = offset - 1
    end

    if #matches < max_lines then
        clicked_line = clicked_line - (max_lines - #matches)
        max_lines = #matches
    end

    if clicked_line >= min_line and clicked_line <= max_lines then
        return offset + clicked_line
    end
end

local function bind_mouse()
    mp.add_forced_key_binding('MOUSE_MOVE', '_console_mouse_move', function()
        local item = determine_hovered_item()
        if item and item ~= selected_match then
            selected_match = item
            update()
        end
    end)

    mp.add_forced_key_binding('MBTN_LEFT', '_console_mbtn_left', function()
        local item = determine_hovered_item()
        if item then
            selected_match = item
            handle_enter()
        else
            set_active(false)
        end
    end)
end

-- Go to the specified position in the command history
local function go_history(new_pos)
    local old_pos = history_pos
    history_pos = new_pos

    -- Restrict the position to a legal value
    if history_pos > #history + 1 then
        history_pos = #history + 1
    elseif history_pos < 1 then
        history_pos = 1
    end

    -- Do nothing if the history position didn't actually change
    if history_pos == old_pos then
        return
    end

    -- If the user was editing a non-history line, save it as the last history
    -- entry. This makes it much less frustrating to accidentally hit Up/Down
    -- while editing a line.
    if old_pos == #history + 1 and line ~= '' and history[#history] ~= line then
        history_add(line)
    end

    -- Now show the history line (or a blank line for #history + 1)
    if history_pos <= #history then
        line = history[history_pos]
    else
        line = ''
    end
    cursor = line:len() + 1
    insert_mode = false
    suggestion_buffer = {}
    update()
end

-- Go to the specified relative position in the command history (Up, Down)
local function move_history(amount, is_wheel)
    if is_wheel and selectable_items then
        local max_lines = calculate_max_log_lines()

        -- Update selected_match only if it's the first or last printed item and
        -- there are hidden items.
        if (amount > 0 and selected_match == first_match_to_print
            and first_match_to_print + max_lines - 2 < #matches)
           or (amount < 0 and selected_match == first_match_to_print + max_lines - 2
               and first_match_to_print > 1) then
            selected_match = selected_match + amount
        end

        if amount > 0 and first_match_to_print < #matches - max_lines + 2
           or amount < 0 and first_match_to_print > 1 then
           -- math.min and math.max would only be needed with amounts other than
           -- 1 and -1.
            first_match_to_print = math.min(
                math.max(first_match_to_print + amount, 1), #matches - max_lines + 2)
        end

        local item = determine_hovered_item()
        if item then
            selected_match = item
        end

        update()
        return
    end

    if selectable_items then
        selected_match = selected_match + amount
        if selected_match > #matches then
            selected_match = 1
        elseif selected_match < 1 then
            selected_match = #matches
        end
        update()
        return
    end

    go_history(history_pos + amount)
end

-- Go to the first command in the command history (PgUp)
local function handle_pgup()
    if selectable_items then
        selected_match = math.max(selected_match - calculate_max_log_lines() + 2, 1)
        update()
        return
    end

    go_history(1)
end

-- Stop browsing history and start editing a blank line (PgDown)
local function handle_pgdown()
    if selectable_items then
        selected_match = math.min(selected_match + calculate_max_log_lines() - 2, #matches)
        update()
        return
    end

    go_history(#history + 1)
end

local function search_history()
    if selectable_items or #history == 0 then
        return
    end

    searching_history = true
    selectable_items = {}
    matches = {}
    selected_match = 1
    first_match_to_print = 1

    for i = 1, #history do
        selectable_items[i] = history[#history + 1 - i]
    end

    for i, match in ipairs(fuzzy_find(line, selectable_items)) do
        matches[i] = { index = match, text = selectable_items[match] }
    end

    update()
    bind_mouse()
end

local function page_up_or_prev_char()
    if selectable_items then
        handle_pgup()
    else
        prev_char()
    end
end

local function page_down_or_next_char()
    if selectable_items then
        handle_pgdown()
    else
        next_char()
    end
end

-- Move to the start of the current word, or if already at the start, the start
-- of the previous word. (Ctrl+Left)
local function prev_word()
    -- This is basically the same as next_word() but backwards, so reverse the
    -- string in order to do a "backwards" find. This wouldn't be as annoying
    -- to do if Lua didn't insist on 1-based indexing.
    cursor = line:len() - select(2, line:reverse():find('%s*[^%s]*', line:len() - cursor + 2)) + 1
    suggestion_buffer = {}
    update()
end

-- Move to the end of the current word, or if already at the end, the end of
-- the next word. (Ctrl+Right)
local function next_word()
    cursor = select(2, line:find('%s*[^%s]*', cursor)) + 1
    suggestion_buffer = {}
    update()
end

-- Move the cursor to the beginning of the line (HOME)
local function go_home()
    cursor = 1
    suggestion_buffer = {}
    update()
end

-- Move the cursor to the end of the line (END)
local function go_end()
    cursor = line:len() + 1
    suggestion_buffer = {}
    update()
end

-- Delete from the cursor to the beginning of the word (Ctrl+Backspace)
local function del_word()
    local before_cur = line:sub(1, cursor - 1)
    local after_cur = line:sub(cursor)

    before_cur = before_cur:gsub('[^%s]+%s*$', '', 1)
    line = before_cur .. after_cur
    cursor = before_cur:len() + 1
    handle_edit()
end

-- Delete from the cursor to the end of the word (Ctrl+Del)
local function del_next_word()
    if cursor > line:len() then return end

    local before_cur = line:sub(1, cursor - 1)
    local after_cur = line:sub(cursor)

    after_cur = after_cur:gsub('^%s*[^%s]+', '', 1)
    line = before_cur .. after_cur
    handle_edit()
end

-- Delete from the cursor to the end of the line (Ctrl+K)
local function del_to_eol()
    line = line:sub(1, cursor - 1)
    handle_edit()
end

-- Delete from the cursor back to the start of the line (Ctrl+U)
local function del_to_start()
    line = line:sub(cursor)
    cursor = 1
    handle_edit()
end

-- Empty the log buffer of all messages (Ctrl+L)
local function clear_log_buffer()
    log_buffers[id] = {}
    update()
end

-- Returns a string of UTF-8 text from the clipboard (or the primary selection)
local function get_clipboard(clip)
    if platform == 'x11' then
        local res = utils.subprocess({
            args = { 'xclip', '-selection', clip and 'clipboard' or 'primary', '-out' },
            playback_only = false,
        })
        if not res.error then
            return res.stdout
        end
    elseif platform == 'wayland' then
        local res = utils.subprocess({
            args = { 'wl-paste', clip and '-n' or  '-np' },
            playback_only = false,
        })
        if not res.error then
            return res.stdout
        end
    elseif platform == 'windows' then
        local res = utils.subprocess({
            args = { 'powershell', '-NoProfile', '-Command', [[& {
                Trap {
                    Write-Error -ErrorRecord $_
                    Exit 1
                }

                $clip = ""
                if (Get-Command "Get-Clipboard" -errorAction SilentlyContinue) {
                    $clip = Get-Clipboard -Raw -Format Text -TextFormatType UnicodeText
                } else {
                    Add-Type -AssemblyName PresentationCore
                    $clip = [Windows.Clipboard]::GetText()
                }

                $clip = $clip -Replace "`r",""
                $u8clip = [System.Text.Encoding]::UTF8.GetBytes($clip)
                [Console]::OpenStandardOutput().Write($u8clip, 0, $u8clip.Length)
            }]] },
            playback_only = false,
        })
        if not res.error then
            return res.stdout
        end
    elseif platform == 'darwin' then
        local res = utils.subprocess({
            args = { 'pbpaste' },
            playback_only = false,
        })
        if not res.error then
            return res.stdout
        end
    end
    return ''
end

-- Paste text from the window-system's clipboard. 'clip' determines whether the
-- clipboard or the primary selection buffer is used (on X11 and Wayland only.)
local function paste(clip)
    local text = get_clipboard(clip)
    local before_cur = line:sub(1, cursor - 1)
    local after_cur = line:sub(cursor)
    line = before_cur .. text .. after_cur
    cursor = cursor + text:len()
    handle_edit()
end

local function text_input(info)
    if info.key_text and (info.event == "press" or info.event == "down"
                          or info.event == "repeat")
    then
        handle_char_input(info.key_text)
    end
end

local function command_list()
    local commands = {}
    for i, command in ipairs(mp.get_property_native('command-list')) do
        commands[i] = command.name
    end

    return commands
end

local function property_list()
    local properties = mp.get_property_native('property-list')

    for _, sub_property in pairs({'video', 'audio', 'sub', 'sub2'}) do
        properties[#properties + 1] = 'current-tracks/' .. sub_property
    end

    for _, option in ipairs(mp.get_property_native('options')) do
        properties[#properties + 1] = 'options/' .. option
        properties[#properties + 1] = 'file-local-options/' .. option
        properties[#properties + 1] = 'option-info/' .. option

        for _, sub_property in pairs({
            'name', 'type', 'set-from-commandline', 'set-locally',
            'expects-file', 'default-value', 'min', 'max', 'choices',
        }) do
            properties[#properties + 1] = 'option-info/' .. option .. '/' ..
                                          sub_property
        end
    end

    return properties
end

local function profile_list()
    local profiles = {}

    for i, profile in ipairs(mp.get_property_native('profile-list')) do
        profiles[i] = profile.name
    end

    return profiles
end

local function list_option_list()
    local options = {}

    -- Don't log errors for renamed and removed properties.
    -- (Just mp.enable_messages('fatal') still logs them to the terminal.)
    local msg_level_backup = mp.get_property('msg-level')
    mp.set_property('msg-level', msg_level_backup == '' and 'cplayer=no'
                                 or msg_level_backup .. ',cplayer=no')

    for _, option in pairs(mp.get_property_native('options')) do
        if mp.get_property('option-info/' .. option .. '/type', ''):find(' list$') then
            options[#options + 1] = option
        end
    end

    mp.set_property('msg-level', msg_level_backup)

    return options
end

local function list_option_action_list(option)
    local type = mp.get_property('option-info/' .. option .. '/type')

    if type == 'Key/value list' then
        return {'add', 'append', 'set', 'remove'}
    end

    if type == 'String list' or type == 'Object settings list' then
        return {'add', 'append', 'clr', 'pre', 'set', 'remove', 'toggle'}
    end
end

local function list_option_value_list(option)
    local values = mp.get_property_native(option)

    if type(values) ~= 'table' then
        return
    end

    if type(values[1]) ~= 'table' then
        return values
    end

    for i, value in ipairs(values) do
        values[i] = value.label and '@' .. value.label or value.name
    end

    return values
end

local function has_file_argument(candidate_command)
    for _, command in pairs(mp.get_property_native('command-list')) do
        if command.name == candidate_command then
            return command.args[1] and
                   (command.args[1].name == 'filename' or command.args[1].name == 'url')
        end
    end
end

local function file_list(directory)
    if directory == '' then
        directory = '.'
    end

    local files = utils.readdir(directory, 'files') or {}

    for _, dir in pairs(utils.readdir(directory, 'dirs') or {}) do
        files[#files + 1] = dir .. path_separator
    end

    return files
end

local function handle_file_completion(before_cur, path_pos)
    local directory, last_component_pos =
        before_cur:sub(path_pos):match('(.-)()[^' .. path_separator ..']*$')
    completion_pos = path_pos + last_component_pos - 1

    if directory:find('^~' .. path_separator) then
        local home = mp.command_native({'expand-path', '~/'})
        before_cur = before_cur:sub(1, completion_pos - #directory - 1) ..
                     home ..
                     before_cur:sub(completion_pos - #directory + 1)
        directory = home .. directory:sub(2)
        completion_pos = completion_pos + #home - 1
    end

    -- Don't use completion_append for file completion to not add quotes after
    -- directories whose entries you may want to complete afterwards.
    completion_append = ''

    return file_list(directory), before_cur
end

local function handle_choice_completion(option, before_cur, path_pos)
    local info = mp.get_property_native('option-info/' .. option, {})

    if info.type == 'Flag' then
        return { 'no', 'yes' }, before_cur
    end

    if info['expects-file'] then
        return handle_file_completion(before_cur, path_pos)
    end

    -- Fix completing the empty value for --dscale and --cscale.
    if info.choices and info.choices[1] == '' and completion_append == '' then
        info.choices[1] = '""'
    end

    return info.choices, before_cur
end

local function common_prefix_length(s1, s2)
    local common_count = 0
    for i = 1, #s1 do
        if s1:byte(i) ~= s2:byte(i) then
            break
        end
        common_count = common_count + 1
    end
    return common_count
end

local function max_overlap_length(s1, s2)
    for s1_offset = 0, #s1 - 1 do
        local match = true
        for i = 1, #s1 - s1_offset do
            if s1:byte(s1_offset + i) ~= s2:byte(i) then
                match = false
                break
            end
        end
        if match then
            return #s1 - s1_offset
        end
    end
    return 0
end

-- If str starts with the first or last characters of prefix, strip them.
local function strip_common_characters(str, prefix)
    return str:sub(1 + math.max(
    common_prefix_length(prefix, str),
    max_overlap_length(prefix, str)))
end

-- Find the longest common case-sensitive prefix of the entries in "list".
local function find_common_prefix(list)
    local prefix = list[1]

    for i = 2, #list do
        prefix = prefix:sub(1, common_prefix_length(prefix, list[i]))
    end

    return prefix
end

-- Return the entries of "list" beginning with "part" and the longest common
-- prefix of the matches.
local function complete_match(part, list)
    local completions = {}

    for _, candidate in pairs(list) do
        if candidate:sub(1, part:len()) == part then
            completions[#completions + 1] = candidate
        end
    end

    local prefix = find_common_prefix(completions)

    if opts.case_sensitive then
        return completions, prefix or part
    end

    completions = {}
    local lower_case_completions = {}
    local lower_case_part = part:lower()

    for _, candidate in pairs(list) do
        if candidate:sub(1, part:len()):lower() == lower_case_part then
            completions[#completions + 1] = candidate
            lower_case_completions[#lower_case_completions + 1] = candidate:lower()
        end
    end

    local lower_case_prefix = find_common_prefix(lower_case_completions)

    -- Behave like GNU readline with completion-ignore-case On.
    -- part = 'fooBA', completions = {'foobarbaz', 'fooBARqux'} =>
    -- prefix = 'fooBARqux', lower_case_prefix = 'foobar', return 'fooBAR'
    if prefix then
        return completions, prefix:sub(1, lower_case_prefix:len())
    end

    -- part = 'fooba', completions = {'fooBARbaz', 'fooBarqux'} =>
    -- prefix = nil, lower_case_prefix ='foobar', return 'fooBAR'
    if lower_case_prefix then
        return completions, completions[1]:sub(1, lower_case_prefix:len())
    end

    return {}, part
end

local function cycle_through_suggestions(backwards)
    selected_suggestion_index = selected_suggestion_index + (backwards and -1 or 1)

    if selected_suggestion_index > #suggestion_buffer then
        selected_suggestion_index = 1
    elseif selected_suggestion_index < 1 then
        selected_suggestion_index = #suggestion_buffer
    end

    local before_cur = line:sub(1, completion_pos - 1) ..
                       suggestion_buffer[selected_suggestion_index] .. completion_append
    line = before_cur .. strip_common_characters(line:sub(cursor), completion_append)
    cursor = before_cur:len() + 1
    update()
end

-- Complete the option or property at the cursor (TAB)
local function complete(backwards)
    if #suggestion_buffer > 0 then
        cycle_through_suggestions(backwards)
        return
    end

    if input_caller then
        completion_old_line = line
        completion_old_cursor = cursor
        mp.commandv('script-message-to', input_caller, 'input-event',
                    'complete', utils.format_json({line:sub(1, cursor - 1)}))
        return
    end

    local before_cur = line:sub(1, cursor - 1)
    local after_cur = line:sub(cursor)
    local tokens = {}
    local first_useful_token_index = 1
    local completions

    local begin_new_token = true
    local last_quote
    for pos, char in before_cur:gmatch('()(.)') do
        if char:find('[%s;]') and not last_quote then
            begin_new_token = true
            if char == ';' then
                first_useful_token_index = #tokens + 1
            end
        elseif begin_new_token then
            tokens[#tokens + 1] = { text = char, pos = pos }
            last_quote = char:match('["\']')
            begin_new_token = false
        else
            tokens[#tokens].text = tokens[#tokens].text .. char
            if char == last_quote then
                last_quote = nil
            end
        end
    end

    completion_append = last_quote or ''

    -- Strip quotes from tokens.
    for _, token in pairs(tokens) do
        if token.text:find('^"') then
            token.text = token.text:sub(2):gsub('"$', '')
            token.pos = token.pos + 1
        elseif token.text:find("^'") then
            token.text = token.text:sub(2):gsub("'$", '')
            token.pos = token.pos + 1
        end
    end

    -- Skip command prefixes because it is not worth lumping them together with
    -- command completions when they are useless for interactive usage.
    local command_prefixes = {
        ['osd-auto'] = true, ['no-osd'] = true, ['osd-bar'] = true,
        ['osd-msg'] = true, ['osd-msg-bar'] = true, ['raw'] = true,
        ['expand-properties'] = true, ['repeatable'] = true,
        ['nonrepeatable'] = true, ['nonscalable'] = true,
        ['async'] = true, ['sync'] = true
    }

    while tokens[first_useful_token_index] and
          command_prefixes[tokens[first_useful_token_index].text] do
        first_useful_token_index = first_useful_token_index + 1
    end

    -- Add an empty token if the cursor is after whitespace to simplify
    -- comparisons.
    if before_cur == '' or before_cur:find('[%s;]$') then
        tokens[#tokens + 1] = { text = "", pos = cursor }
    end

    local add_actions = {
        ['add'] = true, ['append'] = true, ['pre'] = true, ['set'] = true
    }

    local first_useful_token = tokens[first_useful_token_index]

    completion_pos = before_cur:match('${[=>]?()[%w_/-]*$')
    if completion_pos then
        completions = property_list()
        completion_append = '} '
    elseif #tokens == first_useful_token_index then
        completions = command_list()
        completions[#completions + 1] = 'help'
        completion_pos = first_useful_token.pos
        completion_append = completion_append .. ' '
    elseif #tokens == first_useful_token_index + 1 then
        if first_useful_token.text == 'set' or
           first_useful_token.text == 'add' or
           first_useful_token.text == 'cycle' or
           first_useful_token.text == 'cycle-values' or
           first_useful_token.text == 'multiply' then
            completions = property_list()
            completion_pos = tokens[first_useful_token_index + 1].pos
            completion_append = completion_append .. ' '
        elseif first_useful_token.text == 'help' then
            completions = command_list()
            completion_pos = tokens[first_useful_token_index + 1].pos
        elseif first_useful_token.text == 'apply-profile' then
            completions = profile_list()
            completion_pos = tokens[first_useful_token_index + 1].pos
        elseif first_useful_token.text == 'change-list' then
            completions = list_option_list()
            completion_pos = tokens[first_useful_token_index + 1].pos
            completion_append = completion_append .. ' '
        elseif first_useful_token.text == 'vf' or
               first_useful_token.text == 'af' then
            completions = list_option_action_list(first_useful_token.text)
            completion_pos = tokens[first_useful_token_index + 1].pos
            completion_append = completion_append .. ' '
        elseif has_file_argument(first_useful_token.text) then
            completions, before_cur =
                handle_file_completion(before_cur, tokens[first_useful_token_index + 1].pos)
        end
    elseif first_useful_token.text == 'cycle-values' then
        completion_pos = tokens[#tokens].pos
        completion_append = completion_append .. ' '
        completions, before_cur =
            handle_choice_completion(tokens[first_useful_token_index + 1].text,
                                     before_cur, tokens[#tokens].pos)
    elseif #tokens == first_useful_token_index + 2 then
        if first_useful_token.text == 'set' then
            completion_pos = tokens[#tokens].pos
            completions, before_cur =
                handle_choice_completion(tokens[first_useful_token_index + 1].text,
                                         before_cur,
                                         tokens[first_useful_token_index + 2].pos)
        elseif first_useful_token.text == 'change-list' then
            completions = list_option_action_list(tokens[first_useful_token_index + 1].text)
            completion_pos = tokens[first_useful_token_index + 2].pos
            completion_append = completion_append .. ' '
        elseif first_useful_token.text == 'vf' or
               first_useful_token.text == 'af' then
            if add_actions[tokens[first_useful_token_index + 1].text] then
                completion_pos = tokens[#tokens].pos
                completions, before_cur =
                    handle_choice_completion(first_useful_token.text,
                                             before_cur, tokens[#tokens].pos)
            elseif tokens[first_useful_token_index + 1].text == 'remove' then
                completions = list_option_value_list(first_useful_token.text)
                completion_pos = tokens[#tokens].pos
            end
        end
    elseif #tokens == first_useful_token_index + 3 then
        if first_useful_token.text == 'change-list' then
            if add_actions[tokens[first_useful_token_index + 2].text] then
                completion_pos = tokens[#tokens].pos
                completions, before_cur =
                    handle_choice_completion(tokens[first_useful_token_index + 1].text,
                                             before_cur, tokens[#tokens].pos)
            elseif tokens[first_useful_token_index + 2].text == 'remove' then
                completion_pos = tokens[#tokens].pos
                completions = list_option_value_list(tokens[first_useful_token_index + 1].text)
            end
        elseif first_useful_token.text == 'dump-cache' then
            completions, before_cur =
                handle_file_completion(before_cur,
                                       tokens[first_useful_token_index + 3].pos)
        end
    end

    if completions == nil then
        return
    end

    local prefix
    completions, prefix =
        complete_match(before_cur:sub(completion_pos), completions)

    if #completions == 1 then
        prefix = prefix .. completion_append
        after_cur = strip_common_characters(after_cur, completion_append)
    else
        table.sort(completions)
        suggestion_buffer = completions
        selected_suggestion_index = 0
    end

    before_cur = before_cur:sub(1, completion_pos - 1) .. prefix
    cursor = before_cur:len() + 1
    line = before_cur .. after_cur
    update()
end

-- List of input bindings. This is a weird mashup between common GUI text-input
-- bindings and readline bindings.
local function get_bindings()
    local bindings = {
        { 'esc',         function() set_active(false) end       },
        { 'ctrl+[',      function() set_active(false) end       },
        { 'enter',       handle_enter                           },
        { 'kp_enter',    handle_enter                           },
        { 'shift+enter', function() handle_char_input('\n') end },
        { 'ctrl+j',      handle_enter                           },
        { 'ctrl+m',      handle_enter                           },
        { 'bs',          handle_backspace                       },
        { 'shift+bs',    handle_backspace                       },
        { 'ctrl+h',      handle_backspace                       },
        { 'del',         handle_del                             },
        { 'shift+del',   handle_del                             },
        { 'ins',         handle_ins                             },
        { 'shift+ins',   function() paste(false) end            },
        { 'mbtn_mid',    function() paste(false) end            },
        { 'mbtn_right',  function() set_active(false) end       },
        { 'left',        function() prev_char() end             },
        { 'ctrl+b',      function() page_up_or_prev_char() end  },
        { 'right',       function() next_char() end             },
        { 'ctrl+f',      function() page_down_or_next_char() end},
        { 'up',          function() move_history(-1) end        },
        { 'ctrl+p',      function() move_history(-1) end        },
        { 'wheel_up',    function() move_history(-1, true) end  },
        { 'down',        function() move_history(1) end         },
        { 'ctrl+n',      function() move_history(1) end         },
        { 'wheel_down',  function() move_history(1, true) end   },
        { 'wheel_left',  function() end                         },
        { 'wheel_right', function() end                         },
        { 'ctrl+left',   prev_word                              },
        { 'alt+b',       prev_word                              },
        { 'ctrl+right',  next_word                              },
        { 'alt+f',       next_word                              },
        { 'tab',         complete                               },
        { 'ctrl+i',      complete                               },
        { 'shift+tab',   function() complete(true) end          },
        { 'ctrl+a',      go_home                                },
        { 'home',        go_home                                },
        { 'ctrl+e',      go_end                                 },
        { 'end',         go_end                                 },
        { 'pgup',        handle_pgup                            },
        { 'pgdwn',       handle_pgdown                          },
        { 'ctrl+r',      search_history                         },
        { 'ctrl+c',      clear                                  },
        { 'ctrl+d',      maybe_exit                             },
        { 'ctrl+k',      del_to_eol                             },
        { 'ctrl+l',      clear_log_buffer                       },
        { 'ctrl+u',      del_to_start                           },
        { 'ctrl+v',      function() paste(true) end             },
        { 'meta+v',      function() paste(true) end             },
        { 'ctrl+bs',     del_word                               },
        { 'ctrl+w',      del_word                               },
        { 'ctrl+del',    del_next_word                          },
        { 'alt+d',       del_next_word                          },
        { 'kp_dec',      function() handle_char_input('.') end  },
        { 'kp_add',      function() handle_char_input('+') end  },
        { 'kp_subtract', function() handle_char_input('-') end  },
        { 'kp_multiply', function() handle_char_input('*') end  },
        { 'kp_divide',   function() handle_char_input('/') end  },
    }

    for i = 0, 9 do
        bindings[#bindings + 1] =
            {'kp' .. i, function() handle_char_input('' .. i) end}
    end

    return bindings
end

local function define_key_bindings()
    if #key_bindings > 0 then
        return
    end
    for _, bind in ipairs(get_bindings()) do
        if not (dont_bind_up_down and (bind[1] == 'up' or bind[1] == 'down')) then
            -- Generate arbitrary name for removing the bindings later.
            local name = "_console_" .. (#key_bindings + 1)
            key_bindings[#key_bindings + 1] = name
            mp.add_forced_key_binding(bind[1], name, bind[2], {repeatable = true})
        end
    end
    mp.add_forced_key_binding("any_unicode", "_console_text", text_input,
        {repeatable = true, complex = true})
    key_bindings[#key_bindings + 1] = "_console_text"
end

local function undefine_key_bindings()
    for _, name in ipairs(key_bindings) do
        mp.remove_key_binding(name)
    end
    key_bindings = {}
end

-- Set the REPL visibility ("enable", Esc)
set_active = function (active)
    if active == repl_active then return end
    if active then
        repl_active = true
        insert_mode = false
        define_key_bindings()

        if not input_caller then
            prompt = default_prompt
            id = default_id
            history = histories[id]
            history_pos = #history + 1
            mp.enable_messages('terminal-default')
        end
    elseif searching_history then
        searching_history = false
        line = ''
        cursor = 1
        selectable_items = nil
        log_buffers[id] = {}
        unbind_mouse()
    else
        repl_active = false
        suggestion_buffer = {}
        undefine_key_bindings()
        mp.enable_messages('silent:terminal-default')

        if input_caller then
            mp.commandv('script-message-to', input_caller, 'input-event',
                        'closed', utils.format_json({line, cursor}))
            input_caller = nil
            line = ''
            cursor = 1
            selectable_items = nil
            default_item = nil
            dont_bind_up_down = false
            unbind_mouse()
        end
        collectgarbage()
    end
    update()
end

-- Show the repl if hidden and replace its contents with 'text'
-- (script-message-to repl type)
local function show_and_type(text, cursor_pos)
    text = text or ''
    cursor_pos = tonumber(cursor_pos)

    -- Save the line currently being edited, just in case
    if line ~= text and line ~= '' and history[#history] ~= line then
        history_add(line)
    end

    line = text
    if cursor_pos ~= nil and cursor_pos >= 1
        and cursor_pos <= line:len() + 1 then
        cursor = math.floor(cursor_pos)
    else
        cursor = line:len() + 1
    end
    history_pos = #history + 1
    insert_mode = false
    if repl_active then
        update()
    else
        set_active(true)
    end
end

-- Add a global binding for enabling the REPL. While it's enabled, its bindings
-- will take over and it can be closed with ESC.
mp.add_key_binding(nil, 'enable', function()
    set_active(true)
end)

mp.register_script_message('disable', function()
    set_active(false)
end)

-- Add a script-message to show the REPL and fill it with the provided text
mp.register_script_message('type', function(text, cursor_pos)
    show_and_type(text, cursor_pos)
end)

mp.register_script_message('get-input', function (script_name, args)
    if repl_active then
        return
    end

    input_caller = script_name
    args = utils.parse_json(args)
    prompt = args.prompt or default_prompt
    line = args.default_text or ''
    cursor = args.cursor_position or line:len() + 1
    id = args.id or script_name .. prompt
    dont_bind_up_down = args.dont_bind_up_down
    if histories[id] == nil then
        histories[id] = {}
        log_buffers[id] = {}
    end
    history = histories[id]
    history_pos = #history + 1

    selectable_items = args.items
    if selectable_items then
        matches = {}
        selected_match = args.default_item or 1
        default_item = args.default_item

        local max_lines = calculate_max_log_lines()
        first_match_to_print = math.max(1, selected_match - math.floor(max_lines / 2) + 1)
        if first_match_to_print > #selectable_items - max_lines + 2 then
            first_match_to_print = math.max(1, #selectable_items - max_lines + 1)
        end

        for i, item in ipairs(selectable_items) do
            matches[i] = { index = i, text = item }
        end
        bind_mouse()
    end

    set_active(true)
    mp.commandv('script-message-to', input_caller, 'input-event', 'opened')
end)

mp.register_script_message('log', function (message)
    -- input.get's edited handler is invoked after submit, so avoid modifying
    -- the default log.
    if input_caller == nil then
        return
    end

    message = utils.parse_json(message)

    log_add(message.text,
            message.error and styles.error or message.style,
            message.error and terminal_styles.error or message.terminal_style)
end)

mp.register_script_message('set-log', function (log)
    if input_caller == nil then
        return
    end

    log = utils.parse_json(log)
    log_buffers[id] = {}

    for i = 1, #log do
        if type(log[i]) == 'table' then
            log[i].text = log[i].text
            log[i].style = log[i].style or ''
            log[i].terminal_style = log[i].terminal_style or ''
            log_buffers[id][i] = log[i]
        else
            log_buffers[id][i] = {
                text = log[i],
                style = '',
                terminal_style = '',
            }
        end
    end

    update()
end)

mp.register_script_message('complete', function(list, start_pos)
    if line ~= completion_old_line or cursor ~= completion_old_cursor then
        return
    end

    local completions, prefix = complete_match(line:sub(start_pos, cursor),
                                               utils.parse_json(list))
    local before_cur = line:sub(1, start_pos - 1) .. prefix
    local after_cur = line:sub(cursor)
    cursor = before_cur:len() + 1
    line = before_cur .. after_cur

    if #completions > 1 then
        suggestion_buffer = completions
        selected_suggestion_index = 0
        completion_pos = start_pos
        completion_append = ''
    end

    update()
end)

-- Redraw the REPL when the OSD size changes. This is needed because the
-- PlayRes of the OSD will need to be adjusted.
mp.observe_property('osd-width', 'native', update)
mp.observe_property('osd-height', 'native', update)
mp.observe_property('display-hidpi-scale', 'native', update)
mp.observe_property('focused', 'native', update)

mp.observe_property("user-data/osc/margins", "native", function(_, val)
    if type(val) == "table" and type(val.t) == "number" and type(val.b) == "number" then
        global_margins = val
    else
        global_margins = { t = 0, b = 0 }
    end
    update()
end)

-- Enable log messages. In silent mode, mpv will queue log messages in a buffer
-- until enable_messages is called again without the silent: prefix.
mp.enable_messages('silent:terminal-default')

mp.register_event('log-message', function(e)
    -- Ignore log messages from the OSD because of paranoia, since writing them
    -- to the OSD could generate more messages in an infinite loop.
    if e.prefix:sub(1, 3) == 'osd' then return end

    -- Ignore messages output by this script.
    if e.prefix == mp.get_script_name() then return end

    -- Ignore buffer overflow warning messages. Overflowed log messages would
    -- have been offscreen anyway.
    if e.prefix == 'overflow' then return end

    -- Filter out trace-level log messages, even if the terminal-default log
    -- level includes them. These aren't too useful for an on-screen display
    -- without scrollback and they include messages that are generated from the
    -- OSD display itself.
    if e.level == 'trace' then return end

    -- Use color for debug/v/warn/error/fatal messages.
    log_add('[' .. e.prefix .. '] ' .. e.text:sub(1, -2), styles[e.level],
            terminal_styles[e.level])
end)

collectgarbage()
