local mp = require("mp")
local utils = require("mp.utils")
local options = require("mp.options")

local o = {
    base_output_dir = "C:/Mediashots",
    movies_subdir = "Movies",
    shows_subdir = "Shows",
    fallback_subdir = "Unsorted",
    image_format = "png",
    include_year_in_movie_folder = false,
    zero_pad_season_episode = true,
    screenshot_key = "Win+y",
    fallback_screenshot_key = "Win+Print",
    debug_trigger_key = "Ctrl+Alt+y",
    show_osd_confirmation = true,
    save_debug_log = false,
    debug_trigger_feedback = true,
    debug_trigger_captures_screenshot = true,
    support_daily_show_folders = false,
}

local release_junk_tokens = {
    ["1080p"] = true,
    ["720p"] = true,
    ["2160p"] = true,
    ["x264"] = true,
    ["x265"] = true,
    ["h264"] = true,
    ["h265"] = true,
    ["hevc"] = true,
    ["bluray"] = true,
    ["bdrip"] = true,
    ["brrip"] = true,
    ["webrip"] = true,
    ["webdl"] = true,
    ["web"] = true,
    ["hdr"] = true,
    ["dv"] = true,
    ["hdtv"] = true,
    ["proper"] = true,
    ["repack"] = true,
    ["remux"] = true,
    ["amzn"] = true,
    ["nf"] = true,
}

local show_binding_name = "screenshot-folder-capture"
local fallback_binding_name = "screenshot-folder-capture-fallback"

local strip_release_junk
local extract_year_metadata
local parse_date
local parse_episode_title

local function debug_log(message)
    if o.save_debug_log then
        mp.msg.info("[screenshot-folder] " .. message)
    end
end

local function notify_user(message)
    if o.show_osd_confirmation then
        mp.osd_message(message, 2)
    end
end

local function debug_trigger_message(source)
    if o.debug_trigger_feedback then
        notify_user("screenshot-folder trigger: " .. source)
    end
    debug_log("trigger source=" .. source)
end

local function is_url(path)
    return type(path) == "string" and path:match("^%a[%w+.-]*://") ~= nil
end

local function trim(s)
    if not s then
        return ""
    end
    return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

local function normalize_spaces(s)
    return trim((s or ""):gsub("%s+", " "))
end

local function normalize_separators(s)
    local out = s or ""
    out = out:gsub("[_%.]+", " ")
    out = out:gsub("%s*%-%s*", " - ")
    out = out:gsub("%s+", " ")
    return trim(out)
end

local function pad2(value)
    local n = tonumber(value)
    if not n then
        return nil
    end
    if o.zero_pad_season_episode then
        return string.format("%02d", n)
    end
    return tostring(n)
end

local function sanitize_name(name)
    local s = normalize_spaces(name)
    s = s:gsub("[<>:\"/\\|%?%*]", "")
    s = s:gsub("%s+", " ")
    s = s:gsub("[ %.]+$", "")
    s = trim(s)
    if s == "" then
        return "Unknown"
    end
    return s
end

local function cleanup_series_title(name)
    local s = normalize_spaces(name)
    s = s:gsub("%(%s*%)", "")
    s = s:gsub("%s+%-%s*$", "")
    s = s:gsub("%s+", " ")
    s = trim(s)
    return sanitize_name(s)
end

local function is_plausible_season_episode(season, episode, pattern)
    local s = tonumber(season)
    local e = tonumber(episode)

    if s and (s < 0 or s > 60) then
        return false
    end
    if e and (e < 0 or e > 500) then
        return false
    end

    if pattern == "x-style" then
        if (s and s > 30) or (e and e > 40) then
            return false
        end
    end

    return true
end

local function has_x_style_episode_marker(text)
    local _, _, season, episode = (text or ""):find("%f[%d](%d%d?)x(%d%d?)%f[%D]")
    if not season then
        return false
    end
    return is_plausible_season_episode(season, episode, "x-style")
end

local function parse_explicit_sxe_from_text(text)
    local normalized = normalize_separators(text or "")
    local lowered = normalized:lower()
    local start_idx, end_idx, empty_idx, season, episode = lowered:find("()s(%d%d?)%s*e(%d%d?)")
    if not start_idx then
        return nil
    end

    if not is_plausible_season_episode(season, episode, "sxe") then
        return nil
    end

    local show_title = normalize_spaces(normalized:sub(1, start_idx - 1))
    show_title = strip_release_junk(show_title)
    local clean_title, year = extract_year_metadata(show_title)
    show_title = cleanup_series_title(clean_title)

    local episode_title
    if end_idx < #normalized then
        episode_title = parse_episode_title(normalized:sub(end_idx + 1))
    end

    if show_title == "Unknown" then
        return nil
    end

    return {
        show_title = show_title,
        season = pad2(season),
        episode = pad2(episode),
        episode_title = episode_title,
        date = parse_date(text),
        year = year,
        confidence = "high",
        score = 130,
        matched_pattern = "filename-explicit-sxe",
        evidence = "filename",
    }
end

local function split_path_components(path)
    local parts = {}
    for part in (path or ""):gmatch("[^/\\]+") do
        table.insert(parts, part)
    end
    return parts
end

strip_release_junk = function(text)
    local normalized = normalize_separators(text)
    local kept = {}
    for token in normalized:gmatch("[^%s]+") do
        local check = token:lower():gsub("^%W+", ""):gsub("%W+$", "")
        local is_junk = release_junk_tokens[check]
            or check:match("^%d%d%d?%d?p$")
            or check:match("^10bit$")
            or check:match("^aac%d?$")
            or check:match("^ddp%d?$")
            or check:match("^dts%d?$")
            or check:match("^atmos$")
            or check:match("^[^-]+%-%w+$")
        if not is_junk then
            table.insert(kept, token)
        end
    end
    return normalize_spaces(table.concat(kept, " "))
end

extract_year_metadata = function(text)
    local clean = normalize_spaces(text)
    local year = clean:match("%f[%d](19%d%d)%f[%D]") or clean:match("%f[%d](20%d%d)%f[%D]")
    if year then
        clean = clean:gsub("%f[%d]" .. year .. "%f[%D]", "")
        clean = normalize_spaces(clean)
    end
    return clean, year
end

parse_date = function(text)
    if not text then
        return nil
    end
    local y, m, d = text:match("(20%d%d)[%._%-](%d%d)[%._%-](%d%d)")
    if not y then
        return nil
    end
    return string.format("%s-%s-%s", y, m, d)
end

parse_episode_title = function(fragment)
    local t = normalize_separators(fragment)
    t = t:gsub("^%-+", "")
    t = strip_release_junk(t)
    t = t:gsub("^%-+", "")
    t = trim(t)
    if t == "" then
        return nil
    end
    return sanitize_name(t)
end

local function get_current_media_info()
    local full_path = mp.get_property("path", "")
    local filename = mp.get_property("filename", "")
    local basename = mp.get_property("filename/no-ext", "")
    local media_title = mp.get_property("media-title", "")
    local playback_time = mp.get_property_number("time-pos", 0) or 0

    local parent_folders = {}
    if full_path ~= "" and not is_url(full_path) then
        local dir = utils.split_path(full_path)
        parent_folders = split_path_components(dir)
    end

    return {
        full_path = full_path,
        filename = filename,
        filename_no_ext = basename,
        media_title = media_title,
        playback_time = playback_time,
        parent_folders = parent_folders,
        is_stream = is_url(full_path),
    }
end

local function parse_from_folders(info)
    local folders = info.parent_folders or {}
    local season
    local episode
    local show_title
    local season_index

    for i = #folders, 1, -1 do
        local lowered = normalize_separators(folders[i]):lower()
        if not season then
            season = lowered:match("season%s*(%d%d?)")
        end
        if not episode then
            episode = lowered:match("episode%s*(%d%d?)") or lowered:match("ep%s*(%d%d?)")
        end
        if season and not season_index and lowered:match("season%s*%d") then
            season_index = i
        end
    end

    if season_index and season_index > 1 then
        show_title = folders[season_index - 1]
    elseif #folders > 0 then
        show_title = folders[#folders]
    end

    return {
        show_title = show_title and sanitize_name(strip_release_junk(show_title)) or nil,
        season = season,
        episode = episode,
        confidence = (season and episode) and "medium" or ((season or episode) and "low" or nil),
        score = (season and episode) and 70 or ((season or episode) and 50 or 0),
        matched_pattern = (season and episode) and "folder-season-episode"
            or ((season or episode) and "folder-partial" or nil),
        evidence = "parent_folders",
    }
end

local function parse_show_info(info)
    if info.filename_no_ext and info.filename_no_ext ~= "" then
        local explicit = parse_explicit_sxe_from_text(info.filename_no_ext)
        if explicit then
            return explicit
        end
    end

    local base = info.filename_no_ext ~= "" and info.filename_no_ext or info.media_title
    local normalized = normalize_separators(base)
    local lowered = normalized:lower()
    local folder_parse = parse_from_folders(info)

    local best = {
        score = 0,
        confidence = "low",
        evidence = "filename",
    }
    local explicit_sxe_found = false

    local function candidate(score, confidence, pattern, season, episode, title_start, title_end, date)
        if not is_plausible_season_episode(season, episode, pattern) then
            debug_log("reject implausible " .. pattern .. " season=" .. tostring(season) .. " episode=" .. tostring(episode))
            return
        end

        local show_title
        local episode_title

        if title_start and title_start > 1 then
            show_title = normalize_spaces(normalized:sub(1, title_start - 1))
        end

        if title_end and title_end < #normalized then
            episode_title = parse_episode_title(normalized:sub(title_end + 1))
        end

        show_title = show_title and strip_release_junk(show_title) or nil
        show_title = show_title and sanitize_name(show_title) or nil

        if show_title and show_title == "Unknown" then
            show_title = nil
        end

        if not show_title and folder_parse.show_title then
            show_title = folder_parse.show_title
        end

        if not show_title and info.media_title and info.media_title ~= "" then
            local media_title = sanitize_name(strip_release_junk(info.media_title))
            if media_title ~= "Unknown" then
                show_title = media_title
            end
        end

        if not show_title then
            local fallback = sanitize_name(strip_release_junk(normalized))
            show_title = fallback ~= "Unknown" and fallback or nil
        end

        local clean_title, year = extract_year_metadata(show_title or "")
        if clean_title ~= "" then
            show_title = cleanup_series_title(clean_title)
        end

        local result = {
            show_title = show_title,
            season = season,
            episode = episode,
            episode_title = episode_title,
            date = date,
            year = year,
            confidence = confidence,
            score = score,
            matched_pattern = pattern,
            evidence = "filename",
        }

        if folder_parse.season and not result.season then
            result.season = folder_parse.season
        end
        if folder_parse.episode and not result.episode then
            result.episode = folder_parse.episode
        end

        if result.score > best.score then
            best = result
        end
    end

    local s, e, empty_idx, season, episode
    s, e, empty_idx, season, episode = lowered:find("()s(%d%d?)%s*e(%d%d?)%s*[%-%+]%s*e?%d%d?")
    if s then
        candidate(100, "high", "sxe-range", season, episode, s, e, parse_date(base))
        explicit_sxe_found = true
    end

    s, e, empty_idx, season, episode = lowered:find("()s(%d%d?)%s*e(%d%d?)%s*e%d%d?")
    if s then
        candidate(100, "high", "sxe-multi", season, episode, s, e, parse_date(base))
        explicit_sxe_found = true
    end

    s, e, empty_idx, season, episode = lowered:find("()s(%d%d?)%s*e(%d%d?)")
    if s then
        candidate(100, "high", "sxe", season, episode, s, e, parse_date(base))
        explicit_sxe_found = true
    end

    if not explicit_sxe_found then
        s, e, empty_idx, season, episode = lowered:find("()%f[%d](%d%d?)x(%d%d?)%f[%D]")
        if s then
            candidate(90, "high", "x-style", season, episode, s, e, parse_date(base))
        end

        s, e, empty_idx, season, episode = lowered:find("()season%s*(%d%d?).-episode%s*(%d%d?)")
        if s then
            candidate(95, "high", "season-episode-text", season, episode, s, e, parse_date(base))
        end

        s, e, empty_idx, season, episode = lowered:find("()season%s*(%d%d?).-ep%s*(%d%d?)")
        if s then
            candidate(92, "high", "season-ep-text", season, episode, s, e, parse_date(base))
        end

        local ep_start, ep_end, empty_idx, episode_only
        ep_start, ep_end, empty_idx, episode_only = lowered:find("()episode%s*(%d%d?)")
        if ep_start then
            local score = folder_parse.season and 78 or 55
            local confidence = folder_parse.season and "medium" or "low"
            candidate(score, confidence, "episode-only", nil, episode_only, ep_start, ep_end, parse_date(base))
        end

        ep_start, ep_end, empty_idx, episode_only = lowered:find("()ep%s*(%d%d?)")
        if ep_start then
            local score = folder_parse.season and 76 or 52
            local confidence = folder_parse.season and "medium" or "low"
            candidate(score, confidence, "ep-only", nil, episode_only, ep_start, ep_end, parse_date(base))
        end
    end

    if folder_parse.score > best.score then
        best = folder_parse
    end

    local date = parse_date(base)
    if date and best.score < 65 then
        local date_start = lowered:find("20%d%d[%s%._%-]%d%d[%s%._%-]%d%d") or 0
        candidate(65, "medium", "date-based", nil, nil, date_start, date_start + 10, date)
    end

    if best.score <= 0 then
        return nil
    end

    if best.season then
        best.season = pad2(best.season)
    end
    if best.episode then
        best.episode = pad2(best.episode)
    end

    if best.show_title then
        best.show_title = cleanup_series_title(best.show_title)
    end

    return best
end

local function parse_movie_info(info)
    local base = info.filename_no_ext ~= "" and info.filename_no_ext or info.media_title
    local normalized = normalize_separators(base)
    local lowered = normalized:lower()

    if lowered:find("s%d%d?%s*e%d%d?") or has_x_style_episode_marker(lowered)
        or lowered:find("season%s*%d") then
        return {
            score = 0,
            confidence = "low",
        }
    end

    local cleaned = strip_release_junk(normalized)
    local year
    cleaned, year = extract_year_metadata(cleaned)
    cleaned = sanitize_name(cleaned)

    if cleaned == "Unknown" then
        cleaned = sanitize_name(strip_release_junk(info.media_title or ""))
    end

    if cleaned == "Unknown" then
        return nil
    end

    return {
        title = cleaned,
        year = year,
        confidence = "medium",
        score = 80,
        matched_pattern = "movie-default",
    }
end

local function classify_media(info)
    local show = parse_show_info(info)
    local movie = parse_movie_info(info)

    local show_score = show and show.score or 0
    local movie_score = movie and movie.score or 0

    if show_score >= 90 then
        show.classification = "show"
        return show
    end

    if show_score >= 70 and movie_score < 70 then
        show.classification = "show"
        return show
    end

    if movie_score >= 70 and show_score < 70 then
        movie.classification = "movie"
        return movie
    end

    if show_score > 0 and movie_score > 0 and math.abs(show_score - movie_score) <= 15 then
        return {
            classification = "unknown",
            confidence = "low",
            fallback_reason = "conflicting-evidence",
            raw_name = sanitize_name(strip_release_junk(info.filename_no_ext ~= "" and info.filename_no_ext or info.media_title)),
        }
    end

    if show_score > 0 then
        return {
            classification = "unknown",
            confidence = show.confidence,
            fallback_reason = "show-low-confidence",
            show_title = show.show_title,
            season = show.season,
            episode = show.episode,
            date = show.date,
            raw_name = sanitize_name(strip_release_junk(info.filename_no_ext ~= "" and info.filename_no_ext or info.media_title)),
        }
    end

    if movie then
        movie.classification = "movie"
        return movie
    end

    return {
        classification = "unknown",
        confidence = "low",
        fallback_reason = "no-parse",
        raw_name = sanitize_name(strip_release_junk(info.filename_no_ext ~= "" and info.filename_no_ext or info.media_title)),
    }
end

local function make_relative_path(path, base)
    local normalized_path = (path or ""):gsub("\\", "/")
    local normalized_base = (base or ""):gsub("\\", "/")
    if normalized_base ~= "" and normalized_path:sub(1, #normalized_base) == normalized_base then
        local rel = normalized_path:sub(#normalized_base + 1)
        rel = rel:gsub("^/", "")
        if rel ~= "" then
            return rel
        end
    end
    return normalized_path
end

local function ensure_dir(path)
    local info = utils.file_info(path)
    if info and info.is_dir then
        debug_log("ensure_dir exists: " .. path)
        return true
    end

    local is_windows = package.config:sub(1, 1) == "\\"
    local cmd
    if is_windows then
        cmd = {
            _name = "subprocess",
            playback_only = false,
            capture_stdout = true,
            capture_stderr = true,
            args = {
                "cmd",
                "/d",
                "/c",
                "mkdir",
                path,
            },
        }
    else
        cmd = {
            _name = "subprocess",
            playback_only = false,
            args = {"mkdir", "-p", path},
        }
    end

    local result = mp.command_native(cmd)
    if result then
        debug_log("ensure_dir status=" .. tostring(result.status) .. " path=" .. path)
    else
        debug_log("ensure_dir subprocess returned nil for path=" .. path)
    end
    if result and result.status == 0 then
        return true
    end

    local retry = utils.file_info(path)
    return retry and retry.is_dir or false
end

local function build_output_path(parsed)
    local base_output = mp.command_native({"expand-path", o.base_output_dir})
    base_output = mp.command_native({"normalize-path", base_output})

    local folder
    if parsed.classification == "movie" then
        local movie_title = parsed.title or "Unknown Movie"
        if o.include_year_in_movie_folder and parsed.year then
            movie_title = movie_title .. " (" .. parsed.year .. ")"
        end
        folder = utils.join_path(base_output, sanitize_name(o.movies_subdir))
        folder = utils.join_path(folder, sanitize_name(movie_title))
    elseif parsed.classification == "show" then
        folder = utils.join_path(base_output, sanitize_name(o.shows_subdir))
        folder = utils.join_path(folder, sanitize_name(parsed.show_title or "Unknown Show"))

        if parsed.season and parsed.episode then
            folder = utils.join_path(folder, "Season " .. parsed.season)
            folder = utils.join_path(folder, "Episode " .. parsed.episode)
        elseif parsed.season then
            folder = utils.join_path(folder, "Season " .. parsed.season)
            folder = utils.join_path(folder, "Episode Unknown")
        elseif parsed.date and o.support_daily_show_folders then
            folder = utils.join_path(folder, "By Date")
            folder = utils.join_path(folder, parsed.date)
        else
            folder = utils.join_path(base_output, sanitize_name(o.fallback_subdir))
            folder = utils.join_path(folder, sanitize_name(parsed.show_title or "Unknown Show"))
        end
    else
        local fallback_name = sanitize_name(parsed.raw_name or "Unknown")
        folder = utils.join_path(base_output, sanitize_name(o.fallback_subdir))
        folder = utils.join_path(folder, fallback_name)
    end

    return folder, base_output
end

local function format_playback_timestamp(seconds)
    local ms_total = math.floor((seconds or 0) * 1000 + 0.5)
    local hours = math.floor(ms_total / 3600000)
    local minutes = math.floor((ms_total % 3600000) / 60000)
    local secs = math.floor((ms_total % 60000) / 1000)
    local ms = ms_total % 1000
    return string.format("%02d-%02d-%02d-%03d", hours, minutes, secs, ms)
end

local function build_screenshot_filename(parsed, playback_time)
    local ts = format_playback_timestamp(playback_time)
    local ext = trim(o.image_format):lower()
    if ext == "" then
        ext = "png"
    end

    local stem
    if parsed.classification == "movie" then
        stem = string.format("%s - %s", parsed.title or "Movie", ts)
    elseif parsed.classification == "show" then
        if parsed.season and parsed.episode then
            stem = string.format("%s - S%sE%s - %s", parsed.show_title or "Show", parsed.season, parsed.episode, ts)
        elseif parsed.date then
            stem = string.format("%s - %s - %s", parsed.show_title or "Show", parsed.date, ts)
        else
            stem = string.format("%s - %s", parsed.show_title or "Show", ts)
        end
    else
        stem = string.format("%s - %s", parsed.raw_name or "Screenshot", ts)
    end

    stem = sanitize_name(stem)
    return stem .. "." .. ext
end

local function with_collision_suffix(dir, file_name)
    local stem, ext = file_name:match("^(.*)%.([^.]+)$")
    if not stem then
        stem = file_name
        ext = "png"
    end

    local candidate = utils.join_path(dir, file_name)
    if not utils.file_info(candidate) then
        return candidate
    end

    local counter = 1
    while true do
        local next_name = string.format("%s - %d.%s", stem, counter, ext)
        local next_path = utils.join_path(dir, next_name)
        if not utils.file_info(next_path) then
            return next_path
        end
        counter = counter + 1
    end
end

local function debug_log_parse_result(result)
    if not o.save_debug_log then
        return
    end
    local msg = string.format(
        "class=%s pattern=%s title=%s season=%s episode=%s date=%s confidence=%s fallback=%s",
        result.classification or "n/a",
        result.matched_pattern or "n/a",
        result.show_title or result.title or result.raw_name or "n/a",
        result.season or "n/a",
        result.episode or "n/a",
        result.date or "n/a",
        result.confidence or "n/a",
        result.fallback_reason or "n/a"
    )
    debug_log(msg)
end

local function save_screenshot(target_path)
    local ok, ret = pcall(mp.command_native, {"screenshot-to-file", target_path, "video"})
    if not ok then
        return false, ret
    end

    local saved = utils.file_info(target_path)
    if saved and saved.is_file then
        return true, ret
    end

    return false, ret
end

local function take_screenshot(source)
    local trigger_source = source or "hotkey"
    debug_trigger_message(trigger_source)

    local info = get_current_media_info()
    local parsed = classify_media(info)
    debug_log_parse_result(parsed)

    local target_dir, base_output = build_output_path(parsed)
    debug_log("base_output=" .. tostring(base_output))
    debug_log("target_dir(initial)=" .. tostring(target_dir))

    if o.debug_trigger_feedback then
        local title = parsed.show_title or parsed.title or parsed.raw_name or "Unknown"
        local se = ""
        if parsed.season and parsed.episode then
            se = " S" .. parsed.season .. "E" .. parsed.episode
        end
        notify_user("debug: " .. (parsed.classification or "unknown") .. " | " .. title .. se .. " [" .. (parsed.matched_pattern or "n/a") .. "]")
    end

    if not ensure_dir(target_dir) then
        debug_log("target_dir create failed, trying fallback root")
        local fallback_root = mp.command_native({"expand-path", o.base_output_dir})
        fallback_root = mp.command_native({"normalize-path", fallback_root})
        target_dir = utils.join_path(fallback_root, sanitize_name(o.fallback_subdir))
        if not ensure_dir(target_dir) then
            notify_user("Failed to create output directory")
            mp.msg.error("Failed to create output directory: " .. tostring(target_dir))
            return
        end
    end

    local file_name = build_screenshot_filename(parsed, info.playback_time)
    local final_path = with_collision_suffix(target_dir, file_name)
    debug_log("final_path=" .. tostring(final_path))
    local ok, cmd_result = save_screenshot(final_path)

    if ok then
        local rel = make_relative_path(final_path, base_output)
        notify_user("Screenshot saved: " .. rel)
        debug_log("saved => " .. final_path)
    else
        notify_user("Failed to save screenshot")
        mp.msg.error("Failed to save screenshot to: " .. final_path)
        debug_log("screenshot command result=" .. utils.to_string(cmd_result))
    end
end

local function register_hotkey()
    mp.remove_key_binding(show_binding_name)
    mp.remove_key_binding(fallback_binding_name)
    mp.remove_key_binding("screenshot-folder-debug-trigger")

    local primary_key = trim(o.screenshot_key)
    if primary_key ~= "" then
        mp.add_forced_key_binding(primary_key, show_binding_name, function()
            take_screenshot("primary-key")
        end)
        debug_log("registered primary key: " .. primary_key)
    end

    local fallback_key = trim(o.fallback_screenshot_key)
    if fallback_key ~= "" and fallback_key:lower() ~= primary_key:lower() then
        mp.add_forced_key_binding(fallback_key, fallback_binding_name, function()
            take_screenshot("fallback-key")
        end)
        debug_log("registered fallback key: " .. fallback_key)
    end

    local debug_key = trim(o.debug_trigger_key)
    if debug_key ~= "" and debug_key:lower() ~= primary_key:lower() and debug_key:lower() ~= fallback_key:lower() then
        mp.add_forced_key_binding(debug_key, "screenshot-folder-debug-trigger", function()
            debug_trigger_message("debug-key")
            if o.debug_trigger_captures_screenshot then
                take_screenshot("debug-key")
            end
        end)
        debug_log("registered debug key: " .. debug_key)
    end
end

options.read_options(o, "screenshot-folder", function()
    register_hotkey()
end)

mp.register_script_message("screenshot-folder-trigger", function()
    debug_trigger_message("script-message")
    take_screenshot("script-message")
end)

mp.register_script_message("screenshot-folder-debug", function()
    debug_trigger_message("script-message-debug")
end)

register_hotkey()
debug_log("script loaded")
