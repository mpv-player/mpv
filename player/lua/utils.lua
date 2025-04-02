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

local default_config_file = [[# https://mpv.io/manual/master/#configuration-files
# https://mpv.io/manual/master/#options

]]

local default_input_conf = [[# https://mpv.io/manual/master/#command-interface

]]

local function show_error(message)
    mp.msg.error(message)
    if mp.get_property_native("vo-configured") then
        mp.osd_message(message)
    end
end

local function edit_config_file(filename, initial_contents)
    local path = mp.find_config_file(filename)

    if not path then
        path = mp.command_native({"expand-path", "~~/" .. filename})
        local file_handle, error_message = io.open(path, "w")

        if not file_handle then
            show_error(error_message)
            return
        end

        file_handle:write(initial_contents)
        file_handle:close()
    end

    local platform = mp.get_property("platform")
    local args
    if platform == "windows" then
        args = {"rundll32", "url.dll,FileProtocolHandler", path}
    elseif platform == "darwin" then
        args = {"open", path}
    else
        args = {"xdg-open", path}
    end

    local result = mp.command_native({
        name = "subprocess",
        playback_only = false,
        args = args,
    })

    if result.status < 0 then
        show_error("Subprocess error: " .. result.error_string)
    elseif result.status > 0 then
        show_error(utils.to_string(args) .. " failed with code " ..
                   result.status)
    end
end

mp.add_key_binding(nil, "edit-config-file", function ()
    edit_config_file("mpv.conf", default_config_file)
end)

mp.add_key_binding(nil, "edit-input-conf", function ()
    edit_config_file("input.conf", default_input_conf)
end)
