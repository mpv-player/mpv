local assdraw = require 'mp.assdraw'
local msg = require 'mp.msg'
local opt = require 'mp.options'

--
-- Parameters
--

-- default user option values
-- do not touch, change them in osc.conf
local user_opts = {
    showwindowed = true,        -- show OSC when windowed?
    showfullscreen = true,      -- show OSC when fullscreen?
    scalewindowed = 1,          -- scaling of the controller when windowed
    scalefullscreen = 1,        -- scaling of the controller when fullscreen
    scaleforcedwindow = 2,      -- scaling when rendered on a forced window
    vidscale = true,            -- scale the controller with the video?
    valign = 0.8,               -- vertical alignment, -1 (top) to 1 (bottom)
    halign = 0,                 -- horizontal alignment, -1 (left) to 1 (right)
    boxalpha = 80,              -- alpha of the background box,
                                -- 0 (opaque) to 255 (fully transparent)
    hidetimeout = 500,          -- duration in ms until the OSC hides if no
                                -- mouse movement, negative value = disabled
    fadeduration = 200,         -- duration of fade out in ms, 0 = no fade
    deadzonesize = 0,           -- size of deadzone
    minmousemove = 3,           -- minimum amount of pixels the mouse has to
                                -- move between ticks to make the OSC show up
    iamaprogrammer = false,     -- use native mpv values and disable OSC
                                -- internal track list management (and some
                                -- functions that depend on it)
    layout = "box",
    seekbarstyle = "slider",    -- slider (diamond marker) or bar (fill)
}

-- read options from config and command-line
read_options(user_opts, "osc")

local osc_param = { -- calculated by osc_init()
    playresy = 0,                           -- canvas size Y
    playresx = 0,                           -- canvas size X
    display_aspect = 1,
    areas = {},
}

local osc_styles = {
    bigButtons = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs50\\fnmpv-osd-symbols}",
    smallButtonsL = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs20\\fnmpv-osd-symbols}",
    smallButtonsLlabel = "{\\fs17\\fn" .. mp.get_property("options/osd-font") .. "}",
    smallButtonsR = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs30\\fnmpv-osd-symbols}",
    topButtons = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs12\\fnmpv-osd-symbols}",

    elementDown = "{\\1c&H999999}",
    timecodes = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs20}",
    vidtitle = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs12}",
    box = "{\\rDefault\\blur0\\bord1\\1c&H000000\\3c&HFFFFFF}",
}

-- internal states, do not touch
local state = {
    showtime,                               -- time of last invocation (last mouse move)
    osc_visible = false,
    anistart,                               -- time when the animation started
    anitype,                                -- current type of animation
    animation,                              -- current animation alpha
    mouse_down_counter = 0,                 -- used for softrepeat
    active_element = nil,                   -- nil = none, 0 = background, 1+ = see elements[]
    active_event_source = nil,              -- the "button" that issued the current event
    rightTC_trem = true,                    -- if the right timcode should display total or remaining time
    tc_ms = false,                          -- Should the timecodes display their time with milliseconds
    mp_screen_sizeX, mp_screen_sizeY,       -- last screen-resolution, to detect resolution changes to issue reINITs
    initREQ = false,                        -- is a re-init request pending?
    last_mouseX, last_mouseY,               -- last mouse position, to detect siginificant mouse movement
    message_text,
    message_timeout,
    fullscreen = false,
    timer = nil,
    cache_idle = false,
    idle = false,
    enabled = true,
}




--
-- Helperfunctions
--

function scale_value(x0, x1, y0, y1, val)
    local m = (y1 - y0) / (x1 - x0)
    local b = y0 - (m * x0)
    return (m * val) + b
end

-- returns hitbox spanning coordinates (top left, bottom right corner)
-- according to alignment
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

function get_hitbox_coords_geo(geometry)
    return get_hitbox_coords(geometry.x, geometry.y, geometry.an,
        geometry.w, geometry.h)
end

function get_element_hitbox(element)
    return element.hitbox.x1, element.hitbox.y1,
        element.hitbox.x2, element.hitbox.y2
end

function mouse_hit(element)
    return mouse_hit_coords(get_element_hitbox(element))
end

function mouse_hit_coords(bX1, bY1, bX2, bY2)
    local mX, mY = mp.get_mouse_pos()
    return (mX >= bX1 and mX <= bX2 and mY >= bY1 and mY <= bY2)
end

function limit_range(min, max, val)
    if val > max then
        val = max
    elseif val < min then
        val = min
    end
    return val
end

-- translate value into element coordinates
function get_slider_ele_pos_for(element, val)

    local ele_pos = scale_value(
        element.slider.min.value, element.slider.max.value,
        element.slider.min.ele_pos, element.slider.max.ele_pos,
        val)

    return limit_range(
        element.slider.min.ele_pos, element.slider.max.ele_pos,
        ele_pos)
end

-- translates global (mouse) coordinates to value
function get_slider_value_at(element, glob_pos)

    local val = scale_value(
        element.slider.min.glob_pos, element.slider.max.glob_pos,
        element.slider.min.value, element.slider.max.value,
        glob_pos)

    return limit_range(
        element.slider.min.value, element.slider.max.value,
        val)
end

-- get value at current mouse position
function get_slider_value(element)
    return get_slider_value_at(element, mp.get_mouse_pos())
end

function countone(val)
    if not (user_opts.iamaprogrammer) then
        val = val + 1
    end
    return val
end

-- align:  -1 .. +1
-- frame:  size of the containing area
-- obj:    size of the object that should be positioned inside the area
-- margin: min. distance from object to frame (as long as -1 <= align <= +1)
function get_align(align, frame, obj, margin)
    return (frame / 2) + (((frame / 2) - margin - (obj / 2)) * align)
end

-- multiplies two alpha values, formular can probably be improved
function mult_alpha(alphaA, alphaB)
    return 255 - (((1-(alphaA/255)) * (1-(alphaB/255))) * 255)
end

function add_area(name, x1, y1, x2, y2)
    -- create area if needed
    if (osc_param.areas[name] == nil) then
        osc_param.areas[name] = {}
    end
    table.insert(osc_param.areas[name], {x1=x1, y1=y1, x2=x2, y2=y2})
end


--
-- Tracklist Management
--

local nicetypes = {video = "Video", audio = "Audio", sub = "Subtitle"}

-- updates the OSC internal playlists, should be run each time the track-layout changes
function update_tracklist()
    local tracktable = mp.get_property_native("track-list", {})

    -- by osc_id
    tracks_osc = {}
    tracks_osc.video, tracks_osc.audio, tracks_osc.sub = {}, {}, {}
    -- by mpv_id
    tracks_mpv = {}
    tracks_mpv.video, tracks_mpv.audio, tracks_mpv.sub = {}, {}, {}
    for n = 1, #tracktable do
        if not (tracktable[n].type == "unknown") then
            local type = tracktable[n].type
            local mpv_id = tonumber(tracktable[n].id)

            -- by osc_id
            table.insert(tracks_osc[type], tracktable[n])

            -- by mpv_id
            tracks_mpv[type][mpv_id] = tracktable[n]
            tracks_mpv[type][mpv_id].osc_id = #tracks_osc[type]
        end
    end
end

-- return a nice list of tracks of the given type (video, audio, sub)
function get_tracklist(type)
    local msg = "Available " .. nicetypes[type] .. " Tracks: "
    if #tracks_osc[type] == 0 then
        msg = msg .. "none"
    else
        for n = 1, #tracks_osc[type] do
            local track = tracks_osc[type][n]
            local lang, title, selected = "unknown", "", "○"
            if not(track.lang == nil) then lang = track.lang end
            if not(track.title == nil) then title = track.title end
            if (track.id == tonumber(mp.get_property(type))) then
                selected = "●"
            end
            msg = msg.."\n"..selected.." "..n..": ["..lang.."] "..title
        end
    end
    return msg
end

-- relatively change the track of given <type> by <next> tracks
    --(+1 -> next, -1 -> previous)
function set_track(type, next)
    local current_track_mpv, current_track_osc
    if (mp.get_property(type) == "no") then
        current_track_osc = 0
    else
        current_track_mpv = tonumber(mp.get_property(type))
        current_track_osc = tracks_mpv[type][current_track_mpv].osc_id
    end
    local new_track_osc = (current_track_osc + next) % (#tracks_osc[type] + 1)
    local new_track_mpv
    if new_track_osc == 0 then
        new_track_mpv = "no"
    else
        new_track_mpv = tracks_osc[type][new_track_osc].id
    end

    mp.commandv("set", type, new_track_mpv)

        if (new_track_osc == 0) then
        show_message(nicetypes[type] .. " Track: none")
    else
        show_message(nicetypes[type]  .. " Track: "
            .. new_track_osc .. "/" .. #tracks_osc[type]
            .. " [".. (tracks_osc[type][new_track_osc].lang or "unknown") .."] "
            .. (tracks_osc[type][new_track_osc].title or ""))
    end
end

-- get the currently selected track of <type>, OSC-style counted
function get_track(type)
    local track = mp.get_property(type)
    if track ~= "no" and track ~= nil then
        local tr = tracks_mpv[type][tonumber(track)]
        if tr then
            return tr.osc_id
        end
    end
    return 0
end


--
-- Element Management
--

local elements = {}

function prepare_elements()

    -- remove elements without layout or invisble
    local elements2 = {}
    for n, element in pairs(elements) do
        if not (element.layout == nil) and (element.visible) then
            table.insert(elements2, element)
        end
    end
    elements = elements2

    function elem_compare (a, b)
        return a.layout.layer < b.layout.layer
    end

    table.sort(elements, elem_compare)


    for _,element in pairs(elements) do

        local elem_geo = element.layout.geometry

        -- Calculate the hitbox
        local bX1, bY1, bX2, bY2 = get_hitbox_coords_geo(elem_geo)
        element.hitbox = {x1 = bX1, y1 = bY1, x2 = bX2, y2 = bY2}

        local style_ass = assdraw.ass_new()

        -- prepare static elements
        style_ass:append("{}") -- hack to troll new_event into inserting a \n
        style_ass:new_event()
        style_ass:pos(elem_geo.x, elem_geo.y)
        style_ass:an(elem_geo.an)
        style_ass:append(element.layout.style)

        element.style_ass = style_ass

        local static_ass = assdraw.ass_new()


        if (element.type == "box") then
            --draw box
            static_ass:draw_start()
            static_ass:round_rect_cw(0, 0, elem_geo.w, elem_geo.h,
                element.layout.box.radius)
            static_ass:draw_stop()


        elseif (element.type == "slider") then
            --draw static slider parts

            local slider_lo = element.layout.slider
            -- offset between element outline and drag-area
            local foV = slider_lo.border + slider_lo.gap

            -- calculate positions of min and max points
            if (slider_lo.stype == "slider") then
                element.slider.min.ele_pos = elem_geo.h / 2
                element.slider.max.ele_pos = elem_geo.w - (elem_geo.h / 2)

            elseif (slider_lo.stype == "bar") then
                element.slider.min.ele_pos =
                    slider_lo.border + slider_lo.gap
                element.slider.max.ele_pos =
                    elem_geo.w - (slider_lo.border + slider_lo.gap)
            end

            element.slider.min.glob_pos =
                element.hitbox.x1 + element.slider.min.ele_pos
            element.slider.max.glob_pos =
                element.hitbox.x1 + element.slider.max.ele_pos

            -- -- --

            static_ass:draw_start()

            -- the box
            static_ass:rect_cw(0, 0, elem_geo.w, elem_geo.h);

            -- the "hole"
            static_ass:rect_ccw(slider_lo.border, slider_lo.border,
                elem_geo.w - slider_lo.border, elem_geo.h - slider_lo.border)

            -- marker nibbles
            if not (element.slider.markerF == nil) and (slider_lo.gap > 0) then
                local markers = element.slider.markerF()
                for _,marker in pairs(markers) do
                    if (marker > element.slider.min.value) and
                        (marker < element.slider.max.value) then

                        local s = get_slider_ele_pos_for(element, marker)

                        if (slider_lo.gap > 1) then -- draw triangles

                            local a = slider_lo.gap / 0.5 --0.866

                            --top
                            if (slider_lo.nibbles_top) then
                                static_ass:move_to(s - (a/2), slider_lo.border)
                                static_ass:line_to(s + (a/2), slider_lo.border)
                                static_ass:line_to(s, foV)
                            end

                            --bottom
                            if (slider_lo.nibbles_bottom) then
                                static_ass:move_to(s - (a/2),
                                    elem_geo.h - slider_lo.border)
                                static_ass:line_to(s,
                                    elem_geo.h - foV)
                                static_ass:line_to(s + (a/2),
                                    elem_geo.h - slider_lo.border)
                            end

                        else -- draw 1px nibbles

                            --top
                            if (slider_lo.nibbles_top) then
                                static_ass:rect_cw(s - 0.5, slider_lo.gap,
                                    s + 0.5, slider_lo.gap*2);
                            end

                            --bottom
                            if (slider_lo.nibbles_bottom) then
                                static_ass:rect_cw(s - 0.5,
                                    elem_geo.h - slider_lo.gap*2,
                                    s + 0.5,
                                    elem_geo.h - slider_lo.gap);
                            end
                        end
                    end
                end
            end
        end

        element.static_ass = static_ass


        -- if the element is supposed to be disabled,
        -- style it accordingly and kill the eventresponders
        if not (element.enabled) then
            element.layout.alpha[1] = 136
            element.eventresponder = nil
        end
    end
end


--
-- Element Rendering
--

function render_elements(master_ass)

    for n=1, #elements do
        local element = elements[n]

        local style_ass = assdraw.ass_new()
        style_ass:merge(element.style_ass)

        --alpha
        local ar = element.layout.alpha
        if not (state.animation == nil) then
            ar = {}
            for ai, av in pairs(element.layout.alpha) do
                ar[ai] = mult_alpha(av, state.animation)
            end
        end

        style_ass:append(string.format("{\\1a&H%X&\\2a&H%X&\\3a&H%X&\\4a&H%X&}",
            ar[1], ar[2], ar[3], ar[4]))

        if element.eventresponder and (state.active_element == n) then

            -- run render event functions
            if not (element.eventresponder.render == nil) then
                element.eventresponder.render(element)
            end

            if mouse_hit(element) then
                -- mouse down styling
                if (element.styledown) then
                    style_ass:append(osc_styles.elementDown)
                end

                if (element.softrepeat) and (state.mouse_down_counter >= 15
                    and state.mouse_down_counter % 5 == 0) then

                    element.eventresponder[state.active_event_source.."_down"](element)
                end
                state.mouse_down_counter = state.mouse_down_counter + 1
            end

        end

        local elem_ass = assdraw.ass_new()

        elem_ass:merge(style_ass)

        if not (element.type == "button") then
            elem_ass:merge(element.static_ass)
        end



        if (element.type == "slider") then

            local slider_lo = element.layout.slider
            local elem_geo = element.layout.geometry
            local s_min, s_max = element.slider.min.value, element.slider.max.value


            -- draw pos marker
            local pos = element.slider.posF()

            if not (pos == nil) then

                local foV = slider_lo.border + slider_lo.gap
                local foH = 0
                if (slider_lo.stype == "slider") then
                    foH = elem_geo.h / 2
                elseif (slider_lo.stype == "bar") then
                    foH = slider_lo.border + slider_lo.gap
                end

                local xp = get_slider_ele_pos_for(element, pos)

                -- the filling
                local innerH = elem_geo.h - (2*foV)

                if (slider_lo.stype == "bar") then
                    elem_ass:rect_cw(foH, foV, xp, elem_geo.h - foV)
                elseif (slider_lo.stype == "slider") then
                    elem_ass:move_to(xp, foV)
                    elem_ass:line_to(xp+(innerH/2), (innerH/2)+foV)
                    elem_ass:line_to(xp, (innerH)+foV)
                    elem_ass:line_to(xp-(innerH/2), (innerH/2)+foV)
                end
            end

            elem_ass:draw_stop()

            -- add tooltip
            if not (element.slider.tooltipF == nil) then

                if mouse_hit(element) then
                    local sliderpos = get_slider_value(element)
                    local tooltiplabel = element.slider.tooltipF(sliderpos)

                    local an = slider_lo.tooltip_an

                    if (slider_lo.adjust_tooltip) then
                        if (sliderpos < (s_min + 5)) then
                            if an == 2 then
                                an = 1
                            else
                                an = 7
                            end
                        elseif (sliderpos > (s_max - 5)) then
                            if an == 2 then
                                an = 3
                            else
                                an = 9
                            end
                        end
                    end


                    local ty
                    if (slider_lo.tooltip_an == 2) then
                        ty = element.hitbox.y1 - slider_lo.border
                    else
                        ty = element.hitbox.y2 + slider_lo.border
                    end

                    elem_ass:new_event()
                    elem_ass:pos(mp.get_mouse_pos(), ty)
                    elem_ass:an(an)
                    elem_ass:append(slider_lo.tooltip_style)
                    elem_ass:append(tooltiplabel)

                end
            end

        elseif (element.type == "button") then

            local buttontext
            if type(element.content) == "function" then
                buttontext = element.content() -- function objects
            elseif not (element.content == nil) then
                buttontext = element.content -- text objects
            end

            local maxchars = element.layout.button.maxchars

            if not (maxchars == nil) and (#buttontext > maxchars) then
                buttontext = string.format("{\\fscx%f}",
                    (maxchars/#buttontext)*100) .. buttontext
            end

            elem_ass:append(buttontext)
        end

        master_ass:merge(elem_ass)
    end
end

--
-- Message display
--

function show_message(text, duration)

    --print("text: "..text.."   duration: " .. duration)
    if duration == nil then
        duration = tonumber(mp.get_property("options/osd-duration")) / 1000
    elseif not type(duration) == "number" then
        print("duration: " .. duration)
    end

    -- cut the text short, otherwise the following functions
    -- may slow down massively on huge input
    text = string.sub(text, 0, 4000)

    -- replace actual linebreaks with ASS linebreaks
    -- and get the amount of lines along the way
    local lines
    text, lines = string.gsub(text, "\n", "\\N")

    -- append a Zero-Width-Space to . and _ to enable
    -- linebreaking of long filenames
    text = string.gsub(text, "%.", ".\226\128\139")
    text = string.gsub(text, "_", "_\226\128\139")

    -- scale the fontsize for longer multi-line output
    local fontsize = tonumber(mp.get_property("options/osd-font-size"))
    local outline = tonumber(mp.get_property("options/osd-border-size"))

    if lines > 12 then
        fontsize, outline = fontsize / 2, outline / 1.5
    elseif lines > 8 then
        fontsize, outline = fontsize / 1.5, outline / 1.25
    end

    local style = "{\\bord" .. outline .. "\\fs" .. fontsize .. "}"

    state.message_text = style .. text
    state.message_timeout = mp.get_time() + duration
end

function render_message(ass)
    if not(state.message_timeout == nil) and not(state.message_text == nil)
        and state.message_timeout > mp.get_time() then

        ass:new_event()
        ass:append(state.message_text)
    else
        state.message_text = nil
        state.message_timeout = nil
    end
end

--
-- Initialisation and Layout
--

function new_element(name, type)
    elements[name] = {}
    elements[name].type = type

    -- add default stuff
    elements[name].eventresponder = {}
    elements[name].visible = true
    elements[name].enabled = true
    elements[name].softrepeat = false
    elements[name].styledown = (type == "button")
    elements[name].state = {}

    if (type == "slider") then
        elements[name].slider = {min = {value = 0}, max = {value = 100}}
    end


    return elements[name]
end

function add_layout(name)
    if not (elements[name] == nil) then
        -- new layout
        elements[name].layout = {}

        -- set layout defaults
        elements[name].layout.layer = 50
        elements[name].layout.alpha = {[1] = 0, [2] = 255, [3] = 255, [4] = 255}

        if (elements[name].type == "button") then
            elements[name].layout.button = {
                maxchars = nil,
            }
        elseif (elements[name].type == "slider") then
            -- slider defaults
            elements[name].layout.slider = {
                border = 1,
                gap = 1,
                nibbles_top = true,
                nibbles_bottom = true,
                stype = "slider",
                adjust_tooltip = true,
                tooltip_style = "",
                tooltip_an = 2,
            }
        elseif (elements[name].type == "box") then
            elements[name].layout.box = {radius = 0}
        end

        return elements[name].layout
    else
        msg.error("Can't add_layout to element \""..name.."\", doesn't exist.")
    end
end

--
-- Layouts
--

local layouts = {}

-- Classic box layout
layouts["box"] = function ()

    local osc_geo = {
        w = 550,    -- width
        h = 138,    -- height
        r = 10,     -- corner-radius
        p = 15,     -- padding
    }

    -- make sure the OSC actually fits into the video
    if (osc_param.playresx < (osc_geo.w + (2 * osc_geo.p))) then
        osc_param.playresy = (osc_geo.w+(2*osc_geo.p))/osc_param.display_aspect
        osc_param.playresx = osc_param.playresy * osc_param.display_aspect
    end

    -- position of the controller according to video aspect and valignment
    local posX = math.floor(get_align(user_opts.halign, osc_param.playresx,
        osc_geo.w, 0))
    local posY = math.floor(get_align(user_opts.valign, osc_param.playresy,
        osc_geo.h, 0))

    -- position offset for contents aligned at the borders of the box
    local pos_offsetX = (osc_geo.w - (2*osc_geo.p)) / 2
    local pos_offsetY = (osc_geo.h - (2*osc_geo.p)) / 2

    osc_param.areas = {} -- delete areas

    -- area for active mouse input
    add_area("input", get_hitbox_coords(posX, posY, 5, osc_geo.w, osc_geo.h))

    -- area for show/hide
    local sh_area_y0, sh_area_y1
    if user_opts.valign > 0 then
        -- deadzone above OSC
        sh_area_y0 = get_align(-1 + (2*user_opts.deadzonesize),
            posY - (osc_geo.h / 2), 0, 0)
        sh_area_y1 = osc_param.playresy
    else
        -- deadzone below OSC
        sh_area_y0 = 0
        sh_area_y1 = (posY + (osc_geo.h / 2)) +
            get_align(1 - (2*user_opts.deadzonesize),
            osc_param.playresy - (posY + (osc_geo.h / 2)), 0, 0)
    end
    add_area("showhide", 0, sh_area_y0, osc_param.playresx, sh_area_y1)

    -- fetch values
    local osc_w, osc_h, osc_r, osc_p =
        osc_geo.w, osc_geo.h, osc_geo.r, osc_geo.p

    local lo

    --
    -- Background box
    --

    new_element("bgbox", "box")
    lo = add_layout("bgbox")

    lo.geometry = {x = posX, y = posY, an = 5, w = osc_w, h = osc_h}
    lo.layer = 10
    lo.style = osc_styles.box
    lo.alpha[1] = user_opts.boxalpha
    lo.alpha[3] = user_opts.boxalpha
    lo.box.radius = osc_r

    --
    -- Title row
    --

    local titlerowY = posY - pos_offsetY - 10

    lo = add_layout("title")
    lo.geometry = {x = posX, y = titlerowY, an = 8, w = 496, h = 12}
    lo.style = osc_styles.vidtitle
    lo.button.maxchars = 90

    lo = add_layout("pl_prev")
    lo.geometry =
        {x = (posX - pos_offsetX), y = titlerowY, an = 7, w = 12, h = 12}
    lo.style = osc_styles.topButtons

    lo = add_layout("pl_next")
    lo.geometry =
        {x = (posX + pos_offsetX), y = titlerowY, an = 9, w = 12, h = 12}
    lo.style = osc_styles.topButtons

    --
    -- Big buttons
    --

    local bigbtnrowY = posY - pos_offsetY + 35
    local bigbtndist = 60

    lo = add_layout("playpause")
    lo.geometry =
        {x = posX, y = bigbtnrowY, an = 5, w = 40, h = 40}
    lo.style = osc_styles.bigButtons

    lo = add_layout("skipback")
    lo.geometry =
        {x = posX - bigbtndist, y = bigbtnrowY, an = 5, w = 40, h = 40}
    lo.style = osc_styles.bigButtons

    lo = add_layout("skipfrwd")
    lo.geometry =
        {x = posX + bigbtndist, y = bigbtnrowY, an = 5, w = 40, h = 40}
    lo.style = osc_styles.bigButtons

    lo = add_layout("ch_prev")
    lo.geometry =
        {x = posX - (bigbtndist * 2), y = bigbtnrowY, an = 5, w = 40, h = 40}
    lo.style = osc_styles.bigButtons

    lo = add_layout("ch_next")
    lo.geometry =
        {x = posX + (bigbtndist * 2), y = bigbtnrowY, an = 5, w = 40, h = 40}
    lo.style = osc_styles.bigButtons

    lo = add_layout("cy_audio")
    lo.geometry =
        {x = posX - pos_offsetX, y = bigbtnrowY, an = 1, w = 70, h = 18}
    lo.style = osc_styles.smallButtonsL

    lo = add_layout("cy_sub")
    lo.geometry =
        {x = posX - pos_offsetX, y = bigbtnrowY, an = 7, w = 70, h = 18}
    lo.style = osc_styles.smallButtonsL

    lo = add_layout("tog_fs")
    lo.geometry =
        {x = posX+pos_offsetX, y = bigbtnrowY, an = 6, w = 25, h = 25}
    lo.style = osc_styles.smallButtonsR

    --
    -- Seekbar
    --

    lo = add_layout("seekbar")
    lo.geometry =
        {x = posX, y = posY+pos_offsetY-22, an = 2, w = pos_offsetX*2, h = 15}
    lo.style = osc_styles.timecodes
    lo.slider.tooltip_style = osc_styles.vidtitle
    lo.slider.stype = user_opts["seekbarstyle"]

    --
    -- Timecodes + Cache
    --

    local bottomrowY = posY + pos_offsetY - 5

    lo = add_layout("tc_left")
    lo.geometry =
        {x = posX - pos_offsetX, y = bottomrowY, an = 4, w = 110, h = 18}
    lo.style = osc_styles.timecodes

    lo = add_layout("tc_right")
    lo.geometry =
        {x = posX + pos_offsetX, y = bottomrowY, an = 6, w = 110, h = 18}
    lo.style = osc_styles.timecodes

    lo = add_layout("cache")
    lo.geometry =
        {x = posX, y = bottomrowY, an = 5, w = 110, h = 18}
    lo.style = osc_styles.timecodes

end

-- slim box layout
layouts["slimbox"] = function ()

    local osc_geo = {
        w = 660,    -- width
        h = 70,     -- height
        r = 10,     -- corner-radius
    }

    -- make sure the OSC actually fits into the video
    if (osc_param.playresx < (osc_geo.w)) then
        osc_param.playresy = (osc_geo.w)/osc_param.display_aspect
        osc_param.playresx = osc_param.playresy * osc_param.display_aspect
    end

    -- position of the controller according to video aspect and valignment
    local posX = math.floor(get_align(user_opts.halign, osc_param.playresx,
        osc_geo.w, 0))
    local posY = math.floor(get_align(user_opts.valign, osc_param.playresy,
        osc_geo.h, 0))

    osc_param.areas = {} -- delete areas

    -- area for active mouse input
    add_area("input", get_hitbox_coords(posX, posY, 5, osc_geo.w, osc_geo.h))

    -- area for show/hide
    local sh_area_y0, sh_area_y1
    if user_opts.valign > 0 then
        -- deadzone above OSC
        sh_area_y0 = get_align(-1 + (2*user_opts.deadzonesize),
            posY - (osc_geo.h / 2), 0, 0)
        sh_area_y1 = osc_param.playresy
    else
        -- deadzone below OSC
        sh_area_y0 = 0
        sh_area_y1 = (posY + (osc_geo.h / 2)) +
            get_align(1 - (2*user_opts.deadzonesize),
            osc_param.playresy - (posY + (osc_geo.h / 2)), 0, 0)
    end
    add_area("showhide", 0, sh_area_y0, osc_param.playresx, sh_area_y1)

    local lo

    local tc_w, ele_h, inner_w = 100, 20, osc_geo.w - 100

    -- styles
    local styles = {
        box = "{\\rDefault\\blur0\\bord1\\1c&H000000\\3c&HFFFFFF}",
        timecodes = "{\\1c&HFFFFFF\\3c&H000000\\fs20\\bord2\\blur1}",
        tooltip = "{\\1c&HFFFFFF\\3c&H000000\\fs12\\bord1\\blur0.5}",
    }


    new_element("bgbox", "box")
    lo = add_layout("bgbox")

    lo.geometry = {x = posX, y = posY - 1, an = 2, w = inner_w, h = ele_h}
    lo.layer = 10
    lo.style = osc_styles.box
    lo.alpha[1] = user_opts.boxalpha
    lo.alpha[3] = 0
    lo.box.radius = osc_geo.r


    lo = add_layout("seekbar")
    lo.geometry =
        {x = posX, y = posY - 1, an = 2, w = inner_w, h = ele_h}
    lo.style = osc_styles.timecodes
    lo.slider.border = 0
    lo.slider.gap = 1.5
    lo.slider.tooltip_style = styles.tooltip
    lo.slider.stype = user_opts["seekbarstyle"]
    lo.slider.adjust_tooltip = false

    --
    -- Timecodes
    --

    lo = add_layout("tc_left")
    lo.geometry =
        {x = posX - (inner_w/2) + osc_geo.r, y = posY + 1,
        an = 7, w = tc_w, h = ele_h}
    lo.style = styles.timecodes
    lo.alpha[3] = user_opts.boxalpha

    lo = add_layout("tc_right")
    lo.geometry =
        {x = posX + (inner_w/2) - osc_geo.r, y = posY + 1,
        an = 9, w = tc_w, h = ele_h}
    lo.style = styles.timecodes
    lo.alpha[3] = user_opts.boxalpha

    -- Cache

    lo = add_layout("cache")
    lo.geometry =
        {x = posX, y = posY + 1,
        an = 8, w = tc_w, h = ele_h}
    lo.style = styles.timecodes
    lo.alpha[3] = user_opts.boxalpha


end

layouts["bottombar"] = function()
    local osc_geo = {
        x = -2,
        y = osc_param.playresy - 36,
        an = 7,
        w = osc_param.playresx + 4,
        h = 38,
    }

    local padX = 6
    local padY = 2

    osc_param.areas = {}

    add_area("input", get_hitbox_coords(osc_geo.x, osc_geo.y, osc_geo.an,
                                        osc_geo.w, osc_geo.h))

    local sh_area_y0, sh_area_y1
    sh_area_y0 = get_align(-1 + (2*user_opts.deadzonesize),
                           osc_geo.y - (osc_geo.h / 2), 0, 0)
    sh_area_y1 = osc_param.playresy
    add_area("showhide", 0, sh_area_y0, osc_param.playresx, sh_area_y1)

    local lo, geo

    -- Background bar
    new_element("bgbox", "box")
    lo = add_layout("bgbox")

    lo.geometry = osc_geo
    lo.layer = 10
    lo.style = osc_styles.box
    lo.alpha[1] = user_opts.boxalpha


    -- Playlist prev/next
    geo = { x = osc_geo.x + padX, y = osc_geo.y + padY, an = 7, w = 12, h = 12 }
    lo = add_layout("pl_prev")
    lo.geometry = geo
    lo.style = osc_styles.topButtons

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = 7, w = 12, h = 12 }
    lo = add_layout("pl_next")
    lo.geometry = geo
    lo.style = osc_styles.topButtons

    -- Title
    geo = { x = geo.x + geo.w + padX, y = geo.y, an = 7, w = 1000, h = 12 }
    lo = add_layout("title")
    lo.geometry = geo
    lo.style = osc_styles.vidtitle

    -- Cache
    geo = { x = osc_geo.x + osc_geo.w - padX, y = geo.y, an = 9,
            w = 100, h = 12 }
    lo = add_layout("cache")
    lo.geometry = geo
    lo.style = osc_styles.vidtitle


    -- Playback control buttons
    geo = { x = osc_geo.x + padX, y = geo.y + geo.h + padY, an = 7,
            w = 18, h = 18 }
    lo = add_layout("playpause")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsL

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = 7, w = geo.w, h = geo.h }
    lo = add_layout("ch_prev")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsL

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = 7, w = geo.w, h = geo.h }
    lo = add_layout("ch_next")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsL


    -- Left timecode
    geo = { x = geo.x + geo.w + padX + 100, y = geo.y, an = 9,
            w = 100, h = geo.h }
    lo = add_layout("tc_left")
    lo.geometry = geo
    lo.style = osc_styles.timecodes

    local sb_l = geo.x + padX


    -- Track selection buttons
    geo = { x = osc_geo.x + osc_geo.w - padX, y = geo.y, an = 9,
            w = 60, h = geo.h }
    lo = add_layout("cy_sub")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsL

    geo = { x = geo.x - geo.w - padX, y = geo.y, an = 9, w = geo.w, h = geo.h }
    lo = add_layout("cy_audio")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsL


    -- Right timecode
    geo = { x = geo.x - geo.w - padX - 100, y = geo.y, an = 7,
            w = 100, h = geo.h }
    lo = add_layout("tc_right")
    lo.geometry = geo
    lo.style = osc_styles.timecodes

    local sb_r = geo.x - padX


    -- Seekbar
    geo = {x = sb_l, y = osc_param.playresy, an = 1, w = sb_r - sb_l, h = geo.h}
    new_element("bgbar1", "box")
    lo = add_layout("bgbar1")

    lo.geometry = geo
    lo.layer = 15
    lo.style = osc_styles.timecodes
    lo.alpha[1] = math.min(255, user_opts.boxalpha + 140)

    lo = add_layout("seekbar")
    lo.geometry = geo
    lo.style = osc_styles.timecodes
    lo.layer = 16
    lo.slider.border = 0
    lo.slider.tooltip_style = osc_styles.vidtitle
    lo.slider.stype = user_opts["seekbarstyle"]
end

layouts["topbar"] = function()
    local osc_geo = {
        x = -2,
        y = 38,
        an = 1,
        w = osc_param.playresx + 4,
        h = 38,
    }

    local padX = 6
    local padY = 2

    osc_param.areas = {}

    add_area("input", get_hitbox_coords(osc_geo.x, osc_geo.y, osc_geo.an,
                                        osc_geo.w, osc_geo.h))

    local sh_area_y0, sh_area_y1
    sh_area_y0 = 0
    sh_area_y1 = (osc_geo.y + (osc_geo.h / 2)) +
                 get_align(1 - (2*user_opts.deadzonesize),
                 osc_param.playresy - (osc_geo.y + (osc_geo.h / 2)), 0, 0)
    add_area("showhide", 0, sh_area_y0, osc_param.playresx, sh_area_y1)

    local lo, geo

    -- Background bar
    new_element("bgbox", "box")
    lo = add_layout("bgbox")

    lo.geometry = osc_geo
    lo.layer = 10
    lo.style = osc_styles.box
    lo.alpha[1] = user_opts.boxalpha


    -- Playback control buttons
    geo = { x = osc_geo.x + padX, y = padY + 18, an = 1,
            w = 18, h = 18 }
    lo = add_layout("playpause")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsL

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = 1, w = geo.w, h = geo.h }
    lo = add_layout("ch_prev")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsL

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = 1, w = geo.w, h = geo.h }
    lo = add_layout("ch_next")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsL


    -- Left timecode
    geo = { x = geo.x + geo.w + padX + 100, y = geo.y, an = 3,
            w = 100, h = geo.h }
    lo = add_layout("tc_left")
    lo.geometry = geo
    lo.style = osc_styles.timecodes

    local sb_l = geo.x + padX


    -- Track selection buttons
    geo = { x = osc_geo.x + osc_geo.w - padX, y = geo.y, an = 3,
            w = 60, h = geo.h }
    lo = add_layout("cy_sub")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsL

    geo = { x = geo.x - geo.w - padX, y = geo.y, an = 3, w = geo.w, h = geo.h }
    lo = add_layout("cy_audio")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsL


    -- Right timecode
    geo = { x = geo.x - geo.w - padX - 100, y = geo.y, an = 1,
            w = 100, h = geo.h }
    lo = add_layout("tc_right")
    lo.geometry = geo
    lo.style = osc_styles.timecodes

    local sb_r = geo.x - padX


    -- Seekbar
    geo = { x = sb_l, y = 0, an = 7, w = sb_r - sb_l, h = geo.h }
    new_element("bgbar1", "box")
    lo = add_layout("bgbar1")

    lo.geometry = geo
    lo.layer = 15
    lo.style = osc_styles.timecodes
    lo.alpha[1] = math.min(255, user_opts.boxalpha + 140)

    lo = add_layout("seekbar")
    lo.geometry = geo
    lo.style = osc_styles.timecodes
    lo.layer = 16
    lo.slider.border = 0
    lo.slider.tooltip_style = osc_styles.vidtitle
    lo.slider.stype = user_opts["seekbarstyle"]
    lo.slider.tooltip_an = 8


    -- Playlist prev/next
    geo = { x = osc_geo.x + padX, y = osc_geo.h - padY, an = 1, w = 12, h = 12 }
    lo = add_layout("pl_prev")
    lo.geometry = geo
    lo.style = osc_styles.topButtons

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = 1, w = 12, h = 12 }
    lo = add_layout("pl_next")
    lo.geometry = geo
    lo.style = osc_styles.topButtons

    -- Title
    geo = { x = geo.x + geo.w + padX, y = geo.y, an = 1, w = 1000, h = 12 }
    lo = add_layout("title")
    lo.geometry = geo
    lo.style = osc_styles.vidtitle

    -- Cache
    geo = { x = osc_geo.x + osc_geo.w - padX, y = geo.y, an = 3,
            w = 100, h = 12 }
    lo = add_layout("cache")
    lo.geometry = geo
    lo.style = osc_styles.vidtitle
end

-- Validate string type user options
function validate_user_opts()
    if layouts[user_opts.layout] == nil then
        msg.warn("Invalid setting \""..user_opts.layout.."\" for layout")
        user_opts.layout = "box"
    end

    if user_opts.seekbarstyle ~= "slider" and
       user_opts.seekbarstyle ~= "bar" then
        msg.warn("Invalid setting \"" .. user_opts.seekbarstyle
            .. "\" for seekbarstyle")
        user_opts.seekbarstyle = "slider"
    end
end


-- OSC INIT
function osc_init()
    msg.debug("osc_init")

    -- set canvas resolution according to display aspect and scaling setting
    local baseResY = 720
    local display_w, display_h, display_aspect = mp.get_screen_size()
    local scale = 1

    if (mp.get_property("video") == "no") then -- dummy/forced window
        scale = user_opts.scaleforcedwindow
    elseif state.fullscreen then
        scale = user_opts.scalefullscreen
    else
        scale = user_opts.scalewindowed
    end

    if user_opts.vidscale then
        osc_param.playresy = baseResY / scale
    else
        osc_param.playresy = display_h / scale
    end
    osc_param.playresx = osc_param.playresy * display_aspect
    osc_param.display_aspect = display_aspect





    elements = {}

    -- some often needed stuff
    local pl_count = mp.get_property_number("playlist-count")
    local have_pl = (pl_count > 1)
    local have_ch = (mp.get_property_number("chapters", 0) > 0)

    local ne

    -- title
    ne = new_element("title", "button")

    ne.content = function ()
        local title = mp.get_property_osd("media-title")
        if not (title == nil) then
            return (title)
        else
            return ("mpv")
        end
    end

    ne.eventresponder["mouse_btn0_up"] = function ()
        local title = mp.get_property_osd("media-title")
        if (have_pl) then
            local pl_pos = countone(mp.get_property_number("playlist-pos"))
            title = "[" .. pl_pos .. "/" .. pl_count .. "] " .. title
        end
        show_message(title)
    end

    ne.eventresponder["mouse_btn2_up"] =
        function () show_message(mp.get_property_osd("filename")) end

    -- playlist buttons

    -- prev
    ne = new_element("pl_prev", "button")

    ne.content = "\238\132\144"
    ne.visible = have_pl
    ne.eventresponder["mouse_btn0_up"] =
        function () mp.commandv("playlist_prev", "weak") end
    ne.eventresponder["shift+mouse_btn0_up"] =
        function () show_message(mp.get_property_osd("playlist"), 3) end

    --next
    ne = new_element("pl_next", "button")

    ne.content = "\238\132\129"
    ne.visible = have_pl
    ne.eventresponder["mouse_btn0_up"] =
        function () mp.commandv("playlist_next", "weak") end
    ne.eventresponder["shift+mouse_btn0_up"] =
        function () show_message(mp.get_property_osd("playlist"), 3) end


    -- big buttons

    --playpause
    ne = new_element("playpause", "button")

    ne.content = function ()
        if mp.get_property("pause") == "yes" then
            return ("\238\132\129")
        else
            return ("\238\128\130")
        end
    end
    ne.eventresponder["mouse_btn0_up"] =
        function () mp.commandv("cycle", "pause") end

    --skipback
    ne = new_element("skipback", "button")

    ne.softrepeat = true
    ne.content = "\238\128\132"
    ne.eventresponder["mouse_btn0_down"] =
        function () mp.commandv("seek", -5, "relative", "keyframes") end
    ne.eventresponder["shift+mouse_btn0_down"] =
        function () mp.commandv("frame_back_step") end
    ne.eventresponder["mouse_btn2_down"] =
        function () mp.commandv("seek", -30, "relative", "keyframes") end

    --skipfrwd
    ne = new_element("skipfrwd", "button")

    ne.softrepeat = true
    ne.content = "\238\128\133"
    ne.eventresponder["mouse_btn0_down"] =
        function () mp.commandv("seek", 10, "relative", "keyframes") end
    ne.eventresponder["shift+mouse_btn0_down"] =
        function () mp.commandv("frame_step") end
    ne.eventresponder["mouse_btn2_down"] =
        function () mp.commandv("seek", 60, "relative", "keyframes") end

    --ch_prev
    ne = new_element("ch_prev", "button")

    ne.enabled = have_ch
    ne.content = "\238\132\132"
    ne.eventresponder["mouse_btn0_up"] =
        function () mp.commandv("osd-msg", "add", "chapter", -1) end
    ne.eventresponder["shift+mouse_btn0_up"] =
        function () show_message(mp.get_property_osd("chapter-list"), 3) end

    --ch_next
    ne = new_element("ch_next", "button")

    ne.enabled = have_ch
    ne.content = "\238\132\133"
    ne.eventresponder["mouse_btn0_up"] =
        function () mp.commandv("osd-msg", "add", "chapter", 1) end
    ne.eventresponder["shift+mouse_btn0_up"] =
        function () show_message(mp.get_property_osd("chapter-list"), 3) end

    --
    update_tracklist()

    --cy_audio
    ne = new_element("cy_audio", "button")

    ne.enabled = (#tracks_osc.audio > 0)
    ne.content = function ()
        local aid = "–"
        if not (get_track("audio") == 0) then
            aid = get_track("audio")
        end
        return ("\238\132\134" .. osc_styles.smallButtonsLlabel
            .. " " .. aid .. "/" .. #tracks_osc.audio)
    end
    ne.eventresponder["mouse_btn0_up"] =
        function () set_track("audio", 1) end
    ne.eventresponder["mouse_btn2_up"] =
        function () set_track("audio", -1) end
    ne.eventresponder["shift+mouse_btn0_down"] =
        function () show_message(get_tracklist("audio"), 2) end

    --cy_sub
    ne = new_element("cy_sub", "button")

    ne.enabled = (#tracks_osc.sub > 0)
    ne.content = function ()
        local sid = "–"
        if not (get_track("sub") == 0) then
            sid = get_track("sub")
        end
        return ("\238\132\135" .. osc_styles.smallButtonsLlabel
            .. " " .. sid .. "/" .. #tracks_osc.sub)
    end
    ne.eventresponder["mouse_btn0_up"] =
        function () set_track("sub", 1) end
    ne.eventresponder["mouse_btn2_up"] =
        function () set_track("sub", -1) end
    ne.eventresponder["shift+mouse_btn0_down"] =
        function () show_message(get_tracklist("sub"), 2) end

    --tog_fs
    ne = new_element("tog_fs", "button")
    ne.content = function ()
        if (state.fullscreen) then
            return ("\238\132\137")
        else
            return ("\238\132\136")
        end
    end
    ne.eventresponder["mouse_btn0_up"] =
        function () mp.commandv("cycle", "fullscreen") end


    --seekbar
    ne = new_element("seekbar", "slider")

    ne.enabled = not (mp.get_property("percent-pos") == nil)
    ne.slider.markerF = function ()
        local duration = mp.get_property_number("length", nil)
        if not (duration == nil) then
            local chapters = mp.get_property_native("chapter-list", {})
            local markers = {}
            for n = 1, #chapters do
                markers[n] = (chapters[n].time / duration * 100)
            end
            return markers
        else
            return {}
        end
    end
    ne.slider.posF =
        function () return mp.get_property_number("percent-pos", nil) end
    ne.slider.tooltipF = function (pos)
        local duration = mp.get_property_number("length", nil)
        if not ((duration == nil) or (pos == nil)) then
            possec = duration * (pos / 100)
            return mp.format_time(possec)
        else
            return ""
        end
    end
    ne.eventresponder["mouse_move"] = --keyframe seeking when mouse is dragged
        function (element)
            -- mouse move events may pile up during seeking and may still get
            -- sent when the user is done seeking, so we need to throw away
            -- identical seeks
            local seekto = get_slider_value(element)
            if (element.state.lastseek == nil) or
                (not (element.state.lastseek == seekto)) then
                    mp.commandv("seek", seekto,
                        "absolute-percent", "keyframes")
                    element.state.lastseek = seekto
            end

        end
    ne.eventresponder["mouse_btn0_down"] = --exact seeks on single clicks
        function (element) mp.commandv("seek", get_slider_value(element),
            "absolute-percent", "exact") end
    ne.eventresponder["reset"] =
        function (element) element.state.lastseek = nil end


    -- tc_left (current pos)
    ne = new_element("tc_left", "button")

    ne.content = function ()
        if (state.tc_ms) then
            return (mp.get_property_osd("playback-time/full"))
        else
            return (mp.get_property_osd("playback-time"))
        end
    end
    ne.eventresponder["mouse_btn0_up"] =
        function () state.tc_ms = not state.tc_ms end

    -- tc_right (total/remaining time)
    ne = new_element("tc_right", "button")

    ne.visible = (not (mp.get_property("length") == nil))
        and (mp.get_property_number("length") > 0)
    ne.content = function ()
        if (state.rightTC_trem) then
            if state.tc_ms then
                return ("-"..mp.get_property_osd("playtime-remaining/full"))
            else
                return ("-"..mp.get_property_osd("playtime-remaining"))
            end
        else
            if state.tc_ms then
                return (mp.get_property_osd("length/full"))
            else
                return (mp.get_property_osd("length"))
            end
        end
    end
    ne.eventresponder["mouse_btn0_up"] =
        function () state.rightTC_trem = not state.rightTC_trem end

    -- cache
    ne = new_element("cache", "button")

    ne.content = function ()
        local dmx_cache = mp.get_property_number("demuxer-cache-duration")
        if not (dmx_cache == nil) then
            dmx_cache = math.floor(dmx_cache + 0.5) .. "s + "
        else
            dmx_cache = ""
        end
        local cache_used = mp.get_property_number("cache-used")
        if not (cache_used == nil) then
            if (cache_used < 1024) then
                cache_used = cache_used .. " KB"
            else
                cache_used = math.floor((cache_used/102.4)+0.5)/10 .. " MB"
            end
            return ("Cache: " .. dmx_cache .. cache_used)
        else
            return ""
        end
    end



    -- load layout
    layouts[user_opts.layout]()

    --do something with the elements
    prepare_elements()

end



--
-- Other important stuff
--


function show_osc()
    msg.debug("show_osc")
    --remember last time of invocation (mouse move)
    state.showtime = mp.get_time()

    osc_visible(true)

    if (user_opts.fadeduration > 0) then
        state.anitype = nil
    end

end

function hide_osc()
    msg.debug("hide_osc")
    if (user_opts.fadeduration > 0) then
        if not(state.osc_visible == false) then
            state.anitype = "out"
            control_timer()
        end
    else
        osc_visible(false)
    end
end

function osc_visible(visible)
    state.osc_visible = visible
    control_timer()
end

function pause_state(name, enabled)
    state.paused = enabled
    control_timer()
end

function cache_state(name, idle)
    state.cache_idle = idle
    control_timer()
end

function control_timer()
    if (state.paused) and (state.osc_visible) and
        ( not(state.cache_idle) or not (state.anitype == nil) ) then

        timer_start()
    else
        timer_stop()
    end
end

function timer_start()
    if not (state.timer_active) then
        msg.debug("timer start")

        if (state.timer == nil) then
            -- create new timer
            state.timer = mp.add_periodic_timer(0.03, tick)
        else
            -- resume existing one
            state.timer:resume()
        end

        state.timer_active = true
    end
end

function timer_stop()
    if (state.timer_active) then
        msg.debug("timer stop")

        if not (state.timer == nil) then
            -- kill timer
            state.timer:kill()
        end

        state.timer_active = false
    end
end



function mouse_leave()
    hide_osc()
    -- reset mouse position
    state.last_mouseX, state.last_mouseY = nil, nil
end

function request_init()
    state.initREQ = true
end

function render()
    msg.debug("rendering")
    local current_screen_sizeX, current_screen_sizeY, aspect = mp.get_screen_size()
    local mouseX, mouseY = mp.get_mouse_pos()
    local now = mp.get_time()

    -- check if display changed, if so request reinit
    if not (state.mp_screen_sizeX == current_screen_sizeX
        and state.mp_screen_sizeY == current_screen_sizeY) then

        request_init()

        state.mp_screen_sizeX = current_screen_sizeX
        state.mp_screen_sizeY = current_screen_sizeY
    end

    -- init management
    if state.initREQ then
        osc_init()
        state.initREQ = false

        -- store initial mouse position
        if (state.last_mouseX == nil or state.last_mouseY == nil)
            and not (mouseX == nil or mouseY == nil) then

            state.last_mouseX, state.last_mouseY = mouseX, mouseY
        end
    end


    -- fade animation
    if not(state.anitype == nil) then

        if (state.anistart == nil) then
            state.anistart = now
        end

        if (now < state.anistart + (user_opts.fadeduration/1000)) then

            if (state.anitype == "in") then --fade in
                osc_visible(true)
                state.animation = scale_value(state.anistart,
                    (state.anistart + (user_opts.fadeduration/1000)),
                    255, 0, now)
            elseif (state.anitype == "out") then --fade out
                state.animation = scale_value(state.anistart,
                    (state.anistart + (user_opts.fadeduration/1000)),
                    0, 255, now)
            end

        else
            if (state.anitype == "out") then
                osc_visible(false)
            end
            state.anistart = nil
            state.animation = nil
            state.anitype =  nil
        end
    else
        state.anistart = nil
        state.animation = nil
        state.anitype =  nil
    end

    --mouse show/hide area
    for k,cords in pairs(osc_param.areas["showhide"]) do
        mp.set_mouse_area(cords.x1, cords.y1, cords.x2, cords.y2, "showhide")
    end
    do_enable_keybindings()

    --mouse input area
    local mouse_over_osc = false

    for _,cords in ipairs(osc_param.areas["input"]) do
        if state.osc_visible then -- activate only when OSC is actually visible
            mp.set_mouse_area(cords.x1, cords.y1, cords.x2, cords.y2, "input")
            mp.enable_key_bindings("input")
        else
            mp.disable_key_bindings("input")
        end

        if (mouse_hit_coords(cords.x1, cords.y1, cords.x2, cords.y2)) then
            mouse_over_osc = true
        end
    end

    -- autohide
    if not (state.showtime == nil) and (user_opts.hidetimeout >= 0)
        and (state.showtime + (user_opts.hidetimeout/1000) < now)
        and (state.active_element == nil) and not (mouse_over_osc) then

        hide_osc()
    end


    -- actual rendering
    local ass = assdraw.ass_new()

    -- Messages
    render_message(ass)

    -- actual OSC
    if state.osc_visible then
        render_elements(ass)
    end

    -- submit
    mp.set_osd_ass(osc_param.playresy * aspect, osc_param.playresy, ass.text)




end

--
-- Eventhandling
--

function process_event(source, what)

    if what == "down" then

        for n = 1, #elements do

            if not (elements[n].eventresponder == nil) then
                if not (elements[n].eventresponder[source .. "_up"] == nil)
                    or not (elements[n].eventresponder[source .. "_down"] == nil) then

                    if mouse_hit(elements[n]) then
                        state.active_element = n
                        state.active_event_source = source
                        -- fire the down event if the element has one
                        if not (elements[n].eventresponder[source .. "_" .. what] == nil) then
                            elements[n].eventresponder[source .. "_" .. what](elements[n])
                        end
                    end
                end

            end
        end

    elseif what == "up" then

        if not (state.active_element == nil) then

            local n = state.active_element

            if n == 0 then
                --click on background (does not work)
            elseif n > 0 and not (elements[n].eventresponder[source .. "_" .. what] == nil) then

                if mouse_hit(elements[n]) then
                    elements[n].eventresponder[source .. "_" .. what](elements[n])
                end
            end

            --reset active element
            if not (elements[n].eventresponder["reset"] == nil) then
                elements[n].eventresponder["reset"](elements[n])
            end

        end
        state.active_element = nil
        state.mouse_down_counter = 0

    elseif source == "mouse_move" then
        local mouseX, mouseY = mp.get_mouse_pos()
        if (user_opts.minmousemove == 0) or
            (not ((state.last_mouseX == nil) or (state.last_mouseY == nil)) and
                ((math.abs(mouseX - state.last_mouseX) >= user_opts.minmousemove)
                    or (math.abs(mouseY - state.last_mouseY) >= user_opts.minmousemove)
                )
            ) then
            show_osc()
        end
        state.last_mouseX, state.last_mouseY = mouseX, mouseY

        if not (state.active_element == nil) then

            local n = state.active_element

            if not (elements[n].eventresponder == nil) then
                if not (elements[n].eventresponder[source] == nil) then
                    elements[n].eventresponder[source](elements[n])
                end
            end
        end
        tick()
    end
end

-- called by mpv on every frame
function tick()
    if (state.idle) then

        -- render idle message
        msg.debug("idle message")
        local icon_x, icon_y = 320 - 26, 140

        local ass = assdraw.ass_new()
        ass:new_event()
        ass:pos(icon_x, icon_y)
        ass:append("{\\rDefault\\an7\\c&H430142&\\1a&H00&\\bord0\\shad0\\p6}m 1605 828 b 1605 1175 1324 1456 977 1456 631 1456 349 1175 349 828 349 482 631 200 977 200 1324 200 1605 482 1605 828{\\p0}")
        ass:new_event()
        ass:pos(icon_x, icon_y)
        ass:append("{\\rDefault\\an7\\c&HDDDBDD&\\1a&H00&\\bord0\\shad0\\p6}m 1296 910 b 1296 1131 1117 1310 897 1310 676 1310 497 1131 497 910 497 689 676 511 897 511 1117 511 1296 689 1296 910{\\p0}")
        ass:new_event()
        ass:pos(icon_x, icon_y)
        ass:append("{\\rDefault\\an7\\c&H691F69&\\1a&H00&\\bord0\\shad0\\p6}m 762 1113 l 762 708 b 881 776 1000 843 1119 911 1000 978 881 1046 762 1113{\\p0}")
        ass:new_event()
        ass:pos(icon_x, icon_y)
        ass:append("{\\rDefault\\an7\\c&H682167&\\1a&H00&\\bord0\\shad0\\p6}m 925 42 b 463 42 87 418 87 880 87 1343 463 1718 925 1718 1388 1718 1763 1343 1763 880 1763 418 1388 42 925 42 m 925 42 m 977 200 b 1324 200 1605 482 1605 828 1605 1175 1324 1456 977 1456 631 1456 349 1175 349 828 349 482 631 200 977 200{\\p0}")
        ass:new_event()
        ass:pos(icon_x, icon_y)
        ass:append("{\\rDefault\\an7\\c&H753074&\\1a&H00&\\bord0\\shad0\\p6}m 977 198 b 630 198 348 480 348 828 348 1176 630 1458 977 1458 1325 1458 1607 1176 1607 828 1607 480 1325 198 977 198 m 977 198 m 977 202 b 1323 202 1604 483 1604 828 1604 1174 1323 1454 977 1454 632 1454 351 1174 351 828 351 483 632 202 977 202{\\p0}")
        ass:new_event()
        ass:pos(icon_x, icon_y)
        ass:append("{\\rDefault\\an7\\c&HE5E5E5&\\1a&H00&\\bord0\\shad0\\p6}m 895 10 b 401 10 0 410 0 905 0 1399 401 1800 895 1800 1390 1800 1790 1399 1790 905 1790 410 1390 10 895 10 m 895 10 m 925 42 b 1388 42 1763 418 1763 880 1763 1343 1388 1718 925 1718 463 1718 87 1343 87 880 87 418 463 42 925 42{\\p0}")
        ass:new_event()
        ass:pos(320, icon_y+65)
        ass:an(8)
        ass:append("Drop files to play here.")
        mp.set_osd_ass(640, 360, ass.text)

        mp.disable_key_bindings("showhide")


    elseif (state.fullscreen and user_opts.showfullscreen)
        or (not state.fullscreen and user_opts.showwindowed) then

        -- render the OSC
        render()
    else
        -- Flush OSD
        mp.set_osd_ass(osc_param.playresy, osc_param.playresy, "")
    end
end

function do_enable_keybindings()
    if state.enabled then
        mp.enable_key_bindings("showhide", "allow-vo-dragging|allow-hide-cursor")
    end
end

function enable_osc(enable)
    state.enabled = enable
    if enable then
        do_enable_keybindings()
        show_osc()
    else
        hide_osc()
        mp.disable_key_bindings("showhide")
    end
end

validate_user_opts()

mp.register_event("tick", tick)
mp.register_event("start-file", request_init)
mp.register_event("tracks-changed", request_init)

mp.register_script_message("enable-osc", function() enable_osc(true) end)
mp.register_script_message("disable-osc", function() enable_osc(false) end)

mp.register_script_message("osc-message", show_message)

mp.observe_property("fullscreen", "bool",
    function(name, val)
        state.fullscreen = val
    end
)
mp.observe_property("idle", "bool",
    function(name, val)
        state.idle = val
        tick()
    end
)
mp.observe_property("pause", "bool", pause_state)
mp.observe_property("cache-idle", "bool", cache_state)

mp.observe_property("disc-menu-active", "bool", function(name, val)
    if val == true then
        hide_osc()
        mp.disable_key_bindings("showhide")
    else
        do_enable_keybindings()
    end
end)

-- mouse show/hide bindings
mp.set_key_bindings({
    {"mouse_move",              function(e) process_event("mouse_move", nil) end},
    {"mouse_leave",             mouse_leave},
}, "showhide", "force")
do_enable_keybindings()

--mouse input bindings
mp.set_key_bindings({
    {"mouse_btn0",              function(e) process_event("mouse_btn0", "up") end,
                                function(e) process_event("mouse_btn0", "down")  end},
    {"shift+mouse_btn0",        function(e) process_event("shift+mouse_btn0", "up") end,
                                function(e) process_event("shift+mouse_btn0", "down")  end},
    {"mouse_btn2",              function(e) process_event("mouse_btn2", "up") end,
                                function(e) process_event("mouse_btn2", "down")  end},
    {"mouse_btn0_dbl",          "ignore"},
    {"shift+mouse_btn0_dbl",    "ignore"},
    {"mouse_btn2_dbl",          "ignore"},
    {"del",                     function() enable_osc(false) end}
}, "input", "force")
mp.enable_key_bindings("input")
