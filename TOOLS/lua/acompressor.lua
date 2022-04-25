-- This script adds control to the dynamic range compression ffmpeg
-- filter including key bindings for adjusting parameters.
--
-- See https://ffmpeg.org/ffmpeg-filters.html#acompressor for explanation
-- of the parameters.

local mp = require 'mp'
local options = require 'mp.options'

local o = {
	default_enable = false,
	show_osd = true,
	osd_timeout = 4000,
	filter_label = mp.get_script_name(),

	key_toggle = 'n',
	key_increase_threshold = 'F1',
	key_decrease_threshold = 'Shift+F1',
	key_increase_ratio = 'F2',
	key_decrease_ratio = 'Shift+F2',
	key_increase_knee = 'F3',
	key_decrease_knee = 'Shift+F3',
	key_increase_makeup = 'F4',
	key_decrease_makeup = 'Shift+F4',
	key_increase_attack = 'F5',
	key_decrease_attack = 'Shift+F5',
	key_increase_release = 'F6',
	key_decrease_release = 'Shift+F6',

	default_threshold = -25.0,
	default_ratio = 3.0,
	default_knee = 2.0,
	default_makeup = 8.0,
	default_attack = 20.0,
	default_release = 250.0,

	step_threshold = -2.5,
	step_ratio = 1.0,
	step_knee = 1.0,
	step_makeup = 1.0,
	step_attack = 10.0,
	step_release = 10.0,
}
options.read_options(o)

local params = {
	{ name = 'attack',    min=0.01, max=2000, hide_default=true,  dB=''   },
	{ name = 'release',   min=0.01, max=9000, hide_default=true,  dB=''   },
	{ name = 'threshold', min= -30, max=   0, hide_default=false, dB='dB' },
	{ name = 'ratio',     min=   1, max=  20, hide_default=false, dB=''   },
	{ name = 'knee',      min=   1, max=  10, hide_default=true,  dB='dB' },
	{ name = 'makeup',    min=   0, max=  24, hide_default=false, dB='dB' },
}

local function parse_value(value)
	-- Using nil here because tonumber differs between lua 5.1 and 5.2 when parsing fractions in combination with explicit base argument set to 10.
	-- And we can't omit it because gsub returns 2 values which would get unpacked and cause more problems. Gotta love scripting languages.
	return tonumber(value:gsub('dB$', ''), nil)
end

local function format_value(value, dB)
	return string.format('%g%s', value, dB)
end

local function show_osd(filter)
	if not o.show_osd then
		return
	end

	if not filter.enabled then
		mp.commandv('show-text', 'Dynamic range compressor: disabled', o.osd_timeout)
		return
	end

	local pretty = {}
	for _,param in ipairs(params) do
		local value = parse_value(filter.params[param.name])
		if not (param.hide_default and value == o['default_' .. param.name]) then
			pretty[#pretty+1] = string.format('%s: %g%s', param.name:gsub("^%l", string.upper), value, param.dB)
		end
	end

	if #pretty == 0 then
		pretty = ''
	else
		pretty = '\n(' .. table.concat(pretty, ', ') .. ')'
	end

	mp.commandv('show-text', 'Dynamic range compressor: enabled' .. pretty, o.osd_timeout)
end

local function get_filter()
	local af = mp.get_property_native('af', {})

	for i = 1, #af do
		if af[i].label == o.filter_label then
			return af, i
		end
	end

	af[#af+1] = {
		name = 'acompressor',
		label = o.filter_label,
		enabled = false,
		params = {},
	}

	for _,param in pairs(params) do
		af[#af].params[param.name] = format_value(o['default_' .. param.name], param.dB)
	end

	return af, #af
end

local function toggle_acompressor()
	local af, i = get_filter()
	af[i].enabled = not af[i].enabled
	mp.set_property_native('af', af)
	show_osd(af[i])
end

local function update_param(name, increment)
	for _,param in pairs(params) do
		if param.name == string.lower(name) then
			local af, i = get_filter()
			local value = parse_value(af[i].params[param.name])
			value = math.max(param.min, math.min(value + increment, param.max))
			af[i].params[param.name] = format_value(value, param.dB)
			af[i].enabled = true
			mp.set_property_native('af', af)
			show_osd(af[i])
			return
		end
	end

	mp.msg.error('Unknown parameter "' .. name .. '"')
end

mp.add_key_binding(o.key_toggle, "toggle-acompressor", toggle_acompressor)
mp.register_script_message('update-param', update_param)

for _,param in pairs(params) do
	for direction,step in pairs({increase=1, decrease=-1}) do
		mp.add_key_binding(o['key_' .. direction .. '_' .. param.name],
		                   'acompressor-' .. direction .. '-' .. param.name,
		                   function() update_param(param.name, step*o['step_' .. param.name]); end,
		                   { repeatable = true })
	end
end

if o.default_enable then
	local af, i = get_filter()
	af[i].enabled = true
	mp.set_property_native('af', af)
end
