local utils = require("mp.utils")

function hardsleep()
    os.execute("sleep 1s")
end

local hooks = {"on_before_start_file", "on_load", "on_load_fail",
               "on_preloaded", "on_unload", "on_after_end_file"}

for _, name in ipairs(hooks) do
    mp.add_hook(name, 0, function()
        print("--- hook: " .. name)
        hardsleep()
        print("    ... continue")
    end)
end

local events = {"start-file", "end-file", "file-loaded", "seek",
                "playback-restart", "idle", "shutdown"}
for _, name in ipairs(events) do
    mp.register_event(name, function()
        print("--- event: " .. name)
    end)
end

local props = {"path", "metadata"}
for _, name in ipairs(props) do
    mp.observe_property(name, "native", function(prop, val)
        print("property '" .. prop .. "' changed to '" ..
              utils.to_string(val) .. "'")
    end)
end
