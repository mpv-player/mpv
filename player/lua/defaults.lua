
local callbacks = {}
-- each script has its own section, so that they don't conflict
local default_section = "input_" .. mp.script_name

-- Set the list of key bindings. These will override the user's bindings, so
-- you should use this sparingly.
-- A call to this function will remove all bindings previously set with this
-- function. For example, set_key_bindings({}) would remove all script defined
-- key bindings.
-- Note: the bindings are not active by default. Use enable_key_bindings().
--
-- list is an array of key bindings, where each entry is an array as follow:
--      {key, callback}
--      {key, callback, callback_down}
-- key is the key string as used in input.conf, like "ctrl+a"
-- callback is a Lua function that is called when the key binding is used.
-- callback_down can be given too, and is called when a mouse button is pressed
-- if the key is a mouse button. (The normal callback will be for mouse button
-- down.)
--
-- callback can be a string too, in which case the following will be added like
-- an input.conf line: key .. " " .. callback
-- (And callback_down is ignored.)
function mp.set_key_bindings(list, section)
    local cfg = ""
    for i = 1, #list do
        local entry = list[i]
        local key = entry[1]
        local cb = entry[2]
        local cb_down = entry[3]
        if type(cb) == "function" then
            callbacks[#callbacks + 1] = {press=cb, before_press=cb_down}
            cfg = cfg .. key .. " script_dispatch " .. mp.script_name
                  .. " " .. #callbacks .. "\n"
        else
            cfg = cfg .. key .. " " .. cb .. "\n"
        end
    end
    mp.input_define_section(section or default_section, cfg)
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

local function script_dispatch(event)
    local cb = callbacks[event.arg0]
    if cb then
        if event.type == "press" and cb.press then
            cb.press()
        elseif event.type == "keyup_follows" and cb.before_press then
            cb.before_press()
        end
    end
end

local timers = {}

-- Install a one-shot timer. Once the given amount of seconds has passed from
-- now, the callback will be called as cb(), and the timer is removed.
function mp.add_timeout(seconds, cb)
    local t = mp.add_periodic_timer(seconds, cb)
    t.oneshot = true
    return t
end

-- Install a periodic timer. It works like add_timeout(), but after cb() is
-- called, the timer is re-added.
function mp.add_periodic_timer(seconds, cb)
    local t = {
        timeout = seconds,
        cb = cb,
        oneshot = false,
        next_deadline = mp.get_timer() + seconds,
    }
    timers[t] = t
    return t
end

function mp.cancel_timer(t)
    if t then
        timers[t] = nil
    end
end

-- Return the timer that expires next.
local function get_next_timer()
    local best = nil
    for t, _ in pairs(timers) do
        if (best == nil) or (t.next_deadline < best.next_deadline) then
            best = t
        end
    end
    return best
end

-- Run timers that have met their deadline.
-- Return: next absolute time a timer expires as number, or nil if no timers
local function process_timers()
    while true do
        local timer = get_next_timer()
        if not timer then
            return
        end
        local wait = timer.next_deadline - mp.get_timer()
        if wait > 0 then
            return wait
        else
            if timer.oneshot then
                timers[timer] = nil
            end
            timer.cb()
            if not timer.oneshot then
                timer.next_deadline = mp.get_timer() + timer.timeout
            end
        end
    end
end

-- used by default event loop (mp_event_loop()) to decide when to quit
mp.keep_running = true

local event_handlers = {}

function mp.register_event(name, cb)
    event_handlers[name] = cb
    mp.request_event(name, true)
end

-- default handlers
mp.register_event("shutdown", function() mp.keep_running = false end)
mp.register_event("script-input-dispatch", script_dispatch)

mp.msg = {
    log = mp.log,
    fatal = function(...) return mp.log("fatal", ...) end,
    error = function(...) return mp.log("error", ...) end,
    warn = function(...) return mp.log("warn", ...) end,
    info = function(...) return mp.log("info", ...) end,
    verbose = function(...) return mp.log("v", ...) end,
    debug = function(...) return mp.log("debug", ...) end,
}

_G.print = mp.msg.info

package.loaded["mp"] = mp
package.loaded["mp.msg"] = mp.msg

_G.mp_event_loop = function()
    local more_events = true
    mp.suspend()
    while mp.keep_running do
        local wait = process_timers()
        if wait == nil then
            wait = 1e20 -- infinity for all practical purposes
        end
        if more_events then
            wait = 0
        end
        -- Resume playloop - important especially if an error happened while
        -- suspended, and the error was handled, but no resume was done.
        if wait > 0 then
            mp.resume("all")
        end
        local e = mp.wait_event(wait)
        -- Empty the event queue while suspended; otherwise, each
        -- event will keep us waiting until the core suspends again.
        mp.suspend()
        more_events = (e.event ~= "none")
        if more_events then
            local handler = event_handlers[e.event]
            if handler then
                handler(e)
            end
        end
    end
end

return {}
