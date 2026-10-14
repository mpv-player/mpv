--[[
This file is part of mpv.

mpv is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

mpv is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
]]

local utils = require "mp.utils"
local msg = require "mp.msg"

local opts = {
    -- Switch to fullscreen while calibrating.
    fullscreen = true,
    -- Initial pattern window size in percent of the screen area.
    black_window = 20,
    peak_window = 10,
    -- Seconds without input after which the readout reappears.
    hide_time = 1,
}
require "mp.options".read_options(opts)

local WINDOW_SIZES = {2, 5, 10, 20, 50, 100}
local NITS_MIN, NITS_MAX = 10, 10000
local CONTRAST_MIN, CONTRAST_MAX = 10, 1e7
-- Mastering luminance declared while probing the peak, low enough that no
-- display should tone map. See the comment at the top.
local METADATA_NITS = 100
-- Level of the bright rows on the transfer function page, and the codes at
-- which a solid patch emits half of their linear light, oetf(eotf(0.2) / 2),
-- for the sRGB piecewise curve and for a pure 2.2 power function.
local STRIPE_LEVEL = 0.2
local ANCHOR_SRGB = 0.136038
local ANCHOR_GAMMA22 = 0.145946
-- Resolution of the source probe on the SDR black level page, PQ codes.
local PQ_STEPS = 1023

-- SMPTE ST 2084, needed to express the peak probe, an output code above the
-- range of the display, in nits.
local PQ_M1 = 2610 / 16384
local PQ_M2 = 2523 / 4096 * 128
local PQ_C1 = 3424 / 4096
local PQ_C2 = 2413 / 4096 * 32
local PQ_C3 = 2392 / 4096 * 32

local function pq_to_nits(v)
    local p = math.max(v, 0) ^ (1 / PQ_M2)
    return 10000 * (math.max(p - PQ_C1, 0) / (PQ_C2 - PQ_C3 * p)) ^ (1 / PQ_M1)
end

local function nits_to_pq(nits)
    local p = (math.max(nits, 0) / 10000) ^ PQ_M1
    return ((PQ_C1 + PQ_C2 * p) / (1 + PQ_C3 * p)) ^ PQ_M2
end

local function round(x)
    return math.floor(x + 0.5)
end

local function clamp(x, lo, hi)
    return math.max(lo, math.min(hi, x))
end

local function significant(x, digits)
    if x == 0 then
        return 0
    end
    local m = 10 ^ (math.floor(math.log(math.abs(x)) / math.log(10)) - digits + 1)
    return round(x / m) * m
end

local function fmt_nits(n)
    if n >= 100 then
        return string.format("%.0f", n)
    elseif n >= 1 then
        return string.format("%.1f", n)
    elseif n >= 0.01 then
        return string.format("%.3f", n)
    end
    return string.format("%.2g", n)
end

local function fmt_int(n)
    return string.format("%.0f", n)
end

-- libplacebo texture format names, e.g. rgba8, rgb10a2, rgba16hf
local function parse_format(name)
    if not name then
        return nil, false
    end
    local bits = tonumber(name:match("^%a+(%d+)"))
    return bits, name:match("%d+h?f$") ~= nil
end

-- Describes how the shader probes (integer codes) map to output values and,
-- for absolute transfer functions, to luminance for the current swapchain.
local function make_mode(tp)
    local gamma = tp.gamma
    local bits, float = parse_format(tp.pixelformat)
    local mode = {
        gamma = gamma,
        format = tp.pixelformat,
        levels = tp.colorlevels,
        absolute = gamma == "pq" or gamma == "scrgb",
        bits = bits,
        float = float,
    }

    if mode.absolute then
        -- PQ codes are perceptually uniform, so probe in those even when the
        -- swapchain is float and quantized further downstream.
        local depth = (bits and not float and bits <= 12) and bits or 10
        mode.kmax = 2 ^ depth - 1
        mode.desc = gamma == "pq" and "HDR (PQ)" or "HDR (scRGB)"
        mode.k_to_nits = function(k) return pq_to_nits(k / mode.kmax) end
        mode.nits_to_k = function(n) return round(nits_to_pq(n) * mode.kmax) end
        if gamma == "pq" then
            mode.k_to_value = function(k) return k / mode.kmax end
            mode.max_value = 1
        else
            mode.k_to_value = function(k) return pq_to_nits(k / mode.kmax) / 80 end
            mode.max_value = 10000 / 80
        end
    else
        local depth = (bits and not float) and math.min(bits, 12) or (gamma == "hlg" and 10 or 8)
        mode.kmax = 2 ^ depth - 1
        mode.desc = gamma == "hlg" and "HDR (HLG)" or ("SDR (" .. tostring(gamma) .. ")")
        mode.k_to_value = function(k) return k / mode.kmax end
        mode.max_value = 1
    end

    mode.coarse = math.max(1, round(mode.kmax / 128))
    return mode
end

local function temp_dir()
    local dir = mp.command_native({"expand-path", "~~cache/"})
    if dir and utils.file_info(dir) then
        return dir
    end
    return os.getenv("TMPDIR") or os.getenv("TEMP") or os.getenv("TMP") or "/tmp"
end

local function write_file(path, ...)
    local f, err = io.open(path, "wb")
    if not f then
        return false, err
    end
    local ok, werr = f:write(...)
    f:close()
    return ok ~= nil, werr
end

-- Two passes. MAIN can replace the video with a solid source level, which
-- the pipeline then maps to the target like any content. PREOUTPUT either
-- draws a square window with a disc in the center with exact output codes,
-- or masks everything outside the window to display black. The edges are hard
-- on purpose, antialiasing would introduce intermediate codes.
local SHADER = [[
//!PARAM src_mode
//!DESC replace the video with the solid source level (1)
//!TYPE DYNAMIC float
//!MINIMUM 0
//!MAXIMUM 1
0

//!PARAM src_level
//!DESC source level replacing the video, in the encoding of the source
//!TYPE DYNAMIC float
//!MINIMUM 0
//!MAXIMUM 1
0

//!PARAM draw
//!DESC 0 shows the video, 1 draws the pattern, 2 masks outside the window
//!TYPE DYNAMIC float
//!MINIMUM 0
//!MAXIMUM 2
1

//!PARAM lvl_win
//!DESC output level of the pattern window
//!TYPE DYNAMIC float
//!MINIMUM 0
//!MAXIMUM 1000
0

//!PARAM lvl_probe
//!DESC output level of the disc in the center of the window
//!TYPE DYNAMIC float
//!MINIMUM 0
//!MAXIMUM 1000
0

//!PARAM lvl_outside
//!DESC output level outside the pattern window
//!TYPE DYNAMIC float
//!MINIMUM 0
//!MAXIMUM 1000
0

//!PARAM win_area
//!DESC pattern window size as a fraction of the screen area
//!TYPE DYNAMIC float
//!MINIMUM 0
//!MAXIMUM 1
0.1

//!PARAM stripes
//!DESC fill the window with single pixel rows alternating with black
//!TYPE DYNAMIC float
//!MINIMUM 0
//!MAXIMUM 1
0

//!HOOK MAIN
//!BIND HOOKED
//!DESC display calibration source black

vec4 hook() {
    if (src_mode < 0.5)
        return HOOKED_tex(HOOKED_pos);
    return vec4(vec3(src_level), 1.0);
}

//!HOOK PREOUTPUT
//!BIND HOOKED
//!DESC display calibration pattern

// Ordered dither threshold of an 8x8 Bayer matrix, p are integer coordinates
float bayer2(vec2 p) {
    return 2.0 * abs(p.x - p.y) + p.y;
}

float bayer8(vec2 p) {
    float b = 16.0 * bayer2(mod(p, 2.0)) + 4.0 * bayer2(mod(floor(p / 2.0), 2.0)) +
              bayer2(mod(floor(p / 4.0), 2.0));
    return (b + 0.5) / 64.0;
}

vec4 hook() {
    if (draw < 0.5)
        return HOOKED_tex(HOOKED_pos);
    vec2 size = HOOKED_size;
    vec2 d = HOOKED_pos * size - 0.5 * size;
    vec2 half_size = vec2(0.5 * sqrt(win_area * size.x * size.y));
    if (win_area >= 1.0)
        half_size = 0.5 * size;
    bool inside = all(lessThanEqual(abs(d), half_size));
    if (draw > 1.5)
        return inside ? HOOKED_tex(HOOKED_pos) : vec4(vec3(lvl_outside), 1.0);
    float v = lvl_outside;
    if (inside) {
        v = lvl_win;
        if (stripes > 0.5 && mod(floor(HOOKED_pos.y * size.y), 2.0) < 0.5)
            v = lvl_outside;
        float radius = 0.5 * min(half_size.x, half_size.y);
        float dist = length(d);
        float t = step(radius, dist);
        if (stripes > 0.5) {
            float edge = 0.25 * radius;
            t = smoothstep(radius - edge, radius + edge, dist);
        }
        vec2 px = floor(HOOKED_pos * size);
        if (t < bayer8(vec2(px.x, floor(px.y / 2.0))))
            v = lvl_probe;
    }
    return vec4(vec3(v), 1.0);
}
]]

-- SDR scene shown by the reference white page: a white "page" with mid gray
-- and black areas, and a gray scale at the bottom.
local function ppm_image(w, h)
    local function px(v)
        return string.char(v):rep(3)
    end
    local split = round(w * 2 / 3)
    local top = px(255):rep(split) .. px(128):rep(w - split)
    local bottom = px(255):rep(split) .. px(0):rep(w - split)
    local steps = {}
    local x = 0
    for i = 0, 10 do
        local x1 = round(w * (i + 1) / 11)
        steps[#steps + 1] = px(round(i * 25.5)):rep(x1 - x)
        x = x1
    end
    local scale = table.concat(steps)
    local h_scale = round(h * 0.14)
    local h_top = round((h - h_scale) / 2)
    local h_bottom = h - h_scale - h_top
    return string.format("P6\n%d %d\n255\n", w, h),
           top:rep(h_top), bottom:rep(h_bottom), scale:rep(h_scale)
end

local function ass_escape(s)
    return (s:gsub("\\", "\\\\"):gsub("{", "\\{"):gsub("}", "\\}"))
end

local state = nil
-- Event handler that sets --idle=once again, while it is pending.
local restore_idle = nil

-- SDR content is only rendered into an HDR container in "target" hint mode.
local function is_source_hint_mode()
    return mp.get_property("target-colorspace-hint-mode") ~= "target"
end

local function current_entry_id()
    local pos = mp.get_property_number("playlist-playing-pos", -1)
    if pos < 0 then
        return nil
    end
    return mp.get_property_number("playlist/" .. pos .. "/id")
end

-- Set an option only when it differs, every change re-renders.
local function set_option(name, value)
    if mp.get_property(name) ~= value then
        mp.set_property(name, value)
    end
end

local function contrast_str(contrast)
    return contrast and fmt_int(contrast) or "inf"
end

-- Nearest candidate curve for the matched code, nil before confirmation.
local function classify_transfer()
    local s = state
    if not s.transfer_touched then
        return nil
    end
    local match = s.match_k / s.mode.kmax
    local d_srgb = math.abs(match - ANCHOR_SRGB)
    local d_g22 = math.abs(match - ANCHOR_GAMMA22)
    return d_g22 <= d_srgb and "gamma2.2" or "srgb"
end

local function effective_gamma()
    local s = state
    local match = s.match_k / s.mode.kmax
    if match <= 0 or match >= STRIPE_LEVEL then
        return nil
    end
    return math.log(2) / math.log(STRIPE_LEVEL / match)
end

local SRGB_CLASS = {["srgb"] = true, ["gamma2.2"] = true}

-- The --treat-srgb-as-power22 value matching the detected curve. Input and
-- output are set alike, SDR content is rendered as-is by default and a
-- differing input side would introduce a conversion. auto stays when it
-- already resolves to the detected curve on this platform.
local function treat_suggestion(detected)
    local s = state
    if s.auto_gamma == detected then
        return "auto"
    end
    return detected == "gamma2.2" and "both" or "no"
end

-- Tag the pattern carrier for the current page. The black level page needs
-- an absolute source, SDR sources are relative and keep their black at code
-- zero, while the black of an HDR source is mapped to the black point of the
-- target, which is the path --target-contrast is used on. The other pages
-- carry the detected transfer function, so that input and output match in
-- the actual pipeline.
local function update_carrier_tag()
    local s = state
    local vf = s.carrier_vf
    if s.pages[s.page] == "black" and s.mode.absolute then
        -- With a mastering black level, like graded HDR content carries. The
        -- inferred PQ black is the infinite contrast sentinel, which is kept
        -- at zero instead of being mapped to the black point.
        vf = "format=gamma=pq:primaries=bt.2020:min-luma=0.005:max-luma=1000"
    elseif s.pages[s.page] == "black" then
        -- Source range matching the target, so that the probe level reaches
        -- the display unchanged apart from the black point compensation.
        vf = string.format("format=gamma=pq:primaries=bt.2020:min-luma=0.0001:max-luma=%d",
                           s.white_nits)
    else
        local detected = classify_transfer()
        if detected then
            vf = "format=gamma=" .. detected
        end
    end
    if vf ~= s.applied_vf then
        s.applied_vf = vf
        mp.set_property("vf", vf)
    end
end

-- Make the pipeline follow the detection right away, the remaining pages
-- reflect the display. Restores the configured value while nothing is
-- measured.
local function apply_transfer()
    local s = state
    local detected = classify_transfer()
    set_option("treat-srgb-as-power22", detected and treat_suggestion(detected) or s.user_treat)
    update_carrier_tag()
end

-- Level of the SDR black level probe as a fraction of white, which is how it
-- is matched on the display. Until it is adjusted it is the black point of
-- the contrast in effect.
local function probe_level()
    local s = state
    return s.probe or 1 / (s.start.contrast or math.huge)
end

-- Results derived from the current probes.
local function results()
    local s = state
    local m = s.mode
    local r = {}
    if m.absolute then
        r.peak = clamp(round(m.k_to_nits(s.peak_k)), NITS_MIN, NITS_MAX)
        r.contrast = s.contrast
    else
        r.peak = s.white_nits
        -- The level found is the black point, as a fraction of white.
        local probe = probe_level()
        r.level = probe * r.peak
        r.contrast = nil
        if probe > 0 then
            local c = significant(1 / probe, 2)
            if c <= CONTRAST_MAX then
                r.contrast = math.max(c, CONTRAST_MIN)
            end
        end
    end
    r.black = r.contrast and r.peak / r.contrast or 0
    r.ref = s.ref_nits
    return r
end

local function update_target_options()
    local s = state
    local m = s.mode
    local page = s.pages[s.page]
    local r = results()
    if page == "peak" then
        -- Pass everything through, the pattern is limited by the display
        -- only. See the comment at the top.
        set_option("target-peak", tostring(METADATA_NITS))
        set_option("target-contrast", "inf")
        return
    end
    -- The black point is peak / contrast, so the black level is measured
    -- against the measured peak. The remaining pages preview the result and
    -- signal what playback will signal.
    if m.absolute then
        set_option("target-peak", tostring(r.peak))
        set_option("target-contrast", contrast_str(s.contrast))
        if s.has_ref and (page == "ref" or page == "summary") then
            set_option("hdr-reference-white", tostring(s.ref_nits))
        end
        return
    end
    -- The white the results refer to is made the white of both the source and
    -- the target, whatever is configured or reported, so that the pipeline
    -- renders the probe relative to it.
    set_option("target-peak", tostring(r.peak))
    set_option("hdr-reference-white", tostring(r.peak))
    if page == "black" then
        -- The probe must reach the display, the black point is derived from
        -- the level found.
        set_option("target-contrast", "inf")
    else
        set_option("target-contrast", contrast_str(r.contrast))
    end
end

local function update_pattern()
    local s = state
    local m = s.mode
    local page = s.pages[s.page]
    local p = {}
    local function set(k, v)
        p[s.prefix .. "/" .. k] = string.format("%.9g", v)
    end
    -- The reference white page shows the video, the black level page shows
    -- it inside the window only. The others draw the pattern, which is a black
    -- screen on the results page.
    set("draw", (page == "ref" and 0) or (page == "black" and 2) or 1)
    set("src_mode", page == "black" and 1 or 0)
    set("src_level", (page == "black" and not m.absolute) and
                     nits_to_pq(probe_level() * s.white_nits) or 0)
    set("stripes", page == "transfer" and 1 or 0)
    set("lvl_outside", 0)
    set("lvl_win", 0)
    set("lvl_probe", 0)
    set("win_area", 0)
    if page == "transfer" then
        set("lvl_win", STRIPE_LEVEL)
        set("lvl_probe", m.k_to_value(s.match_k))
        set("win_area", s.black_area / 100)
    elseif page == "black" then
        set("win_area", s.black_area / 100)
    elseif page == "peak" then
        set("lvl_win", m.max_value)
        set("lvl_probe", m.k_to_value(s.peak_k))
        set("win_area", s.peak_area / 100)
    elseif page == "white" then
        set("lvl_win", m.max_value)
        set("lvl_probe", m.max_value)
        set("win_area", s.peak_area / 100)
    end
    mp.set_property_native("glsl-shader-opts", p)
end

local PAGE_TITLES = {
    transfer = "Transfer function",
    black = "Black level",
    peak = "Peak luminance",
    white = "White luminance",
    ref = "SDR reference white",
    summary = "Results",
}

local HELP = {
    transfer = {
        "Adjust the disc until it blends into the stripes around it, viewed from",
        "a normal distance, then continue. It may blend in without adjustment.",
        "The level it matches at tells whether the display decodes sRGB as the",
        "piecewise curve or as a pure 2.2 power function.",
        "Use the native resolution and disable sharpening in the display, the",
        "stripes must reach the panel unscaled.",
    },
    black = {
        "Lower the black level until the patch is no longer visible against the",
        "black around it. Raise it first if the patch is not visible.",
        "The patch is black as mpv maps it to the black point, the area around",
        "it is the black of the display, including the light it reflects.",
    },
    black_sdr = {
        "Lower the level until the patch is no longer visible against the black",
        "around it. Raise it first if the patch is not visible.",
        "The patch is HDR content at the black point, rendered onto the black of",
        "the display. The level found is the black point.",
    },
    peak = {
        "Raise the level until the disc is no longer visible inside the white",
        "window. This is where the display clips. The peak of many displays",
        "depends on the size of the window, w changes it.",
    },
    white = {
        "The white luminance of an SDR output cannot be found by eye. Enter a",
        "value measured with a meter or taken from the display specification,",
        "or continue to keep the current setting. It is the white that HDR",
        "content is mapped to.",
    },
    ref = {
        "Set the brightness of SDR content to a comfortable level for the viewing",
        "environment. This is a preference, not a measurement, and can be changed",
        "later on real content.",
    },
}

local function readout_line(r)
    local s = state
    local m = s.mode
    local page = s.pages[s.page]
    local title = string.format("%s (%d/%d)", PAGE_TITLES[page], s.page, #s.pages - 1)
    if page == "transfer" then
        local verdict = string.format("continue to confirm  (sRGB %d, gamma 2.2 %d)",
                                      round(ANCHOR_SRGB * m.kmax), round(ANCHOR_GAMMA22 * m.kmax))
        local tf = classify_transfer()
        if tf then
            local g = effective_gamma()
            verdict = (tf == "srgb" and "sRGB piecewise" or "pure gamma 2.2") ..
                      (g and string.format("  (effective gamma %.2f)", g) or "")
        end
        return string.format("%s   code %d of %d   →  %s", title, s.match_k, m.kmax, verdict)
    elseif page == "black" and m.absolute then
        return string.format("%s   target-contrast=%s   (black %s cd/m² at %s cd/m² peak)",
                             title, contrast_str(r.contrast), fmt_nits(r.black), fmt_nits(r.peak))
    elseif page == "black" then
        return string.format("%s   black %s cd/m² (PQ code %d)   →  target-contrast=%s  " ..
                             "(at %s cd/m² white)",
                             title, fmt_nits(r.level), round(nits_to_pq(r.level) * PQ_STEPS),
                             contrast_str(r.contrast), fmt_nits(r.peak))
    elseif page == "peak" then
        return string.format("%s   PQ code %d of %d = %s cd/m²   →  target-peak=%d",
                             title, s.peak_k, m.kmax, fmt_nits(r.peak), r.peak)
    elseif page == "white" then
        return string.format("%s   %s cd/m²   →  hdr-reference-white=%d  (manual entry)",
                             title, fmt_nits(r.peak), r.peak)
    end
    return string.format("%s   %s cd/m²   →  hdr-reference-white=%d", title, fmt_nits(r.ref), r.ref)
end

local function render()
    local s = state
    local m = s.mode
    local page = s.pages[s.page]
    local top, bottom = {}, {}
    local function add(t, fmt, ...)
        t[#t + 1] = string.format(fmt, ...)
    end

    if not m then
        add(top, "Display calibration")
        add(top, s.status or "Waiting for video output...")
        add(bottom, "Esc abort")
    elseif page == "summary" then
        local r = results()
        add(top, "Display calibration: results")
        add(top, "")
        if m.absolute then
            add(top, "target-peak=%d", r.peak)
        else
            add(top, "hdr-reference-white=%d  (%s)", r.peak,
                s.white_touched and "entered manually" or "not measured, current setting")
        end
        add(top, "target-contrast=%s  (black level %s cd/m²)", contrast_str(r.contrast),
            fmt_nits(r.black))
        if s.has_ref then
            add(top, "hdr-reference-white=%d", r.ref)
        end
        local tf = not m.absolute and classify_transfer()
        if tf then
            add(top, "treat-srgb-as-power22=%s  (display decodes as %s)", treat_suggestion(tf),
                tf == "srgb" and "sRGB piecewise" or "pure gamma 2.2")
        end
        add(top, "")
        add(top, "The values are also written to the log. No settings have been changed.")
        add(bottom, "← back   a apply to this session   Enter/Esc finish")
    else
        local r = results()
        if s.help then
            add(top, "%s", PAGE_TITLES[page])
            local help = (page == "black" and not m.absolute) and HELP.black_sdr or HELP[page]
            for _, l in ipairs(help) do
                add(top, "%s", l)
            end
            add(top, "")
            local depth = m.bits and string.format("%d-bit%s", m.bits, m.float and " float" or "")
                                  or "unknown depth"
            add(top, "Output %s, %s, %s range", m.desc, depth, m.levels or "unknown")
            if m.absolute then
                -- What the options set for this page make mpv signal. The
                -- target params of the VO only follow with the next frame.
                local max = page == "peak" and METADATA_NITS or r.peak
                local min = page == "peak" and 0 or r.black
                add(top, "Signaled metadata max %s, min %s cd/m²",
                    fmt_nits(max), min < 0.0005 and "0" or fmt_nits(min))
            end
            for _, w in ipairs(s.warnings) do
                add(top, "Note, %s", w)
            end
            add(top, "")
            add(top, "↑/↓/wheel adjust   PgUp/PgDn coarse   r reset   w window size")
            add(top, "←/→ pages   Enter next   a apply on the last page   Esc finish")
        end
        add(bottom, "%s", readout_line(r))
        local hints = {"h help"}
        if #s.warnings > 0 then
            hints[#hints + 1] = "(" .. table.concat(s.warnings, ", ") .. ")"
        end
        add(bottom, "↑↓ adjust   Enter next   Esc finish   %s", table.concat(hints, "   "))
    end

    local color = page == "black" and "505050" or "A0A0A0"
    local style = string.format("\\bord1.2\\shad0\\1c&H%s&\\3c&H000000&", color)
    local function block(t, tags)
        local text = {}
        for _, l in ipairs(t) do
            text[#text + 1] = ass_escape(l)
        end
        return tags .. style .. "}" .. table.concat(text, "\\N")
    end
    s.ov.hidden = s.hidden or false
    s.ov.data = block(top, "{\\an7\\pos(20,16)\\fs22") .. "\n" ..
                block(bottom, "{\\an1\\pos(20,704)\\fs18")
    s.ov:update()
end

local function refresh()
    if state.mode then
        update_pattern()
        update_target_options()
        update_carrier_tag()
    end
    render()
end

-- Hide the readout while adjusting, only the pattern should be on screen.
-- It comes back once the input is idle.
local function hide_readout()
    local s = state
    s.hidden = true
    if s.show_timer then
        s.show_timer:kill()
        s.show_timer:resume()
    else
        s.show_timer = mp.add_timeout(opts.hide_time, function()
            s.hidden = false
            render()
        end)
    end
end

local function set_page(n)
    local s = state
    if not s.mode then
        return
    end
    -- Continuing from the transfer page confirms the match as shown, the
    -- disc may already blend in without any adjustment.
    if s.pages[s.page] == "transfer" and n > s.page then
        s.transfer_touched = true
        apply_transfer()
    end
    s.page = clamp(n, 1, #s.pages)
    s.hidden = false
    refresh()
end

-- Contrast steps are ratios, rounded to two significant digits.
local function step_contrast(dir, coarse)
    local s = state
    local factor = coarse and 2 or 1.1
    if not s.contrast then
        if dir < 0 then
            s.contrast = CONTRAST_MAX
        end
        return
    end
    local c = significant(dir > 0 and s.contrast * factor or s.contrast / factor, 2)
    if c > CONTRAST_MAX then
        s.contrast = nil
    else
        s.contrast = math.max(c, CONTRAST_MIN)
    end
end

local function adjust(dir, coarse)
    local s = state
    if not s.mode then
        return
    end
    local m = s.mode
    local page = s.pages[s.page]
    local step = coarse and m.coarse or 1
    if page == "transfer" then
        s.match_k = clamp(s.match_k + dir * step, 0, m.kmax)
        s.transfer_touched = true
        apply_transfer()
    elseif page == "black" and m.absolute then
        -- Up raises the black level, which lowers the contrast.
        step_contrast(-dir, coarse)
    elseif page == "black" then
        local k = round(nits_to_pq(probe_level() * s.white_nits) * PQ_STEPS)
        k = clamp(k + dir * (coarse and 8 or 1), 0, PQ_STEPS)
        s.probe = pq_to_nits(k / PQ_STEPS) / s.white_nits
    elseif page == "peak" then
        s.peak_k = clamp(s.peak_k + dir * step, m.nits_to_k(NITS_MIN), m.kmax)
    elseif page == "white" then
        s.white_nits = clamp(s.white_nits + dir * (coarse and 50 or 5), NITS_MIN, NITS_MAX)
        s.white_touched = true
    elseif page == "ref" then
        s.ref_nits = clamp(s.ref_nits + dir * (coarse and 25 or 5), NITS_MIN, NITS_MAX)
    else
        return
    end
    hide_readout()
    refresh()
end

local function reset_value()
    local s = state
    if not s.mode then
        return
    end
    local page = s.pages[s.page]
    if page == "transfer" then
        s.match_k = s.start.match_k
        s.transfer_touched = false
        apply_transfer()
    elseif page == "black" then
        s.contrast = s.start.contrast
        s.probe = nil
    elseif page == "peak" then
        s.peak_k = s.start.peak_k
    elseif page == "white" then
        s.white_nits = s.start.white_nits
        s.white_touched = false
    elseif page == "ref" then
        s.ref_nits = s.start.ref_nits
    end
    s.hidden = false
    refresh()
end

local function cycle_window()
    local s = state
    if not s.mode then
        return
    end
    local page = s.pages[s.page]
    local key = (page == "black" or page == "transfer") and "black_area" or "peak_area"
    if page ~= "transfer" and page ~= "black" and page ~= "peak" and page ~= "white" then
        return
    end
    local next_size = WINDOW_SIZES[1]
    for _, size in ipairs(WINDOW_SIZES) do
        if size > s[key] then
            next_size = size
            break
        end
    end
    s[key] = next_size
    s.hidden = false
    refresh()
end

local function toggle_help()
    local s = state
    if not s.mode then
        return
    end
    s.help = not s.help
    s.hidden = false
    render()
end

local function log_results()
    local s = state
    if not s.mode then
        return
    end
    local r = results()
    msg.info(string.format("Results for %s output:", s.mode.desc))
    if s.mode.absolute then
        msg.info(string.format("  target-peak=%d", r.peak))
    else
        msg.info(string.format("  hdr-reference-white=%d  (%s)", r.peak,
                               s.white_touched and "entered manually" or "not measured"))
    end
    msg.info(string.format("  target-contrast=%s  (black level %s cd/m2)",
                           contrast_str(r.contrast), fmt_nits(r.black)))
    if s.has_ref then
        msg.info(string.format("  hdr-reference-white=%d", r.ref))
    end
    local tf = not s.mode.absolute and classify_transfer()
    if tf then
        msg.info(string.format("  treat-srgb-as-power22=%s  (display decodes as %s)",
                               treat_suggestion(tf),
                               tf == "srgb" and "sRGB piecewise" or "pure gamma 2.2"))
    end
end

local function playlist_index_of(id)
    local playlist = mp.get_property_native("playlist") or {}
    for i, entry in ipairs(playlist) do
        if entry.id == id then
            return i - 1
        end
    end
end

local function remove_files(s)
    for _, path in ipairs(s.files) do
        os.remove(path)
    end
    s.files = {}
end

-- Called once the pattern entry is gone (or never started).
local function cleanup()
    local s = state
    state = nil
    mp.unobserve_property(s.on_target_params)
    mp.unregister_event(s.on_start_file)
    mp.unregister_event(s.on_end_file)
    mp.unregister_event(s.on_playback_restart)
    mp.unregister_event(s.on_shutdown)
    if s.show_timer then
        s.show_timer:kill()
    end
    for _, name in ipairs(s.bindings) do
        mp.remove_key_binding(name)
    end
    s.ov:remove()
    remove_files(s)
    local index = playlist_index_of(s.entry_id)
    if index then
        mp.commandv("playlist-remove", index)
    end
    if s.idle_once then
        -- Back to --idle=once when the next playback starts, the player is
        -- idle until then either way.
        restore_idle = function(e)
            if e.playlist_entry_id ~= s.entry_id then
                mp.unregister_event(restore_idle)
                restore_idle = nil
                mp.set_property("idle", "once")
            end
        end
        mp.register_event("start-file", restore_idle)
    end

    if s.apply then
        if s.apply.absolute then
            mp.set_property_native("target-peak", s.apply.peak)
        else
            mp.set_property_native("hdr-reference-white", s.apply.peak)
        end
        if s.apply.treat then
            mp.set_property("treat-srgb-as-power22", s.apply.treat)
        end
        mp.set_property("target-contrast", contrast_str(s.apply.contrast))
        if s.has_ref then
            mp.set_property_native("hdr-reference-white", s.apply.ref)
        end
        mp.osd_message("Calibration results applied to this session", 3)
    end

    if s.entry_id and s.saved_time and s.saved_id then
        local function restore_position()
            local id = current_entry_id()
            if id == s.entry_id then
                return
            end
            mp.unregister_event(restore_position)
            if id == s.saved_id then
                mp.commandv("seek", s.saved_time, "absolute")
            end
        end
        mp.register_event("file-loaded", restore_position)
    end
end

local function finish(apply)
    local s = state
    if s.finishing then
        return
    end
    s.finishing = true
    if apply and s.mode then
        s.apply = results()
        s.apply.absolute = s.mode.absolute
        local tf = not s.mode.absolute and classify_transfer()
        s.apply.treat = tf and treat_suggestion(tf) or nil
    end
    log_results()
    -- Leaving the pattern entry restores all options set on it. cleanup()
    -- follows through the end-file event.
    if s.saved_pos >= 0 then
        mp.commandv("playlist-play-index", s.saved_pos)
    else
        mp.commandv("playlist-play-index", "none")
    end
    if not s.started then
        cleanup()
    end
end

local function abort(reason)
    msg.error(reason)
    mp.osd_message("Calibration failed: " .. reason, 5)
    finish(false)
end

-- Initialize the pages for the negotiated output.
local function configure(tp)
    local s = state
    local mode = make_mode(tp)
    s.mode = mode

    local peak = tp["max-luma"]
    local black = tp["min-luma"] or 0
    local peak_known = peak and peak >= NITS_MIN and peak < NITS_MAX
    local start = {}
    if mode.absolute then
        start.peak_k = clamp(mode.nits_to_k(peak_known and peak or 1000),
                             mode.nits_to_k(NITS_MIN), mode.kmax)
        start.white_nits = 0
    else
        -- Prefer the configured luminance over the reported one, it is what
        -- mpv would use.
        local nits = type(s.user_ref_num) == "number" and s.user_ref_num or
                     type(s.user_peak) == "number" and s.user_peak or
                     peak_known and peak or 203
        start.white_nits = clamp(round(nits), NITS_MIN, NITS_MAX)
        start.peak_k = 0
    end
    -- The contrast currently in effect, inf below the sentinel.
    if peak_known and black > 1e-6 then
        local c = significant(peak / black, 2)
        start.contrast = c <= CONTRAST_MAX and math.max(c, CONTRAST_MIN) or nil
    else
        start.contrast = nil
    end
    start.match_k = round((mode.gamma == "srgb" and ANCHOR_SRGB or ANCHOR_GAMMA22) * mode.kmax)
    start.ref_nits = type(s.user_ref_num) == "number" and s.user_ref_num or 203
    s.start = start
    s.peak_k = start.peak_k
    s.white_nits = start.white_nits
    s.contrast = start.contrast
    s.probe = nil
    s.match_k = start.match_k
    s.ref_nits = start.ref_nits
    s.transfer_touched = false
    s.white_touched = false
    -- What auto resolves to on this platform, known when it was in effect.
    s.auto_gamma = s.user_treat == "auto" and mode.gamma or nil

    s.has_ref = mode.absolute and not is_source_hint_mode()
    s.pages = {}
    if mode.absolute then
        -- The peak first, the black level is measured against it.
        s.pages[#s.pages + 1] = "peak"
        s.pages[#s.pages + 1] = "black"
    else
        if SRGB_CLASS[mode.gamma] then
            s.pages[#s.pages + 1] = "transfer"
        end
        s.pages[#s.pages + 1] = "black"
        s.pages[#s.pages + 1] = "white"
    end
    if s.has_ref then
        s.pages[#s.pages + 1] = "ref"
    end
    s.pages[#s.pages + 1] = "summary"

    s.warnings = {}
    if not mp.get_property_bool("fullscreen") then
        s.warnings[#s.warnings + 1] = "not fullscreen"
    end
    local icc = mp.get_property("icc-profile")
    if (icc and icc ~= "") or mp.get_property_bool("icc-profile-auto") then
        s.warnings[#s.warnings + 1] = "ICC profile is bypassed by the patterns"
    end

    set_page(1)
end

local function load_pattern()
    local s = state
    local dir = temp_dir()
    local name = "mpv-calibrate-" .. utils.getpid()
    local shader = utils.join_path(dir, name .. ".glsl")
    local image = utils.join_path(dir, name .. ".ppm")
    s.prefix = name

    local w, h = 1920, 1080
    local dims = mp.get_property_native("osd-dimensions")
    if opts.fullscreen then
        w = mp.get_property_number("display-width") or w
        h = mp.get_property_number("display-height") or h
    elseif dims and dims.w > 0 and dims.h > 0 then
        w, h = dims.w, dims.h
    end
    w, h = clamp(w, 16, 4096), clamp(h, 16, 4096)

    local ok, err = write_file(shader, SHADER)
    if ok then
        s.files[#s.files + 1] = shader
        ok, err = write_file(image, ppm_image(w, h))
    end
    if not ok then
        remove_files(s)
        return false, "cannot write to " .. dir .. ": " .. tostring(err)
    end
    s.files[#s.files + 1] = image

    local vf = ""
    if s.request_hdr and is_source_hint_mode() then
        vf = "format=gamma=pq:primaries=bt.2020"
    end
    s.carrier_vf = vf
    s.applied_vf = vf
    local shader_opts = {}
    for _, k in ipairs({"src_mode=0", "src_level=0", "draw=1", "lvl_win=0", "lvl_probe=0",
                        "lvl_outside=0", "win_area=0.1", "stripes=0"}) do
        shader_opts[#shader_opts + 1] = name .. "/" .. k
    end

    -- Everything set here is restored when the entry stops playing, including
    -- later runtime changes of the same options. The tone mapping curve is
    -- pinned, the black level measurement relies on its black point
    -- compensation, which clip does not perform.
    local file_opts = {
        {"keepaspect", "no"},
        {"dither", "no"},
        {"gamut-mapping-mode", "clip"},
        {"tone-mapping", "auto"},
        {"inverse-tone-mapping", "no"},
        {"hdr-compute-peak", "no"},
        {"pause", "yes"},
        {"sub-visibility", "no"},
        {"secondary-sub-visibility", "no"},
        {"osc", "no"},
        {"image-display-duration", "inf"},
        {"loop-file", "no"},
        {"fullscreen", opts.fullscreen and "yes" or nil},
        {"vf", vf},
        {"glsl-shaders-clr", ""},
        {"glsl-shaders-append", shader},
        {"glsl-shader-opts", table.concat(shader_opts, ",")},
        {"target-peak", "auto"},
        {"target-contrast", "auto"},
        {"hdr-reference-white", s.user_ref},
        {"treat-srgb-as-power22", s.user_treat},
    }
    local parts = {}
    for _, kv in ipairs(file_opts) do
        if kv[2] then
            parts[#parts + 1] = string.format("%s=%%%d%%%s", kv[1], #kv[2], kv[2])
        end
    end

    local count = mp.get_property_number("playlist-count", 0)
    mp.commandv("loadfile", image, "append", "-1", table.concat(parts, ","))
    s.entry_id = mp.get_property_number("playlist/" .. count .. "/id")
    if not s.entry_id then
        remove_files(s)
        return false, "failed to add the pattern to the playlist"
    end
    mp.commandv("playlist-play-index", count)
    return true
end

local function next_page()
    local s = state
    if s.mode and s.page == #s.pages then
        finish(false)
    else
        set_page(s.page + 1)
    end
end

local function bind_keys()
    local s = state
    local function bind(key, name, fn, flags)
        mp.add_forced_key_binding(key, name, fn, flags)
        s.bindings[#s.bindings + 1] = name
    end
    local rep = {repeatable = true}
    bind("UP", "up", function() adjust(1) end, rep)
    bind("DOWN", "down", function() adjust(-1) end, rep)
    bind("WHEEL_UP", "wheel-up", function() adjust(1) end, rep)
    bind("WHEEL_DOWN", "wheel-down", function() adjust(-1) end, rep)
    bind("PGUP", "pgup", function() adjust(1, true) end, rep)
    bind("PGDWN", "pgdwn", function() adjust(-1, true) end, rep)
    bind("Shift+UP", "shift-up", function() adjust(1, true) end, rep)
    bind("Shift+DOWN", "shift-down", function() adjust(-1, true) end, rep)
    bind("RIGHT", "right", function() set_page(s.page + 1) end)
    bind("LEFT", "left", function() set_page(s.page - 1) end)
    bind("ENTER", "enter", next_page)
    bind("KP_ENTER", "kp-enter", next_page)
    bind("ESC", "esc", function() finish(false) end)
    bind("q", "quit", function() finish(false) end)
    bind("w", "window", cycle_window)
    bind("r", "reset", reset_value)
    bind("h", "help", toggle_help)
    bind("a", "apply", function()
        if s.mode and s.pages[s.page] == "summary" then
            finish(true)
        end
    end)
end

local function start(mode)
    if state then
        finish(false)
        return
    end
    -- Checked again once the pattern is shown, the VO may not exist yet.
    local current_vo = mp.get_property("current-vo")
    if current_vo and current_vo ~= "gpu-next" then
        local reason = "--vo=gpu-next is required, current VO is " .. current_vo
        msg.error(reason)
        mp.osd_message("Calibration failed: " .. reason, 5)
        return
    end
    if mp.get_property_bool("user-data/mpv/console/open") then
        mp.commandv("script-message-to", "console", "disable")
    end

    local target = mp.get_property_native("video-target-params")
    local request_hdr = target and (target.gamma == "pq" or target.gamma == "hlg" or
                                    target.gamma == "scrgb")
    if mode == "hdr" then
        request_hdr = true
    elseif mode == "sdr" then
        request_hdr = false
    end
    local pos = mp.get_property_number("playlist-pos", -1)
    state = {
        page = 1,
        help = true,
        pages = {},
        files = {},
        bindings = {},
        warnings = {},
        saved_pos = pos,
        saved_id = pos >= 0 and mp.get_property_number("playlist/" .. pos .. "/id") or nil,
        saved_time = mp.get_property_number("time-pos"),
        request_hdr = request_hdr,
        user_peak = mp.get_property_native("target-peak"),
        user_ref = mp.get_property("hdr-reference-white") or "auto",
        user_ref_num = mp.get_property_native("hdr-reference-white"),
        user_treat = mp.get_property("treat-srgb-as-power22") or "auto",
        black_area = opts.black_window,
        peak_area = opts.peak_window,
        ov = mp.create_osd_overlay("ass-events"),
    }
    local s = state

    -- An --idle=once player quits when its first playback stops. Keep it
    -- running, the calibration returns to the idle state it was started from.
    if pos < 0 and (restore_idle or mp.get_property("idle") == "once") then
        if restore_idle then
            mp.unregister_event(restore_idle)
            restore_idle = nil
        end
        mp.set_property("idle", "yes")
        s.idle_once = true
    end

    local function check_target_params(tp)
        if not s.loaded or s.finishing or not tp or not tp.gamma then
            return
        end
        local vo = mp.get_property("current-vo")
        if vo ~= "gpu-next" then
            abort("--vo=gpu-next is required, current VO is " .. tostring(vo))
            return
        end
        if not s.mode then
            configure(tp)
        elseif tp.pixelformat ~= s.mode.format or
               (tp.gamma ~= s.mode.gamma and
                not (SRGB_CLASS[tp.gamma] and SRGB_CLASS[s.mode.gamma]))
        then
            -- The swapchain was renegotiated, start over for the new output.
            configure(tp)
        elseif tp.gamma ~= s.mode.gamma then
            -- Same sRGB swapchain, the pipeline follows the detected curve.
            s.mode.gamma = tp.gamma
            s.mode.desc = "SDR (" .. tp.gamma .. ")"
            render()
        end
    end

    s.on_start_file = function(e)
        if e.playlist_entry_id == s.entry_id then
            s.started = true
        end
    end
    s.on_end_file = function(e)
        if e.playlist_entry_id ~= s.entry_id then
            return
        end
        if e.reason == "error" and not s.finishing then
            msg.error("failed to load the pattern: " .. tostring(e.error))
            mp.osd_message("Calibration failed: cannot display the pattern", 5)
            s.finishing = true
            if s.saved_pos >= 0 then
                mp.commandv("playlist-play-index", s.saved_pos)
            end
        end
        cleanup()
    end
    -- Only trust the target params once the pattern was rendered. Before that
    -- the property observer may still deliver the values of the previous file,
    -- and it does not fire at all if they happen to be identical.
    s.on_playback_restart = function()
        if current_entry_id() == s.entry_id then
            s.loaded = true
            check_target_params(mp.get_property_native("video-target-params"))
        end
    end
    s.on_target_params = function(_, tp)
        check_target_params(tp)
    end
    s.on_shutdown = function() remove_files(s) end
    mp.register_event("start-file", s.on_start_file)
    mp.register_event("end-file", s.on_end_file)
    mp.register_event("playback-restart", s.on_playback_restart)
    mp.register_event("shutdown", s.on_shutdown)
    mp.observe_property("video-target-params", "native", s.on_target_params)
    bind_keys()
    render()

    local ok, err = load_pattern()
    if not ok then
        s.entry_id = nil
        msg.error(err)
        mp.osd_message("Calibration failed: " .. err, 5)
        cleanup()
    end
end

mp.add_key_binding(nil, "start", function() start() end)
mp.register_script_message("start", start)
