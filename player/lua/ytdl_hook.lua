local utils = require 'mp.utils'
local msg = require 'mp.msg'
local options = require 'mp.options'

local o = {
    exclude = "",
    try_ytdl_first = false,
    use_manifests = false
}
options.read_options(o)

local ytdl = {
    path = "youtube-dl",
    searched = false,
    blacklisted = {}
}

local chapter_list = {}

function Set (t)
    local set = {}
    for _, v in pairs(t) do set[v] = true end
    return set
end

local safe_protos = Set {
    "http", "https", "ftp", "ftps",
    "rtmp", "rtmps", "rtmpe", "rtmpt", "rtmpts", "rtmpte",
    "data"
}

local function exec(args)
    local ret = utils.subprocess({args = args})
    return ret.status, ret.stdout, ret, ret.killed_by_us
end

-- return true if it was explicitly set on the command line
local function option_was_set(name)
    return mp.get_property_bool("option-info/" ..name.. "/set-from-commandline",
                                false)
end

-- return true if the option was set locally
local function option_was_set_locally(name)
    return mp.get_property_bool("option-info/" ..name.. "/set-locally", false)
end

-- youtube-dl may set special http headers for some sites (user-agent, cookies)
local function set_http_headers(http_headers)
    if not http_headers then
        return
    end
    local headers = {}
    local useragent = http_headers["User-Agent"]
    if useragent and not option_was_set("user-agent") then
        mp.set_property("file-local-options/user-agent", useragent)
    end
    local additional_fields = {"Cookie", "Referer", "X-Forwarded-For"}
    for idx, item in pairs(additional_fields) do
        local field_value = http_headers[item]
        if field_value then
            headers[#headers + 1] = item .. ": " .. field_value
        end
    end
    if #headers > 0 and not option_was_set("http-header-fields") then
        mp.set_property_native("file-local-options/http-header-fields", headers)
    end
end

local function append_libav_opt(props, name, value)
    if not props then
        props = {}
    end

    if name and value and not props[name] then
        props[name] = value
    end

    return props
end

local function edl_escape(url)
    return "%" .. string.len(url) .. "%" .. url
end

local function url_is_safe(url)
    local proto = type(url) == "string" and url:match("^(.+)://") or nil
    local safe = proto and safe_protos[proto]
    if not safe then
        msg.error(("Ignoring potentially unsafe url: '%s'"):format(url))
    end
    return safe
end

local function time_to_secs(time_string)
    local ret

    local a, b, c = time_string:match("(%d+):(%d%d?):(%d%d)")
    if a ~= nil then
        ret = (a*3600 + b*60 + c)
    else
        a, b = time_string:match("(%d%d?):(%d%d)")
        if a ~= nil then
            ret = (a*60 + b)
        end
    end

    return ret
end

local function extract_chapters(data, video_length)
    local ret = {}

    for line in data:gmatch("[^\r\n]+") do
        local time = time_to_secs(line)
        if time and (time < video_length) then
            table.insert(ret, {time = time, title = line})
        end
    end
    table.sort(ret, function(a, b) return a.time < b.time end)
    return ret
end

local function is_blacklisted(url)
    if o.exclude == "" then return false end
    if #ytdl.blacklisted == 0 then
        local joined = o.exclude
        while joined:match('%|?[^|]+') do
            local _, e, substring = joined:find('%|?([^|]+)')
            table.insert(ytdl.blacklisted, substring)
            joined = joined:sub(e+1)
        end
    end
    if #ytdl.blacklisted > 0 then
        url = url:match('https?://(.+)')
        for _, exclude in ipairs(ytdl.blacklisted) do
            if url:match(exclude) then
                msg.verbose('URL matches excluded substring. Skipping.')
                return true
            end
        end
    end
    return false
end

local function parse_yt_playlist(url, json)
    -- return 0-based index to use with --playlist-start

    if not json.extractor or json.extractor ~= "youtube:playlist" then
        return nil
    end

    local query = url:match("%?.+")
    if not query then return nil end

    local args = {}
    for arg, param in query:gmatch("(%a+)=([^&?]+)") do
        if arg and param then
            args[arg] = param
        end
    end

    local maybe_idx = tonumber(args["index"])

    -- if index matches v param it's probably the requested item
    if maybe_idx and #json.entries >= maybe_idx and
        json.entries[maybe_idx].id == args["v"] then
        msg.debug("index matches requested video")
        return maybe_idx - 1
    end

    -- if there's no index or it doesn't match, look for video
    for i = 1, #json.entries do
        if json.entries[i] == args["v"] then
            msg.debug("found requested video in index " .. (i - 1))
            return i - 1
        end
    end

    msg.debug("requested video not found in playlist")
    -- if item isn't on the playlist, give up
    return nil
end

local function make_absolute_url(base_url, url)
    if url:find("https?://") == 1 then return url end

    local proto, domain, rest =
        base_url:match("(https?://)([^/]+/)(.*)/?")
    local segs = {}
    rest:gsub("([^/]+)", function(c) table.insert(segs, c) end)
    url:gsub("([^/]+)", function(c) table.insert(segs, c) end)
    local resolved_url = {}
    for i, v in ipairs(segs) do
        if v == ".." then
            table.remove(resolved_url)
        elseif v ~= "." then
            table.insert(resolved_url, v)
        end
    end
    return proto .. domain ..
        table.concat(resolved_url, "/")
end

local function join_url(base_url, fragment)
    local res = ""
    if base_url and fragment.path then
        res = make_absolute_url(base_url, fragment.path)
    elseif fragment.url then
        res = fragment.url
    end
    return res
end

local function edl_track_joined(fragments, protocol, is_live, base)
    if not (type(fragments) == "table") or not fragments[1] then
        msg.debug("No fragments to join into EDL")
        return nil
    end

    local edl = "edl://"
    local offset = 1
    local parts = {}

    if (protocol == "http_dash_segments") and
        not fragments[1].duration and not is_live then
        -- assume MP4 DASH initialization segment
        table.insert(parts,
            "!mp4_dash,init=" .. edl_escape(join_url(base, fragments[1])))
        offset = 2

        -- Check remaining fragments for duration;
        -- if not available in all, give up.
        for i = offset, #fragments do
            if not fragments[i].duration then
                msg.error("EDL doesn't support fragments" ..
                         "without duration with MP4 DASH")
                return nil
            end
        end
    end

    for i = offset, #fragments do
        local fragment = fragments[i]
        if not url_is_safe(join_url(base, fragment)) then
            return nil
        end
        table.insert(parts, edl_escape(join_url(base, fragment)))
        if fragment.duration then
            parts[#parts] =
                parts[#parts] .. ",length="..fragment.duration
        end
    end
    return edl .. table.concat(parts, ";") .. ";"
end

local function has_native_dash_demuxer()
    local demuxers = mp.get_property_native("demuxer-lavf-list", {})
    for _, v in ipairs(demuxers) do
        if v == "dash" then
            return true
        end
    end
    return false
end

local function valid_manifest(json)
    local reqfmt = json["requested_formats"] and json["requested_formats"][1] or {}
    if not reqfmt["manifest_url"] and not json["manifest_url"] then
        return false
    end
    local proto = reqfmt["protocol"] or json["protocol"] or ""
    return (proto == "http_dash_segments" and has_native_dash_demuxer()) or
        proto:find("^m3u8")
end

local function add_single_video(json)
    local streamurl = ""
    local max_bitrate = 0
    local reqfmts = json["requested_formats"]

    -- prefer manifest_url if present
    if o.use_manifests and valid_manifest(json) then
        local mpd_url = reqfmts and reqfmts[1]["manifest_url"] or
            json["manifest_url"]
        if not mpd_url then
            msg.error("No manifest URL found in JSON data.")
            return
        elseif not url_is_safe(mpd_url) then
            return
        end

        streamurl = mpd_url

        if reqfmts then
            for _, track in pairs(reqfmts) do
                max_bitrate = track.tbr > max_bitrate and
                    track.tbr or max_bitrate
            end
        elseif json.tbr then
            max_bitrate = json.tbr > max_bitrate and json.tbr or max_bitrate
        end

    -- DASH/split tracks
    elseif reqfmts then
        for _, track in pairs(reqfmts) do
            local edl_track = nil
            edl_track = edl_track_joined(track.fragments,
                track.protocol, json.is_live,
                track.fragment_base_url)
            if not edl_track and not url_is_safe(track.url) then
                return
            end
            if track.vcodec and track.vcodec ~= "none" then
                -- video track
                streamurl = edl_track or track.url
            elseif track.vcodec == "none" then
                -- according to ytdl, if vcodec is None, it's audio
                mp.commandv("audio-add",
                    edl_track or track.url, "auto",
                    track.format_note or "")
            end
        end

    elseif not (json.url == nil) then
        local edl_track = nil
        edl_track = edl_track_joined(json.fragments, json.protocol,
            json.is_live, json.fragment_base_url)

        if not edl_track and not url_is_safe(json.url) then
            return
        end
        -- normal video or single track
        streamurl = edl_track or json.url
        set_http_headers(json.http_headers)
    else
        msg.error("No URL found in JSON data.")
        return
    end

    msg.debug("streamurl: " .. streamurl)

    mp.set_property("stream-open-filename", streamurl:gsub("^data:", "data://", 1))

    mp.set_property("file-local-options/force-media-title", json.title)

    -- set hls-bitrate for dash track selection
    if max_bitrate > 0 and
        not option_was_set("hls-bitrate") and
        not option_was_set_locally("hls-bitrate") then
        mp.set_property_native('file-local-options/hls-bitrate', max_bitrate*1000)
    end

    -- add subtitles
    if not (json.requested_subtitles == nil) then
        for lang, sub_info in pairs(json.requested_subtitles) do
            msg.verbose("adding subtitle ["..lang.."]")

            local sub = nil

            if not (sub_info.data == nil) then
                sub = "memory://"..sub_info.data
            elseif not (sub_info.url == nil) and
                url_is_safe(sub_info.url) then
                sub = sub_info.url
            end

            if not (sub == nil) then
                mp.commandv("sub-add", sub,
                    "auto", sub_info.ext, lang)
            else
                msg.verbose("No subtitle data/url for ["..lang.."]")
            end
        end
    end

    -- add chapters
    if json.chapters then
        msg.debug("Adding pre-parsed chapters")
        for i = 1, #json.chapters do
            local chapter = json.chapters[i]
            local title = chapter.title or ""
            if title == "" then
                title = string.format('Chapter %02d', i)
            end
            table.insert(chapter_list, {time=chapter.start_time, title=title})
        end
    elseif not (json.description == nil) and not (json.duration == nil) then
        chapter_list = extract_chapters(json.description, json.duration)
    end

    -- set start time
    if not (json.start_time == nil) and
        not option_was_set("start") and
        not option_was_set_locally("start") then
        msg.debug("Setting start to: " .. json.start_time .. " secs")
        mp.set_property("file-local-options/start", json.start_time)
    end

    -- set aspect ratio for anamorphic video
    if not (json.stretched_ratio == nil) and
        not option_was_set("video-aspect") then
        mp.set_property('file-local-options/video-aspect', json.stretched_ratio)
    end

    local stream_opts = mp.get_property_native("file-local-options/stream-lavf-o", {})

    -- for rtmp
    if (json.protocol == "rtmp") then
        stream_opts = append_libav_opt(stream_opts,
            "rtmp_tcurl", streamurl)
        stream_opts = append_libav_opt(stream_opts,
            "rtmp_pageurl", json.page_url)
        stream_opts = append_libav_opt(stream_opts,
            "rtmp_playpath", json.play_path)
        stream_opts = append_libav_opt(stream_opts,
            "rtmp_swfverify", json.player_url)
        stream_opts = append_libav_opt(stream_opts,
            "rtmp_swfurl", json.player_url)
        stream_opts = append_libav_opt(stream_opts,
            "rtmp_app", json.app)
    end

    if json.proxy and json.proxy ~= "" then
        stream_opts = append_libav_opt(stream_opts,
            "http_proxy", json.proxy)
    end

    mp.set_property_native("file-local-options/stream-lavf-o", stream_opts)
end

function run_ytdl_hook(url)
    local start_time = os.clock()

    -- check for youtube-dl in mpv's config dir
    if not (ytdl.searched) then
        local exesuf = (package.config:sub(1,1) == '\\') and '.exe' or ''
        local ytdl_mcd = mp.find_config_file("youtube-dl" .. exesuf)
        if not (ytdl_mcd == nil) then
            msg.verbose("found youtube-dl at: " .. ytdl_mcd)
            ytdl.path = ytdl_mcd
        end
        ytdl.searched = true
    end

    -- strip ytdl://
    if (url:find("ytdl://") == 1) then
        url = url:sub(8)
    end

    local format = mp.get_property("options/ytdl-format")
    local raw_options = mp.get_property_native("options/ytdl-raw-options")
    local allsubs = true
    local proxy = nil
    local use_playlist = false

    local command = {
        ytdl.path, "--no-warnings", "-J", "--flat-playlist",
        "--sub-format", "ass/srt/best"
    }

    -- Checks if video option is "no", change format accordingly,
    -- but only if user didn't explicitly set one
    if (mp.get_property("options/vid") == "no")
        and not option_was_set("ytdl-format") then

        format = "bestaudio/best"
        msg.verbose("Video disabled. Only using audio")
    end

    if (format == "") then
        format = "bestvideo+bestaudio/best"
    end
    table.insert(command, "--format")
    table.insert(command, format)

    for param, arg in pairs(raw_options) do
        table.insert(command, "--" .. param)
        if (arg ~= "") then
            table.insert(command, arg)
        end
        if (param == "sub-lang") and (arg ~= "") then
            allsubs = false
        elseif (param == "proxy") and (arg ~= "") then
            proxy = arg
        elseif (param == "yes-playlist") then
            use_playlist = true
        end
    end

    if (allsubs == true) then
        table.insert(command, "--all-subs")
    end
    if not use_playlist then
        table.insert(command, "--no-playlist")
    end
    table.insert(command, "--")
    table.insert(command, url)
    msg.debug("Running: " .. table.concat(command,' '))
    local es, json, result, aborted = exec(command)

    if aborted then
        return
    end

    if (es < 0) or (json == nil) or (json == "") then
        local err = "youtube-dl failed: "
        if result.error and result.error == "init" then
            err = err .. "not found or not enough permissions"
        elseif not result.killed_by_us then
            err = err .. "unexpected error ocurred"
        else
            err = string.format("%s returned '%d'", err, es)
        end
        msg.error(err)
        return
    end

    local json, err = utils.parse_json(json)

    if (json == nil) then
        msg.error("failed to parse JSON data: " .. err)
        return
    end

    msg.verbose("youtube-dl succeeded!")
    msg.debug('ytdl parsing took '..os.clock()-start_time..' seconds')

    json["proxy"] = json["proxy"] or proxy

    -- what did we get?
    if json["direct"] then
        -- direct URL, nothing to do
        msg.verbose("Got direct URL")
        return
    elseif (json["_type"] == "playlist")
        or (json["_type"] == "multi_video") then
        -- a playlist

        if (#json.entries == 0) then
            msg.warn("Got empty playlist, nothing to play.")
            return
        end

        local self_redirecting_url =
            json.entries[1]["_type"] ~= "url_transparent" and
            json.entries[1]["webpage_url"] and
            json.entries[1]["webpage_url"] == json["webpage_url"]


        -- some funky guessing to detect multi-arc videos
        if self_redirecting_url and #json.entries > 1
            and json.entries[1].protocol == "m3u8_native"
            and json.entries[1].url then
            msg.verbose("multi-arc video detected, building EDL")

            local playlist = edl_track_joined(json.entries)

            msg.debug("EDL: " .. playlist)

            if not playlist then
                return
            end

            -- can't change the http headers for each entry, so use the 1st
            set_http_headers(json.entries[1].http_headers)

            mp.set_property("stream-open-filename", playlist)
            if not (json.title == nil) then
                mp.set_property("file-local-options/force-media-title",
                    json.title)
            end

            -- there might not be subs for the first segment
            local entry_wsubs = nil
            for i, entry in pairs(json.entries) do
                if not (entry.requested_subtitles == nil) then
                    entry_wsubs = i
                    break
                end
            end

            if not (entry_wsubs == nil) and
                not (json.entries[entry_wsubs].duration == nil) then
                for j, req in pairs(json.entries[entry_wsubs].requested_subtitles) do
                    local subfile = "edl://"
                    for i, entry in pairs(json.entries) do
                        if not (entry.requested_subtitles == nil) and
                            not (entry.requested_subtitles[j] == nil) and
                            url_is_safe(entry.requested_subtitles[j].url) then
                            subfile = subfile..edl_escape(entry.requested_subtitles[j].url)
                        else
                            subfile = subfile..edl_escape("memory://WEBVTT")
                        end
                        subfile = subfile..",length="..entry.duration..";"
                    end
                    msg.debug(j.." sub EDL: "..subfile)
                    mp.commandv("sub-add", subfile, "auto", req.ext, j)
                end
            end

        elseif self_redirecting_url and #json.entries == 1 then
            msg.verbose("Playlist with single entry detected.")
            add_single_video(json.entries[1])
        else
            local playlist_index = parse_yt_playlist(url, json)
            local playlist = {"#EXTM3U"}
            for i, entry in pairs(json.entries) do
                local site = entry.url
                local title = entry.title

                if not (title == nil) then
                    title = string.gsub(title, '%s+', ' ')
                    table.insert(playlist, "#EXTINF:0," .. title)
                end

                --[[ some extractors will still return the full info for
                     all clips in the playlist and the URL will point
                     directly to the file in that case, which we don't
                     want so get the webpage URL instead, which is what
                     we want, but only if we aren't going to trigger an
                     infinite loop
                --]]
                if entry["webpage_url"] and not self_redirecting_url then
                    site = entry["webpage_url"]
                end

                -- links without protocol as returned by --flat-playlist
                if not site:find("://") then
                    -- youtube extractor provides only IDs,
                    -- others come prefixed with the extractor name and ":"
                    local prefix = site:find(":") and "ytdl://" or
                        "https://youtu.be/"
                    table.insert(playlist, prefix .. site)
                elseif url_is_safe(site) then
                    table.insert(playlist, site)
                end

            end

            if use_playlist and
                not option_was_set("playlist-start") and playlist_index then
                mp.set_property_number("playlist-start", playlist_index)
            end

            mp.set_property("stream-open-filename", "memory://" .. table.concat(playlist, "\n"))

            -- This disables mpv's mushy playlist security code, which will
            -- break links that will be resolved to EDL later (because EDL is
            -- not considered "safe", and the playlist entries got tagged as
            -- network originating due to the playlist redirection).
            mp.set_property_native("file-local-options/load-unsafe-playlists", true)
        end

    else -- probably a video
        add_single_video(json)
    end
    msg.debug('script running time: '..os.clock()-start_time..' seconds')
end

if (not o.try_ytdl_first) then
    mp.add_hook("on_load", 10, function ()
        msg.verbose('ytdl:// hook')
        local url = mp.get_property("stream-open-filename", "")
        if not (url:find("ytdl://") == 1) then
            msg.verbose('not a ytdl:// url')
            return
        end
        run_ytdl_hook(url)
    end)
end

mp.add_hook(o.try_ytdl_first and "on_load" or "on_load_fail", 10, function()
    msg.verbose('full hook')
    local url = mp.get_property("stream-open-filename", "")
    if not (url:find("ytdl://") == 1) and
        not ((url:find("https?://") == 1) and not is_blacklisted(url)) then
        return
    end
    run_ytdl_hook(url)
end)

mp.add_hook("on_preloaded", 10, function ()
    if next(chapter_list) ~= nil then
        msg.verbose("Setting chapters")

        mp.set_property_native("chapter-list", chapter_list)
        chapter_list = {}
    end
end)
