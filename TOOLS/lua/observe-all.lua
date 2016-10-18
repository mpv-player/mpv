-- Test script for property change notification mechanism.
-- Note that watching/reading some properties can be very expensive, or
-- require the player to synchronously wait on network (when playing
-- remote files), so you should in general only watch properties you
-- are interested in.

local utils = require("mp.utils")

function observe(name)
    mp.observe_property(name, "native", function(name, val)
        print("property '" .. name .. "' changed to '" ..
              utils.to_string(val) .. "'")
    end)
end

for i,name in ipairs(mp.get_property_native("property-list")) do
    observe(name)
end

for i,name in ipairs(mp.get_property_native("options")) do
    observe("options/" .. name)
end
