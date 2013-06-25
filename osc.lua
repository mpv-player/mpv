local assdraw = require 'assdraw'

local osc_geo = {
    -- user-safe
    scale = 1,                              -- scaling of the controller
    vidscale = true,                        -- scale the controller with the video? don't use false, currently causes glitches
    valign = 0.8,                           -- vertical alignment, -1 (top) to 1 (bottom)
    halign = 0,                             -- vertical alignment, -1 (left) to 1 (right)
    deadzonedist = 0.2,                     -- distance between OSC and deadzone
    iAmAProgrammer = false,                 -- start counting stuff at 0

    -- not user-safe
    osc_w = 550,                            -- width, height, corner-radius, padding of the box
    osc_h = 150,
    osc_r = 10,
    osc_p = 15,

    -- calculated by osc_init()
    playresy = 0,                           -- canvas size Y
    playresx = 0,                           -- canvas size X
    posX, posY = 0,0,                       -- position of the controler
    pos_offsetX, pos_offsetY = 0,0,         -- vertical/horizontal position offset for contents aligned at the borders of the box
}

-- internal states, do not touch
local state = {
    osc_visible = false,
    mouse_down = false,
    last_mouse_posX,
    bar_location,
    mouse_down_counter = 0,
    active_button = nil,                    -- nil = none, 0 = background, 1+ = see elements[]
    rightTC_trem = false,
    mp_screen_sizeX,
    mp_screen_sizeY,
}

-- align:  -1 .. +1
-- frame:  size of the containing area
-- obj:    size of the object that should be positioned inside the area
-- margin: min. distance from object to frame (as long as -1 <= align <= +1)
function get_align(align, frame, obj, margin)
    return (frame / 2) + (((frame / 2) - margin - (obj / 2)) * align)
end

function draw_bar_simple(ass, x, y, w, h)
    local pos = 0
    local duration = 0
    if not (mp.property_get("length") == nil) then
        duration = tonumber(mp.property_get("length"))
        pos = tonumber(mp.property_get("ratio-pos"))
        --local pos = tonumber(mp.property_get("percent-pos")) / 100
    end

    -- thickness of border and gap between border and filling bar
    local border, gap = 1, 2

    local fill_offset = border + gap
    local xp = (pos * (w - (2*fill_offset))) + fill_offset

    ass:draw_start()
    -- the box
    ass:rect_cw(0, 0, w, h);

    -- the "hole"
    ass:rect_ccw(border, border, w - border, h - border)

    -- chapter nibbles
    local chapters = mp.get_chapter_list()
    for n = 1, #chapters do
        if chapters[n].time > 0 and chapters[n].time < duration then
            local s = (chapters[n].time / duration * (w - (2*fill_offset))) + fill_offset

            ass:rect_cw(s - 1, 1, s, 2);
            ass:rect_cw(s - 1, h - 2, s, h - 1);

        end
    end

    -- the filling, draw it only if positive
    if pos > 0 then
        ass:rect_cw(fill_offset, fill_offset, xp, h - fill_offset)
    end

    -- remember where the bar is for seeking
    local b_x, b_y, b_w, b_h = x - (w/2) + fill_offset, y - h + fill_offset, (w - (2*fill_offset)), (h - (2*fill_offset))
    state.bar_location = {b_x=b_x, b_y=b_y, b_w=b_w, b_h=b_h}

end


-- returns hitbox spanning coordinates (top left, bottom right corner) according to alignment
function get_hitbox_coords(x, y, an, w, h)

    local alignments = {
      [1] = function () return x, y-h, x+w, y end,
      [2] = function () return x-(w/2), y-h, x+(w/2), y end,
      [3] = function () return x-w, y-h, x, y end,

      [4] = function () return x, y-(h/2), x+w, y+(h/2) end,
      [5] = function () return x-(w/2), y-(h/2), x+(w/2), y+(h/2) end,
      [6] = function () return x-w, y-(h/2), x, y+(h/2) end,

      [7] = function () return x, y, x+w, y+h end,
      [8] = function () return x-(w/2), y, x+(w/2), y+h end,
      [9] = function () return x-w, y, x, y+h end,
    }

    return alignments[an]()
end

local elements = {}

function register_element(x, y, an, w, h, styleA, styleB, content, down_cmd, up_cmd, down_repeat)

    local element = {
        x = x,
        y = y,
        an = an,
        w = w,
        h = h,
        styleA = styleA,
        styleB = styleB,
        content = content,
        down_cmd = down_cmd,
        up_cmd = up_cmd,
        down_repeat = down_repeat,
    }

    table.insert(elements, element)
end


function render_elements(ass)

    for n = 1, #elements do

        local element = elements[n]
        local style = element.styleA

        if state.mouse_down == true and state.active_button == n then
            local mX, mY = mp.get_mouse_pos()
            local bX1, bY1, bX2, bY2 = get_hitbox_coords(element.x, element.y, element.an, element.w, element.h)

            if mX >= bX1 and mX <= bX2 and mY >= bY1 and mY <= bY2 then
                if element.styleB == nil then
                else
                    style = style .. element.styleB
                end

                if element.down_cmd == nil then
                elseif element.down_repeat == false then
                    element.down_cmd()
                elseif state.mouse_down_counter == 0 or (state.mouse_down_counter >= 15 and state.mouse_down_counter % 5 == 0) then
                    element.down_cmd()
                end
                state.mouse_down_counter = state.mouse_down_counter + 1
            end

        end

        ass:new_event()
        ass:pos(element.x, element.y)
        ass:an(element.an) -- positioning
        ass:append(style) -- styling
        if type(element.content) == "function" then
            element.content(ass) -- function objects
        else
            ass:append(element.content) -- text objects
        end
    end
end

-- Did mouse go down on a button?
function any_button_down()

    local mX, mY = mp.get_mouse_pos()
    for n = 1, #elements do

        local bX1, bY1, bX2, bY2 = get_hitbox_coords(elements[n].x, elements[n].y, elements[n].an, elements[n].w, elements[n].h)

        if mX >= bX1 and mX <= bX2 and mY >= bY1 and mY <= bY2 then
            --print("click on button #" .. n .. "   \n")
            state.active_button = n
        end
    end

    -- if state.active_button is still nil, user must have clicked outside the controller -> 0
    if state.active_button == nil then
        state.active_button = 0
    end
end

-- Did mouse go up on the same button?
function any_button_up()
    if not (state.active_button == nil) then

        local n = state.active_button

        if n == 0 and not (mouse_over_osc()) then
            --click on background
            --hide_osc()
        elseif n > 0 and not (elements[n].up_cmd == nil) then
            local bX1, bY1, bX2, bY2 = get_hitbox_coords(elements[n].x, elements[n].y, elements[n].an, elements[n].w, elements[n].h)
            local mX, mY = mp.get_mouse_pos()

            if mX >= bX1 and mX <= bX2 and mY >= bY1 and mY <= bY2 then
                --print("up on button #" .. n .. "    \n")
                elements[n].up_cmd()
            end
        end
    end
    state.active_button = nil
end


local osc_styles = {
    bigButtons = "{\\bord0\\1c&HFFFFFF\\1a&H00&\\3c&HFFFFFF\\3a&HFF&\\fs50\\fnosd-font}",
    smallButtonsL = "{\\bord0\\1c&HFFFFFF\\1a&H00&\\3c&HFFFFFF\\3a&HFF&\\fs20\\fnosd-font}",
    smallButtonsR = "{\\bord0\\1c&HFFFFFF\\1a&H00&\\3c&HFFFFFF\\3a&HFF&\\fs30\\fnosd-font}",

    elementDown = "{\\1c&H999999}",
    elementDisab = "{\\1a&H88&}",
    timecodes = "{\\bord0\\1c&HFFFFFF\\1a&H00&\\3c&HFFFFFF\\3a&HFF&\\fs25\\fnsans-serif}",
    vidtitle = "{\\bord0\\1c&HFFFFFF\\1a&H00&\\3c&HFFFFFF\\3a&HFF&\\fs12\\fnsans-serif}",
    box = "{\\bord1\\1c&H000000\\1a&H64&\\3c&HFFFFFF\\3a&H00&}",
}


-- OSC INIT
function osc_init ()

    -- kill old Elements
    elements = {}

    -- set canvas resolution acording to display aspect and scaleing setting
    local baseResY = 720
    local display_w, display_h, display_aspect = mp.get_screen_size()
    if osc_geo.vidscale == true then
        osc_geo.playresy = baseResY / osc_geo.scale
    else
        osc_geo.playresy = display_h / osc_geo.scale
    end
    osc_geo.playresx = osc_geo.playresy * display_aspect


    -- Some calculations on stuff we'll need
    -- vertical/horizontal position offset for contents aligned at the borders of the box
    osc_geo.pos_offsetX, osc_geo.pos_offsetY = (osc_geo.osc_w - (2*osc_geo.osc_p)) / 2, (osc_geo.osc_h - (2*osc_geo.osc_p)) / 2

    -- position of the controller according to video aspect and valignment
    osc_geo.posX = math.floor(get_align(osc_geo.halign, osc_geo.playresx, osc_geo.osc_w, 0))
    osc_geo.posY = math.floor(get_align(osc_geo.valign, osc_geo.playresy, osc_geo.osc_h, 0))

    -- fetch values
    local osc_w, osc_h, osc_r, osc_p = osc_geo.osc_w, osc_geo.osc_h, osc_geo.osc_r, osc_geo.osc_p
    local pos_offsetX, pos_offsetY = osc_geo.pos_offsetX, osc_geo.pos_offsetY
    local posX, posY = osc_geo.posX, osc_geo.posY

    --
    -- Backround box
    --

    local contentF = function (ass)
        ass:draw_start()
        ass:move_to(osc_r, 0)
        ass:line_to(osc_w - osc_r, 0) -- top line
        if osc_r > 0 then ass:bezier_curve(osc_w, 0, osc_w, 0, osc_w, osc_r) end -- top right corner
        ass:line_to(osc_w, osc_h - osc_r) -- right line
        if osc_r > 0 then ass:bezier_curve(osc_w, osc_h, osc_w, osc_h, osc_w - osc_r, osc_h) end -- bottom right corner
        ass:line_to(osc_r, osc_h) -- bottom line
        if osc_r > 0 then ass:bezier_curve(0, osc_h, 0, osc_h, 0, osc_h - osc_r) end -- bottom left corner
        ass:line_to(0, osc_r) -- left line
        if osc_r > 0 then ass:bezier_curve(0, 0, 0, 0, osc_r, 0) end -- top left corner
        ass:draw_stop()
    end
    register_element(posX, posY, 5, osc_w, osc_h, osc_styles.box, nil, contentF, nil, nil, false)

    --
    -- Title
    --

    -- title
    local contentF = function (ass)
        local title = mp.property_get_string("media-title")
        if not (title == nil) then

            if #title > 80 then
                title = string.format("{\\fscx%f}", (80 / #title) * 100) .. title
            end

            ass:append(title)
        else
            ass:append("mpv")
        end
    end
    local up_cmd = function () mp.send_command("show_text ${media-title}") end
    register_element(posX, posY - pos_offsetY - 10, 8, 496, 12, osc_styles.vidtitle, osc_styles.elementDown, contentF, nil, up_cmd, false)

    -- If we have more than one playlist entry, render playlist navigation buttons
    if tonumber(mp.property_get("playlist-count")) > 1 then
        -- playlist prev
        local up_cmd = function () mp.send_command("playlist_prev weak") end
        register_element(posX - pos_offsetX, posY - pos_offsetY - 10, 7, 12, 12, osc_styles.vidtitle, osc_styles.elementDown, "◀", nil, up_cmd, false)

        -- playlist next
        local up_cmd = function () mp.send_command("playlist_next weak") end
        register_element(posX + pos_offsetX, posY - pos_offsetY - 10, 9, 12, 12, osc_styles.vidtitle, osc_styles.elementDown, "▶", nil, up_cmd, false)
    end

    --
    -- Big buttons
    --

    local bbposY = posY - pos_offsetY + 35

    --play/pause
    local contentF = function (ass)
        if mp.property_get("pause") == "yes" then
            ass:append("\238\132\129")
        else
            ass:append("\238\128\130")
        end
    end
    local up_cmd = function () mp.send_command("no-osd cycle pause") end
    register_element(posX, bbposY, 5, 40, 40, osc_styles.bigButtons, osc_styles.elementDown, contentF, nil, up_cmd, false)

    --skipback
    local down_cmd = function () mp.send_command("no-osd seek -5 relative keyframes") end
    register_element(posX-60, bbposY, 5, 40, 40, osc_styles.bigButtons, osc_styles.elementDown, "\238\128\132", down_cmd, nil, true)

    --skipfrwd
    local down_cmd = function () mp.send_command("no-osd seek 10 relative keyframes") end
    register_element(posX+60, bbposY, 5, 40, 40, osc_styles.bigButtons, osc_styles.elementDown, "\238\128\133", down_cmd, nil, true)

    -- do we have chapters?
    if (#mp.get_chapter_list()) > 0 then

        --prev
        local up_cmd = function () mp.send_command("add chapter -1") end
        register_element(posX-120, bbposY, 5, 40, 40, osc_styles.bigButtons, osc_styles.elementDown, "\238\132\132", nil, up_cmd, false)

        --next
        local up_cmd = function () mp.send_command("add chapter 1") end
        register_element(posX+120, bbposY, 5, 40, 40, osc_styles.bigButtons, osc_styles.elementDown, "\238\132\133", nil, up_cmd, false)

    else -- if not, render buttons as disabled and don't attach functions
        --prev
        register_element(posX-120, bbposY, 5, 40, 40, (osc_styles.bigButtons .. osc_styles.elementDisab), nil, "\238\132\132", nil, nil, false)

        --next
        register_element(posX+120, bbposY, 5, 40, 40, (osc_styles.bigButtons .. osc_styles.elementDisab), nil, "\238\132\133", nil, nil, false)

    end

    --
    -- Smaller buttons
    --

    --get video/audio/sub track counts
    local tracktable = mp.get_track_list()
    local videotracks, audiotracks, subtracks = 0, 0, 0
    for n = 1, #tracktable do
        if tracktable[n].type == "video" then
            videotracks = videotracks + 1
        elseif tracktable[n].type == "audio" then
            audiotracks = audiotracks + 1
        elseif tracktable[n].type == "sub" then
            subtracks = subtracks + 1
        end
    end

    --cycle audio tracks
    
    local contentF = function (ass)
        local aid = ""
        if (mp.property_get("audio") == "no") then
            aid = "–"
        else
            if (osc_geo.iAmAProgrammer == true) then
                aid = tonumber(mp.property_get("audio"))
            else
                aid = tonumber(mp.property_get("audio")) + 1
            end
        end
        ass:append("\238\132\134 {\\fs17}" .. aid .. "/" .. audiotracks)
    end
    if audiotracks > 0 then -- do we have any?
        local up_cmd = function () mp.send_command("add audio") end
        register_element(posX-pos_offsetX, bbposY, 1, 70, 18, osc_styles.smallButtonsL, osc_styles.elementDown, contentF, nil, up_cmd, false)
    else
        register_element(posX-pos_offsetX, bbposY, 1, 70, 18, (osc_styles.smallButtonsL .. osc_styles.elementDown), nil, contentF, nil, nil, false)
    end

    --cycle sub tracks
    
    local contentF = function (ass)
        local sid = ""
        if (mp.property_get("sub") == "no") then
            sid = "–"
        else
            if (osc_geo.iAmAProgrammer == true) then
                sid = tonumber(mp.property_get("sub"))
            else
                sid = tonumber(mp.property_get("sub")) + 1
            end
        end
        ass:append("\238\132\135 {\\fs17}" .. sid .. "/" .. subtracks)
    end
    if subtracks > 0 then -- do we have any?
        local up_cmd = function () mp.send_command("add sub") end
        register_element(posX-pos_offsetX, bbposY, 7, 70, 18, osc_styles.smallButtonsL, osc_styles.elementDown, contentF, nil, up_cmd, false)
    else
        register_element(posX-pos_offsetX, bbposY, 7, 70, 18, (osc_styles.smallButtonsL .. osc_styles.elementDown), nil, contentF, nil, nil, false)
    end



    --toggle FS
    local contentF = function (ass)
        if mp.property_get("fullscreen") == "yes" then
            ass:append("\238\132\137")
        else
            ass:append("\238\132\136")
        end
    end
    local up_cmd = function () mp.send_command("no-osd cycle fullscreen") end
    register_element(posX+pos_offsetX, bbposY, 6, 25, 25, osc_styles.smallButtonsR, osc_styles.elementDown, contentF, nil, up_cmd, false)


    --
    -- Seekbar
    --

    local contentF = function (ass)
            draw_bar_simple(ass, posX, posY+pos_offsetY-30, pos_offsetX*2, 17)
    end

    local down_cmd = function ()
        -- Ignore identical seeks
        if not (state.last_mouse_posX == mp.get_mouse_pos()) then
            state.last_mouse_posX = mp.get_mouse_pos()

            local b_x, b_y, b_w, b_h = state.bar_location.b_x, state.bar_location.b_y, state.bar_location.b_w, state.bar_location.b_h
            local x, y = mp.get_mouse_pos()

            if x >= b_x and y >= b_y and x <= b_x + b_w and y <= b_y + b_h then
                local time = (x - b_x) / b_w * 100

                mp.send_command(string.format("no-osd seek %f absolute-percent keyframes", time))
            end
        end
    end
    -- do we have a usuable duration?
    if (not (mp.property_get("length") == nil)) and (tonumber(mp.property_get("length")) > 0) then
        register_element(posX, posY+pos_offsetY-30, 2, pos_offsetX*2, 17, osc_styles.timecodes, nil, contentF, down_cmd, nil, false)
    else
        register_element(posX, posY+pos_offsetY-30, 2, pos_offsetX*2, 17, (osc_styles.timecodes .. osc_styles.elementDisab), nil, contentF, nil, nil, false)
    end

    --
    -- Timecodes
    --

    -- left (current pos)
    local contentF = function (ass) return ass:append(mp.property_get_string("time-pos")) end
    register_element(posX - pos_offsetX, posY + pos_offsetY, 1, 110, 25, osc_styles.timecodes, nil, contentF, nil, nil, false)

    -- right (total/remaining time)
    local contentF = function (ass)
        if state.rightTC_trem == true then
            ass:append("-" .. mp.property_get_string("time-remaining"))
        else
            ass:append(mp.property_get_string("length"))
        end
    end
    local up_cmd = function () state.rightTC_trem = not state.rightTC_trem end
    -- do we have a usuable duration?
    if (not (mp.property_get("length") == nil)) and (tonumber(mp.property_get("length")) > 0) then
        register_element(posX + pos_offsetX, posY + pos_offsetY, 3, 110, 25, osc_styles.timecodes, osc_styles.elementDown, contentF, nil, up_cmd, false)
    end

end


function draw_osc(ass)

    render_elements(ass)

end

function mouse_over_osc()
    local mX, mY = mp.get_mouse_pos()
    local bX1, bY1, bX2, bY2 = get_hitbox_coords(osc_geo.posX, osc_geo.posY, 5, osc_geo.osc_w, osc_geo.osc_h)

    if mX >= bX1 and mX <= bX2 and mY >= bY1 and mY <= bY2 then
        return true
    else
        return false
    end
end

function show_osc()
    state.last_osd_time = mp.get_timer()
    state.osc_visible = true
end

function hide_osc()
    state.osc_visible = false
end

-- called by input.conf bindings
function mouse_move()
    --print "\nI like to move it, move it!\n"
    show_osc()
end

function mouse_leave()
    hide_osc()
end

function mouse_click(down)

    -- Build our own mouse_down/up events
    if down == true and state.mouse_down == false then
        mouse_down();
    elseif down == false and state.mouse_down == true then
        mouse_up();
    end

    state.mouse_down = down
    mp_update()
end

function mouse_up()
    --mp.send_command("set pause no")
    state.mouse_down_counter = 0
    any_button_up()
    state.last_mouse_pos = nil
end

function mouse_down()
    --mp.send_command("set pause yes")
    if state.osc_visible then
        any_button_down()
    end
end

-- called by mpv on every frame
function mp_update()
    local current_screen_sizeX, current_screen_sizeY = mp.get_screen_size()

    if not (state.mp_screen_sizeX == current_screen_sizeX and state.mp_screen_sizeY == current_screen_sizeY) then
    -- display changed, reinit everything
        osc_init()
        state.mp_screen_sizeX, state.mp_screen_sizeY = current_screen_sizeX, current_screen_sizeY
    end

    local area_y0, area_y1
    if osc_geo.valign > 0 then
        -- deadzone above OSC
        area_y0 = get_align(1 - osc_geo.deadzonedist, osc_geo.posY - (osc_geo.osc_h / 2), 0, 0)
        area_y1 = osc_geo.playresy
    else
        -- deadzone below OSC
        area_y0 = 0
        area_y1 = (osc_geo.posY + (osc_geo.osc_h / 2)) + get_align(-1 + osc_geo.deadzonedist, osc_geo.playresy - (osc_geo.posY + (osc_geo.osc_h / 2)), 0, 0)
    end
    set_mouse_area(0, area_y0, osc_geo.playresx, area_y1)

    local ass = assdraw.ass_new()

    local x, y = mp.get_mouse_pos()

    local now = mp.get_timer()




    --state.append_calls = 0

    if state.osc_visible then
        draw_osc(ass)
    end


    --[[ Old show handling

    if mouse_over_osc() or state.mouse_down then
        show_osc()
    end

    local osd_time = 1

    if state.osc_visible and now - state.last_osd_time < osd_time then
        draw_osc(ass)
        state.osc_visible = true
    else
        state.osc_visible = false
    end
    --]]

    ass:new_event()

    local playresx, playresy = mp.get_osd_resolution()
    ass:append("{get_osd_resolution: X:" .. playresx .. " Y:" .. playresy .. "}")
    --local playresx, playresy = mp.get_osd_resolution()
    --ass:append("get_mouse_pos: X:" .. x .. " Y:" .. y .. "")
    --if state.active_button == nil then
    --    ass:append("state.active_button: " .. "nil")
    --else
    --    ass:append("state.active_button: " .. state.active_button)
    --end
    --ass:append("Rendertime: " .. mp.get_timer() - now .. "   state.append_calls: " .. state.append_calls)



    --[[
    ass:new_event()
    ass:pos(x, y)
    ass:append("{\\an5}")
    if state.mouse_down == true then
        ass:append("-")
    else
        ass:append("+")
    end
    -- set canvas size
    --mp.set_osd_ass(osc_geo.playresx, osc_geo.playresy, ass.text)
    --]]

    -- make sure there is something in ass so that mp.set_osd_ass won't fail
    --ass:new_event()
    --ass:append("blah")

    local w, h, aspect = mp.get_screen_size()
    mp.set_osd_ass(osc_geo.playresy * aspect, osc_geo.playresy, ass.text)

end

set_key_bindings {
    {"mouse_btn0",      function(e) mouse_click(false) end,
                        function(e) mouse_click(true)  end},
    {"mouse_move",      mouse_move},
    {"mouse_leave",     mouse_leave},
    {"mouse_btn0_dbl",  "ignore"},
}
enable_key_bindings()
