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
local input = {}

local function register_event_handler(t)
    mp.register_script_message("input-event", function (type, args)
        if t[type] then
            local suggestions, completion_start_position =
                t[type](unpack(utils.parse_json(args or "") or {}))

            if type == "complete" and suggestions then
                mp.commandv("script-message-to", "console", "complete",
                            utils.format_json(suggestions), completion_start_position)
            end
        end

        if type == "closed" then
            mp.unregister_script_message("input-event")
        end
    end)
end

function input.get(t)
    mp.commandv("script-message-to", "console", "get-input",
                mp.get_script_name(), utils.format_json({
                    prompt = t.prompt,
                    default_text = t.default_text,
                    cursor_position = t.cursor_position,
                    id = t.id,
                }))

    register_event_handler(t)
end

function input.select(t)
    mp.commandv("script-message-to", "console", "get-input",
                mp.get_script_name(), utils.format_json({
                    prompt = t.prompt,
                    items = t.items,
                    default_item = t.default_item,
                }))

    register_event_handler(t)
end

function input.terminate()
    mp.commandv("script-message-to", "console", "disable")
end

function input.log(message, style, terminal_style)
    mp.commandv("script-message-to", "console", "log", utils.format_json({
                   text = message,
                   style = style,
                   terminal_style = terminal_style,
               }))
end

function input.log_error(message)
    mp.commandv("script-message-to", "console", "log",
                 utils.format_json({ text = message, error = true }))
end

function input.set_log(log)
    mp.commandv("script-message-to", "console", "set-log", utils.format_json(log))
end

return input
