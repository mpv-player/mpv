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
    font = "sans",
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
-- Save all properties known to this version of mpv
local property_list = {}
for p in string.gmatch(mp.get_property("property-list"), "([^,]+)") do property_list[p] = true end
-- Mapping of properties to their deprecated names
local property_aliases = {
    ["decoder-frame-drop-count"] = "drop-frame-count",
    ["frame-drop-count"] = "vo-drop-frame-count",
    ["container-fps"] = "fps",
}

local function graph_add_value(graph, value)
    graph.pos = (graph.pos % graph.len) + 1
    graph[graph.pos] = value
    graph.max = max(graph.max, value)
end

-- Return deprecated name for the given property
local function compat(p)
    while not property_list[p] and property_aliases[p] do
        p = property_aliases[p]
    end
    return p
end

-- "\\<U+2060>" in UTF-8 (U+2060 is WORD-JOINER)
local ESC_BACKSLASH = "\\" .. string.char(0xE2, 0x81, 0xA0)

local function no_ASS(t)
    if not o.use_ass then
        return t
    elseif not o.persistent_overlay then
        -- mp.osd_message supports ass-escape using osd-ass-cc/{0|1}
        return ass_stop .. t .. ass_start
    else
        -- mp.set_osd_ass doesn't support ass-escape. roll our own.
        -- similar to mpv's sub/osd_libass.c:mangle_ass(...), excluding
        -- space after newlines because no_ASS is not used with multi-line.
        -- space at the begining is replaced with "\\h" because it matters
        -- at the begining of a line, and we can't know where our output
        -- ends up. no issue if it ends up at the middle of a line.
        return tostring(t)
               :gsub("\\", ESC_BACKSLASH)
               :gsub("{", "\\{")
               :gsub("^ ", "\\h")
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
        return format("{\\r}{\\an7}{\\fs%d}{\\fn%s}{\\bord%f}{\\3c&H%s&}" ..
                      "{\\1c&H%s&}{\\alpha&H%s&}{\\xshad%f}{\\yshad%f}{\\4c&H%s&}",
                      o.font_size, o.font, o.border_size,
                      o.border_color, o.font_color, o.alpha, o.shadow_x_offset,
                      o.shadow_y_offset, o.shadow_color)
    end
end


local function has_vo_window()
    return mp.get_property("vo-configured") == "yes"
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

    -- try and center the graph if possible, but avoid going above `scale`
    if v_avg then
        scale = min(scale, v_max / (2 * v_avg))
    end

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
    return format("%s{\\r}{\\pbo%f}{\\shad0}{\\alpha&H00}{\\p1}%s{\\p0}{\\bord0}{\\1c&H%s}{\\p1}%s{\\p0}%s",
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


local function append_perfdata(s, dedicated_page)
    local vo_p = mp.get_property_native("vo-passes")
    if not vo_p then
        return
    end

    local ds = mp.get_property_bool("display-sync-active", false)
    local target_fps = ds and mp.get_property_number("display-fps", 0)
                       or mp.get_property_number(compat("container-fps"), 0)
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
        return format("%05d", i / 1000)
    end

    -- Format n/m with a font weight based on the ratio
    local function p(n, m)
        local i = 0
        if m > 0 then
            i = tonumber(n) / m
        end
        -- Calculate font weight. 100 is minimum, 400 is normal, 700 bold, 900 is max
        local w = (700 * math.sqrt(i)) + 200
        return format("{\\b%d}%02d%%{\\b0}", w, i * 100)
    end

    -- ensure that the fixed title is one element and every scrollable line is
    -- also one single element.
    s[#s+1] = format("%s%s%s%s{\\fs%s}%s{\\fs%s}",
                     dedicated_page and "" or o.nl, dedicated_page and "" or o.indent,
                     b("Frame Timings:"), o.prefix_sep, o.font_size * 0.66,
                     "(last/average/peak  μs)", o.font_size)

    for frame, data in pairs(vo_p) do
        local f = "%s%s%s{\\fn%s}%s / %s / %s %s%s{\\fn%s}%s%s%s"

        if dedicated_page then
            s[#s+1] = format("%s%s%s:", o.nl, o.indent,
                             b(frame:gsub("^%l", string.upper)))

            for _, pass in ipairs(data) do
                s[#s+1] = format(f, o.nl, o.indent, o.indent,
                                 o.font_mono, pp(pass["last"]),
                                 pp(pass["avg"]), pp(pass["peak"]),
                                 o.prefix_sep .. o.prefix_sep, p(pass["last"], last_s[frame]),
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
                             pp(avg_s[frame]), pp(peak_s[frame]), "", "", o.font,
                             o.prefix_sep, o.prefix_sep, b("Total"))
        else
            -- for the simplified view, we just print the sum of each pass
            s[#s+1] = format(f, o.nl, o.indent, o.indent, o.font_mono,
                            pp(last_s[frame]), pp(avg_s[frame]), pp(peak_s[frame]),
                            "", "", o.font, o.prefix_sep, o.prefix_sep,
                            frame:gsub("^%l", string.upper))
        end
    end
end

local function ellipsis(s, maxlen)
    if not maxlen or s:len() <= maxlen then return s end
    return s:sub(1, maxlen - 3) .. "..."
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

local function get_kbinfo_lines(width)
    -- active keys: only highest priotity of each key, and not our (stats) keys
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
    local _, itabs = o.indent:gsub("\t", "")
    local cutoff = term and (width or 79) - o.indent:len() - itabs * 7 - spre:len()

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
        append(info_lines, ellipsis(bind.cmd, cutoff),
               { prefix = kpre .. no_ASS(align_right(bind.key)) .. kpost })
    end
    return info_lines
end

local function append_general_perfdata(s, offset)
    local perf_info = mp.get_property_native("perf-info") or {}
    local count = 0
    for _, data in ipairs(perf_info) do
        count = count + 1
    end
    offset = max(1, min((offset or 1), count))

    local i = 0
    for _, data in ipairs(perf_info) do
        i = i + 1
        if i >= offset then
            append(s, data.text or data.value, {prefix="["..tostring(i).."] "..data.name..":"})

            if o.plot_perfdata and o.use_ass and data.value then
                buf = perf_buffers[data.name]
                if not buf then
                    buf = {0, pos = 1, len = 50, max = 0}
                    perf_buffers[data.name] = buf
                end
                graph_add_value(buf, data.value)
                s[#s+1] = generate_graph(buf, buf.pos, buf.len, buf.max, nil, 0.8, 1)
            end
        end
    end
    return offset
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

    append_property(s, "mistimed-frame-count", {prefix="Mistimed:", nl=""})
    append_property(s, "vo-delayed-frame-count", {prefix="Delayed:", nl=""})

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
        local vratio = append_property(s, "vsync-ratio", {prefix="VSync Ratio:"})
        append_property(s, "vsync-jitter", {prefix="VSync Jitter:", nl="" or o.nl})
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
        for key,value in pairs(f.params) do
            p[#p+1] = key .. "=" .. value
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
                        {prefix="(" .. tostring(ch_index + 1) .. "/", suffix=")", nl="",
                         indent=" ", prefix_sep=" ", no_prefix_markup=true})
    end

    local fs = append_property(s, "file-size", {prefix="Size:"})
    append_property(s, "file-format", {prefix="Format/Protocol:", nl=fs and "" or o.nl})

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


local function add_video(s)
    local r = mp.get_property_native("video-params")
    -- in case of e.g. lavfi-complex there can be no input video, only output
    if not r then
        r = mp.get_property_native("video-out-params")
    end
    if not r then
        return
    end

    local osd_dims = mp.get_property_native("osd-dimensions")
    local scaled_width = osd_dims["w"] - osd_dims["ml"] - osd_dims["mr"]
    local scaled_height = osd_dims["h"] - osd_dims["mt"] - osd_dims["mb"]

    append(s, "", {prefix=o.nl .. o.nl .. "Video:", nl="", indent=""})
    if append_property(s, "video-codec", {prefix_sep="", nl="", indent=""}) then
        append_property(s, "hwdec-current", {prefix="(hwdec:", nl="", indent=" ",
                         no_prefix_markup=true, suffix=")"}, {no=true, [""]=true})
    end
    append_property(s, "avsync", {prefix="A-V:"})
    if append_property(s, compat("decoder-frame-drop-count"),
                       {prefix="Dropped Frames:", suffix=" (decoder)"}) then
        append_property(s, compat("frame-drop-count"), {suffix=" (output)", nl="", indent=""})
    end
    if append_property(s, "display-fps", {prefix="Display FPS:", suffix=" (specified)"}) then
        append_property(s, "estimated-display-fps",
                        {suffix=" (estimated)", nl="", indent=""})
    else
        append_property(s, "estimated-display-fps",
                        {prefix="Display FPS:", suffix=" (estimated)"})
    end
    if append_property(s, compat("container-fps"), {prefix="FPS:", suffix=" (specified)"}) then
        append_property(s, "estimated-vf-fps",
                        {suffix=" (estimated)", nl="", indent=""})
    else
        append_property(s, "estimated-vf-fps",
                        {prefix="FPS:", suffix=" (estimated)"})
    end

    append_display_sync(s)
    append_perfdata(s, o.print_perfdata_passes)

    if append(s, r["w"], {prefix="Native Resolution:"}) then
        append(s, r["h"], {prefix="x", nl="", indent=" ", prefix_sep=" ", no_prefix_markup=true})
    end
    if append(s, scaled_width, {prefix="Scaled Resolution:"}) then
        append(s, scaled_height, {prefix="x", nl="", indent=" ", prefix_sep=" ", no_prefix_markup=true})
    end
    append_property(s, "current-window-scale", {prefix="Window Scale:"})
    if r["aspect"] ~= nil then
        append(s, format("%.2f", r["aspect"]), {prefix="Aspect Ratio:"})
    end
    append(s, r["pixelformat"], {prefix="Pixel Format:"})
    if r["hw-pixelformat"] ~= nil then
        append(s, r["hw-pixelformat"], {prefix_sep="[", nl="", indent=" ",
                suffix="]"})
    end

    -- Group these together to save vertical space
    local prim = append(s, r["primaries"], {prefix="Primaries:"})
    local cmat = append(s, r["colormatrix"], {prefix="Colormatrix:", nl=prim and "" or o.nl})
    append(s, r["colorlevels"], {prefix="Levels:", nl=cmat and "" or o.nl})

    -- Append HDR metadata conditionally (only when present and interesting)
    local hdrpeak = r["sig-peak"] or 0
    local hdrinfo = ""
    if hdrpeak > 1 then
        hdrinfo = " (HDR peak: " .. format("%.2f", hdrpeak) .. ")"
    end

    append(s, r["gamma"], {prefix="Gamma:", suffix=hdrinfo})
    append_property(s, "packet-video-bitrate", {prefix="Bitrate:", suffix=" kbps"})
    append_filters(s, "vf", "Filters:")
end


local function add_audio(s)
    local r = mp.get_property_native("audio-params")
    -- in case of e.g. lavfi-complex there can be no input audio, only output
    if not r then
        r = mp.get_property_native("audio-out-params")
    end
    if not r then
        return
    end

    append(s, "", {prefix=o.nl .. o.nl .. "Audio:", nl="", indent=""})
    append_property(s, "audio-codec", {prefix_sep="", nl="", indent=""})
    local cc = append(s, r["channel-count"], {prefix="Channels:"})
    append(s, r["format"], {prefix="Format:", nl=cc and "" or o.nl})
    append(s, r["samplerate"], {prefix="Sample Rate:", suffix=" Hz"})
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


-- Returns an ASS string with "normal" stats
local function default_stats()
    local stats = {}
    eval_ass_formatting()
    add_header(stats)
    add_file(stats)
    add_video(stats)
    add_audio(stats)
    return table.concat(stats)
end

local function scroll_vo_stats(stats, fixed_items, offset)
    local ret = {}
    local count = #stats - fixed_items
    offset = max(1, min((offset or 1), count))

    for i, line in pairs(stats) do
        if i <= fixed_items or i >= fixed_items + offset then
            ret[#ret+1] = stats[i]
        end
    end
    return ret, offset
end

-- Returns an ASS string with extended VO stats
local function vo_stats()
    local stats = {}
    eval_ass_formatting()
    add_header(stats)

    -- first line (title) added next is considered fixed
    local fixed_items = #stats + 1
    append_perfdata(stats, true)

    local page = pages[o.key_page_2]
    stats, page.offset = scroll_vo_stats(stats, fixed_items, page.offset)
    return table.concat(stats)
end

local kbinfo_lines = nil
local function keybinding_info(after_scroll)
    local header = {}
    local page = pages[o.key_page_4]
    eval_ass_formatting()
    add_header(header)
    append(header, "", {prefix=o.nl .. page.desc .. ":", nl="", indent=""})

    if not kbinfo_lines or not after_scroll then
        kbinfo_lines = get_kbinfo_lines()
    end
    -- up to 20 lines for the terminal - so that mpv can also print
    -- the status line without scrolling, and up to 40 lines for libass
    -- because it can put a big performance toll on libass to process
    -- many lines which end up outside (below) the screen.
    local term = not o.use_ass
    local nlines = #kbinfo_lines
    page.offset = max(1, min((page.offset or 1), term and nlines - 20 or nlines))
    local maxline = min(nlines, page.offset + (term and 20 or 40))
    return table.concat(header) ..
           table.concat(kbinfo_lines, "", page.offset, maxline)
end

local function perf_stats()
    local stats = {}
    eval_ass_formatting()
    add_header(stats)
    local page = pages[o.key_page_0]
    append(stats, "", {prefix=o.nl .. o.nl .. page.desc .. ":", nl="", indent=""})
    page.offset = append_general_perfdata(stats, page.offset)
    return table.concat(stats)
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
    append(stats, "", {prefix=o.nl .. o.nl .. "Cache info:", nl="", indent=""})

    local info = mp.get_property_native("demuxer-cache-state")
    if info == nil then
        append(stats, "Unavailable.", {})
        return table.concat(stats)
    end

    local a = info["reader-pts"]
    local b = info["cache-end"]

    append(stats, opt_time(a) .. " - " .. opt_time(b), {prefix = "Packet queue:"})

    local r = nil
    if (a ~= nil) and (b ~= nil) then
        r = b - a
    end

    local r_graph = nil
    if not display_timer.oneshot and o.use_ass then
        r_graph = generate_graph(cache_ahead_buf, cache_ahead_buf.pos,
                                 cache_ahead_buf.len, cache_ahead_buf.max,
                                 nil, 0.8, 1)
        r_graph = o.prefix_sep .. r_graph
    end
    append(stats, opt_time(r), {prefix = "Read-ahead:", suffix = r_graph})

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
    append(stats, fc, {prefix = "Disk cache:"})

    append(stats, info["debug-low-level-seeks"], {prefix = "Media seeks:"})
    append(stats, info["debug-byte-level-seeks"], {prefix = "Stream seeks:"})

    append(stats, "", {prefix=o.nl .. o.nl .. "Ranges:", nl="", indent=""})

    append(stats, info["bof-cached"] and "yes" or "no",
           {prefix = "Start cached:"})
    append(stats, info["eof-cached"] and "yes" or "no",
           {prefix = "End cached:"})

    local ranges = info["seekable-ranges"] or {}
    for n, r in ipairs(ranges) do
        append(stats, mp.format_time(r["start"]) .. " - " ..
                      mp.format_time(r["end"]),
               {prefix = format("Range %s:", n)})
    end

    return table.concat(stats)
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
    if (a ~= nil) and (b ~= nil) then
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
    [o.key_page_4] = { f = keybinding_info, desc = "Active key bindings", scroll = true },
    [o.key_page_0] = { f = perf_stats, desc = "Internal performance info", scroll = true },
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
    if (pages[k].scroll) then
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
        eval_ass_formatting()
        io.write(pages[o.key_page_4].desc .. ":" ..
                 table.concat(get_kbinfo_lines(width)) .. "\n")
        mp.command("quit")
    end)
end
