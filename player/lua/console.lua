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
    elseif os.getenv('WAYLAND_DISPLAY') or os.getenv('WAYLAND_SOCKET') then
        return 'wayland'
    end
    return 'x11'
end

local platform = detect_platform()

-- Default options
local opts = {
    font = "",
    font_size = 24,
    border_size = 1.65,
    background_alpha = 50,
    padding = 10,
    menu_outline_size = 0,
    menu_outline_color = '#FFFFFF',
    corner_radius = 8,
    margin_x = -1,
    margin_y = -1,
    scale_with_window = "auto",
    selected_color = '#222222',
    selected_back_color = '#FFFFFF',
    case_sensitive = platform ~= 'windows' and true or false,
    history_dedup = true,
    font_hw_ratio = 'auto',
}

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
    completion = '{\\1c&Hcc99cc&}',
}
for key, style in pairs(styles) do
    styles[key] = style .. '{\\3c&H111111&}'
end

local terminal_styles = {
    debug = '\027[90m',
    v = '\027[32m',
    warn = '\027[33m',
    error = '\027[31m',
    fatal = '\027[91m',
    selected_completion = '\027[7m',
    default_item = '\027[1m',
    disabled = '\027[38;5;8m',
}

local repl_active = false
local osd_msg_active = false
local insert_mode = false
local pending_update = false
local ime_active = mp.get_property_bool('input-ime')
local line = ''
local cursor = 1
local default_prompt = '>'
local prompt = default_prompt
local default_id = 'default'
local id = default_id
local histories = {[id] = {}}
local history = histories[id]
local history_pos = 1
local searching_history = false
local log_buffers = {[id] = {}}
local key_bindings = {}
local dont_bind_up_down = false
local overlay = mp.create_osd_overlay('ass-events')
local global_margins = { t = 0, b = 0 }
local input_caller

local completion_buffer = {}
local selected_completion_index
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
local item_positions = {}
local max_item_width = 0

local complete
local cycle_through_completions
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

local function get_margin_x()
    return opts.margin_x > -1 and opts.margin_x or mp.get_property_native('osd-margin-x')
end


local function get_margin_y()
    return opts.margin_y > -1 and opts.margin_y or mp.get_property_native('osd-margin-y')
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

local function terminal_output()
    -- Unlike vo-configured, current-vo doesn't become falsy while switching VO,
    -- which would print the log to the OSD.
    return not mp.get_property('current-vo') or not mp.get_property_native('video-osd')
end

local function get_scaled_osd_dimensions()
    local dims = mp.get_property_native('osd-dimensions')
    local scale = scale_factor()

    return dims.w / scale, dims.h /scale
end

local function get_line_height()
    return selectable_items and opts.font_size * 1.1 or opts.font_size
end

local function calculate_max_log_lines()
    if terminal_output() then
        -- Subtract 1 for the input line and for each line in the status line.
        -- This does not detect wrapped lines.
        return mp.get_property_native('term-size/h', 24) - 2 -
               select(2, mp.get_property('term-status-msg'):gsub('\\n', ''))
    end

    return math.floor((select(2, get_scaled_osd_dimensions())
                       * (1 - global_margins.t - global_margins.b)
                       - get_margin_y() - (selectable_items and opts.padding * 2 or 0))
                      / get_line_height()
                      -- Subtract 1 for the input line and 0.5 for the empty
                      -- line between the log and the input line.
                      - 1.5)
end

local function calculate_max_item_width()
    if not selectable_items or terminal_output() then
        return
    end

    local longest_item = prompt .. ('a'):rep(9)
    for _, item in pairs(selectable_items) do
        if #item > #longest_item then
            longest_item = item
        end
    end

    local osd_w, osd_h = get_scaled_osd_dimensions()
    local font = get_font()
    local width_overlay = mp.create_osd_overlay('ass-events')
    width_overlay.compute_bounds = true
    width_overlay.hidden = true
    width_overlay.res_x = osd_w
    width_overlay.res_y = osd_h
    width_overlay.data = '{\\fs' .. opts.font_size ..
                         (font and '\\fn' .. font or '') .. '\\q2}' ..
                         ass_escape(longest_item)
    local result = width_overlay:update()
    max_item_width = math.min(result.x1 - result.x0,
                              osd_w - get_margin_x() * 2 - opts.padding * 2)
end

local function should_highlight_completion(i)
    return i == selected_completion_index or
           (i == 1 and selected_completion_index == 0 and input_caller == nil)
end

local function mpv_color_to_ass(color)
    return color:sub(8,9) .. color:sub(6,7) ..  color:sub(4,5),
           string.format('%x', 255 - tonumber('0x' .. color:sub(2,3)))
end

local function option_color_to_ass(color)
    return color:sub(6,7) .. color:sub(4,5) ..  color:sub(2,3)
end

local function get_selected_ass()
    local color, alpha = mpv_color_to_ass(mp.get_property('osd-selected-color'))
    local outline_color, outline_alpha =
        mpv_color_to_ass(mp.get_property('osd-selected-outline-color'))
    return '{\\b1\\1c&H' .. color .. '&\\1a&H' .. alpha ..
           '&\\3c&H' .. outline_color .. '&\\3a&H' .. outline_alpha .. '&}'
end

-- Takes a list of strings, a max width in characters and
-- optionally a max row count.
-- The result contains at least one column.
-- Rows are cut off from the top if rows_max is specified.
-- returns a string containing the formatted table and the row count
local function format_grid(list, width_max, rows_max)
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

            if should_highlight_completion(i) then
                columns[column] = get_selected_ass() .. columns[column] ..
                                  '{\\b\\1a&\\3a&}' .. styles.completion
            end
        end
        -- first row is at the bottom
        rows[row_count - row + 1] = table.concat(columns, spacing)
    end
    return table.concat(rows, ass_escape('\n')), row_count
end

local function fuzzy_find(needle, haystacks, case_sensitive)
    local result = require 'mp.fzy'.filter(needle, haystacks, case_sensitive)
    table.sort(result, function (i, j)
        if i[3] ~= j[3] then
            return i[3] > j[3]
        end

        return i[1] < j[1]
    end)
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

    if selected_match < first_match_to_print then
        first_match_to_print = selected_match
    elseif selected_match > first_match_to_print + max_log_lines - 1 then
        first_match_to_print = selected_match - max_log_lines + 1
    end

    local last_match_to_print  = math.min(first_match_to_print + max_log_lines - 1,
                                          #matches)

    for i = first_match_to_print, last_match_to_print do
        local style = ''
        local terminal_style = ''

        if matches[i].index == default_item then
            terminal_style = terminal_styles.default_item
        end
        if i == selected_match then
            if searching_history and
               mp.get_property('osd-border-style') == 'outline-and-shadow' then
                style = get_selected_ass()
            else
                style = '{\\1c&H' .. option_color_to_ass(opts.selected_color) .. '&}'
            end
            terminal_style = terminal_style .. terminal_styles.selected_completion
        end

        log[#log + 1] = {
            text = matches[i].text,
            style = style,
            terminal_style = terminal_style,
        }
    end
end

local function update_overlay(data, res_x, res_y, z)
    if overlay.data == data and
       overlay.res_x == res_x and
       overlay.res_y == res_y and
       overlay.z == z then
        return
    end

    overlay.data = data
    overlay.res_x = res_x
    overlay.res_y = res_y
    overlay.z = z
    overlay:update()
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

    local completions = ''
    for i, completion in ipairs(completion_buffer) do
        if should_highlight_completion(i) then
            completions = completions .. terminal_styles.selected_completion ..
                          completion .. '\027[0m'
        else
            completions = completions .. completion
        end
        completions = completions .. (i < #completion_buffer and '\t' or '\n')
    end

    local counter = ''
    if selectable_items and #selectable_items > calculate_max_log_lines() then
        local digits = math.ceil(math.log(#selectable_items, 10))
        counter = terminal_styles.disabled ..
                  '[' .. string.format('%0' .. digits .. 'd', selected_match) ..
                  '/' .. string.format('%0' .. digits .. 'd', #matches) ..
                  ']\027[0m '
    end

    local before_cur = line:sub(1, cursor - 1)
    local after_cur = line:sub(cursor)
    -- Ensure there is a character with inverted colors to print.
    if after_cur == '' then
        after_cur = ' '
    end

    mp.osd_message(log .. completions .. counter .. prompt .. ' ' .. before_cur
                   .. '\027[7m' .. after_cur:sub(1, 1) .. '\027[0m' ..
                   after_cur:sub(2), 999)
    osd_msg_active = true
end

local function render()
    pending_update = false

    if terminal_output() then
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
        update_overlay('', 0, 0, 0)
        return
    end

    local ass = assdraw.ass_new()
    local osd_w, osd_h = get_scaled_osd_dimensions()
    local line_height = get_line_height()
    local max_lines = calculate_max_log_lines()

    local x, y, alignment, clipping_coordinates
    if selectable_items and not searching_history then
        x = (osd_w - max_item_width) / 2
        y = osd_h / 2 - (math.min(#selectable_items, max_lines) + 1.5) * line_height / 2
        alignment = 7
        clipping_coordinates = '0,0,' .. x + max_item_width .. ',' .. osd_h
    else
        x = get_margin_x()
        y = osd_h * (1 - global_margins.b) - get_margin_y()
        alignment = 1
        -- Avoid drawing below topbar OSC when there are wrapped lines.
        local coordinate_top = math.floor(global_margins.t * osd_h + 0.5)
        clipping_coordinates = '0,' .. coordinate_top .. ',' .. osd_w .. ',' .. osd_h
    end

    local font = get_font()
    -- Use the same blur value as the rest of the OSD. 288 is the OSD's
    -- PlayResY.
    local blur = mp.get_property_native('osd-blur') * osd_h / 288
    local border_style = mp.get_property('osd-border-style')

    local style = '{\\r' ..
                  (font and '\\fn' .. font or '') ..
                  '\\fs' .. opts.font_size ..
                  '\\bord' .. opts.border_size .. '\\fsp0' ..
                  '\\blur' .. blur ..
                  (selectable_items and '\\q2' or '\\q1') ..
                  '\\clip(' .. clipping_coordinates .. ')}'

    -- Create the cursor glyph as an ASS drawing. ASS will draw the cursor
    -- inline with the surrounding text, but it sets the advance to the width
    -- of the drawing. So the cursor doesn't affect layout too much, make it as
    -- thin as possible and make it appear to be 1px wide by giving it 0.5px
    -- horizontal borders.
    local color, alpha = mpv_color_to_ass(mp.get_property('osd-color'))
    local cheight = opts.font_size * 8
    local cglyph = '{\\r\\blur0' ..
                   (mp.get_property_native('focused') == false
                    and '\\alpha&HFF&' or '\\3a&H' .. alpha .. '&') ..
                   '\\3c&H' .. color .. '&' ..
                   '\\xbord0.5\\ybord0\\xshad0\\yshad1\\p4\\pbo24}' ..
                   'm 0 0 l 1 0 l 1 ' .. cheight .. ' l 0 ' .. cheight ..
                   '{\\p0}'
    local before_cur = ass_escape(line:sub(1, cursor - 1))
    local after_cur = ass_escape(line:sub(cursor))

    -- Render log messages as ASS.
    -- This will render at most screeny / font_size - 1 messages.

    local completion_ass = ''
    if next(completion_buffer) then
        -- Estimate how many characters fit in one line
        -- Even with bottom-left anchoring,
        -- libass/ass_render.c:ass_render_event() subtracts --osd-margin-x from
        -- the maximum text width twice.
        -- TODO: --osd-margin-x should scale with osd-width and PlayResX to make
        -- the calculation accurate.
        local width_max = math.floor(
            (osd_w - x - mp.get_property_native('osd-margin-x') * 2)
            / opts.font_size * get_font_hw_ratio())

        local completions, rows = format_grid(completion_buffer, width_max, max_lines)
        max_lines = max_lines - rows
        completion_ass = style .. styles.completion .. completions .. '\\N'
    end

    populate_log_with_matches()

    -- Background
    if selectable_items and
       (not searching_history or border_style == 'background-box') then
        style = style .. '{\\bord0\\blur0\\4a&Hff&}'
        local back_color, back_alpha = mpv_color_to_ass(mp.get_property(
            border_style == 'background-box' and 'osd-back-color' or 'osd-outline-color'))
        if not searching_history then
            back_alpha = string.format('%x', opts.background_alpha)
        end

        ass:new_event()
        ass:an(alignment)
        ass:pos(x, y)
        ass:append('{\\1c&H' .. back_color .. '&\\1a&H' .. back_alpha ..
                   '&\\bord' .. opts.menu_outline_size .. '\\3c&H' ..
                   option_color_to_ass(opts.menu_outline_color) .. '&}')
        if border_style == 'background-box' then
            ass:append('{\\4a&Hff&}')
        end
        ass:draw_start()
        ass:round_rect_cw(-opts.padding,
                          opts.padding * (alignment == 7 and -1 or 1),
                          max_item_width + opts.padding,
                          (1.5 + math.min(#matches, max_lines)) * line_height +
                          opts.padding * (alignment == 7 and 1 or 2),
                          opts.corner_radius, opts.corner_radius)
        ass:draw_stop()
    end

    local log_ass = ''
    local log_buffer = log_buffers[id]
    item_positions = {}

    for i = #log_buffer - math.min(max_lines, #log_buffer) + 1, #log_buffer do
        local log_item = style .. log_buffer[i].style .. ass_escape(log_buffer[i].text)

        if selectable_items then
            local item_y = alignment == 7
                and y + (1 + i) * line_height
                or y - (1.5 + #log_buffer - i) * line_height

            if (first_match_to_print - 1 + i == selected_match or
                matches[first_match_to_print - 1 + i].index == default_item)
               and (not searching_history or border_style == 'background-box') then
                ass:new_event()
                ass:an(4)
                ass:pos(x, item_y)
                ass:append('{\\blur0\\bord0\\4aH&ff&\\1c&H' ..
                           option_color_to_ass(opts.selected_back_color) .. '&}')
                if first_match_to_print - 1 + i ~= selected_match then
                    ass:append('{\\1aH&dd&}')
                end
                ass:draw_start()
                ass:rect_cw(-opts.padding, 0, max_item_width + opts.padding, line_height)
                ass:draw_stop()
            end

            ass:new_event()
            ass:an(4)
            ass:pos(x, item_y)
            ass:append(log_item)

            item_positions[#item_positions + 1] =
                { item_y - line_height / 2, item_y + line_height / 2 }
        else
            log_ass = log_ass .. log_item .. '\\N'
        end
    end

    -- Scrollbar
    if selectable_items and #matches > max_lines then
        ass:new_event()
        ass:an(alignment + 2)
        ass:pos(x + max_item_width, y)
        ass:append('{\\bord0\\4a&Hff&\\blur0}' .. selected_match .. '/' .. #matches)

        local start_percentage = (first_match_to_print - 1) / #matches
        local end_percentage = (first_match_to_print - 1 + max_lines) / #matches
        if end_percentage - start_percentage < 0.04 then
            local diff = 0.04 - (end_percentage - start_percentage)
            start_percentage = start_percentage * (1 - diff)
            end_percentage = end_percentage + diff * (1 - end_percentage)
        end

        local max_height = max_lines * line_height
        local bar_y = alignment == 7
                      and y + 1.5 * line_height + start_percentage * max_height
                      or y - 1.5 * line_height - max_height * (1 - end_percentage)
       local height = max_height * (end_percentage - start_percentage)

        ass:new_event()
        ass:an(alignment)
        ass:append('{\\blur0\\4a&Hff&\\bord1}')
        ass:pos(x + max_item_width + opts.padding - 1, bar_y)
        ass:draw_start()
        ass:rect_cw(0, 0, -opts.padding / 2, height)
        ass:draw_stop()
    end

    ass:new_event()
    ass:an(alignment)
    ass:pos(x, y)
    if not selectable_items then
        ass:append(log_ass .. '\\N' .. completion_ass)
    end
    ass:append(style .. ass_escape(prompt) .. ' ' .. before_cur)
    ass:append(cglyph)
    ass:append(style .. after_cur)

    -- Redraw the cursor with the REPL text invisible. This will make the
    -- cursor appear in front of the text.
    ass:new_event()
    ass:an(alignment)
    ass:pos(x, y)
    ass:append(style .. '{\\alpha&HFF&}' .. ass_escape(prompt) .. ' ' .. before_cur)
    ass:append(cglyph)
    ass:append(style .. '{\\alpha&HFF&}' .. after_cur)

    -- z with selectable_items needs to be greater than the OSC's.
    update_overlay(ass.text, osd_w, osd_h, selectable_items and 2000 or 0)
end

local update_timer = nil
update_timer = mp.add_periodic_timer(0.05, function()
    if pending_update then
        render()
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
            render()
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

local function handle_cursor_move()
    -- Don't show completions after a command is entered because they move its
    -- output up, and allow clearing completions by emptying the line.
    if line == '' then
        completion_buffer = {}
        render()
    else
        complete()
    end
end

local function handle_edit()
    if selectable_items then
        matches = {}
        for i, match in ipairs(fuzzy_find(line, selectable_items)) do
            matches[i] = { index = match, text = selectable_items[match] }
        end

        if line == '' and default_item then
            selected_match = default_item

            local max_lines = calculate_max_log_lines()
            first_match_to_print = math.max(1, selected_match + 1 - math.ceil(max_lines / 2))
            if first_match_to_print > #selectable_items - max_lines + 1 then
                first_match_to_print = math.max(1, #selectable_items - max_lines + 1)
            end
        else
            selected_match = 1
        end

        render()

        return
    end

    handle_cursor_move()

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
    handle_cursor_move()
end

-- Move the cursor to the previous character (Left)
local function prev_char()
    cursor = prev_utf8(line, cursor)
    handle_cursor_move()
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
        handle_edit()
        unbind_mouse()
        return
    end

    if line == '' and input_caller == nil then
        return
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
        if selected_completion_index == 0 then
            cycle_through_completions()
        end

        -- match "help [<text>]", return <text> or "", strip all whitespace
        local help = line:match('^%s*help%s+(.-)%s*$') or
                     (line:match('^%s*help$') and '')
        if help then
            help_command(help)
        else
            mp.command(line)
        end
    end

    if history[#history] ~= line and line ~= '' then
        history_add(line)
    end

    clear()
end

local function determine_hovered_item()
    local osd_w, _ = get_scaled_osd_dimensions()
    local scale = scale_factor()
    local mouse_pos = mp.get_property_native('mouse-pos')
    local mouse_x = mouse_pos.x / scale
    local mouse_y = mouse_pos.y / scale
    local item_x0 = (searching_history and get_margin_x() or (osd_w - max_item_width) / 2)
                    - opts.padding

    if mouse_x < item_x0 or mouse_x > item_x0 + max_item_width + opts.padding * 2 then
        return
    end

    for i, positions in ipairs(item_positions) do
        if mouse_y >= positions[1] and mouse_y <= positions[2] then
            return first_match_to_print - 1 + i
        end
    end
end

local function bind_mouse()
    mp.add_forced_key_binding('MOUSE_MOVE', '_console_mouse_move', function()
        local item = determine_hovered_item()
        if item and item ~= selected_match then
            selected_match = item
            render()
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
    handle_edit()
end

-- Go to the specified relative position in the command history (Up, Down)
local function move_history(amount, is_wheel)
    if is_wheel and selectable_items then
        local max_lines = calculate_max_log_lines()

        -- Update selected_match only if it's the first or last printed item and
        -- there are hidden items.
        if (amount > 0 and selected_match == first_match_to_print
            and first_match_to_print - 1 + max_lines < #matches)
           or (amount < 0 and selected_match == first_match_to_print - 1 + max_lines
               and first_match_to_print > 1) then
            selected_match = selected_match + amount
        end

        if amount > 0 and first_match_to_print < #matches - max_lines + 1
           or amount < 0 and first_match_to_print > 1 then
           -- math.min and math.max would only be needed with amounts other than
           -- 1 and -1.
            first_match_to_print = math.min(
                math.max(first_match_to_print + amount, 1), #matches - max_lines + 1)
        end

        local item = determine_hovered_item()
        if item then
            selected_match = item
        end

        render()
        return
    end

    if selectable_items then
        selected_match = selected_match + amount
        if selected_match > #matches then
            selected_match = 1
        elseif selected_match < 1 then
            selected_match = #matches
        end
        render()
        return
    end

    go_history(history_pos + amount)
end

-- Go to the first command in the command history (PgUp)
local function handle_pgup()
    if selectable_items then
        selected_match = math.max(selected_match - calculate_max_log_lines() + 1, 1)
        render()
        return
    end

    go_history(1)
end

-- Stop browsing history and start editing a blank line (PgDown)
local function handle_pgdown()
    if selectable_items then
        selected_match = math.min(selected_match + calculate_max_log_lines() - 1, #matches)
        render()
        return
    end

    go_history(#history + 1)
end

local function search_history()
    if selectable_items or #history == 0 then
        return
    end

    searching_history = true
    completion_buffer = {}
    selectable_items = {}

    for i = 1, #history do
        selectable_items[i] = history[#history + 1 - i]
    end

    calculate_max_item_width()
    handle_edit()
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
    handle_cursor_move()
end

-- Move to the end of the current word, or if already at the end, the end of
-- the next word. (Ctrl+Right)
local function next_word()
    cursor = select(2, line:find('%s*[^%s]*', cursor)) + 1
    handle_cursor_move()
end

-- Move the cursor to the beginning of the line (HOME)
local function go_home()
    cursor = 1
    handle_cursor_move()
end

-- Move the cursor to the end of the line (END)
local function go_end()
    cursor = line:len() + 1
    handle_cursor_move()
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
    render()
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
        if mp.get_property('current-clipboard-backend') == 'wayland' then
            local property = clip and 'clipboard/text' or 'clipboard/text-primary'
            return mp.get_property(property, '')
        end
        -- Wayland VO clipboard is only updated on window focus
        if clip and mp.get_property_native('focused') then
            return mp.get_property('clipboard/text', '')
        end
        local res = utils.subprocess({
            args = { 'wl-paste', clip and '-n' or  '-np' },
            playback_only = false,
        })
        if not res.error then
            return res.stdout
        end
    elseif platform == 'windows' or platform == 'darwin' then
        return mp.get_property('clipboard/text', '')
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

    for _, sub_property in pairs({'text', 'text-primary'}) do
        properties[#properties + 1] = 'clipboard/' .. sub_property
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
    else
        directory = mp.command_native({'expand-path', directory})
    end

    local files = utils.readdir(directory, 'files') or {}

    for _, dir in pairs(utils.readdir(directory, 'dirs') or {}) do
        files[#files + 1] = dir .. path_separator
    end

    return files
end

local function handle_file_completion(before_cur)
    local directory, last_component_pos =
        before_cur:sub(completion_pos):match('(.-)()[^' .. path_separator ..']*$')

    completion_pos = completion_pos + last_component_pos - 1

    -- Don't use completion_append for file completion to not add quotes after
    -- directories whose entries you may want to complete afterwards.
    completion_append = ''

    return file_list(directory)
end

local function handle_choice_completion(option, before_cur)
    local info = mp.get_property_native('option-info/' .. option, {})

    if info.type == 'Flag' then
        return { 'no', 'yes' }, before_cur
    end

    if info['expects-file'] then
        return handle_file_completion(before_cur)
    end

    -- Fix completing the empty value for --dscale and --cscale.
    if info.choices and info.choices[1] == '' and completion_append == '' then
        info.choices[1] = '""'
    end

    return info.choices
end

local function command_flags_at_1st_argument_list(command)
    local flags = {
        ['playlist-next'] = {'weak', 'force'},
        ['playlist-play-index'] = {'current', 'none'},
        ['playlist-remove'] = {'current'},
        ['rescan-external-files'] = {'reselect', 'keep-selection'},
        ['revert-seek'] = {'mark', 'mark-permanent'},
        ['screenshot'] = {'subtitles', 'video', 'window', 'each-frame'},
        ['stop'] = {'keep-playlist'},
    }
    flags['playlist-prev'] = flags['playlist-next']
    flags['screenshot-raw'] = flags.screenshot

    return flags[command]
end

local function command_flags_at_2nd_argument_list(command)
    local flags = {
        ['apply-profile'] = {'default', 'restore'},
        ['loadfile'] = {'replace', 'append', 'append-play', 'insert-next',
                        'insert-next-play', 'insert-at', 'insert-at-play'},
        ['screenshot-to-file'] = {'subtitles', 'video', 'window', 'each-frame'},
        ['screenshot-raw'] = {'bgr0', 'bgra', 'rgba', 'rgba64'},
        ['seek'] = {'relative', 'absolute', 'absolute-percent',
                    'relative-percent', 'keyframes', 'exact'},
        ['sub-add'] = {'select', 'auto', 'cached'},
        ['sub-seek'] = {'primary', 'secondary'},
    }
    flags.loadlist = flags.loadfile
    flags['audio-add'] = flags['sub-add']
    flags['video-add'] = flags['sub-add']
    flags['sub-step'] = flags['sub-seek']

    return flags[command]
end

local function list_executables()
    local executable_map = {}
    local path = os.getenv('PATH') or ''
    local separator = platform == 'windows' and ';' or ':'
    local exts = {}

    for ext in (os.getenv('PATHEXT') or ''):gmatch('[^;]+') do
        exts[ext:lower()] = true
    end

    for directory in path:gmatch('[^' .. separator .. ']+') do
        for _, executable in pairs(utils.readdir(directory, 'files') or {}) do
            if not next(exts) or exts[(executable:match('%.%w+$') or ''):lower()] then
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

local function list_filter_labels(type)
    local values = {'all'}

    for _, value in pairs(mp.get_property_native(type)) do
        if value.label then
            values[#values + 1] = value.label
        end
    end

    return values
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

cycle_through_completions = function (backwards)
    if #completion_buffer == 0 then
        -- Allow Tab completion of commands before typing anything.
        if line == '' then
            complete()
        end

        return
    end

    selected_completion_index = selected_completion_index + (backwards and -1 or 1)

    if selected_completion_index > #completion_buffer then
        selected_completion_index = 1
    elseif selected_completion_index < 1 then
        selected_completion_index = #completion_buffer
    end

    local before_cur = line:sub(1, completion_pos - 1) ..
                       completion_buffer[selected_completion_index] .. completion_append
    line = before_cur .. strip_common_characters(line:sub(cursor),
        completion_buffer[selected_completion_index] .. completion_append)
    cursor = before_cur:len() + 1
    render()
end

-- Show autocompletions.
complete = function ()
    if input_caller then
        completion_old_line = line
        completion_old_cursor = cursor
        mp.commandv('script-message-to', input_caller, 'input-event',
                    'complete', utils.format_json({line:sub(1, cursor - 1)}))
        render()
        return
    end

    local before_cur = line:sub(1, cursor - 1)
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

    -- Add an empty token if the cursor is after whitespace or ; to simplify
    -- comparisons.
    if before_cur == '' or before_cur:find('[%s;]$') then
        tokens[#tokens + 1] = { text = "", pos = cursor }
    end

    while tokens[first_useful_token_index] and
          command_prefixes[tokens[first_useful_token_index].text] do
        if first_useful_token_index == #tokens then
            completion_buffer = {}
            render()
            return
        end

        first_useful_token_index = first_useful_token_index + 1
    end

    completion_pos = tokens[#tokens].pos

    local add_actions = {
        ['add'] = true, ['append'] = true, ['pre'] = true, ['set'] = true
    }

    local first_useful_token = tokens[first_useful_token_index]

    local property_pos = before_cur:match('${[=>]?()[%w_/-]*$')
    if property_pos then
        completion_pos = property_pos
        completions = property_list()
        completion_append = '}'
    elseif #tokens == first_useful_token_index then
        completions = command_list()
        completions[#completions + 1] = 'help'
    elseif #tokens == first_useful_token_index + 1 then
        if first_useful_token.text == 'set' or
           first_useful_token.text == 'add' or
           first_useful_token.text == 'cycle' or
           first_useful_token.text == 'cycle-values' or
           first_useful_token.text == 'multiply' then
            completions = property_list()
        elseif first_useful_token.text == 'help' then
            completions = command_list()
        elseif first_useful_token.text == 'apply-profile' then
            completions = profile_list()
        elseif first_useful_token.text == 'change-list' then
            completions = list_option_list()
        elseif first_useful_token.text == 'run' then
            completions = list_executables()
        elseif first_useful_token.text == 'vf' or
               first_useful_token.text == 'af' then
            completions = list_option_action_list(first_useful_token.text)
        elseif first_useful_token.text == 'vf-command' or
               first_useful_token.text == 'af-command' then
            completions = list_filter_labels(first_useful_token.text:sub(1,2))
        elseif has_file_argument(first_useful_token.text) then
            completions = handle_file_completion(before_cur)
        else
            completions = command_flags_at_1st_argument_list(first_useful_token.text)
        end
    elseif first_useful_token.text == 'cycle-values' then
        completions = handle_choice_completion(tokens[first_useful_token_index + 1].text,
                                               before_cur)
    elseif #tokens == first_useful_token_index + 2 then
        if first_useful_token.text == 'set' then
            completions = handle_choice_completion(tokens[first_useful_token_index + 1].text,
                                                   before_cur)
        elseif first_useful_token.text == 'change-list' then
            completions = list_option_action_list(tokens[first_useful_token_index + 1].text)
        elseif first_useful_token.text == 'vf' or
               first_useful_token.text == 'af' then
            if add_actions[tokens[first_useful_token_index + 1].text] then
                completions = handle_choice_completion(first_useful_token.text, before_cur)
            elseif tokens[first_useful_token_index + 1].text == 'remove' then
                completions = list_option_value_list(first_useful_token.text)
            end
        else
            completions = command_flags_at_2nd_argument_list(first_useful_token.text)
        end
    elseif #tokens == first_useful_token_index + 3 then
        if first_useful_token.text == 'change-list' then
            if add_actions[tokens[first_useful_token_index + 2].text] then
                completions = handle_choice_completion(tokens[first_useful_token_index + 1].text,
                                                       before_cur)
            elseif tokens[first_useful_token_index + 2].text == 'remove' then
                completions = list_option_value_list(tokens[first_useful_token_index + 1].text)
            end
        elseif first_useful_token.text == 'dump-cache' then
            completions = handle_file_completion(before_cur)
        end
    end

    completion_buffer = {}
    selected_completion_index = 0
    completions = completions or {}
    table.sort(completions)
    completion_pos = completion_pos or 1
    for i, match in ipairs(fuzzy_find(before_cur:sub(completion_pos),
                                      completions, opts.case_sensitive)) do
        completion_buffer[i] = completions[match]
    end

    render()
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
        { 'tab',         cycle_through_completions              },
        { 'ctrl+i',      cycle_through_completions              },
        { 'shift+tab',   function() cycle_through_completions(true) end },
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
        mp.set_property_bool('user-data/mpv/console/open', true)
        ime_active = mp.get_property_bool('input-ime')
        mp.set_property_bool('input-ime', true)

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
        completion_buffer = {}
        undefine_key_bindings()
        mp.enable_messages('silent:terminal-default')
        mp.set_property_bool('user-data/mpv/console/open', false)
        mp.set_property_bool('input-ime', ime_active)

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
    render()
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
        render()
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
    if repl_active and input_caller and script_name ~= input_caller then
        mp.commandv('script-message-to', input_caller, 'input-event',
                    'closed', utils.format_json({line, cursor}))
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
    searching_history = false

    if args.items then
        selectable_items = {}
        for i, item in ipairs(args.items) do
            selectable_items[i] = item:gsub("[\r\n].*", "⋯"):sub(1, 300)
        end
        default_item = args.default_item
        calculate_max_item_width()
        handle_edit()
        bind_mouse()
    else
        selectable_items = nil
        unbind_mouse()
    end

    set_active(true)
    mp.commandv('script-message-to', input_caller, 'input-event', 'opened')
end)

mp.register_script_message('log', function (message)
    -- input.get edited handler is invoked after submit, so avoid modifying
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

    render()
end)

mp.register_script_message('complete', function(list, start_pos)
    if line ~= completion_old_line or cursor ~= completion_old_cursor then
        return
    end

    completion_buffer = {}
    selected_completion_index = 0
    local completions = utils.parse_json(list)
    table.sort(completions)
    completion_pos = start_pos
    completion_append = ''
    for i, match in ipairs(fuzzy_find(line:sub(completion_pos, cursor),
                                      completions)) do
        completion_buffer[i] = completions[match]
    end

    render()
end)

for _, property in pairs({'osd-width', 'osd-height', 'display-hidpi-scale'}) do
    mp.observe_property(property, 'native', function ()
        calculate_max_item_width()
        render()
    end)
end
mp.observe_property('focused', 'native', render)

mp.observe_property("user-data/osc/margins", "native", function(_, val)
    if type(val) == "table" and type(val.t) == "number" and type(val.b) == "number" then
        global_margins = val
    else
        global_margins = { t = 0, b = 0 }
    end
    render()
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

mp.register_event('shutdown', function ()
    mp.del_property('user-data/mpv/console')
end)

require 'mp.options'.read_options(opts, nil, render)

collectgarbage()
