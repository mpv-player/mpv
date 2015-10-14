-- Display some stats.
--
-- You can invoke the script with "i" by default or create a different key
-- binding in input.conf using "<yourkey> script_binding stats".
--
-- The style is configurable through a config file named "lua-settings/stats.conf"
-- located in your mpv directory.
--
-- Please note: not every property is always available and therefore not always
-- visible.

local options = require 'mp.options'

-- Options
local o = {
    ass_formatting = true,
    duration = 3,
    debug = false,

    -- Text style
    font = "Source Sans Pro",
    font_size = 11,
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


function main()
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

    mp.osd_message(join_stats(stats), o.duration)
end


function add_file(s)
    local sec = "file"
    s[sec] = ""

    append_property(s, sec, "filename", {prefix="File:", nl="", indent=""})
    append_property(s, sec, "metadata/title", {prefix="Title:"})
    append_property(s, sec, "chapter", {prefix="Chapter:"})
    if append_property(s, sec, "cache-used", {prefix="Cache:"}) then
        append_property(s, sec, "demuxer-cache-duration",
                        {prefix="+", suffix=" sec", nl="", indent=o.prefix_sep,
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
        append_property(s, sec, "hwdec-active",
                        {prefix="(hwdec)", nl="", indent=" ",
                         no_prefix_markup=true, no_value=true},
                        {no=true})
    end
    append_property(s, sec, "avsync", {prefix="A-V:"})
    if append_property(s, sec, "drop-frame-count", {prefix="Dropped:"}) then
        append_property(s, sec, "vo-drop-frame-count", {prefix="VO:", nl=""})
        append_property(s, sec, "vo-missed-frame-count", {prefix="Missed:", nl=""})
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
    if append_property(s, sec, "video-params/w", {prefix="Native Resolution:"}) then
        append_property(s, sec, "video-params/h",
                        {prefix="x", nl="", indent=" ", prefix_sep=" ", no_prefix_markup=true})
    end
    append_property(s, sec, "window-scale", {prefix="Window Scale:"})
    append_property(s, sec, "video-params/aspect", {prefix="Aspect Ratio:"})
    append_property(s, sec, "video-params/pixelformat", {prefix="Pixel format:"})
    append_property(s, sec, "video-params/colormatrix", {prefix="Colormatrix:"})
    append_property(s, sec, "video-params/primaries", {prefix="Primaries:"})
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
    if not o.ass_formatting then
        s.header = ""
        return
    end
    if o.custom_header and o.custom_header ~= "" then
        s.header = set_ASS(true) .. o.custom_header
    else
        s.header = string.format("%s{\\fs%d}{\\fn%s}{\\bord%f}{\\3c&H%s&}{\\1c&H%s&}" ..
                                 "{\\alpha&H%s&}{\\xshad%f}{\\yshad%f}{\\4c&H%s&}",
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

    s[sec] = string.format("%s%s%s%s%s%s%s", s[sec], attr.nl, attr.indent,
                           attr.prefix, attr.prefix_sep, no_ASS(ret), attr.suffix)
    return true
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



mp.add_key_binding("i", mp.get_script_name(), main, {repeatable=true})
