-- Display some stats.
--
-- You can invoke the script with "i" by default or create a different key
-- binding in input.conf using "<yourkey> script_binding stats".
--
-- Default appearance: http://a.pomf.se/paphjk.png
-- The style is configurable through a config file named "lua-settings/stats.conf"
-- located in your mpv directory.
--
-- Please note: not every property is always available and therefore not always
-- visible.

require 'mp.options'

local o = {
    duration = 3,
    -- text formatting
    font = "Source Sans Pro",
    font_size = 11,
    font_color = "FFFFFF",
    border_size = 1.0,
    border_color = "262626",
    shadow_x_offset = 0.0,
    shadow_y_offset = 0.0,
    shadow_color = "000000",
    alpha = "11",
    -- indentation
    nl = "\\N",
    prop_indent = "\\h\\h\\h\\h\\h",
    kv_sep = "\\h\\h",  -- key<kv_sep>value

    -- Custom header for ASS tags to format the text output.
    -- Specifying this will ignore the text formatting values above and just
    -- use this string instead.
    custom_header = ""
}
read_options(o)


function main()
    local stats = {
        header = "",
        file = "",
        video = "",
        audio = ""
    }
    
    add_header(stats)
    add_file(stats)
    add_video(stats)
    add_audio(stats)

    mp.osd_message(join_stats(stats), o.duration)
end


function add_file(s)
    s.file = ""
    local fn = mp.get_property_osd("filename")
    s.file = s.file .. b("File:") .. o.kv_sep .. no_ASS(fn)
    
    append_property(s, "file", "metadata/title", "Title:")
    append_property(s, "file", "chapter", "Chapter:")
    if append_property(s, "file", "cache-used", "Cache:") then
        append_property_inline(s, "file", "demuxer-cache-duration", "+", " sec", true, true)
    end

    s.file = s.file .. o.nl .. o.nl
end


function add_video(s)
    s.video = ""
    local r = mp.get_property_osd("video")
    if not r or r == "no" or r == "" then
        return
    end
    local fn = mp.get_property_osd("video-codec")
    s.video = s.video .. b("Video:") .. o.kv_sep .. no_ASS(fn)
    
    append_property(s, "video", "avsync", "A-V:")
    if append_property(s, "video", "drop-frame-count", "Dropped:") then
        append_property_inline(s, "video", "vo-drop-frame-count", "   VO:")
    end
    if append_property(s, "video", "fps", "FPS:", " (specified)") then
        append_property_inline(s, "video", "estimated-vf-fps", "", " (estimated)", true, true)
    end
    if append_property(s, "video", "video-params/w", "Native Resolution:") then
        append_property_inline(s, "video", "video-params/h", " x ", "", true, true, true)
    end
    append_property(s, "video", "window-scale", "Window Scale:")
    append_property(s, "video", "video-params/aspect", "Aspect Ratio:")
    append_property(s, "video", "video-params/pixelformat", "Pixel format:")
    append_property(s, "video", "video-params/colormatrix", "Colormatrix:")
    append_property(s, "video", "video-params/primaries", "Primaries:")
    append_property(s, "video", "video-params/colorlevels", "Levels:")
    append_property(s, "video", "packet-video-bitrate", "Bitrate:", " kbps")

    s.video = s.video .. o.nl .. o.nl
end


function add_audio(s)
    s.audio = ""
    local r = mp.get_property_osd("audio-codec")
    if not r or r == "no" or r == "" then
        return
    end
    s.audio = s.audio .. b("Audio:") .. o.kv_sep .. no_ASS(r)

    append_property(s, "audio", "audio-samplerate", "Sample Rate:")
    append_property(s, "audio", "audio-channels", "Channels:")
    append_property(s, "audio", "packet-audio-bitrate", "Bitrate:", " kbps")

    s.audio = s.audio .. o.nl .. o.nl
end


function add_header(s)
    if o.custom_header and o.custom_header ~= "" then
        s.header = set_ASS(true) .. o.custom_header
    else
        s.header = string.format([[%s{\\fs%d}{\\fn%s}{\\bord%f}{\\3c&H%s&}{\\1c&H%s&}
                                 {\\alpha&H%s&}{\\xshad%f}{\\yshad%f}{\\4c&H%s&}]],
                        set_ASS(true), o.font_size, o.font, o.border_size, 
                        o.border_color, o.font_color, o.alpha, o.shadow_x_offset, 
                        o.shadow_y_offset, o.shadow_color)
    end
end


function append_property(s, sec, prop, prefix, suffix)
    local ret = mp.get_property_osd(prop)
    if ret == nil or ret == "" then
        return false
    end

    local suf = suffix or ""
    local desc = prefix or ""
    desc = no_prefix_markup and desc or b(desc)
    s[sec] = s[sec] .. o.nl .. o.prop_indent .. b(desc) .. o.kv_sep .. no_ASS(ret) .. suf
    return true
end


-- one could merge this into append_property, it's just a bit more verbose this way imo
function append_property_inline(s, sec, prop, prefix, suffix, no_prefix_markup, no_prefix_sep, no_indent)
    local ret = mp.get_property_osd(prop)
    if ret == nil or ret == "" then
        return false
    end

    local suf = suffix or ""
    local prefix_sep = no_prefix_sep and "" or o.kv_sep
    local indent = no_indent and "" or o.kv_sep
    local desc = prefix or ""
    desc = no_prefix_markup and desc or b(desc)
    s[sec] = s[sec] .. indent .. desc .. prefix_sep .. no_ASS(ret) .. suf
    return true
end


function no_ASS(t)
    return set_ASS(false) .. t .. set_ASS(true)
end


function set_ASS(b)
    return mp.get_property_osd("osd-ass-cc/" .. (b and "0" or "1"))
end


function join_stats(s)
    return s.header .. s.file .. s.video .. s.audio
end


function b(t)
    return "{\\b1}" .. t .. "{\\b0}"
end
function i(t)
    return "{\\i1}" .. t .. "{\\i0}"
end
function u(t)
    return "{\\u1}" .. t .. "{\\u0}"
end


mp.add_key_binding("i", "stats", main, {repeatable=true})
