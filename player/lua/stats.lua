-- Display some stats.
--
-- Please consult the readme for information about usage and configuration:
-- https://github.com/Argon-/mpv-stats
--
-- Please note: not every property is always available and therefore not always
-- visible.

local options = require 'mp.options'

-- Options
local o = {
    -- Default key bindings
    key_oneshot = "i",
    key_toggle = "I",

    duration = 3,
    redraw_delay = 1,           -- acts as duration in the toggling case
    ass_formatting = true,
    debug = false,

    -- Graph options and style
    skip_frames = 5,
    plot_graphs = true,
    plot_bg_border_color = "0000FF",
    plot_bg_color = "262626",
    plot_color = "FFFFFF",

    -- Text style
    font = "Source Sans Pro",
    font_size = 9,
    font_color = "FFFFFF",
    border_size = 1.0,
    border_color = "262626",
    shadow_x_offset = 0.0,
    shadow_y_offset = 0.0,
    shadow_color = "000000",
    alpha = "11",

    -- Custom header for ASS tags to style the text output.
    -- Specifying this will ignore the text style values above and just
    -- use this string instead.
    custom_header = "",

    -- Text formatting
    -- With ASS
    nl = "\\N",
    indent = "\\h\\h\\h\\h\\h",
    prefix_sep = "\\h\\h",
    b1 = "{\\b1}",
    b0 = "{\\b0}",
    -- Without ASS
    no_ass_nl = "\n",
    no_ass_indent = "\t",
    no_ass_prefix_sep = " ",
    no_ass_b1 = "\027[1m",
    no_ass_b0 = "\027[0m",
}
options.read_options(o)

local format = string.format
local plast = {{}, {}, {}}
local ppos = 0
local plen = 50
local recorder = nil
local timer = mp.add_periodic_timer(o.redraw_delay, function() print_stats(o.redraw_delay + 1) end)
timer:kill()



function print_stats(duration)
    --local start = mp.get_time()
    local stats = {
        header = "",
        file = "",
        video = "",
        audio = ""
    }

    o.ass_formatting = o.ass_formatting and has_vo_window()
    if not o.ass_formatting then
        o.nl = o.no_ass_nl
        o.indent = o.no_ass_indent
        o.prefix_sep = o.no_ass_prefix_sep
        if not has_ansi() then
            o.b1 = ""
            o.b0 = ""
        else
            o.b1 = o.no_ass_b1
            o.b0 = o.no_ass_b0
        end
    end

    add_header(stats)
    add_file(stats)
    add_video(stats)
    add_audio(stats)

    mp.osd_message(join_stats(stats), duration or o.duration)
    --print("main: " .. mp.get_time() - start)
end


function add_file(s)
    local sec = "file"
    s[sec] = ""

    append_property(s, sec, "filename", {prefix="File:", nl="", indent=""})
    if not (mp.get_property_osd("filename") == mp.get_property_osd("media-title")) then
        append_property(s, sec, "media-title", {prefix="Title:"})
    end
    append_property(s, sec, "chapter", {prefix="Chapter:"})
    if append_property(s, sec, "cache-used", {prefix="Cache:"}) then
        append_property(s, sec, "demuxer-cache-duration",
                        {prefix="+", suffix=" sec", nl="", indent=o.prefix_sep,
                         prefix_sep="", no_prefix_markup=true})
        append_property(s, sec, "cache-speed",
                        {prefix="", suffix="", nl="", indent=o.prefix_sep,
                         prefix_sep="", no_prefix_markup=true})
    end
end


function add_video(s)
    local sec = "video"
    s[sec] = ""
    if not has_video() then
        return
    end

    if append_property(s, sec, "video-codec", {prefix="Video:", nl="", indent=""}) then
        if not append_property(s, sec, "hwdec-current",
                        {prefix="(hwdec:", nl="", indent=" ",
                         no_prefix_markup=true, suffix=")"},
                        {no=true, [""]=true}) then
            append_property(s, sec, "hwdec-active",
                        {prefix="(hwdec)", nl="", indent=" ",
                         no_prefix_markup=true, no_value=true},
                        {no=true})
        end
    end
    append_property(s, sec, "avsync", {prefix="A-V:"})
    if append_property(s, sec, "drop-frame-count", {prefix="Dropped:"}) then
        append_property(s, sec, "vo-drop-frame-count", {prefix="VO:", nl=""})
        append_property(s, sec, "mistimed-frame-count", {prefix="Mistimed:", nl=""})
        append_property(s, sec, "vo-delayed-frame-count", {prefix="Delayed:", nl=""})
    end
    if append_property(s, sec, "display-fps", {prefix="Display FPS:", suffix=" (specified)"}) then
        append_property(s, sec, "estimated-display-fps",
                        {suffix=" (estimated)", nl="", indent=""})
    else
        append_property(s, sec, "estimated-display-fps",
                        {prefix="Display FPS:", suffix=" (estimated)"})
    end
    if append_property(s, sec, "fps", {prefix="FPS:", suffix=" (specified)"}) then
        append_property(s, sec, "estimated-vf-fps",
                        {suffix=" (estimated)", nl="", indent=""})
    else
        append_property(s, sec, "estimated-vf-fps",
                        {prefix="FPS:", suffix=" (estimated)"})
    end
    if append_property(s, sec, "video-speed-correction", {prefix="DS:"}) then
        append_property(s, sec, "audio-speed-correction",
                        {prefix="/", nl="", indent=" ", prefix_sep=" ", no_prefix_markup=true})
    end

    append_perfdata(s, sec)

    if append_property(s, sec, "video-params/w", {prefix="Native Resolution:"}) then
        append_property(s, sec, "video-params/h",
                        {prefix="x", nl="", indent=" ", prefix_sep=" ", no_prefix_markup=true})
    end
    append_property(s, sec, "window-scale", {prefix="Window Scale:"})
    append_property(s, sec, "video-params/aspect", {prefix="Aspect Ratio:"})
    append_property(s, sec, "video-params/pixelformat", {prefix="Pixel Format:"})
    append_property(s, sec, "video-params/colormatrix", {prefix="Colormatrix:"})
    append_property(s, sec, "video-params/primaries", {prefix="Primaries:"})
    append_property(s, sec, "video-params/gamma", {prefix="Gamma:"})
    append_property(s, sec, "video-params/colorlevels", {prefix="Levels:"})
    append_property(s, sec, "packet-video-bitrate", {prefix="Bitrate:", suffix=" kbps"})
end


function add_audio(s)
    local sec = "audio"
    s[sec] = ""
    if not has_audio() then
        return
    end

    append_property(s, sec, "audio-codec", {prefix="Audio:", nl="", indent=""})
    append_property(s, sec, "audio-params/samplerate", {prefix="Sample Rate:", suffix=" Hz"})
    append_property(s, sec, "audio-params/channel-count", {prefix="Channels:"})
    append_property(s, sec, "packet-audio-bitrate", {prefix="Bitrate:", suffix=" kbps"})
end


function add_header(s)
    s.header = text_style()
end


function text_style()
    if not o.ass_formatting then
        return ""
    end
    if o.custom_header and o.custom_header ~= "" then
        return set_ASS(true) .. o.custom_header
    else
        return format("%s{\\fs%d}{\\fn%s}{\\bord%f}{\\3c&H%s&}{\\1c&H%s&}{\\alpha&H%s&}{\\xshad%f}{\\yshad%f}{\\4c&H%s&}",
                        set_ASS(true), o.font_size, o.font, o.border_size,
                        o.border_color, o.font_color, o.alpha, o.shadow_x_offset,
                        o.shadow_y_offset, o.shadow_color)
    end
end


-- Format and append a property.
-- A property whose value is either `nil` or empty (hereafter called "invalid")
-- is skipped and not appended.
-- Returns `false` in case nothing was appended, otherwise `true`.
--
-- s       : Table containing key `sec`.
-- sec     : Existing key in table `s`, value treated as a string.
-- property: The property to query and format (based on its OSD representation).
-- attr    : Optional table to overwrite certain (formatting) attributes for
--           this property.
-- exclude : Optional table containing keys which are considered invalid values
--           for this property. Specifying this will replace empty string as
--           default invalid value (nil is always invalid).
function append_property(s, sec, prop, attr, excluded)
    excluded = excluded or {[""] = true}
    local ret = mp.get_property_osd(prop)
    if not ret or excluded[ret] then
        if o.debug then
            print("No value for property: " .. prop)
        end
        return false
    end

    attr.prefix_sep = attr.prefix_sep or o.prefix_sep
    attr.indent = attr.indent or o.indent
    attr.nl = attr.nl or o.nl
    attr.suffix = attr.suffix or ""
    attr.prefix = attr.prefix or ""
    attr.no_prefix_markup = attr.no_prefix_markup or false
    attr.prefix = attr.no_prefix_markup and attr.prefix or b(attr.prefix)
    ret = attr.no_value and "" or ret

    s[sec] = format("%s%s%s%s%s%s%s", s[sec], attr.nl, attr.indent,
                    attr.prefix, attr.prefix_sep, no_ASS(ret), attr.suffix)
    return true
end


function generate_graph(values)
    -- check if at least one value was recorded yet
    if ppos < 1 then
        return ""
    end

    local pmax = 1
    for e = 1, plen do
        if values[e] and values[e] > pmax then
            pmax = values[e]
        end
    end

    local x_tics = 1
    local x_max = (plen - 1) * x_tics
    local y_offset = o.border_size
    local y_max = o.font_size / 1.5
    local x = 0


    local i = ppos
    local s = {format("m 0 0 n %f %f l ", x, y_max - (y_max * values[i] / pmax * 0.8))}
    i = ((i - 2) % plen) + 1

    while i ~= ppos do
        if values[i] then
            x = x - x_tics
            s[#s+1] = format("%f %f ", x, y_max - (y_max * values[i] / pmax * 0.8))
        end
        i = ((i - 2) % plen) + 1
    end

    s[#s+1] = format("%f %f %f %f", x, y_max, 0, y_max)

    local bg_box = format("{\\bord0.5}{\\3c&H%s&}{\\1c&H%s&}m 0 %f l %f %f %f 0 0 0",
                          o.plot_bg_border_color, o.plot_bg_color, y_max, x_max, y_max, x_max)
    return format("\\h\\h\\h{\\r}{\\pbo%f}{\\shad0}{\\alpha&H00}{\\p1}%s{\\p0}{\\bord0}{\\1c&H%s}{\\p1}%s{\\p0}{\\r}%s",
                  y_offset, bg_box, o.plot_color, table.concat(s), text_style())
end


function record_perfdata(skip)
    skip = math.max(skip, 0)
    local i = skip
    return function()
        if i < skip then
            i = i + 1
            return
        else
            i = 0
        end

        local vo_p = mp.get_property_native("vo-performance")
        if not vo_p then
            return
        end
        ppos = (ppos % plen) + 1
        plast[1][ppos] = vo_p["render-last"]
        plast[2][ppos] = vo_p["present-last"]
        plast[3][ppos] = vo_p["upload-last"]
    end
end


function append_perfdata(s, sec)
    local vo_p = mp.get_property_native("vo-performance")
    if not vo_p then
        return
    end

    local graph_render = ""
    local graph_present = ""
    local graph_upload = ""

    if o.plot_graphs and timer:is_enabled() then
        graph_render = generate_graph(plast[1])
        graph_present = generate_graph(plast[2])
        graph_upload = generate_graph(plast[3])
    end

    local dfps = mp.get_property_number("display-fps", 0)
    dfps = dfps > 0 and (1 / dfps * 1e6)

    local last_s = vo_p["render-last"] + vo_p["present-last"] + vo_p["upload-last"]
    local avg_s = vo_p["render-avg"] + vo_p["present-avg"] + vo_p["upload-avg"]
    local peak_s = vo_p["render-peak"] + vo_p["present-peak"] + vo_p["upload-peak"]

    -- highlight i with a red border when t exceeds the time for one frame
    function hl(i, t)
        if t > dfps and dfps > 0 then
            return format("{\\bord0.5}{\\3c&H0000FF&}%s{\\bord%s}{\\3c&H%s&}",
                            i, o.border_size, o.border_color)
        end
        return i
    end

    s[sec] = format("%s%s%s%s%s%s / %s / %s μs %s", s[sec], o.nl, o.indent, b("Render Time:"),
                    o.prefix_sep, hl(vo_p["render-last"], last_s), hl(vo_p["render-avg"], avg_s),
                    hl(vo_p["render-peak"], peak_s), graph_render)
    s[sec] = format("%s%s%s%s%s%s / %s / %s μs %s", s[sec], o.nl, o.indent, b("Present Time:"),
                    o.prefix_sep, hl(vo_p["present-last"], last_s), hl(vo_p["present-avg"], avg_s),
                    hl(vo_p["present-peak"], peak_s), graph_present)
    s[sec] = format("%s%s%s%s%s%s / %s / %s μs %s", s[sec], o.nl, o.indent, b("Upload Time:"),
                    o.prefix_sep, hl(vo_p["upload-last"], last_s), hl(vo_p["upload-avg"], avg_s),
                    hl(vo_p["upload-peak"], peak_s), graph_upload)
end


function no_ASS(t)
    return set_ASS(false) .. t .. set_ASS(true)
end


function set_ASS(b)
    if not o.ass_formatting then
        return ""
    end
    return mp.get_property_osd("osd-ass-cc/" .. (b and "0" or "1"))
end


function join_stats(s)
    r = s.header .. s.file

    if s.video and s.video ~= "" then
        r = r .. o.nl .. o.nl .. s.video
    end
    if s.audio and s.audio ~= "" then
        r = r .. o.nl .. o.nl .. s.audio
    end

    return r
end


function has_vo_window()
    return mp.get_property("vo-configured") == "yes"
end


function has_video()
    local r = mp.get_property("video")
    return r and r ~= "no" and r ~= ""
end


function has_audio()
    local r = mp.get_property("audio")
    return r and r ~= "no" and r ~= ""
end


function has_ansi()
    local is_windows = type(package) == 'table' and type(package.config) == 'string' and package.config:sub(1,1) == '\\'
    if is_windows then
        return os.getenv("ANSICON")
    end
    return true
end


function b(t)
    return o.b1 .. t .. o.b0
end


function toggle_stats()
    if timer:is_enabled() then
        if o.plot_graphs then
            mp.unregister_event(recorder)
        end
        timer:kill()
        mp.osd_message("", 0)
    else
        if o.plot_graphs then
            recorder = record_perfdata(o.skip_frames)
            mp.register_event("tick", recorder)
        end
        timer:resume()
        print_stats(o.redraw_delay + 1)
    end
end


if not pcall(function() timer:is_enabled() end) then
    local txt = "Stats.lua: your version of mpv does not possess required functionality. \nPlease upgrade mpv."
    print(txt)
    mp.osd_message(txt, 12)
    return
end


-- Single invocation key binding
mp.add_key_binding(o.key_oneshot, "display_stats", print_stats, {repeatable=true})

-- Toggling key binding
mp.add_key_binding(o.key_toggle, "display_stats_toggle", toggle_stats, {repeatable=false})
mp.register_event("video-reconfig",
        function()
            if timer:is_enabled() then
                print_stats(o.redraw_delay + 1)
            end
        end)
