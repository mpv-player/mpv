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

 --- mpv 0.21.0 ---
    - setting certain options at runtime will now take care of updating them
      property (see for example issue #3281). On the other hand, it will also
      do runtime verification and reject option changes that do not work
      (example: setting the "vf" option to a filter during playback, which fails
      to initialize - the option value will remain at its old value). In general,
      "set name value" should be mostly equivalent to "set options/name value"
      in cases where the "name" property is not deprecated and "options/name"
      exists - deviations from this are either bugs, or documented as caveats
      in the "Inconsistencies between options and properties" manpage section.
    - deprecate _all_ --vo and --ao suboptions. Generally, all suboptions are
      replaced by global options, which do exactly the same. For example,
      "--vo=opengl:scale=nearest" turns into "--scale=nearest". In some cases,
      the global option is prefixed, e.g. "--vo=opengl:pbo" turns into
      "--opengl-pbo".
      Most of the exact replacements are documented here:
        https://github.com/mpv-player/mpv/wiki/Option-replacement-list
    - remove --vo=opengl-hq. Set --profile=opengl-hq instead. Note that this
      profile does not force the VO. This means if you use the --vo option to
      set another VO, it won't work. But this also means it can be used with
      opengl-cb.
    - remove the --vo=opengl "pre-shaders", "post-shaders" and "scale-shader"
      sub-options: they were deprecated in favor of "user-shaders"
    - deprecate --vo-defaults (no replacement)
    - remove the vo-cmdline command. You can set OpenGL renderer options
      directly via properties instead.
    - deprecate the device/sink options on all AOs. Use --audio-device instead.
    - deprecate "--ao=wasapi:exclusive" and "--ao=coreaudio:exclusive",
      use --audio-exclusive instead.
    - subtle changes in how "--no-..." options are treated mean that they are
      not accessible under "options/..." anymore (instead, these are resolved
      at parsing time). This does not affect options which start with "--no-",
      but do not use the mechanism for negation options.
      (Also see client API change for API version 1.23.)
    - rename the following properties
        - "demuxer" -> "current-demuxer"
        - "fps" -> "container-fps"
        - "idle" -> "idle-active"
        - "cache" -> "cache-percent"
      the old names are deprecated and will change behavior in mpv 0.22.0.
    - remove deprecated "hwdec-active" and "hwdec-detected" properties
    - deprecate the ao and vo auto-profiles (they never made any sense)
    - deprecate "--vo=direct3d_shaders" - use "--vo=direct3d" instead.
      Change "--vo=direct3d" to always use shaders by default.
    - deprecate --playlist-pos option, renamed to --playlist-start
    - deprecate the --chapter option, as it is redundant with --start/--end,
      and conflicts with the semantics of the "chapter" property
    - rename --sub-text-* to --sub-* and --ass-* to --sub-ass-* (old options
      deprecated)
    - incompatible change to cdda:// protocol options: the part after cdda://
      now always sets the device, not the span or speed to be played. No
      separating extra "/" is needed. The hidden --cdda-device options is also
      deleted (it was redundant with the documented --cdrom-device).
    - deprecate --vo=rpi. It will be removed in mpv 0.22.0. Its functionality
      was folded into --vo=opengl, which now uses RPI hardware decoding by
      treating it as a hardware overlay (without applying GL filtering). Also
      to be changed in 0.22.0: the --fs flag will be reset to "no" by default
      (like on the other platforms).
    - deprecate --mute=auto (informally has been since 0.18.1)
    - deprecate "resume" and "suspend" IPC commands. They will be completely
      removed in 0.22.0.
    - deprecate mp.suspend(), mp.resume(), mp.resume_all() Lua scripting
      commands, as well as setting mp.use_suspend. They will be completely
      removed in 0.22.0.
    - the "seek" command's absolute seek mode will now interpret negative
      seek times as relative from the end of the file (and clamps seeks that
      still go before 0)
    - add almost all options to the property list, meaning you can change
      options without adding "options/" to the property name (a new section
      has been added to the manpage describing some conflicting behavior
      between options and properties)
    - implement changing sub-speed during playback
    - make many previously fixed options changeable at runtime (for example
      --terminal, --osc, --ytdl, can all be enable/disabled after
      mpv_initialize() - this can be extended to other still fixed options
      on user requests)
 --- mpv 0.20.0 ---
    - add --image-display-duration option - this also means that image duration
      is not influenced by --mf-fps anymore in the general case (this is an
      incompatible change)
 --- mpv 0.19.0 ---
    - deprecate "balance" option/property (no replacement)
 --- mpv 0.18.1 ---
    - deprecate --heartbeat-cmd
    - remove --softvol=no capability:
        - deprecate --softvol, it now does nothing
        - --volume, --mute, and the corresponding properties now always control
          softvol, and behave as expected without surprises (e.g. you can set
          them normally while no audio is initialized)
        - rename --softvol-max to --volume-max (deprecated alias is added)
        - the --volume-restore-data option and property are removed without
          replacement. They were _always_ internal, and used for watch-later
          resume/restore. Now --volume/--mute are saved directly instead.
        - the previous point means resuming files with older watch-later configs
          will print an error about missing --volume-restore-data (which you can
          ignore), and will not restore the previous value
        - as a consequence, volume controls will no longer control PulseAudio
          per-application value, or use the system mixer's per-application
          volume processing
        - system or per-application volume can still be controlled with the
          ao-volume and ao-mute properties (there are no command line options)
 --- mpv 0.18.0 ---
    - now ab-loops are active even if one of the "ab-loop-a"/"-b" properties is
      unset ("no"), in which case the start of the file is used if the A loop
      point is unset, and the end of the file for an unset B loop point
    - deprecate --sub-ass=no option by --ass-style-override=strip
      (also needs --embeddedfonts=no)
    - add "hwdec-interop" and "hwdec-current" properties
    - deprecated "hwdec-active" and "hwdec-detected" properties (to be removed
      in mpv 0.20.0)
    - choice option/property values that are "yes" or "no" will now be returned
      as booleans when using the mpv_node functions in the client API, the
      "native" property accessors in Lua, and the JSON API. They can be set as
      such as well.
    - the VO opengl fbo-format sub-option does not accept "rgb" or "rgba"
      anymore
    - all VO opengl prescalers have been removed (replaced by user scripts)
 --- mpv 0.17.0 ---
    - deprecate "track-list/N/audio-channels" property (use
      "track-list/N/demux-channel-count" instead)
    - remove write access to "stream-pos", and change semantics for read access
    - Lua scripts now don't suspend mpv by default while script code is run
    - add "cache-speed" property
    - rename --input-unix-socket to --input-ipc-server, and make it work on
      Windows too
    - change the exact behavior of the "video-zoom" property
    - --video-unscaled no longer disables --video-zoom and --video-aspect
      To force the old behavior, set --video-zoom=0 and --video-aspect=0
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
