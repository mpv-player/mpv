---
name: 'Report a Linux Issue'
about: 'Create a report for a runtime related Linux Issue'
title: ''
labels: 'os:linux'
assignees: ''

---

### Important Information

Provide following Information:
- mpv version
- Linux Distribution and Version
- Source of the mpv binary
- If known which version of mpv introduced the problem
- Window Manager and version
- GPU model, driver and version
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

Make a log file made with -v -v or --log-file=output.txt, paste it to
https://0x0.st/ or attach it to the github issue, and replace this text with a
link to it.

Without the log file, this issue will be closed for ignoring the issue template.

In the case of a crash, please provide a backtrace.

### Sample files

Sample files needed to reproduce this issue can be uploaded to https://0x0.st/
or similar sites. (Only needed if the issue cannot be reproduced without it.)
Do not use garbage like "cloud storage", especially not Google Drive.
