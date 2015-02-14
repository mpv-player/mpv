local utils = require("mp.utils")

mp.observe_property("audio-device-list", "native", function(name, val)
    print("Audio device list changed:")
    for index, e in ipairs(val) do
        print("  - '" .. e.name .. "' (" .. e.description .. ")")
    end
end)

mp.observe_property("audio-out-detected-device", "native", function(name, val)
    print("Detected audio device changed:")
    print("  - '" .. val)
end)
