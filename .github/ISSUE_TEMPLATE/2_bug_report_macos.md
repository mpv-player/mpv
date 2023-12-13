---
name: 'Report a macOS Issue'
about: 'Create a report for a runtime related macOS Issue'
title: ''
labels: 'os:mac'
assignees: ''

---

### Important Information

Provide following Information:
- mpv version
- macOS Version
- Source of the mpv binary or bundle
- If known which version of mpv introduced the problem
- Possible screenshot or video of visual glitches

If you're not using git master or the latest release, update.
Releases are listed here: https://github.com/mpv-player/mpv/releases

### Reproduction steps

Try to reproduce your issue with --no-config first. If it isn't reproducible
with --no-config try to first find out which option or script causes your issue.

Describe the reproduction steps as precise as possible. It's very likely that
the bug you experience wasn't reproduced by the developer because the workflow
differs from your own.

### Expected behavior

### Actual behavior

### Log file

Make a log file made with -v -v or --log-file=output.txt, attach it to
the issue, and replace this text with a link to it.

If you use the Bundle, a default log is created for your last run at
~/Library/Logs/mpv.log. You can jump to that file via the
Help > Show log File… menu.

Without the log file, this issue will be closed for ignoring the issue template.

In the case of a crash, please provide the macOS Crash Report (Backtrace).

### Sample files

Sample files needed to reproduce this issue can be attached to the issue
(preferred), or be uploaded to https://0x0.st/ or similar sites.
(Only needed if the issue cannot be reproduced without it.)
Do not use garbage like "cloud storage", especially not Google Drive.
