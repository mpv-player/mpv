local assdraw = require 'mp.assdraw'
local msg = require 'mp.msg'
local opt = require 'mp.options'

-- -----------------------------------------------
-- Customization Options
-- -----------------------------------------------

local user_opts = {
    -- Time OSD stays on screen after any change
    duration = 1.0,

    -- Bar geometry
    bar_width        = 0.75,                -- bar width (as fraction of screen width)
    bar_height       = 0.04,                -- bar height (as fraction of screen height)
    bar_x_offset     = 0,                   -- horizontal position for bar counting from center of screen (as fraction of screen width)
    bar_y_pos        = 0.75,                -- vertical position for bar counting from top of screen (as fraction of screen height)
    bar_radius       = 0,                   -- corner radius of the bar (as fraction of screen diagonal)
    round_fill_end   = true,                -- toggle for rounding fill bar right corners
    show_bar_outline = true,                -- toggle for bar outline
    outline_gap      = 0.003,               -- gap between bar and it's outline (as fraction of screen diagonal)
    marker_width     = 0.005,               -- marker width (as fraction of bar width)
    marker_height    = 0.2,                 -- marker height (as fraction of bar height)

    -- Colors
    bar_body_color          = "#FFFFFF",            -- color of the bar's section that isn't filled
    bar_body_border_color   = "#000000",            -- color of the outer border of the bar's section that isn't filled
    bar_fill_color          = "#FFFFFF",            -- color of the bar's filled section
    bar_fill_border_color   = "#000000",            -- color of the outer border of the bar's filled section
    bar_outline_color       = "#FFFFFF",            -- color of the bar's outline
    bar_outline_border_color= "#000000",            -- color of the outer border of the bar's outline
    text_color              = "#FFFFFF",            -- color of the text of the property that is being changed (eg: volume, seek, etc)
    text_border_color       = "#000000",            -- color of the text's border
    text_shadow_color       = "#000000",            -- color of the text's shadow
    icon_color              = "#FFFFFF",            -- color of the icon of the property that is being changed 
    icon_border_color       = "#000000",            -- color of the icon's border
    text_box_color          = "#FFFFFF",            -- color of the text's background
    text_box_border_color   = "#000000",            -- color of the text's box 

    -- Alphas (aka opacity)
    -- goes from 0 (fully opaque) to 255 (fully transparent)
    bar_body_alpha          = 255,                 -- alpha of the bar's section that isn't filled 
    bar_body_border_alpha   = 0,                   -- alpha of the outer border of the bar's section that isn't filled
    bar_fill_alpha          = 0,                   -- alpha of the bar's filled section
    bar_fill_border_alpha   = 0,                   -- alpha of the outer border of the bar's filled section
    bar_outline_alpha       = 0,                   -- alpha of the bar's outline
    bar_outline_border_alpha= 0,                   -- alpha of the outer border of the bar's outline
    icon_border_alpha       = 0,                   -- alpha of the icon border
    text_alpha              = 0,                   -- opacity of text
    text_border_alpha       = 0,                   -- opacity of text outline
    text_shadow_alpha       = 0,                   -- opacity of text shadow
    icon_alpha              = 0,                   -- opacity of icon
    text_box_alpha          = 200,                 -- opacity of text background
    text_box_border_alpha   = 255,                 -- opacity of text background border

    -- Text
    font_size       = 0.030,            -- text size (as fraction of screen diagonal)
    font            = "",               -- optional text font name
    label_x_offset  = 0.02,             -- horizontal offset for label counting from left of screen (as fraction of screen width)
    label_y_offset  = 0.02,             -- vertical offset for label counting from top of screen (as fraction of screen height)

    text_border       = 0.002,          -- size of text outline (as fraction of screen diagonal)
    text_shadow       = 0.001,          -- size of text shadow (as fraction of screen diagonal)

    text_box          = false,          -- toggle for background square behind label
    label_box_size    = 0.5,            -- controls how wide the background box is compared to the text
    label_box_pad_x   = 0.04,           -- horizontal padding inside the text background box (as fraction of screen width)
    label_box_pad_y   = 0.02,           -- vertical padding inside the text background box (as fraction of screen height)

    text_center       = false,          -- align label with center of screen

    -- Icons
    show_icons_label  = false,          -- toggle for showing icon at start of label
    show_icons_bar    = true,           -- toggle for showing icon next to bar
    icon_size         = 0.055,          -- size of icon next to bar (as fraction of screen diagonal)
    distance_bar      = 0.020,          -- horizontal distance from bar (as fraction of screen width)
    icon_y_offset     = -0.005,         -- vertical offset from bar level (as fraction of screen height)
    icon_volume       = "🔈",           -- icon for Volume
    icon_mute         = "🔇",           -- icon for Muting
    icon_play         = "▶",            -- icon for Playing
    icon_pause        = "⏸",            -- icon for Pausing
    icon_seek         = "⏩",           -- icon for Seek
    icon_brightness   = "🔆",           -- icon for Brightness
    icon_saturation   = "◑",            -- icon for Saturation
    icon_contrast     = "◐",            -- icon for Contrast

    bar_when_pausing = false,           -- toggle for showing seek bar when pausing

    -- Feature on/off switches
    show_volume      = true,            -- toggle for showing changes in Volume
    show_seek        = true,            -- toggle for showing changes when seeking
    show_pause       = true,            -- toggle for showing state when pausing/ playing
    show_brightness  = true,            -- toggle for showing changes in Brightness
    show_saturation  = true,            -- toggle for showing changes in Saturation
    show_contrast    = true,            -- toggle for showing changes in Contrast
}


-- 3. Runtime state
local state = {
    hide_timer = nil,
}

-- -----------------------------------------------
-- Utility Helpers
-- -----------------------------------------------

local function clamp(value, min_v, max_v)
    if value < min_v then return min_v end
    if value > max_v then return max_v end
    return value
end

local function to_ratio(value, max_value)
    if max_value == 0 then return 0 end
    return clamp(value / max_value, 0, 1)
end

local function percent_to_ass_alpha(value)
    value = clamp(value, 0, 255)

    local p = math.floor(value + 0.5)

    return string.format("&H%02X&", p)
end

local function scale_value(value, scale)
    return value * scale
end

-- Build the label string, optionally with a leading icon
local function make_label(icon, text, ratio)
    local icon_str = ""
    if user_opts.show_icons_label and icon and icon ~= "" then
        icon_str = icon .. " "
    end

    if ratio == -1 then
        return string.format("%s%s", icon_str, text)
    else
        return string.format("%s%s: %d%%", icon_str, text, math.floor(ratio * 100 + 0.5))
    end
end


-- Converts colors in HEX formatting to ASS/SSA formatting
local function osd_color_convert(color)
	return "&H" .. color:sub(6,7) .. color:sub(4,5) .. color:sub(2,3) .. "&"
end

local function validate_colors()
	for key, value in pairs(user_opts) do
		if type(value) == "string" and value:sub(1,1) == "#" then
			if value:find("^#%x%x%x%x%x%x$") == nil then
            	msg.warn("'" .. value .. "' is not a valid color")
			end
		
			-- if invalid, color will fallback to black, same as OSC behaviour
			user_opts[key] = osd_color_convert(value)
		end
	end
end



-- -----------------------------------------------
-- OSD Helpers
-- -----------------------------------------------

local function clear_osd()
    local w, h = mp.get_osd_size()
    mp.set_osd_ass(w, h, "")
end

local function schedule_clear()
    if state.hide_timer then
        state.hide_timer:kill()
    end

    state.hide_timer = mp.add_timeout(user_opts.duration, clear_osd)
end

-- -----------------------------------------------
-- ASS Style Helpers
-- -----------------------------------------------

-- Primary color tag
local function ass_color(color)
    return string.format("{\\c%s}", color)
end

-- Alpha tag
local function ass_alpha(alpha)
    return string.format("{\\1a%s}", percent_to_ass_alpha(alpha))
end

-- Font size tag
local function ass_fs(size)
    return string.format("{\\fs%d}", size)
end

local function ass_border(size)
    return string.format("{\\bord%d}", size)
end

local function ass_shadow(size)
    return string.format("{\\shad%d}", size)
end

local function ass_border_color(color)
    return string.format("{\\3c%s}", color)
end

local function ass_border_alpha(alpha)
    return string.format("{\\3a%s}", percent_to_ass_alpha(alpha))
end

local function ass_shadow_color(color)
    return string.format("{\\4c%s}", color)
end

local function remove_border()
    return "{\\bord0}"
end

local function remove_shadow()
    return "{\\shad0}"
end

local function position_center()
    return "{\\an5}"
end

local function position_top_left()
    return "{\\an7}"
end

-- Optional font name tag
local function ass_font_tag()
    if user_opts.font and user_opts.font ~= "" then
        return string.format("{\\fn%s}", user_opts.font)
    end
    return ""
end

-- Reset all overrides
local function ass_reset()
    return "{\\r}"
end

-- -----------------------------------------------
-- Drawing Functions
-- -----------------------------------------------

local function draw_text_center(ass, s_diagonal, s_width, s_height, text, approx_w, approx_h)

    local s_box_pad_x = scale_value(user_opts.label_box_pad_x, s_width)
    local s_box_pad_y = scale_value(user_opts.label_box_pad_y, s_height)

    -- background box
    if user_opts.text_box then
        ass:new_event()
        ass:pos(s_width / 2, s_height / 2)
        ass:append(ass_reset())
        ass:append(ass_color(user_opts.text_box_color))
        ass:append(ass_alpha(user_opts.text_box_alpha))
        ass:append(ass_border_color(user_opts.text_box_border_color))
        ass:append(ass_border_alpha(user_opts.text_box_border_alpha))
        ass:append(remove_shadow())

        ass:draw_start()
        ass:rect_cw(-approx_w/2 - s_box_pad_x, -approx_h/2 - s_box_pad_y, approx_w/2 + s_box_pad_x, approx_h/2 + s_box_pad_y)
        ass:draw_stop()
    end

    -- text
    ass:new_event()
    ass:pos(s_width / 2, s_height / 2)
    ass:append(ass_reset())
    ass:append(ass_font_tag())
    ass:append(ass_fs(scale_value(user_opts.font_size, s_diagonal)))
    ass:append(ass_color(user_opts.text_color))
    ass:append(ass_alpha(user_opts.text_alpha))
    ass:append(position_center())
    ass:append(text)

end

local function draw_text(ass, s_diagonal, s_width, s_height, lx, ly, text, approx_w, approx_h)

    local s_box_pad_x = scale_value(user_opts.label_box_pad_x, s_width)
    local s_box_pad_y = scale_value(user_opts.label_box_pad_y, s_height)

    -- background box
    if user_opts.text_box then
        ass:new_event()
        ass:pos(0, 0)
        ass:append(ass_reset())
        ass:append(ass_color(user_opts.text_box_color))
        ass:append(ass_alpha(user_opts.text_box_alpha))
        ass:append(ass_border_color(user_opts.text_box_border_color))
        ass:append(ass_border_alpha(user_opts.text_box_border_alpha))
        ass:append(remove_shadow())
        ass:draw_start()
        ass:rect_cw(lx - s_box_pad_x, ly - s_box_pad_y, lx + approx_w + s_box_pad_x, ly + approx_h + s_box_pad_y)
        ass:draw_stop()
    end

    -- text
    ass:new_event()
    ass:pos(lx, ly)
    ass:append(ass_reset())
    ass:append(position_top_left())
    ass:append(ass_font_tag())
    ass:append(ass_fs(scale_value(user_opts.font_size, s_diagonal)))
    ass:append(ass_color(user_opts.text_color))
    ass:append(ass_alpha(user_opts.text_alpha))
    ass:append(ass_border(scale_value(user_opts.text_border, s_diagonal)))
    ass:append(ass_border_color(user_opts.text_border_color))
    ass:append(ass_shadow(scale_value(user_opts.text_shadow, s_diagonal)))
    ass:append(ass_shadow_color(user_opts.text_shadow_color))
    ass:append(ass_border_color(user_opts.text_border_color))
    ass:append(ass_border_alpha(user_opts.text_border_alpha))
    ass:append(text)
end

local function draw_label(ass, s_diagonal, s_width, s_height, lx, ly, icon, label, ratio)

    local text = make_label(icon, label, ratio)

    -- approximate text size
    local approx_h = scale_value(user_opts.font_size, s_diagonal)
    local approx_w = #text * approx_h * user_opts.label_box_size

    if user_opts.text_center then
        draw_text_center(ass, s_diagonal, s_width, s_height, text, approx_w, approx_h)
    else
        draw_text(ass, s_diagonal, s_width, s_height, lx, ly, text, approx_w, approx_h)
    end

end

local function draw_icon(ass, s_diagonal, s_width, s_height, x, y, icon)
    if not (user_opts.show_icons_bar and icon and icon ~= "") then return end
    
    local v_icon_size = scale_value(user_opts.icon_size, s_diagonal)

    ass:new_event()
    ass:pos(x - scale_value(user_opts.distance_bar, s_width) - v_icon_size / 2, y + scale_value(user_opts.bar_height, s_height) / 2 - v_icon_size / 2 + scale_value(user_opts.icon_y_offset, s_height))
    ass:append(ass_reset())
    ass:append(position_top_left())
    ass:append(ass_fs(v_icon_size))
    ass:append(ass_color(user_opts.icon_color))
    ass:append(ass_alpha(user_opts.icon_alpha))
    ass:append(ass_border_color(user_opts.icon_border_color))
    ass:append(ass_border_alpha(user_opts.icon_border_alpha))
    ass:append(icon)
end

local function draw_outline(ass, s_diagonal, s_width, s_height, x, y)
    if not user_opts.show_bar_outline then return end

    ass:new_event()
    ass:pos(x, y)
    ass:append(ass_reset())
    ass:append(ass_color(user_opts.bar_outline_color))
    ass:append(ass_alpha(user_opts.bar_outline_alpha))
    ass:append(ass_border_color(user_opts.bar_outline_border_color))
    ass:append(ass_border_alpha(user_opts.bar_outline_border_alpha))

    local v_bar_width = scale_value(user_opts.bar_width, s_width)
    local v_bar_height = scale_value(user_opts.bar_height, s_height)

    local g  = scale_value(user_opts.outline_gap, s_diagonal)
    local r  = clamp(scale_value(user_opts.bar_radius, v_bar_height), 0, v_bar_height / 2)
    local x0 = -g
    local y0 = -g
    local x1 = v_bar_width + g
    local y1 = v_bar_height + g

    ass:draw_start()

    -- outer path
    if r > 0 and user_opts.round_fill_end then
        ass:move_to(x0 + r, y0)
        ass:line_to(x1 - r, y0)
        ass:line_to(x1,     y0 + r)
        ass:line_to(x1,     y1 - r)
        ass:line_to(x1 - r, y1)
        ass:line_to(x0 + r, y1)
        ass:line_to(x0,     y1 - r)
        ass:line_to(x0,     y0 + r)
    elseif r > 0 then
        ass:move_to(x0 + r, y0)
        ass:line_to(x1,     y0)
        ass:line_to(x1,     y1)
        ass:line_to(x0 + r, y1)
        ass:line_to(x0,     y1 - r)
        ass:line_to(x0,     y0 + r)
    else
        ass:move_to(x0, y0)
        ass:line_to(x1, y0)
        ass:line_to(x1, y1)
        ass:line_to(x0, y1)
    end

    -- inner hole
    if r > 0 and user_opts.round_fill_end then
        ass:move_to(r,               0)
        ass:line_to(0,               r)
        ass:line_to(0,               v_bar_height - r)
        ass:line_to(r,               v_bar_height)
        ass:line_to(v_bar_width - r, v_bar_height)
        ass:line_to(v_bar_width,     v_bar_height - r)
        ass:line_to(v_bar_width,     r)
        ass:line_to(v_bar_width - r, 0)
    elseif r > 0 then
        ass:move_to(r,              0)
        ass:line_to(0,              r)
        ass:line_to(0,              v_bar_height - r)
        ass:line_to(r,              v_bar_height)
        ass:line_to(v_bar_width,    v_bar_height)
        ass:line_to(v_bar_width,    0)
    else
        ass:move_to(0,              0)
        ass:line_to(0,              v_bar_height)
        ass:line_to(v_bar_width,    v_bar_height)
        ass:line_to(v_bar_width,    0)
    end

    ass:draw_stop()
end

local function draw_bar_body(ass, s_diagonal, s_width, s_height, x, y)

    local v_bar_height = scale_value(user_opts.bar_height, s_height)

    ass:new_event()
    ass:pos(x, y)
    ass:append(ass_reset())
    ass:append(ass_color(user_opts.bar_body_color))
    ass:append(ass_alpha(user_opts.bar_body_alpha))
    ass:append(ass_border_color(user_opts.bar_body_border_color))
    ass:append(ass_border_alpha(user_opts.bar_body_border_alpha))

    ass:draw_start()

    local r = clamp(scale_value(user_opts.bar_radius, v_bar_height), 0, v_bar_height / 2)

    if r > 0 then
        local bw, bh = scale_value(user_opts.bar_width, s_width), v_bar_height
        ass:move_to(r, 0)
        ass:line_to(bw - r, 0)
        ass:line_to(bw, r)
        ass:line_to(bw, bh - r)
        ass:line_to(bw - r, bh)
        ass:line_to(r, bh)
        ass:line_to(0, bh - r)
        ass:line_to(0, r)
    else
        ass:rect_cw(0, 0, scale_value(user_opts.bar_width, s_width), v_bar_height)
    end

    ass:draw_stop()
end

local function draw_fill(ass, s_diagonal, s_width, s_height, x, y, filled)
    if filled <= 0 then return end

    local v_bar_height = scale_value(user_opts.bar_height, s_height)

    ass:new_event()
    ass:pos(x, y)
    ass:append(ass_reset())
    ass:append(ass_color(user_opts.bar_fill_color))
    ass:append(ass_alpha(user_opts.bar_fill_alpha))
    ass:append(ass_border_color(user_opts.bar_fill_border_color))
    ass:append(ass_border_alpha(user_opts.bar_fill_border_alpha))

    ass:draw_start()

    local r = clamp(scale_value(user_opts.bar_radius, v_bar_height), 0, v_bar_height / 2)

    if r > 0 then
        local bh = v_bar_height
        if user_opts.round_fill_end then
            -- rounded on both left and right ends
            ass:move_to(r, 0)
            ass:line_to(filled - r, 0)
            ass:line_to(filled, r)
            ass:line_to(filled, bh - r)
            ass:line_to(filled - r, bh)
            ass:line_to(r, bh)
            ass:line_to(0, bh - r)
            ass:line_to(0, r)
        else
            -- rounded only on left, flat on right
            ass:move_to(r, 0)
            ass:line_to(filled, 0)
            ass:line_to(filled, bh)
            ass:line_to(r, bh)
            ass:line_to(0, bh - r)
            ass:line_to(0, r)
        end
    else
        ass:rect_cw(0, 0, filled, v_bar_height)
    end

    ass:draw_stop()
end

local function draw_marker(ass, s_diagonal, s_width, s_height, x, y, marker_position)

    local v_bar_height = scale_value(user_opts.bar_height, s_height)

    local bord        = scale_value(user_opts.text_border, s_diagonal)
    local mark_x      = x + (scale_value(user_opts.bar_width, s_width) * marker_position) + bord / 2
    local half_base   = scale_value(user_opts.bar_width, s_width)  * user_opts.marker_width
    local mark_height = v_bar_height * user_opts.marker_height

    local bar_top    = y
    local bar_bottom = y + v_bar_height

    local g  = scale_value(user_opts.outline_gap, s_diagonal)

    -- triangle to form V shape
    ass:new_event()
    ass:pos(0, 0)
    ass:append(ass_reset())
    ass:append(ass_color(user_opts.bar_body_border_color))
    ass:append(ass_alpha(user_opts.bar_body_border_alpha))
    ass:append(remove_border())
    ass:append(remove_shadow())
    ass:draw_start()
    ass:move_to(mark_x - half_base - bord, bar_top)
    ass:line_to(mark_x + half_base + bord, bar_top)
    ass:line_to(mark_x,                    bar_top + mark_height + bord)
    ass:move_to(mark_x - half_base - bord, bar_bottom)
    ass:line_to(mark_x + half_base + bord, bar_bottom)
    ass:line_to(mark_x,                    bar_bottom - mark_height - bord)
    ass:draw_stop()

    -- triangle that hides the inside of the other triangle
    ass:new_event()
    ass:pos(0, 0)
    ass:append(ass_reset())
    ass:append(ass_color(user_opts.bar_outline_color))
    ass:append(ass_alpha(user_opts.bar_outline_alpha))
    ass:append(remove_border())
    ass:append(remove_shadow())
    ass:draw_start()
    ass:move_to(mark_x - half_base, bar_top    - g / 2)
    ass:line_to(mark_x + half_base, bar_top    - g / 2)
    ass:line_to(mark_x,             bar_top + mark_height)
    ass:move_to(mark_x - half_base, bar_bottom + g / 2)
    ass:line_to(mark_x + half_base, bar_bottom + g / 2)
    ass:line_to(mark_x,             bar_bottom - mark_height)
    ass:draw_stop()
end

local function draw_thin_marker(ass, s_diagonal, s_width, s_height, x, y, marker_position)

    local v_bar_height = scale_value(user_opts.bar_height, s_height)

    local bord        = scale_value(user_opts.text_border, s_diagonal)
    local mark_x      = x + (scale_value(user_opts.bar_width, s_width) * marker_position) + bord / 2
    local mark_height = v_bar_height * user_opts.marker_height
    local half_base   = scale_value(user_opts.bar_width, s_width)  * user_opts.marker_width

    local bar_top    = y
    local bar_bottom = y + v_bar_height

    ass:new_event()
    ass:pos(0, 0)
    ass:append(ass_reset())
    ass:append(ass_color(user_opts.bar_outline_color))
    ass:append(ass_alpha(user_opts.bar_outline_alpha))
    ass:append(ass_border_color(user_opts.bar_outline_color))
    ass:append(remove_border())
    ass:append(remove_shadow())
    ass:draw_start()

    ass:move_to(mark_x - half_base, bar_top)
    ass:line_to(mark_x + half_base, bar_top)
    ass:line_to(mark_x,             bar_top + mark_height)

    ass:move_to(mark_x - half_base, bar_bottom)
    ass:line_to(mark_x + half_base, bar_bottom)
    ass:line_to(mark_x,             bar_bottom - mark_height)

    ass:draw_stop()
end

local function draw_bar(icon, label, value, min_value, max_value, marker_position)
    local s_width, s_height = mp.get_osd_size()
    local s_diagonal = math.sqrt(s_width * s_width + s_height * s_height)
    local is_color_controls = (label == "Brightness" or label == "Saturation" or label == "Contrast")

    local ratio = 0
    if is_color_controls then
        ratio  = to_ratio(value + 100, math.abs(min_value) + max_value)
    else
        ratio  = to_ratio(value, math.abs(min_value) + max_value)
    end

    local filled = scale_value(user_opts.bar_width, s_width) * ratio

    local label_ratio = (label == "Volume" or is_color_controls) and (value / 100) or ratio
 
    local x  = (s_width - scale_value(user_opts.bar_width, s_width)) / 2 + scale_value(user_opts.bar_x_offset, s_width)
    local y  = s_height * user_opts.bar_y_pos
    local lx = scale_value(user_opts.label_x_offset, s_width)
    local ly = scale_value(user_opts.label_y_offset, s_height)
 
    local ass = assdraw.ass_new()
    
    if label == "Paused" or label == "Playing" then
        draw_label(ass, s_diagonal, s_width, s_height, lx, ly, icon, label, -1)
        if not user_opts.bar_when_pausing then
            mp.set_osd_ass(s_width, s_height, ass.text)
            schedule_clear()
            return
        end
    else
        draw_label(ass, s_diagonal, s_width, s_height, lx, ly, icon, label, label_ratio)
    end

    draw_icon(ass, s_diagonal, s_width, s_height, x, y, icon)
    draw_outline(ass, s_diagonal, s_width, s_height, x, y)
    draw_bar_body(ass, s_diagonal, s_width, s_height, x, y)
    draw_fill(ass, s_diagonal, s_width, s_height, x, y, filled)
 
    if marker_position then
        if user_opts.show_bar_outline then
            draw_marker(ass, s_diagonal, s_width, s_height, x, y, marker_position)
        else
            draw_thin_marker(ass, s_diagonal, s_width, s_height, x, y, marker_position)
        end
    end
 
    mp.set_osd_ass(s_width, s_height, ass.text)
    schedule_clear()
end

-- -----------------------------------------------
-- OSD Actions
-- -----------------------------------------------

local function show_volume()
    if not user_opts.show_volume then return end
    local volume = mp.get_property_number("volume", 0)
    local muted  = mp.get_property_bool("mute", false)
    local icon   = (muted or volume == 0) and user_opts.icon_mute or user_opts.icon_volume
    draw_bar(icon, "Volume", volume, 0, 130, 100 / 130)
end

local function show_seek()
    if not user_opts.show_seek then return end
    local time_pos = mp.get_property_number("time-pos", 0)
    local duration = mp.get_property_number("duration", 0)
    draw_bar(user_opts.icon_seek, "Seek", time_pos, 0, duration, nil)
end

local function show_pause_state()
    if not user_opts.show_pause then return end
    local paused = mp.get_property_bool("pause", false)
    local time_pos = mp.get_property_number("time-pos", 0)
    local duration = mp.get_property_number("duration", 0)

    local icon   = (paused) and user_opts.icon_pause or user_opts.icon_play
    local label   = (paused) and "Paused" or "Playing"

    draw_bar(icon, label, time_pos, 0, duration, nil)
end

local function show_brightness()
    if not user_opts.show_brightness then return end
    local v = mp.get_property_number("brightness", 0)
    draw_bar(user_opts.icon_brightness, "Brightness", v, -100, 100, 0.5)
end

local function show_saturation()
    if not user_opts.show_saturation then return end
    local v = mp.get_property_number("saturation", 0)
    draw_bar(user_opts.icon_saturation, "Saturation", v, -100, 100, 0.5)
end

local function show_contrast()
    if not user_opts.show_contrast then return end
    local v = mp.get_property_number("contrast", 0)
    draw_bar(user_opts.icon_contrast, "Contrast", v, -100, 100, 0.5)
end



-- Disable C OSD
mp.set_property("osd-level", "0")


-- Read options from config file and command-line with callback possibility 
opt.read_options(user_opts, "osd", function(changed)
    validate_colors()
end)

validate_colors()




-- -----------------------------------------------
-- Hook to mpv
-- -----------------------------------------------

mp.observe_property("volume",     "number", show_volume)
mp.observe_property("mute",       "bool",   show_volume)
mp.observe_property("pause",      "bool",   show_pause_state)
mp.observe_property("brightness", "number", show_brightness)
mp.observe_property("saturation", "number", show_saturation)
mp.observe_property("contrast",   "number", show_contrast)

mp.register_event("seek", show_seek)

msg.info("Lua OSD loaded")


