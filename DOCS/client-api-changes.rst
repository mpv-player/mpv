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

 --- mpv 0.6.0 is released ---
 1.5    - change in X11 and "--wid" behavior again. The previous change didn't
          work as expected, and now the behavior can be explicitly controlled
          with the "input-x11-keyboard" option. This is only a temporary
          measure until XEmbed is implemented and confirmed working.
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

