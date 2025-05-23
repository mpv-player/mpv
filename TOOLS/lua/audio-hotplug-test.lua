mp.observe_property("audio-device-list", "native", function(_, val)
    print("Audio device list changed:")
    for _, e in ipairs(val) do
        print("  - '" .. e.name .. "' (" .. e.description .. ")")
    end
end)
