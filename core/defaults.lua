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
end
