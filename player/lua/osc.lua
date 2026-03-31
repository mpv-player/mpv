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
    idlescreen = true,          -- show mpv logo on idle
    audioonlyscreen = false,    -- show mpv logo when no video
    scalewindowed = 1,          -- scaling of the controller when windowed
    scalefullscreen = 1,        -- scaling of the controller when fullscreen
    vidscale = "auto",          -- scale the controller with the video?
    valign = 0.8,               -- vertical alignment, -1 (top) to 1 (bottom)
    halign = 0,                 -- horizontal alignment, -1 (left) to 1 (right)
    barmargin = 0,              -- vertical margin of top/bottombar
    boxalpha = 80,              -- alpha of the background box,
                                -- 0 (opaque) to 255 (fully transparent)
    hidetimeout = 500,          -- duration in ms until the OSC hides if no
                                -- mouse movement. enforced non-negative for the
                                -- user, but internally negative is "always-on".
    fadeduration = 200,         -- duration of fade out (and fade in, if enabled) in ms, 0 = no fade
    fadein = false,             -- whether to enable fade-in effect
    deadzonesize = 0.75,        -- size of deadzone
    minmousemove = 0,           -- minimum amount of pixels the mouse has to
                                -- move between ticks to make the OSC show up
    layout = "bottombar",
    seekbarstyle = "bar",       -- bar, diamond or knob
    seekbarhandlesize = 0.6,    -- size ratio of the diamond and knob handle
    seekrangestyle = "inverted",-- bar, line, slider, inverted or none
    seekrangeseparate = true,   -- whether the seekranges overlay on the bar-style seekbar
    seekrangealpha = 200,       -- transparency of seekranges
    seekbarkeyframes = true,    -- use keyframes when dragging the seekbar
    scrollcontrols = true,      -- allow scrolling when hovering certain OSC elements
    title = "${!playlist-count==1:[${playlist-pos-1}/${playlist-count}] }${media-title}",
                                -- to be shown as OSC title
    tooltipborder = 1,          -- border of tooltip in bottom/topbar
    timetotal = false,          -- display total time instead of remaining time?
    remaining_playtime = true,  -- display the remaining time in playtime or video-time mode
                                -- playtime takes speed into account, whereas video-time doesn't
    timems = false,             -- display timecodes with milliseconds?
    tcspace = 100,              -- timecode spacing (compensate font size estimation)
    visibility = "auto",        -- only used at init to set visibility_mode(...)
    visibility_modes = "never_auto_always", -- visibility modes to cycle through
    boxmaxchars = 80,           -- title crop threshold for box layout
    boxvideo = false,           -- apply osc_param.video_margins to video
    dynamic_margins = false,    -- update margins dynamically with OSC visibility
    sub_margins = false,        -- adjust sub-margin-y to not overlap with OSC
    osd_margins = true,         -- adjust osd-margin-y to not overlap with OSC
    windowcontrols = "auto",    -- whether to show window controls
    windowcontrols_alignment = "right", -- which side to show window controls on
    windowcontrols_independent = true, -- show window controls and bottom bar independently
    floatingtitle = true,         -- show title in the floating layout?
    floatingwidth = 700,          -- width of the floating layout
    floatingalpha = 130,          -- alpha of the floating layout background
    tracknumberswidth = 35,       -- width for track number labels (0 = icon only)
    greenandgrumpy = false,     -- disable santa hat
    livemarkers = true,         -- update seekbar chapter markers on duration change
    chapter_fmt = "Chapter: %s", -- chapter print format for seekbar-hover. "no" to disable
    unicodeminus = false,       -- whether to use the Unicode minus sign character
    icon_style = "layout",      -- icon style: layout/classic/fluent

    background_color = "#000000",     -- background color of the osc
    timecode_color = "#FFFFFF",       -- color of the progress bar and time color
    title_color = "#FFFFFF",          -- color of the title
    time_pos_color = "#FFFFFF",       -- color of the timecode at hovered position
    buttons_color = "#FFFFFF",        -- color of big buttons, wc buttons, and bar small buttons
    small_buttonsL_color = "#FFFFFF", -- color of left small buttons
    small_buttonsR_color = "#FFFFFF", -- color of right small buttons
    top_buttons_color = "#FFFFFF",    -- color of top buttons
    held_element_color = "#999999",   -- color of an element while held down

    time_pos_outline_color = "#000000",   -- color of the border timecodes in slimbox and TimePosBar

    tick_delay = 1 / 60,                   -- minimum interval between OSC redraws in seconds
    tick_delay_follow_display_fps = false, -- use display fps as the minimum interval

    -- luacheck: push ignore
    -- luacheck: max line length
    menu_mbtn_left_command = "script-binding select/menu; script-message-to osc osc-hide",
    menu_mbtn_mid_command = "",
    menu_mbtn_right_command = "",

    playlist_prev_mbtn_left_command = "playlist-prev",
    playlist_prev_mbtn_mid_command = "show-text ${playlist} 3000",
    playlist_prev_mbtn_right_command = "script-binding select/select-playlist; script-message-to osc osc-hide",

    playlist_next_mbtn_left_command = "playlist-next",
    playlist_next_mbtn_mid_command = "show-text ${playlist} 3000",
    playlist_next_mbtn_right_command = "script-binding select/select-playlist; script-message-to osc osc-hide",

    title_mbtn_left_command = "script-binding stats/display-page-5",
    title_mbtn_mid_command = "show-text ${path}",
    title_mbtn_right_command = "script-binding select/select-watch-history; script-message-to osc osc-hide",

    play_pause_mbtn_left_command = "cycle pause",
    play_pause_mbtn_mid_command = "cycle-values loop-playlist inf no",
    play_pause_mbtn_right_command = "cycle-values loop-file inf no",

    chapter_prev_mbtn_left_command = "osd-msg add chapter -1",
    chapter_prev_mbtn_mid_command = "show-text ${chapter-list} 3000",
    chapter_prev_mbtn_right_command = "script-binding select/select-chapter; script-message-to osc osc-hide",

    chapter_next_mbtn_left_command = "osd-msg add chapter 1",
    chapter_next_mbtn_mid_command = "show-text ${chapter-list} 3000",
    chapter_next_mbtn_right_command = "script-binding select/select-chapter; script-message-to osc osc-hide",

    audio_track_mbtn_left_command = "cycle audio",
    audio_track_mbtn_mid_command = "cycle audio down",
    audio_track_mbtn_right_command = "script-binding select/select-aid; script-message-to osc osc-hide",
    audio_track_wheel_down_command = "cycle audio",
    audio_track_wheel_up_command = "cycle audio down",

    sub_track_mbtn_left_command = "cycle sub",
    sub_track_mbtn_mid_command = "cycle sub down",
    sub_track_mbtn_right_command = "script-binding select/select-sid; script-message-to osc osc-hide",
    sub_track_wheel_down_command = "cycle sub",
    sub_track_wheel_up_command = "cycle sub down",

    volume_mbtn_left_command = "no-osd cycle mute",
    volume_mbtn_mid_command = "",
    volume_mbtn_right_command = "script-binding select/select-audio-device; script-message-to osc osc-hide",
    volume_wheel_down_command = "add volume -5",
    volume_wheel_up_command = "add volume 5",

    fullscreen_mbtn_left_command = "cycle fullscreen",
    fullscreen_mbtn_mid_command = "",
    fullscreen_mbtn_right_command = "cycle window-maximized",
    -- luacheck: pop
}

for i = 1, 99 do
    user_opts["custom_button_" .. i .. "_content"] = ""
    user_opts["custom_button_" .. i .. "_mbtn_left_command"] = ""
    user_opts["custom_button_" .. i .. "_mbtn_mid_command"] = ""
    user_opts["custom_button_" .. i .. "_mbtn_right_command"] = ""
    user_opts["custom_button_" .. i .. "_wheel_down_command"] = ""
    user_opts["custom_button_" .. i .. "_wheel_up_command"] = ""
end

local icon_font = "mpv-osd-symbols"

-- Running this in Lua 5.3+ or LuaJIT converts a hexadecimal Unicode code point
-- to the decimal value of every byte for Lua 5.1 and 5.2 compatibility:
-- glyph='\u{e000}' output=''
-- for i = 1, #glyph do output = output .. '\\' .. string.byte(glyph, i) end
-- print(output)
local icon_styles = {
    classic = {
        menu = "\238\132\130",           -- E102
        prev = "\238\132\144",           -- E110
        next = "\238\132\129",           -- E101
        pause = "\238\128\130",          -- E002
        play = "\238\132\129",           -- E101
        clock = "\238\128\134",          -- E006
        play_backward = "\238\132\144",  -- E110
        skip_backward = "\238\128\132",  -- E004
        skip_forward = "\238\128\133",   -- E005
        chapter_prev = "\238\132\132",   -- E104
        chapter_next = "\238\132\133",   -- E105
        audio = "\238\132\134",          -- E106
        subtitle = "\238\132\135",       -- E107
        mute = "\238\132\138",           -- E10A
        volume = {                               -- E10B-E10E
            "\238\132\139", "\238\132\140", "\238\132\141", "\238\132\142",
        },
        fullscreen = "\238\132\136",     -- E108
        exit_fullscreen = "\238\132\137",-- E109
        close = "\238\132\149",          -- E115
        minimize = "\238\132\146",       -- E112
        maximize = "\238\132\147",       -- E113
        unmaximize = "\238\132\148",     -- E114
    },
    fluent = {
        menu = "\238\136\128",           -- E200
        prev = "\238\136\129",           -- E201
        next = "\238\136\130",           -- E202
        pause = "\238\136\131",          -- E203
        play = "\238\136\130",           -- E202
        clock = "\238\136\132",          -- E204
        play_backward = "\238\136\129",  -- E201
        skip_backward = "\238\136\133",  -- E205
        skip_forward = "\238\136\134",   -- E206
        chapter_prev = "\238\136\135",   -- E207
        chapter_next = "\238\136\136",   -- E208
        audio = "\238\136\137",          -- E209
        subtitle = "\238\136\138",       -- E20A
        mute = "\238\136\139",           -- E20B
        volume = {                               -- E20C-E20F
            "\238\136\140", "\238\136\141", "\238\136\142", "\238\136\143",
        },
        fullscreen = "\238\136\144",     -- E210
        exit_fullscreen = "\238\136\145",-- E211
        close = "\238\136\146",          -- E212
        minimize = "\238\136\147",       -- E213
        maximize = "\238\136\148",       -- E214
        unmaximize = "\238\136\149",     -- E215
    },
}
local icons = icon_styles.classic

local function set_icon_style()
    local icon_style = user_opts.icon_style
    if user_opts.icon_style == "layout" then
        icon_style = user_opts.layout == "floating" and "fluent" or "classic"
    end
    icons = icon_styles[icon_style] or icon_styles.classic
end

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

local margins_opts = {
    {"l", "video-margin-ratio-left"},
    {"r", "video-margin-ratio-right"},
    {"t", "video-margin-ratio-top"},
    {"b", "video-margin-ratio-bottom"},
}

local tick_delay = 1 / 60
local window_control_box_width = 80
local layouts = {}
local is_december = os.date("*t").month == 12
local UNICODE_MINUS = string.char(0xe2, 0x88, 0x92)  -- UTF-8 for U+2212 MINUS SIGN
local last_custom_button = 0

local function osc_color_convert(color)
    return color:sub(6,7) .. color:sub(4,5) ..  color:sub(2,3)
end

-- luacheck: push ignore
-- luacheck: max line length
local osc_styles

local function set_osc_styles()
    osc_styles = {
        bigButtons = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.buttons_color) .. "\\3c&HFFFFFF\\fs50\\fn" .. icon_font .. "}",
        smallButtonsL = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.small_buttonsL_color) .. "\\3c&HFFFFFF\\fs19\\fn" .. icon_font .. "}",
        smallButtonsLlabel = "{\\fscx105\\fscy105\\fn" .. mp.get_property("options/osd-font") .. "}",
        smallButtonsR = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.small_buttonsR_color) .. "\\3c&HFFFFFF\\fs30\\fn" .. icon_font .. "}",
        topButtons = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.top_buttons_color) .. "\\3c&HFFFFFF\\fs12\\fn" .. icon_font .. "}",

        elementDown = "{\\1c&H" .. osc_color_convert(user_opts.held_element_color) .."}",
        timecodes = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.timecode_color) .. "\\3c&HFFFFFF\\fs20}",
        vidtitle = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.title_color) .. "\\3c&HFFFFFF\\fs14\\q2}",
        box = "{\\rDefault\\blur0\\bord1\\1c&H" .. osc_color_convert(user_opts.background_color) .. "\\3c&HFFFFFF}",

        topButtonsBar = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.top_buttons_color) .. "\\3c&HFFFFFF\\fs18\\fn" .. icon_font .. "}",
        smallButtonsBar = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.buttons_color) .. "\\3c&HFFFFFF\\fs28\\fn" .. icon_font .. "}",
        timecodesBar = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.timecode_color) .."\\3c&HFFFFFF\\fs27}",
        timePosBar = "{\\blur0\\bord".. user_opts.tooltipborder .."\\1c&H" .. osc_color_convert(user_opts.time_pos_color) .. "\\3c&H" .. osc_color_convert(user_opts.time_pos_outline_color) .. "\\fs30}",
        vidtitleBar = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.title_color) .. "\\3c&HFFFFFF\\fs18\\q2}",

        wcButtons = "{\\1c&H" .. osc_color_convert(user_opts.buttons_color) .. "\\fs24\\fn" .. icon_font .. "}",
        wcTitle = "{\\1c&H" .. osc_color_convert(user_opts.title_color) .. "\\fs24\\q2}",
        wcBar = "{\\1c&H" .. osc_color_convert(user_opts.background_color) .. "}",
        floatingButtons = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.buttons_color) .. "\\3c&HFFFFFF\\fs26\\fn" .. icon_font .. "}",
        floatingButtonslabel = "{\\fs26\\fn" .. mp.get_property("options/osd-font") .. "}",
        floatingButtonsBig = "{\\blur0\\bord0\\1c&H" .. osc_color_convert(user_opts.buttons_color) .. "\\3c&HFFFFFF\\fs30\\fn" .. icon_font .. "}",
        floatingBox = "{\\rDefault\\blur0\\bord0\\1c&H" ..
            osc_color_convert(user_opts.background_color) .. "\\3c&H" ..
            osc_color_convert(user_opts.background_color) .. "}",
    }
end

-- internal states, do not touch
local state = {
    showtime = nil,                         -- time of last invocation (last mouse move)
    wc_showtime = nil,                      -- time of last invocation for window controls
    wc_anistart = nil,                      -- time when the wc animation started
    wc_anitype = nil,                       -- current type of wc animation
    wc_animation = nil,                     -- current wc animation alpha
    touchtime = nil,                        -- time of last invocation (last touch event)
    touchpoints = {},                       -- current touch points
    osc_visible = false,
    wc_visible = false,                     -- window controls visibility
    anistart = nil,                         -- time when the animation started
    anitype = nil,                          -- current type of animation
    animation = nil,                        -- current animation alpha
    mouse_down_counter = 0,                 -- used for softrepeat
    active_element = nil,                   -- nil = none, 0 = background, 1+ = see elements[]
    active_event_source = nil,              -- the "button" that issued the current event
    rightTC_trem = not user_opts.timetotal, -- if the right timecode should display total or remaining time
    tc_ms = user_opts.timems,               -- Should the timecodes display their time with milliseconds
    screen_sizeX = nil, screen_sizeY = nil, -- last screen-resolution, to detect resolution changes to issue reINITs
    initREQ = false,                        -- is a re-init request pending?
    marginsREQ = false,                     -- is a margins update pending?
    last_mouseX = nil, last_mouseY = nil,   -- last mouse position, to detect significant mouse movement
    last_touchX = -1, last_touchY = -1,     -- last touch position
    mouse_in_window = false,
    fullscreen = false,
    tick_timer = nil,
    tick_last_time = 0,                     -- when the last tick() was run
    hide_timer = nil,
    cache_state = nil,
    idle = false,
    audio_track_count = 0,
    sub_track_count = 0,
    no_video = false,
    file_loaded = false,
    enabled = true,
    input_enabled = true,
    showhide_enabled = false,
    windowcontrols_buttons = false,
    dmx_cache = 0,
    using_video_margins = false,
    border = true,
    maximized = false,
    osd = mp.create_osd_overlay("ass-events"),
    logo_osd = mp.create_osd_overlay("ass-events"),
    chapter_list = {},                      -- sorted by time
    visibility_modes = {},                  -- visibility_modes to cycle through
    osc_message_warned = false,             -- deprecation warnings
    osc_chapterlist_warned = false,
    osc_playlist_warned = false,
    osc_tracklist_warned = false,
}

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
-- luacheck: pop


--
-- Helper functions
--

local function kill_animation(anitype_key, anistart_key, animation_key)
    state[anitype_key]   = nil
    state[anistart_key]  = nil
    state[animation_key] = nil
end

local function set_osd(osd, res_x, res_y, text, z)
    if osd.res_x == res_x and
       osd.res_y == res_y and
       osd.data == text then
        return
    end
    osd.res_x = res_x
    osd.res_y = res_y
    osd.data = text
    osd.z = z
    osd:update()
end

local function set_time_styles(timetotal_changed, timems_changed)
    if timetotal_changed then
        state.rightTC_trem = not user_opts.timetotal
    end
    if timems_changed then
        state.tc_ms = user_opts.timems
    end
end

-- scale factor for translating between real and virtual ASS coordinates
local function get_virt_scale_factor()
    local w, h = mp.get_osd_size()
    if w <= 0 or h <= 0 then
        return 0, 0
    end
    return osc_param.playresx / w, osc_param.playresy / h
end

local function recently_touched()
    if state.touchtime == nil then
        return false
    end
    return state.touchtime + 1 >= mp.get_time()
end

-- return mouse position in virtual ASS coordinates (playresx/y)
local function get_virt_mouse_pos()
    if recently_touched() then
        local sx, sy = get_virt_scale_factor()
        return state.last_touchX * sx, state.last_touchY * sy
    elseif state.mouse_in_window then
        local sx, sy = get_virt_scale_factor()
        local x, y = mp.get_mouse_pos()
        return x * sx, y * sy
    else
        return -1, -1
    end
end

local function set_virt_mouse_area(x0, y0, x1, y1, name)
    local sx, sy = get_virt_scale_factor()
    mp.set_mouse_area(x0 / sx, y0 / sy, x1 / sx, y1 / sy, name)
end

local function scale_value(x0, x1, y0, y1, val)
    local m = (y1 - y0) / (x1 - x0)
    local b = y0 - (m * x0)
    return (m * val) + b
end

-- returns hitbox spanning coordinates (top left, bottom right corner)
-- according to alignment
local function get_hitbox_coords(x, y, an, w, h)

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

local function get_hitbox_coords_geo(geometry)
    return get_hitbox_coords(geometry.x, geometry.y, geometry.an,
        geometry.w, geometry.h)
end

local function get_element_hitbox(element)
    return element.hitbox.x1, element.hitbox.y1,
        element.hitbox.x2, element.hitbox.y2
end

local function mouse_hit_coords(bX1, bY1, bX2, bY2)
    local mX, mY = get_virt_mouse_pos()
    return (mX >= bX1 and mX <= bX2 and mY >= bY1 and mY <= bY2)
end

local function mouse_in_area(names)
    if type(names) == "string" then names = {names} end
    for _, name in pairs(names) do
        for _, cords in pairs(osc_param.areas[name] or {}) do
            if mouse_hit_coords(cords.x1, cords.y1, cords.x2, cords.y2) then
                return true
            end
        end
    end
    return false
end

local function mouse_hit(element)
    return mouse_hit_coords(get_element_hitbox(element))
end

local function limit_range(min, max, val)
    if val > max then
        val = max
    elseif val < min then
        val = min
    end
    return val
end

-- translate value into element coordinates
local function get_slider_ele_pos_for(element, val)

    local ele_pos = scale_value(
        element.slider.min.value, element.slider.max.value,
        element.slider.min.ele_pos, element.slider.max.ele_pos,
        val)

    return limit_range(
        element.slider.min.ele_pos, element.slider.max.ele_pos,
        ele_pos)
end

-- translates global (mouse) coordinates to value
local function get_slider_value_at(element, glob_pos)

    local val = scale_value(
        element.slider.min.glob_pos, element.slider.max.glob_pos,
        element.slider.min.value, element.slider.max.value,
        glob_pos)

    return limit_range(
        element.slider.min.value, element.slider.max.value,
        val)
end

-- get value at current mouse position
local function get_slider_value(element)
    return get_slider_value_at(element, get_virt_mouse_pos())
end

-- align:  -1 .. +1
-- frame:  size of the containing area
-- obj:    size of the object that should be positioned inside the area
-- margin: min. distance from object to frame (as long as -1 <= align <= +1)
local function get_align(align, frame, obj, margin)
    return (frame / 2) + (((frame / 2) - margin - (obj / 2)) * align)
end

-- multiplies two alpha values, formular can probably be improved
local function mult_alpha(alphaA, alphaB)
    return 255 - (((1-(alphaA/255)) * (1-(alphaB/255))) * 255)
end

local function add_area(name, x1, y1, x2, y2)
    -- create area if needed
    if osc_param.areas[name] == nil then
        osc_param.areas[name] = {}
    end
    table.insert(osc_param.areas[name], {x1=x1, y1=y1, x2=x2, y2=y2})
end

local function ass_append_alpha(ass, alpha, modifier)
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

local function ass_draw_rr_h_cw(ass, x0, y0, x1, y1, r1, hexagon, r2)
    if hexagon then
        ass:hexagon_cw(x0, y0, x1, y1, r1, r2)
    else
        ass:round_rect_cw(x0, y0, x1, y1, r1, r2)
    end
end

local function ass_draw_rr_h_ccw(ass, x0, y0, x1, y1, r1, hexagon, r2)
    if hexagon then
        ass:hexagon_ccw(x0, y0, x1, y1, r1, r2)
    else
        ass:round_rect_ccw(x0, y0, x1, y1, r1, r2)
    end
end

local function get_hidetimeout()
    if user_opts.visibility == "always" then
        return -1 -- disable autohide
    end
    return user_opts.hidetimeout
end

local function get_touchtimeout()
    if state.touchtime == nil then
        return 0
    end
    return state.touchtime + (get_hidetimeout() / 1000) - mp.get_time()
end

local function cache_enabled()
    return state.cache_state and #state.cache_state["seekable-ranges"] > 0
end

local function reset_margins()
    if state.using_video_margins then
        for _, mopt in ipairs(margins_opts) do
            mp.set_property_number(mopt[2], 0.0)
        end
        state.using_video_margins = false
    end
    mp.set_property_number("sub-margin-y-offset", 0)
    mp.set_property_number("osd-margin-y-offset", 0)
end

local function update_margins()
    local use_margins = get_hidetimeout() < 0 or user_opts.dynamic_margins
    local top_vis = (user_opts.layout:find("top") and state.osc_visible) or state.wc_visible
    local bottom_vis = user_opts.layout:find("bottom") and state.osc_visible
    local margins = {
        l = use_margins and osc_param.video_margins.l or 0,
        r = use_margins and osc_param.video_margins.r or 0,
        t = (use_margins and top_vis) and osc_param.video_margins.t or 0,
        b = (use_margins and bottom_vis) and osc_param.video_margins.b or 0,
    }

    if user_opts.boxvideo then
        -- check whether any margin option has a non-default value
        local margins_used = false

        if not state.using_video_margins then
            for _, mopt in ipairs(margins_opts) do
                if mp.get_property_number(mopt[2], 0.0) ~= 0.0 then
                    margins_used = true
                end
            end
        end

        if not margins_used then
            for _, mopt in ipairs(margins_opts) do
                local v = margins[mopt[1]]
                if v ~= 0 or state.using_video_margins then
                    mp.set_property_number(mopt[2], v)
                    state.using_video_margins = true
                end
            end
        end
    else
        reset_margins()
    end

    local function get_margin(ent)
        local margin = 0
        if user_opts[ent .. "_margins"] then
            local align = mp.get_property(ent .. "-align-y")
            if align == "top" and top_vis then
                margin = margins.t
            elseif align == "bottom" and bottom_vis then
                margin = margins.b
            end
        end
        if ent == "sub" and user_opts.boxvideo and mp.get_property_bool("sub-use-margins") then
            margin = 0
        end
        return margin * osc_param.playresy
    end
    mp.set_property_number("sub-margin-y-offset", get_margin("sub"))
    mp.set_property_number("osd-margin-y-offset", get_margin("osd"))

    mp.set_property_native("user-data/osc/margins", margins)
end

local tick
-- Request that tick() is called (which typically re-renders the OSC).
-- The tick is then either executed immediately, or rate-limited if it was
-- called a small time ago.
local function request_tick()
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

local function request_init()
    state.initREQ = true
    request_tick()
end

-- Like request_init(), but also request an immediate update
local function request_init_resize()
    request_init()
    -- ensure immediate update
    state.tick_timer:kill()
    state.tick_timer.timeout = 0
    state.tick_timer:resume()
end

local function render_wipe(osd)
    msg.trace("render_wipe()")
    osd.data = "" -- allows set_osd to immediately update on enable
    osd:remove()
end

local function update_tracklist(_, track_list)
    state.audio_track_count = 0
    state.sub_track_count = 0

    for _, track in pairs(track_list) do
        if track.type == "audio" then
            state.audio_track_count = state.audio_track_count + 1
        elseif track.type == "sub" then
            state.sub_track_count = state.sub_track_count + 1
        end
    end

    request_init()
end

-- WindowControl helpers
local function window_controls_enabled()
    local val = user_opts.windowcontrols
    if val == "auto" then
        return not (state.border and state.title_bar)
    else
        return val ~= "no"
    end
end

local function window_controls_alignment()
    return user_opts.windowcontrols_alignment
end


--
-- Element Management
--

local elements = {}

local function prepare_elements()

    -- remove elements without layout or invisible
    local elements2 = {}
    for _, element in pairs(elements) do
        if element.layout ~= nil and element.visible then
            table.insert(elements2, element)
        end
    end
    elements = elements2

    local function elem_compare (a, b)
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


        if element.type == "box" then
            --draw box
            static_ass:draw_start()
            ass_draw_rr_h_cw(static_ass, 0, 0, elem_geo.w, elem_geo.h,
                             element.layout.box.radius, element.layout.box.hexagon)
            static_ass:draw_stop()

        elseif element.type == "slider" then
            --draw static slider parts

            local r1 = 0
            local r2 = 0
            local slider_lo = element.layout.slider
            -- offset between element outline and drag-area
            local foV = slider_lo.border + slider_lo.gap

            -- calculate positions of min and max points
            if slider_lo.stype ~= "bar" then
                r1 = elem_geo.h / 2
                element.slider.min.ele_pos = elem_geo.h / 2
                element.slider.max.ele_pos = elem_geo.w - (elem_geo.h / 2)
                if slider_lo.stype == "diamond" then
                    r2 = (elem_geo.h - 2 * slider_lo.border) / 2
                elseif slider_lo.stype == "knob" then
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
            ass_draw_rr_h_cw(static_ass, 0, 0, elem_geo.w, elem_geo.h, r1,
                             slider_lo.stype == "diamond")

            -- the "hole"
            ass_draw_rr_h_ccw(static_ass, slider_lo.border, slider_lo.border,
                              elem_geo.w - slider_lo.border, elem_geo.h - slider_lo.border,
                              r2, slider_lo.stype == "diamond")

            -- marker nibbles
            if element.slider.markerF ~= nil and slider_lo.gap > 0 then
                local markers = element.slider.markerF()
                for _,marker in pairs(markers) do
                    if marker > element.slider.min.value and
                        marker < element.slider.max.value then

                        local s = get_slider_ele_pos_for(element, marker)

                        if slider_lo.gap > 1 then -- draw triangles

                            local a = slider_lo.gap / 0.5 --0.866

                            --top
                            if slider_lo.nibbles_top then
                                static_ass:move_to(s - (a / 2), slider_lo.border)
                                static_ass:line_to(s + (a / 2), slider_lo.border)
                                static_ass:line_to(s, foV)
                            end

                            --bottom
                            if slider_lo.nibbles_bottom then
                                static_ass:move_to(s - (a / 2),
                                    elem_geo.h - slider_lo.border)
                                static_ass:line_to(s,
                                    elem_geo.h - foV)
                                static_ass:line_to(s + (a / 2),
                                    elem_geo.h - slider_lo.border)
                            end

                        else -- draw 2x1px nibbles

                            --top
                            if slider_lo.nibbles_top then
                                static_ass:rect_cw(s - 1, slider_lo.border,
                                    s + 1, slider_lo.border + slider_lo.gap);
                            end

                            --bottom
                            if slider_lo.nibbles_bottom then
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
        if not element.enabled then
            element.layout.alpha[1] = 136
            element.eventresponder = nil
        end
    end
end


--
-- Element Rendering
--

-- returns nil or a chapter element from the native property chapter-list
local function get_chapter(possec)
    local cl = state.chapter_list  -- sorted, get latest before possec, if any

    for n=#cl,1,-1 do
        if possec >= cl[n].time then
            return cl[n]
        end
    end
end

local function render_elements(master_ass)
    -- when the slider is dragged or hovered and we have a target chapter name
    -- then we use it instead of the normal title. we calculate it before the
    -- render iterations because the title may be rendered before the slider.
    state.forced_title = nil
    local se, ae = state.slider_element, elements[state.active_element]
    if user_opts.chapter_fmt ~= "no" and se and (ae == se or (not ae and mouse_hit(se))) then
        local dur = mp.get_property_number("duration", 0)
        if dur > 0 then
            local possec = get_slider_value(se) * dur / 100 -- of mouse pos
            local ch = get_chapter(possec)
            if ch and ch.title and ch.title ~= "" then
                state.forced_title = string.format(user_opts.chapter_fmt, ch.title)
            end
        end
    end

    local function render_element(n)
        local element = elements[n]

        if element.is_wc then
            if not state.wc_visible then return end
        else
            if not state.osc_visible then return end
        end

        local saved_animation
        if element.is_wc then
            saved_animation = state.animation
            state.animation = state.wc_animation
        end

        local style_ass = assdraw.ass_new()
        style_ass:merge(element.style_ass)
        ass_append_alpha(style_ass, element.layout.alpha, 0)

        if element.eventresponder and (state.active_element == n) then

            -- run render event functions
            if element.eventresponder.render ~= nil then
                element.eventresponder.render(element)
            end

            if mouse_hit(element) then
                -- mouse down styling
                if element.styledown then
                    style_ass:append(osc_styles.elementDown)
                end

                if element.softrepeat and state.mouse_down_counter >= 15
                    and state.mouse_down_counter % 5 == 0 then

                    element.eventresponder[state.active_event_source.."_down"](element)
                end
                state.mouse_down_counter = state.mouse_down_counter + 1
            end

        end

        local elem_ass = assdraw.ass_new()

        elem_ass:merge(style_ass)

        if element.type ~= "button" then
            elem_ass:merge(element.static_ass)
        end

        if element.type == "slider" then

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
                    if seekRanges and user_opts.seekrangeseparate and
                       slider_lo.rtype ~= "inverted" then
                        h = seekRangeLineHeight
                    end
                    elem_ass:rect_cw(foH, foV, xp, elem_geo.h - foV - h)

                    if seekRanges and not user_opts.seekrangeseparate and
                       slider_lo.rtype ~= "inverted" then
                        -- Punch holes for the seekRanges to be drawn later
                        for _,range in pairs(seekRanges) do
                            if range["start"] < pos then
                                local pstart = get_slider_ele_pos_for(element, range["start"])
                                local pend = xp

                                if pos > range["end"] then
                                    pend = get_slider_ele_pos_for(element, range["end"])
                                end
                                elem_ass:rect_ccw(pstart, elem_geo.h - foV - seekRangeLineHeight,
                                                  pend, elem_geo.h - foV)
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
                            elem_ass:rect_cw(pstart, elem_geo.h - foV - seekRangeLineHeight,
                                             pend, elem_geo.h - foV)
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
                            elem_ass:rect_cw(pstart, elem_geo.h - foV - seekRangeLineHeight, pend,
                                             elem_geo.h - foV)
                        end
                    elseif slider_lo.rtype == "inverted" then
                        local rlh = slider_lo.inverted_size or 1
                        if slider_lo.stype ~= "bar" then
                            ass_draw_rr_h_ccw(elem_ass, pstart, (elem_geo.h / 2) - rlh, pend,
                                              (elem_geo.h / 2) + rlh,
                                              rlh, slider_lo.stype == "diamond")
                        else
                            elem_ass:rect_ccw(pstart, (elem_geo.h / 2) - rlh, pend,
                                              (elem_geo.h / 2) + rlh)
                        end
                    end
                end
            end

            elem_ass:draw_stop()

            -- hover position indicator
            if slider_lo.hover_bar and element.slider.tooltipF ~= nil
                and mouse_hit(element) then

                local hover_val = get_slider_value(element)
                local hover_x = get_slider_ele_pos_for(element, hover_val)

                elem_ass:new_event()
                elem_ass:pos(element.hitbox.x1 + hover_x - 0.75,
                             element.hitbox.y1 + foV)
                elem_ass:an(7)
                elem_ass:append(element.layout.style)
                ass_append_alpha(elem_ass,
                    {[1] = 80, [2] = 255, [3] = 255, [4] = 255}, 0)
                elem_ass:draw_start()
                elem_ass:rect_cw(0, 0, 1.5, elem_geo.h - 2 * foV)
                elem_ass:draw_stop()
            end

            -- add tooltip
            if element.slider.tooltipF ~= nil then
                if mouse_hit(element) then
                    local sliderpos = get_slider_value(element)
                    local tooltiplabel = element.slider.tooltipF(sliderpos)

                    local an = slider_lo.tooltip_an

                    local ty

                    if an == 2 then
                        ty = element.hitbox.y1 - slider_lo.border
                    else
                        ty = element.hitbox.y1 + elem_geo.h / 2
                    end

                    local tx = get_virt_mouse_pos()
                    if slider_lo.adjust_tooltip then
                        if an == 2 then
                            if sliderpos < (s_min + 3) then
                                an = an - 1
                            elseif sliderpos > (s_max - 3) then
                                an = an + 1
                            end
                        elseif sliderpos > (s_max+s_min) / 2 then
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

        elseif element.type == "button" then

            local buttontext
            if type(element.content) == "function" then
                buttontext = element.content() -- function objects
            elseif element.content ~= nil then
                buttontext = element.content -- text objects
            end

            local maxchars = element.layout.button.maxchars
            if maxchars ~= nil and #buttontext > maxchars then
                local max_ratio = 1.25  -- up to 25% more chars while shrinking
                local limit = math.max(0, math.floor(maxchars * max_ratio) - 3)
                if #buttontext > limit then
                    while (#buttontext > limit) do
                        buttontext = buttontext:gsub(".[\128-\191]*$", "")
                    end
                    buttontext = buttontext .. "..."
                end
                buttontext = string.format("{\\fscx%f}",
                    (maxchars/#buttontext)*100) .. buttontext
            end

            elem_ass:append(buttontext)
        end

        master_ass:merge(elem_ass)

        if element.is_wc then
            state.animation = saved_animation
        end
    end

    for n = 1, #elements do
        render_element(n)
    end
end


--
-- Initialisation and Layout
--

local function new_element(name, type)
    elements[name] = {}
    elements[name].type = type

    -- add default stuff
    elements[name].eventresponder = {}
    elements[name].visible = true
    elements[name].enabled = true
    elements[name].softrepeat = false
    elements[name].styledown = (type == "button")
    elements[name].state = {}
    elements[name].is_wc = false

    if type == "slider" then
        elements[name].slider = {min = {value = 0}, max = {value = 100}}
    end


    return elements[name]
end

local function add_layout(name)
    if elements[name] ~= nil then
        -- new layout
        elements[name].layout = {}

        -- set layout defaults
        elements[name].layout.layer = 50
        elements[name].layout.alpha = {[1] = 0, [2] = 255, [3] = 255, [4] = 255}

        if elements[name].type == "button" then
            elements[name].layout.button = {
                maxchars = nil,
            }
        elseif elements[name].type == "slider" then
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
        elseif elements[name].type == "box" then
            elements[name].layout.box = {radius = 0, hexagon = false}
        end

        return elements[name].layout
    else
        msg.error("Can't add_layout to element '"..name.."', doesn't exist.")
    end
end

-- Window Controls
local function window_controls(topbar)
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

    local floating_buttons_only = user_opts.layout == "floating"
                                  and user_opts.floatingtitle

    local lo

    -- Background Bar
    local ne = new_element("wcbar", "box")
    ne.is_wc = true
    lo = add_layout("wcbar")
    lo.layer = 10
    if floating_buttons_only then
        -- Compact background behind buttons only
        lo.alpha[1] = user_opts.floatingalpha
        local blur_extend = 4
        local r = 10
        lo.geometry = {
            x = controlbox_left - blur_extend,
            y = wc_geo.y,
            an = wc_geo.an,
            w = controlbox_w + blur_extend + r,
            h = wc_geo.h + blur_extend + r,
        }
        lo.style = osc_styles.floatingBox
        lo.box.radius = r
    elseif user_opts.layout == "floating" then
        -- Full-width bar
        lo.alpha[1] = user_opts.floatingalpha
        local blur_extend = 4
        lo.geometry = {
            x = wc_geo.x - blur_extend, y = wc_geo.y, an = wc_geo.an,
            w = wc_geo.w + 2 * blur_extend, h = wc_geo.h + blur_extend,
        }
        lo.style = osc_styles.floatingBox
    else
        lo.alpha[1] = user_opts.boxalpha
        lo.geometry = wc_geo
        lo.style = osc_styles.wcBar
    end

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
    ne.is_wc = true
    ne.content = icons.close
    ne.eventresponder["mbtn_left_up"] =
        function () mp.commandv("quit") end
    lo = add_layout("close")
    lo.geometry = alignment == "left" and first_geo or third_geo
    lo.style = osc_styles.wcButtons

    -- Minimize: 🗕
    ne = new_element("minimize", "button")
    ne.is_wc = true
    ne.content = icons.minimize
    ne.eventresponder["mbtn_left_up"] =
        function () mp.commandv("cycle", "window-minimized") end
    lo = add_layout("minimize")
    lo.geometry = alignment == "left" and second_geo or first_geo
    lo.style = osc_styles.wcButtons

    -- Maximize: 🗖 /🗗
    ne = new_element("maximize", "button")
    ne.is_wc = true
    if state.maximized or state.fullscreen then
        ne.content = icons.unmaximize
    else
        ne.content = icons.maximize
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
    sh_area_y1 = wc_geo.y + get_align(1 - (2 * user_opts.deadzonesize),
                                      osc_param.playresy - wc_geo.y, 0, 0)
    add_area("showhide_wc", wc_geo.x, sh_area_y0, wc_geo.w, sh_area_y1)
    if topbar then
        -- The title is already there as part of the top bar
        return
    elseif floating_buttons_only then
        -- No title bar or video margins, just the buttons
        return
    else
        -- Apply boxvideo margins to the control bar
        osc_param.video_margins.t = wc_geo.h / osc_param.playresy
    end

    -- Window Title
    ne = new_element("wctitle", "button")
    ne.is_wc = true
    ne.content = function ()
        local title = mp.command_native({"expand-text", mp.get_property("title")})
        title = title:gsub("\n", " ")
        return title ~= "" and mp.command_native({"escape-ass", title}) or "mpv"
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

-- Classic box layout
layouts["box"] = function ()

    local osc_geo = {
        w = 550,    -- width
        h = 138,    -- height
        r = 10,     -- corner-radius
        p = 15,     -- padding
    }

    -- make sure the OSC actually fits into the video
    if osc_param.playresx < (osc_geo.w + (2 * osc_geo.p)) then
        osc_param.playresy = (osc_geo.w + (2 * osc_geo.p)) / osc_param.display_aspect
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
    local osc_w, osc_h, osc_r = osc_geo.w, osc_geo.h, osc_geo.r

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

    lo = add_layout("playlist_prev")
    lo.geometry =
        {x = (posX - pos_offsetX), y = titlerowY, an = 7, w = 12, h = 12}
    lo.style = osc_styles.topButtons

    lo = add_layout("playlist_next")
    lo.geometry =
        {x = (posX + pos_offsetX), y = titlerowY, an = 9, w = 12, h = 12}
    lo.style = osc_styles.topButtons

    --
    -- Big buttons
    --

    local bigbtnrowY = posY - pos_offsetY + 35
    local bigbtndist = 60

    lo = add_layout("play_pause")
    lo.geometry =
        {x = posX, y = bigbtnrowY, an = 5, w = 40, h = 40}
    lo.style = osc_styles.bigButtons

    lo = add_layout("skip_backward")
    lo.geometry =
        {x = posX - bigbtndist, y = bigbtnrowY, an = 5, w = 40, h = 40}
    lo.style = osc_styles.bigButtons

    lo = add_layout("skip_forward")
    lo.geometry =
        {x = posX + bigbtndist, y = bigbtnrowY, an = 5, w = 40, h = 40}
    lo.style = osc_styles.bigButtons

    lo = add_layout("chapter_prev")
    lo.geometry =
        {x = posX - (bigbtndist * 2), y = bigbtnrowY, an = 5, w = 40, h = 40}
    lo.style = osc_styles.bigButtons

    lo = add_layout("chapter_next")
    lo.geometry =
        {x = posX + (bigbtndist * 2), y = bigbtnrowY, an = 5, w = 40, h = 40}
    lo.style = osc_styles.bigButtons

    lo = add_layout("audio_track")
    lo.geometry =
        {x = posX - pos_offsetX, y = bigbtnrowY, an = 1, w = 70, h = 18}
    lo.style = osc_styles.smallButtonsL

    lo = add_layout("sub_track")
    lo.geometry =
        {x = posX - pos_offsetX, y = bigbtnrowY, an = 7, w = 70, h = 18}
    lo.style = osc_styles.smallButtonsL

    lo = add_layout("fullscreen")
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
    if osc_param.playresx < (osc_geo.w) then
        osc_param.playresy = (osc_geo.w) / osc_param.display_aspect
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
        box = "{\\rDefault\\blur0\\bord1\\1c&H" ..
              osc_color_convert(user_opts.background_color) .. "\\3c&HFFFFFF}",
        timecodes = "{\\1c&H" .. osc_color_convert(user_opts.timecode_color) .. "\\3c&H" ..
                    osc_color_convert(user_opts.time_pos_outline_color) .. "\\fs20\\bord2\\blur1}",
        tooltip = "{\\1c&H" .. osc_color_convert(user_opts.time_pos_color).. "\\3c&H" ..
                  osc_color_convert(user_opts.time_pos_outline_color) .. "\\fs12\\bord1\\blur0.5}",
    }


    new_element("bgbox", "box")
    lo = add_layout("bgbox")

    lo.geometry = {x = posX, y = posY - 1, an = 2, w = inner_w, h = ele_h}
    lo.layer = 10
    lo.style = osc_styles.box
    lo.alpha[1] = user_opts.boxalpha
    lo.alpha[3] = 0
    if user_opts["seekbarstyle"] ~= "bar" then
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

local function bar_layout(direction, slim)
    local osc_geo = {
        x = -2,
        y = nil,
        an = (direction < 0) and 7 or 1,
        w = nil,
        h = slim and 25 or 56,
    }

    local padX = 9
    local padY = 3
    local buttonW = 27
    local tcW = (state.tc_ms) and 170 or 110
    if user_opts.tcspace >= 50 and user_opts.tcspace <= 200 then
        -- adjust our hardcoded font size estimation
        tcW = tcW * user_opts.tcspace / 100
    end

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

    if osc_param.display_aspect > 0 and osc_param.playresx < minW then
        osc_param.playresy = minW / osc_param.display_aspect
        osc_param.playresx = osc_param.playresy * osc_param.display_aspect
    end

    osc_geo.y = direction * (osc_geo.h - 2 + user_opts.barmargin)
    osc_geo.w = osc_param.playresx + 4
    if direction < 0 then
        osc_geo.y = osc_geo.y + osc_param.playresy
    end

    if direction < 0 then
        osc_param.video_margins.b = osc_geo.h / osc_param.playresy
    else
        osc_param.video_margins.t = osc_geo.h / osc_param.playresy
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
        sh_area_y1 = osc_geo.y + get_align(1 - (2 * user_opts.deadzonesize),
                                           osc_param.playresy - osc_geo.y, 0, 0)
    else
        -- deadzone above OSC
        sh_area_y0 = get_align(-1 + (2 * user_opts.deadzonesize), osc_geo.y, 0, 0)
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


    -- Menu
    geo = { x = osc_geo.x + padX + 4, y = line1, an = 4, w = 18, h = 18 - padY }
    lo = add_layout("menu")
    lo.geometry = geo
    lo.style = osc_styles.topButtonsBar

    -- Playlist prev/next
    geo = { x = geo.x + geo.w + padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("playlist_prev")
    lo.geometry = geo
    lo.style = osc_styles.topButtonsBar

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("playlist_next")
    lo.geometry = geo
    lo.style = osc_styles.topButtonsBar

    local t_l = geo.x + geo.w + padX

    -- Custom buttons
    local t_r = osc_geo.x + osc_geo.w

    for i = last_custom_button, 1, -1 do
        t_r = t_r - padX
        geo = { x = t_r, y = geo.y, an = 6, w = geo.w, h = geo.h }
        t_r = t_r - geo.w
        lo = add_layout("custom_button_" .. i)
        lo.geometry = geo
        lo.style = osc_styles.vidtitleBar
    end

    t_r = t_r - padX

    if slim then
        -- Fullscreen button
        geo = { x = t_r, y = geo.y, an = 6, w = buttonW, h = geo.h }
        lo = add_layout("fullscreen")
        lo.geometry = geo
        lo.style = osc_styles.topButtonsBar
    else
        -- Cache
        geo = { x = t_r, y = geo.y, an = 6, w = 150, h = geo.h }
        lo = add_layout("cache")
        lo.geometry = geo
        lo.style = osc_styles.vidtitleBar
    end

    t_r = t_r - geo.w - padX

    -- Title
    geo = { x = t_l, y = geo.y, an = 4,
            w = t_r - t_l, h = geo.h }
    lo = add_layout("title")
    lo.geometry = geo
    lo.style = string.format("%s{\\clip(%f,%f,%f,%f)}",
        osc_styles.vidtitleBar,
        geo.x, geo.y-geo.h, geo.w, geo.y+geo.h)

    if slim then
        return
    end

    -- Playback control buttons
    geo = { x = osc_geo.x + padX + padwc_l, y = line2, an = 4,
            w = buttonW, h = 36 - padY*2}
    lo = add_layout("play_pause")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("chapter_prev")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    geo = { x = geo.x + geo.w + padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("chapter_next")
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
    lo = add_layout("fullscreen")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    -- Volume
    geo = { x = geo.x - geo.w - padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("volume")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    -- Track selection buttons
    geo = { x = geo.x - tsW - padX, y = geo.y, an = geo.an, w = tsW, h = geo.h }
    lo = add_layout("sub_track")
    lo.geometry = geo
    lo.style = osc_styles.smallButtonsBar

    geo = { x = geo.x - geo.w - padX, y = geo.y, an = geo.an, w = geo.w, h = geo.h }
    lo = add_layout("audio_track")
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
    if user_opts["seekbarstyle"] ~= "bar" then
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
end

layouts["bottombar"] = function()
    bar_layout(-1)
end

layouts["topbar"] = function()
    bar_layout(1)
end

layouts["slimbottombar"] = function()
    bar_layout(-1, true)
end

layouts["slimtopbar"] = function()
    bar_layout(1, true)
end

layouts["floating"] = function ()
    local show_title = user_opts.floatingtitle
    local osc_geo = {
        w = user_opts.floatingwidth,
        h = show_title and 90 or 72,
        r = 14,
        p = 18,
    }

    local min_width = 620
    local margin = 80
    if osc_param.display_aspect > 0 and osc_param.playresx < min_width + margin then
        osc_param.playresy = (min_width + margin) / osc_param.display_aspect
        osc_param.playresx = osc_param.playresy * osc_param.display_aspect
    end

    osc_geo.w = math.max(min_width, math.min(osc_geo.w, osc_param.playresx - margin))

    -- halign/valign
    local x = math.floor(get_align(user_opts.halign, osc_param.playresx,
        osc_geo.w, 0))
    local y = math.floor(get_align(user_opts.valign, osc_param.playresy,
        osc_geo.h, 0))

    local half_w = (osc_geo.w - 2 * osc_geo.p) / 2

    osc_param.areas = {}

    add_area("input", get_hitbox_coords(x, y, 5, osc_geo.w, osc_geo.h))

    -- Show/hide deadzone
    local sh_area_y0, sh_area_y1
    if user_opts.valign > 0 then
        sh_area_y0 = get_align(-1 + (2 * user_opts.deadzonesize),
            y - (osc_geo.h / 2), 0, 0)
        sh_area_y1 = osc_param.playresy
    else
        sh_area_y0 = 0
        sh_area_y1 = (y + (osc_geo.h / 2)) +
            get_align(1 - (2 * user_opts.deadzonesize),
            osc_param.playresy - (y + (osc_geo.h / 2)), 0, 0)
    end
    add_area("showhide", 0, sh_area_y0, osc_param.playresx, sh_area_y1)

    local lo

    -- Background
    new_element("bgbox", "box")
    lo = add_layout("bgbox")
    lo.geometry = {x = x, y = y, an = 5, w = osc_geo.w, h = osc_geo.h}
    lo.layer = 10
    lo.style = osc_styles.floatingBox
    lo.alpha[1] = user_opts.floatingalpha
    lo.alpha[3] = 255
    lo.box.radius = osc_geo.r

    -- Row positions
    local title_h = 14
    local tile_pos, seekbar_pos, ctrl_pos
    if show_title then
        tile_pos = y + (-osc_geo.h + title_h + osc_geo.p) / 2 -- title row
        seekbar_pos = y - 2                                   -- seekbar row
        ctrl_pos = y + (osc_geo.h - 18 - osc_geo.p) / 2 - 2   -- controls row
    else
        seekbar_pos = y - 15  -- seekbar row
        ctrl_pos = y + 15     -- controls row
    end

    local tc_w = (state.tc_ms) and 122 or 87
    if user_opts.tcspace >= 50 and user_opts.tcspace <= 200 then
        tc_w = tc_w * user_opts.tcspace / 100
    end

    -- Title row (optional)
    if show_title then
        lo = add_layout("title")
        lo.geometry = {x = x - half_w, y = tile_pos, an = 4,
            w = half_w * 2, h = title_h}
        lo.style = string.format("%s{\\clip(%f,%f,%f,%f)}",
            osc_styles.vidtitle,
            x - half_w, tile_pos - title_h, x + half_w, tile_pos + title_h)
        lo.button.maxchars = 105
    end

    -- Seekbar row
    local tc_h = 20
    local seekbar_pad = 2
    local seekbar_pos_bar = seekbar_pos - 1

    lo = add_layout("tc_left")
    lo.geometry = {x = x - half_w, y = seekbar_pos_bar, an = 4, w = tc_w, h = tc_h}
    lo.style = osc_styles.timecodes

    lo = add_layout("tc_right")
    lo.geometry = {x = x + half_w, y = seekbar_pos_bar, an = 6, w = tc_w, h = tc_h}
    lo.style = osc_styles.timecodes

    local sb_l = x - half_w + tc_w + seekbar_pad
    local sb_r = x + half_w - tc_w - seekbar_pad
    local seekbar_w = math.max(0, sb_r - sb_l)
    local seekbar_cx = (sb_l + sb_r) / 2

    -- Seekbar track background
    local seekH = 16
    new_element("bgbar1", "box")
    lo = add_layout("bgbar1")
    lo.geometry = {x = seekbar_cx, y = seekbar_pos_bar, an = 5, w = seekbar_w, h = seekH}
    lo.layer = 15
    lo.style = osc_styles.timecodes
    lo.alpha[1] = math.min(255, user_opts.floatingalpha + (255 - user_opts.floatingalpha) * 0.75)
    if user_opts["seekbarstyle"] ~= "bar" then
        lo.box.radius = seekH / 2
        lo.box.hexagon = user_opts["seekbarstyle"] == "diamond"
    end

    lo = add_layout("seekbar")
    lo.geometry = {x = seekbar_cx, y = seekbar_pos_bar, an = 5, w = seekbar_w, h = seekH}
    lo.style = osc_styles.timecodes
    lo.slider.tooltip_style = osc_styles.vidtitle
    lo.slider.tooltip_an = 2
    lo.slider.adjust_tooltip = false
    lo.slider.hover_bar = true
    lo.slider.stype = user_opts["seekbarstyle"]
    lo.slider.rtype = user_opts["seekrangestyle"]
    lo.slider.gap = 2
    lo.slider.border = 0
    lo.slider.inverted_size = 0.4

    -- Control row
    local btn_dist = 38
    local btn_bsize = 32

    lo = add_layout("play_pause")
    lo.geometry = {x = x, y = ctrl_pos, an = 5, w = btn_bsize, h = btn_bsize}
    lo.style = osc_styles.floatingButtonsBig

    lo = add_layout("skip_backward")
    lo.geometry = {x = x - btn_dist, y = ctrl_pos, an = 5, w = btn_bsize, h = btn_bsize}
    lo.style = osc_styles.floatingButtonsBig

    lo = add_layout("skip_forward")
    lo.geometry = {x = x + btn_dist, y = ctrl_pos, an = 5, w = btn_bsize, h = btn_bsize}
    lo.style = osc_styles.floatingButtonsBig

    lo = add_layout("chapter_prev")
    lo.geometry = {x = x - btn_dist * 2, y = ctrl_pos, an = 5, w = btn_bsize, h = btn_bsize}
    lo.style = osc_styles.floatingButtonsBig

    lo = add_layout("chapter_next")
    lo.geometry = {x = x + btn_dist * 2, y = ctrl_pos, an = 5, w = btn_bsize, h = btn_bsize}
    lo.style = osc_styles.floatingButtonsBig

    local btn_pad = 8

    -- Control row - left
    local ll = x - half_w
    local btn_size = 18

    lo = add_layout("menu")
    lo.geometry = {x = ll, y = ctrl_pos, an = 4, w = btn_size, h = btn_size}
    lo.style = osc_styles.floatingButtons
    ll = ll + btn_size + btn_pad

    lo = add_layout("playlist_prev")
    lo.geometry = {x = ll, y = ctrl_pos, an = 4, w = btn_size, h = btn_size}
    lo.style = osc_styles.floatingButtons
    ll = ll + btn_size + btn_pad

    lo = add_layout("playlist_next")
    lo.geometry = {x = ll, y = ctrl_pos, an = 4, w = btn_size, h = btn_size}
    lo.style = osc_styles.floatingButtons
    ll = ll + btn_size + btn_pad

    -- Custom buttons
    for i = 1, last_custom_button do
        lo = add_layout("custom_button_" .. i)
        lo.geometry = {x = ll, y = ctrl_pos, an = 4, w = btn_size, h = btn_size  }
        lo.style = osc_styles.vidtitleBar
        ll = ll + btn_size + btn_pad
    end

    -- Control row - right
    local rr = x + half_w - btn_size / 2
    btn_pad = 12
    local tnW = user_opts.tracknumberswidth
    local tsW = btn_size + (tnW > 0 and tnW or 0)

    lo = add_layout("fullscreen")
    lo.geometry = {x = rr, y = ctrl_pos, an = 5, w = btn_size, h = btn_size}
    lo.style = osc_styles.floatingButtons
    rr = rr - btn_size - btn_pad

    lo = add_layout("volume")
    lo.geometry = {x = rr, y = ctrl_pos, an = 5, w = btn_size, h = btn_size}
    lo.style = osc_styles.floatingButtons
    rr = rr - tsW - btn_pad

    lo = add_layout("sub_track")
    lo.geometry = {x = rr, y = ctrl_pos, an = 5, w = tsW, h = btn_size}
    lo.style = osc_styles.floatingButtons
    rr = rr - tsW - btn_pad

    lo = add_layout("audio_track")
    lo.geometry = {x = rr, y = ctrl_pos, an = 5, w = tsW, h = btn_size}
    lo.style = osc_styles.floatingButtons
end


local function bind_mouse_buttons(element_name)
    for _, button in pairs({"mbtn_left", "mbtn_mid", "mbtn_right"}) do
        local command = user_opts[element_name .. "_" .. button .. "_command"]

        if command ~= "" then
            elements[element_name].eventresponder[button .. "_up"] = function ()
                mp.command(command)
            end
        end
    end

    if user_opts.scrollcontrols then
        for _, button in pairs({"wheel_down", "wheel_up"}) do
            local command = user_opts[element_name .. "_" .. button .. "_command"]

            if command and command ~= "" then
                elements[element_name].eventresponder[button .. "_press"] = function ()
                    mp.command(command)
                end
            end
        end
    end
end

local function to_fraction(num, den)
    local sup = {['0']='\226\129\176',['1']='\194\185',    ['2']='\194\178',
                 ['3']='\194\179',    ['4']='\226\129\180',['5']='\226\129\181',
                 ['6']='\226\129\182',['7']='\226\129\183',['8']='\226\129\184',
                 ['9']='\226\129\185',['-']='\226\129\187'}
    local sub = {['0']='\226\130\128',['1']='\226\130\129',['2']='\226\130\130',
                 ['3']='\226\130\131',['4']='\226\130\132',['5']='\226\130\133',
                 ['6']='\226\130\134',['7']='\226\130\135',['8']='\226\130\136',
                 ['9']='\226\130\137'}
    return tostring(num):gsub('.', sup) .. "\226\129\132" .. tostring(den):gsub('.', sub)
end

local function osc_init()
    msg.debug("osc_init")

    -- set canvas resolution according to display aspect and scaling setting
    local baseResY = 720
    local _, display_h, display_aspect = mp.get_osd_size()
    local scale

    if state.fullscreen then
        scale = user_opts.scalefullscreen
    else
        scale = user_opts.scalewindowed
    end

    local scale_with_video
    if user_opts.vidscale == "auto" then
        scale_with_video = mp.get_property_native("osd-scale-by-window")
    else
        scale_with_video = user_opts.vidscale == "yes"
    end

    if scale_with_video then
        osc_param.unscaled_y = baseResY
    else
        osc_param.unscaled_y = display_h
    end
    osc_param.playresy = osc_param.unscaled_y / scale
    if display_aspect > 0 then
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
        title = title:gsub("\n", " ")
        return title ~= "" and mp.command_native({"escape-ass", title}) or "mpv"
    end
    bind_mouse_buttons("title")

    -- menu
    ne = new_element("menu", "button")
    ne.content = icons.menu
    bind_mouse_buttons("menu")

    -- playlist buttons

    -- prev
    ne = new_element("playlist_prev", "button")

    ne.content = icons.prev
    ne.enabled = (pl_pos > 1) or (loop ~= "no")
    bind_mouse_buttons("playlist_prev")

    --next
    ne = new_element("playlist_next", "button")

    ne.content = icons.next
    ne.enabled = (have_pl and (pl_pos < pl_count)) or (loop ~= "no")
    bind_mouse_buttons("playlist_next")


    -- big buttons

    --play_pause
    ne = new_element("play_pause", "button")

    ne.content = function ()
        if mp.get_property_bool("paused-for-cache") ~= false then
            return icons.clock
        end

        if not mp.get_property_native("pause") then
            return icons.pause
        end

        return mp.get_property("play-direction") == "forward"
            and icons.play
            or icons.play_backward
    end
    bind_mouse_buttons("play_pause")

    --skip_backward
    ne = new_element("skip_backward", "button")

    ne.softrepeat = true
    ne.content = icons.skip_backward
    ne.eventresponder["mbtn_left_down"] =
        function () mp.commandv("seek", -5) end
    ne.eventresponder["mbtn_mid"] =
        function () mp.commandv("frame-back-step") end
    ne.eventresponder["mbtn_right_down"] =
        function () mp.commandv("seek", -30) end

    --skip_forward
    ne = new_element("skip_forward", "button")

    ne.softrepeat = true
    ne.content = icons.skip_forward
    ne.eventresponder["mbtn_left_down"] =
        function () mp.commandv("seek", 10) end
    ne.eventresponder["mbtn_mid"] =
        function () mp.commandv("frame-step") end
    ne.eventresponder["mbtn_right_down"] =
        function () mp.commandv("seek", 60) end

    --chapter_prev
    ne = new_element("chapter_prev", "button")

    ne.enabled = have_ch
    ne.content = icons.chapter_prev
    bind_mouse_buttons("chapter_prev")

    --chapter_next
    ne = new_element("chapter_next", "button")

    ne.enabled = have_ch
    ne.content = icons.chapter_next
    bind_mouse_buttons("chapter_next")

    local label_style = user_opts.layout == "floating" and osc_styles.floatingButtonslabel
                                                        or osc_styles.smallButtonsLlabel

    --audio_track
    ne = new_element("audio_track", "button")

    ne.enabled = state.audio_track_count > 0
    ne.content = function ()
        if user_opts.tracknumberswidth == 0 then
            return icons.audio
        end
        local track = mp.get_property_number("aid", "-")
        local count = state.audio_track_count
        return icons.audio .. label_style .. " " ..
               (user_opts.layout == "floating" and to_fraction(track, count)
                                                or track .. "/" .. count)
    end
    bind_mouse_buttons("audio_track")

    --sub_track
    ne = new_element("sub_track", "button")

    ne.enabled = state.sub_track_count > 0
    ne.content = function ()
        if user_opts.tracknumberswidth == 0 then
            return icons.subtitle
        end
        local track = mp.get_property_number("sid", "-")
        local count = state.sub_track_count
        return icons.subtitle .. label_style .. " " ..
               (user_opts.layout == "floating" and to_fraction(track, count)
                                                or track .. "/" .. count)
    end
    bind_mouse_buttons("sub_track")

    --fullscreen
    ne = new_element("fullscreen", "button")
    ne.content = function ()
        return state.fullscreen and icons.exit_fullscreen or icons.fullscreen
    end
    bind_mouse_buttons("fullscreen")

    --seekbar
    ne = new_element("seekbar", "slider")

    ne.enabled = mp.get_property("percent-pos") ~= nil
                 and user_opts.layout ~= "slimbottombar"
                 and user_opts.layout ~= "slimtopbar"
    state.slider_element = ne.enabled and ne or nil  -- used for forced_title
    ne.slider.markerF = function ()
        local duration = mp.get_property_number("duration")
        if duration ~= nil then
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
        function () return mp.get_property_number("percent-pos") end
    ne.slider.tooltipF = function (pos)
        local duration = mp.get_property_number("duration")
        if duration ~= nil and pos ~= nil then
            local possec = duration * (pos / 100)
            return mp.format_time(possec)
        else
            return ""
        end
    end
    ne.slider.seekRangesF = function()
        if user_opts.seekrangestyle == "none" or not cache_enabled() then
            return nil
        end
        local duration = mp.get_property_number("duration")
        if duration == nil or duration <= 0 then
            return nil
        end
        local nranges = {}
        for _, range in pairs(state.cache_state["seekable-ranges"]) do
            nranges[#nranges + 1] = {
                ["start"] = 100 * range["start"] / duration,
                ["end"] = 100 * range["end"] / duration,
            }
        end
        return nranges
    end
    ne.eventresponder["mouse_move"] = --keyframe seeking when mouse is dragged
        function (element)
            if not element.state.mbtn_left then
                return
            end

            -- mouse move events may pile up during seeking and may still get
            -- sent when the user is done seeking, so we need to throw away
            -- identical seeks
            local seekto = get_slider_value(element)
            if element.state.lastseek == nil or
                element.state.lastseek ~= seekto then
                    local flags = "absolute-percent"
                    if not user_opts.seekbarkeyframes then
                        flags = flags .. "+exact"
                    end
                    mp.commandv("seek", seekto, flags)
                    element.state.lastseek = seekto
            end

        end
    ne.eventresponder["mbtn_left_down"] = function (element)
        element.state.mbtn_left = true
        mp.commandv("seek", get_slider_value(element), "absolute-percent+exact")
    end
    ne.eventresponder["mbtn_left_up"] = function (element)
        element.state.mbtn_left = false
    end
    ne.eventresponder["mbtn_right_up"] = function (element)
        local chapter
        local pos = get_slider_value(element)
        local diff = math.huge

        for i, marker in ipairs(element.slider.markerF()) do
            if math.abs(pos - marker) < diff then
                diff = math.abs(pos - marker)
                chapter = i
            end
        end

        if chapter then
            mp.set_property("chapter", chapter - 1)
        end
    end
    ne.eventresponder["reset"] =
        function (element) element.state.lastseek = nil end

    if user_opts.scrollcontrols then
        ne.eventresponder["wheel_up_press"] =
            function () mp.commandv("osd-auto", "seek",  10) end
        ne.eventresponder["wheel_down_press"] =
            function () mp.commandv("osd-auto", "seek", -10) end
    end


    -- tc_left (current pos)
    ne = new_element("tc_left", "button")

    ne.content = function ()
        if state.tc_ms then
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
        if state.rightTC_trem then
            local minus = user_opts.unicodeminus and UNICODE_MINUS or "-"
            local property = user_opts.remaining_playtime and "playtime-remaining"
                                                           or "time-remaining"
            if state.tc_ms then
                return (minus..mp.get_property_osd(property .. "/full"))
            else
                return (minus..mp.get_property_osd(property))
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
        if not cache_enabled() then
            return ""
        end
        local dmx_cache = state.cache_state["cache-duration"]
        local thresh = math.min(state.dmx_cache * 0.05, 5)  -- 5% or 5s
        if dmx_cache and math.abs(dmx_cache - state.dmx_cache) >= thresh then
            state.dmx_cache = dmx_cache
        else
            dmx_cache = state.dmx_cache
        end
        local min = math.floor(dmx_cache / 60)
        local sec = math.floor(dmx_cache % 60) -- don't round e.g. 59.9 to 60
        return "Cache: " .. (min > 0 and
            string.format("%sm%02.0fs", min, sec) or
            string.format("%3.0fs", sec))
    end

    -- volume
    ne = new_element("volume", "button")

    ne.content = function()
        local volume = mp.get_property_number("volume")
        if volume == 0 or mp.get_property_native("mute") then
            return icons.mute
        end

        return icons.volume[math.min(4, math.ceil(volume / (100/3)))]
    end
    bind_mouse_buttons("volume")


    -- custom buttons
    for i = 1, math.huge do
        local content = user_opts["custom_button_" .. i .. "_content"]
        if not content or content == "" then
            break
        end
        ne = new_element("custom_button_" .. i, "button")
        ne.content = content
        bind_mouse_buttons("custom_button_" .. i)
        last_custom_button = i
    end


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

local function set_bar_visible(visible_key, visible)
    if state[visible_key] ~= visible then
        state[visible_key] = visible
        update_margins()
    end
    request_tick()
end

local function osc_visible(visible)
    set_bar_visible("osc_visible", visible)
end

local function set_wc_visible(visible)
    set_bar_visible("wc_visible", visible)
end

local function show_bar(label, showtime_key, visible_key, anitype_key, set_visible)
    -- show when disabled can happen (e.g. mouse_move) due to async/delayed unbinding
    if not state.enabled then return end

    msg.trace("show_" .. label)
    --remember last time of invocation (mouse move)
    state[showtime_key] = mp.get_time()

    if user_opts.fadeduration <= 0 then
        set_visible(true)
    elseif user_opts.fadein then
        if not state[visible_key] then
            state[anitype_key] = "in"
            request_tick()
        end
    else
        set_visible(true)
        state[anitype_key] = nil
    end
end

local function show_osc()
    if state.idle then return end
    show_bar("osc", "showtime", "osc_visible", "anitype", osc_visible)
end

local function show_wc()
    show_bar("wc",  "wc_showtime", "wc_visible",  "wc_anitype", set_wc_visible)
end

local function hide_bar(label, visible_key, anitype_key, set_visible)
    msg.trace("hide_" .. label)
    if not state.enabled then
        -- typically hide happens at render() from tick(), but now tick() is
        -- no-op and won't render again to remove the osc, so do that manually.
        state[visible_key] = false
        render_wipe(state.osd)
    elseif user_opts.fadeduration > 0 then
        if state[visible_key] then
            state[anitype_key] = "out"
            request_tick()
        end
    else
        set_visible(false)
    end
end

local function hide_osc()
    hide_bar("osc", "osc_visible", "anitype", osc_visible)
end

local function hide_wc()
    hide_bar("wc", "wc_visible", "wc_anitype", set_wc_visible)
end

local function cache_state(_, st)
    state.cache_state = st
    request_tick()
end

local function mouse_leave()
    if get_hidetimeout() >= 0 and get_touchtimeout() <= 0 then
        hide_osc()
        hide_wc()
    end
    -- reset mouse position
    state.last_mouseX, state.last_mouseY = nil, nil
    state.mouse_in_window = false
end

local function handle_touch(_, touchpoints)
    --remember last touch points
    if touchpoints then
        state.touchpoints = touchpoints
        if #touchpoints > 0 then
            --remember last time of invocation (touch event)
            state.touchtime = mp.get_time()
            state.last_touchX = touchpoints[1].x
            state.last_touchY = touchpoints[1].y
        end
    end
end


--
-- Eventhandling
--

local function element_has_action(element, action)
    return element and element.eventresponder and
        element.eventresponder[action]
end

local function process_event(source, what)
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

            if n == 0 then --luacheck: ignore 542
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
        if user_opts.minmousemove == 0 or
            ((state.last_mouseX ~= nil and state.last_mouseY ~= nil) and
                ((math.abs(mouseX - state.last_mouseX) >= user_opts.minmousemove)
                    or (math.abs(mouseY - state.last_mouseY) >= user_opts.minmousemove)
                )
            ) then
            if window_controls_enabled() and user_opts.windowcontrols_independent then
                if mouse_in_area("showhide_wc") then
                    show_wc()
                elseif user_opts.visibility ~= "always" then
                    hide_wc()
                end
                if mouse_in_area("showhide") then
                    show_osc()
                elseif user_opts.visibility ~= "always" then
                    hide_osc()
                end
            else
                show_osc()
                if window_controls_enabled() then show_wc() end
            end
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

local function do_enable_keybindings()
    if state.enabled then
        if not state.showhide_enabled then
            mp.enable_key_bindings("showhide", "allow-vo-dragging+allow-hide-cursor")
            mp.enable_key_bindings("showhide_wc", "allow-vo-dragging+allow-hide-cursor")
        end
        state.showhide_enabled = true
    end
end

local function enable_osc(enable)
    state.enabled = enable
    if enable then
        do_enable_keybindings()
    else
        hide_osc() -- acts immediately when state.enabled == false
        hide_wc()
        if state.showhide_enabled then
            mp.disable_key_bindings("showhide")
            mp.disable_key_bindings("showhide_wc")
        end
        state.showhide_enabled = false
    end
end

local function render()
    msg.trace("rendering")
    local current_screen_sizeX, current_screen_sizeY = mp.get_osd_size()
    local mouseX, mouseY = get_virt_mouse_pos()
    local now = mp.get_time()

    -- check if display changed, if so request reinit
    if state.screen_sizeX ~= current_screen_sizeX
        or state.screen_sizeY ~= current_screen_sizeY then

        request_init_resize()

        state.screen_sizeX = current_screen_sizeX
        state.screen_sizeY = current_screen_sizeY
    end

    -- init management
    if state.active_element then
        -- mouse is held down on some element - keep ticking and ignore initReq
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
            and not (mouseX == nil or mouseY == nil or mouseX == -1 or mouseY == -1) then

            state.last_mouseX, state.last_mouseY = mouseX, mouseY
        end
    end


    -- fade animation
    local function run_fade(anitype_key, anistart_key, animation_key, set_visible)
        local anitype = state[anitype_key]
        if anitype == nil then
            kill_animation(anitype_key, anistart_key, animation_key)
            return
        end
        if state[anistart_key] == nil then state[anistart_key] = now end
        local fadelen = user_opts.fadeduration / 1000
        if now < state[anistart_key] + fadelen then
            if anitype == "in" then --fade in
                set_visible(true)
                state[animation_key] = scale_value(state[anistart_key],
                    state[anistart_key] + fadelen, 255, 0, now)
            elseif anitype == "out" then --fade out
                state[animation_key] = scale_value(state[anistart_key],
                    state[anistart_key] + fadelen, 0, 255, now)
            end
        else
            if anitype == "out" then set_visible(false) end
            kill_animation(anitype_key, anistart_key, animation_key)
        end
    end

    run_fade("anitype", "anistart", "animation", osc_visible)
    run_fade("wc_anitype", "wc_anistart", "wc_animation", set_wc_visible)

    --mouse show/hide area
    for _, cords in pairs(osc_param.areas["showhide"]) do
        set_virt_mouse_area(cords.x1, cords.y1, cords.x2, cords.y2, "showhide")
    end
    if osc_param.areas["showhide_wc"] then
        for _, cords in pairs(osc_param.areas["showhide_wc"]) do
            set_virt_mouse_area(cords.x1, cords.y1, cords.x2, cords.y2, "showhide_wc")
        end
    else
        set_virt_mouse_area(0, 0, 0, 0, "showhide_wc")
    end
    do_enable_keybindings()

    --mouse input area
    local function update_input_area(area_name, visible, enabled_key, enable_fn)
        local areas = osc_param.areas[area_name]
        if not areas then return end
        for _,cords in ipairs(areas) do
            if visible then
                set_virt_mouse_area(cords.x1, cords.y1, cords.x2, cords.y2, area_name)
            end
            if visible ~= state[enabled_key] then
                if visible then enable_fn() else mp.disable_key_bindings(area_name) end
                state[enabled_key] = visible
            end
        end
    end

    update_input_area("input", state.osc_visible, "input_enabled",
                      function() mp.enable_key_bindings("input") end)
    update_input_area("window-controls", state.wc_visible, "windowcontrols_buttons",
                      function() mp.enable_key_bindings("window-controls") end)
    update_input_area("window-controls-title", state.wc_visible, "windowcontrols_title",
                      function()
                          mp.enable_key_bindings("window-controls-title", "allow-vo-dragging")
                      end)

    if state.hide_timer then state.hide_timer.timeout = math.huge end

    -- autohide
    local function run_autohide(showtime_key, hide_fn, input_areas)
        if state[showtime_key] == nil or get_hidetimeout() < 0 then return end
        local timeout = state[showtime_key] + (get_hidetimeout() / 1000) - now
        if timeout <= 0 and get_touchtimeout() <= 0 then
            if state.active_element == nil and not mouse_in_area(input_areas) then
                hide_fn()
            end
        else
            -- the timer is only used to recheck the state and to possibly run
            -- the code above again
            if not state.hide_timer then
                state.hide_timer = mp.add_timeout(0, tick)
            end
            if timeout < state.hide_timer.timeout then
                state.hide_timer.timeout = timeout
                -- re-arm
                state.hide_timer:kill()
                state.hide_timer:resume()
            end
        end
    end
    local osc_areas = {"input"}
    local wc_areas = {"window-controls", "window-controls-title"}
    if not user_opts.windowcontrols_independent then
        osc_areas = {"input", "window-controls", "window-controls-title"}
        wc_areas = osc_areas
    end
    run_autohide("showtime", hide_osc, osc_areas)
    run_autohide("wc_showtime", hide_wc, wc_areas)


    -- actual rendering
    local ass = assdraw.ass_new()

    -- actual OSC
    if state.osc_visible or state.wc_visible then
        render_elements(ass)
    end

    -- submit
    set_osd(state.osd, osc_param.playresy * osc_param.display_aspect,
            osc_param.playresy, ass.text, 1000)
end

local function render_logo()
    local _, _, display_aspect = mp.get_osd_size()
    if display_aspect == 0 then
        return
    end
    local display_h = 360
    local display_w = display_h * display_aspect
    -- logo is rendered at 2^(6-1) = 32 times resolution with size 1800x1800
    local icon_x, icon_y = (display_w - 1800 / 32) / 2, (display_h - 1800 / 32) / 2
    local line_prefix = ("{\\rDefault\\an7\\1a&H00&\\bord0\\shad0\\pos(%f,%f)}"):format(icon_x,
                                                                                        icon_y)

    local ass = assdraw.ass_new()
    -- mpv logo
    for _, line in ipairs(logo_lines) do
        ass:new_event()
        ass:append(line_prefix .. line)
    end

    -- Santa hat
    if is_december and not user_opts.greenandgrumpy then
        for _, line in ipairs(santa_hat_lines) do
            ass:new_event()
            ass:append(line_prefix .. line)
        end
    end

    if state.idle then
        ass:new_event()
        ass:pos(display_w / 2, icon_y + 65)
        ass:an(8)
        ass:append("Drop files or URLs to play here.")
    end
    set_osd(state.logo_osd, display_w, display_h, ass.text, -1000)
end

-- called by mpv on every frame
tick = function()
    if state.marginsREQ == true then
        update_margins()
        state.marginsREQ = false
    end

    if not state.enabled then return end

    if state.idle then
        -- render idle message
        msg.trace("idle message")
        if user_opts.idlescreen then
            render_logo()
        end

        -- Hide main OSC but keep window controls functional
        if state.osc_visible then
            osc_visible(false)
        end
        if window_controls_enabled() then
            render()
        else
            render_wipe(state.osd)
            if state.showhide_enabled then
                mp.disable_key_bindings("showhide")
                mp.disable_key_bindings("showhide_wc")
                state.showhide_enabled = false
            end
        end
    elseif state.fullscreen and user_opts.showfullscreen
        or (not state.fullscreen and user_opts.showwindowed) then

        if state.no_video and state.file_loaded and user_opts.audioonlyscreen then
            render_logo()
        else
            render_wipe(state.logo_osd)
        end
        -- render the OSC
        render()
    else
        -- Flush OSD
        render_wipe(state.osd)
        render_wipe(state.logo_osd)
    end

    state.tick_last_time = mp.get_time()

    local function tick_animation(anitype_key, anistart_key, animation_key, allow_idle)
        -- state.anistart can be nil - animation should now start, or it can
        -- be a timestamp when it started. state.idle has no animation.
        if state[anitype_key] ~= nil then
            if (allow_idle or not state.idle) and
               (not state[anistart_key] or
                mp.get_time() < 1 + state[anistart_key] + user_opts.fadeduration/1000)
            then
                -- animating or starting, or still within 1s past the deadline
                request_tick()
            else
                kill_animation(anitype_key, anistart_key, animation_key)
            end
        end
    end
    tick_animation("anitype", "anistart", "animation")
    tick_animation("wc_anitype", "wc_anistart", "wc_animation", window_controls_enabled())
end

local function shutdown()
    reset_margins()
    mp.del_property("user-data/osc")
end

-- duration is observed for the sole purpose of updating chapter markers
-- positions. live streams with chapters are very rare, and the update is also
-- expensive (with request_init), so it's only observed when we have chapters
-- and the user didn't disable the livemarkers option (update_duration_watch).
local function on_duration() request_init() end

local duration_watched = false
local function update_duration_watch()
    local want_watch = user_opts.livemarkers and
                       (mp.get_property_number("chapters", 0) or 0) > 0 and
                       true or false  -- ensure it's a boolean

    if want_watch ~= duration_watched then
        if want_watch then
            mp.observe_property("duration", "native", on_duration)
        else
            mp.unobserve_property(on_duration)
        end
        duration_watched = want_watch
    end
end

local function set_tick_delay(_, display_fps)
    -- may be nil if unavailable or 0 fps is reported
    if not display_fps or not user_opts.tick_delay_follow_display_fps then
        tick_delay = user_opts.tick_delay
        return
    end
    tick_delay = 1 / display_fps
end

mp.register_event("shutdown", shutdown)
mp.register_event("start-file", request_init)
mp.observe_property("track-list", "native", update_tracklist)
mp.observe_property("playlist-count", "native", request_init)
mp.observe_property("playlist-pos", "native", request_init)
mp.observe_property("chapter-list", "native", function(_, list)
    list = list or {}  -- safety, shouldn't return nil
    table.sort(list, function(a, b) return a.time < b.time end)
    state.chapter_list = list
    update_duration_watch()
    request_init()
end)

-- These are for backwards compatibility only.
mp.register_script_message("osc-message", function(message, dur)
    if not state.osc_message_warned then
        mp.msg.warn("osc-message is deprecated and may be removed in the future.",
                    "Use the show-text command instead.")
        state.osc_message_warned = true
    end
    mp.osd_message(message, dur)
end)
mp.register_script_message("osc-chapterlist", function(dur)
    if not state.osc_chapterlist_warned then
        mp.msg.warn("osc-chapterlist is deprecated and may be removed in the future.",
                    "Use show-text ${chapter-list} instead.")
        state.osc_chapterlist_warned = true
    end
    mp.command("show-text ${chapter-list} " .. (dur and dur * 1000 or ""))
end)
mp.register_script_message("osc-playlist", function(dur)
    if not state.osc_playlist_warned then
        mp.msg.warn("osc-playlist is deprecated and may be removed in the future.",
                    "Use show-text ${playlist} instead.")
        state.osc_playlist_warned = true
    end
    mp.command("show-text ${playlist} " .. (dur and dur * 1000 or ""))
end)
mp.register_script_message("osc-tracklist", function(dur)
    if not state.osc_tracklist_warned then
        mp.msg.warn("osc-tracklist is deprecated and may be removed in the future.",
                    "Use show-text ${track-list} instead.")
        state.osc_tracklist_warned = true
    end
    mp.command("show-text ${track-list} " .. (dur and dur * 1000 or ""))
end)

mp.observe_property("fullscreen", "bool", function(_, val)
    state.fullscreen = val
    state.marginsREQ = true
    request_init_resize()
end)
mp.observe_property("border", "bool", function(_, val)
    state.border = val
    request_init_resize()
end)
mp.observe_property("title-bar", "bool", function(_, val)
    state.title_bar = val
    request_init_resize()
end)
mp.observe_property("window-maximized", "bool", function(_, val)
    state.maximized = val
    request_init_resize()
end)
mp.observe_property("idle-active", "bool", function(_, val)
    state.idle = val
    request_tick()
end)
mp.observe_property("current-tracks/video", "native", function(_, val)
    state.no_video = val == nil
    request_tick()
end)

mp.register_event("file-loaded", function()
    state.file_loaded = true
    state.no_video = mp.get_property_native("current-tracks/video") == nil
    request_tick()
end)
mp.add_hook("on_unload", 50, function()
    state.file_loaded = false
    request_tick()
end)

mp.observe_property("display-fps", "number", set_tick_delay)
mp.observe_property("pause", "bool", request_tick)
mp.observe_property("volume", "number", request_tick)
mp.observe_property("mute", "bool", request_tick)
mp.observe_property("demuxer-cache-state", "native", cache_state)
mp.observe_property("vo-configured", "bool", request_tick)
mp.observe_property("playback-time", "number", request_tick)
mp.observe_property("osd-dimensions", "native", function()
    -- (we could use the value instead of re-querying it all the time, but then
    --  we might have to worry about property update ordering)
    request_init_resize()
end)
mp.observe_property('osd-scale-by-window', 'native', request_init_resize)
mp.observe_property('touch-pos', 'native', handle_touch)

-- mouse show/hide bindings
mp.set_key_bindings({
    {"mouse_move",              function() process_event("mouse_move", nil) end},
    {"mouse_leave",             mouse_leave},
}, "showhide", "force")
mp.set_key_bindings({
    {"mouse_move",              function() process_event("mouse_move", nil) end},
    {"mouse_leave",             mouse_leave},
}, "showhide_wc", "force")
do_enable_keybindings()

--mouse input bindings
mp.set_key_bindings({
    {"mbtn_left",           function() process_event("mbtn_left", "up") end,
                            function() process_event("mbtn_left", "down")  end},
    {"mbtn_mid",            function() process_event("mbtn_mid", "up") end,
                            function() process_event("mbtn_mid", "down")  end},
    {"mbtn_right",          function() process_event("mbtn_right", "up") end,
                            function() process_event("mbtn_right", "down")  end},
    -- alias shift+mbtn_left to mbtn_mid for touchpads
    {"shift+mbtn_left",     function() process_event("mbtn_mid", "up") end,
                            function() process_event("mbtn_mid", "down")  end},
    {"wheel_up",            function() process_event("wheel_up", "press") end},
    {"wheel_down",          function() process_event("wheel_down", "press") end},
    {"mbtn_left_dbl",       "ignore"},
    {"shift+mbtn_left_dbl", "ignore"},
    {"mbtn_right_dbl",      "ignore"},
}, "input", "force")
mp.enable_key_bindings("input")

mp.set_key_bindings({
    {"mbtn_left",           function() process_event("mbtn_left", "up") end,
                            function() process_event("mbtn_left", "down")  end},
}, "window-controls", "force")
mp.enable_key_bindings("window-controls")

local function always_on(val)
    if state.enabled then
        if val then
            show_osc()
            show_wc()
        else
            hide_osc()
            hide_wc()
        end
    end
end

-- mode can be auto/always/never/cycle
-- the modes only affect internal variables and not stored on its own.
local function visibility_mode(mode, no_osd)
    if mode == "cycle" then
        for i, allowed_mode in ipairs(state.visibility_modes) do
            if i == #state.visibility_modes then
                mode = state.visibility_modes[1]
                break
            elseif user_opts.visibility == allowed_mode then
                mode = state.visibility_modes[i + 1]
                break
            end
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
    mp.set_property_native("user-data/osc/visibility", mode)

    if not no_osd and tonumber(mp.get_property("osd-level")) >= 1 then
        mp.osd_message("OSC visibility: " .. mode)
    end

    -- Reset the input state on a mode change. The input state will be
    -- recalculated on the next render cycle, except in 'never' mode where it
    -- will just stay disabled.
    mp.disable_key_bindings("input")
    mp.disable_key_bindings("window-controls")
    state.input_enabled = false

    update_margins()
    request_tick()
end

local function idlescreen_visibility(mode, no_osd)
    if mode == "cycle" then
        if user_opts.idlescreen then
            mode = "no"
        else
            mode = "yes"
        end
    end

    if mode == "yes" then
        user_opts.idlescreen = true
    else
        user_opts.idlescreen = false
    end

    mp.set_property_native("user-data/osc/idlescreen", user_opts.idlescreen)

    if not no_osd and tonumber(mp.get_property("osd-level")) >= 1 then
        mp.osd_message("OSC logo visibility: " .. tostring(mode))
    end

    request_tick()
end

mp.register_script_message("osc-visibility", visibility_mode)
mp.register_script_message("osc-show", show_osc)
mp.register_script_message("osc-hide", function ()
    if user_opts.visibility == "auto" then
        osc_visible(false)
        set_wc_visible(false)
    end
end)
mp.add_key_binding(nil, "visibility", function() visibility_mode("cycle") end)

mp.register_script_message("osc-idlescreen", idlescreen_visibility)

-- Validate string type user options
local function validate_user_opts()
    if layouts[user_opts.layout] == nil then
        msg.warn("Invalid setting '"..user_opts.layout.."' for layout")
        user_opts.layout = "bottombar"
    end

    if user_opts.seekbarstyle ~= "bar" and
       user_opts.seekbarstyle ~= "diamond" and
       user_opts.seekbarstyle ~= "knob" then
        msg.warn("Invalid setting '" .. user_opts.seekbarstyle
            .. "' for seekbarstyle")
        user_opts.seekbarstyle = "bar"
    end

    if user_opts.seekrangestyle ~= "bar" and
       user_opts.seekrangestyle ~= "line" and
       user_opts.seekrangestyle ~= "slider" and
       user_opts.seekrangestyle ~= "inverted" and
       user_opts.seekrangestyle ~= "none" then
        msg.warn("Invalid setting '" .. user_opts.seekrangestyle
            .. "' for seekrangestyle")
        user_opts.seekrangestyle = "inverted"
    end

    if user_opts.seekrangestyle == "slider" and
       user_opts.seekbarstyle == "bar" then
        msg.warn(
            "Using 'slider' seekrangestyle together with 'bar' seekbarstyle is not supported")
        user_opts.seekrangestyle = "inverted"
    end

    if user_opts.windowcontrols ~= "auto" and
       user_opts.windowcontrols ~= "yes" and
       user_opts.windowcontrols ~= "no" then
        msg.warn("windowcontrols cannot be '" ..
                user_opts.windowcontrols .. "'. Ignoring.")
        user_opts.windowcontrols = "auto"
    end
    if user_opts.windowcontrols_alignment ~= "right" and
       user_opts.windowcontrols_alignment ~= "left" then
        msg.warn("windowcontrols_alignment cannot be '" ..
                user_opts.windowcontrols_alignment .. "'. Ignoring.")
        user_opts.windowcontrols_alignment = "right"
    end

    local colors = {
        user_opts.background_color, user_opts.top_buttons_color,
        user_opts.small_buttonsL_color, user_opts.small_buttonsR_color,
        user_opts.buttons_color, user_opts.title_color,
        user_opts.timecode_color, user_opts.time_pos_color,
        user_opts.held_element_color, user_opts.time_pos_outline_color,
    }
    for _, color in pairs(colors) do
        if color:find("^#%x%x%x%x%x%x$") == nil then
            msg.warn("'" .. color .. "' is not a valid color")
        end
    end

    for str in string.gmatch(user_opts.visibility_modes, "([^_]+)") do
        if str ~= "auto" and str ~= "always" and str ~= "never" then
            msg.warn("Ignoring unknown visibility mode '" .. str .."' in list")
        else
            table.insert(state.visibility_modes, str)
        end
    end
end

-- read options from config and command-line
opt.read_options(user_opts, "osc", function(changed)
    validate_user_opts()
    set_icon_style()
    set_osc_styles()
    set_time_styles(changed.timetotal, changed.timems)
    if changed.tick_delay or changed.tick_delay_follow_display_fps then
        set_tick_delay("display_fps", mp.get_property_number("display_fps"))
    end
    request_tick()
    visibility_mode(user_opts.visibility, true)
    update_duration_watch()
    request_init()
end)

validate_user_opts()
set_icon_style()
set_osc_styles()
set_time_styles(true, true)
set_tick_delay()
visibility_mode(user_opts.visibility, true)
update_duration_watch()

set_virt_mouse_area(0, 0, 0, 0, "input")
set_virt_mouse_area(0, 0, 0, 0, "window-controls")
set_virt_mouse_area(0, 0, 0, 0, "window-controls-title")
