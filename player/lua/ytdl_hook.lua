local utils = require 'mp.utils'
local msg = require 'mp.msg'

local ytdl = {
    path = "youtube-dl",
    minver = "2015.02.23.1",
    vercheck = nil,
}

local function exec(args)
    local ret = utils.subprocess({args = args})
    return ret.status, ret.stdout
end

-- return if it was explicitly set on the command line
local function option_was_set(name)
    return mp.get_property_bool("option-info/" .. name .. "/set-from-commandline",
                                false)
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
    local cookies = http_headers["Cookie"]
    if cookies then
        headers[#headers + 1] = "Cookie: " .. cookies
    end
    if #headers > 0 and not option_was_set("http-header-fields") then
        mp.set_property_native("file-local-options/http-header-fields", headers)
    end
end

local function append_rtmp_prop(props, name, value)
    if not name or not value then
        return props
    end

    if props and props ~= "" then
        props = props..","
    else
        props = ""
    end

    return props..name.."=\""..value.."\""
end

mp.add_hook("on_load", 10, function ()
    local url = mp.get_property("stream-open-filename")

    if (url:find("http://") == 1) or (url:find("https://") == 1)
        or (url:find("ytdl://") == 1) then

       -- check version of youtube-dl if not done yet
        if (ytdl.vercheck == nil) then

             -- check for youtube-dl in mpv's config dir
            local ytdl_mcd = mp.find_config_file("youtube-dl")
            if not (ytdl_mcd == nil) then
                msg.verbose("found youtube-dl at: " .. ytdl_mcd)
                ytdl.path = ytdl_mcd
            end

            msg.debug("checking ytdl version ...")
            local es, version = exec({ytdl.path, "--version"})
            if (es < 0) then
                msg.warn("youtube-dl not found, not executable, or broken.")
                ytdl.vercheck = false
            elseif (version < ytdl.minver) then
                msg.verbose("found youtube-dl version: " .. version)
                msg.warn("Your version of youtube-dl is too old! "
                    .. "You need at least version '"..ytdl.minver
                    .. "', try running `youtube-dl -U`.")
                ytdl.vercheck = false
            else
                msg.verbose("found youtube-dl version: " .. version)
                ytdl.vercheck = true
            end
        end

        if not (ytdl.vercheck) then
            return
        end

        -- strip ytdl://
        if (url:find("ytdl://") == 1) then
            url = url:sub(8)
        end

        local format = mp.get_property("options/ytdl-format")
        local raw_options = mp.get_property_native("options/ytdl-raw-options")

        -- subformat workaround
        local subformat = "ass/srt/best"

        local command = {
            ytdl.path, "--no-warnings", "-J", "--flat-playlist", "--all-subs",
            "--sub-format", subformat, "--no-playlist"
        }

        -- Checks if video option is "no", change options accordingly
        if (mp.get_property("options/vid") == "no") then
            format = "bestaudio/best"
            msg.verbose("Video disabled. Only using audio")
        end

        if (format ~= "") then
            table.insert(command, "--format")
            table.insert(command, format)
        end

        for param, arg in pairs(raw_options) do
            table.insert(command, "--" .. param)
            if (arg ~= "") then
                table.insert(command, arg)
            end
        end
        table.insert(command, "--")
        table.insert(command, url)
        local es, json = exec(command)

        if (es < 0) or (json == nil) or (json == "") then
            msg.warn("youtube-dl failed, trying to play URL directly ...")
            return
        end

        local json, err = utils.parse_json(json)

        if (json == nil) then
            msg.error("failed to parse JSON data: " .. err)
            return
        end

        msg.verbose("youtube-dl succeeded!")

        -- what did we get?
        if not (json["direct"] == nil) and (json["direct"] == true) then
            -- direct URL, nothing to do
            msg.verbose("Got direct URL")
            return
        elseif not (json["_type"] == nil) and (json["_type"] == "playlist") then
            -- a playlist

            if (#json.entries == 0) then
                msg.warn("Got empty playlist, nothing to play.")
                return
            end


            -- some funky guessing to detect multi-arc videos
            if  not (json.entries[1]["webpage_url"] == nil)
                and (json.entries[1]["webpage_url"] == json["webpage_url"]) then
                msg.verbose("multi-arc video detected, building EDL")


                local playlist = "edl://"
                for i, entry in pairs(json.entries) do

                    local urllength = string.len(entry.url)
                    playlist = playlist .. "%" .. urllength .. "%" .. entry.url .. ";"

                end

                msg.debug("EDL: " .. playlist)

                -- can't change the http headers for each entry, so use the 1st
                if json.entries[1] then
                    set_http_headers(json.entries[1].http_headers)
                end

                mp.set_property("stream-open-filename", playlist)
                if not (json.title == nil) then
                    mp.set_property("file-local-options/media-title", json.title)
                end

            else

                local playlist = "#EXTM3U\n"
                for i, entry in pairs(json.entries) do
                    local site = entry.url

                    -- some extractors will still return the full info for
                    -- all clips in the playlist and the URL will point
                    -- directly to the file in that case, which we don't
                    -- want so get the webpage URL instead, which is what
                    -- we want
                    if not (entry["webpage_url"] == nil) then
                        site = entry["webpage_url"]
                    end

                    playlist = playlist .. "ytdl://" .. site .. "\n"
                end

                mp.set_property("stream-open-filename", "memory://" .. playlist)
            end

        else -- probably a video
            local streamurl = ""

            -- DASH?
            if not (json["requested_formats"] == nil) then
                msg.info("Using DASH, expect inaccurate duration.")
                if not (json.duration == nil) then
                    msg.info("Actual duration: " .. mp.format_time(json.duration))
                end

                -- video url
                streamurl = json["requested_formats"][1].url

                -- audio url
                mp.set_property("file-local-options/audio-file",
                    json["requested_formats"][2].url)

                -- workaround for slow startup (causes inaccurate duration)
                mp.set_property("file-local-options/demuxer-lavf-o",
                    "fflags=+ignidx")

            elseif not (json.url == nil) then
                -- normal video
                streamurl = json.url
                set_http_headers(json.http_headers)
            else
                msg.error("No URL found in JSON data.")
                return
            end

            msg.debug("streamurl: " .. streamurl)

            mp.set_property("stream-open-filename", streamurl)

            mp.set_property("file-local-options/media-title", json.title)

            -- add subtitles
            if not (json.requested_subtitles == nil) then
                for lang, sub_info in pairs(json.requested_subtitles) do
                    msg.verbose("adding subtitle ["..lang.."]")

                    local slang = lang
                    if (lang:len() > 3) then
                        slang = lang:sub(1,2)
                    end

                    if not (sub_info.data == nil) then
                        sub = "memory://"..sub_info.data
                    else
                        sub = sub_info.url
                    end
                    mp.commandv("sub_add", sub,
                        "auto", lang.." "..sub_info.ext, slang)
                end
            end

            -- for rtmp
            if not (json.play_path == nil) then
                local rtmp_prop = append_rtmp_prop(nil, "rtmp_tcurl", streamurl)
                rtmp_prop = append_rtmp_prop(rtmp_prop, "rtmp_pageurl", json.page_url)
                rtmp_prop = append_rtmp_prop(rtmp_prop, "rtmp_playpath", json.play_path)
                rtmp_prop = append_rtmp_prop(rtmp_prop, "rtmp_swfverify", json.player_url)
                rtmp_prop = append_rtmp_prop(rtmp_prop, "rtmp_app", json.app)

                mp.set_property("file-local-options/stream-lavf-o", rtmp_prop)
            end
        end
    end
end)
