-- Display some stats.
--
-- Please consult the readme for information about usage and configuration:
-- https://github.com/Argon-/mpv-stats
--
-- Please note: not every property is always available and therefore not always
-- visible.

local mp = require 'mp'
local options = require 'mp.options'
local utils = require 'mp.utils'

-- Options
local o = {
    -- Default key bindings
    key_page_1 = "1",
    key_page_2 = "2",
    key_page_3 = "3",
    key_page_4 = "4",
    key_page_0 = "0",
    -- For pages which support scrolling
    key_scroll_up = "UP",
    key_scroll_down = "DOWN",
    scroll_lines = 1,

    duration = 4,
    redraw_delay = 1,                -- acts as duration in the toggling case
    ass_formatting = true,
    persistent_overlay = false,      -- whether the stats can be overwritten by other output
    print_perfdata_passes = false,   -- when true, print the full information about all passes
    filter_params_max_length = 100,  -- a filter list longer than this many characters will be shown one filter per line instead
    show_frame_info = false,          -- whether to show the current frame info
    term_width_limit = -1,           -- overwrites the terminal width
    term_height_limit = -1,          -- overwrites the terminal height
    debug = false,

    -- Graph options and style
    plot_perfdata = true,
    plot_vsync_ratio = true,
    plot_vsync_jitter = true,
    plot_tonemapping_lut = false,
    skip_frames = 5,
    global_max = true,
    flush_graph_data = true,         -- clear data buffers when toggling
    plot_bg_border_color = "0000FF",
    plot_bg_color = "262626",
    plot_color = "FFFFFF",

    -- Text style
    font = "sans-serif",
    font_mono = "monospace",   -- monospaced digits are sufficient
    font_size = 8,
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
    ass_nl = "\\N",
    ass_indent = "\\h\\h\\h\\h\\h",
    ass_prefix_sep = "\\h\\h",
    ass_b1 = "{\\b1}",
    ass_b0 = "{\\b0}",
    ass_it1 = "{\\i1}",
    ass_it0 = "{\\i0}",
    -- Without ASS
    no_ass_nl = "\n",
    no_ass_indent = "\t",
    no_ass_prefix_sep = " ",
    no_ass_b1 = "\027[1m",
    no_ass_b0 = "\027[0m",
    no_ass_it1 = "\027[3m",
    no_ass_it0 = "\027[0m",

    bindlist = "no",  -- print page 4 to the terminal on startup and quit mpv
}
options.read_options(o)

o.term_width_limit = tonumber(o.term_width_limit) or -1
o.term_height_limit = tonumber(o.term_height_limit) or -1
if o.term_width_limit < 0 then
    o.term_width_limit = nil
end
if o.term_height_limit < 0 then
    o.term_height_limit = nil
end

local format = string.format
local max = math.max
local min = math.min

-- Function used to record performance data
local recorder = nil
-- Timer used for redrawing (toggling) and clearing the screen (oneshot)
local display_timer = nil
-- Timer used to update cache stats.
local cache_recorder_timer = nil
-- Current page and <page key>:<page function> mappings
local curr_page = o.key_page_1
local pages = {}
local scroll_bound = false
local tm_viz_prev = nil
-- Save these sequences locally as we'll need them a lot
local ass_start = mp.get_property_osd("osd-ass-cc/0")
local ass_stop = mp.get_property_osd("osd-ass-cc/1")
-- Ring buffers for the values used to construct a graph.
-- .pos denotes the current position, .len the buffer length
-- .max is the max value in the corresponding buffer
local vsratio_buf, vsjitter_buf
local function init_buffers()
    vsratio_buf = {0, pos = 1, len = 50, max = 0}
    vsjitter_buf = {0, pos = 1, len = 50, max = 0}
end
local cache_ahead_buf, cache_speed_buf
local perf_buffers = {}

local function graph_add_value(graph, value)
    graph.pos = (graph.pos % graph.len) + 1
    graph[graph.pos] = value
    graph.max = max(graph.max, value)
end

local function no_ASS(t)
    if not o.use_ass then
        return t
    elseif not o.persistent_overlay then
        -- mp.osd_message supports ass-escape using osd-ass-cc/{0|1}
        return ass_stop .. t .. ass_start
    else
        return mp.command_native({"escape-ass", tostring(t)})
    end
end


local function b(t)
    return o.b1 .. t .. o.b0
end


local function it(t)
    return o.it1 .. t .. o.it0
end


local function text_style()
    if not o.use_ass then
        return ""
    end
    if o.custom_header and o.custom_header ~= "" then
        return o.custom_header
    else
        local has_shadow = mp.get_property('osd-back-color'):sub(2, 3) == '00'
        return format("{\\r\\an7\\fs%d\\fn%s\\bord%f\\3c&H%s&" ..
                      "\\1c&H%s&\\1a&H%s&\\3a&H%s&" ..
                      (has_shadow and "\\4a&H%s&\\xshad%f\\yshad%f\\4c&H%s&}" or "}"),
                      o.font_size, o.font, o.border_size,
                      o.border_color, o.font_color, o.alpha, o.alpha, o.alpha,
                      o.shadow_x_offset, o.shadow_y_offset, o.shadow_color)
    end
end


local function has_vo_window()
    return mp.get_property_native("vo-configured") and mp.get_property_native("video-osd")
end


-- Generate a graph from the given values.
-- Returns an ASS formatted vector drawing as string.
--
-- values: Array/table of numbers representing the data. Used like a ring buffer
--         it will get iterated backwards `len` times starting at position `i`.
-- i     : Index of the latest data value in `values`.
-- len   : The length/amount of numbers in `values`.
-- v_max : The maximum number in `values`. It is used to scale all data
--         values to a range of 0 to `v_max`.
-- v_avg : The average number in `values`. It is used to try and center graphs
--         if possible. May be left as nil
-- scale : A value that will be multiplied with all data values.
-- x_tics: Horizontal width multiplier for the steps
local function generate_graph(values, i, len, v_max, v_avg, scale, x_tics)
    -- Check if at least one value exists
    if not values[i] then
        return ""
    end

    local x_max = (len - 1) * x_tics
    local y_offset = o.border_size
    local y_max = o.font_size * 0.66
    local x = 0

    if v_max > 0 then
        -- try and center the graph if possible, but avoid going above `scale`
        if v_avg and v_avg > 0 then
            scale = min(scale, v_max / (2 * v_avg))
        end
        scale = scale * y_max / v_max
    end  -- else if v_max==0 then all values are 0 and scale doesn't matter

    local s = {format("m 0 0 n %f %f l ", x, y_max - scale * values[i])}
    i = ((i - 2) % len) + 1

    for p = 1, len - 1 do
        if values[i] then
            x = x - x_tics
            s[#s+1] = format("%f %f ", x, y_max - scale * values[i])
        end
        i = ((i - 2) % len) + 1
    end

    s[#s+1] = format("%f %f %f %f", x, y_max, 0, y_max)

    local bg_box = format("{\\bord0.5}{\\3c&H%s&}{\\1c&H%s&}m 0 %f l %f %f %f 0 0 0",
                          o.plot_bg_border_color, o.plot_bg_color, y_max, x_max, y_max, x_max)
    return format("%s{\\r}{\\rDefault}{\\pbo%f}{\\shad0}{\\alpha&H00}{\\p1}%s{\\p0}{\\bord0}{\\1c&H%s}{\\p1}%s{\\p0}%s",
                  o.prefix_sep, y_offset, bg_box, o.plot_color, table.concat(s), text_style())
end


local function append(s, str, attr)
    if not str then
        return false
    end
    attr.prefix_sep = attr.prefix_sep or o.prefix_sep
    attr.indent = attr.indent or o.indent
    attr.nl = attr.nl or o.nl
    attr.suffix = attr.suffix or ""
    attr.prefix = attr.prefix or ""
    attr.no_prefix_markup = attr.no_prefix_markup or false
    attr.prefix = attr.no_prefix_markup and attr.prefix or b(attr.prefix)
    s[#s+1] = format("%s%s%s%s%s%s", attr.nl, attr.indent,
                     attr.prefix, attr.prefix_sep, no_ASS(str), attr.suffix)
    return true
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
    return append(s, ret, attr)
end

local function sorted_keys(t, comp_fn)
    local keys = {}
    for k,_ in pairs(t) do
        keys[#keys+1] = k
    end
    table.sort(keys, comp_fn)
    return keys
end

local function scroll_hint()
    local hint = format("(hint: scroll with %s/%s)", o.key_scroll_up, o.key_scroll_down)
    if not o.use_ass then return " " .. hint end
    return format(" {\\fs%s}%s{\\fs%s}", o.font_size * 0.66, hint, o.font_size)
end

local function append_perfdata(header, s, dedicated_page, print_passes)
    local vo_p = mp.get_property_native("vo-passes")
    if not vo_p then
        return
    end

    local ds = mp.get_property_bool("display-sync-active", false)
    local target_fps = ds and mp.get_property_number("display-fps", 0)
                       or mp.get_property_number("container-fps", 0)
    if target_fps > 0 then target_fps = 1 / target_fps * 1e9 end

    -- Sums of all last/avg/peak values
    local last_s, avg_s, peak_s = {}, {}, {}
    for frame, data in pairs(vo_p) do
        last_s[frame], avg_s[frame], peak_s[frame] = 0, 0, 0
        for _, pass in ipairs(data) do
            last_s[frame] = last_s[frame] + pass["last"]
            avg_s[frame]  = avg_s[frame]  + pass["avg"]
            peak_s[frame] = peak_s[frame] + pass["peak"]
        end
    end

    -- Pretty print measured time
    local function pp(i)
        -- rescale to microseconds for a saner display
        return format("%5d", i / 1000)
    end

    -- Format n/m with a font weight based on the ratio
    local function p(n, m)
        local i = 0
        if m > 0 then
            i = tonumber(n) / m
        end
        -- Calculate font weight. 100 is minimum, 400 is normal, 700 bold, 900 is max
        local w = (700 * math.sqrt(i)) + 200
        return format("{\\b%d}%2d%%{\\b0}", w, i * 100)
    end

    -- ensure that the fixed title is one element and every scrollable line is
    -- also one single element.
    local h = dedicated_page and header or s
    h[#h+1] = format("%s%s%s%s{\\fs%s}%s{\\fs%s}%s",
                     dedicated_page and "" or o.nl, dedicated_page and "" or o.indent,
                     b("Frame Timings:"), o.prefix_sep, o.font_size * 0.66,
                     "(last/average/peak μs)", o.font_size,
                     dedicated_page and scroll_hint() or "")

    for _,frame in ipairs(sorted_keys(vo_p)) do  -- ensure fixed display order
        local data = vo_p[frame]
        local f = "%s%s%s{\\fn%s}%s / %s / %s %s%s{\\fn%s}%s%s%s"

        if print_passes then
            s[#s+1] = format("%s%s%s:", o.nl, o.indent,
                             b(frame:gsub("^%l", string.upper)))

            for _, pass in ipairs(data) do
                s[#s+1] = format(f, o.nl, o.indent, o.indent,
                                 o.font_mono, pp(pass["last"]),
                                 pp(pass["avg"]), pp(pass["peak"]),
                                 o.prefix_sep .. "\\h\\h", p(pass["last"], last_s[frame]),
                                 o.font, o.prefix_sep, o.prefix_sep, pass["desc"])

                if o.plot_perfdata and o.use_ass then
                    -- use the same line that was already started for this iteration
                    s[#s] = s[#s] ..
                              generate_graph(pass["samples"], pass["count"],
                                             pass["count"], pass["peak"],
                                             pass["avg"], 0.9, 0.25)
                end
            end

            -- Print sum of timing values as "Total"
            s[#s+1] = format(f, o.nl, o.indent, o.indent,
                             o.font_mono, pp(last_s[frame]),
                             pp(avg_s[frame]), pp(peak_s[frame]),
                             o.prefix_sep, b("Total"), o.font, "", "", "")
        else
            -- for the simplified view, we just print the sum of each pass
            s[#s+1] = format(f, o.nl, o.indent, o.indent, o.font_mono,
                            pp(last_s[frame]), pp(avg_s[frame]), pp(peak_s[frame]),
                            "", "", o.font, o.prefix_sep, o.prefix_sep,
                            frame:gsub("^%l", string.upper))
        end
    end
end

-- command prefix tokens to strip - includes generic property commands
local cmd_prefixes = {
    osd_auto=1, no_osd=1, osd_bar=1, osd_msg=1, osd_msg_bar=1, raw=1, sync=1,
    async=1, expand_properties=1, repeatable=1, set=1, add=1, multiply=1,
    toggle=1, cycle=1, cycle_values=1, ["!reverse"]=1, change_list=1,
}
-- commands/writable-properties prefix sub-words (followed by -) to strip
local name_prefixes = {
    define=1, delete=1, enable=1, disable=1, dump=1, write=1, drop=1, revert=1,
    ab=1, hr=1, secondary=1, current=1,
}
-- extract a command "subject" from a command string, by removing all
-- generic prefix tokens and then returning the first interesting sub-word
-- of the next token. For target-script name we also check another token.
-- The tokenizer works fine for things we care about - valid mpv commands,
-- properties and script names, possibly quoted, white-space[s]-separated.
-- It's decent in practice, and worst case is "incorrect" subject.
local function cmd_subject(cmd)
    cmd = cmd:gsub(";.*", ""):gsub("%-", "_")  -- only first cmd, s/-/_/
    local TOKEN = '^%s*["\']?([%w_!]*)'  -- captures+ends before (maybe) final "
    local tok, sname, subw

    repeat tok, cmd = cmd:match(TOKEN .. '["\']?(.*)')
    until not cmd_prefixes[tok]
    -- tok is the 1st non-generic command/property name token, cmd is the rest

    sname = tok == "script_message_to" and cmd:match(TOKEN)
         or tok == "script_binding" and cmd:match(TOKEN .. "/")
    if sname and sname ~= "" then
        return "script: " .. sname
    end

    -- return the first sub-word of tok which is not a useless prefix
    repeat subw, tok = tok:match("([^_]*)_?(.*)")
    until tok == "" or not name_prefixes[subw]
    return subw:len() > 1 and subw or "[unknown]"
end

-- key names are valid UTF-8, ascii7 except maybe the last/only codepoint.
-- we count codepoints and ignore wcwidth. no need for grapheme clusters.
-- our error for alignment is at most one cell (if last CP is double-width).
-- (if k was valid but arbitrary: we'd count all bytes <0x80 or >=0xc0)
local function keyname_cells(k)
    local klen = k:len()
    if klen > 1 and k:byte(klen) >= 0x80 then  -- last/only CP is not ascii7
        repeat klen = klen-1
        until klen == 1 or k:byte(klen) >= 0xc0  -- last CP begins at klen
    end
    return klen
end

local function get_kbinfo_lines()
    -- active keys: only highest priority of each key, and not our (stats) keys
    local bindings = mp.get_property_native("input-bindings", {})
    local active = {}  -- map: key-name -> bind-info
    for _, bind in pairs(bindings) do
        if bind.priority >= 0 and (
               not active[bind.key] or
               (active[bind.key].is_weak and not bind.is_weak) or
               (bind.is_weak == active[bind.key].is_weak and
                bind.priority > active[bind.key].priority)
           ) and not bind.cmd:find("script-binding stats/__forced_", 1, true)
        then
            active[bind.key] = bind
        end
    end

    -- make an array, find max key len, add sort keys (.subject/.mods[_count])
    local ordered = {}
    local kspaces = ""  -- as many spaces as the longest key name
    for _, bind in pairs(active) do
        bind.subject = cmd_subject(bind.cmd)
        if bind.subject ~= "ignore" then
            ordered[#ordered+1] = bind
            _,_, bind.mods = bind.key:find("(.*)%+.")
            _, bind.mods_count = bind.key:gsub("%+.", "")
            if bind.key:len() > kspaces:len() then
                kspaces = string.rep(" ", bind.key:len())
            end
        end
    end

    local function align_right(key)
        return kspaces:sub(keyname_cells(key)) .. key
    end

    -- sort by: subject, mod(ifier)s count, mods, key-len, lowercase-key, key
    table.sort(ordered, function(a, b)
        if a.subject ~= b.subject then
            return a.subject < b.subject
        elseif a.mods_count ~= b.mods_count then
            return a.mods_count < b.mods_count
        elseif a.mods ~= b.mods then
            return a.mods < b.mods
        elseif a.key:len() ~= b.key:len() then
            return a.key:len() < b.key:len()
        elseif a.key:lower() ~= b.key:lower() then
            return a.key:lower() < b.key:lower()
        else
            return a.key > b.key  -- only case differs, lowercase first
        end
    end)

    -- key/subject pre/post formatting for terminal/ass.
    -- key/subject alignment uses spaces (with mono font if ass)
    -- word-wrapping is disabled for ass, or cut at 79 for the terminal
    local LTR = string.char(0xE2, 0x80, 0x8E)  -- U+200E Left To Right mark
    local term = not o.use_ass
    local kpre = term and "" or format("{\\q2\\fn%s}%s", o.font_mono, LTR)
    local kpost = term and " " or format(" {\\fn%s}", o.font)
    local spre = term and kspaces .. "   "
                       or format("{\\q2\\fn%s}%s   {\\fn%s}{\\fs%d\\u1}",
                                 o.font_mono, kspaces, o.font, 1.3*o.font_size)
    local spost = term and "" or format("{\\u0\\fs%d}", o.font_size)

    -- create the display lines
    local info_lines = {}
    local subject = nil
    for _, bind in ipairs(ordered) do
        if bind.subject ~= subject then  -- new subject (title)
            subject = bind.subject
            append(info_lines, "", {})
            append(info_lines, "", { prefix = spre .. subject .. spost })
        end
        if bind.comment then
            bind.cmd = bind.cmd .. "  # " .. bind.comment
        end
        append(info_lines, bind.cmd, { prefix = kpre .. no_ASS(align_right(bind.key)) .. kpost })
    end
    return info_lines
end

local function append_general_perfdata(s)
    for i, data in ipairs(mp.get_property_native("perf-info") or {}) do
        append(s, data.text or data.value, {prefix="["..tostring(i).."] "..data.name..":"})

        if o.plot_perfdata and o.use_ass and data.value then
            buf = perf_buffers[data.name]
            if not buf then
                buf = {0, pos = 1, len = 50, max = 0}
                perf_buffers[data.name] = buf
            end
            graph_add_value(buf, data.value)
            s[#s] = s[#s] .. generate_graph(buf, buf.pos, buf.len, buf.max, nil, 0.8, 1)
        end
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

    append_property(s, "mistimed-frame-count", {prefix="Mistimed:", nl="",
                                                indent=o.prefix_sep .. o.prefix_sep})
    append_property(s, "vo-delayed-frame-count", {prefix="Delayed:", nl="",
                                                  indent=o.prefix_sep .. o.prefix_sep})

    -- As we need to plot some graphs we print jitter and ratio on their own lines
    if not display_timer.oneshot and (o.plot_vsync_ratio or o.plot_vsync_jitter) and o.use_ass then
        local ratio_graph = ""
        local jitter_graph = ""
        if o.plot_vsync_ratio then
            ratio_graph = generate_graph(vsratio_buf, vsratio_buf.pos, vsratio_buf.len, vsratio_buf.max, nil, 0.8, 1)
        end
        if o.plot_vsync_jitter then
            jitter_graph = generate_graph(vsjitter_buf, vsjitter_buf.pos, vsjitter_buf.len, vsjitter_buf.max, nil, 0.8, 1)
        end
        append_property(s, "vsync-ratio", {prefix="VSync Ratio:", suffix=o.prefix_sep .. ratio_graph})
        append_property(s, "vsync-jitter", {prefix="VSync Jitter:", suffix=o.prefix_sep .. jitter_graph})
    else
        -- Since no graph is needed we can print ratio/jitter on the same line and save some space
        local vr = append_property(s, "vsync-ratio", {prefix="VSync Ratio:"})
        append_property(s, "vsync-jitter", {prefix="VSync Jitter:",
                            nl=vr and "" or o.nl,
                            indent=vr and o.prefix_sep .. o.prefix_sep})
    end
end


local function append_filters(s, prop, prefix)
    local length = 0
    local filters = {}

    for _,f in ipairs(mp.get_property_native(prop, {})) do
        local n = f.name
        if f.enabled ~= nil and not f.enabled then
            n = n .. " (disabled)"
        end

        if f.label ~= nil then
            n = "@" .. f.label .. ": " .. n
        end

        local p = {}
        for _,key in ipairs(sorted_keys(f.params)) do
            p[#p+1] = key .. "=" .. f.params[key]
        end
        if #p > 0 then
            p = " [" .. table.concat(p, " ") .. "]"
        else
            p = ""
        end

        length = length + n:len() + p:len()
        filters[#filters+1] = no_ASS(n) .. it(no_ASS(p))
    end

    if #filters > 0 then
        local ret
        if length < o.filter_params_max_length then
            ret = table.concat(filters, ", ")
        else
            local sep = o.nl .. o.indent .. o.indent
            ret = sep .. table.concat(filters, sep)
        end
        s[#s+1] = o.nl .. o.indent .. b(prefix) .. o.prefix_sep .. ret
    end
end


local function add_header(s)
    s[#s+1] = text_style()
end


local function add_file(s)
    append(s, "", {prefix="File:", nl="", indent=""})
    append_property(s, "filename", {prefix_sep="", nl="", indent=""})
    if not (mp.get_property_osd("filename") == mp.get_property_osd("media-title")) then
        append_property(s, "media-title", {prefix="Title:"})
    end

    local editions = mp.get_property_number("editions")
    local edition = mp.get_property_number("current-edition")
    local ed_cond = (edition and editions > 1)
    if ed_cond then
        append_property(s, "edition-list/" .. tostring(edition) .. "/title",
                       {prefix="Edition:"})
        append_property(s, "edition-list/count",
                        {prefix="(" .. tostring(edition + 1) .. "/", suffix=")", nl="",
                         indent=" ", prefix_sep=" ", no_prefix_markup=true})
    end

    local ch_index = mp.get_property_number("chapter")
    if ch_index and ch_index >= 0 then
        append_property(s, "chapter-list/" .. tostring(ch_index) .. "/title", {prefix="Chapter:",
                        nl=ed_cond and "" or o.nl})
        append_property(s, "chapter-list/count",
                        {prefix="(" .. tostring(ch_index + 1) .. " /", suffix=")", nl="",
                         indent=" ", prefix_sep=" ", no_prefix_markup=true})
    end

    local fs = append_property(s, "file-size", {prefix="Size:"})
    append_property(s, "file-format", {prefix="Format/Protocol:",
                                       nl=fs and "" or o.nl,
                                       indent=fs and o.prefix_sep .. o.prefix_sep})

    local demuxer_cache = mp.get_property_native("demuxer-cache-state", {})
    if demuxer_cache["fw-bytes"] then
        demuxer_cache = demuxer_cache["fw-bytes"] -- returns bytes
    else
        demuxer_cache = 0
    end
    local demuxer_secs = mp.get_property_number("demuxer-cache-duration", 0)
    if demuxer_cache + demuxer_secs > 0 then
        append(s, utils.format_bytes_humanized(demuxer_cache), {prefix="Total Cache:"})
        append(s, format("%.1f", demuxer_secs), {prefix="(", suffix=" sec)", nl="",
               no_prefix_markup=true, prefix_sep="", indent=o.prefix_sep})
    end
end


local function crop_noop(w, h, r)
    return r["crop-x"] == 0 and r["crop-y"] == 0 and
           r["crop-w"] == w and r["crop-h"] == h
end


local function crop_equal(r, ro)
    return r["crop-x"] == ro["crop-x"] and r["crop-y"] == ro["crop-y"] and
           r["crop-w"] == ro["crop-w"] and r["crop-h"] == ro["crop-h"]
end


local function append_resolution(s, r, prefix, w_prop, h_prop, video_res)
    if not r then
        return
    end
    w_prop = w_prop or "w"
    h_prop = h_prop or "h"
    if append(s, r[w_prop], {prefix=prefix}) then
        append(s, r[h_prop], {prefix="x", nl="", indent=" ", prefix_sep=" ",
                           no_prefix_markup=true})
        if r["aspect"] ~= nil and not video_res then
            append(s, format("%.2f:1", r["aspect"]), {prefix="", nl="", indent="",
                                                      no_prefix_markup=true})
            append(s, r["aspect-name"], {prefix="(", suffix=")", nl="", indent=" ",
                                         prefix_sep="", no_prefix_markup=true})
        end
        if r["sar"] ~= nil and video_res then
            append(s, format("%.2f:1", r["sar"]), {prefix="", nl="", indent="",
                                                      no_prefix_markup=true})
            append(s, r["sar-name"], {prefix="(", suffix=")", nl="", indent=" ",
                                         prefix_sep="", no_prefix_markup=true})
        end
        if r["s"] then
            append(s, format("%.2f", r["s"]), {prefix="(", suffix="x)", nl="",
                                               indent=o.prefix_sep, prefix_sep="",
                                               no_prefix_markup=true})
        end
        -- We can skip crop if it is the same as video decoded resolution
        if r["crop-w"] and (not video_res or
                            not crop_noop(r[w_prop], r[h_prop], r)) then
            append(s, format("[x: %d, y: %d, w: %d, h: %d]",
                            r["crop-x"], r["crop-y"], r["crop-w"], r["crop-h"]),
                            {prefix="", nl="", indent="", no_prefix_markup=true})
        end
    end
end


local function pq_eotf(x)
    if not x then
        return x;
    end

    local PQ_M1 = 2610.0 / 4096 * 1.0 / 4
    local PQ_M2 = 2523.0 / 4096 * 128
    local PQ_C1 = 3424.0 / 4096
    local PQ_C2 = 2413.0 / 4096 * 32
    local PQ_C3 = 2392.0 / 4096 * 32

    x = x ^ (1.0 / PQ_M2)
    x = max(x - PQ_C1, 0.0) / (PQ_C2 - PQ_C3 * x)
    x = x ^ (1.0 / PQ_M1)
    x = x * 10000.0

    return x
end


local function append_hdr(s, hdr, video_out)
    if not hdr then
        return
    end

    local function should_show(val)
        return val and val ~= 203 and val > 0
    end

    -- If we are printing video out parameters it is just display, not mastering
    local display_prefix = video_out and "Display:" or "Mastering display:"

    local indent = ""

    if should_show(hdr["max-cll"]) or should_show(hdr["max-luma"]) then
        append(s, "", {prefix="HDR10:"})
        if hdr["min-luma"] and should_show(hdr["max-luma"]) then
            -- libplacebo uses close to zero values as "defined zero"
            hdr["min-luma"] = hdr["min-luma"] <= 1e-6 and 0 or hdr["min-luma"]
            append(s, format("%.2g / %.0f", hdr["min-luma"], hdr["max-luma"]),
                {prefix=display_prefix, suffix=" cd/m²", nl="", indent=indent})
            indent = o.prefix_sep .. o.prefix_sep
        end
        if should_show(hdr["max-cll"]) then
            append(s, hdr["max-cll"], {prefix="MaxCLL:", suffix=" cd/m²", nl="",
                                       indent=indent})
            indent = o.prefix_sep .. o.prefix_sep
        end
        if hdr["max-fall"] and hdr["max-fall"] > 0 then
            append(s, hdr["max-fall"], {prefix="MaxFALL:", suffix=" cd/m²", nl="",
                                        indent=indent})
        end
    end

    indent = o.prefix_sep .. o.prefix_sep

    if hdr["scene-max-r"] or hdr["scene-max-g"] or
       hdr["scene-max-b"] or hdr["scene-avg"] then
        append(s, "", {prefix="HDR10+:"})
        append(s, format("%.1f / %.1f / %.1f", hdr["scene-max-r"] or 0,
                         hdr["scene-max-g"] or 0, hdr["scene-max-b"] or 0),
               {prefix="MaxRGB:", suffix=" cd/m²", nl="", indent=""})
        append(s, format("%.1f", hdr["scene-avg"] or 0),
               {prefix="Avg:", suffix=" cd/m²", nl="", indent=indent})
    end

    if hdr["max-pq-y"] and hdr["avg-pq-y"] then
        append(s, "", {prefix="PQ(Y):"})
        append(s, format("%.2f cd/m² (%.2f%% PQ)", pq_eotf(hdr["max-pq-y"]),
                         hdr["max-pq-y"] * 100), {prefix="Max:", nl="",
                         indent=""})
        append(s, format("%.2f cd/m² (%.2f%% PQ)", pq_eotf(hdr["avg-pq-y"]),
                         hdr["avg-pq-y"] * 100), {prefix="Avg:", nl="",
                         indent=indent})
    end
end


local function append_img_params(s, r, ro)
    if not r then
        return
    end

    append_resolution(s, r, "Resolution:", "w", "h", true)
    if ro and (r["w"] ~= ro["dw"] or r["h"] ~= ro["dh"]) then
        if ro["crop-w"] and (crop_noop(r["w"], r["h"], ro) or crop_equal(r, ro)) then
            ro["crop-w"] = nil
        end
        append_resolution(s, ro, "Output Resolution:", "dw", "dh")
    end

    local indent = o.prefix_sep .. o.prefix_sep
    r = ro or r

    local pixel_format = r["hw-pixelformat"] or r["pixelformat"]
    append(s, pixel_format, {prefix="Format:"})
    append(s, r["colorlevels"], {prefix="Levels:", nl="", indent=indent})
    if r["chroma-location"] and r["chroma-location"] ~= "unknown" then
        append(s, r["chroma-location"], {prefix="Chroma Loc:", nl="", indent=indent})
    end

    -- Group these together to save vertical space
    append(s, r["colormatrix"], {prefix="Colormatrix:"})
    append(s, r["primaries"], {prefix="Primaries:", nl="", indent=indent})
    append(s, r["gamma"], {prefix="Transfer:", nl="", indent=indent})
end


local function append_fps(s, prop, eprop)
    local fps = mp.get_property_osd(prop)
    local efps = mp.get_property_osd(eprop)
    local single = fps ~= "" and efps ~= "" and fps == efps
    local unit = prop == "display-fps" and " Hz" or " fps"
    local suffix = single and "" or " (specified)"
    local esuffix = single and "" or " (estimated)"
    local prefix = prop == "display-fps" and "Refresh Rate:" or "Frame Rate:"
    local nl = o.nl
    local indent = o.indent

    if fps ~= "" and append(s, fps, {prefix=prefix, suffix=unit .. suffix}) then
        prefix = ""
        nl = ""
        indent = ""
    end

    if not single and efps ~= "" then
        append(s, efps,
               {prefix=prefix, suffix=unit .. esuffix, nl=nl, indent=indent})
    end
end


local function add_video_out(s)
    local vo = mp.get_property_native("current-vo")
    if not vo then
        return
    end

    append(s, "", {prefix=o.nl .. o.nl .. "Display:", nl="", indent=""})
    append(s, vo, {prefix_sep="", nl="", indent=""})
    append_property(s, "display-names", {prefix_sep="", prefix="(", suffix=")",
                                         no_prefix_markup=true, nl="", indent=" "})
    append(s, mp.get_property_native("current-gpu-context"),
           {prefix="Context:", nl="", indent=o.prefix_sep .. o.prefix_sep})
    append_property(s, "avsync", {prefix="A-V:"})
    append_fps(s, "display-fps", "estimated-display-fps")
    if append_property(s, "decoder-frame-drop-count",
                       {prefix="Dropped Frames:", suffix=" (decoder)"}) then
        append_property(s, "frame-drop-count", {suffix=" (output)", nl="", indent=""})
    end
    append_display_sync(s)
    append_perfdata(nil, s, false, o.print_perfdata_passes)

    if mp.get_property_native("deinterlace-active") then
        append_property(s, "deinterlace", {prefix="Deinterlacing:"})
    end

    local scale = nil
    if not mp.get_property_native("fullscreen") then
        scale = mp.get_property_native("current-window-scale")
    end

    local od = mp.get_property_native("osd-dimensions")
    local rt = mp.get_property_native("video-target-params")
    r = rt or {}

    -- Add window scale
    r["s"] = scale
    r["crop-x"] = od["ml"]
    r["crop-y"] = od["mt"]
    r["crop-w"] = od["w"] - od["ml"] - od["mr"]
    r["crop-h"] = od["h"] - od["mt"] - od["mb"]

    if not rt then
        r["w"] = r["crop-w"]
        r["h"] = r["crop-h"]
        append_resolution(s, r, "Resolution:", "w", "h", true)
        return
    end

    append_img_params(s, r)
    append_hdr(s, r, true)
end


local function add_video(s)
    local r = mp.get_property_native("video-params")
    local ro = mp.get_property_native("video-out-params")
    -- in case of e.g. lavfi-complex there can be no input video, only output
    if not r then
        r = ro
    end
    if not r then
        return
    end

    local osd_dims = mp.get_property_native("osd-dimensions")
    local scaled_width = osd_dims["w"] - osd_dims["ml"] - osd_dims["mr"]
    local scaled_height = osd_dims["h"] - osd_dims["mt"] - osd_dims["mb"]

    append(s, "", {prefix=o.nl .. o.nl .. "Video:", nl="", indent=""})
    if append_property(s, "video-codec", {prefix_sep="", nl="", indent=""}) then
        append_property(s, "hwdec-current", {prefix="HW:", nl="",
                        indent=o.prefix_sep .. o.prefix_sep,
                        no_prefix_markup=false, suffix=""}, {no=true, [""]=true})
    end
    local has_prefix = false
    if o.show_frame_info then
        if append_property(s, "estimated-frame-number", {prefix="Frame:"}) then
            append_property(s, "estimated-frame-count", {indent=" / ", nl="",
                                                        prefix_sep=""})
            has_prefix = true
        end
        local frame_info = mp.get_property_native("video-frame-info")
        if frame_info and frame_info["picture-type"] then
            local attrs = has_prefix and {prefix="(", suffix=")", indent=" ", nl="",
                                          prefix_sep="", no_prefix_markup=true}
                                      or {prefix="Picture Type:"}
            append(s, frame_info["picture-type"], attrs)
            has_prefix = true
        end
        if frame_info and frame_info["interlaced"] then
            local attrs = has_prefix and {indent=" ", nl="", prefix_sep=""}
                                      or {prefix="Picture Type:"}
            append(s, "Interlaced", attrs)
        end
    end

    if mp.get_property_native("current-tracks/video/image") == false then
        append_fps(s, "container-fps", "estimated-vf-fps")
    end
    append_img_params(s, r, ro)
    append_hdr(s, ro)
    append_property(s, "packet-video-bitrate", {prefix="Bitrate:", suffix=" kbps"})
    append_filters(s, "vf", "Filters:")
end


local function add_audio(s)
    local r = mp.get_property_native("audio-params")
    -- in case of e.g. lavfi-complex there can be no input audio, only output
    local ro = mp.get_property_native("audio-out-params") or r
    r = r or ro
    if not r then
        return
    end

    local merge = function(r, ro, prop)
        local a = r[prop] or ro[prop]
        local b = ro[prop] or r[prop]
        return (a == b or a == nil) and a or (a .. " ➜ " .. b)
    end

    append(s, "", {prefix=o.nl .. o.nl .. "Audio:", nl="", indent=""})
    append_property(s, "audio-codec", {prefix_sep="", nl="", indent=""})
    append_property(s, "current-ao", {prefix="AO:", nl="",
                                      indent=o.prefix_sep .. o.prefix_sep})
    local dev = append_property(s, "audio-device", {prefix="Device:"})
    local ao_mute = mp.get_property_native("ao-mute") and " (Muted)" or ""
    append_property(s, "ao-volume", {prefix="AO Volume:", suffix="%" .. ao_mute,
                                     nl=dev and "" or o.nl,
                                     indent=dev and o.prefix_sep .. o.prefix_sep})
    if math.abs(mp.get_property_native("audio-delay")) > 1e-6 then
        append_property(s, "audio-delay", {prefix="A-V delay:"})
    end
    local cc = append(s, merge(r, ro, "channel-count"), {prefix="Channels:"})
    append(s, merge(r, ro, "format"), {prefix="Format:", nl=cc and "" or o.nl,
                            indent=cc and o.prefix_sep .. o.prefix_sep})
    append(s, merge(r, ro, "samplerate"), {prefix="Sample Rate:", suffix=" Hz"})
    append_property(s, "packet-audio-bitrate", {prefix="Bitrate:", suffix=" kbps"})
    append_filters(s, "af", "Filters:")
end


-- Determine whether ASS formatting shall/can be used and set formatting sequences
local function eval_ass_formatting()
    o.use_ass = o.ass_formatting and has_vo_window()
    if o.use_ass then
        o.nl = o.ass_nl
        o.indent = o.ass_indent
        o.prefix_sep = o.ass_prefix_sep
        o.b1 = o.ass_b1
        o.b0 = o.ass_b0
        o.it1 = o.ass_it1
        o.it0 = o.ass_it0
    else
        o.nl = o.no_ass_nl
        o.indent = o.no_ass_indent
        o.prefix_sep = o.no_ass_prefix_sep
        o.b1 = o.no_ass_b1
        o.b0 = o.no_ass_b0
        o.it1 = o.no_ass_it1
        o.it0 = o.no_ass_it0
    end
end

-- assumptions:
--   s is composed of SGR escape sequences and/or printable UTF8 sequences
--   printable codepoints occupy one terminal cell (we don't have wcwidth)
--   tabstops are 8, 16, 24..., and the output starts at 0 or a tabstop
-- note: if maxwidth <= 2 and s doesn't fit: the result is "..." (more than 2)
function term_ellipsis(s, maxwidth)
    local TAB, ESC, SGR_END = 9, 27, ("m"):byte()
    local width, ellipsis = 0, "..."
    local fit_len, in_sgr

    for i = 1, #s do
        local x = s:byte(i)

        if in_sgr then
            in_sgr = x ~= SGR_END
        elseif x == ESC then
            in_sgr = true
            ellipsis = "\27[0m..."  -- ensure SGR reset
        elseif x < 128 or x >= 192 then  -- non UTF8-continuation
            -- tab adds till the next stop, else add 1
            width = width + (x == TAB and 8 - width % 8 or 1)

            if fit_len == nil and width > maxwidth - 3 then
                fit_len = i - 1  -- adding "..." still fits maxwidth
            end
            if width > maxwidth then
                return s:sub(1, fit_len) .. ellipsis
            end
        end
    end

    return s
end

local function term_ellipsis_array(arr, from, to, max_width)
    for i = from, to do
        arr[i] = term_ellipsis(arr[i], max_width)
    end
    return arr
end

-- split str into a table
-- example: local t = split(s, "\n")
-- plain: whether pat is a plain string (default false - pat is a pattern)
local function split(str, pat, plain)
    local init = 1
    local r, i, find, sub = {}, 1, string.find, string.sub
    repeat
        local f0, f1 = find(str, pat, init, plain)
        r[i], i = sub(str, init, f0 and f0 - 1), i+1
        init = f0 and f1 + 1
    until f0 == nil
    return r
end

-- Composes the output with header and scrollable content
-- Returns string of the finished page and the actually chosen offset
--
-- header      : table of the header where each entry is one line
-- content     : table of the content where each entry is one line
-- apply_scroll: scroll the content
local function finalize_page(header, content, apply_scroll)
    local term_size = mp.get_property_native("term-size", {})
    local term_width = o.term_width_limit or term_size.w or 80
    local term_height = o.term_height_limit or term_size.h or 24
    local from, to = 1, #content
    if apply_scroll and term_height > 0 then
        -- Up to 40 lines for libass because it can put a big performance toll on
        -- libass to process many lines which end up outside (below) the screen.
        -- In the terminal reduce height by 2 for the status line (can be more then one line)
        local max_content_lines = (o.use_ass and 40 or term_height - 2) - #header
        -- in the terminal the scrolling should stop once the last line is visible
        local max_offset = o.use_ass and #content or #content - max_content_lines + 1
        from = max(1, min((pages[curr_page].offset or 1), max_offset))
        to = min(#content, from + max_content_lines - 1)
        pages[curr_page].offset = from
    end
    local output = table.concat(header) .. table.concat(content, "", from, to)
    if not o.use_ass and term_width > 0 then
        local t = split(output, "\n", true)
        -- limit width for the terminal
        output = table.concat(term_ellipsis_array(t, 1, #t, term_width), "\n")
    end
    return output, from
end

-- Returns an ASS string with "normal" stats
local function default_stats()
    local stats = {}
    eval_ass_formatting()
    add_header(stats)
    add_file(stats)
    add_video_out(stats)
    add_video(stats)
    add_audio(stats)
    return finalize_page({}, stats, false)
end

-- Returns an ASS string with extended VO stats
local function vo_stats()
    local header, content = {}, {}
    eval_ass_formatting()
    add_header(header)
    append_perfdata(header, content, true, true)
    header = {table.concat(header)}
    return finalize_page(header, content, true)
end

local kbinfo_lines = nil
local function keybinding_info(after_scroll, bindlist)
    local header = {}
    local page = pages[o.key_page_4]
    eval_ass_formatting()
    add_header(header)
    append(header, "", {prefix=format("%s:%s", page.desc, scroll_hint()), nl="", indent=""})
    header = {table.concat(header)}

    if not kbinfo_lines or not after_scroll then
        kbinfo_lines = get_kbinfo_lines(o.term_width_limit)
    end

    return finalize_page(header, kbinfo_lines, not bindlist)
end

local function perf_stats()
    local header, content = {}, {}
    eval_ass_formatting()
    add_header(header)
    local page = pages[o.key_page_0]
    append(header, "", {prefix=format("%s:%s", page.desc, scroll_hint()), nl="", indent=""})
    append_general_perfdata(content)
    header = {table.concat(header)}
    return finalize_page(header, content, true)
end

local function opt_time(t)
    if type(t) == type(1.1) then
        return mp.format_time(t)
    end
    return "?"
end

-- Returns an ASS string with stats about the demuxer cache etc.
local function cache_stats()
    local stats = {}

    eval_ass_formatting()
    add_header(stats)
    append(stats, "", {prefix="Cache Info:", nl="", indent=""})

    local info = mp.get_property_native("demuxer-cache-state")
    if info == nil then
        append(stats, "Unavailable.", {})
        return finalize_page({}, stats, false)
    end

    local a = info["reader-pts"]
    local b = info["cache-end"]

    append(stats, opt_time(a) .. " - " .. opt_time(b), {prefix = "Packet Queue:"})

    local r = nil
    if a ~= nil and b ~= nil then
        r = b - a
    end

    local r_graph = nil
    if not display_timer.oneshot and o.use_ass then
        r_graph = generate_graph(cache_ahead_buf, cache_ahead_buf.pos,
                                 cache_ahead_buf.len, cache_ahead_buf.max,
                                 nil, 0.8, 1)
        r_graph = o.prefix_sep .. r_graph
    end
    append(stats, opt_time(r), {prefix = "Readahead:", suffix = r_graph})

    -- These states are not necessarily exclusive. They're about potentially
    -- separate mechanisms, whose states may be decoupled.
    local state = "reading"
    local seek_ts = info["debug-seeking"]
    if seek_ts ~= nil then
        state = "seeking (to " .. mp.format_time(seek_ts) .. ")"
    elseif info["eof"] == true then
        state = "eof"
    elseif info["underrun"] then
        state = "underrun"
    elseif info["idle"]  == true then
        state = "inactive"
    end
    append(stats, state, {prefix = "State:"})

    local speed = info["raw-input-rate"] or 0
    local speed_graph = nil
    if not display_timer.oneshot and o.use_ass then
        speed_graph = generate_graph(cache_speed_buf, cache_speed_buf.pos,
                                     cache_speed_buf.len, cache_speed_buf.max,
                                     nil, 0.8, 1)
        speed_graph = o.prefix_sep .. speed_graph
    end
    append(stats, utils.format_bytes_humanized(speed) .. "/s", {prefix="Speed:",
        suffix=speed_graph})

    append(stats, utils.format_bytes_humanized(info["total-bytes"]),
           {prefix = "Total RAM:"})
    append(stats, utils.format_bytes_humanized(info["fw-bytes"]),
           {prefix = "Forward RAM:"})

    local fc = info["file-cache-bytes"]
    if fc ~= nil then
        fc = utils.format_bytes_humanized(fc)
    else
        fc = "(disabled)"
    end
    append(stats, fc, {prefix = "Disk Cache:"})

    append(stats, info["debug-low-level-seeks"], {prefix = "Media Seeks:"})
    append(stats, info["debug-byte-level-seeks"], {prefix = "Stream Seeks:"})

    append(stats, "", {prefix=o.nl .. o.nl .. "Ranges:", nl="", indent=""})

    append(stats, info["bof-cached"] and "yes" or "no",
           {prefix = "Start Cached:"})
    append(stats, info["eof-cached"] and "yes" or "no",
           {prefix = "End Cached:"})

    local ranges = info["seekable-ranges"] or {}
    for n, r in ipairs(ranges) do
        append(stats, mp.format_time(r["start"]) .. " - " ..
                      mp.format_time(r["end"]),
               {prefix = format("Range %s:", n)})
    end

    return finalize_page({}, stats, false)
end

-- Record 1 sample of cache statistics.
-- (Unlike record_data(), this does not return a function, but runs directly.)
local function record_cache_stats()
    local info = mp.get_property_native("demuxer-cache-state")
    if info == nil then
        return
    end

    local a = info["reader-pts"]
    local b = info["cache-end"]
    if a ~= nil and b ~= nil then
        graph_add_value(cache_ahead_buf, b - a)
    end

    graph_add_value(cache_speed_buf, info["raw-input-rate"] or 0)
end

cache_recorder_timer = mp.add_periodic_timer(0.25, record_cache_stats)
cache_recorder_timer:kill()

-- Current page and <page key>:<page function> mapping
curr_page = o.key_page_1
pages = {
    [o.key_page_1] = { f = default_stats, desc = "Default" },
    [o.key_page_2] = { f = vo_stats, desc = "Extended Frame Timings", scroll = true },
    [o.key_page_3] = { f = cache_stats, desc = "Cache Statistics" },
    [o.key_page_4] = { f = keybinding_info, desc = "Active Key Bindings", scroll = true },
    [o.key_page_0] = { f = perf_stats, desc = "Internal Performance Info", scroll = true },
}


-- Returns a function to record vsratio/jitter with the specified `skip` value
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

-- Call the function for `page` and print it to OSD
local function print_page(page, after_scroll)
    -- the page functions assume we start in ass-enabled mode.
    -- that's true for mp.set_osd_ass, but not for mp.osd_message.
    local ass_content = pages[page].f(after_scroll)
    if o.persistent_overlay then
        mp.set_osd_ass(0, 0, ass_content)
    else
        mp.osd_message((o.use_ass and ass_start or "") .. ass_content,
                       display_timer.oneshot and o.duration or o.redraw_delay + 1)
    end
end


local function clear_screen()
    if o.persistent_overlay then mp.set_osd_ass(0, 0, "") else mp.osd_message("", 0) end
end

local function scroll_delta(d)
    if display_timer.oneshot then display_timer:kill() ; display_timer:resume() end
    pages[curr_page].offset = (pages[curr_page].offset or 1) + d
    print_page(curr_page, true)
end
local function scroll_up() scroll_delta(-o.scroll_lines) end
local function scroll_down() scroll_delta(o.scroll_lines) end

local function reset_scroll_offsets()
    for _, page in pairs(pages) do
        page.offset = nil
    end
end
local function bind_scroll()
    if not scroll_bound then
        mp.add_forced_key_binding(o.key_scroll_up, "__forced_"..o.key_scroll_up, scroll_up, {repeatable=true})
        mp.add_forced_key_binding(o.key_scroll_down, "__forced_"..o.key_scroll_down, scroll_down, {repeatable=true})
        scroll_bound = true
    end
end
local function unbind_scroll()
    if scroll_bound then
        mp.remove_key_binding("__forced_"..o.key_scroll_up)
        mp.remove_key_binding("__forced_"..o.key_scroll_down)
        scroll_bound = false
    end
end
local function update_scroll_bindings(k)
    if pages[k].scroll then
        bind_scroll()
    else
        unbind_scroll()
    end
end

-- Add keybindings for every page
local function add_page_bindings()
    local function a(k)
        return function()
            reset_scroll_offsets()
            update_scroll_bindings(k)
            curr_page = k
            print_page(k)
            if display_timer.oneshot then display_timer:kill() ; display_timer:resume() end
        end
    end
    for k, _ in pairs(pages) do
        mp.add_forced_key_binding(k, "__forced_"..k, a(k), {repeatable=true})
    end
    update_scroll_bindings(curr_page)
end


-- Remove keybindings for every page
local function remove_page_bindings()
    for k, _ in pairs(pages) do
        mp.remove_key_binding("__forced_"..k)
    end
    unbind_scroll()
end


local function process_key_binding(oneshot)
    reset_scroll_offsets()
    -- Stats are already being displayed
    if display_timer:is_enabled() then
        -- Previous and current keys were oneshot -> restart timer
        if display_timer.oneshot and oneshot then
            display_timer:kill()
            print_page(curr_page)
            display_timer:resume()
        -- Previous and current keys were toggling -> end toggling
        elseif not display_timer.oneshot and not oneshot then
            display_timer:kill()
            cache_recorder_timer:stop()
            if tm_viz_prev ~= nil then
                mp.set_property_native("tone-mapping-visualize", tm_viz_prev)
                tm_viz_prev = nil
            end
            clear_screen()
            remove_page_bindings()
            if recorder then
                mp.unobserve_property(recorder)
                recorder = nil
            end
        end
    -- No stats are being displayed yet
    else
        if not oneshot and (o.plot_vsync_jitter or o.plot_vsync_ratio) then
            recorder = record_data(o.skip_frames)
            -- Rely on the fact that "vsync-ratio" is updated at the same time.
            -- Using "none" to get a sample any time, even if it does not change.
            -- Will stop working if "vsync-jitter" property change notification
            -- changes, but it's fine for an internal script.
            mp.observe_property("vsync-jitter", "none", recorder)
        end
        if not oneshot and o.plot_tonemapping_lut then
            tm_viz_prev = mp.get_property_native("tone-mapping-visualize")
            mp.set_property_native("tone-mapping-visualize", true)
        end
        if not oneshot then
            cache_ahead_buf = {0, pos = 1, len = 50, max = 0}
            cache_speed_buf = {0, pos = 1, len = 50, max = 0}
            cache_recorder_timer:resume()
        end
        display_timer:kill()
        display_timer.oneshot = oneshot
        display_timer.timeout = oneshot and o.duration or o.redraw_delay
        add_page_bindings()
        print_page(curr_page)
        display_timer:resume()
    end
end


-- Create the timer used for redrawing (toggling) or clearing the screen (oneshot)
-- The duration here is not important and always set in process_key_binding()
display_timer = mp.add_periodic_timer(o.duration,
    function()
        if display_timer.oneshot then
            display_timer:kill() ; clear_screen() ; remove_page_bindings()
        else
            print_page(curr_page)
        end
    end)
display_timer:kill()

-- Single invocation key binding
mp.add_key_binding(nil, "display-stats", function() process_key_binding(true) end,
    {repeatable=true})

-- Toggling key binding
mp.add_key_binding(nil, "display-stats-toggle", function() process_key_binding(false) end,
    {repeatable=false})

-- Single invocation bindings without key, can be used in input.conf to create
-- bindings for a specific page: "e script-binding stats/display-page-2"
for k, _ in pairs(pages) do
    mp.add_key_binding(nil, "display-page-" .. k,
        function()
            curr_page = k
            process_key_binding(true)
        end, {repeatable=true})
end

-- Reprint stats immediately when VO was reconfigured, only when toggled
mp.register_event("video-reconfig",
    function()
        if display_timer:is_enabled() then
            print_page(curr_page)
        end
    end)

--  --script-opts=stats-bindlist=[-]{yes|<TERM-WIDTH>}
if o.bindlist ~= "no" then
    mp.command("no-osd set really-quiet yes")
    if o.bindlist:sub(1, 1) == "-" then
        o.bindlist = o.bindlist:sub(2)
        o.no_ass_b0 = ""
        o.no_ass_b1 = ""
    end
    local width = max(40, math.floor(tonumber(o.bindlist) or 79))
    mp.add_timeout(0, function()  -- wait for all other scripts to finish init
        o.ass_formatting = false
        o.no_ass_indent = " "
        o.term_size = { w = width , h = 0}
        io.write(keybinding_info(false, true) .. "\n")
        mp.command("quit")
    end)
end
