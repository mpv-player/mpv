Introduction
============

This file lists all changes that can cause compatibility issues when using
mpv through the client API (libmpv and ``client.h``). Since the client API
interfaces to input handling (commands, properties) as well as command line
options, this list is interesting for other uses of mpv, such as the Lua
scripting interface, key bindings in ``input.rst``, or plain command line
usage.

Normally, changes to the C API that are incompatible to previous iterations
receive a major version bump (i.e. the first version number is increased),
while C API additions bump the minor version (i.e. the second number is
increased). Changes to properties/commands/options may also lead to a minor
version bump, in particular if they are incompatible.

The version number is the same as used for MPV_CLIENT_API_VERSION (see
``client.h`` how to convert between major/minor version numbers and the flat
32 bit integer).

Also, read the section ``Compatibility`` in ``client.h``.

API changes
===========

::

 --- mpv 0.9.0 is released ---
 1.17   - add MPV_FORMAT_BYTE_ARRAY
 1.16   - add mpv_opengl_cb_report_flip()
        - introduce mpv_opengl_cb_draw() and deprecate mpv_opengl_cb_render()
 1.15   - mpv_initialize() will now load config files. This requires setting
          the "config" and "config-dir" options. In particular, it will load
          mpv.conf.
        - minor backwards-compatible change to the "seek" and "screenshot"
          commands (new flag syntax, old additional args deprecated)
 --- mpv 0.8.0 is released ---
 1.14   - add mpv_wait_async_requests()
        - the --msg-level option changes its native type from a flat string to
          a key-value list (setting/reading the option as string still works)
 1.13   - add MPV_EVENT_QUEUE_OVERFLOW
 1.12   - add class Handle to qthelper.hpp
        - improve opengl_cb.h API uninitialization behavior, and fix the qml
          example
        - add mpv_create_client() function
 1.11   - add OpenGL rendering interop API - allows an application to combine
          its own and mpv's OpenGL rendering
          Warning: this API is not stable yet - anything in opengl_cb.h might
                   be changed in completely incompatible ways in minor API bumps
 --- mpv 0.7.0 is released ---
 1.10   - deprecate/disable everything directly related to script_dispatch
          (most likely affects nobody)
 1.9    - add enum mpv_end_file_reason for mpv_event_end_file.reason
        - add MPV_END_FILE_REASON_ERROR and the mpv_event_end_file.error field
          for slightly better error reporting on playback failure
        - add --stop-playback-on-init-failure option, and make it the default
          behavior for libmpv only
        - add qthelper.hpp set_option_variant()
        - mark the following events as deprecated:
            MPV_EVENT_TRACKS_CHANGED
            MPV_EVENT_TRACK_SWITCHED
            MPV_EVENT_PAUSE
            MPV_EVENT_UNPAUSE
            MPV_EVENT_METADATA_UPDATE
            MPV_EVENT_CHAPTER_CHANGE
          They are handled better with mpv_observe_property() as mentioned in
          the documentation comments. They are not removed and still work.
 1.8    - add qthelper.hpp
 1.7    - add mpv_command_node(), mpv_command_node_async()
 1.6    - modify "core-idle" property behavior
        - MPV_EVENT_LOG_MESSAGE now always sends complete lines
        - introduce numeric log levels (mpv_log_level)
 --- mpv 0.6.0 is released ---
 1.5    - change in X11 and "--wid" behavior again. The previous change didn't
          work as expected, and now the behavior can be explicitly controlled
          with the "input-x11-keyboard" option. This is only a temporary
          measure until XEmbed is implemented and confirmed working.
          Note: in 1.6, "input-x11-keyboard" was renamed to "input-vo-keyboard",
          although the old option name still works.
 1.4    - subtle change in X11 and "--wid" behavior
          (this change was added to 0.5.2, and broke some things, see #1090)
 --- mpv 0.5.0 is released ---
 1.3    - add MPV_MAKE_VERSION()
 1.2    - remove "stream-time-pos" property (no replacement)
 1.1    - remap dvdnav:// to dvd://
        - add "--cache-file", "--cache-file-size"
        - add "--colormatrix-primaries" (and property)
        - add "primaries" sub-field to image format properties
        - add "playback-time" property
        - extend the "--start" option; a leading "+", which was previously
          insignificant is now significant
        - add "cache-free" and "cache-used" properties
        - OSX: the "coreaudio" AO spdif code is split into a separate AO
 --- mpv 0.4.0 is released ---
 1.0    - the API is declared stable

