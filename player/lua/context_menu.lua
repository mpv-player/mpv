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

local assdraw = require "mp.assdraw"

local options = {
    font_size = 14,
    gap = 3,
    padding_x = 8,
    padding_y = 4,
    menu_outline_size = 0,
    menu_outline_color = "#FFFFFF",
    corner_radius = 5,
    scale_with_window = "auto",
    focused_color = "#222222",
    focused_back_color = "#FFFFFF",
    disabled_color = "#555555",
    seconds_to_open_submenus = 0.2,
    seconds_to_close_submenus = 0.2,
}

local open_menus
local items
local focused_level = 1
local focused_index
local open_submenu_timer
local close_submenu_timer
local overlay = mp.create_osd_overlay("ass-events")
local width_overlay = mp.create_osd_overlay("ass-events")
width_overlay.compute_bounds = true
width_overlay.hidden = true

local close

local function escape(str)
    return mp.command_native({"escape-ass", str})
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

local function clear_timers()
    if open_submenu_timer then
        open_submenu_timer:kill()
        open_submenu_timer = nil
    end

    if close_submenu_timer then
        close_submenu_timer:kill()
        close_submenu_timer = nil
    end
end

local function should_scale()
    return options.scale_with_window == "yes" or
           (options.scale_with_window == "auto" and mp.get_property_native("osd-scale-by-window"))
end

local function scale_factor()
    local osd_height = mp.get_property_native("osd-height")

    if should_scale() and osd_height > 0 then
        return osd_height / 720
    end

    return mp.get_property_native("display-hidpi-scale", 1)
end

local function get_scaled_osd_dimensions()
    local dims = mp.get_property_native("osd-dimensions")
    local scale = scale_factor()

    return dims.w / scale, dims.h / scale
end

local function get_scaled_mouse_pos()
    local pos = mp.get_property_native("mouse-pos")
    local scale = scale_factor()

    return pos.x / scale, pos.y / scale, pos.hover
end

local function has_state(item, state)
    for _, value in pairs(item.state or {}) do
        if value == state then
            return true
        end
    end
end

local function has_checkbox(menu_items)
    for _, item in pairs(menu_items) do
        if has_state(item, "checked") then
            return true
        end
    end
end

local function get_right_aligned_text(item)
    return (item.shortcut or "") ..
           (item.shortcut and item.submenu and " " or "") ..
           (item.submenu and "▸" or "")
end

local function calculate_width(menu_items, osd_w, osd_h, checkbox)
    local titles = {}
    for _, item in pairs(menu_items) do
        if item.title then
            local right_text = get_right_aligned_text(item)
            titles[#titles + 1] = item.title ..
                                  (right_text ~= "" and "    " or "") .. right_text
        end
    end

    local longest = ""
    for _, title in pairs(titles) do
        if #title > #longest then
            longest = title
        end
    end

    if checkbox then
        longest = "✔ " .. longest
    end

    for _, item in ipairs(menu_items) do
        if has_state(item, "checked") then
            longest = "✔ " .. longest
            break
        end
    end

    width_overlay.res_x = osd_w
    width_overlay.res_y = osd_h
    width_overlay.data = "{\\fs" .. options.font_size .. "\\q2}" ..
                         escape(longest)
    local result = width_overlay:update()

    return math.min(result.x1 - result.x0, osd_w * 0.95)
end

local function get_line_height()
    return options.font_size + options.gap
end

local function calculate_height(menu_items)
    local item_count = 0
    for _, item in pairs(menu_items) do
        if item.type ~= "separator" then
            item_count = item_count + 1
        end
    end

    return item_count * get_line_height() + options.padding_y * 2
end

local function add_menu(menu_items, x, y)
    if not menu_items[1] then
        return
    end

    local visible_items = {}
    for _, item in ipairs(menu_items) do
        if not has_state(item, "hidden") then
            visible_items[#visible_items + 1] = item
        end
    end

    local checkbox = has_checkbox(visible_items)
    local osd_w, osd_h = get_scaled_osd_dimensions()
    local width = calculate_width(visible_items, osd_w, osd_h, checkbox) +
                  options.padding_x * 2
    local height = math.min(calculate_height(visible_items), osd_h)
    local last_menu = open_menus[#open_menus]

    if x + width > osd_w then
        x = math.max(osd_w - width, 0)

        -- If menus overlap
        if last_menu and last_menu.x <= x + width
           and x <= last_menu.x + last_menu.width then

            x = math.max(last_menu.x - width, 0)
        end
    end

    if y + height > osd_h then
        y = math.max(osd_h - height, 0)
    end

    open_menus[#open_menus + 1] = {
        items = visible_items,
        x = x,
        y = y,
        width = width,
        height = height,
        has_checkbox = checkbox,
        page = 1,
    }
end

local function mpv_color_to_ass(color)
    return color:sub(8,9) .. color:sub(6,7) ..  color:sub(4,5)
end

local function color_option_to_ass(color)
    return color:sub(6,7) .. color:sub(4,5) ..  color:sub(2,3)
end

local function append_item(ass, menu, level, style, item, item_y,
                           non_separator_item_index, line_height)
    local focused = (non_separator_item_index == focused_index and level == focused_level) or
                    (non_separator_item_index == menu.index_with_open_submenu
                     and (level < focused_level or not focused_index))

    if focused and not has_state(item, "disabled") then
        ass:new_event()
        ass:an(4)
        ass:pos(menu.x, item_y)
        ass:append("{\\blur0\\bord0\\4aH&ff&\\1c&H" ..
                    color_option_to_ass(options.focused_back_color) .. "&}")
        ass:draw_start()
        ass:rect_cw(0, 0, menu.width, line_height)
        ass:draw_stop()
    end

    ass:new_event()
    if item.page_offset then
        ass:an(5)
        ass:pos(menu.x + options.padding_x + menu.width / 2, item_y)
    else
        ass:an(4)
        ass:pos(menu.x + options.padding_x, item_y)
    end
    ass:append(style .. "{\\clip(0,0," .. menu.x + menu.width .. ",99999)}")

    if has_state(item, "disabled") then
        ass:append("{\\1c&H" ..
                    color_option_to_ass(options.disabled_color) .. "&}")
    elseif focused then
        ass:append("{\\1c&H" ..
                    color_option_to_ass(options.focused_color) .. "&}")
    end

    if has_state(item, "checked") then
        ass:append("✔ ")
    elseif menu.has_checkbox then
        ass:append("{\\1a&HFF&}✔ {\\1a&}")
    end

    ass:append(escape(item.title))

    if item.submenu or item.shortcut then
        ass:new_event()
        ass:an(6)
        ass:pos(menu.x + menu.width - options.padding_x, item_y)
        ass:append(style)

        if has_state(item, "disabled") then
            ass:append("{\\1c&H" ..
                    color_option_to_ass(options.disabled_color) .. "&}")
        elseif focused then
            ass:append("{\\1c&H" ..
                    color_option_to_ass(options.focused_color) .. "&}")
        end

        ass:append(escape(get_right_aligned_text(item)))
    end

    items[level][non_separator_item_index] = {
        data = item,
        x0 = menu.x,
        x1 = menu.x + menu.width,
        y0 = item_y - line_height / 2,
        y1 = item_y + line_height / 2,
    }
end

local function add_submenu(ass, menu, level, style, background_style)
    ass:new_event()
    ass:an(7)
    ass:pos(menu.x, menu.y)
    ass:append(background_style)
    ass:draw_start()
    ass:round_rect_cw(0, 0, menu.width, menu.height, options.corner_radius,
                      options.corner_radius)
    ass:draw_stop()

    local line_height = get_line_height()

    items[level] = {}
    local first = 1
    local non_separator_item_index = 1

    if menu.page > 1 then
        local item_y = menu.y + options.padding_y + 0.5 * line_height
        append_item(ass, menu, level, style, { title = '▴', page_offset = -1 },
                    item_y, non_separator_item_index, line_height)
        non_separator_item_index = 2

        local i = 1
        local per_page = (menu.height - options.padding_y * 2) / get_line_height() - 2
        for _, item in ipairs(menu.items) do
            if item.type ~= "separator" then
                if i < (menu.page - 1) * per_page then
                    first = first + 1
                else
                    break
                end
            end
            i = i + 1
        end
    end

    for i = first, #menu.items do
        local item = menu.items[i]
        local item_y = menu.y + options.padding_y + (non_separator_item_index - 0.5) * line_height

        if item.type == "separator" then
            ass:new_event()
            ass:an(7)
            ass:pos(menu.x, item_y - line_height / 2)
            ass:append(style)
            ass:draw_start()
            ass:rect_cw(0, -1, menu.width, 0)
            ass:draw_stop()
        else
            local add_down_arrow = i < #menu.items and item_y + line_height > menu.y + menu.height
            if add_down_arrow then
                item = { title = '▾', page_offset = 1 }
            end

            append_item(ass, menu, level, style, item, item_y,
                        non_separator_item_index, line_height)

            if add_down_arrow then
                return
            end

            non_separator_item_index = non_separator_item_index + 1
        end
    end
end

local function render()
    local ass = assdraw.ass_new()
    local osd_w, osd_h = get_scaled_osd_dimensions()
    local border_style = mp.get_property("osd-border-style")

    local style = "{\\fs" .. options.font_size .. "\\bord0\\4a&Hff&\\blur0\\q2}"

    local back_color = mpv_color_to_ass(mp.get_property(
        border_style == "background-box" and "osd-back-color" or "osd-outline-color"))

    -- Don't make the background pure black in the default configuration
    -- because it causes eye strain. It is fine for other UI elements because
    -- they have background transparency, but the context menu doesn't.
    if back_color == "000000" then
        back_color = "222222"
    end

    local background_style = "{\\1c&H" .. back_color .. "&" ..
                       "&\\bord" .. options.menu_outline_size .. "\\3c&H" ..
                       color_option_to_ass(options.menu_outline_color) .. "&}"
    if border_style == "background-box" then
        background_style = background_style .. "{\\4a&Hff&}"
    end

    items = {}

    for i, open_menu in ipairs(open_menus) do
        add_submenu(ass, open_menu, i, style, background_style)
    end

    update_overlay(ass.text, osd_w, osd_h, 3000)
end

local function determine_hovered_item()
    local x, y = get_scaled_mouse_pos()

    for level = #items, 1, -1 do
        for i, item in ipairs(items[level]) do
            if x >= item.x0 and x <= item.x1 and
               y >= item.y0 and y <= item.y1 then
                return level, i
            end
        end
    end

    return 1
end

local function open_submenu(update_focus)
    local item = items[focused_level][focused_index]

    if not item or not item.data.submenu or not item.data.submenu[1] then
        return
    end

    clear_timers()

    for i = focused_level + 1, #open_menus do
        open_menus[i] = nil
    end

    open_menus[#open_menus].index_with_open_submenu = focused_index
    add_menu(item.data.submenu, item.x1, item.y0 - options.padding_y)

    if update_focus then
        focused_level = focused_level + 1
        focused_index = 0
        repeat
            focused_index = focused_index + 1
            local new_item = item.data.submenu[focused_index]
        until not new_item or not has_state(new_item, "disabled")
    end

    render()
end

local function handle_mouse_move()
    local level, index = determine_hovered_item()

    if level == focused_level and index == focused_index then
        return
    end

    focused_level = level
    focused_index = index
    local item = items[level][index]

    render()

    if open_submenu_timer then
        open_submenu_timer:kill()
        open_submenu_timer = nil
    end

    if item and item.data.submenu then
        open_submenu_timer = mp.add_timeout(options.seconds_to_open_submenus, function ()
            open_submenu()
        end)
    end

    if item and level < #open_menus and not close_submenu_timer and
       (item and item.submenu) ~= open_menus[#open_menus].items then
        close_submenu_timer = mp.add_timeout(options.seconds_to_close_submenus, function ()
            for i = level + 1, #open_menus do
                open_menus[i] = nil
            end

            open_menus[#open_menus].index_with_open_submenu = nil
            close_submenu_timer = nil
            render()
        end)
    elseif close_submenu_timer and level == #open_menus then
        close_submenu_timer:kill()
        close_submenu_timer = nil
    end
end

local function move_focus(offset)
    local item

    repeat
        focused_index = (focused_index or 0) + offset
        item = items[focused_level][focused_index]
    until not item or not has_state(item.data, "disabled")

    if item then
        render()
        return
    end

    focused_index = offset > 0 and 0 or #items[focused_level] + 1

    repeat
        focused_index = focused_index + offset
        item = items[focused_level][focused_index]
    until not item or not has_state(item.data, "disabled")

    render()
end

local function focus_first()
    focused_index = 0
    move_focus(1)
end

local function focus_last()
    focused_index = #items[focused_level] + 1
    move_focus(-1)
end

local function close_submenu()
    if focused_level == 1 then
        return
    end

    open_menus[#open_menus] = nil
    focused_level = focused_level - 1
    focused_index = open_menus[#open_menus].index_with_open_submenu
    open_menus[#open_menus].index_with_open_submenu = nil

    render()
end

local function activate_focused_item(update_focus)
    local item = items[focused_level][focused_index]

    if not item or has_state(item.data, "disabled") then
        return
    end

    if item.data.cmd then
        mp.command(item.data.cmd)
        close()
        return
    end

    if item.data.page_offset then
        open_menus[#open_menus].page = open_menus[#open_menus].page + item.data.page_offset
        render()
        return
    end

    open_submenu(update_focus)
end

local function handle_click()
    focused_level, focused_index = determine_hovered_item()

    if not focused_index then
        close()
        return
    end

    activate_focused_item()
end

local function activate_shortcut(info)
    if info.event == "up" then
        return
   end

   if info.key_text == " " then
       activate_focused_item(true)
   end

   for i, item in ipairs(items[focused_level]) do
        if (item.data.title or ""):sub(1, 1):lower() == info.key_text then
           focused_index = i
           activate_focused_item(true)
           break
       end
   end
end

local bindings = {
    MOUSE_MOVE = handle_mouse_move,
    MBTN_LEFT = handle_click,
    MBTN_MID = handle_click,
    MBTN_RIGHT = handle_click,
    UP = function () move_focus(-1) end,
    LEFT = close_submenu,
    DOWN = function () move_focus(1) end,
    RIGHT = function () open_submenu(true) end,
    HOME = focus_first,
    END = focus_last,
    PGUP = focus_first,
    PGDWN = focus_last,
    ENTER = function () activate_focused_item(true) end,
    ESC = function () close() end,
    ANY_UNICODE = activate_shortcut,
}
for _, key in pairs({"UP", "DOWN", "LEFT", "RIGHT", "HOME", "END", "PGUP",
                    "PGDWN", "ENTER"}) do
    bindings["KP_" .. key] = bindings[key]
end

close = function ()
    update_overlay("", 0, 0, 0)
    clear_timers()
    focused_index = nil

    for key, _ in pairs(bindings) do
        mp.remove_key_binding("_context_menu_" .. key)
    end
end

mp.register_script_message("open", function ()
    open_menus = {}
    focused_level = 1

    local x, y, hover = get_scaled_mouse_pos()

    if not hover then
        x = 0
        y = 0
    end

    add_menu(mp.get_property_native("menu-data"), x, y)

    render()

    for key, fn in pairs(bindings) do
        mp.add_forced_key_binding(key, "_context_menu_" .. key, fn, {
            repeatable = true,
            complex = key == "ANY_UNICODE",
        })
    end
end)

mp.register_script_message("select", activate_focused_item)

require "mp.options".read_options(options)
