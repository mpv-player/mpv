local utils = require 'mp.utils'
local msg = require 'mp.msg'

local svtplay_dl = {
    path = "svtplay-dl",
    searched = false
}

local function exec(args)
    local ret = utils.subprocess({args = args})
    return ret.status, ret.stdout, ret
end

mp.add_hook("on_load", 10, function ()
    local url = mp.get_property("stream-open-filename")

    if (url:find("http://") == 1) or (url:find("https://") == 1) then

        -- check for svtplay-dl in mpv's config dir
        if not (svtplay_dl.searched) then
            local svtplay_dl_mcd = mp.find_config_file("svtplay-dl")
            if not (svtplay_dl_mcd == nil) then
                msg.verbose("found svtplay-dl at: " .. svtplay_dl_mcd)
                svtplay_dl.path = svtplay_dl_mcd
            end
            svtplay_dl.searched = true
        end

        local quality = mp.get_property("options/svtplay-dl-quality")
        local raw_options =
            mp.get_property_native("options/svtplay-dl-raw-options")

        local command = {
            svtplay_dl.path, "--silent", "--get-url"
        }

        if (quality ~= "") then
            table.insert(command, "--quality")
            table.insert(command, quality)
        end

        for param, arg in pairs(raw_options) do
            table.insert(command, "--" .. param)
            if (arg ~= "") then
                table.insert(command, arg)
            end
        end

        table.insert(command, url)

        msg.debug("Running: " .. table.concat(command, ' '))
        local es, stdout, result = exec(command)

        if (es < 0) or (stdout == nil) or (stdout == "") then
            if not result.killed_by_us then
                msg.warn("svtplay-dl failed, trying to play URL directly ...")
                msg.warn("try disabling youtube-dl hook with --no-ytdl")
            end
            return
        end

        msg.verbose("svtplay-dl succeeded!")

        streamurl = string.gsub(stdout, "\n", "")
        mp.set_property("stream-open-filename", streamurl)
    end
end)
