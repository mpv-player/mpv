

local ass_mt = {}
ass_mt.__index = ass_mt

function ass_new()
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

-- ---------------------------------------------------

local osc_geo = {
	-- static
	playresx = 1280,						-- canvas size X
	playresy = 720,							-- canvas size Y
	valign = 1,								-- vertical alignment, -1 (top) to 1 (bottom)
	osc_w = 550,							-- width, height, corner-radius, padding of the box
	osc_h = 150,
	osc_r = 10,
	osc_p = 15,
	
	-- calculated by osc_init
	posX, posY = 0,0, 						-- position of the controler
	pos_offsetX, pos_offsetY = 0,0, 		-- vertical/horizontal position offset for contents aligned at the borders of the box
}


local state = {
    osd_visible = false,
    mouse_down = false,
    last_mouse_pos,
    bar_location,
    mouse_down_counter = 0,
    active_button = 0,
    rightTC_trem = false,
    mp_screen_size,
    append_calls = 0,
}

-- align: -1 .. +1
-- frame: size of the containing area
-- obj: size of the object that should be positioned inside the area
-- margin: min. distance from object to frame (as long as -1 <= align <= +1)
function get_align(align, frame, obj, margin)
    frame = frame - margin * 2
    return margin + frame / 2 - obj / 2 + (frame - obj) / 2 * align
end

function draw_bar_simple(ass, x, y, w, h, style)
	local pos = 0
	if not (mp.property_get("length") == nil) then
		local duration = tonumber(mp.property_get("length"))
	    pos = tonumber(mp.property_get("time-pos")) / duration
	else
		
    	--local pos = tonumber(mp.property_get("percent-pos")) / 100
    end
        
    -- thickness of border and gap between border and filling bar
    local border, gap = 1, 2
    
    local fill_offset = border + gap
    local xp = (pos * (w - (2*fill_offset))) + fill_offset
    
    ass:draw_start()
    -- the box
    ass:rect_cw(0, 0, w, h);

    -- the "hole"
    ass:rect_ccw(border, border, w - border, h - border)
    
    -- chapter nibbles
    local chapters = mp.get_chapter_list()
    for n = 1, #chapters do
    	if chapters[n].time > 0 then
	        local s = (chapters[n].time / duration * (w - (2*fill_offset))) + fill_offset
	        	
	        ass:rect_cw(s - 1, 1, s, 2);
	        ass:rect_cw(s - 1, h - 2, s, h - 1);
	        
        end
    end
    
    -- the filling, draw it only if positive
    if pos > 0 then
	    ass:rect_cw(fill_offset, fill_offset, xp, h - fill_offset)
    end
    
    -- remember where the bar is for seeking
    local b_x, b_y, b_w, b_h = x - (w/2) + fill_offset, y - h + fill_offset, (w - (2*fill_offset)), (h - (2*fill_offset))
    state.bar_location = {b_x=b_x, b_y=b_y, b_w=b_w, b_h=b_h}
		
end


-- returns hitbox spanning coordinates (top left, bottom right corner) according to alignment
function get_hitbox_coords(x, y, an, w, h)
	
	local alignments = {
	  [1] = function (z) return x, y-h, x+w, y end,
	  [2] = function (z) return x-(w/2), y-h, x+(w/2), y end,
	  [3] = function (z) return x-w, y-h, x, y end,
	  
	  [4] = function (z) return x, y-(h/2), x+w, y+(h/2) end,
	  [5] = function (z) return x-(w/2), y-(h/2), x+(w/2), y+(h/2) end,
	  [6] = function (z) return x-w, y-(h/2), x, y+(h/2) end,
	  
	  [7] = function (z) return x, y, x+w, y+h end,
	  [8] = function (z) return x-(w/2), y, x+(w/2), y+h end,
	  [9] = function (z) return x-w, y, x, y+h end,

	}
	
	return alignments[an]()
	
end

local elements = {}

function register_element(x, y, an, w, h, styleA, styleB, content, down_cmd, up_cmd, down_repeat)

	local element = {
		x = x,
		y = y,
		an = an,
		w = w,
		h = h,
		styleA = styleA,
		styleB = styleB,
		content = content,
		down_cmd = down_cmd,
		up_cmd = up_cmd,
		down_repeat = down_repeat,
	}
	
	table.insert(elements, element)

end


function render_elements(ass)

	for n = 1, #elements do
		
		local element = elements[n]
		local style = element.styleA
		
		if state.mouse_down == true and state.active_button == n then
			local mX, mY = mp.get_mouse_pos()
			local bX1, bY1, bX2, bY2 = get_hitbox_coords(element.x, element.y, element.an, element.w, element.h)
			
			
			
			if mX >= bX1 and mX <= bX2 and mY >= bY1 and mY <= bY2 then
				if element.styleB == nil then
				else
					style = style .. element.styleB
				end
				
				if element.down_cmd == nil then
				elseif element.down_repeat == false then
					element.down_cmd()
				elseif state.mouse_down_counter == 0 or (state.mouse_down_counter >= 15 and state.mouse_down_counter % 5 == 0) then
					element.down_cmd()
				end
				state.mouse_down_counter = state.mouse_down_counter + 1
				--print("\n Oink! " .. mp.get_timer().."\n")
			end
			
		end
		
		ass:new_event()
		ass:pos(element.x, element.y)
		ass:an(element.an) -- positioning
		ass:append(style) -- styling
		if type(element.content) == "function" then
			element.content(ass) -- function objects
		else
			ass:append(element.content) -- text objects
		end
    end
end

-- Did mouse go down on a button?
function any_button_down()

	local mX, mY = mp.get_mouse_pos()
	for n = 1, #elements do
		
		local bX1, bY1, bX2, bY2 = get_hitbox_coords(elements[n].x, elements[n].y, elements[n].an, elements[n].w, elements[n].h)

		if mX >= bX1 and mX <= bX2 and mY >= bY1 and mY <= bY2 then
			--print("click on button #" .. n .. "   \n")
			state.active_button = n
		end
    end
end

-- Did mouse go up on the same button?
function any_button_up()

	local mX, mY = mp.get_mouse_pos()
	for n = 1, #elements do
		
		if elements[n].up_cmd == nil then
		-- Ignore if button doesn't have a up_cmd
		else
			local bX1, bY1, bX2, bY2 = get_hitbox_coords(elements[n].x, elements[n].y, elements[n].an, elements[n].w, elements[n].h)
	
			if mX >= bX1 and mX <= bX2 and mY >= bY1 and mY <= bY2 and state.active_button == n then
				--print("up on button #" .. n .. "    \n")
				elements[n].up_cmd()
			end
		end		
    end
    state.active_button = 0
end


local osc_styles = {
	bigButtons = "{\\bord0\\1c&HFFFFFF\\1a&H00&\\3c&HFFFFFF\\3a&HFF&\\fs50\\fnWebdings}",
	elementDown = "{\\1c&H999999}",
	elementDisab = "{\\1a&H88&}",
	timecodes = "{\\bord0\\1c&HFFFFFF\\1a&H00&\\3c&HFFFFFF\\3a&HFF&\\fs25\\fnArial}",
	vidtitle = "{\\bord0\\1c&HFFFFFF\\1a&H00&\\3c&HFFFFFF\\3a&HFF&\\fs12\\fnArial}",
	box = "{\\bord1\\1c&H000000\\1a&H64&\\3c&HFFFFFF\\3a&H00&}",
}


-- OSC INIT
function osc_init ()
	-- kill old Elements
	elements = {}
	
	-- set vertical resolution acording to display aspect
	local display_w, display_h, display_aspect = mp.get_screen_size()
	osc_geo.playresx = osc_geo.playresy * display_aspect
	

	-- Some calculations on stuff we'll need
	-- vertical/horizontal position offset for contents aligned at the borders of the box
	osc_geo.pos_offsetX, osc_geo.pos_offsetY = (osc_geo.osc_w - (2*osc_geo.osc_p)) / 2, (osc_geo.osc_h - (2*osc_geo.osc_p)) / 2

	--local playresx, playresy = mp.get_osd_resolution() -- Not avaiable here, so hardcode it
	osc_geo.posX, osc_geo.posY = math.floor(osc_geo.playresx/2), math.floor(get_align(osc_geo.valign, osc_geo.playresy, osc_geo.osc_h, 10))
	
	
	-- fetch values
	local osc_w, osc_h, osc_r, osc_p = osc_geo.osc_w, osc_geo.osc_h, osc_geo.osc_r, osc_geo.osc_p
	local pos_offsetX, pos_offsetY = osc_geo.pos_offsetX, osc_geo.pos_offsetY
	local posX, posY = osc_geo.posX, osc_geo.posY
	
	--
	-- Backround box
	--
	
	local contentF = function (ass) 
    	ass:draw_start()
	    ass:move_to(osc_r, 0)
	    ass:line_to(osc_w - osc_r, 0) -- top line
	    ass:bezier_curve(osc_w, 0, osc_w, 0, osc_w, osc_r) -- top right corner
	    ass:line_to(osc_w, osc_h - osc_r) -- right line
	    ass:bezier_curve(osc_w, osc_h, osc_w, osc_h, osc_w - osc_r, osc_h) -- bottom right corner
	    ass:line_to(osc_r, osc_h) -- bottom line
	    ass:bezier_curve(0, osc_h, 0, osc_h, 0, osc_h - osc_r) -- bottom left corner
	    ass:line_to(0, osc_r) -- left line
	    ass:bezier_curve(0, 0, 0, 0, osc_r, 0) -- top left corner
	    ass:draw_stop()
    end
	register_element(posX, posY, 5, 0, 0, osc_styles.box, osc_styles.box, contentF, nil, nil, false)
	
	-- title
	local contentF = function (ass) return ass:append(mp.property_get_string("media-title")) end
    register_element(posX, posY - pos_offsetY - 10, 8, 0, 0, osc_styles.vidtitle, nil, contentF, nil, nil, false)

	--
	-- Big buttons
	-- 
	
	local bbposY = posY - pos_offsetY + 10
	
    --play/pause
    local contentF = function (ass) 
    	if mp.property_get("pause") == "yes" then
    		ass:append("{\\fscx150}{\\fscx100}")
    	else
    		ass:append("")
    	end
    end
    local up_cmd = function () mp.send_command("no-osd cycle pause") end
    register_element(posX, bbposY, 8, 40, 40, osc_styles.bigButtons, osc_styles.elementDown, contentF, nil, up_cmd, false)
    
    --skipback
    local down_cmd = function () mp.send_command("no-osd seek -5 relative keyframes") end
    register_element(posX-60, bbposY, 8, 40, 40, osc_styles.bigButtons, osc_styles.elementDown, "", down_cmd, nil, true)
    
    --skipfrwd
    local down_cmd = function () mp.send_command("no-osd seek 10 relative keyframes") end
    register_element(posX+60, bbposY, 8, 40, 40, osc_styles.bigButtons, osc_styles.elementDown, "", down_cmd, nil, true)
    
    -- do we have chapters?
    if (#mp.get_chapter_list()) > 0 then
	    
	    --prev
	    local up_cmd = function () mp.send_command("add chapter -1") end
	    register_element(posX-120, bbposY, 8, 40, 40, osc_styles.bigButtons, osc_styles.elementDown, "", nil, up_cmd, false)
	    
	    --next
	    local up_cmd = function () mp.send_command("add chapter 1") end
	    register_element(posX+120, bbposY, 8, 40, 40, osc_styles.bigButtons, osc_styles.elementDown, "", nil, up_cmd, false)
	    
	else -- if not, render buttons as disabled and don't attach functions
	    --prev
	    register_element(posX-120, bbposY, 8, 40, 40, (osc_styles.bigButtons .. osc_styles.elementDisab), nil, "", nil, nil, false)
	    
	    --next
	    register_element(posX+120, bbposY, 8, 40, 40, (osc_styles.bigButtons .. osc_styles.elementDisab), nil, "", nil, nil, false)
	
	end
    
    -- 
    -- Seekbar
    -- 
    
    -- do we have a usuable duration?
    local contentF = function (ass) 
	    	draw_bar_simple(ass, posX, posY+pos_offsetY-30, pos_offsetX*2, 17, osc_styles.timecodes)
	end
    
    local down_cmd = function ()
    	-- Ignore identical seeks
		if state.last_mouse_pos == mp.get_mouse_pos() then
		else
			state.last_mouse_pos = mp.get_mouse_pos()
			
	    	local b_x, b_y, b_w, b_h = state.bar_location.b_x, state.bar_location.b_y, state.bar_location.b_w, state.bar_location.b_h
		    local x, y = mp.get_mouse_pos()

		    if x >= b_x and y >= b_y and x <= b_x + b_w and y <= b_y + b_h then
		        local duration = tonumber(mp.property_get("length"))
		        local time = (x - b_x) / b_w * duration
	
		        mp.send_command(string.format("no-osd seek %f absolute keyframes", time))
		    end
	    
	    end
    end
    -- do we have a usuable duration?
    if (not (mp.property_get("length") == nil)) and (tonumber(mp.property_get("length")) > 0) then
	    register_element(posX, posY+pos_offsetY-30, 2, pos_offsetX*2, 17, osc_styles.timecodes, nil, contentF, down_cmd, nil, false)
	else
		register_element(posX, posY+pos_offsetY-30, 2, pos_offsetX*2, 17, (osc_styles.timecodes .. osc_styles.elementDisab), nil, contentF, nil, nil, false)
	end
    
    --
    -- Timecodes
    --
    
    -- left (current pos)
    local contentF = function (ass) return ass:append(mp.property_get_string("time-pos")) end
    register_element(posX - pos_offsetX, posY + pos_offsetY, 1, 110, 25, osc_styles.timecodes, nil, contentF, nil, nil, false)
    
    -- right (total/remaining time)
    local contentF = function (ass)
    	if state.rightTC_trem == true then
    		return ass:append(mp.property_get_string("time-remaining"))
    	else
    		return ass:append(mp.property_get_string("length"))
    	end
    end
    local up_cmd = function () state.rightTC_trem = not state.rightTC_trem end
    -- do we have a usuable duration?
    if (not (mp.property_get("length") == nil)) and (tonumber(mp.property_get("length")) > 0) then
	    register_element(posX + pos_offsetX, posY + pos_offsetY, 3, 110, 25, osc_styles.timecodes, nil, contentF, nil, up_cmd, false)
	end
	
end


function draw_osc(ass)
    
	render_elements(ass)
		
end

function mouse_over_osc()
	local mX, mY = mp.get_mouse_pos()
	local bX1, bY1, bX2, bY2 = get_hitbox_coords(osc_geo.posX, osc_geo.posY, 5, osc_geo.osc_w, osc_geo.osc_h)
			
	if mX >= bX1 and mX <= bX2 and mY >= bY1 and mY <= bY2 then
		return true
	else
		return false
	end
end

function show_osc()
	state.last_osd_time = mp.get_timer()
    state.osd_visible = true
end

-- called by input.conf bindings
function mp_mouse_move()
	show_osc()
end

function mp_mouse_click(down)

	-- Build our own mouse_down/up events
	if down == true and state.mouse_down == false then
		mouse_down();
	elseif down == false and state.mouse_down == true then
		mouse_up();
	end
	
	

	state.mouse_down = down
	mp_update()
end

function mouse_up()
	--mp.send_command("set pause no")
	state.mouse_down_counter = 0
	any_button_up()
end

function mouse_down()
	--mp.send_command("set pause yes")
	if state.osd_visible == true then
		any_button_down()
	end
end


-- called by mpv on every frame
function mp_update()

	if state.mp_screen_size == mp.get_screen_size() then
	-- nothing changed
	else
	-- display changed, reinit everything
		osc_init()
		state.mp_screen_size = mp.get_screen_size()
	end
	
	local ass = ass_new()
    
    local x, y = mp.get_mouse_pos()
    
    local now = mp.get_timer()
    
    if mouse_over_osc() == true or state.mouse_down == true then
    	show_osc()
    end
    
    --state.append_calls = 0
    
    local osd_time = 1
    if state.osd_visible and now - state.last_osd_time < osd_time then
        draw_osc(ass)
        state.osd_visible = true
    else
        state.osd_visible = false
    end
    
    --ass:new_event()
    --local playresx, playresy = mp.get_osd_resolution()
    --ass:append("get_osd_resolution: X:" .. playresx .. " Y:" .. playresy)
        
    --ass:append("Rendertime: " .. mp.get_timer() - now .. "   state.append_calls: " .. state.append_calls)
    
    --[[

    ass:new_event()
    ass:pos(x, y)
    ass:append("{\\an5}")
    if state.mouse_down == true then
        ass:append("-")
    else
        ass:append("+")
    end
    --]]
    -- set canvas size
    --mp.set_osd_ass(osc_geo.playresx, osc_geo.playresy, ass.text)
    
    local w, h, aspect = mp.get_screen_size()
    mp.set_osd_ass(osc_geo.playresy * aspect, osc_geo.playresy, ass.text)
    
end
