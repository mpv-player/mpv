-- Display some stats.
--
-- Please consult the readme for information about usage and configuration:
-- https://github.com/Argon-/mpv-stats
--
-- Please note: not every property is always available and therefore not always
-- visible.

local mp = require 'mp'
local options = require 'mp.options'

-- Options
local o = {
    -- Default key bindings
    key_oneshot = "i",
    key_toggle = "I",

    duration = 3,
    redraw_delay = 1,                -- acts as duration in the toggling case
    ass_formatting = true,
    timing_warning = true,
    timing_warning_th = 0.85,        -- *no* warning threshold (warning when > target_fps * timing_warning_th)
    print_perfdata_total = false,    -- prints an additional line adding up the perfdata lines
    debug = false,

    -- Graph options and style
    plot_perfdata = true,
    plot_vsync_ratio = true,
    plot_vsync_jitter = true,
    skip_frames = 5,
    global_max = true,
    flush_graph_data = true,         -- clear data buffers when toggling
    plot_bg_border_color = "0000FF",
    plot_bg_color = "262626",
    plot_color = "FFFFFF",

    -- Text style
    font = "Source Sans Pro",
    font_mono = "Source Sans Pro",   -- monospaced digits are sufficient
    font_size = 9,
    font_color = "FFFFFF",
    border_size = 0.8,
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
local max = math.max
-- Function used to record performance data
local recorder = nil
-- Timer used for toggling
local timer = nil
-- Save these sequences locally as we'll need them a lot
local ass_start = mp.get_property_osd("osd-ass-cc/0")
local ass_stop = mp.get_property_osd("osd-ass-cc/1")

-- Ring buffers for the values used to construct a graph.
-- .pos denotes the current position, .len the buffer length
-- .max is the max value in the corresponding buffer as computed in record_data().
-- `plast_buf` is a table of buffers for the "last" value of performance data
-- for render/present/upload.
local plast_buf, vsratio_buf, vsjitter_buf
local function init_buffers()
    plast_buf = {{0, max = 0}, {0, max = 0}, {0, max = 0}, pos = 1, len = 50}
    vsratio_buf = {0, pos = 1, len = 50, max = 0}
    vsjitter_buf = {0, pos = 1, len = 50, max = 0}
end


local function set_ASS(b)
    if not o.ass_formatting then
        return ""
    end
    return b and ass_start or ass_stop
end


local function no_ASS(t)
    return set_ASS(false) .. t .. set_ASS(true)
end


local function b(t)
    return o.b1 .. t .. o.b0
end


local function text_style()
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


local function has_vo_window()
    return mp.get_property("vo-configured") == "yes"
end


local function has_video()
    local r = mp.get_property("video")
    return r and r ~= "no" and r ~= ""
end


local function has_audio()
    local r = mp.get_property("audio")
    return r and r ~= "no" and r ~= ""
end


local function has_ansi()
    local is_windows = type(package) == 'table' and type(package.config) == 'string' and package.config:sub(1,1) == '\\'
    if is_windows then
        return os.getenv("ANSICON")
    end
    return true
end


-- Generate a graph from the given values.
-- Returns an ASS formatted vector drawing as string.
--
-- values: Array/Table of numbers representing the data. Used like a ring buffer
--         it will get iterated backwards `len` times starting at position `i`.
-- i     : Index of the latest data value in `values`.
-- len   : The length/amount of numbers in `values`.
-- v_max : The maximum number in `values`. It is used to scale all data
--         values to a range of 0 to `v_max`.
-- scale : A value that will be multiplied with all data values.
local function generate_graph(values, i, len, v_max, scale)
    -- Check if at least one value exists
    if not values[i] then
        return ""
    end

    local x_tics = 1
    local x_max = (len - 1) * x_tics
    local y_offset = o.border_size
    local y_max = o.font_size * 0.66
    local x = 0

    local s = {format("m 0 0 n %f %f l ", x, y_max - (y_max * values[i] / v_max * scale))}
    i = ((i - 2) % len) + 1

    for p = 1, len - 1 do
        if values[i] then
            x = x - x_tics
            s[#s+1] = format("%f %f ", x, y_max - (y_max * values[i] / v_max * scale))
        end
        i = ((i - 2) % len) + 1
    end

    s[#s+1] = format("%f %f %f %f", x, y_max, 0, y_max)

    local bg_box = format("{\\bord0.5}{\\3c&H%s&}{\\1c&H%s&}m 0 %f l %f %f %f 0 0 0",
                          o.plot_bg_border_color, o.plot_bg_color, y_max, x_max, y_max, x_max)
    return format("%s{\\r}{\\pbo%f}{\\shad0}{\\alpha&H00}{\\p1}%s{\\p0}{\\bord0}{\\1c&H%s}{\\p1}%s{\\p0}{\\r}%s",
                  o.prefix_sep, y_offset, bg_box, o.plot_color, table.concat(s), text_style())
end


-- Format and append a property.
-- A property whose value is either `nil` or empty (hereafter called "invalid")
-- is skipped and not appended.
-- Returns `false` in case nothing was appended, otherwise `true`.
--
-- s      : Table containing strings.
-- prop   : The property to query and format (based on its OSD representation).
-- attr   : Optional table to overwrite certain (formatting) attributes for
--          this property.
-- exclude: Optional table containing keys which are considered invalid values
--          for this property. Specifying this will replace empty string as
--          default invalid value (nil is always invalid).
local function append_property(s, prop, attr, excluded)
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

    s[#s+1] = format("%s%s%s%s%s%s", attr.nl, attr.indent,
                     attr.prefix, attr.prefix_sep, no_ASS(ret), attr.suffix)
    return true
end


local function append_perfdata(s)
    local vo_p = mp.get_property_native("vo-performance")
    if not vo_p then
        return
    end

    local ds = mp.get_property_bool("display-sync-active", false)
    local target_fps = ds and mp.get_property_number("display-fps", 0)
                       or mp.get_property_number("container-fps", 0)
                       or mp.get_property_number("fps", 0)
    if target_fps > 0 then target_fps = 1 / target_fps * 1e6 end

    local last_s = vo_p["render-last"] + vo_p["present-last"] + vo_p["upload-last"]
    local avg_s = vo_p["render-avg"] + vo_p["present-avg"] + vo_p["upload-avg"]
    local peak_s = vo_p["render-peak"] + vo_p["present-peak"] + vo_p["upload-peak"]

    -- Highlight i with a red border when t exceeds the time for one frame
    -- or yellow when it exceeds a given threshold
    local function hl(i, t)
        if o.timing_warning and target_fps > 0 then
            if t > target_fps then
                return format("{\\bord0.5}{\\3c&H0000FF&}%05d{\\bord%s}{\\3c&H%s&}",
                                i, o.border_size, o.border_color)
            elseif t > (target_fps * o.timing_warning_th) then
                return format("{\\bord0.5}{\\1c&H00DDDD&}%05d{\\bord%s}{\\1c&H%s&}",
                                i, o.border_size, o.font_color)
            end
        end
        return format("%05d", i)
    end


    local rsuffix, psuffix, usuffix

    -- Plot graphs when configured and we are toggled
    if o.plot_perfdata and o.ass_formatting and timer:is_enabled() then
        local pmax = {plast_buf[1].max, plast_buf[2].max, plast_buf[3].max}
        if o.global_max then
            pmax[1] = max(pmax[1], pmax[2], pmax[3])
            pmax[2], pmax[3] = pmax[1], pmax[1]
        end

        rsuffix = generate_graph(plast_buf[1], plast_buf.pos, plast_buf.len, pmax[1], 0.8)
        psuffix = generate_graph(plast_buf[2], plast_buf.pos, plast_buf.len, pmax[2], 0.8)
        usuffix = generate_graph(plast_buf[3], plast_buf.pos, plast_buf.len, pmax[3], 0.8)

        s[#s+1] = format("%s%s%s%s{\\fs%s}%s%s%s{\\fs%s}", o.nl, o.indent,
                         b("Frame Timings:"), o.prefix_sep, o.font_size * 0.66,
                         "Render  ⏎  Present  ⏎  Upload", o.prefix_sep,
                         "(last/average/peak  μs)", o.font_size)
    else
        rsuffix = o.prefix_sep .. "Render"
        psuffix = o.prefix_sep .. "Present"
        usuffix = o.prefix_sep .. "Upload"

        s[#s+1] = format("%s%s%s%s{\\fs%s}%s{\\fs%s}", o.nl, o.indent,
                         b("Frame Timings:"), o.prefix_sep, o.font_size * 0.66,
                         "(last/average/peak  μs)", o.font_size)
    end

    local f = "%s%s%s{\\fn%s}%s / %s / %s{\\fn%s}%s%s"
    s[#s+1] = format(f, o.nl, o.indent, o.indent, o.font_mono,
                    hl(vo_p["render-last"], last_s), hl(vo_p["render-avg"], avg_s),
                    hl(vo_p["render-peak"], -math.huge), o.font, o.prefix_sep, rsuffix)
    s[#s+1] = format(f, o.nl, o.indent, o.indent, o.font_mono,
                    hl(vo_p["present-last"], last_s), hl(vo_p["present-avg"], avg_s),
                    hl(vo_p["present-peak"], -math.huge), o.font, o.prefix_sep, psuffix)
    s[#s+1] = format(f, o.nl, o.indent, o.indent, o.font_mono,
                    hl(vo_p["upload-last"], last_s), hl(vo_p["upload-avg"], avg_s),
                    hl(vo_p["upload-peak"], -math.huge), o.font, o.prefix_sep, usuffix)
    if o.print_perfdata_total then
        s[#s+1] = format(f, o.nl, o.indent, o.indent, o.font_mono,
                        hl(last_s, last_s), hl(avg_s, avg_s),
                        hl(peak_s, peak_s), o.font, o.prefix_sep, o.prefix_sep .. "Total")
    end
end


local function append_display_sync(s)
    if not mp.get_property_bool("display-sync-active", false) then
        return
    end

    local vspeed = append_property(s, "video-speed-correction", {prefix="DS:"})
    if vspeed then
        append_property(s, "audio-speed-correction",
                        {prefix="/", nl="", indent=" ", prefix_sep=" ", no_prefix_markup=true})
    else
        append_property(s, "audio-speed-correction",
                        {prefix="DS:" .. o.prefix_sep .. " - / ", prefix_sep=""})
    end

    -- As we need to plot some graphs we print jitter and ratio on their own lines
    if timer:is_enabled() and (o.plot_vsync_ratio or o.plot_vsync_jitter) and o.ass_formatting then
        local ratio_graph = ""
        local jitter_graph = ""
        if o.plot_vsync_ratio then
            ratio_graph = generate_graph(vsratio_buf, vsratio_buf.pos, vsratio_buf.len, vsratio_buf.max, 0.8)
        end
        if o.plot_vsync_jitter then
            jitter_graph = generate_graph(vsjitter_buf, vsjitter_buf.pos, vsjitter_buf.len, vsjitter_buf.max, 0.8)
        end
        append_property(s, "vsync-ratio", {prefix="VSync Ratio:", suffix=o.prefix_sep .. ratio_graph})
        append_property(s, "vsync-jitter", {prefix="VSync Jitter:", suffix=o.prefix_sep .. jitter_graph})
    else
        -- Since no graph is needed we can print ratio/jitter on the same line and save some space
        local vratio = append_property(s, "vsync-ratio", {prefix="VSync Ratio:"})
        append_property(s, "vsync-jitter", {prefix="VSync Jitter:", nl="" or o.nl})
    end
end


local function add_header(s)
    s[1] = text_style()
end


local function add_file(s)
    append_property(s, "filename", {prefix="File:", nl="", indent=""})
    if not (mp.get_property_osd("filename") == mp.get_property_osd("media-title")) then
        append_property(s, "media-title", {prefix="Title:"})
    end
    append_property(s, "chapter", {prefix="Chapter:"})
    if append_property(s, "cache-used", {prefix="Cache:"}) then
        append_property(s, "demuxer-cache-duration",
                        {prefix="+", suffix=" sec", nl="", indent=o.prefix_sep,
                         prefix_sep="", no_prefix_markup=true})
        append_property(s, "cache-speed",
                        {prefix="", suffix="", nl="", indent=o.prefix_sep,
                         prefix_sep="", no_prefix_markup=true})
    end
end


local function add_video(s)
    if not has_video() then
        return
    end

    if append_property(s, "video-codec", {prefix=o.nl .. o.nl .. "Video:", nl="", indent=""}) then
        append_property(s, "hwdec-current",
                        {prefix="(hwdec:", nl="", indent=" ",
                         no_prefix_markup=true, suffix=")"},
                        {no=true, [""]=true})
    end
    append_property(s, "avsync", {prefix="A-V:"})
    if append_property(s, "decoder-frame-drop-count", {prefix="Dropped:"}) then
        append_property(s, "frame-drop-count", {prefix="VO:", nl=""})
        append_property(s, "mistimed-frame-count", {prefix="Mistimed:", nl=""})
        append_property(s, "vo-delayed-frame-count", {prefix="Delayed:", nl=""})
    -- Deprecated FPS properties for backwards compatibility
    elseif append_property(s, "drop-frame-count", {prefix="Dropped:"}) then
        append_property(s, "vo-drop-frame-count", {prefix="VO:", nl=""})
        append_property(s, "mistimed-frame-count", {prefix="Mistimed:", nl=""})
        append_property(s, "vo-delayed-frame-count", {prefix="Delayed:", nl=""})
    end
    if append_property(s, "display-fps", {prefix="Display FPS:", suffix=" (specified)"}) then
        append_property(s, "estimated-display-fps",
                        {suffix=" (estimated)", nl="", indent=""})
    else
        append_property(s, "estimated-display-fps",
                        {prefix="Display FPS:", suffix=" (estimated)"})
    end
    if append_property(s, "container-fps", {prefix="FPS:", suffix=" (specified)"}) or
        append_property(s, "fps", {prefix="FPS:", suffix=" (specified)"}) then
        append_property(s, "estimated-vf-fps",
                        {suffix=" (estimated)", nl="", indent=""})
    else
        append_property(s, "estimated-vf-fps",
                        {prefix="FPS:", suffix=" (estimated)"})
    end

    append_display_sync(s)
    append_perfdata(s)

    if append_property(s, "video-params/w", {prefix="Native Resolution:"}) then
        append_property(s, "video-params/h",
                        {prefix="x", nl="", indent=" ", prefix_sep=" ", no_prefix_markup=true})
    end
    append_property(s, "window-scale", {prefix="Window Scale:"})
    append_property(s, "video-params/aspect", {prefix="Aspect Ratio:"})
    append_property(s, "video-params/pixelformat", {prefix="Pixel Format:"})

    -- Group these together to save vertical space
    local prim = append_property(s, "video-params/primaries", {prefix="Primaries:"})
    local cmat = append_property(s, "video-params/colormatrix",
                                 {prefix="Colormatrix:", nl=prim and "" or o.nl})
    append_property(s, "video-params/colorlevels", {prefix="Levels:", nl=cmat and "" or o.nl})

    -- Append HDR metadata conditionally (only when present and interesting)
    local hdrpeak = mp.get_property_number("video-params/sig-peak", 0)
    local hdrinfo = ""
    if hdrpeak > 0 then
        hdrinfo = " (HDR peak: " .. hdrpeak .. " cd/m²)"
    end

    append_property(s, "video-params/gamma", {prefix="Gamma:", suffix=hdrinfo})
    append_property(s, "packet-video-bitrate", {prefix="Bitrate:", suffix=" kbps"})
end


local function add_audio(s)
    if not has_audio() then
        return
    end

    append_property(s, "audio-codec", {prefix=o.nl .. o.nl .. "Audio:", nl="", indent=""})
    append_property(s, "audio-params/samplerate", {prefix="Sample Rate:", suffix=" Hz"})
    append_property(s, "audio-params/channel-count", {prefix="Channels:"})
    append_property(s, "packet-audio-bitrate", {prefix="Bitrate:", suffix=" kbps"})
end


local function print_stats(duration)
    local stats = {
        header = {},
        file = {},
        video = {},
        audio = {},
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

    add_header(stats.header)
    add_file(stats.file)
    add_video(stats.video)
    add_audio(stats.audio)

    mp.osd_message(table.concat(stats.header) .. table.concat(stats.file) ..
                   table.concat(stats.video) .. table.concat(stats.audio),
                   duration or o.duration)
end


local function record_data(skip)
    init_buffers()
    skip = max(skip, 0)
    local i = skip
    return function()
        if i < skip then
            i = i + 1
            return
        else
            i = 0
        end

        if o.plot_perfdata then
            local vo_p = mp.get_property_native("vo-performance")
            if vo_p then
                plast_buf.pos = (plast_buf.pos % plast_buf.len) + 1
                plast_buf[1][plast_buf.pos] = vo_p["render-last"]
                plast_buf[1].max = max(plast_buf[1].max, plast_buf[1][plast_buf.pos])
                plast_buf[2][plast_buf.pos] = vo_p["present-last"]
                plast_buf[2].max = max(plast_buf[2].max, plast_buf[2][plast_buf.pos])
                plast_buf[3][plast_buf.pos] = vo_p["upload-last"]
                plast_buf[3].max = max(plast_buf[3].max, plast_buf[3][plast_buf.pos])
            end
        end

        if o.plot_vsync_jitter then
            local r = mp.get_property_number("vsync-jitter", nil)
            if r then
                vsjitter_buf.pos = (vsjitter_buf.pos % vsjitter_buf.len) + 1
                vsjitter_buf[vsjitter_buf.pos] = r
                vsjitter_buf.max = max(vsjitter_buf.max, r)
            end
        end

        if o.plot_vsync_ratio then
            local r = mp.get_property_number("vsync-ratio", nil)
            if r then
                vsratio_buf.pos = (vsratio_buf.pos % vsratio_buf.len) + 1
                vsratio_buf[vsratio_buf.pos] = r
                vsratio_buf.max = max(vsratio_buf.max, r)
            end
        end
    end
end


local function toggle_stats()
    -- Disable
    if timer:is_enabled() then
        if recorder then
            mp.unregister_event(recorder)
            recorder = nil
        end
        timer:kill()
        mp.osd_message("", 0)
    -- Enable
    else
        if o.plot_perfdata or o.plot_vsync_jitter or o.plot_vsync_ratio then
            recorder = record_data(o.skip_frames)
            mp.register_event("tick", recorder)
        end
        timer:resume()
        print_stats(o.redraw_delay + 1)
    end
end


-- Create timer used for toggling, pause it immediately
timer = mp.add_periodic_timer(o.redraw_delay, function() print_stats(o.redraw_delay + 1) end)
timer:kill()

-- Check if timer has required method
if not pcall(function() timer:is_enabled() end) then
    local txt = "Stats.lua: your version of mpv does not possess required functionality. \nPlease upgrade mpv or use an older version of this script."
    print(txt)
    mp.osd_message(txt, 15)
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
