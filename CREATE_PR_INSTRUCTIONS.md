# 🚀 Create Pull Request Instructions

## 📋 Steps to Create the Pull Request

### 1. Open Your Browser
Go to: **https://github.com/dollaransh17/mpv**

### 2. Click "Compare & pull request"
You should see a yellow banner at the top saying something like:
```
This branch is 1 commit ahead of mpv-player:master
```
Click the **"Compare & pull request"** button.

### 3. Set the Base Repository
- **base repository**: `mpv-player/mpv` 
- **base**: `master`
- **head repository**: `dollaransh17/mpv`
- **compare**: `master`

### 4. Fill in the Pull Request Details

**Title:**
```
ytdl_hook: Fix path expansion for ~/ on macOS
```

**Description:**
Copy and paste the content from `PULL_REQUEST_TEMPLATE.md` (the file we just created).

### 5. Review Your Changes
Scroll down to see the diff showing your changes to `player/lua/ytdl_hook.lua`:
- ✅ 42 lines added
- ✅ New `expand_user_path()` function
- ✅ Enhanced path search logic

### 6. Submit the Pull Request
Click **"Create pull request"**

## 🎯 Alternative: Direct Link Method

If you don't see the banner, you can also:
1. Go to: **https://github.com/mpv-player/mpv/compare**
2. Click "compare across forks"
3. Select:
   - **base fork**: `mpv-player/mpv`
   - **base**: `master`
   - **head fork**: `dollaransh17/mpv`
   - **compare**: `master`

## ✅ What Happens Next

After creating the PR:
1. **Automated checks** will run (CI/CD)
2. **Maintainers will review** your code
3. They may request changes or ask questions
4. Once approved, it will be **merged**!

## 🎊 Congratulations!

You'll have successfully contributed to mpv for Hacktoberfest 2025! 

Your PR addresses issue #16824 and provides a real solution to help macOS users who were struggling with path expansion in ytdl_hook configurations.

## 📝 PR Summary for Quick Reference
- **Issue**: #16824 - ytdl_hook path expansion
- **Solution**: Added `expand_user_path()` function 
- **Impact**: Fixes `~/.local/bin/yt-dlp` paths on macOS
- **Files changed**: `player/lua/ytdl_hook.lua` (+42 lines)
- **Status**: Ready for review ✅