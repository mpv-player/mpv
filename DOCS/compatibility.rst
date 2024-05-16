CLI and API compatibility policy
================================

Human users and API users rely on the mpv/libmpv/scripting/IPC interface not
breaking their use case. On the other hand, active development occasionally
requires breaking things, such as removing old options or changing options in
a way that may break.

This document lists rules when, what, and how incompatible changes can be made.
It's interesting both for mpv developers, who want to change user-visible parts,
and mpv users, who want to know what is guaranteed to remain stable.

Any of the rules below may be overridden by statements in more specific
documentation (for example, if the manpage says that a particular option may be
removed any time, it means that, and the option probably won't even go through
deprecation).

Additions
---------

Additions are basically always allowed. API users etc. are supposed to deal with
the possibility that for example new API functions are added. Some parts of the
API may document how they are extended.

Options, commands, properties, events, hooks (command interface)
----------------------------------------------------------------

All of these are important for interfacing both with end users and API users
(which include Lua scripts, libmpv, and the JSON IPC). As such, they constitute
a large part of the user interface and APIs.

All incompatible changes to this must be declared in interface-changes.rst.
(This may also list compatible additions, but it's not a requirement.)

Degrees of importance and compatibility preservation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Critical and central parts of the command interface have the strictest
requirements. It may not be reasonable to break them, and other means to achieve
some change have to be found. For example, the "seek" command is a bit of a
mess, but since changing it would likely affect almost every user, it may be
impossible to break at least the commonly used syntax. If changed anyway, there
should be a deprecation period of at least 1 year, during which the command
still works, and possibly a warning should remain even after this.

Important/often used parts must be deprecated for at least 2 releases before
they can be broken. There must be at least 1 release where the deprecated
functionality still works, and a replacement is available (if technically
reasonable). For example, a feature deprecated in mpv 0.30.0 may be removed in
mpv 0.32.0. Minor releases do not count towards this.

Less useful parts can be broken immediately, but must come with some sort of
removal warning.

Parts for debugging and testing may be removed any time, potentially even
without any sort of documentation.

Currently, the importance of a part is not documented and not even well-defined,
which is probably a mistake.

Renaming or removing options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Typically, renaming an option can be done in a compatible way with OPT_REPLACED.
You may need to check whether the corresponding properties still work (including
messy details like observing properties for changes).

OPT_REMOVED can be used to inform the user of alternatives or reasons for the
removal, which is better than an option not found error. Likewise,
m_option.deprecation_message should be set to something helpful.

Both OPT_REPLACED and OPT_REMOVED can remain in the code for a long time, since
they're unintrusive and hopefully make incompatible changes less painful for
users.

Scripting APIs
--------------

This affects internal scripting APIs (currently Lua and JavaScript).

Vaguely the same rules as with the command interface apply. Since there is a
large number of scripts, an effort should be made to provide compatibility
for old scripts, but it does not need to be stronger than that of the command
interface.

Undocumented parts of the scripting APIs are _not_ guaranteed for compatibility.
This applies especially for internals. Languages like Lua do not have strict
access control (nor does the mpv code try to emulate any), so if a script
accesses private parts, and breaks on the next mpv release, it's not mpv's
problem.

JSON IPC
--------

The JSON IPC is a thin protocol wrapping the libmpv API and the command
interface. Compatibility-wise, it's about the same as the scripting APIs.
The JSON protocol commands should remain as compatible as possible, and it
should probably accept the current way commands are delimited (line breaks)
forever.

The protocol may accept non-standard JSON extensions, but only standard JSON
(possibly with restrictions) is guaranteed for compatibility. Clients which want
to remain compatible should not use any extensions.

CLI
---

Things such as default key bindings do not necessarily require compatibility.
However, the release notes should be extremely clear on changes to "important"
key bindings. Bindings which restore the old behavior should be added to
restore-old-bindings.conf.

Some option parsing is CLI-only and not available from libmpv or scripting. No
compatibility guarantees come with them. However, the rules which mpv uses to
distinguish between options and filenames must remain consistent (if the
non-deprecated options syntax is used).

Terminal and log output
-----------------------

There are no compatibility guarantees for the terminal output, or the text
logged via ``MPV_EVENT_LOG_MESSAGE`` and similar APIs. In particular, scripts
invoking mpv CLI are extremely discouraged from trying to parse text output,
and should use other mechanisms such as the JSON IPC.

Protocols, filters, demuxers, etc.
----------------------------------

Which of these are present is generally not guaranteed, and can even depend
on compile time settings.

The filter list and their sub-options are considered part of the
command-interface.

libmpv C API
------------

The libmpv client API (such as ``<libmpv/client.h>``) mostly gives access to
the command interface. The API itself (if looked at as a component separate
from the command interface) is intended to be extremely stable.

All API changes are documented in client-api-changes.rst.

API compatibility
^^^^^^^^^^^^^^^^^

The API is *always* compatible. Incompatible changes are only allowed on major
API version changes (see ``MPV_CLIENT_API_VERSION``). A major version change is
an extremely rare event, which means usually no API symbols are ever removed.

Essentially removing API functions by making them always return an error, or
making it do nothing is allowed in cases where it is unlikely to break most
clients, but requires a deprecation period of 2 releases. (This has happened to
``mpv_suspend()`` for example.)

API symbols can be deprecated. This should be clearly marked in the doxygen
with ``@deprecated``, and if possible, the affected API symbols should not be
visible if the API user defines ``MPV_ENABLE_DEPRECATED`` to 0.

ABI compatibility
^^^^^^^^^^^^^^^^^

The ABI must never be broken, except on major API version changes. For example,
constants don't change their values.

Structs are tricky. If a struct can be allocated by a user (such as ``mpv_node``),
no fields can be added. (Unless it's an union, and the addition does not change
the offset or alignment of any of the fields or the struct itself. This has
happened to ``mpv_node`` in the past.) If a struct is allocated by libmpv only,
new fields can be appended to the end (for example ``mpv_event``).

The ABI is only backward compatible. This means if a host application is linked
to an older libmpv, and libmpv is updated to a newer version, it will still
work (as in not causing any undefined behavior).

Forward compatibility (an application would work with an older libmpv than it
was linked to) is not required.
