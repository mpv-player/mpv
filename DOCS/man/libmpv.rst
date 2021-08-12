EMBEDDING INTO OTHER PROGRAMS (LIBMPV)
======================================

mpv can be embedded into other programs as video/audio playback backend. The
recommended way to do so is using libmpv. See ``libmpv/client.h`` in the mpv
source code repository. This provides a C API. Bindings for other languages
might be available (see wiki).

Since libmpv merely allows access to underlying mechanisms that can control
mpv, further documentation is spread over a few places:

- https://github.com/mpv-player/mpv/blob/master/libmpv/client.h
- https://mpv.io/manual/master/#options
- https://mpv.io/manual/master/#list-of-input-commands
- https://mpv.io/manual/master/#properties
- https://github.com/mpv-player/mpv-examples/tree/master/libmpv

C PLUGINS
=========

You can write C plugins for mpv. These use the libmpv API, although they do not
use the libmpv library itself.

They are available on Linux/BSD platforms only and enabled by default if the
compiler supports linking with the ``-rdynamic`` flag.

C plugins location
------------------

C plugins are put into the mpv scripts directory in its config directory
(see the `FILES`_ section for details). They must have a ``.so`` file extension.
They can also be explicitly loaded with the ``--script`` option.

API
---

A C plugin must export the following function::

    int mpv_open_cplugin(mpv_handle *handle)

The plugin function will be called on loading time. This function does not
return as long as your plugin is loaded (it runs in its own thread). The
``handle`` will be deallocated as soon as the plugin function returns.

The return value is interpreted as error status. A value of ``0`` is
interpreted as success, while ``-1`` signals an error. In the latter case,
the player prints an uninformative error message that loading failed.

Return values other than ``0`` and ``-1`` are reserved, and trigger undefined
behavior.

Within the plugin function, you can call libmpv API functions. The ``handle``
is created by ``mpv_create_client()`` (or actually an internal equivalent),
and belongs to you. You can call ``mpv_wait_event()`` to wait for things
happening, and so on.

Note that the player might block until your plugin calls ``mpv_wait_event()``
for the first time. This gives you a chance to install initial hooks etc.
before playback begins.

The details are quite similar to Lua scripts.

Linkage to libmpv
-----------------

The current implementation requires that your plugins are **not** linked against
libmpv. What your plugins uses are not symbols from a libmpv binary, but
symbols from the mpv host binary.

Examples
--------

See:

- https://github.com/mpv-player/mpv-examples/tree/master/cplugins
