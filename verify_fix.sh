#!/bin/bash

echo "=== Path Expansion Fix Verification ==="
echo "Current user: $(whoami)"
echo "HOME directory: $HOME"
echo ""

echo "=== Testing path expansion logic (simulated) ==="
echo "Test cases that would be handled by expand_user_path():"
echo ""

echo "1. Input: '~/.local/bin/yt-dlp'"
echo "   Should expand to: '$HOME/.local/bin/yt-dlp'"
echo ""

echo "2. Input: '~/bin/youtube-dl'"
echo "   Should expand to: '$HOME/bin/youtube-dl'"
echo ""

echo "3. Input: '/usr/local/bin/yt-dlp'"
echo "   Should remain: '/usr/local/bin/yt-dlp' (no expansion needed)"
echo ""

echo "4. Input: 'yt-dlp'"
echo "   Should remain: 'yt-dlp' (no expansion needed)"
echo ""

echo "=== Checking if yt-dlp exists in common locations ==="
locations=(
    "$HOME/.local/bin/yt-dlp"
    "/usr/local/bin/yt-dlp"
    "/opt/homebrew/bin/yt-dlp"
)

for location in "${locations[@]}"; do
    if [[ -f "$location" ]]; then
        echo "✓ Found yt-dlp at: $location"
    else
        echo "✗ Not found at: $location"
    fi
done

echo ""
echo "=== Testing basic path expansion manually ==="
test_path="~/.local/bin/yt-dlp"
expanded_path="${test_path/#\~/$HOME}"
echo "Manual expansion test:"
echo "  Input: $test_path"
echo "  Output: $expanded_path"

if [[ -f "$expanded_path" ]]; then
    echo "  Status: ✓ File exists"
    ls -la "$expanded_path"
else
    echo "  Status: ✗ File does not exist"
fi

echo ""
echo "=== Summary ==="
echo "The fix in ytdl_hook.lua should now properly handle paths starting with '~/' by:"
echo "1. Detecting when a path starts with '~/' or '~\\'"
echo "2. Expanding it using the HOME environment variable"
echo "3. Falling back to the original path if expansion fails"
echo "4. Trying both expanded and original paths during ytdl executable search"