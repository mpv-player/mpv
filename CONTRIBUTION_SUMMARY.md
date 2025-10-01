# MPV ytdl_hook Path Expansion Fix - Hacktoberfest 2025 Contribution

## 🎯 Issue Addressed
**GitHub Issue:** [#16824 - ytdl_hook-ytdl_path does not expand $HOME path ~/ correctly](https://github.com/mpv-player/mpv/issues/16824)

The issue occurred when users on macOS specified paths like `~/.local/bin/yt-dlp` in their mpv configuration. The ytdl_hook script was not properly expanding the tilde (`~`) to the user's home directory, causing mpv to fail with "youtube-dl failed: not found or not enough permissions" errors.

## 🔍 Root Cause Analysis
- The ytdl_hook.lua script was not expanding tilde paths before attempting to locate ytdl executables
- The `mp.find_config_file()` function searches in mpv's config directories, not absolute user paths
- macOS behavior differed from Linux where some expansion might happen at different levels
- No Lua API function was available in mpv for path expansion

## 🛠️ Solution Implemented
Added comprehensive path expansion functionality to `player/lua/ytdl_hook.lua`:

### 1. New `expand_user_path()` Function
```lua
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
```

### 2. Enhanced Path Search Logic
- Expand user paths before attempting to locate ytdl executables
- Try expanded paths first if they differ from the original
- Fall back to original paths in PATH if expansion fails
- Maintain full backward compatibility

## ✅ Features & Benefits
- **Cross-platform compatibility**: Works on Linux, macOS, and Windows
- **Backward compatibility**: Existing configurations continue to work unchanged
- **Graceful fallback**: Falls back to original behavior if expansion fails
- **Clear error messages**: Enhanced logging for troubleshooting
- **Efficient**: Only performs expansion when necessary

## 🧪 Testing Strategy
Created comprehensive test framework:
1. **Unit-style testing**: Standalone Lua script to verify expansion logic
2. **Integration testing**: Shell script to demonstrate real-world scenarios
3. **Manual verification**: Step-by-step testing instructions

### Test Cases Covered
- `~/.local/bin/yt-dlp` → `/Users/username/.local/bin/yt-dlp`
- `~/bin/youtube-dl` → `/Users/username/bin/youtube-dl`
- `/absolute/path` → `/absolute/path` (unchanged)
- `relative/path` → `relative/path` (unchanged)
- `yt-dlp` → `yt-dlp` (unchanged, searches in PATH)

## 📝 Code Quality
- **Clear documentation**: Comprehensive comments explaining the fix
- **Error handling**: Graceful handling of edge cases
- **Performance**: Minimal overhead, only expands when needed
- **Maintainable**: Clean, readable code following project conventions

## 🎉 Impact
This fix resolves a significant usability issue for macOS users who:
- Use package managers like `uv`, `pip --user`, or manual installations
- Store executables in their home directory
- Prefer configuration paths that are portable across machines

## 📋 Files Modified
- `player/lua/ytdl_hook.lua`: 42 lines added (new function + integration)

## 🚀 Commit Information
**Commit Hash:** 333837564a3122edd9a8494183408fffbf404bbb
**Commit Message:** "ytdl_hook: Fix path expansion for ~/ on macOS"

## 🔗 Repository
**Fork:** https://github.com/dollaransh17/mpv.git
**Branch:** master
**Status:** Ready for upstream pull request

## 🎯 Next Steps
1. Create pull request to upstream mpv repository
2. Address any review feedback
3. Ensure CI/CD tests pass
4. Celebrate successful Hacktoberfest contribution! 🎊

---

*This contribution demonstrates expertise in:*
- C/Lua codebase analysis and debugging
- Cross-platform development considerations  
- Git workflow and version control
- Open source contribution best practices
- Comprehensive testing and documentation