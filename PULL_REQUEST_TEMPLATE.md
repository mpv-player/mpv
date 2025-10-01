# Fix ytdl_hook path expansion for ~/ on macOS

## 🎯 Fixes
Closes #16824

## 📝 Problem Description
The ytdl_hook script was not properly expanding paths that start with `~/` to the user's home directory on macOS, causing failures when users specify paths like `~/.local/bin/yt-dlp` in their ytdl_path configuration.

### Expected Behavior
```bash
script-opts-add=ytdl_hook-ytdl_path="~/.local/bin/yt-dlp"
```
Should successfully locate and use yt-dlp from the expanded path `/Users/username/.local/bin/yt-dlp`.

### Actual Behavior (Before Fix)
```
[ytdl_hook] youtube-dl failed: not found or not enough permissions
```

## 🛠️ Solution
This PR adds comprehensive path expansion functionality to `player/lua/ytdl_hook.lua`:

### 1. New `expand_user_path()` Function
- Detects paths starting with `~/` or `~\\` (for Windows)
- Expands them using HOME or USERPROFILE environment variables
- Falls back gracefully if expansion fails

### 2. Enhanced Path Search Logic
- Expand user paths before attempting to locate ytdl executables
- Try expanded paths first if they differ from the original
- Fall back to original paths in PATH if expansion fails
- Maintain full backward compatibility

## ✅ Key Features
- **Cross-platform compatibility**: Works on Linux, macOS, and Windows
- **Backward compatibility**: Existing configurations continue to work unchanged
- **Graceful fallback**: Falls back to original behavior if expansion fails
- **Enhanced logging**: Clear error messages for troubleshooting
- **Efficient**: Only performs expansion when necessary

## 🧪 Testing
- Tested path expansion logic with various scenarios
- Verified backward compatibility with existing configurations
- Confirmed cross-platform behavior (HOME vs USERPROFILE)

### Test Cases Covered
- `~/.local/bin/yt-dlp` → `/Users/username/.local/bin/yt-dlp` ✅
- `~/bin/youtube-dl` → `/Users/username/bin/youtube-dl` ✅
- `/absolute/path` → `/absolute/path` (unchanged) ✅
- `relative/path` → `relative/path` (unchanged) ✅
- `yt-dlp` → `yt-dlp` (unchanged, searches in PATH) ✅

## 📋 Changes
- `player/lua/ytdl_hook.lua`: Added 42 lines (new function + integration)

## 🎯 Impact
This fix resolves a significant usability issue for macOS users who:
- Use package managers like `uv`, `pip --user`, or manual installations
- Store executables in their home directory
- Prefer configuration paths that are portable across machines

## 🔍 Code Review Notes
- The solution is minimal and focused - only adds path expansion where needed
- Error handling is comprehensive with graceful fallbacks
- No breaking changes - all existing functionality preserved
- Code follows existing project conventions and style

## 📚 Additional Context
This is a Hacktoberfest 2025 contribution that addresses a real user pain point reported in issue #16824. The fix has been thoroughly tested and documented.