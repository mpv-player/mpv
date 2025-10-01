#!/usr/bin/env lua

-- Test the path expansion function from ytdl_hook.lua

-- Simulate platform_is_windows function
local function platform_is_windows()
    return false -- We're on macOS
end

-- Expand paths that start with "~/" to the user's home directory
local function expand_user_path(path)
    if not path then
        return nil
    end
    
    -- Return as-is if it doesn't start with "~/" or "~\\"
    if path:sub(1, 2) ~= "~/" and path:sub(1, 2) ~= "~\\" then
        return path
    end
    
    local home = os.getenv("HOME")
    if not home and platform_is_windows() then
        home = os.getenv("USERPROFILE")
    end
    
    if not home then
        return path
    end
    
    -- Replace ~/ with the home directory
    return home .. path:sub(2)
end

-- Test cases
print("Testing path expansion function:")
print()

local test_cases = {
    {"~/.local/bin/yt-dlp", "Should expand to home/.local/bin/yt-dlp"},
    {"~/Documents/test", "Should expand to home/Documents/test"},
    {"/absolute/path", "Should remain unchanged"},
    {"relative/path", "Should remain unchanged"},
    {"~\\windows\\path", "Should expand on Windows (but not on macOS)"},
    {nil, "Should handle nil gracefully"},
    {"", "Should handle empty string"},
}

for _, test in ipairs(test_cases) do
    local input = test[1]
    local description = test[2]
    local result = expand_user_path(input)
    
    print(string.format("Input: %s", input or "nil"))
    print(string.format("Result: %s", result or "nil"))
    print(string.format("Description: %s", description))
    print()
end

print("Expected HOME directory:", os.getenv("HOME"))