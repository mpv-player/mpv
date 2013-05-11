-- print as rectangular table, taking maxwidth space at most
function print_ascii_table(input, maxwidth)
    local t = {}
    local cols = {}
    for x, row in ipairs(input) do
        local nrow = {}
        for y, _ in ipairs(row) do
            nrow[y] = tostring(row[y])
            cols[y] = math.max(#nrow[y], cols[y] or 0)
        end
        t[#t + 1] = nrow
    end
    -- reduce columns that take more space than allowed average width
    if maxwidth then
        maxwidth = maxwidth - (#cols + 1)
        local w = 0
        local aw = math.floor(maxwidth / #cols)
        local xw, xn = 0, 0
        for y = 1, #cols do
            w = w + cols[y]
            if cols[y] <= aw then
                xw = xw + cols[y]
                xn = xn + 1
            end
        end
        if w > maxwidth then
            local nw = math.floor((maxwidth - xw) / (#cols - xn))
            for y = 1, #cols do
                if cols[y] > aw then
                    cols[y] = nw
                end
            end
        end
    end
    local function horiz_line()
        local s = "+"
        for y = 1, #cols do
            s = s .. ("-"):rep(cols[y]) .. "+"
        end
        print(s)
    end
    horiz_line()
    for x, row in ipairs(t) do
        local s = "|"
        for y = 1, #cols do
            local col = row[y] or ""
            if #col > cols[y] then
                col = col:sub(1, cols[y] - 3) .. "..."
            end
            s = s .. col .. (" "):rep(cols[y] - #col) .. "|"
        end
        print(s)
    end
    horiz_line()
end

function dump_properties()
    local t = {}
    for i,n in ipairs(mp.property_list()) do
        local r = {n, mp.property_get(n), mp.property_get_string(n)}
        for i = 2, 3 do
            if r[i] == nil then
                r[i] = "nil"
            end
        end
        t[#t + 1] = r
    end
    print_ascii_table(t, 80)
end

--dump_properties()

function mp_update()
    -- called on each playloop iteration
    --mp.set_osd_ass("Time: {\\b1}" .. mp.property_get_string("time-pos"))
end

local ass_mt = {}
ass_mt.__index = ass_mt

local function ass_new()
    return setmetatable({ scale = 4, text = "" }, ass_mt)
end

package.loaded["assdraw"] = {ass_new = ass_new}

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
    local scale = math.pow(2, ass.scale - 1)
    local ix = math.ceil(x * scale)
    local iy = math.ceil(y * scale)
    ass.text = string.format("%s %d %d", ass.text, ix, iy)
end

function ass_mt.append(ass, s)
    ass.text = ass.text .. s
    --state.append_calls = state.append_calls + 1
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
