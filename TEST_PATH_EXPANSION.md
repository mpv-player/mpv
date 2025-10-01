# Path Expansion Fix for ytdl_hook.lua

## Issue Description
The ytdl_hook in mpv doesn't properly expand paths that start with `~/` on macOS, causing the script to fail when users specify paths like `~/.local/bin/yt-dlp` in their ytdl_path configuration.

## Root Cause
The ytdl_hook.lua script was not expanding tilde (`~`) paths to absolute paths before attempting to find the ytdl executable. This worked on some systems where the shell or other components handled the expansion, but failed on macOS.

## Fix Implemented
1. Added a `expand_user_path()` function that:
   - Detects paths starting with `~/` or `~\\` (for Windows)
   - Replaces the tilde with the appropriate home directory from `$HOME` or `%USERPROFILE%`
   - Returns unchanged paths that don't need expansion

2. Modified the path search logic to:
   - Expand user paths before attempting to locate the ytdl executable
   - Try expanded paths first if they differ from the original
   - Fall back to the original path in PATH if the expanded path doesn't work

## Testing Instructions

### Manual Testing
To test this fix:

1. **Build mpv with the modified ytdl_hook.lua**
2. **Create a test configuration** in `~/.config/mpv/mpv.conf`:
   ```
   script-opts-add=ytdl_hook-ytdl_path="~/.local/bin/yt-dlp"
   ```
3. **Install yt-dlp** in that location:
   ```bash
   pip install --user yt-dlp
   # or
   uv tool install yt-dlp
   ```
4. **Test with a YouTube URL**:
   ```bash
   mpv "https://www.youtube.com/watch?v=C0DPdy98e4c"
   ```

### Expected Behavior
- **Before fix**: Error message "youtube-dl failed: not found or not enough permissions"
- **After fix**: Should successfully locate and use yt-dlp from the expanded path

### Test Cases Covered
1. `~/.local/bin/yt-dlp` → `/Users/username/.local/bin/yt-dlp`
2. `~/bin/youtube-dl` → `/Users/username/bin/youtube-dl`
3. `/absolute/path` → `/absolute/path` (unchanged)
4. `relative/path` → `relative/path` (unchanged)
5. `yt-dlp` → `yt-dlp` (unchanged, searches in PATH)

### Error Handling
- Gracefully handles cases where HOME environment variable is not set
- Falls back to original behavior if path expansion fails
- Provides clear error messages in logs

## Verification
The fix ensures that:
- Tilde expansion works consistently across all platforms (Linux, macOS, Windows)
- Backward compatibility is maintained for existing configurations
- Path expansion is only performed when necessary (paths starting with `~`)
- The original search behavior is preserved as a fallback

## Files Modified
- `player/lua/ytdl_hook.lua`: Added `expand_user_path()` function and integrated it into the path search logic