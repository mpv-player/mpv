-- defaults.lua
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

function dump_tracks()
    local tracks = mp.get_track_list()
    local t = {}
    for i = 1, #tracks do
        local track = tracks[i]
        t[#t + 1] = {"-"}
        for name, val in pairs(track) do
            t[#t + 1] = {name, val}
        end
    end
    print_ascii_table(t)
end

function mp_update()
    -- called on each playloop iteration
    --mp.set_osd_ass("Time: {\\b1}" .. mp.property_get_string("time-pos"))
end

local callbacks = {}
-- ideally, each script would have its own section, so that they don't conflict
local section = "script"

-- Set the list of key bindings. These will override the user's bindings, so
-- you should use this sparingly.
-- A call to this function will remove all bindings previously set with this
-- function. For example, set_key_bindings({}) would remove all script defined
-- key bindings.
-- Note: the bindings are not active by default. Use enable_key_bindings().
--
-- list is an array of key bindings, where each entry is an array as follow:
--      {key, callback}
--      {key, callback, callback_down}
-- key is the key string as used in input.conf, like "ctrl+a"
-- callback is a Lua function that is called when the key binding is used.
-- callback_down can be given too, and is called when a mouse button is pressed
-- if the key is a mouse button. (The normal callback will be for mouse button
-- down.)
--
-- callback can be a string too, in which case the following will be added like
-- an input.conf line: key .. " " .. callback
-- (And callback_down is ignored.)
function set_key_bindings(list)
    local cfg = ""
    for i = 1, #list do
        local entry = list[i]
        local key = entry[1]
        local cb = entry[2]
        local cb_down = entry[3]
        if type(cb) == "function" then
            callbacks[#callbacks + 1] = {press=cb, before_press=cb_down}
            cfg = cfg .. key .. " script_dispatch " .. #callbacks .. "\n"
        else
            cfg = cfg .. key .. " " .. cb .. "\n"
        end
    end
    mp.input_define_section(section, cfg)
end

function enable_key_bindings()
    mp.input_enable_section(section)
end

function disable_key_bindings()
    mp.input_disable_section(section)
end

function set_mouse_area(x0, y0, x1, y1)
    mp.input_set_section_mouse_area(section, x0, y0, x1, y1)
end

--[[
set_key_bindings {
    {"a", function(e) print("\nkey a") end},
    {"b", function(e) print("\nkey b") end},
    {"d", function(e) print("\ndisable input") disable_key_bindings() end},
    {"mouse_btn0", function(e) print("\nmouse up") end,
                   function(e) print("\nmouse down") end},
    {"mouse_btn2", function(e) print("\nright mouse up") end,
                   function(e) print("\nright mouse down") end},
    {"mouse_move", function(e) print("\nmouse move") end},
}
enable_key_bindings()
set_mouse_area(50, 50, 500, 500)
--]]

function mp_script_dispatch(id, event)
    local cb = callbacks[id]
    if cb then
        if event == "press" and cb.press then
            cb.press()
        elseif event == "keyup_follows" and cb.before_press then
            cb.before_press()
        end
    end
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
    ass:move_to(x0 + r, y0)
    ass:line_to(x1 - r, y0) -- top line
    if r > 0 then ass:bezier_curve(x1, y0, x1, y0, x1, y0 + r) end -- top right corner
    ass:line_to(x1, y1 - r) -- right line
    if r > 0 then ass:bezier_curve(x1, y1, x1, y1, x1 - r, y1) end -- bottom right corner
    ass:line_to(x0 + r, y1) -- bottom line
    if r > 0 then ass:bezier_curve(x0, y1, x0, y1, x0, y1 - r) end -- bottom left corner
    ass:line_to(x0, y0 + r) -- left line
    if r > 0 then ass:bezier_curve(x0, y0, x0, y0, x0 + r, y0) end -- top left corner
end

