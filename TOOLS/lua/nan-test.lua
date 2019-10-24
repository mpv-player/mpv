-- Test a float property which internally uses NaN.
-- Run with --no-config (or just scale-param1 not set).

local utils = require 'mp.utils'

prop_name = "scale-param1"

-- internal NaN, return string "default" instead of NaN
v = mp.get_property_native(prop_name, "fail")
print("Exp:", "string", "\"default\"")
print("Got:", type(v), utils.to_string(v))

v = mp.get_property(prop_name)
print("Exp:", "default")
print("Got:", v)

-- not representable -> return provided fallback value
v = mp.get_property_number(prop_name, -100)
print("Exp:", -100)
print("Got:", v)

mp.set_property_native(prop_name, 123)
v = mp.get_property_number(prop_name, -100)
print("Exp:", "number", 123)
print("Got:", type(v), utils.to_string(v))

-- try to set an actual NaN
st, msg = mp.set_property_number(prop_name, 0.0/0)
print("Exp:", nil, "<message>")
print("Got:", st, msg)

-- set default
mp.set_property(prop_name, "default")

v = mp.get_property(prop_name)
print("Exp:", "default")
print("Got:", v)
