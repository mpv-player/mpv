--  zones.lua: mpv script for handling commands depending on where the mouse pointer is at,
--             mostly for mouse wheel handling, by configuring it via input.conf, e.g.:
--
--  Ported from avih's ( https://github.com/avih ) zones.js
--
--  Vertical positions can be top, middle, bottom or "*" to represent the whole column.
--  Horizontal positions can be left, middle, bottom or "*" to represent the whole row.
--  "default" will be the fallback command to be used if no command is assigned to that area.
--
--  input.conf example of use:
-- # wheel up/down with mouse
-- MOUSE_BTN3 script_message_to zones commands "middle-right: add brightness  1" "*-left: add volume  5" "default: seek  10"
-- MOUSE_BTN4 script_message_to zones commands "middle-right: add brightness -1" "*-left: add volume -5"  "default: seek -10"

local ZONE_THRESH_PERCENTAGE = 20;
-- sides get 20% each, mid gets 60%, same vertically

local msg = mp.msg

function getMouseZone()
    -- returns the mouse zone as two strings [top/middle/bottom], [left/middle/right], e.g. "middle", "right"

    local screenW, screenH = mp.get_osd_resolution()
    local mouseX, mouseY   = mp.get_mouse_pos()

    local threshY = screenH * ZONE_THRESH_PERCENTAGE / 100
    local threshX = screenW * ZONE_THRESH_PERCENTAGE / 100

    local yZone = (mouseY < threshY) and "top"  or (mouseY < (screenH - threshY)) and "middle" or "bottom"
    local xZone = (mouseX < threshX) and "left" or (mouseX < (screenW - threshX)) and "middle" or "right"

    return yZone, xZone
end

function main (...)
    local arg={...}
    msg.debug('commands: \n\t'..table.concat(arg,'\n\t'))

    local keyY, keyX = getMouseZone()
    msg.debug("mouse at: " .. keyY .. '-' .. keyX)

    local fallback = nil

    for i, v in ipairs(arg) do
        cmdY = v:match("^([%w%*]+)%-?[%w%*]*:")
        cmdX = v:match("^[%w%*]*%-([%w%*]+)%s*:")
        cmd  = v:match("^[%S]-%s*:%s*(.+)")
        msg.debug('cmdY: '..tostring(cmdY))
        msg.debug('cmdX: '..tostring(cmdX))
        msg.debug('cmd : '..tostring(cmd))

        if (cmdY == keyY and cmdX == keyX) then
            msg.verbose("running cmd: "..cmd)
            mp.command(cmd)
            return
        elseif  (cmdY == "*"  and cmdX == keyX) or
                (cmdY == keyY and cmdX == "*") then
            msg.verbose("running cmd: "..cmd)
            mp.command(cmd)
            return
        elseif cmdY == "default" then
            fallback = cmd
        end
    end
    if fallback ~= nil then
        msg.verbose("running cmd: "..fallback)
        mp.command(fallback)
        return
    else
        msg.debug("no command assigned for "..keyY .. '-' .. keyX)
        return
    end
end
mp.register_script_message("commands", main)
