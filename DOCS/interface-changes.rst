Introduction
============

mpv provides access to its internal via the following means:

- options
- commands
- properties
- events

All of these are important for interfacing both with end users and API users
(which include Lua scripts, libmpv, and the JSON IPC). As such, they constitute
a large part of the user interface and APIs.

This document lists changes to them. New changes are added to the top.

Interface changes
=================

::

 --- mpv 0.10.0 will be released ---
    - deprecate audio-samplerate and audio-channels properties
      (audio-params sub-properties are the replacement)
    - add audio-params and audio-out-params properties
    - deprecate "audio-format" property, replaced with "audio-codec-name"
    - deprecate --media-title, replaced with --force-media-title
    - deprecate "length" property, replaced with "duration"
    - change volume property:
        - the value 100 is now always "unchanged volume" - with softvol, the
          range is 0 to --softvol-max, without it is 0-100
        - the minimum value of --softvol-max is raised to 100
    - remove vo opengl npot suboption
    - add relative seeking by percentage to "seek" command
    - add playlist_shuffle command
    - add --force-window=immediate
    - add ao coreaudio change-physical-format suboption
    - remove vo opengl icc-cache suboption, add icc-cache-dir suboption
    - add --screenshot-directory
    - add --screenshot-high-bit-depth
    - add --screenshot-jpeg-source-chroma
    - default action for "rescan_external_files" command changes
 --- mpv 0.9.0 is released ---
