-- Compatibility shim for lua 5.2/5.3
-- luacheck: globals unpack
unpack = unpack or table.unpack -- luacheck: globals table.unpack

-- these are used internally by lua.c
mp.UNKNOWN_TYPE.info = "this value is inserted if the C type is not supported"
mp.UNKNOWN_TYPE.type = "UNKNOWN_TYPE"

mp.ARRAY.info = "native array"
mp.ARRAY.type = "ARRAY"

mp.MAP.info = "native map"
mp.MAP.type = "MAP"

function mp.get_script_name()
    return mp.script_name
end

function mp.get_opt(key, def)
    local opts = mp.get_property_native("options/script-opts")
    local val = opts[key]
    if val == nil then
        val = def
    end
    return val
end

function mp.input_define_section(section, contents, flags)
    if flags == nil or flags == "" then
        flags = "default"
    end
    mp.commandv("define-section", section, contents, flags)
end

function mp.input_enable_section(section, flags)
    if flags == nil then
        flags = ""
    end
    mp.commandv("enable-section", section, flags)
end

function mp.input_disable_section(section)
    mp.commandv("disable-section", section)
end

function mp.get_mouse_pos()
    local m = mp.get_property_native("mouse-pos")
    return m.x, m.y
end

-- For dispatching script-binding. This is sent as:
--      script-message-to $script_name $binding_name $keystate
-- The array is indexed by $binding_name, and has functions like this as value:
--      fn($binding_name, $keystate)
local dispatch_key_bindings = {}

local message_id = 0
local function reserve_binding()
    message_id = message_id + 1
    return "__keybinding" .. tostring(message_id)
end

local function dispatch_key_binding(name, state, key_name, key_text, scale, arg)
    local fn = dispatch_key_bindings[name]
    if fn then
        fn(name, state, key_name, key_text, scale, arg)
    end
end

-- "Old", deprecated API

-- each script has its own section, so that they don't conflict
local default_section = "input_dispatch_" .. mp.script_name

-- Set the list of key bindings. These will override the user's bindings, so
-- you should use this sparingly.
-- A call to this function will remove all bindings previously set with this
-- function. For example, set_key_bindings({}) would remove all script defined
-- key bindings.
-- Note: the bindings are not active by default. Use enable_key_bindings().
--
-- list is an array of key bindings, where each entry is an array as follow:
--      {key, callback_press, callback_down, callback_up}
-- key is the key string as used in input.conf, like "ctrl+a"
--
-- callback can be a string too, in which case the following will be added like
-- an input.conf line: key .. " " .. callback
-- (And callback_down is ignored.)
function mp.set_key_bindings(list, section, flags)
    local cfg = ""
    for i = 1, #list do
        local entry = list[i]
        local key = entry[1]
        local cb = entry[2]
        local cb_down = entry[3]
        local cb_up = entry[4]
        if type(cb) ~= "string" then
            local mangle = reserve_binding()
            dispatch_key_bindings[mangle] = function(_, state)
                local event = state:sub(1, 1)
                local is_mouse = state:sub(2, 2) == "m"
                local def = (is_mouse and "u") or "d"
                if event == "r" then
                    return
                end
                if event == "p" and cb then
                    cb()
                elseif event == "d" and cb_down then
                    cb_down()
                elseif event == "u" and cb_up then
                    cb_up()
                elseif event == def and cb then
                    cb()
                end
            end
            cfg = cfg .. key .. " script-binding " ..
                  mp.script_name .. "/" .. mangle .. "\n"
        else
            cfg = cfg .. key .. " " .. cb .. "\n"
        end
    end
    mp.input_define_section(section or default_section, cfg, flags)
end

function mp.enable_key_bindings(section, flags)
    mp.input_enable_section(section or default_section, flags)
end

function mp.disable_key_bindings(section)
    mp.input_disable_section(section or default_section)
end

function mp.set_mouse_area(x0, y0, x1, y1, section)
    mp.input_set_section_mouse_area(section or default_section, x0, y0, x1, y1)
end

-- "Newer" and more convenient API

local key_bindings = {}
local key_binding_counter = 0
local key_bindings_dirty = false

function mp.flush_keybindings()
    if not key_bindings_dirty then
        return
    end
    key_bindings_dirty = false

    for i = 1, 2 do
        local section, flags
        local def = i == 1
        if def then
            section = "input_" .. mp.script_name
            flags = "default"
        else
            section = "input_forced_" .. mp.script_name
            flags = "force"
        end
        local bindings = {}
        for _, v in pairs(key_bindings) do
            if v.bind and v.forced ~= def then
                bindings[#bindings + 1] = v
            end
        end
        table.sort(bindings, function(a, b)
            return a.priority < b.priority
        end)
        local cfg = ""
        for _, v in ipairs(bindings) do
            cfg = cfg .. v.bind .. "\n"
        end
        mp.input_define_section(section, cfg, flags)
        mp.input_enable_section(section, "allow-hide-cursor+allow-vo-dragging")
    end
end

local function add_binding(attrs, key, name, fn, rp)
    if type(name) ~= "string" and name ~= nil then
        rp = fn
        fn = name
        name = nil
    end
    rp = rp or ""
    if name == nil then
        name = reserve_binding()
    end
    local repeatable = rp == "repeatable" or rp["repeatable"]
    local scalable = rp == "scalable" or rp["scalable"]
    if rp["forced"] then
        attrs.forced = true
    end
    local key_cb, msg_cb
    if not fn then
        fn = function() end
    end
    if rp["complex"] then
        local key_states = {
            ["u"] = "up",
            ["d"] = "down",
            ["r"] = "repeat",
            ["p"] = "press",
        }
        key_cb = function(_, state, key_name, key_text, scale, arg)
            if key_text == "" then
                key_text = nil
            end
            fn({
                event = key_states[state:sub(1, 1)] or "unknown",
                is_mouse = state:sub(2, 2) == "m",
                canceled = state:sub(3, 3) == "c",
                key_name = key_name,
                key_text = key_text,
                scale = tonumber(scale),
                arg = arg,
            })
        end
        msg_cb = function()
            fn({event = "press", is_mouse = false})
        end
    else
        key_cb = function(_, state)
            -- Emulate the same semantics as input.c uses for most bindings:
            -- For keyboard, "down" runs the command, "up" does nothing;
            -- for mouse, "down" does nothing, "up" runs the command.
            -- Also, key repeat triggers the binding again.
            local event = state:sub(1, 1)
            local is_mouse = state:sub(2, 2) == "m"
            local canceled = state:sub(3, 3) == "c"
            if canceled or event == "r" and not repeatable then
                return
            end
            if is_mouse and (event == "u" or event == "p") then
                fn()
            elseif not is_mouse and (event == "d" or event == "r" or event == "p") then
                fn()
            end
        end
        msg_cb = fn
    end
    local prefix = scalable and "" or " nonscalable"
    if key and #key > 0 then
        attrs.bind = key .. prefix .. " script-binding " .. mp.script_name .. "/" .. name
    end
    attrs.name = name
    -- new bindings override old ones (but do not overwrite them)
    key_binding_counter = key_binding_counter + 1
    attrs.priority = key_binding_counter
    key_bindings[name] = attrs
    key_bindings_dirty = true
    dispatch_key_bindings[name] = key_cb
    mp.register_script_message(name, msg_cb)
end

function mp.add_key_binding(...)
    add_binding({forced=false}, ...)
end

function mp.add_forced_key_binding(...)
    add_binding({forced=true}, ...)
end

function mp.remove_key_binding(name)
    key_bindings[name] = nil
    dispatch_key_bindings[name] = nil
    key_bindings_dirty = true
    mp.unregister_script_message(name)
end

local timers = {}

local timer_mt = {}
timer_mt.__index = timer_mt

function mp.add_timeout(seconds, cb, disabled)
    local t = mp.add_periodic_timer(seconds, cb, disabled)
    t.oneshot = true
    return t
end

function mp.add_periodic_timer(seconds, cb, disabled)
    local t = {
        timeout = seconds,
        cb = cb,
        oneshot = false,
    }
    setmetatable(t, timer_mt)
    if not disabled then
        t:resume()
    end
    return t
end

function timer_mt.stop(t)
    if timers[t] then
        timers[t] = nil
        t.next_deadline = t.next_deadline - mp.get_time()
    end
end

function timer_mt.kill(t)
    timers[t] = nil
    t.next_deadline = nil
end
mp.cancel_timer = timer_mt.kill

function timer_mt.resume(t)
    if not timers[t] then
        local timeout = t.next_deadline
        if timeout == nil then
            timeout = t.timeout
        end
        t.next_deadline = mp.get_time() + timeout
        timers[t] = t
    end
end

function timer_mt.is_enabled(t)
    return timers[t] ~= nil
end

-- Return the timer that expires next.
local function get_next_timer()
    local best = nil
    for t, _ in pairs(timers) do
        if best == nil or t.next_deadline < best.next_deadline then
            best = t
        end
    end
    return best
end

function mp.get_next_timeout()
    local timer = get_next_timer()
    if not timer then
        return
    end
    local now = mp.get_time()
    return timer.next_deadline - now
end

-- Run timers that have met their deadline at the time of invocation.
-- Return: time>0 in seconds till the next due timer, 0 if there are due timers
--         (aborted to avoid infinite loop), or nil if no timers
local function process_timers()
    local t0 = nil
    while true do
        local timer = get_next_timer()
        if not timer then
            return
        end
        local now = mp.get_time()
        local wait = timer.next_deadline - now
        if wait > 0 then
            return wait
        else
            if not t0 then
                t0 = now  -- first due callback: always executes, remember t0
            elseif timer.next_deadline > t0 then
                -- don't block forever with slow callbacks and endless timers.
                -- we'll continue right after checking mpv events.
                return 0
            end

            if timer.oneshot then
                timer:kill()
            else
                timer.next_deadline = now + timer.timeout
            end
            timer.cb()
        end
    end
end

local messages = {}

function mp.register_script_message(name, fn)
    messages[name] = fn
end

function mp.unregister_script_message(name)
    messages[name] = nil
end

local function message_dispatch(ev)
    if #ev.args > 0 then
        local handler = messages[ev.args[1]]
        if handler then
            handler(unpack(ev.args, 2))
        end
    end
end

local property_id = 0
local properties = {}

function mp.observe_property(name, t, cb)
    local id = property_id + 1
    property_id = id
    properties[id] = cb
    mp.raw_observe_property(id, name, t)
end

function mp.unobserve_property(cb)
    for prop_id, prop_cb in pairs(properties) do
        if cb == prop_cb then
            properties[prop_id] = nil
            mp.raw_unobserve_property(prop_id)
        end
    end
end

local function property_change(ev)
    local prop = properties[ev.id]
    if prop then
        prop(ev.name, ev.data)
    end
end

-- used by default event loop (mp_event_loop()) to decide when to quit
mp.keep_running = true

function _G.exit()
    mp.keep_running = false
end

local event_handlers = {}

function mp.register_event(name, cb)
    local list = event_handlers[name]
    if not list then
        list = {}
        event_handlers[name] = list
    end
    list[#list + 1] = cb
    return mp.request_event(name, true)
end

function mp.unregister_event(cb)
    for name, sub in pairs(event_handlers) do
        local found = false
        for _, e in ipairs(sub) do
            if e == cb then
                found = true
                break
            end
        end
        if found then
            -- create a new array, just in case this function was called
            -- from an event handler
            local new = {}
            for i = 1, #sub do
                if sub[i] ~= cb then
                    new[#new + 1] = sub[i]
                end
            end
            event_handlers[name] = new
            if #new == 0 then
                mp.request_event(name, false)
            end
        end
    end
end

-- default handlers
mp.register_event("shutdown", exit)
mp.register_event("client-message", message_dispatch)
mp.register_event("property-change", property_change)

-- called before the event loop goes back to sleep
local idle_handlers = {}

function mp.register_idle(cb)
    idle_handlers[#idle_handlers + 1] = cb
end

function mp.unregister_idle(cb)
    local new = {}
    for _, handler in ipairs(idle_handlers) do
        if handler ~= cb then
            new[#new + 1] = handler
        end
    end
    idle_handlers = new
end

-- sent by "script-binding"
mp.register_script_message("key-binding", dispatch_key_binding)

function mp.log(level, ...)
    local cmd = {"msg", level}
    for i = 1, select("#", ...) do
        cmd[#cmd + 1] = tostring(select(i, ...))
    end
    mp.command_native(cmd)
end

mp.msg = {
    log = mp.log,
    fatal = function(...) return mp.log("fatal", ...) end,
    error = function(...) return mp.log("error", ...) end,
    warn = function(...) return mp.log("warn", ...) end,
    info = function(...) return mp.log("info", ...) end,
    verbose = function(...) return mp.log("v", ...) end,
    debug = function(...) return mp.log("debug", ...) end,
    trace = function(...) return mp.log("trace", ...) end,
}

_G.print = mp.msg.info

package.loaded["mp"] = mp
package.loaded["mp.msg"] = mp.msg

function mp.wait_event(t)
    local r = mp.raw_wait_event(t)
    if r and r.file_error and not r.error then
        -- compat; deprecated
        r.error = r.file_error
    end
    return r
end

_G.mp_event_loop = function()
    mp.dispatch_events(true)
end

local function call_event_handlers(e)
    local handlers = event_handlers[e.event]
    if handlers then
        for _, handler in ipairs(handlers) do
            handler(e)
        end
    end
end

mp.use_suspend = false

local suspend_warned = false

function mp.dispatch_events(allow_wait)
    local more_events = true
    if mp.use_suspend then
        if not suspend_warned then
            mp.msg.error("mp.use_suspend is now ignored.")
            suspend_warned = true
        end
    end
    while mp.keep_running do
        local wait = 0
        if not more_events then
            wait = process_timers() or 1e20 -- infinity for all practical purposes
            if wait ~= 0 then
                local idle_called = nil
                for _, handler in ipairs(idle_handlers) do
                    idle_called = true
                    handler()
                end
                if idle_called then
                    -- handlers don't complete in 0 time, and may modify timers
                    wait = mp.get_next_timeout() or 1e20
                    if wait < 0 then
                        wait = 0
                    end
                end
            end
            if allow_wait ~= true then
                return
            end
        end
        local e = mp.wait_event(wait)
        more_events = false
        if e.event ~= "none" then
            call_event_handlers(e)
            more_events = true
        end
    end
end

mp.register_idle(mp.flush_keybindings)

-- additional helpers

function mp.osd_message(text, duration)
    if not duration then
        duration = "-1"
    else
        duration = tostring(math.floor(duration * 1000))
    end
    mp.commandv("show-text", text, duration)
end

local hook_table = {}

local hook_mt = {}
hook_mt.__index = hook_mt

function hook_mt.cont(t)
    if t._id == nil then
        mp.msg.error("hook already continued")
    else
        mp.raw_hook_continue(t._id)
        t._id = nil
    end
end

function hook_mt.defer(t)
    t._defer = true
end

mp.register_event("hook", function(ev)
    local fn = hook_table[tonumber(ev.id)]
    local hookobj = {
        _id = ev.hook_id,
        _defer = false,
    }
    setmetatable(hookobj, hook_mt)
    if fn then
        fn(hookobj)
    end
    if not hookobj._defer and hookobj._id ~= nil then
        hookobj:cont()
    end
end)

function mp.add_hook(name, pri, cb)
    local id = #hook_table + 1
    hook_table[id] = cb
    -- The C API suggests using 0 for a neutral priority, but lua.rst suggests
    -- 50 (?), so whatever.
    mp.raw_hook_add(id, name, pri - 50)
end

local async_call_table = {}
local async_next_id = 1

function mp.command_native_async(node, cb)
    local id = async_next_id
    async_next_id = async_next_id + 1
    cb = cb or function() end
    local res, err = mp.raw_command_native_async(id, node)
    if not res then
        mp.add_timeout(0, function() cb(false, nil, err) end)
        return res, err
    end
    local t = {cb = cb, id = id}
    async_call_table[id] = t
    return t
end

mp.register_event("command-reply", function(ev)
    local id = tonumber(ev.id)
    local t = async_call_table[id]
    local cb = t.cb
    t.id = nil
    async_call_table[id] = nil
    if ev.error then
        cb(false, nil, ev.error)
    else
        cb(true, ev.result, nil)
    end
end)

function mp.abort_async_command(t)
    if t.id ~= nil then
        mp.raw_abort_async_command(t.id)
    end
end

local overlay_mt = {}
overlay_mt.__index = overlay_mt
local overlay_new_id = 0

function mp.create_osd_overlay(format)
    overlay_new_id = overlay_new_id + 1
    local overlay = {
        format = format,
        id = overlay_new_id,
        data = "",
        res_x = 0,
        res_y = 720,
    }
    setmetatable(overlay, overlay_mt)
    return overlay
end

function overlay_mt.update(ov)
    local cmd = {}
    for k, v in pairs(ov) do
        cmd[k] = v
    end
    cmd.name = "osd-overlay"
    cmd.res_x = math.floor(cmd.res_x)
    cmd.res_y = math.floor(cmd.res_y)
    return mp.command_native(cmd)
end

function overlay_mt.remove(ov)
    mp.command_native {
        name = "osd-overlay",
        id = ov.id,
        format = "none",
        data = "",
    }
end

-- legacy API
function mp.set_osd_ass(res_x, res_y, data)
    if not mp._legacy_overlay then
        mp._legacy_overlay = mp.create_osd_overlay("ass-events")
    end
    if mp._legacy_overlay.res_x ~= res_x or
       mp._legacy_overlay.res_y ~= res_y or
       mp._legacy_overlay.data ~= data
    then
        mp._legacy_overlay.res_x = res_x
        mp._legacy_overlay.res_y = res_y
        mp._legacy_overlay.data = data
        mp._legacy_overlay:update()
    end
end

function mp.get_osd_size()
    local prop = mp.get_property_native("osd-dimensions")
    return prop.w, prop.h, prop.aspect
end

function mp.get_osd_margins()
    local prop = mp.get_property_native("osd-dimensions")
    return prop.ml, prop.mt, prop.mr, prop.mb
end

local mp_utils = package.loaded["mp.utils"]

function mp_utils.format_table(t, set)
    if not set then
        set = { [t] = true }
    end
    local res = "{"
    -- pretty expensive but simple way to distinguish array and map parts of t
    local keys = {}
    local vals = {}
    local arr = 0
    for i = 1, #t do
        if t[i] == nil then
            break
        end
        keys[i] = i
        vals[i] = t[i]
        arr = i
    end
    for k, v in pairs(t) do
        if not (type(k) == "number" and k >= 1 and k <= arr and keys[k]) then
            keys[#keys + 1] = k
            vals[#keys] = v
        end
    end
    for i = 1, #keys do
        if #res > 1 then
            res = res .. ", "
        end
        if i > arr then
            res = res .. mp_utils.to_string(keys[i], set) .. " = "
        end
        res = res .. mp_utils.to_string(vals[i], set)
    end
    res = res .. "}"
    return res
end

function mp_utils.to_string(v, set)
    if type(v) == "string" then
        return "\"" .. v .. "\""
    elseif type(v) == "table" then
        if set then
            if set[v] then
                return "[cycle]"
            end
            set[v] = true
        end
        return mp_utils.format_table(v, set)
    else
        return tostring(v)
    end
end

function mp_utils.getcwd()
    return mp.get_property("working-directory")
end

function mp_utils.getpid()
    return mp.get_property_number("pid")
end

function mp_utils.format_bytes_humanized(b)
    local d = {"Bytes", "KiB", "MiB", "GiB", "TiB", "PiB"}
    local i = 1
    while b >= 1024 do
        b = b / 1024
        i = i + 1
    end
    return string.format("%0.2f %s", b, d[i] and d[i] or "*1024^" .. (i-1))
end

function mp_utils.subprocess(t)
    local cmd = {}
    cmd.name = "subprocess"
    cmd.capture_stdout = true
    for k, v in pairs(t) do
        if k == "cancellable" then
            k = "playback_only"
        elseif k == "max_size" then
            k = "capture_size"
        end
        cmd[k] = v
    end
    local res, err = mp.command_native(cmd)
    if res == nil then
        -- an error usually happens only if parsing failed (or no args passed)
        res = {error_string = err, status = -1}
    end
    if res.error_string ~= "" then
        res.error = res.error_string
    end
    return res
end

function mp_utils.subprocess_detached(t)
    mp.commandv("run", unpack(t.args))
end

return {}
