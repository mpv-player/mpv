local utils = require("mp.utils")

mp.observe_property("audio-device-list", "native", function(name, val)
    print("Audio device list changed:")
    for index, e in ipairs(val) do
        print("  - '" .. e.name .. "' (" .. e.description .. ")")
    end
end)
