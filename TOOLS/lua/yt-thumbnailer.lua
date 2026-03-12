--[[
This script uses youtube-dl/yt-dlp's storyboard feature to download pregenerated
thumbnails for youtube (and other supported sites) and render a preview
thumbnail on hover via the osc's thumbnailer api.

This script requires the following binaries to be available in your $PATH:
    1. curl    (to fetch the storyboard)
    2. ffmpeg  (to extract and resize the thumbnails)

This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
For more information, please refer to <https://unlicense.org/>
--]]

local msg = require 'mp.msg'
local utils = require 'mp.utils'
local mpopt = require 'mp.options'

local opt = {
    -- Limit the number of subprocesses to launch asynchronously
    proc_limit = 4,
    -- Limit the number of raw bgra thumbnail files extracted to disk.
    -- Note this does not include fragments. They are kept as long as the file is still playing.
    raw_thumb_limit = 64,
    -- Temporary directory within which the thumbnail cache directory will be created
    -- Defaults to "$TMPDIR" or "/tmp". On windows defaults to %TEMP%, %TMP% or %LOCALAPPDATA%.
    tmpdir = "",
    -- Fetches all fragments immediately. This is usually faster due to curl
    -- being able to reuse connections. If disabled, fragments will be fetched
    -- only when they're needed, one curl invocation per fragment. Greedy mode
    -- is recommended unless you have severe bandwidth restrictions.
    greedy_fetch = true,
    -- Change if this conflicts with another scripts
    -- see: https://github.com/mpv-player/mpv/issues/17534
    overlay_id = 32,
}

local NIL_REQUEST = { hover_sec = -1, x = 0, y = 0, w = 0, h = 0 }
local function NIL_SB()
    return { rows = 0, cols = 0, thumb_w = 0, thumb_h = 0, fragments = {} }
end

local state = {
    enabled = false,
    sb = NIL_SB(),
    current_request = NIL_REQUEST,
    tmpdir = nil,
    num_proc = 0,
    extracting = {},
    fetching = {},
    thumb_cache = {},
    thumb_lru = {},
    platform = "",
}

local thumb_show, fetch_sprite, extract_thumb -- "forward declare"

local function get_tmpdir()
    if state.tmpdir == nil then
        local base = opt.tmpdir
        if base == "" then
            if state.platform == "windows" then
                base = os.getenv("TEMP") or os.getenv("TMP") or os.getenv("LOCALAPPDATA")
                if base == nil then
                    error("Failed to find a suitable temporary directory " ..
                        "(TEMP, TMP, LOCALAPPDATA all unset)")
                end
            else
                base = os.getenv("TMPDIR") or "/tmp"
            end
        end
        local tmpdir = utils.join_path(base, "mpv-yt-thumb-" .. tostring(utils.getpid()))
        local cmd = { "mkdir" }
        if state.platform ~= "windows" then
            table.insert(cmd, "-p") -- windows mkdir creates parents by default
        end
        table.insert(cmd, tmpdir)
        local res = mp.command_native({
            name          = "subprocess",
            args          = cmd,
            playback_only = false,
        })
        state.tmpdir = tmpdir
        if res.status ~= 0 then
            error(tmpdir .. ": Failed to create temp dir. Exiting...")
        end
    end
    return state.tmpdir
end

local function clean_tmpdir()
    if not state.tmpdir then return end
    local cmd = {"rm", "-rf", "--", state.tmpdir}
    if state.platform == "windows" then
        cmd = {"rmdir", "/s", "/q", state.tmpdir}
    end
    mp.command_native({
        name          = "subprocess",
        args          = cmd,
        playback_only = false,
    })
    state.tmpdir = nil
    state.thumb_cache = {}
    state.thumb_lru = {}
end

local function murder(t)
    msg.debug("Murdering " .. tostring(#t) .. " running processes...")
    for k,v in pairs(t) do
        mp.abort_async_command(v)
        t[k] = nil
    end
    return {}
end

local function thumb_lru_add(path)
    state.thumb_cache[path] = true
    table.insert(state.thumb_lru, path)
    while #state.thumb_lru > opt.raw_thumb_limit do
        local old = table.remove(state.thumb_lru, 1)
        os.remove(old)
        state.thumb_cache[old] = nil
        msg.debug("Evicted cached thumbnail: " .. old)
    end
end

local function thumb_lru_access(path)
    if state.thumb_cache[path] then
        for i,v in ipairs(state.thumb_lru) do
            if v == path then
                table.remove(state.thumb_lru, i)
                table.insert(state.thumb_lru, path)
                break
            end
        end
        return true
    end
    return false
end

local function make_frag_path(index)
    return utils.join_path(get_tmpdir(), "ytdl-frag." .. tostring(index))
end

local function fetch_all_sprites(fragments)
    local cmd = { "curl", "--retry", "3", "--retry-delay", "1", "-Ss" }
    for i,frag in ipairs(fragments) do
        frag.path = make_frag_path(i)
        table.insert(cmd, "-o")
        table.insert(cmd, frag.path)
        table.insert(cmd, frag.url)
    end

    msg.debug("Greedily fetching all fragments: " .. utils.to_string(cmd))

    local r = mp.command_native({
        name = "subprocess",
        args = cmd,
        capture_stderr = true,
        playback_only = false,
    })
    if r.status ~= 0 then
        msg.error("Failed to fetch fragments")
        -- conservatively assumes nothing succeeded and will fallback to lazy fetches.
        for _,frag in ipairs(fragments) do
            -- TODO: only clear out the ones that actually failed
            frag.path = nil
        end
    end
end

-- return path if it's fetched already, otherwise kick off an async command
fetch_sprite = function(sprite_idx)
    local frag_path = state.sb.fragments[sprite_idx].path
    if frag_path ~= nil then
        return frag_path
    end
    frag_path = make_frag_path(sprite_idx)

    if state.num_proc >= opt.proc_limit then
        -- we can either kill one of the running processes or wait for one of them to finish.
        -- the former approach needs more care in order to avoid starting and
        -- killing a bunch of processes in rapid succession.
        -- this takes the latter approach, when one of the running task
        -- completes, it will call show_thumb() which will start processing the
        -- most recent request.
        msg.verbose("Reached subprocess limit")
        return nil
    end
    if state.fetching[frag_path] then
        msg.debug("Fragment fetch is in progress")
        return nil
    end
    state.num_proc = state.num_proc + 1
    state.fetching[frag_path] = mp.command_native_async({
        name = "subprocess",
        args = {
            "curl", "--retry", "3", "--retry-delay", "1",
            "-Ss", state.sb.fragments[sprite_idx].url,
            "-o", frag_path
        },
        capture_stderr = true,
        playback_only = false,
    }, function(success, res, err)
        state.fetching[frag_path] = nil
        state.num_proc = state.num_proc - 1
        if success and res.status == 0 then
            state.sb.fragments[sprite_idx].path = frag_path
            thumb_show()
        else
            local stderr = res and ("\n" .. res.stderr) or ""
            msg.error("Failed to fetch storyboard fragment: " .. err .. stderr)
        end
    end)
    return nil
end

-- same architecture as fetch_sprite()
extract_thumb = function(sprite_path, sprite_idx, thumb_idx, req)
    local bgra_base = string.format("ytdl-thumb-%d-%d-%dx%d.bgra",
        sprite_idx, thumb_idx, req.w, req.h)
    local bgra_path = utils.join_path(get_tmpdir(), bgra_base)

    if thumb_lru_access(bgra_path) then
        return bgra_path
    end

    local thumb_row = math.floor(thumb_idx / state.sb.cols)
    local thumb_col = thumb_idx % state.sb.cols
    local src_x = thumb_col * state.sb.thumb_w
    local src_y = thumb_row * state.sb.thumb_h

    if state.num_proc >= opt.proc_limit then
        msg.verbose("Reached subprocess limit")
        return nil
    end
    if state.extracting[bgra_path] then
        msg.debug("Thumbnail extraction already in progress: " .. bgra_base)
        return nil
    end
    state.num_proc = state.num_proc + 1
    state.extracting[bgra_path] = mp.command_native_async({
        name = "subprocess",
        args = {
            "ffmpeg", "-y", "-loglevel", "error",
            "-i", sprite_path,
            "-vf", string.format("crop=%d:%d:%d:%d,scale=%d:%d",
                state.sb.thumb_w, state.sb.thumb_h, src_x, src_y, req.w, req.h),
            "-f", "rawvideo", "-pix_fmt", "bgra", bgra_path,
        },
        capture_stderr = false,
        playback_only  = false,
    }, function(success, res, err)
        state.extracting[bgra_path] = nil
        state.num_proc = state.num_proc - 1
        if success and res.status == 0 then
            thumb_lru_add(bgra_path)
            thumb_show()
        else
            local stderr = res and ("\n" .. res.stderr) or ""
            msg.warn("Thumbnail extraction failed: " .. err .. stderr)
        end
    end)
    return nil
end

-- this always tries to show the current request. if sprite/thumbnail isn't
-- immediately available, will kick off an async command that "restarts" this
-- function upon completion.
thumb_show = function()
    local req = state.current_request
    if req.hover_sec < 0 or not state.enabled then return end

    local sprite_idx = 0
    local sprite_delta = math.huge
    for i = #state.sb.fragments, 1, -1 do
        local d = math.abs(req.hover_sec - state.sb.fragments[i].start_time)
        if d < sprite_delta then
            sprite_idx = i
            sprite_delta = d
        end
    end
    if sprite_idx == 0 then
        mp.command_native({"overlay-remove", opt.overlay_id})
        return
    end

    local frag = state.sb.fragments[sprite_idx]
    local num_thumbs = state.sb.rows * state.sb.cols
    local fragtime = req.hover_sec - state.sb.fragments[sprite_idx].start_time
    local thumb_idx = math.min(math.floor(fragtime / frag.duration * num_thumbs), num_thumbs - 1)

    local thumb_path = nil
    local sprite_path = fetch_sprite(sprite_idx)
    if sprite_path ~= nil then
        thumb_path = extract_thumb(sprite_path, sprite_idx, thumb_idx, req)
    end
    if thumb_path == nil then
        mp.command_native({"overlay-remove", opt.overlay_id})
        return
    end

    mp.command_native({
        name   = "overlay-add",
        id     = opt.overlay_id,
        x      = req.x,
        y      = req.y,
        file   = thumb_path,
        offset = 0,
        fmt    = "bgra",
        w      = req.w,
        h      = req.h,
        stride = req.w * 4,
    })
end

-- callbacks

local function on_ytdl_result(_, jsonstr)
    msg.debug("Received ytdl update")

    clean_tmpdir()
    state.sb = NIL_SB()
    state.current_request = NIL_REQUEST
    state.extracting = murder(state.extracting)
    state.fetching = murder(state.fetching)

    mp.set_property_native("user-data/mpv/thumbnailer/enabled", false)
    state.enabled = false

    if not jsonstr then return end

    local json, parse_err = utils.parse_json(jsonstr)
    if not json then
        msg.warn("Could not parse ytdl JSON: " .. (parse_err or "?"))
        return
    end

    local sb_fmt = nil
    local sb_preference = {
        sb0 = 4, sb1 = 3, sb2 = 2, sb3 = 1
    }
    local sb_prio = 0
    for _, format in pairs(json.formats or {}) do
        local prio = sb_preference[format.format_id] or 0
        if prio > sb_prio then
            sb_fmt = format
            sb_prio = prio
        end
    end
    if sb_fmt == nil then
        msg.verbose("Storyboard format not available")
        return
    end

    local rows = sb_fmt.rows or 0
    local cols = sb_fmt.columns or 0
    local tw = sb_fmt.width or 0
    local th = sb_fmt.height or 0
    local valid = json.duration and sb_fmt.width and sb_fmt.height and
        sb_fmt.fragments and #sb_fmt.fragments > 0 and
        tw > 0 and th > 0 and rows > 0 and cols > 0
    if not valid then
        msg.warn("Storyboard format is corrupted/unexpected")
        return
    end

    local t = 0
    local frags = sb_fmt.fragments
    for i, frag in ipairs(frags) do
        frags[i].start_time = t
        if frag.duration == nil or frag.duration == 0 then
            msg.warn("Fragment with missing/zero duration")
            frag.duration = 0.1
        end
        t = t + frag.duration
    end

    state.sb = {
        rows       = rows,
        cols       = cols,
        thumb_w    = tw,
        thumb_h    = th,
        fragments  = frags,
    }

    if opt.greedy_fetch then
        fetch_all_sprites(frags)
    end

    state.enabled = true
    mp.set_property_native("user-data/mpv/thumbnailer/enabled", true)
end

local function on_draw_request(_, req)
    msg.trace("received draw request: " .. utils.to_string(req))
    if not state.enabled then return end
    if req == nil then
        state.current_request = NIL_REQUEST
        mp.command_native({"overlay-remove", opt.overlay_id})
    elseif req.hover_sec and req.x and req.y and req.w and req.h then
        state.current_request = req
        thumb_show()
    else
        state.current_request = NIL_REQUEST
        msg.error("user-data/mpv/thumbnailer/draw: received malformed property")
    end
end

----------------

mpopt.read_options(opt)

mp.register_event("shutdown", clean_tmpdir)
mp.observe_property("platform", "native", function(_, data) state.platform = data or "" end)
mp.observe_property("user-data/mpv/thumbnailer/draw", "native", on_draw_request)
mp.observe_property("user-data/mpv/ytdl/json-subprocess-result/stdout", "native", on_ytdl_result)
