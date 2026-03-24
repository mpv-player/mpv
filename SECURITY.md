# Security Policy

## Supported Versions

Only the most recent stable release of mpv is supported with security updates.
Older releases are **not** maintained and will not receive patches.

## What Qualifies as a Security Issue

mpv is a desktop media player. It is not designed to be exposed to untrusted
environments. By design, mpv provides features such as loading user scripts,
running subprocesses, and controlling playback via IPC. These documented
mechanisms are not considered security vulnerabilities.

A security issue is a bug in mpv's own code that allows untrusted input to cause
harm beyond the expected scope of playback. Examples include:

- Exploitable memory corruption (buffer overflows, use-after-free, etc.) caused
  by crafted media files, subtitles, or playlists.
- Arbitrary code execution or unexpected file system access triggered by
  malicious input.
- Information disclosure (e.g. unintended local file reads) through crafted
  input.

The following are generally **not** considered security issues:

- Behavior resulting from documented features and interfaces.
- Denial of service (crashes, excessive memory or CPU usage) from crafted input.
  These are typically unavoidable when processing complex media formats and are
  treated as regular bugs.
- Bugs in third-party dependencies (FFmpeg, libass, libplacebo, etc.) that are
  not caused by mpv-specific usage. These should be reported to the respective
  upstream projects.

When in doubt, report it. We would rather triage a non-issue than miss a real
vulnerability.

## Reporting a Vulnerability

If you discover a security vulnerability in mpv, please report it responsibly
using **GitHub's private vulnerability reporting** feature:

1. Go to the [Security](https://github.com/mpv-player/mpv/security) tab of the
   mpv repository.
2. Click **"Report a vulnerability"**.
3. Fill in the advisory form with as much detail as possible, including steps to
   reproduce, affected versions, and potential impact.

This creates a private advisory visible only to you and our security team,
allowing us to discuss and address the issue before any public disclosure.

For more information on this process, see
[GitHub's documentation on private vulnerability reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing/privately-reporting-a-security-vulnerability).

## Direct Contact

If you are unable to use GitHub's reporting feature, you can reach out directly
to the security team:

- **@Akemi** - https://github.com/Akemi
- **@kasper93** - https://github.com/kasper93

## Disclosure Policy

- Please **do not** open public issues for security vulnerabilities.
- We aim to acknowledge reports promptly and work toward a fix in a reasonable
  timeframe.
