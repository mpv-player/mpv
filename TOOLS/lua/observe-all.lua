-- Test script for property change notification mechanism.

function format_property_val(v)
    if type(v) == "table" then
        return mp.format_table(v) -- undocumented function; might be removed
    else
        return tostring(v)
    end
end

for i,name in ipairs(mp.get_property_native("property-list")) do
    mp.observe_property(name, "native", function(name, val)
        print("property '" .. name .. "' changed to '" ..
              format_property_val(val) .. "'")
    end)
end
