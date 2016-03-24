Introduction
============

mpv provides access to its internals via the following means:

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

 --- mpv 0.17.0 ---
    - deprecate "track-list/N/audio-channels" property (use
      "track-list/N/demux-channel-count" instead)
    - remove write access to "stream-pos", and change semantics for read access
    - Lua scripts now don't suspend mpv by default while script code is run
    - add "cache-speed" property
    - rename --input-unix-socket to --input-ipc-server, and make it work on
      Windows too
 --- mpv 0.16.0 ---
    - change --audio-channels default to stereo (use --audio-channels=auto to
      get the old default)
    - add --audio-normalize-downmix
    - change the default downmix behavior (--audio-normalize-downmix=yes to get
      the old default)
    - VO opengl custom shaders must now use "sample_pixel" as function name,
      instead of "sample"
    - change VO opengl scaler-resizes-only default to enabled
    - add VO opengl "interpolation-threshold" suboption (introduces new default
      behavior, which can change e.g. ``--video-sync=display-vdrop`` to the
      worse, but is usually what you want)
    - make "volume" and "mute" properties changeable even if no audio output is
      active (this gives not-ideal behavior if --softvol=no is used)
    - add "volume-max" and "mixer-active" properties
    - ignore --input-cursor option for events injected by input commands like
      "mouse", "keydown", etc.
 --- mpv 0.15.0 ---
    - change "yadif" video filter defaults
 --- mpv 0.14.0 ---
    - vo_opengl interpolation now requires --video-sync=display-... to be set
    - change some vo_opengl defaults (including changing tscale)
    - add "vsync-ratio", "estimated-display-fps" properties
    - add --rebase-start-time option
      This is a breaking change to start time handling. Instead of making start
      time handling an aspect of different options and properties (like
      "time-pos" vs. "playback-time"), make it dependent on the new option. For
      compatibility, the "time-start" property now always returns 0, so code
      which attempted to handle rebasing manually will not break.
 --- mpv 0.13.0 ---
    - remove VO opengl-cb frame queue suboptions (no replacement)
 --- mpv 0.12.0 ---
    - remove --use-text-osd (useless; fontconfig isn't a requirement anymore,
      and text rendering is also lazily initialized)
    - some time properties (at least "playback-time", "time-pos",
      "time-remaining", "playtime-remaining") now are unavailable if the time
      is unknown, instead of just assuming that the internal playback position
      is 0
    - add --audio-fallback-to-null option
    - replace vf_format outputlevels suboption with "video-output-levels" global
      property/option; also remove "colormatrix-output-range" property
    - vo_opengl: remove sharpen3/sharpen5 scale filters, add sharpen sub-option
 --- mpv 0.11.0 ---
    - add "af-metadata" property
 --- mpv 0.10.0 ---
    - add --video-aspect-method option
    - add --playlist-pos option
    - add --video-sync* options
      "display-sync-active" property
      "vo-missed-frame-count" property
      "audio-speed-correction" and "video-speed-correction" properties
    - remove --demuxer-readahead-packets and --demuxer-readahead-bytes
      add --demuxer-max-packets and --demuxer-max-bytes
      (the new options are not replacement and have very different semantics)
    - change "video-aspect" property: always settable, even if no video is
      running; always return the override - if no override is set, return
      the video's aspect ratio
    - remove disc-nav (DVD, BD) related properties and commands
    - add "option-info/<name>/set-locally" property
    - add --cache-backbuffer; change --cache-default default to 75MB
      the new total cache size is the sum of backbuffer and the cache size
      specified by --cache-default or --cache
    - add ``track-list/N/audio-channels`` property
    - change --screenshot-tag-colorspace default value
    - add --stretch-image-subs-to-screen
    - add "playlist/N/title" property
    - add --video-stereo-mode=no to disable auto-conversions
    - add --force-seekable, and change default seekability in some cases
    - add vf yadif/vavpp/vdpaupp interlaced-only suboptions
      Also, the option is enabled by default (Except vf_yadif, which has
      it enabled only if it's inserted by the deinterlace property.)
    - add --hwdec-preload
    - add ao coreaudio exclusive suboption
    - add ``track-list/N/forced`` property
    - add audio-params/channel-count and ``audio-params-out/channel-count props.
    - add af volume replaygain-fallback suboption
    - add video-params/stereo-in property
    - add "keypress", "keydown", and "keyup" commands
    - deprecate --ad-spdif-dtshd and enabling passthrough via --ad
      add --audio-spdif as replacement
    - remove "get_property" command
    - remove --slave-broken
    - add vo opengl custom shader suboptions (source-shader, scale-shader,
      pre-shaders, post-shaders)
    - completely change how the hwdec properties work:
        - "hwdec" now reflects the --hwdec option
        - "hwdec-detected" does partially what the old "hwdec" property did
          (and also, "detected-hwdec" is removed)
        - "hwdec-active" is added
    - add protocol-list property
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
 --- mpv 0.9.0 ---
