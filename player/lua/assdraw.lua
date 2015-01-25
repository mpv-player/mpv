local ass_mt = {}
ass_mt.__index = ass_mt

local function ass_new()
    return setmetatable({ scale = 4, text = "" }, ass_mt)
end

function ass_mt.new_event(ass)
    -- osd_libass.c adds an event per line
    if #ass.text > 0 then
        ass.text = ass.text .. "\n"
    end
end

function ass_mt.draw_start(ass)
    ass.text = string.format("%s{\\p%d}", ass.text, ass.scale)
end

function ass_mt.draw_stop(ass)
    ass.text = ass.text .. "{\\p0}"
end

function ass_mt.coord(ass, x, y)
    local scale = 2 ^ (ass.scale - 1)
    local ix = math.ceil(x * scale)
    local iy = math.ceil(y * scale)
    ass.text = string.format("%s %d %d", ass.text, ix, iy)
end

function ass_mt.append(ass, s)
    ass.text = ass.text .. s
end

function ass_mt.merge(ass1, ass2)
    ass1.text = ass1.text .. ass2.text
end

function ass_mt.pos(ass, x, y)
    ass:append(string.format("{\\pos(%f,%f)}", x, y))
end

function ass_mt.an(ass, an)
    ass:append(string.format("{\\an%d}", an))
end

function ass_mt.move_to(ass, x, y)
    ass:append(" m")
    ass:coord(x, y)
end

function ass_mt.line_to(ass, x, y)
    ass:append(" l")
    ass:coord(x, y)
end

function ass_mt.bezier_curve(ass, x1, y1, x2, y2, x3, y3)
    ass:append(" b")
    ass:coord(x1, y1)
    ass:coord(x2, y2)
    ass:coord(x3, y3)
end


function ass_mt.rect_ccw(ass, x0, y0, x1, y1)
    ass:move_to(x0, y0)
    ass:line_to(x0, y1)
    ass:line_to(x1, y1)
    ass:line_to(x1, y0)
end

function ass_mt.rect_cw(ass, x0, y0, x1, y1)
    ass:move_to(x0, y0)
    ass:line_to(x1, y0)
    ass:line_to(x1, y1)
    ass:line_to(x0, y1)
end

function ass_mt.round_rect_cw(ass, x0, y0, x1, y1, r)
    local c = 0.551915024494 * r -- circle approximation
    ass:move_to(x0 + r, y0)
    ass:line_to(x1 - r, y0) -- top line
    if r > 0 then
        ass:bezier_curve(x1 - r + c, y0, x1, y0 + r - c, x1, y0 + r) -- top right corner
    end
    ass:line_to(x1, y1 - r) -- right line
    if r > 0 then
        ass:bezier_curve(x1, y1 - r + c, x1 - r + c, y1, x1 - r, y1) -- bottom right corner
    end
    ass:line_to(x0 + r, y1) -- bottom line
    if r > 0 then
        ass:bezier_curve(x0 + r - c, y1, x0, y1 - r + c, x0, y1 - r) -- bottom left corner
    end
    ass:line_to(x0, y0 + r) -- left line
    if r > 0 then
        ass:bezier_curve(x0, y0 + r - c, x0 + r - c, y0, x0 + r, y0) -- top left corner
    end
end

return {ass_new = ass_new}
