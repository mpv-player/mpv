-- align: -1 .. +1
-- frame: size of the containing area
-- obj: size of the object that should be positioned inside the area
-- margin: min. distance from object to frame (as long as -1 <= align <= +1)
function get_align(align, frame, obj, margin)
    frame = frame - margin * 2
    return margin + frame / 2 - obj / 2 + (frame - obj) / 2 * align
end

local ass_mt = {}
ass_mt.__index = ass_mt

function ass_new()
    return setmetatable({ scale = 4, text = "" }, ass_mt)
end

function ass_mt.new_event(ass)
    -- osd_libass.c adds an event per line
    if #ass.text > 0 then
        ass.text = ass.text .. "\n"
    end
end

function ass_mt.draw_start(ass)
    ass.text = string.format("%s{\\p%d}", ass.text, ass.scale)
end

function ass_mt.draw_stop(ass)
    ass.text = ass.text .. "{\\p0}"
end

function ass_mt.coord(ass, x, y)
    local scale = math.pow(2, ass.scale - 1)
    local ix = math.ceil(x * scale)
    local iy = math.ceil(y * scale)
    ass.text = string.format("%s %d %d", ass.text, ix, iy)
end

function ass_mt.append(ass, s)
    ass.text = ass.text .. s
end

function ass_mt.pos(ass, x, y)
    ass:append(string.format("{\\pos(%f,%f)}", x, y))
end

function ass_mt.move_to(ass, x, y)
    ass:append(" m")
    ass:coord(x, y)
end

function ass_mt.line_to(ass, x, y)
    ass:append(" l")
    ass:coord(x, y)
end

function ass_mt.rect_ccw(ass, x0, y0, x1, y1)
    ass:move_to(x0, y0)
    ass:line_to(x0, y1)
    ass:line_to(x1, y1)
    ass:line_to(x1, y0)
end

function ass_mt.rect_cw(ass, x0, y0, x1, y1)
    ass:move_to(x0, y0)
    ass:line_to(x1, y0)
    ass:line_to(x1, y1)
    ass:line_to(x0, y1)
end

function get_bar_location()
    local playresx, playresy = mp.get_osd_resolution()

    local osd_w = 75.0
    local osd_h = 3.125
    local outline = 2.5

    local w = playresx * osd_w / 100.0
    local h = playresy * osd_h / 100.0
    local o = outline * h / playresy / 0.03125
    o = math.min(o, h / 5.2)
    o = math.max(o, h / 32.0)

    local x = get_align(0,   playresx, w, o)
    local y = get_align(0.5, playresy, h, o)

    return x, y, w, h, o
end

function draw_bar(ass)
    local duration = tonumber(mp.property_get("length"))
    local pos = tonumber(mp.property_get("time-pos")) / duration

    local x, y, w, h, border = get_bar_location()

    -- override the style header (which we can't access from Lua as of now)
    local prefix = string.format("{\\shad0\\bord%f}", border)

    -- filled area
    ass:new_event()
    ass:append(prefix)
    ass:append("{\\bord0}")
    ass:pos(x, y)
    ass:draw_start()
    local xp = pos * w - border / 2
    ass:rect_ccw(0, 0, xp, h)
    ass:draw_stop()

    -- position marker
    ass:new_event()
    ass:append(prefix)
    ass:append(string.format("{\\bord%f}", border / 2))
    ass:pos(x, y)
    ass:draw_start(d)
    ass:move_to(xp + border / 2, 0)
    ass:line_to(xp + border / 2, h)
    ass:draw_stop(d)

    ass:new_event()
    ass:append(prefix)
    ass:pos(x, y)
    ass:draw_start()

    -- the box
    ass:rect_cw(-border, -border, w + border, h + border);

    -- the "hole"
    ass:rect_ccw(0, 0, w, h)

    -- chapter marks
    local chapters = mp.get_chapter_list()
    for n = 1, #chapters do
        local s = chapters[n].time / duration * w
        local dent = border * 1.3

        if s > dent and s < w - dent then
            ass:move_to(s + dent, 0)
            ass:line_to(s,        dent)
            ass:line_to(s - dent, 0)

            ass:move_to(s - dent, h)
            ass:line_to(s,        h - dent)
            ass:line_to(s + dent, h)
        end
    end

    ass:draw_stop()
end

local state = {
    osd_visible = false,
    mouse_down = false,
}

-- called by input.conf bindings
function mp_mouse_move()
    state.last_osd_time = mp.get_timer()
    state.osd_visible = true
end

function mp_mouse_click(down)
    local b_x, b_y, b_w, b_h, _ = get_bar_location()
    local x, y = mp.get_mouse_pos()

    state.mouse_down = down

    if x >= b_x and y >= b_y and x <= b_x + b_w and y <= b_y + b_h then
        local duration = tonumber(mp.property_get("length"))
        local time = (x - b_x) / b_w * duration
        mp.send_command(string.format("seek %f absolute", time))
    end
end

-- called by mpv on every frame
function mp_update()
    local ass = ass_new()

    local x, y = mp.get_mouse_pos()
    local now = mp.get_timer()

    local osd_time = 1.5
    if state.osd_visible and now - state.last_osd_time < osd_time then
        draw_bar(ass)
    else
        state.osd_visible = false
    end

    ass:new_event()
    ass:pos(x, y)
    ass:append("{\\an5}")
    if state.mouse_down == true then
        ass:append("-")
    else
        ass:append("+")
    end

    local w, h, aspect = mp.get_screen_size()
    mp.set_osd_ass(h * aspect, h, ass.text)
end
