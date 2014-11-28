-- Test script for property change notification mechanism.

local utils = require("mp.utils")

for i,name in ipairs(mp.get_property_native("property-list")) do
    mp.observe_property(name, "native", function(name, val)
        print("property '" .. name .. "' changed to '" ..
              utils.to_string(val) .. "'")
    end)
end
