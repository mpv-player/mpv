-- Note: anything global is accessible by profile condition expressions.

local utils = require 'mp.utils'
local msg = require 'mp.msg'

local profiles = {}
local watched_properties = {}       -- indexed by property name (used as a set)
local cached_properties = {}        -- property name -> last known raw value
local properties_to_profiles = {}   -- property name -> set of profiles using it
local have_dirty_profiles = false   -- at least one profile is marked dirty
local pending_hooks = {}            -- as set (keys only, meaningless values)

-- Used during evaluation of the profile condition, and should contain the
-- profile the condition is evaluated for.
local current_profile = nil

-- Cached set of all top-level mpv properities. Only used for extra validation.
local property_set = {}
for _, property in pairs(mp.get_property_native("property-list")) do
    property_set[property] = true
end

local function evaluate(profile)
    msg.verbose("Re-evaluating auto profile " .. profile.name)

    current_profile = profile
    local status, res = pcall(profile.cond)
    current_profile = nil

    if not status then
        -- errors can be "normal", e.g. in case properties are unavailable
        msg.verbose("Profile condition error on evaluating: " .. res)
        res = false
    end
    res = not not res
    if res ~= profile.status then
        if res == true then
            msg.info("Applying auto profile: " .. profile.name)
            mp.commandv("apply-profile", profile.name)
        elseif profile.status == true and profile.has_restore_opt then
            msg.info("Restoring profile: " .. profile.name)
            mp.commandv("apply-profile", profile.name, "restore")
        end
    end
    profile.status = res
    profile.dirty = false
end

local function on_property_change(name, val)
    cached_properties[name] = val
    -- Mark all profiles reading this property as dirty, so they get re-evaluated
    -- the next time the script goes back to sleep.
    local dependent_profiles = properties_to_profiles[name]
    if dependent_profiles then
        for profile, _ in pairs(dependent_profiles) do
            assert(profile.cond) -- must be a profile table
            profile.dirty = true
            have_dirty_profiles = true
        end
    end
end

local function on_idle()
    -- When events and property notifications stop, re-evaluate all dirty profiles.
    if have_dirty_profiles then
        for _, profile in ipairs(profiles) do
            if profile.dirty then
                evaluate(profile)
            end
        end
    end
    have_dirty_profiles = false
    -- Release all hooks (the point was to wait until an idle event)
    while true do
        local h = next(pending_hooks)
        if not h then
            break
        end
        pending_hooks[h] = nil
        h:cont()
    end
end

local function on_hook(h)
    h:defer()
    pending_hooks[h] = true
end

function get(name, default)
    -- Normally, we use the cached value only
    if not watched_properties[name] then
        watched_properties[name] = true
        local res, err = mp.get_property_native(name)
        -- Property has to not exist and the toplevel of property in the name must also
        -- not have an existing match in the property set for this to be considered an error.
        -- This allows things like user-data/test to still work.
        if err == "property not found" and property_set[name:match("^([^/]+)")] == nil then
            msg.error("Property '" .. name .. "' was not found.")
            return default
        end
        cached_properties[name] = res
        mp.observe_property(name, "native", on_property_change)
    end
    -- The first time the property is read we need add it to the
    -- properties_to_profiles table, which will be used to mark the profile
    -- dirty if a property referenced by it changes.
    if current_profile then
        local map = properties_to_profiles[name]
        if not map then
            map = {}
            properties_to_profiles[name] = map
        end
        map[current_profile] = true
    end
    local val = cached_properties[name]
    if val == nil then
        val = default
    end
    return val
end

local function magic_get(name)
    -- Lua identifiers can't contain "-", so in order to match with mpv
    -- property conventions, replace "_" to "-"
    name = string.gsub(name, "_", "-")
    return get(name, nil)
end

local evil_magic = {}
setmetatable(evil_magic, {
    __index = function(table, key)
        -- interpret everything as property, unless it already exists as
        -- a non-nil global value
        local v = _G[key]
        if type(v) ~= "nil" then
            return v
        end
        return magic_get(key)
    end,
})

p = {}
setmetatable(p, {
    __index = function(table, key)
        return magic_get(key)
    end,
})

local function compile_cond(name, s)
    local code, chunkname = "return " .. s, "profile " .. name .. " condition"
    local chunk, err
    if setfenv then -- lua 5.1
        chunk, err = loadstring(code, chunkname)
        if chunk then
            setfenv(chunk, evil_magic)
        end
    else -- lua 5.2
        chunk, err = load(code, chunkname, "t", evil_magic)
    end
    if not chunk then
        msg.error("Profile '" .. name .. "' condition: " .. err)
        chunk = function() return false end
    end
    return chunk
end

local function load_profiles()
    for i, v in ipairs(mp.get_property_native("profile-list")) do
        local cond = v["profile-cond"]
        if cond and #cond > 0 then
            local profile = {
                name = v.name,
                cond = compile_cond(v.name, cond),
                properties = {},
                status = nil,
                dirty = true, -- need re-evaluate
                has_restore_opt = v["profile-restore"] and v["profile-restore"] ~= "default"
            }
            profiles[#profiles + 1] = profile
            have_dirty_profiles = true
        end
    end
end

load_profiles()

if #profiles < 1 and mp.get_property("load-auto-profiles") == "auto" then
    -- make it exit immediately
    _G.mp_event_loop = function() end
    return
end

mp.register_idle(on_idle)
for _, name in ipairs({"on_load", "on_preloaded", "on_before_start_file"}) do
    mp.add_hook(name, 50, on_hook)
end

on_idle() -- re-evaluate all profiles immediately
