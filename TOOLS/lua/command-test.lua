-- Test script for some command API details.

local utils = require("mp.utils")

function join(sep, arr, count)
    local r = ""
    if count == nil then
        count = #arr
    end
    for i = 1, count do
        if i > 1 then
            r = r .. sep
        end
        r = r .. utils.to_string(arr[i])
    end
    return r
end

mp.observe_property("vo-configured", "bool", function(_, v)
    if v ~= true then
        return
    end

    print("async expand-text")
    mp.command_native_async({"expand-text", "hello ${path}!"},
        function(res, val, err)
            print("done async expand-text: " .. join(" ", {res, val, err}))
        end)

    -- make screenshot writing very slow
    mp.set_property("screenshot-format", "png")
    mp.set_property("screenshot-png-compression", "9")

    timer = mp.add_periodic_timer(0.1, function() print("I'm alive") end)
    timer:resume()

    print("Slow screenshot command...")
    res, err = mp.command_native({"screenshot"})
    print("done, res: " .. utils.to_string(res))

    print("Slow screenshot async command...")
    res, err = mp.command_native_async({"screenshot"}, function(res)
        print("done (async), res: " .. utils.to_string(res))
        timer:kill()
    end)
    print("done (sending), res: " .. utils.to_string(res))

    print("Broken screenshot async command...")
    mp.command_native_async({"screenshot-to-file", "/nonexistent/bogus.png"},
        function(res, val, err)
            print("done err scr.: " .. join(" ", {res, val, err}))
        end)

    mp.command_native_async({name = "subprocess", args = {"sh", "-c", "echo hi && sleep 10s"}, capture_stdout = true},
        function(res, val, err)
            print("done subprocess: " .. join(" ", {res, val, err}))
        end)

    local x = mp.command_native_async({name = "subprocess", args = {"sleep", "inf"}},
        function(res, val, err)
            print("done sleep inf subprocess: " .. join(" ", {res, val, err}))
        end)
    mp.add_timeout(15, function()
        print("aborting sleep inf subprocess after timeout")
        mp.abort_async_command(x)
    end)
end)
