-- This script adds control to the dynamic range compression ffmpeg
-- filter including key bindings for adjusting parameters.
--
-- See https://ffmpeg.org/ffmpeg-filters.html#acompressor for explanation
-- of the parameteres.
--
-- Default key bindings:
--              n: Toggle dynamic range compression on or off
--    F1/Shift+F1: Increase/Decrease threshold parameter
--    F2/Shift+F2: Increase/Decrease ratio parameter
--    F3/Shift+F3: Increase/Decrease knee parameter
--    F4/Shift+F4: Increase/Decrease makeup gain parameter
--    F5/Shift+F5: Increase/Decrease attack parameter
--    F6/Shift+F6: Increase/Decrease release parameter
--
-- To change key bindings in input.conf use:
--     BINDING script-message-to acompressor toggle-acompressor
--     BINDING script-message-to acompressor update-param PARAM INCREMENT
-- BINDING is the key binding to use, PARAM is either 'attack', 'release',
-- 'threshold', 'ratio', 'knee' or 'makeup' and INCREMENT is a signed floating
-- point value to add to the current parameter value.
--
-- You may also just adjust default parameters to your liking in this table.
local params = {
	-- 'hide' defines wether the parameter should be hidden from OSD display (it will still be included in ffmpeg filter graph).
	-- 'input_format' is used to parse the value back from a obtained filter graph.
	-- 'output_format' defines how the value shall be formatted for creating the filter graph.
	{ name = 'Attack',    value= 20, min=0.01, max=2000, hide= 20, input_format='attack=(%d+[.%d+]*)',       output_format='%g'   },
	{ name = 'Release',   value=250, min=0.01, max=9000, hide=250, input_format='release=(%d+[.%d+]*)',      output_format='%g'   },
	{ name = 'Threshold', value=-25, min= -30, max=   0, hide=nil, input_format='threshold=(-%d+[.%d+]*)dB', output_format='%gdB' },
	{ name = 'Ratio',     value=  3, min=   1, max=  20, hide=nil, input_format='ratio=(%d+[.%d+]*)',        output_format='%g'   },
	{ name = 'Knee',      value=  2, min=   1, max=  10, hide=  2, input_format='knee=(%d+[.%d+]*)dB',       output_format='%gdB' },
	{ name = 'Makeup',    value=  8, min=   0, max=  24, hide=nil, input_format='makeup=(%d+[.%d+]*)dB',     output_format='%gdB' }
}

-- Defines the mpv filter label to be used. This allows us to easily add/replace/remove it, so
-- it should be left so something meaningful and unique.
local filter_label = 'acompressor'

local function update_filter()
	local graph = {}
	local pretty = {}

	for _,param in pairs(params) do
		graph[#graph+1] = string.format('%s=' .. param.output_format, string.lower(param.name), param.value)
		if param.hide ~= param.value then
			pretty[#pretty+1] = string.format('%s: ' .. param.output_format, param.name, param.value)
		end
	end

	if #pretty == 0 then
		pretty = ''
	else
		pretty = '\n(' .. table.concat(pretty, ', ') .. ')'
	end

	mp.command(string.format('no-osd af add @%s:lavfi=[acompressor=%s]; show-text "Dynamic range compressor: enabled%s" 4000', filter_label, table.concat(graph, ':'), pretty))
end

local function read_filter()
	local graph = nil
	local af = mp.get_property_native('af', {})

	for i = 1, #af do
		if af[i]['name'] == 'lavfi' and af[i]['label'] == 'acompressor' then
			graph = af[i]['params']['graph']
			break
		end
	end

	if graph == nil then
		return false
	end

	for _,param in pairs(params) do
		local value = tonumber(string.match(graph, param.input_format))
		if value ~= nil and value >= param.min and value <= param.max then
			param.value = value
		end
	end
	return true
end

local function toggle_acompressor()
	if read_filter() then
		mp.command(string.format('no-osd af del @%s; show-text "Dynamic range compressor: disabled"', filter_label))
	else
		update_filter()
	end
end

local function update_param(name, increment)
	-- The current filter values could come from a watch_later config.
	-- read_filter() will parse them, so that we don't clobber those with our default values.
	read_filter()
	for _,param in pairs(params) do
		if string.lower(param.name) == string.lower(name) then
			param.value = math.max(param.min, math.min(param.value + increment, param.max))
			update_filter()
			return
		end
	end

	mp.msg.error('Unknown parameter "' .. name .. '"')
end

mp.add_key_binding("n", "toggle-acompressor", toggle_acompressor)
mp.register_script_message('update-param', update_param)

mp.add_key_binding("F1", 'acompressor-inc-threshold', function() update_param('threshold', -5); end, { repeatable = true })
mp.add_key_binding("Shift+F1", 'acompressor-dec-threshold', function() update_param('Threshold', 5); end, { repeatable = true })

mp.add_key_binding("F2", 'acompressor-inc-ratio', function() update_param('Ratio', 1); end, { repeatable = true })
mp.add_key_binding("Shift+F2", 'acompressor-dec-ratio', function() update_param('Ratio', -1); end, { repeatable = true })

mp.add_key_binding("F3", 'acompressor-inc-knee', function() update_param('Knee', 1); end, { repeatable = true })
mp.add_key_binding("Shift+F3", 'acompressor-dec-knee', function() update_param('Knee', -1); end, { repeatable = true })

mp.add_key_binding("F4", 'acompressor-inc-makeup', function() update_param('Makeup', 1); end, { repeatable = true })
mp.add_key_binding("Shift+F4", 'acompressor-dec-makeup', function() update_param('Makeup', -1); end, { repeatable = true })

mp.add_key_binding("F5", 'acompressor-inc-attack', function() update_param('Attack', 10); end, { repeatable = true })
mp.add_key_binding("Shift+F5", 'acompressor-dec-attack', function() update_param('Attack', -10); end, { repeatable = true })

mp.add_key_binding("F6", 'acompressor-inc-release', function() update_param('Release', 10); end, { repeatable = true })
mp.add_key_binding("Shift+F6", 'acompressor-dec-release', function() update_param('Release', -10); end, { repeatable = true })
