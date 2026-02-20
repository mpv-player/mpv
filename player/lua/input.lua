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

local handle_counter = 0
local latest_handler_id
local latest_log_id

local function get_non_callbacks(t)
    local non_callbacks = {}

    for key, value in pairs(t) do
        if type(value) ~= "function" then
            non_callbacks[key] = value
        end
    end

    return non_callbacks
end

local function register_event_handler(t)
    local handler_id = "input-event/"..handle_counter
    handle_counter = handle_counter + 1
    latest_handler_id = handler_id

    mp.register_script_message(handler_id, function (type, args)
        if type == "closed" then
            mp.unregister_script_message(handler_id)
        end

        -- do not process events (other than closed) for an input that has been overwritten
        if not t[type] or (latest_handler_id ~= handler_id and type ~= "closed") then
            return
        end

        args = utils.parse_json(args or "") or {}

        if type == "complete" then
            local function complete(completions, completion_pos, completion_append)
                if not completions then
                    return
                end

                mp.commandv("script-message-to", "console", "complete", utils.format_json({
                                client_name = mp.get_script_name(),
                                handler_id = handler_id,
                                original_line = args[1],
                                list = completions,
                                start_pos = completion_pos,
                                append = completion_append or "",
                            }))
            end

            args[2] = complete
            complete(t[type](unpack(args)))
        else
            t[type](unpack(args))
        end
    end)

    return handler_id
end

local function input_request(t)
    t.has_completions = t.complete ~= nil
    t.client_name = mp.get_script_name()
    t.handler_id = register_event_handler(t)

    mp.commandv("script-message-to", "console", "get-input",
                utils.format_json(get_non_callbacks(t)))
end

function input.get(t)
    -- input.select does not support log buffers, so cannot override the latest id.
    t.id = t.id or mp.get_script_name()..(t.prompt or "")
    latest_log_id = t.id
    return input_request(t)
end

input.select = input_request

function input.terminate()
    mp.commandv("script-message-to", "console", "disable", utils.format_json({
                    client_name = mp.get_script_name(),
                }))
end

function input.log(message, style, terminal_style)
    mp.commandv("script-message-to", "console", "log", utils.format_json({
                    log_id = latest_log_id,
                    text = message,
                    style = style,
                    terminal_style = terminal_style,
                }))
end

function input.log_error(message)
    mp.msg.warn("log_error is deprecated and will be removed.")

    mp.commandv("script-message-to", "console", "log", utils.format_json({
                    log_id = latest_log_id,
                    text = message,
                    error = true,
                }))
end

function input.set_log(log)
    if latest_log_id then
        mp.commandv("script-message-to", "console", "set-log",
            latest_log_id, utils.format_json(log))
    end
end

return input
