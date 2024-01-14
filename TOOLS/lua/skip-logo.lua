--[[

Automatically skip in files if video frames with pre-supplied fingerprints are
detected. This will skip ahead by a pre-configured amount of time if a matching
video frame is detected.

This requires the vf_fingerprint video filter to be compiled in. Read the
documentation of this filter for caveats (which will automatically apply to
this script as well), such as no support for zero-copy hardware decoding.

You need to manually gather and provide fingerprints for video frames and add
them to a configuration file in script-opts/skip-logo.conf (the "script-opts"
directory must be in the mpv configuration directory, typically ~/.config/mpv/).

Example script-opts/skip-logo.conf:


    cases = {
        {
            -- Skip ahead 10 seconds if a black frame was detected
            -- Note: this is dangerous non-sense. It's just for demonstration.
            name = "black frame",   -- print if matched
            skip = 10,              -- number of seconds to skip forward
            score = 0.3,            -- required score
            fingerprint = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
        },
        {
            -- Skip ahead 20 seconds if a white frame was detected
            -- Note: this is dangerous non-sense. It's just for demonstration.
            name = "fun2",
            skip = 20,
            fingerprint = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        },
    }

This is actually a lua file. Lua was chosen because it seemed less of a pain to
parse. Future versions of this script may change the format.

The fingerprint is a video frame, converted to "gray" (8 bit per pixels), full
range, each pixel concatenated into an array, converted to a hex string. You
can produce these fingerprints by running this manually:

   mpv --vf=fingerprint:print yourfile.mkv

This will log the fingerprint of each video frame to the console, along with its
timestamp. You find the fingerprint of a unique-enough looking frame, and add
it as entry to skip-logo.conf.

You can provide a score for "fuzziness". If no score is provided, a default
value of 0.3 is used. The score is inverse: 0 means exactly the same, while a
higher score means a higher difference. Currently, the score is computed as
euclidean distance between the video frame and the pre-provided fingerprint,
thus the highest score is 16. You probably want a score lower than 1 at least.
(This algorithm is very primitive, but also simple and fast to compute.)

There's always the danger of false positives, which might be quite annoying.
It's up to you what you hate more, the logo, or random skips if false positives
are detected. Also, it's always active, and might eat too much CPU with files
that have a high resolution or framerate. To temporarily disable the script,
having a keybind like this in your input.conf will be helpful:

    ctrl+k vf toggle @skip-logo

This will disable/enable the fingerprint filter, which the script automatically
adds at start.

Another important caveat is that the script currently disables matching during
seeking or playback initialization, which means it cannot match the first few
frames of a video. This could be fixed, but the author was too lazy to do so.

--]]

local utils = require "mp.utils"
local msg = require "mp.msg"

local label = "skip-logo"
local meta_property = string.format("vf-metadata/%s", label)

local config = {}
local cases = {}
local cur_bmp
local seeking = false
local playback_start_pts = nil

-- Convert a  hex string to an array. Convert each byte to a [0,1] float by
-- interpreting it as normalized uint8_t.
-- The data parameter, if not nil, may be used as storage (avoiding garbage).
local function hex_to_norm8(hex, data)
    local size = math.floor(#hex / 2)
    if #hex ~= size * 2 then
        return nil
    end
    local res
    if (data ~= nil) and (#data == size) then
        res = data
    else
        res = {}
    end
    for i = 1, size do
        local num = tonumber(hex:sub(i * 2, i * 2 + 1), 16)
        if num == nil then
            return nil
        end
        res[i] = num / 255.0
    end
    return res
end

local function compare_bmp(a, b)
    if #a ~= #b then
        return nil -- can't compare
    end
    local sum = 0
    for i = 1, #a do
        local diff = a[i] - b[i]
        sum = sum + diff * diff
    end
    return math.sqrt(sum)
end

local function load_config()
    local conf_file = mp.find_config_file("script-opts/skip-logo.conf")
    local conf_fn
    local err = nil
    if conf_file then
        if setfenv then
            conf_fn, err = loadfile(conf_file)
            if conf_fn then
                setfenv(conf_fn, config)
            end
        else
            conf_fn, err = loadfile(conf_file, "t", config)
        end
    else
        err = "config file not found"
    end

    if conf_fn and (not err) then
        local ok, err2 = pcall(conf_fn)
        err = err2
    end

    if err then
        msg.error("Failed to load config file:", err)
    end

    if config.cases then
        for n, case in ipairs(config.cases) do
            local err = nil
            case.bitmap = hex_to_norm8(case.fingerprint)
            if case.bitmap == nil then
                err = "invalid or missing fingerprint field"
            end
            if case.score == nil then
                case.score = 0.3
            end
            if type(case.score) ~= "number" then
                err = "score field is not a number"
            end
            if type(case.skip) ~= "number" then
                err = "skip field is not a number or missing"
            end
            if case.name == nil then
                case.name = ("Entry %d"):format(n)
            end
            if err == nil then
                cases[#cases + 1] = case
            else
                msg.error(("Entry %s: %s, ignoring."):format(case.name, err))
            end
        end
    end
end

load_config()

-- Returns true on match and if something was done.
local function check_fingerprint(hex, pts)
    local bmp = hex_to_norm8(hex, cur_bmp)
    cur_bmp = bmp

    -- If parsing the filter's result failed (well, it shouldn't).
    assert(bmp ~= nil, "filter returned nonsense")

    for _, case in ipairs(cases) do
        local score = compare_bmp(case.bitmap, bmp)
        if (score ~= nil) and (score <= case.score) then
            msg.warn(("Matching %s: score=%f (required: %f) at %s, skipping %f seconds"):
                format(case.name, score, case.score, mp.format_time(pts), case.skip))
            mp.commandv("seek", pts + case.skip, "absolute+exact")
            return true
        end
    end

    return false
end

local function read_frames()
    local result = mp.get_property_native(meta_property)
    if result == nil then
        return
    end

    -- Try to get all entries. Out of laziness, assume that there are at most
    -- 100 entries. (In fact, vf_fingerprint limits it to 10.)
    for i = 0, 99 do
        local prefix = string.format("fp%d.", i)
        local hex = result[prefix .. "hex"]

        local pts = tonumber(result[prefix .. "pts"])
        if (hex == nil) or (pts == nil) then
            break
        end

        local skip = false -- blame Lua for not having "continue" or "goto", not me

        -- If seeking just stopped, there will be frames before the seek target,
        -- ignore them by checking the timestamps.
        if playback_start_pts ~= nil then
            if pts >= playback_start_pts then
                playback_start_pts = nil -- just for robustness
            else
                skip = true
            end
        end

        if not skip then
            if check_fingerprint(hex, pts) then
                break
            end
        end
    end
end

mp.observe_property(meta_property, "native", function()
    -- Ignore frames that are decoded/filtered during seeking.
    if seeking then
        return
    end

    read_frames()
end)

mp.observe_property("seeking", "bool", function(name, val)
    seeking = val
    if seeking == false then
        playback_start_pts = mp.get_property_number("playback-time")
        read_frames()
    end
end)

local filters = mp.get_property_native("option-info/vf/choices", {})
local found = false
for _, f in ipairs(filters) do
    if f == "fingerprint" then
        found = true
        break
    end
end

if found then
    mp.command(("no-osd vf add @%s:fingerprint"):format(label, filter))
else
    msg.warn("vf_fingerprint not found")
end
