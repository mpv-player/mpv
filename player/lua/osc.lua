local assdraw = require 'mp.assdraw'
local msg = require 'mp.msg'
local opt = require 'mp.options'
local utils = require 'mp.utils'

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
    barmargin = 0,              -- vertical margin of top/bottombar
    boxalpha = 80,              -- alpha of the background box,
                                -- 0 (opaque) to 255 (fully transparent)
    hidetimeout = 500,          -- duration in ms until the OSC hides if no
                                -- mouse movement. enforced non-negative for the
                                -- user, but internally negative is "always-on".
    fadeduration = 200,         -- duration of fade out in ms, 0 = no fade
    deadzonesize = 0.5,         -- size of deadzone
    minmousemove = 0,           -- minimum amount of pixels the mouse has to
                                -- move between ticks to make the OSC show up
    iamaprogrammer = false,     -- use native mpv values and disable OSC
                                -- internal track list management (and some
                                -- functions that depend on it)
    layout = "bottombar",
    seekbarstyle = "bar",       -- bar, diamond or knob
    seekbarhandlesize = 0.6,    -- size ratio of the diamond and knob handle
    seekrangestyle = "inverted",-- bar, line, slider, inverted or none
    seekrangeseparate = true,   -- wether the seekranges overlay on the bar-style seekbar
    seekrangealpha = 200,       -- transparency of seekranges
    seekbarkeyframes = true,    -- use keyframes when dragging the seekbar
    title = "${media-title}",   -- string compatible with property-expansion
                                -- to be shown as OSC title
    tooltipborder = 1,          -- border of tooltip in bottom/topbar
    timetotal = false,          -- display total time instead of remaining time?
    timems = false,             -- display timecodes with milliseconds?
    visibility = "auto",        -- only used at init to set visibility_mode(...)
    boxmaxchars = 80,           -- title crop threshold for box layout
    boxvideo = false,           -- apply osc_param.video_margins to video
    windowcontrols = "auto",    -- whether to show window controls
    windowcontrols_alignment = "right", -- which side to show window controls on
    greenandgrumpy = false,     -- disable santa hat
    livemarkers = true,         -- update seekbar chapter markers on duration change
}

-- read options from config and command-line
opt.read_options(user_opts, "osc", function(list) update_options(list) end)

local osc_param = { -- calculated by osc_init()
    playresy = 0,                           -- canvas size Y
    playresx = 0,                           -- canvas size X
    display_aspect = 1,
    unscaled_y = 0,
    areas = {},
    video_margins = {
        l = 0, r = 0, t = 0, b = 0,         -- left/right/top/bottom
    },
}

local osc_styles = {
    bigButtons = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs50\\fnmpv-osd-symbols}",
    smallButtonsL = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs19\\fnmpv-osd-symbols}",
    smallButtonsLlabel = "{\\fscx105\\fscy105\\fn" .. mp.get_property("options/osd-font") .. "}",
    smallButtonsR = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs30\\fnmpv-osd-symbols}",
    topButtons = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs12\\fnmpv-osd-symbols}",

    elementDown = "{\\1c&H999999}",
    timecodes = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs20}",
    vidtitle = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs12\\q2}",
    box = "{\\rDefault\\blur0\\bord1\\1c&H000000\\3c&HFFFFFF}",

    topButtonsBar = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs18\\fnmpv-osd-symbols}",
    smallButtonsBar = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs28\\fnmpv-osd-symbols}",
    timecodesBar = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs27}",
    timePosBar = "{\\blur0\\bord".. user_opts.tooltipborder .."\\1c&HFFFFFF\\3c&H000000\\fs30}",
    vidtitleBar = "{\\blur0\\bord0\\1c&HFFFFFF\\3c&HFFFFFF\\fs18\\q2}",

    wcButtons = "{\\1c&HFFFFFF\\fs24\\fnmpv-osd-symbols}",
    wcTitle = "{\\1c&HFFFFFF\\fs24\\q2}",
    wcBar = "{\\1c&H000000}",
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
    rightTC_trem = not user_opts.timetotal, -- if the right timecode should display total or remaining time
    tc_ms = user_opts.timems,               -- Should the timecodes display their time with milliseconds
    mp_screen_sizeX, mp_screen_sizeY,       -- last screen-resolution, to detect resolution changes to issue reINITs
    initREQ = false,                        -- is a re-init request pending?
    marginsREQ = false,                     -- is a margins update pending?
    last_mouseX, last_mouseY,               -- last mouse position, to detect significant mouse movement
    mouse_in_window = false,
    message_text,
    message_hide_timer,
    fullscreen = false,
    tick_timer = nil,
    tick_last_time = 0,                     -- when the last tick() was run
    hide_timer = nil,
    cache_state = nil,
    idle = false,
    enabled = true,
    input_enabled = true,
    showhide_enabled = false,
    dmx_cache = 0,
    using_video_margins = false,
    border = true,
    maximized = false,
    osd = mp.create_osd_overlay("ass-events"),
}

local window_control_box_width = 80
local tick_delay = 0.03

local is_december = os.date("*t").month == 12

--
-- Helperfunctions
--

function kill_animation()
    state.anistart = nil
    state.animation = nil
    state.anitype =  nil
end

function set_osd(res_x, res_y, text)
    if state.osd.res_x == res_x and
       state.osd.res_y == res_y and
       state.osd.data == text then
        return
    end
    state.osd.res_x = res_x
    state.osd.res_y = res_y
    state.osd.data = text
    state.osd.z = 1000
    state.osd:update()
end

local margins_opts = {
    {"l", "video-margin-ratio-left"},
    {"r", "video-margin-ratio-right"},
    {"t", "video-margin-ratio-top"},
    {"b", "video-margin-ratio-bottom"},
}

-- scale factor for translating between real and virtual ASS coordinates
function get_virt_scale_factor()
    local w, h = mp.get_osd_size()
    if w <= 0 or h <= 0 then
        return 0, 0
    end
    return osc_param.playresx / w, osc_param.playresy / h
end

-- return mouse position in virtual ASS coordinates (playresx/y)
function get_virt_mouse_pos()
    if state.mouse_in_window then
        local sx, sy = get_virt_scale_factor()
        local x, y = mp.get_mouse_pos()
        return x * sx, y * sy
    else
        return -1, -1
    end
end

function set_virt_mouse_area(x0, y0, x1, y1, name)
    local sx, sy = get_virt_scale_factor()
    mp.set_mouse_area(x0 / sx, y0 / sy, x1 / sx, y1 / sy, name)
end

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
    local mX, mY = get_virt_mouse_pos()
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
    return get_slider_value_at(element, get_virt_mouse_pos())
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

function ass_append_alpha(ass, alpha, modifier)
    local ar = {}

    for ai, av in pairs(alpha) do
        av = mult_alpha(av, modifier)
        if state.animation then
            av = mult_alpha(av, state.animation)
        end
        ar[ai] = av
    end

    ass:append(string.format("{\\1a&H%X&\\2a&H%X&\\3a&H%X&\\4a&H%X&}",
               ar[1], ar[2], ar[3], ar[4]))
end

function ass_draw_rr_h_cw(ass, x0, y0, x1, y1, r1, hexagon, r2)
    if hexagon then
        ass:hexagon_cw(x0, y0, x1, y1, r1, r2)
    else
        ass:round_rect_cw(x0, y0, x1, y1, r1, r2)
    end
end

function ass_draw_rr_h_ccw(ass, x0, y0, x1, y1, r1, hexagon, r2)
    if hexagon then
        ass:hexagon_ccw(x0, y0, x1, y1, r1, r2)
    else
        ass:round_rect_ccw(x0, y0, x1, y1, r1, r2)
    end
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

-- WindowControl helpers
function window_controls_enabled()
    val = user_opts.windowcontrols
    if val == "auto" then
        return not state.border
    else
        return val ~= "no"
    end
end

function window_controls_alignment()
    return user_opts.windowcontrols_alignment
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
            ass_draw_rr_h_cw(static_ass, 0, 0, elem_geo.w, elem_geo.h,
                             element.layout.box.radius, element.layout.box.hexagon)
            static_ass:draw_stop()

        elseif (element.type == "slider") then
            --draw static slider parts

            local r1 = 0
            local r2 = 0
            local slider_lo = element.layout.slider
            -- offset between element outline and drag-area
            local foV = slider_lo.border + slider_lo.gap

            -- calculate positions of min and max points
            if (slider_lo.stype ~= "bar") then
                r1 = elem_geo.h / 2
                element.slider.min.ele_pos = elem_geo.h / 2
                element.slider.max.ele_pos = elem_geo.w - (elem_geo.h / 2)
                if (slider_lo.stype == "diamond") then
                    r2 = (elem_geo.h - 2 * slider_lo.border) / 2
                elseif (slider_lo.stype == "knob") then
                    r2 = r1
                end
            else
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
            ass_draw_rr_h_cw(static_ass, 0, 0, elem_geo.w, elem_geo.h, r1, slider_lo.stype == "diamond")

            -- the "hole"
            ass_draw_rr_h_ccw(static_ass, slider_lo.border, slider_lo.border,
                              elem_geo.w - slider_lo.border, elem_geo.h - slider_lo.border,
                              r2, slider_lo.stype == "diamond")

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

                        else -- draw 2x1px nibbles

                            --top
                            if (slider_lo.nibbles_top) then
                                static_ass:rect_cw(s - 1, slider_lo.border,
                                    s + 1, slider_lo.border + slider_lo.gap);
                            end

                            --bottom
                            if (slider_lo.nibbles_bottom) then
                                static_ass:rect_cw(s - 1,
                                    elem_geo.h -slider_lo.border -slider_lo.gap,
                                    s + 1, elem_geo.h - slider_lo.border);
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

-- returns nil or a chapter element from the native property chapter-list
function get_chapter(possec)
    local cl = mp.get_property_native("chapter-list", {})
    local ch = nil

    -- chapters might not be sorted by time. find nearest-before/at possec
    for n=1, #cl do
        if possec >= cl[n].time and (not ch or cl[n].time > ch.time) then
            ch = cl[n]
        end
    end
    return ch
end

function render_elements(master_ass)

    -- when the slider is dragged or hovered and we have a target chapter name
    -- then we use it instead of the normal title. we calculate it before the
    -- render iterations because the title may be rendered before the slider.
    state.forced_title = nil
    local se, ae = state.slider_element, elements[state.active_element]
    if se and (ae == se or (not ae and mouse_hit(se))) then
        local dur = mp.get_property_number("duration", 0)
        if dur > 0 then
            local possec = get_slider_value(se) * dur / 100 -- of mouse pos
            local ch = get_chapter(possec)
            if ch and ch.title and ch.title ~= "" then
                state.forced_title = "Chapter: " .. ch.title
            end
        end
    end

    for n=1, #elements do
        local element = elements[n]

        local style_ass = assdraw.ass_new()
        style_ass:merge(element.style_ass)
        ass_append_alpha(style_ass, element.layout.alpha, 0)

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
            local s_min = element.slider.min.value
            local s_max = element.slider.max.value

            -- draw pos marker
            local foH, xp
            local pos = element.slider.posF()
            local foV = slider_lo.border + slider_lo.gap
            local innerH = elem_geo.h - (2 * foV)
            local seekRanges = element.slider.seekRangesF()
            local seekRangeLineHeight = innerH / 5

            if slider_lo.stype ~= "bar" then
                foH = elem_geo.h / 2
            else
                foH = slider_lo.border + slider_lo.gap
            end

            if pos then
                xp = get_slider_ele_pos_for(element, pos)

                if slider_lo.stype ~= "bar" then
                    local r = (user_opts.seekbarhandlesize * innerH) / 2
                    ass_draw_rr_h_cw(elem_ass, xp - r, foH - r,
                                     xp + r, foH + r,
                                     r, slider_lo.stype == "diamond")
                else
                    local h = 0
                    if seekRanges and user_opts.seekrangeseparate and slider_lo.rtype ~= "inverted" then
                        h = seekRangeLineHeight
                    end
                    elem_ass:rect_cw(foH, foV, xp, elem_geo.h - foV - h)

                    if seekRanges and not user_opts.seekrangeseparate and slider_lo.rtype ~= "inverted" then
                        -- Punch holes for the seekRanges to be drawn later
                        for _,range in pairs(seekRanges) do
                            if range["start"] < pos then
                                local pstart = get_slider_ele_pos_for(element, range["start"])
                                local pend = xp

                                if pos > range["end"] then
                                    pend = get_slider_ele_pos_for(element, range["end"])
                                end
                                elem_ass:rect_ccw(pstart, elem_geo.h - foV - seekRangeLineHeight, pend, elem_geo.h - foV)
                            end
                        end
                    end
                end

                if slider_lo.rtype == "slider" then
                    ass_draw_rr_h_cw(elem_ass, foH - innerH / 6, foH - innerH / 6,
                                     xp, foH + innerH / 6,
                                     innerH / 6, slider_lo.stype == "diamond", 0)
                    ass_draw_rr_h_cw(elem_ass, xp, foH - innerH / 15,
                                     elem_geo.w - foH + innerH / 15, foH + innerH / 15,
                                     0, slider_lo.stype == "diamond", innerH / 15)
                    for _,range in pairs(seekRanges or {}) do
                        local pstart = get_slider_ele_pos_for(element, range["start"])
                        local pend = get_slider_ele_pos_for(element, range["end"])
                        ass_draw_rr_h_ccw(elem_ass, pstart, foH - innerH / 21,
                                          pend, foH + innerH / 21,
                                          innerH / 21, slider_lo.stype == "diamond")
                    end
                end
            end

            if seekRanges then
                if slider_lo.rtype ~= "inverted" then
                    elem_ass:draw_stop()
                    elem_ass:merge(element.style_ass)
                    ass_append_alpha(elem_ass, element.layout.alpha, user_opts.seekrangealpha)
                    elem_ass:merge(element.static_ass)
                end

                for _,range in pairs(seekRanges) do
                    local pstart = get_slider_ele_pos_for(element, range["start"])
                    local pend = get_slider_ele_pos_for(element, range["end"])

                    if slider_lo.rtype == "slider" then
                        ass_draw_rr_h_cw(elem_ass, pstart, foH - innerH / 21,
                                         pend, foH + innerH / 21,
                                         innerH / 21, slider_lo.stype == "diamond")
                    elseif slider_lo.rtype == "line" then
                        if slider_lo.stype == "bar" then
                            elem_ass:rect_cw(pstart, elem_geo.h - foV - seekRangeLineHeight, pend, elem_geo.h - foV)
                        else
                            ass_draw_rr_h_cw(elem_ass, pstart - innerH / 8, foH - innerH / 8,
                                             pend + innerH / 8, foH + innerH / 8,
                                             innerH / 8, slider_lo.stype == "diamond")
                        end
                    elseif slider_lo.rtype == "bar" then
                        if slider_lo.stype ~= "bar" then
                            ass_draw_rr_h_cw(elem_ass, pstart - innerH / 2, foV,
                                             pend + innerH / 2, foV + innerH,
                                             innerH / 2, slider_lo.stype == "diamond")
                        elseif range["end"] >= (pos or 0) then
                            elem_ass:rect_cw(pstart, foV, pend, elem_geo.h - foV)
                        else
                            elem_ass:rect_cw(pstart, elem_geo.h - foV - seekRangeLineHeight, pend, elem_geo.h - foV)
                        end
                    elseif slider_lo.rtype == "inverted" then
                        if slider_lo.stype ~= "bar" then
                            ass_draw_rr_h_ccw(elem_ass, pstart, (elem_geo.h / 2) - 1, pend,
                                              (elem_geo.h / 2) + 1,
                                              1, slider_lo.stype == "diamond")
                        else
                            elem_ass:rect_ccw(pstart, (elem_geo.h / 2) - 1, pend, (elem_geo.h / 2) + 1)
                        end
                    end
                end
            end

            elem_ass:draw_stop()

            -- add tooltip
            if not (element.slider.tooltipF == nil) then

                if mouse_hit(element) then
                    local sliderpos = get_slider_value(element)
                    local tooltiplabel = element.slider.tooltipF(sliderpos)

                    local an = slider_lo.tooltip_an

                    local ty

                    if (an == 2) then
                        ty = element.hitbox.y1 - slider_lo.border
                    else
                        ty = element.hitbox.y1 + elem_geo.h/2
                    end

                    local tx = get_virt_mouse_pos()
                    if (slider_lo.adjust_tooltip) then
                        if (an == 2) then
                            if (sliderpos < (s_min + 3)) then
                                an = an - 1
                            elseif (sliderpos > (s_max - 3)) then
                                an = an + 1
                            end
                        elseif (sliderpos > (s_max-s_min)/2) then
                            an = an + 1
                            tx = tx - 5
                        else
                            an = an - 1
                            tx = tx + 10
                        end
                    end

                    -- tooltip label
                    elem_ass:new_event()
                    elem_ass:pos(tx, ty)
                    elem_ass:an(an)
                    elem_ass:append(slider_lo.tooltip_style)
                    ass_append_alpha(elem_ass, slider_lo.alpha, 0)
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
                local max_ratio = 1.25  -- up to 25% more chars while shrinking
                local limit = math.max(0, math.floor(maxchars * max_ratio) - 3)
                if (#buttontext > limit) then
                    while (#buttontext > limit) do
                        buttontext = buttontext:gsub(".[\128-\191]*$", "")
                    end
                    buttontext = buttontext .. "..."
                end
                local _, nchars2 = buttontext:gsub(".[\128-\191]*", "")
                local stretch = (maxchars/#buttontext)*100
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

-- pos is 1 based
function limited_list(prop, pos)
    local proplist = mp.get_property_native(prop, {})
    local count = #proplist
    if count == 0 then
        return count, proplist
    end

    local fs = tonumber(mp.get_property('options/osd-font-size'))
    local max = math.ceil(osc_param.unscaled_y*0.75 / fs)
    if max % 2 == 0 then
        max = max - 1
    end
    local delta = math.ceil(max / 2) - 1
    local begi = math.max(math.min(pos - delta, count - max + 1), 1)
    local endi = math.min(begi + max - 1, count)

    local reslist = {}
    for i=begi, endi do
        local item = proplist[i]
        item.current = (i == pos) and true or nil
        table.insert(reslist, item)
    end
    return count, reslist
end

function get_playlist()
    local pos = mp.get_property_number('playlist-pos', 0) + 1
    local count, limlist = limited_list('playlist', pos)
    if count == 0 then
        return 'Empty playlist.'
    end

    local message = string.format('Playlist [%d/%d]:\n', pos, count)
    for i, v in ipairs(limlist) do
        local title = v.title
        local _, filename = utils.split_path(v.filename)
        if title == nil then
            title = filename
        end
        message = string.format('%s %s %s\n', message,
            (v.current and '●' or '○'), title)
    end
    return message
end

function get_chapterlist()
    local pos = mp.get_property_number('chapter', 0) + 1
    local count, limlist = limited_list('chapter-list', pos)
    if count == 0 then
        return 'No chapters.'
    end

    local message = string.format('Chapters [%d/%d]:\n', pos, count)
    for i, v in ipairs(limlist) do
        local time = mp.format_time(v.time)
        local title = v.title
        if title == nil then
            title = string.format('Chapter %02d', i)
        end
        message = string.format('%s[%s] %s %s\n', message, time,
            (v.current and '●' or '○'), title)
    end
    return message
end

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
    text = string.gsub(text, "\n", "\\N")

    state.message_text = text

    if not state.message_hide_timer then
        state.message_hide_timer = mp.add_timeout(0, request_tick)
    end
    state.message_hide_timer:kill()
    state.message_hide_timer.timeout = duration
    state.message_hide_timer:resume()
    request_tick()
end

function render_message(ass)
    if state.message_hide_timer and state.message_hide_timer:is_enabled() and
       state.message_text
    then
        local _, lines = string.gsub(state.message_text, "\\N", "")

        local fontsize = tonumber(mp.get_property("options/osd-font-size"))
        local outline = tonumber(mp.get_property("options/osd-border-size"))
        local maxlines = math.ceil(osc_param.unscaled_y*0.75 / fontsize)
        local counterscale = osc_param.playresy / osc_param.unscaled_y

        fontsize = fontsize * counterscale / math.max(0.65 + math.min(lines/maxlines, 1), 1)
        outline = outline * counterscale / math.max(0.75 + math.min(lines/maxlines, 1)/2, 1)

        local style = "{\\bord" .. outline .. "\\fs" .. fontsize .. "}"


        ass:new_event()
        ass:append(style .. state.message_text)
    else
        state.message_text = nil
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
                alpha = {[1] = 0, [2] = 255, [3] = 88, [4] = 255},
            }
        elseif (elements[name].type == "box") then
            elements[name].layout.box = {radius = 0, hexagon = false}
        end

        return elements[name].layout
    else
        msg.error("Can't add_layout to element \""..name.."\", doesn't exist.")
    end
end

-- Window Controls
function window_controls(topbar)
    local wc_geo = {
        x = 0,
        y = 30 + user_opts.barmargin,
        an = 1,
        w = osc_param.playresx,
        h = 30,
    }

    local alignment = window_controls_alignment()
    local controlbox_w = window_control_box_width
    local titlebox_w = wc_geo.w - controlbox_w

    -- Default alignment is "right"
    local controlbox_left = wc_geo.w - controlbox_w
    local titlebox_left = wc_geo.x
    local titlebox_right = wc_geo.w - controlbox_w

    if alignment == "left" then
        controlbox_left = wc_geo.x
        titlebox_left = wc_geo.x + controlbox_w
        titlebox_right = wc_geo.w
    end

    add_area("window-controls",
             get_hitbox_coords(controlbox_left, wc_geo.y, wc_geo.an,
                               controlbox_w, wc_geo.h))

    local lo

    -- Background Bar
    new_element("wcbar", "box")
    lo = add_layout("wcbar")
    lo.geometry = wc_geo
    lo.layer = 10
    lo.style = osc_styles.wcBar
    lo.alpha[1] = user_opts.boxalpha

    local button_y = wc_geo.y - (wc_geo.h / 2)
    local first_geo =
        {x = controlbox_left + 5, y = button_y, an = 4, w = 25, h = 25}
    local second_geo =
        {x = controlbox_left + 30, y = button_y, an = 4, w = 25, h = 25}
    local third_geo =
        {x = controlbox_left + 55, y = button_y, an = 4, w = 25, h = 25}

    -- Window control buttons use symbols in the custom mpv osd font
    -- because the official unicode codepoints are sufficiently
    -- exotic that a system might lack an installed font with them,
    -- and libass will complain that they are not present in the
    -- default font, even if another font with them is available.

    -- Close: 🗙
    ne = new_element("close", "button")
    ne.content = "\238\132\149"
    ne.eventresponder["mbtn_left_up"] =
        function () mp.commandv("quit") end
    lo = add_layout("close")
    lo.geometry = alignment == "left" and first_geo or third_geo
    lo.style = osc_styles.wcButtons

    -- Minimize: 🗕
    ne = new_element("minimize", "button")
    ne.content = "\238\132\146"
    ne.eventresponder["mbtn_left_up"] =
        function () mp.commandv("cycle", "window-minimized") end
    lo = add_layout("minimize")
    lo.geometry = alignment == "left" and second_geo or first_geo
    lo.style = osc_styles.wcButtons

    -- Maximize: 🗖 /🗗
    ne = new_element("maximize", "button")
    if state.maximized or state.fullscreen then
        ne.content = "\238\132\148"
    else
        ne.content = "\238\132\147"
    end
    ne.eventresponder["mbtn_left_up"] =
        function ()
            if state.fullscreen then
                mp.commandv("cycle", "fullscreen")
            else
                mp.commandv("cycle", "window-maximized")
            end
        end
    lo = add_layout("maximize")
    lo.geometry = alignment == "left" and third_geo or second_geo
    lo.style = osc_styles.wcButtons

    -- deadzone below window controls
    local sh_area_y0, sh_area_y1
    sh_area_y0 = user_opts.barmargin
    sh_area_y1 = (wc_geo.y + (wc_geo.h / 2)) +
                 get_align(1 - (2 * user_opts.deadzonesize),
                 osc_param.playresy - (wc_geo.y + (wc_geo.h / 2)), 0, 0)
    add_area("showhide_wc", wc_geo.x, sh_area_y0, wc_geo.w, sh_area_y1)

    if topbar then
        -- The title is already there as part of the top bar
        return
    else
        -- Apply boxvideo margins to the control bar
        osc_param.video_margins.t = wc_geo.h / osc_param.playresy
    end

    -- Window Title
    ne = new_element("wctitle", "button")
    ne.content = function ()
        local title = mp.command_native({"expand-text", user_opts.title})
        -- escape ASS, and strip newlines and trailing slashes
        title = title:gsub("\\n", " "):gsub("\\$", ""):gsub("{","\\{")
        return not (title == "") and title or "mpv"
    end
    local left_pad = 5
    local right_pad = 10
    lo = add_layout("wctitle")
    lo.geometry =
        { x = titlebox_left + left_pad, y = wc_geo.y - 3, an = 1,
          w = titlebox_w, h = wc_geo.h }
    lo.style = string.format("%s{\\clip(%f,%f,%f,%f)}",
        osc_styles.wcTitle,
        titlebox_left + left_pad, wc_geo.y - wc_geo.h,
        titlebox_right - right_pad , wc_geo.y + wc_geo.h)

    add_area("window-controls-title",
             titlebox_left, 0, titlebox_right, wc_geo.h)
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
    lo.button.maxchars = user_opts.boxmaxchars

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
        {x = posX+pos_offsetX - 25, y = bigbtnrowY, an = 4, w = 25, h = 25}
    lo.style = osc_styles.smallButtonsR

    lo = add_layout("volume")
    lo.geometry =
        {x = posX+pos_offsetX - (25 * 2) - osc_geo.p,
         y = bigbtnrowY, an = 4, w = 25, h = 25}
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
    lo.slider.rtype = user_opts["seekrangestyle"]

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
    if not (user_opts["seekbarstyle"] == "bar") then
        lo.box.radius = osc_geo.r
        lo.box.hexagon = user_opts["seekbarstyle"] == "diamond"
    end


    lo = add_layout("seekbar")
    lo.geometry =
        {x = posX, y = posY - 1, an = 2, w = inner_w, h = ele_h}
    lo.style = osc_styles.timecodes
    lo.slider.border = 0
    lo.slider.gap = 1.5
    lo.slider.tooltip_style = styles.tooltip
    lo.slider.stype = user_opts["seekbarstyle"]
    lo.slider.rtype = user_opts["seekrangestyle"]
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

function bar_layout(direction)
    local osc_geo = {
        x = -2,
        y,
        an = (direction < 0) and 7 or 1,
        w,
        h = 56,
    }

    local padX = 9
    local padY = 3
    local buttonW = 27
    local tcW = (state.tc_ms) and 170 or 110
    local tsW = 90
    local minW = (buttonW + padX)*5 + (tcW + padX)*4 + (tsW + padX)*2

    -- Special topbar handling when window controls are present
    local padwc_l
    local padwc_r
    if direction < 0 or not window_controls_enabled() then
        padwc_l = 0
        padwc_r = 0
    elseif window_controls_alignment() == "left" then
        padwc_l = window_control_box_width
        padwc_r = 0
    else
        padwc_l = 0
        padwc_r = window_control_box_width
    end

    if ((osc_param.display_aspect > 0) and (osc_param.playresx < minW)) then
        osc_param.playresy = minW / osc_param.display_aspect
        osc_param.playresx = osc_param.playresy * osc_param.display_aspect
    end

    osc_geo.y = direction * (54 + user_opts.barmargin)
    osc_geo.w = osc_param.playresx + 4
    if direction < 0 then
        osc_geo.y = osc_geo.y + osc_param.playresy
    end

    local line1 = osc_geo.y - direction * (9 + padY)
    local line2 = osc_geo.y - direction * (36 + padY)

    osc_param.areas = {}

    add_area("input", get_hitbox_coords(osc_geo.x, osc_geo.y, osc_geo.an,
                                        osc_geo.w, osc_geo.h))

    local sh_area_y0, sh_area_y1
    if direction > 0 then
        -- deadzone below OSC
        sh_area_y0 = user_opts.barmargin
        sh_area_y1 = (osc_geo.y + (osc_geo.h / 2)) +
                     get_align(1 - (2*user_opts.deadzonesize),
                     osc_param.playresy - (osc_geo.y + (osc_geo.h / 2)), 0, 0)
    else
        -- deadzone above OSC
        sh_area_y0 = get_align(-1 + (2*user_opts.deadzonesize),
                               osc_geo.y - (osc_geo.h / 2), 0, 0)
        sh_area_y1 = osc_param.playresy - user_opts.barmargin
    end
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
    geo = { x = osc_geo.x + padX, y = line1,
            an = 4, w = 18, h = 18 - padY }
    lo = add_layout("pl_prev")
    lo.geometry = geo
    lo.style = osc_styles.topButtonsBar

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("pl_next")
    lo.geometry = geo
    lo.style = osc_styles.topButtonsBar

    local t_l = geo.x + geo.w + padX

    -- Cache
    geo = { x = osc_geo.x + osc_geo.w - padX, y = geo.y,
            an = 6, w = 150, h = geo.h }
    lo = add_layout("cache")
    lo.geometry = geo
    lo.style = osc_styles.vidtitleBar

    local t_r = geo.x - geo.w - padX*2

    -- Title
    geo = { x = t_l, y = geo.y, an = 4,
            w = t_r - t_l, h = geo.h }
    lo = add_layout("title")
    lo.geometry = geo
    lo.style = string.format("%s{\\clip(%f,%f,%f,%f)}",
        osc_styles.vidtitleBar,
        geo.x, geo.y-geo.h, geo.w, geo.y+geo.h)


    -- Playback control buttons
    geo = { x = osc_geo.x + padX + padwc_l, y = line2, an = 4,
            w = buttonW, h = 36 - padY*2}
    lo = add_layout("playpause")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("ch_prev")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("ch_next")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    -- Left timecode
    geo = { x = geo.x + geo.w + padX + tcW, y = geo.y, an = 6,
            w = tcW, h = geo.h }
    lo = add_layout("tc_left")
    lo.geometry = geo
    lo.style = osc_styles.timecodesBar

    local sb_l = geo.x + padX

    -- Fullscreen button
    geo = { x = osc_geo.x + osc_geo.w - buttonW - padX - padwc_r, y = geo.y, an = 4,
            w = buttonW, h = geo.h }
    lo = add_layout("tog_fs")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    -- Volume
    geo = { x = geo.x - geo.w - padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("volume")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    -- Track selection buttons
    geo = { x = geo.x - tsW - padX, y = geo.y, an = geo.an, w = tsW, h = geo.h }
    lo = add_layout("cy_sub")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    geo = { x = geo.x - geo.w - padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("cy_audio")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar


    -- Right timecode
    geo = { x = geo.x - padX - tcW - 10, y = geo.y, an = geo.an,
            w = tcW, h = geo.h }
    lo = add_layout("tc_right")
    lo.geometry = geo
    lo.style = osc_styles.timecodesBar

    local sb_r = geo.x - padX


    -- Seekbar
    geo = { x = sb_l, y = geo.y, an = geo.an,
            w = math.max(0, sb_r - sb_l), h = geo.h }
    new_element("bgbar1", "box")
    lo = add_layout("bgbar1")

    lo.geometry = geo
    lo.layer = 15
    lo.style = osc_styles.timecodesBar
    lo.alpha[1] =
        math.min(255, user_opts.boxalpha + (255 - user_opts.boxalpha)*0.8)
    if not (user_opts["seekbarstyle"] == "bar") then
        lo.box.radius = geo.h / 2
        lo.box.hexagon = user_opts["seekbarstyle"] == "diamond"
    end

    lo = add_layout("seekbar")
    lo.geometry = geo
    lo.style = osc_styles.timecodesBar
    lo.slider.border = 0
    lo.slider.gap = 2
    lo.slider.tooltip_style = osc_styles.timePosBar
    lo.slider.tooltip_an = 5
    lo.slider.stype = user_opts["seekbarstyle"]
    lo.slider.rtype = user_opts["seekrangestyle"]

    if direction < 0 then
        osc_param.video_margins.b = osc_geo.h / osc_param.playresy
    else
        osc_param.video_margins.t = osc_geo.h / osc_param.playresy
    end
end

layouts["bottombar"] = function()
    bar_layout(-1)
end

layouts["topbar"] = function()
    bar_layout(1)
end

-- Validate string type user options
function validate_user_opts()
    if layouts[user_opts.layout] == nil then
        msg.warn("Invalid setting \""..user_opts.layout.."\" for layout")
        user_opts.layout = "bottombar"
    end

    if user_opts.seekbarstyle ~= "bar" and
       user_opts.seekbarstyle ~= "diamond" and
       user_opts.seekbarstyle ~= "knob" then
        msg.warn("Invalid setting \"" .. user_opts.seekbarstyle
            .. "\" for seekbarstyle")
        user_opts.seekbarstyle = "bar"
    end

    if user_opts.seekrangestyle ~= "bar" and
       user_opts.seekrangestyle ~= "line" and
       user_opts.seekrangestyle ~= "slider" and
       user_opts.seekrangestyle ~= "inverted" and
       user_opts.seekrangestyle ~= "none" then
        msg.warn("Invalid setting \"" .. user_opts.seekrangestyle
            .. "\" for seekrangestyle")
        user_opts.seekrangestyle = "inverted"
    end

    if user_opts.seekrangestyle == "slider" and
       user_opts.seekbarstyle == "bar" then
        msg.warn("Using \"slider\" seekrangestyle together with \"bar\" seekbarstyle is not supported")
        user_opts.seekrangestyle = "inverted"
    end

    if user_opts.windowcontrols ~= "auto" and
       user_opts.windowcontrols ~= "yes" and
       user_opts.windowcontrols ~= "no" then
        msg.warn("windowcontrols cannot be \"" ..
                user_opts.windowcontrols .. "\". Ignoring.")
        user_opts.windowcontrols = "auto"
    end
    if user_opts.windowcontrols_alignment ~= "right" and
       user_opts.windowcontrols_alignment ~= "left" then
        msg.warn("windowcontrols_alignment cannot be \"" ..
                user_opts.windowcontrols_alignment .. "\". Ignoring.")
        user_opts.windowcontrols_alignment = "right"
    end
end

function update_options(list)
    validate_user_opts()
    request_tick()
    visibility_mode(user_opts.visibility, true)
    update_duration_watch()
    request_init()
end

-- OSC INIT
function osc_init()
    msg.debug("osc_init")

    -- set canvas resolution according to display aspect and scaling setting
    local baseResY = 720
    local display_w, display_h, display_aspect = mp.get_osd_size()
    local scale = 1

    if (mp.get_property("video") == "no") then -- dummy/forced window
        scale = user_opts.scaleforcedwindow
    elseif state.fullscreen then
        scale = user_opts.scalefullscreen
    else
        scale = user_opts.scalewindowed
    end

    if user_opts.vidscale then
        osc_param.unscaled_y = baseResY
    else
        osc_param.unscaled_y = display_h
    end
    osc_param.playresy = osc_param.unscaled_y / scale
    if (display_aspect > 0) then
        osc_param.display_aspect = display_aspect
    end
    osc_param.playresx = osc_param.playresy * osc_param.display_aspect

    -- stop seeking with the slider to prevent skipping files
    state.active_element = nil

    osc_param.video_margins = {l = 0, r = 0, t = 0, b = 0}

    elements = {}

    -- some often needed stuff
    local pl_count = mp.get_property_number("playlist-count", 0)
    local have_pl = (pl_count > 1)
    local pl_pos = mp.get_property_number("playlist-pos", 0) + 1
    local have_ch = (mp.get_property_number("chapters", 0) > 0)
    local loop = mp.get_property("loop-playlist", "no")

    local ne

    -- title
    ne = new_element("title", "button")

    ne.content = function ()
        local title = state.forced_title or
                      mp.command_native({"expand-text", user_opts.title})
        -- escape ASS, and strip newlines and trailing slashes
        title = title:gsub("\\n", " "):gsub("\\$", ""):gsub("{","\\{")
        return not (title == "") and title or "mpv"
    end

    ne.eventresponder["mbtn_left_up"] = function ()
        local title = mp.get_property_osd("media-title")
        if (have_pl) then
            title = string.format("[%d/%d] %s", countone(pl_pos - 1),
                                  pl_count, title)
        end
        show_message(title)
    end

    ne.eventresponder["mbtn_right_up"] =
        function () show_message(mp.get_property_osd("filename")) end

    -- playlist buttons

    -- prev
    ne = new_element("pl_prev", "button")

    ne.content = "\238\132\144"
    ne.enabled = (pl_pos > 1) or (loop ~= "no")
    ne.eventresponder["mbtn_left_up"] =
        function ()
            mp.commandv("playlist-prev", "weak")
            show_message(get_playlist(), 3)
        end
    ne.eventresponder["shift+mbtn_left_up"] =
        function () show_message(get_playlist(), 3) end
    ne.eventresponder["mbtn_right_up"] =
        function () show_message(get_playlist(), 3) end

    --next
    ne = new_element("pl_next", "button")

    ne.content = "\238\132\129"
    ne.enabled = (have_pl and (pl_pos < pl_count)) or (loop ~= "no")
    ne.eventresponder["mbtn_left_up"] =
        function ()
            mp.commandv("playlist-next", "weak")
            show_message(get_playlist(), 3)
        end
    ne.eventresponder["shift+mbtn_left_up"] =
        function () show_message(get_playlist(), 3) end
    ne.eventresponder["mbtn_right_up"] =
        function () show_message(get_playlist(), 3) end


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
    ne.eventresponder["mbtn_left_up"] =
        function () mp.commandv("cycle", "pause") end

    --skipback
    ne = new_element("skipback", "button")

    ne.softrepeat = true
    ne.content = "\238\128\132"
    ne.eventresponder["mbtn_left_down"] =
        function () mp.commandv("seek", -5, "relative", "keyframes") end
    ne.eventresponder["shift+mbtn_left_down"] =
        function () mp.commandv("frame-back-step") end
    ne.eventresponder["mbtn_right_down"] =
        function () mp.commandv("seek", -30, "relative", "keyframes") end

    --skipfrwd
    ne = new_element("skipfrwd", "button")

    ne.softrepeat = true
    ne.content = "\238\128\133"
    ne.eventresponder["mbtn_left_down"] =
        function () mp.commandv("seek", 10, "relative", "keyframes") end
    ne.eventresponder["shift+mbtn_left_down"] =
        function () mp.commandv("frame-step") end
    ne.eventresponder["mbtn_right_down"] =
        function () mp.commandv("seek", 60, "relative", "keyframes") end

    --ch_prev
    ne = new_element("ch_prev", "button")

    ne.enabled = have_ch
    ne.content = "\238\132\132"
    ne.eventresponder["mbtn_left_up"] =
        function ()
            mp.commandv("add", "chapter", -1)
            show_message(get_chapterlist(), 3)
        end
    ne.eventresponder["shift+mbtn_left_up"] =
        function () show_message(get_chapterlist(), 3) end
    ne.eventresponder["mbtn_right_up"] =
        function () show_message(get_chapterlist(), 3) end

    --ch_next
    ne = new_element("ch_next", "button")

    ne.enabled = have_ch
    ne.content = "\238\132\133"
    ne.eventresponder["mbtn_left_up"] =
        function ()
            mp.commandv("add", "chapter", 1)
            show_message(get_chapterlist(), 3)
        end
    ne.eventresponder["shift+mbtn_left_up"] =
        function () show_message(get_chapterlist(), 3) end
    ne.eventresponder["mbtn_right_up"] =
        function () show_message(get_chapterlist(), 3) end

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
    ne.eventresponder["mbtn_left_up"] =
        function () set_track("audio", 1) end
    ne.eventresponder["mbtn_right_up"] =
        function () set_track("audio", -1) end
    ne.eventresponder["shift+mbtn_left_down"] =
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
    ne.eventresponder["mbtn_left_up"] =
        function () set_track("sub", 1) end
    ne.eventresponder["mbtn_right_up"] =
        function () set_track("sub", -1) end
    ne.eventresponder["shift+mbtn_left_down"] =
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
    ne.eventresponder["mbtn_left_up"] =
        function () mp.commandv("cycle", "fullscreen") end

    --seekbar
    ne = new_element("seekbar", "slider")

    ne.enabled = not (mp.get_property("percent-pos") == nil)
    state.slider_element = ne.enabled and ne or nil  -- used for forced_title
    ne.slider.markerF = function ()
        local duration = mp.get_property_number("duration", nil)
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
        local duration = mp.get_property_number("duration", nil)
        if not ((duration == nil) or (pos == nil)) then
            possec = duration * (pos / 100)
            return mp.format_time(possec)
        else
            return ""
        end
    end
    ne.slider.seekRangesF = function()
        if user_opts.seekrangestyle == "none" then
            return nil
        end
        local cache_state = state.cache_state
        if not cache_state then
            return nil
        end
        local duration = mp.get_property_number("duration", nil)
        if (duration == nil) or duration <= 0 then
            return nil
        end
        local ranges = cache_state["seekable-ranges"]
        if #ranges == 0 then
            return nil
        end
        local nranges = {}
        for _, range in pairs(ranges) do
            nranges[#nranges + 1] = {
                ["start"] = 100 * range["start"] / duration,
                ["end"] = 100 * range["end"] / duration,
            }
        end
        return nranges
    end
    ne.eventresponder["mouse_move"] = --keyframe seeking when mouse is dragged
        function (element)
            -- mouse move events may pile up during seeking and may still get
            -- sent when the user is done seeking, so we need to throw away
            -- identical seeks
            local seekto = get_slider_value(element)
            if (element.state.lastseek == nil) or
                (not (element.state.lastseek == seekto)) then
                    local flags = "absolute-percent"
                    if not user_opts.seekbarkeyframes then
                        flags = flags .. "+exact"
                    end
                    mp.commandv("seek", seekto, flags)
                    element.state.lastseek = seekto
            end

        end
    ne.eventresponder["mbtn_left_down"] = --exact seeks on single clicks
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
    ne.eventresponder["mbtn_left_up"] = function ()
        state.tc_ms = not state.tc_ms
        request_init()
    end

    -- tc_right (total/remaining time)
    ne = new_element("tc_right", "button")

    ne.visible = (mp.get_property_number("duration", 0) > 0)
    ne.content = function ()
        if (state.rightTC_trem) then
            if state.tc_ms then
                return ("-"..mp.get_property_osd("playtime-remaining/full"))
            else
                return ("-"..mp.get_property_osd("playtime-remaining"))
            end
        else
            if state.tc_ms then
                return (mp.get_property_osd("duration/full"))
            else
                return (mp.get_property_osd("duration"))
            end
        end
    end
    ne.eventresponder["mbtn_left_up"] =
        function () state.rightTC_trem = not state.rightTC_trem end

    -- cache
    ne = new_element("cache", "button")

    ne.content = function ()
        local cache_state = state.cache_state
        if not (cache_state and cache_state["seekable-ranges"] and
            #cache_state["seekable-ranges"] > 0) then
            -- probably not a network stream
            return ""
        end
        local dmx_cache = cache_state and cache_state["cache-duration"]
        local thresh = math.min(state.dmx_cache * 0.05, 5)  -- 5% or 5s
        if dmx_cache and math.abs(dmx_cache - state.dmx_cache) >= thresh then
            state.dmx_cache = dmx_cache
        else
            dmx_cache = state.dmx_cache
        end
        local min = math.floor(dmx_cache / 60)
        local sec = dmx_cache % 60
        return "Cache: " .. (min > 0 and
            string.format("%sm%02.0fs", min, sec) or
            string.format("%3.0fs", dmx_cache))
    end

    -- volume
    ne = new_element("volume", "button")

    ne.content = function()
        local volume = mp.get_property_number("volume", 0)
        local mute = mp.get_property_native("mute")
        local volicon = {"\238\132\139", "\238\132\140",
                         "\238\132\141", "\238\132\142"}
        if volume == 0 or mute then
            return "\238\132\138"
        else
            return volicon[math.min(4,math.ceil(volume / (100/3)))]
        end
    end
    ne.eventresponder["mbtn_left_up"] =
        function () mp.commandv("cycle", "mute") end

    ne.eventresponder["wheel_up_press"] =
        function () mp.commandv("osd-auto", "add", "volume", 5) end
    ne.eventresponder["wheel_down_press"] =
        function () mp.commandv("osd-auto", "add", "volume", -5) end


    -- load layout
    layouts[user_opts.layout]()

    -- load window controls
    if window_controls_enabled() then
        window_controls(user_opts.layout == "topbar")
    end

    --do something with the elements
    prepare_elements()

    update_margins()
end

function reset_margins()
    if state.using_video_margins then
        for _, opt in ipairs(margins_opts) do
            mp.set_property_number(opt[2], 0.0)
        end
        state.using_video_margins = false
    end
end

function update_margins()
    local margins = osc_param.video_margins

    -- Don't use margins if it's visible only temporarily.
    if (not state.osc_visible) or (get_hidetimeout() >= 0) or
       (state.fullscreen and not user_opts.showfullscreen) or
       (not state.fullscreen and not user_opts.showwindowed)
    then
        margins = {l = 0, r = 0, t = 0, b = 0}
    end

    if user_opts.boxvideo then
        -- check whether any margin option has a non-default value
        local margins_used = false

        if not state.using_video_margins then
            for _, opt in ipairs(margins_opts) do
                if mp.get_property_number(opt[2], 0.0) ~= 0.0 then
                    margins_used = true
                end
            end
        end

        if not margins_used then
            for _, opt in ipairs(margins_opts) do
                local v = margins[opt[1]]
                if (v ~= 0) or state.using_video_margins then
                    mp.set_property_number(opt[2], v)
                    state.using_video_margins = true
                end
            end
        end
    else
        reset_margins()
    end

    utils.shared_script_property_set("osc-margins",
        string.format("%f,%f,%f,%f", margins.l, margins.r, margins.t, margins.b))
end

function shutdown()
    reset_margins()
    utils.shared_script_property_set("osc-margins", nil)
end

--
-- Other important stuff
--


function show_osc()
    -- show when disabled can happen (e.g. mouse_move) due to async/delayed unbinding
    if not state.enabled then return end

    msg.trace("show_osc")
    --remember last time of invocation (mouse move)
    state.showtime = mp.get_time()

    osc_visible(true)

    if (user_opts.fadeduration > 0) then
        state.anitype = nil
    end
end

function hide_osc()
    msg.trace("hide_osc")
    if not state.enabled then
        -- typically hide happens at render() from tick(), but now tick() is
        -- no-op and won't render again to remove the osc, so do that manually.
        state.osc_visible = false
        render_wipe()
    elseif (user_opts.fadeduration > 0) then
        if not(state.osc_visible == false) then
            state.anitype = "out"
            request_tick()
        end
    else
        osc_visible(false)
    end
end

function osc_visible(visible)
    if state.osc_visible ~= visible then
        state.osc_visible = visible
        update_margins()
    end
    request_tick()
end

function pause_state(name, enabled)
    state.paused = enabled
    request_tick()
end

function cache_state(name, st)
    state.cache_state = st
    request_tick()
end

-- Request that tick() is called (which typically re-renders the OSC).
-- The tick is then either executed immediately, or rate-limited if it was
-- called a small time ago.
function request_tick()
    if state.tick_timer == nil then
        state.tick_timer = mp.add_timeout(0, tick)
    end

    if not state.tick_timer:is_enabled() then
        local now = mp.get_time()
        local timeout = tick_delay - (now - state.tick_last_time)
        if timeout < 0 then
            timeout = 0
        end
        state.tick_timer.timeout = timeout
        state.tick_timer:resume()
    end
end

function mouse_leave()
    if get_hidetimeout() >= 0 then
        hide_osc()
    end
    -- reset mouse position
    state.last_mouseX, state.last_mouseY = nil, nil
    state.mouse_in_window = false
end

function request_init()
    state.initREQ = true
    request_tick()
end

-- Like request_init(), but also request an immediate update
function request_init_resize()
    request_init()
    -- ensure immediate update
    state.tick_timer:kill()
    state.tick_timer.timeout = 0
    state.tick_timer:resume()
end

function render_wipe()
    msg.trace("render_wipe()")
    state.osd.data = "" -- allows set_osd to immediately update on enable
    state.osd:remove()
end

function render()
    msg.trace("rendering")
    local current_screen_sizeX, current_screen_sizeY, aspect = mp.get_osd_size()
    local mouseX, mouseY = get_virt_mouse_pos()
    local now = mp.get_time()

    -- check if display changed, if so request reinit
    if not (state.mp_screen_sizeX == current_screen_sizeX
        and state.mp_screen_sizeY == current_screen_sizeY) then

        request_init_resize()

        state.mp_screen_sizeX = current_screen_sizeX
        state.mp_screen_sizeY = current_screen_sizeY
    end

    -- init management
    if state.active_element then
        -- mouse is held down on some element - keep ticking and igore initReq
        -- till it's released, or else the mouse-up (click) will misbehave or
        -- get ignored. that's because osc_init() recreates the osc elements,
        -- but mouse handling depends on the elements staying unmodified
        -- between mouse-down and mouse-up (using the index active_element).
        request_tick()
    elseif state.initREQ then
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
            kill_animation()
        end
    else
        kill_animation()
    end

    --mouse show/hide area
    for k,cords in pairs(osc_param.areas["showhide"]) do
        set_virt_mouse_area(cords.x1, cords.y1, cords.x2, cords.y2, "showhide")
    end
    if osc_param.areas["showhide_wc"] then
        for k,cords in pairs(osc_param.areas["showhide_wc"]) do
            set_virt_mouse_area(cords.x1, cords.y1, cords.x2, cords.y2, "showhide_wc")
        end
    else
        set_virt_mouse_area(0, 0, 0, 0, "showhide_wc")
    end
    do_enable_keybindings()

    --mouse input area
    local mouse_over_osc = false

    for _,cords in ipairs(osc_param.areas["input"]) do
        if state.osc_visible then -- activate only when OSC is actually visible
            set_virt_mouse_area(cords.x1, cords.y1, cords.x2, cords.y2, "input")
        end
        if state.osc_visible ~= state.input_enabled then
            if state.osc_visible then
                mp.enable_key_bindings("input")
            else
                mp.disable_key_bindings("input")
            end
            state.input_enabled = state.osc_visible
        end

        if (mouse_hit_coords(cords.x1, cords.y1, cords.x2, cords.y2)) then
            mouse_over_osc = true
        end
    end

    if osc_param.areas["window-controls"] then
        for _,cords in ipairs(osc_param.areas["window-controls"]) do
            if state.osc_visible then -- activate only when OSC is actually visible
                set_virt_mouse_area(cords.x1, cords.y1, cords.x2, cords.y2, "window-controls")
                mp.enable_key_bindings("window-controls")
            else
                mp.disable_key_bindings("window-controls")
            end

            if (mouse_hit_coords(cords.x1, cords.y1, cords.x2, cords.y2)) then
                mouse_over_osc = true
            end
        end
    end

    if osc_param.areas["window-controls-title"] then
        for _,cords in ipairs(osc_param.areas["window-controls-title"]) do
            if (mouse_hit_coords(cords.x1, cords.y1, cords.x2, cords.y2)) then
                mouse_over_osc = true
            end
        end
    end

    -- autohide
    if not (state.showtime == nil) and (get_hidetimeout() >= 0) then
        local timeout = state.showtime + (get_hidetimeout()/1000) - now
        if timeout <= 0 then
            if (state.active_element == nil) and not (mouse_over_osc) then
                hide_osc()
            end
        else
            -- the timer is only used to recheck the state and to possibly run
            -- the code above again
            if not state.hide_timer then
                state.hide_timer = mp.add_timeout(0, tick)
            end
            state.hide_timer.timeout = timeout
            -- re-arm
            state.hide_timer:kill()
            state.hide_timer:resume()
        end
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
    set_osd(osc_param.playresy * osc_param.display_aspect,
            osc_param.playresy, ass.text)
end

--
-- Eventhandling
--

local function element_has_action(element, action)
    return element and element.eventresponder and
        element.eventresponder[action]
end

function process_event(source, what)
    local action = string.format("%s%s", source,
        what and ("_" .. what) or "")

    if what == "down" or what == "press" then

        for n = 1, #elements do

            if mouse_hit(elements[n]) and
                elements[n].eventresponder and
                (elements[n].eventresponder[source .. "_up"] or
                    elements[n].eventresponder[action]) then

                if what == "down" then
                    state.active_element = n
                    state.active_event_source = source
                end
                -- fire the down or press event if the element has one
                if element_has_action(elements[n], action) then
                    elements[n].eventresponder[action](elements[n])
                end

            end
        end

    elseif what == "up" then

        if elements[state.active_element] then
            local n = state.active_element

            if n == 0 then
                --click on background (does not work)
            elseif element_has_action(elements[n], action) and
                mouse_hit(elements[n]) then

                elements[n].eventresponder[action](elements[n])
            end

            --reset active element
            if element_has_action(elements[n], "reset") then
                elements[n].eventresponder["reset"](elements[n])
            end

        end
        state.active_element = nil
        state.mouse_down_counter = 0

    elseif source == "mouse_move" then

        state.mouse_in_window = true

        local mouseX, mouseY = get_virt_mouse_pos()
        if (user_opts.minmousemove == 0) or
            (not ((state.last_mouseX == nil) or (state.last_mouseY == nil)) and
                ((math.abs(mouseX - state.last_mouseX) >= user_opts.minmousemove)
                    or (math.abs(mouseY - state.last_mouseY) >= user_opts.minmousemove)
                )
            ) then
            show_osc()
        end
        state.last_mouseX, state.last_mouseY = mouseX, mouseY

        local n = state.active_element
        if element_has_action(elements[n], action) then
            elements[n].eventresponder[action](elements[n])
        end
    end

    -- ensure rendering after any (mouse) event - icons could change etc
    request_tick()
end


local logo_lines = {
    -- White border
    "{\\c&HE5E5E5&\\p6}m 895 10 b 401 10 0 410 0 905 0 1399 401 1800 895 1800 1390 1800 1790 1399 1790 905 1790 410 1390 10 895 10 {\\p0}",
    -- Purple fill
    "{\\c&H682167&\\p6}m 925 42 b 463 42 87 418 87 880 87 1343 463 1718 925 1718 1388 1718 1763 1343 1763 880 1763 418 1388 42 925 42{\\p0}",
    -- Darker fill
    "{\\c&H430142&\\p6}m 1605 828 b 1605 1175 1324 1456 977 1456 631 1456 349 1175 349 828 349 482 631 200 977 200 1324 200 1605 482 1605 828{\\p0}",
    -- White fill
    "{\\c&HDDDBDD&\\p6}m 1296 910 b 1296 1131 1117 1310 897 1310 676 1310 497 1131 497 910 497 689 676 511 897 511 1117 511 1296 689 1296 910{\\p0}",
    -- Triangle
    "{\\c&H691F69&\\p6}m 762 1113 l 762 708 b 881 776 1000 843 1119 911 1000 978 881 1046 762 1113{\\p0}",
}

local santa_hat_lines = {
    -- Pompoms
    "{\\c&HC0C0C0&\\p6}m 500 -323 b 491 -322 481 -318 475 -311 465 -312 456 -319 446 -318 434 -314 427 -304 417 -297 410 -290 404 -282 395 -278 390 -274 387 -267 381 -265 377 -261 379 -254 384 -253 397 -244 409 -232 425 -228 437 -228 446 -218 457 -217 462 -216 466 -213 468 -209 471 -205 477 -203 482 -206 491 -211 499 -217 508 -222 532 -235 556 -249 576 -267 584 -272 584 -284 578 -290 569 -305 550 -312 533 -309 523 -310 515 -316 507 -321 505 -323 503 -323 500 -323{\\p0}",
    "{\\c&HE0E0E0&\\p6}m 315 -260 b 286 -258 259 -240 246 -215 235 -210 222 -215 211 -211 204 -188 177 -176 172 -151 170 -139 163 -128 154 -121 143 -103 141 -81 143 -60 139 -46 125 -34 129 -17 132 -1 134 16 142 30 145 56 161 80 181 96 196 114 210 133 231 144 266 153 303 138 328 115 373 79 401 28 423 -24 446 -73 465 -123 483 -174 487 -199 467 -225 442 -227 421 -232 402 -242 384 -254 364 -259 342 -250 322 -260 320 -260 317 -261 315 -260{\\p0}",
    -- Main cap
    "{\\c&H0000F0&\\p6}m 1151 -523 b 1016 -516 891 -458 769 -406 693 -369 624 -319 561 -262 526 -252 465 -235 479 -187 502 -147 551 -135 588 -111 1115 165 1379 232 1909 761 1926 800 1952 834 1987 858 2020 883 2053 912 2065 952 2088 1000 2146 962 2139 919 2162 836 2156 747 2143 662 2131 615 2116 567 2122 517 2120 410 2090 306 2089 199 2092 147 2071 99 2034 64 1987 5 1928 -41 1869 -86 1777 -157 1712 -256 1629 -337 1578 -389 1521 -436 1461 -476 1407 -509 1343 -507 1284 -515 1240 -519 1195 -521 1151 -523{\\p0}",
    -- Cap shadow
    "{\\c&H0000AA&\\p6}m 1657 248 b 1658 254 1659 261 1660 267 1669 276 1680 284 1689 293 1695 302 1700 311 1707 320 1716 325 1726 330 1735 335 1744 347 1752 360 1761 371 1753 352 1754 331 1753 311 1751 237 1751 163 1751 90 1752 64 1752 37 1767 14 1778 -3 1785 -24 1786 -45 1786 -60 1786 -77 1774 -87 1760 -96 1750 -78 1751 -65 1748 -37 1750 -8 1750 20 1734 78 1715 134 1699 192 1694 211 1689 231 1676 246 1671 251 1661 255 1657 248 m 1909 541 b 1914 542 1922 549 1917 539 1919 520 1921 502 1919 483 1918 458 1917 433 1915 407 1930 373 1942 338 1947 301 1952 270 1954 238 1951 207 1946 214 1947 229 1945 239 1939 278 1936 318 1924 356 1923 362 1913 382 1912 364 1906 301 1904 237 1891 175 1887 150 1892 126 1892 101 1892 68 1893 35 1888 2 1884 -9 1871 -20 1859 -14 1851 -6 1854 9 1854 20 1855 58 1864 95 1873 132 1883 179 1894 225 1899 273 1908 362 1910 451 1909 541{\\p0}",
    -- Brim and tip pompom
    "{\\c&HF8F8F8&\\p6}m 626 -191 b 565 -155 486 -196 428 -151 387 -115 327 -101 304 -47 273 2 267 59 249 113 219 157 217 213 215 265 217 309 260 302 285 283 373 264 465 264 555 257 608 252 655 292 709 287 759 294 816 276 863 298 903 340 972 324 1012 367 1061 394 1125 382 1167 424 1213 462 1268 482 1322 506 1385 546 1427 610 1479 662 1510 690 1534 725 1566 752 1611 796 1664 830 1703 880 1740 918 1747 986 1805 1005 1863 991 1897 932 1916 880 1914 823 1945 777 1961 725 1979 673 1957 622 1938 575 1912 534 1862 515 1836 473 1790 417 1755 351 1697 305 1658 266 1633 216 1593 176 1574 138 1539 116 1497 110 1448 101 1402 77 1371 37 1346 -16 1295 15 1254 6 1211 -27 1170 -62 1121 -86 1072 -104 1027 -128 976 -133 914 -130 851 -137 794 -162 740 -181 679 -168 626 -191 m 2051 917 b 1971 932 1929 1017 1919 1091 1912 1149 1923 1214 1970 1254 2000 1279 2027 1314 2066 1325 2139 1338 2212 1295 2254 1238 2281 1203 2287 1158 2282 1116 2292 1061 2273 1006 2229 970 2206 941 2167 938 2138 918{\\p0}",
}

-- called by mpv on every frame
function tick()
    if state.marginsREQ == true then
        update_margins()
        state.marginsREQ = false
    end

    if (not state.enabled) then return end

    if (state.idle) then

        -- render idle message
        msg.trace("idle message")
        local icon_x, icon_y = 320 - 26, 140
        local line_prefix = ("{\\rDefault\\an7\\1a&H00&\\bord0\\shad0\\pos(%f,%f)}"):format(icon_x, icon_y)

        local ass = assdraw.ass_new()
        -- mpv logo
        for i, line in ipairs(logo_lines) do
            ass:new_event()
            ass:append(line_prefix .. line)
        end

        -- Santa hat
        if is_december and not user_opts.greenandgrumpy then
            for i, line in ipairs(santa_hat_lines) do
                ass:new_event()
                ass:append(line_prefix .. line)
            end
        end

        ass:new_event()
        ass:pos(320, icon_y+65)
        ass:an(8)
        ass:append("Drop files or URLs to play here.")
        set_osd(640, 360, ass.text)

        if state.showhide_enabled then
            mp.disable_key_bindings("showhide")
            mp.disable_key_bindings("showhide_wc")
            state.showhide_enabled = false
        end


    elseif (state.fullscreen and user_opts.showfullscreen)
        or (not state.fullscreen and user_opts.showwindowed) then

        -- render the OSC
        render()
    else
        -- Flush OSD
        render_wipe()
    end

    state.tick_last_time = mp.get_time()

    if state.anitype ~= nil then
        -- state.anistart can be nil - animation should now start, or it can
        -- be a timestamp when it started. state.idle has no animation.
        if not state.idle and
           (not state.anistart or
            mp.get_time() < 1 + state.anistart + user_opts.fadeduration/1000)
        then
            -- animating or starting, or still within 1s past the deadline
            request_tick()
        else
            kill_animation()
        end
    end
end

function do_enable_keybindings()
    if state.enabled then
        if not state.showhide_enabled then
            mp.enable_key_bindings("showhide", "allow-vo-dragging+allow-hide-cursor")
            mp.enable_key_bindings("showhide_wc", "allow-vo-dragging+allow-hide-cursor")
        end
        state.showhide_enabled = true
    end
end

function enable_osc(enable)
    state.enabled = enable
    if enable then
        do_enable_keybindings()
    else
        hide_osc() -- acts immediately when state.enabled == false
        if state.showhide_enabled then
            mp.disable_key_bindings("showhide")
            mp.disable_key_bindings("showhide_wc")
        end
        state.showhide_enabled = false
    end
end

-- duration is observed for the sole purpose of updating chapter markers
-- positions. live streams with chapters are very rare, and the update is also
-- expensive (with request_init), so it's only observed when we have chapters
-- and the user didn't disable the livemarkers option (update_duration_watch).
function on_duration() request_init() end

local duration_watched = false
function update_duration_watch()
    local want_watch = user_opts.livemarkers and
                       (mp.get_property_number("chapters", 0) or 0) > 0 and
                       true or false  -- ensure it's a boolean

    if (want_watch ~= duration_watched) then
        if want_watch then
            mp.observe_property("duration", nil, on_duration)
        else
            mp.unobserve_property(on_duration)
        end
        duration_watched = want_watch
    end
end

validate_user_opts()
update_duration_watch()

mp.register_event("shutdown", shutdown)
mp.register_event("start-file", request_init)
mp.observe_property("track-list", nil, request_init)
mp.observe_property("playlist", nil, request_init)
mp.observe_property("chapter-list", nil, function()
    update_duration_watch()
    request_init()
end)

mp.register_script_message("osc-message", show_message)
mp.register_script_message("osc-chapterlist", function(dur)
    show_message(get_chapterlist(), dur)
end)
mp.register_script_message("osc-playlist", function(dur)
    show_message(get_playlist(), dur)
end)
mp.register_script_message("osc-tracklist", function(dur)
    local msg = {}
    for k,v in pairs(nicetypes) do
        table.insert(msg, get_tracklist(k))
    end
    show_message(table.concat(msg, '\n\n'), dur)
end)

mp.observe_property("fullscreen", "bool",
    function(name, val)
        state.fullscreen = val
        state.marginsREQ = true
        request_init_resize()
    end
)
mp.observe_property("border", "bool",
    function(name, val)
        state.border = val
        request_init_resize()
    end
)
mp.observe_property("window-maximized", "bool",
    function(name, val)
        state.maximized = val
        request_init_resize()
    end
)
mp.observe_property("idle-active", "bool",
    function(name, val)
        state.idle = val
        request_tick()
    end
)
mp.observe_property("pause", "bool", pause_state)
mp.observe_property("demuxer-cache-state", "native", cache_state)
mp.observe_property("vo-configured", "bool", function(name, val)
    request_tick()
end)
mp.observe_property("playback-time", "number", function(name, val)
    request_tick()
end)
mp.observe_property("osd-dimensions", "native", function(name, val)
    -- (we could use the value instead of re-querying it all the time, but then
    --  we might have to worry about property update ordering)
    request_init_resize()
end)

-- mouse show/hide bindings
mp.set_key_bindings({
    {"mouse_move",              function(e) process_event("mouse_move", nil) end},
    {"mouse_leave",             mouse_leave},
}, "showhide", "force")
mp.set_key_bindings({
    {"mouse_move",              function(e) process_event("mouse_move", nil) end},
    {"mouse_leave",             mouse_leave},
}, "showhide_wc", "force")
do_enable_keybindings()

--mouse input bindings
mp.set_key_bindings({
    {"mbtn_left",           function(e) process_event("mbtn_left", "up") end,
                            function(e) process_event("mbtn_left", "down")  end},
    {"shift+mbtn_left",     function(e) process_event("shift+mbtn_left", "up") end,
                            function(e) process_event("shift+mbtn_left", "down")  end},
    {"mbtn_right",          function(e) process_event("mbtn_right", "up") end,
                            function(e) process_event("mbtn_right", "down")  end},
    -- alias to shift_mbtn_left for single-handed mouse use
    {"mbtn_mid",            function(e) process_event("shift+mbtn_left", "up") end,
                            function(e) process_event("shift+mbtn_left", "down")  end},
    {"wheel_up",            function(e) process_event("wheel_up", "press") end},
    {"wheel_down",          function(e) process_event("wheel_down", "press") end},
    {"mbtn_left_dbl",       "ignore"},
    {"shift+mbtn_left_dbl", "ignore"},
    {"mbtn_right_dbl",      "ignore"},
}, "input", "force")
mp.enable_key_bindings("input")

mp.set_key_bindings({
    {"mbtn_left",           function(e) process_event("mbtn_left", "up") end,
                            function(e) process_event("mbtn_left", "down")  end},
}, "window-controls", "force")
mp.enable_key_bindings("window-controls")

function get_hidetimeout()
    if user_opts.visibility == "always" then
        return -1 -- disable autohide
    end
    return user_opts.hidetimeout
end

function always_on(val)
    if state.enabled then
        if val then
            show_osc()
        else
            hide_osc()
        end
    end
end

-- mode can be auto/always/never/cycle
-- the modes only affect internal variables and not stored on its own.
function visibility_mode(mode, no_osd)
    if mode == "cycle" then
        if not state.enabled then
            mode = "auto"
        elseif user_opts.visibility ~= "always" then
            mode = "always"
        else
            mode = "never"
        end
    end

    if mode == "auto" then
        always_on(false)
        enable_osc(true)
    elseif mode == "always" then
        enable_osc(true)
        always_on(true)
    elseif mode == "never" then
        enable_osc(false)
    else
        msg.warn("Ignoring unknown visibility mode '" .. mode .. "'")
        return
    end

    user_opts.visibility = mode
    utils.shared_script_property_set("osc-visibility", mode)

    if not no_osd and tonumber(mp.get_property("osd-level")) >= 1 then
        mp.osd_message("OSC visibility: " .. mode)
    end

    -- Reset the input state on a mode change. The input state will be
    -- recalcuated on the next render cycle, except in 'never' mode where it
    -- will just stay disabled.
    mp.disable_key_bindings("input")
    mp.disable_key_bindings("window-controls")
    state.input_enabled = false

    update_margins()
    request_tick()
end

visibility_mode(user_opts.visibility, true)
mp.register_script_message("osc-visibility", visibility_mode)
mp.add_key_binding(nil, "visibility", function() visibility_mode("cycle") end)

set_virt_mouse_area(0, 0, 0, 0, "input")
set_virt_mouse_area(0, 0, 0, 0, "window-controls")
