local assdraw = require 'mp.assdraw'
local utils = require 'mp.utils'

local things = {}
for i = 1, 2 do
    things[i] = {
        osd1 = mp.create_osd_overlay("ass-events"),
        osd2 = mp.create_osd_overlay("ass-events")
    }
end
things[1].text = "{\\an5}hello\\Nworld"
things[2].text = "{\\pos(400, 200)}something something"

mp.add_periodic_timer(2, function()
    for i, thing in ipairs(things) do
        thing.osd1.data = thing.text
        thing.osd1.compute_bounds = true
        --thing.osd1.hidden = true
        local res = thing.osd1:update()
        print("res " .. i .. ": " .. utils.to_string(res))

        thing.osd2.hidden = true
        if res ~= nil and res.x0 ~= nil then
            local draw = assdraw.ass_new()
            draw:append("{\\alpha&H80}")
            draw:draw_start()
            draw:pos(0, 0)
            draw:rect_cw(res.x0, res.y0, res.x1, res.y1)
            draw:draw_stop()
            thing.osd2.hidden = false
            thing.osd2.data = draw.text
        end
        thing.osd2:update()
    end
end)
