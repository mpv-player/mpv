--[[
    puzzle.lua - Jigsaw puzzle mini-game for mpv

    Scrambles the video into draggable jigsaw pieces. Pieces snap back into
    place when dropped near their home position.

    Default keybindings:
        p       toggle puzzle on/off
        P       reshuffle pieces

    Script options (script-opts/puzzle.conf):
        cols=4              grid columns
        rows=4              grid rows
        snap_threshold=0.04 snap distance in normalised [0,1] video coords
--]]

local mp   = require "mp"
local opts = require "mp.options"

local options = {
    cols           = 4,
    rows           = 4,
    snap_threshold = 0.04,
}
opts.read_options(options, "puzzle")

options.cols = math.max(2, options.cols)
options.rows = math.max(2, options.rows)

local pieces      = {}   -- [1..n]  { cx, cy, home_x, home_y, snapped }
local vconn       = {}   -- [1..rows*(cols-1)]  +1=tab / -1=blank on right edge
local hconn       = {}   -- [1..(rows-1)*cols]  +1=tab / -1=blank on bottom edge
local z_order     = {}   -- piece indices bottom→top; topmost rendered last (wins)
local dragging    = nil  -- 1-based piece index currently held, or nil
local drag_ox     = 0    -- piece.cx - mouse_x at the moment of pick-up
local drag_oy     = 0
local shader_path = nil
local n_pieces    = 0
local pw          = 0    -- piece width  in normalised video coords
local ph          = 0    -- piece height in normalised video coords
local piece_ar    = 1.0  -- piece pixel aspect ratio: (pw * vid_w) / (ph * vid_h)
local osc_visibility = nil

local function get_norm_mouse()
    local mouse = mp.get_property_native("mouse-pos")
    local dims  = mp.get_property_native("osd-dimensions")
    if not dims or not mouse then return nil, nil end
    local vw = dims.w - dims.ml - dims.mr
    local vh = dims.h - dims.mt - dims.mb
    if vw <= 0 or vh <= 0 then return nil, nil end
    return (mouse.x - dims.ml) / vw, (mouse.y - dims.mt) / vh
end

local function find_piece_at(nx, ny)
    for i = n_pieces, 1, -1 do
        local p = pieces[z_order[i]]
        if nx >= p.cx and nx < p.cx + pw and
           ny >= p.cy and ny < p.cy + ph then
            return z_order[i]
        end
    end
    return nil
end

local function bring_to_top(idx)
    for i = 1, n_pieces do
        if z_order[i] == idx then
            table.remove(z_order, i)
            z_order[n_pieces] = idx
            return
        end
    end
end

local function try_snap(idx)
    local p = pieces[idx]
    if math.abs(p.cx - p.home_x) < options.snap_threshold and
       math.abs(p.cy - p.home_y) < options.snap_threshold then
        p.cx, p.cy = p.home_x, p.home_y
        p.snapped  = true
    end
end

local function check_win()
    for i = 1, n_pieces do
        if not pieces[i].snapped then return false end
    end
    return true
end

local function hide_osc()
    osc_visibility = mp.get_property_native("user-data/osc/visibility")
    mp.command("script-message osc-visibility never no-osd")
end

local function restore_osc()
    if osc_visibility ~= nil then
        mp.command(string.format("script-message osc-visibility %s no-osd",
                                 osc_visibility))
        osc_visibility = nil
    end
end

local function enable_drag_block()
    mp.input_define_section("puzzle_drag_block", "")
    mp.input_enable_section("puzzle_drag_block", "allow-hide-cursor")
    mp.input_set_section_mouse_area("puzzle_drag_block", 0, 0, 99999, 99999)
end

local function disable_drag_block()
    mp.input_disable_section("puzzle_drag_block")
end

local function update_shader_params()
    if not shader_path then return end
    local parts = {}
    for i = 1, n_pieces do
        parts[#parts + 1] = ("p%dx=%.6f"):format(i - 1, pieces[i].cx)
        parts[#parts + 1] = ("p%dy=%.6f"):format(i - 1, pieces[i].cy)
    end
    for i = 1, n_pieces do
        -- convert 1-based Lua indices to 0-based GLSL indices
        parts[#parts + 1] = ("z%d=%.1f"):format(i - 1, z_order[i] - 1)
    end
    parts[#parts + 1] = ("piece_ar=%.6f"):format(piece_ar)
    mp.commandv("no-osd", "change-list", "glsl-shader-opts", "add",
                table.concat(parts, ","))
end

local function generate_shader()
    local cols     = options.cols
    local rows     = options.rows
    local n        = cols * rows
    local vc_count = rows * (cols - 1)
    local hc_count = (rows - 1) * cols

    local lines = {}
    local function emit(s) lines[#lines + 1] = s or "" end

    emit("//!PARAM piece_ar")
    emit("//!TYPE DYNAMIC float")
    emit("//!MINIMUM 0.05")
    emit("//!MAXIMUM 20.0")
    emit(("%.6f"):format(piece_ar))
    emit("")

    for i = 0, n - 1 do
        local hc_i = i % cols
        local hr_i = math.floor(i / cols)
        emit(("//!PARAM p%dx"):format(i))
        emit("//!TYPE DYNAMIC float")
        emit("//!MINIMUM -0.5")
        emit("//!MAXIMUM 1.5")
        emit(("%.6f"):format(hc_i * pw))
        emit("")
        emit(("//!PARAM p%dy"):format(i))
        emit("//!TYPE DYNAMIC float")
        emit("//!MINIMUM -0.5")
        emit("//!MAXIMUM 1.5")
        emit(("%.6f"):format(hr_i * ph))
        emit("")
    end

    for i = 0, n - 1 do
        emit(("//!PARAM z%d"):format(i))
        emit("//!TYPE DYNAMIC float")
        emit("//!MINIMUM 0.0")
        emit(("//!MAXIMUM %.1f"):format(n - 1))
        emit(("%.1f"):format(i))
        emit("")
    end

    emit("//!HOOK OUTPUT")
    emit("//!BIND HOOKED")
    emit("//!DESC Jigsaw Puzzle")
    emit("")
    emit(("const int   N_COLS   = %d;"):format(cols))
    emit(("const int   N_ROWS   = %d;"):format(rows))
    emit(("const int   N_PIECES = %d;"):format(n))
    emit(("const float PW       = %.8f;"):format(pw))
    emit(("const float PH       = %.8f;"):format(ph))
    emit("const float TAB_R    = 0.20;")
    emit("const float BORDER_W = 0.025;")
    emit("")

    local function emit_conn_array(name, arr, count, fallback)
        if count > 0 then
            local vals = {}
            for k = 1, count do vals[k] = ("%.1f"):format(arr[k]) end
            emit(("const float %s[%d] = float[%d](%s);"):format(
                name, count, count, table.concat(vals, ",")))
        else
            emit(("const float %s[1] = float[1](%s);"):format(name, fallback))
        end
    end
    emit_conn_array("vc_arr", vconn, vc_count, "0.0")
    emit_conn_array("hc_arr", hconn, hc_count, "0.0")
    emit("")

    emit([[
float sdBox(vec2 p) {
    vec2 d = abs(p) - vec2(0.5);
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

// Piece-local space has non-square pixels for non-square grids on non-square
// video.  Scale x by piece_ar so the tab circle is measured in pixel-height
// units, making it appear circular on screen regardless of aspect ratio.
float applyConn(float d, vec2 p, vec2 tab_c, float conn) {
    vec2 diff = p - tab_c;
    float cd  = length(vec2(diff.x * piece_ar, diff.y)) - TAB_R;
    if (conn > 0.0) return min(d, cd);
    if (conn < 0.0) return max(d, -cd);
    return d;
}

// cr/cl/cb/ct: right/left/bottom/top connector type (+1=tab, -1=blank, 0=edge)
float sdPiece(vec2 p, float cr, float cl, float cb, float ct) {
    float d = sdBox(p);
    d = applyConn(d, p, vec2( 0.5,  0.0), cr);
    d = applyConn(d, p, vec2(-0.5,  0.0), cl);
    d = applyConn(d, p, vec2( 0.0,  0.5), cb);
    d = applyConn(d, p, vec2( 0.0, -0.5), ct);
    return d;
}
]])

    emit("vec4 hook() {")
    emit("    vec2 pos = HOOKED_pos;")
    emit("")
    emit(("    float px[%d];"):format(n))
    emit(("    float py[%d];"):format(n))
    for i = 0, n - 1 do
        emit(("    px[%d] = p%dx;  py[%d] = p%dy;"):format(i, i, i, i))
    end
    emit("")
    emit(("    float za[%d];"):format(n))
    for i = 0, n - 1 do
        emit(("    za[%d] = z%d;"):format(i, i))
    end
    emit("")
    emit("    vec4 result = vec4(0.0, 0.0, 0.0, 1.0);")
    emit("")
    emit([[
    for (int i = N_PIECES - 1; i >= 0; i--) {
        int idx = int(za[i]);

        // Cull with margin to include tabs (TAB_R = 0.20 of piece)
        float lx = (pos.x - px[idx]) / PW;
        float ly = (pos.y - py[idx]) / PH;
        if (lx < -0.25 || lx > 1.25 || ly < -0.25 || ly > 1.25) continue;

        vec2 p   = vec2(lx - 0.5, ly - 0.5);
        int  hci = idx % N_COLS;
        int  hri = idx / N_COLS;
        float cr = (hci < N_COLS-1) ? vc_arr[hri*(N_COLS-1)+hci]    : 0.0;
        float cl = (hci > 0)        ? -vc_arr[hri*(N_COLS-1)+hci-1] : 0.0;
        float cb = (hri < N_ROWS-1) ? hc_arr[hri*N_COLS+hci]        : 0.0;
        float ct = (hri > 0)        ? -hc_arr[(hri-1)*N_COLS+hci]   : 0.0;

        float d = sdPiece(p, cr, cl, cb, ct);
        if (d >= 0.0) continue;

        float src_x = (float(hci) + lx) * PW;
        float src_y = (float(hri) + ly) * PH;
        vec4 color  = HOOKED_tex(vec2(src_x, src_y));

        float border = 1.0 - smoothstep(0.0, BORDER_W, -d);
        color.rgb    = mix(color.rgb, vec3(0.05), border * 0.8);
        result       = color;
        break;
    }

    return result;
}]])

    return table.concat(lines, "\n")
end

local function init_puzzle()
    local cols = options.cols
    local rows = options.rows
    n_pieces   = cols * rows
    pw         = 1.0 / cols
    ph         = 1.0 / rows

    local vp = mp.get_property_native("video-out-params")
            or mp.get_property_native("video-params")
    if vp then
        piece_ar = (pw * (vp.dw or vp.w or 1)) / (ph * (vp.dh or vp.h or 1))
    else
        piece_ar = 1.0
    end

    vconn = {}
    for i = 1, rows * (cols - 1) do
        vconn[i] = (math.random(2) == 1) and 1.0 or -1.0
    end
    hconn = {}
    for i = 1, (rows - 1) * cols do
        hconn[i] = (math.random(2) == 1) and 1.0 or -1.0
    end

    pieces = {}
    for i = 0, n_pieces - 1 do
        pieces[i + 1] = {
            cx      = math.random() * (1.0 - pw),
            cy      = math.random() * (1.0 - ph),
            home_x  = (i % cols) * pw,
            home_y  = math.floor(i / cols) * ph,
            snapped = false,
        }
    end

    z_order = {}
    for i = 1, n_pieces do z_order[i] = i end

    shader_path = "memory://" .. generate_shader()
    mp.commandv("change-list", "glsl-shaders", "append", shader_path)
    update_shader_params()
    return true
end

local function cleanup_puzzle()
    restore_osc()
    disable_drag_block()
    mp.remove_key_binding("puzzle-mbtn-left")
    mp.remove_key_binding("puzzle-mouse-move")
    if shader_path then
        mp.commandv("change-list", "glsl-shaders", "remove", shader_path)
        shader_path = nil
    end
    pieces   = {}
    z_order  = {}
    dragging = nil
    n_pieces = 0
end

local function on_mouse_move()
    local mx, my = get_norm_mouse()
    if not mx then return end
    -- Clamp so at least 25% of the piece stays inside the video area.
    local min_vis_x = pw * 0.25
    local min_vis_y = ph * 0.25
    pieces[dragging].cx = math.max(-pw + min_vis_x, math.min(1.0 - min_vis_x, mx + drag_ox))
    pieces[dragging].cy = math.max(-ph + min_vis_y, math.min(1.0 - min_vis_y, my + drag_oy))
    update_shader_params()
end

local function on_mbtn_left(t)
    if t.event == "down" then
        local nx, ny = get_norm_mouse()
        if not nx then return end
        local idx = find_piece_at(nx, ny)
        if not idx then return end
        bring_to_top(idx)
        dragging = idx
        drag_ox  = pieces[idx].cx - nx
        drag_oy  = pieces[idx].cy - ny
        pieces[idx].snapped = false
        update_shader_params()
        mp.add_forced_key_binding("MOUSE_MOVE", "puzzle-mouse-move", on_mouse_move)
    elseif t.event == "up" then
        if not dragging then return end
        mp.remove_key_binding("puzzle-mouse-move")
        try_snap(dragging)
        dragging = nil
        update_shader_params()
        if check_win() then
            cleanup_puzzle()
            mp.osd_message("Puzzle solved!")
        end
    end
end

local active = false

local function start_puzzle()
    if not mp.get_property("video-format") then
        mp.osd_message("Puzzle: no video loaded")
        return false
    end
    math.randomseed(os.time())
    if not init_puzzle() then return false end
    enable_drag_block()
    hide_osc()
    mp.add_forced_key_binding("MBTN_LEFT", "puzzle-mbtn-left",
                              on_mbtn_left, {complex = true})
    return true
end

local function toggle_puzzle()
    if active then
        active = false
        cleanup_puzzle()
        mp.osd_message("Puzzle off")
    else
        if start_puzzle() then
            active = true
            mp.osd_message(("Puzzle on (%d\xC3\x97%d)"):format(
                options.cols, options.rows))
        end
    end
end

local function reshuffle()
    if not active then return end
    cleanup_puzzle()
    if start_puzzle() then
        mp.osd_message("Puzzle reshuffled")
    else
        active = false
    end
end

mp.add_key_binding("p", "puzzle-toggle",    toggle_puzzle)
mp.add_key_binding("P", "puzzle-reshuffle", reshuffle)
