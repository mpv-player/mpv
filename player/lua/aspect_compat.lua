-- Temporary compatibility script for video-aspect-override handling

local msg = require 'mp.msg'

mp.observe_property("video-aspect-override", "native", function (_, value)
    if value == 0 then
        msg.warn("Overriding --video-aspect-method to ignore. "
              .. "This will be removed in the future.")
        mp.set_property_native("video-aspect-method", "ignore")
    elseif value == -1 then
        if mp.get_property("video-aspect-method") == "ignore" then
            msg.warn("Overriding --video-aspect-method to container. "
                  .. "This will be removed in the future.")
            mp.set_property_native("video-aspect-method", "container")
        end
    end
end)
