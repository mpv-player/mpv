Introduction
============

This file lists all changes that can cause compatibility issues when using
mpv through the client API (libmpv and ``client.h``). Since the client API
interfaces to input handling (commands, properties) as well as command line
options, you should also look at ``interface-changes.rst``.

Normally, changes to the C API that are incompatible to previous iterations
receive a major version bump (i.e. the first version number is increased),
while C API additions bump the minor version (i.e. the second number is
increased). Changes to properties/commands/options may also lead to a minor
version bump, in particular if they are incompatible.

The version number is the same as used for MPV_CLIENT_API_VERSION (see
``client.h`` how to convert between major/minor version numbers and the flat
32 bit integer).

Also, read the section ``Compatibility`` in ``client.h``.

Options, commands, properties
=============================

Changes to these are not listed here, but in ``interface-changes.rst``. (Before
client API version 1.17, they were listed here partially.)

This listing includes changes to the bare C API and behavior only, not what
you can access with them.

API changes
===========

::

 --- mpv 0.18.1 ---
 ----   - remove "status" log level from mpv_request_log_messages() docs. This
          is 100% equivalent to "v". The behavior is still the same, thus no
          actual API change.
 --- mpv 0.18.0 ---
 1.21   - mpv_set_property() changes behavior with MPV_FORMAT_NODE. Before this
          change it rejected mpv_nodes with format==MPV_FORMAT_STRING if the
          property was not a string or did not have special mechanisms in place
          the function failed. Now it always invokes the option string parser,
          and mpv_node with a basic data type works exactly as if the function
          is invoked with that type directly. This new behavior is equivalent
          to mpv_set_option().
          This also affects the mp.set_property_native() Lua function.
        - generally, setting choice options/properties with "yes"/"no" options
          can now be set as MPV_FORMAT_FLAG
        - reading a choice property as MPV_FORMAT_NODE will now return a
          MPV_FORMAT_FLAG value if the choice is "yes" (true) or "no" (false)
          This implicitly affects Lua and JSON IPC interfaces as well.
        - big changes to vo-cmdline on vo_opengl and vo_opengl_hq (but not
          vo_opengl_cb): options are now normally not reset, but applied on top
          of the current options. The special undocumented value "-" still
          works, but now resets all options to before any vo-cmdline command
          has been called.
 --- mpv 0.12.0 ---
 1.20   - deprecate "GL_MP_D3D_interfaces"/"glMPGetD3DInterface", and introduce
          "GL_MP_MPGetNativeDisplay"/"glMPGetNativeDisplay" (this is a
          backwards-compatible rename)
 --- mpv 0.11.0 ---
 --- mpv 0.10.0 ---
 1.19   - add "GL_MP_D3D_interfaces" pseudo extension to make it possible to
          use DXVA2 in OpenGL fullscreen mode in some situations
        - mpv_request_log_messages() now accepts "terminal-default" as parameter
 1.18   - add MPV_END_FILE_REASON_REDIRECT, and change behavior of
          MPV_EVENT_END_FILE accordingly
        - a bunch of interface-changes.rst changes
 1.17   - mpv_initialize() now blocks SIGPIPE (details see client.h)
 --- mpv 0.9.0 ---
 1.16   - add mpv_opengl_cb_report_flip()
        - introduce mpv_opengl_cb_draw() and deprecate mpv_opengl_cb_render()
        - add MPV_FORMAT_BYTE_ARRAY
 1.15   - mpv_initialize() will now load config files. This requires setting
          the "config" and "config-dir" options. In particular, it will load
          mpv.conf.
        - minor backwards-compatible change to the "seek" and "screenshot"
          commands (new flag syntax, old additional args deprecated)
 --- mpv 0.8.0 ---
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
 --- mpv 0.7.0 ---
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
 --- mpv 0.6.0 ---
 1.5    - change in X11 and "--wid" behavior again. The previous change didn't
          work as expected, and now the behavior can be explicitly controlled
          with the "input-x11-keyboard" option. This is only a temporary
          measure until XEmbed is implemented and confirmed working.
          Note: in 1.6, "input-x11-keyboard" was renamed to "input-vo-keyboard",
          although the old option name still works.
 1.4    - subtle change in X11 and "--wid" behavior
          (this change was added to 0.5.2, and broke some things, see #1090)
 --- mpv 0.5.0 ---
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
 --- mpv 0.4.0 ---
 1.0    - the API is declared stable

