Build system overview
=====================

mpv's new build system is based on Python and completely replaces the previous
./waf build system.

This file describes internals. See the README in the top level directory for
user help.

User help (to be moved to README.md)
====================================

Compiling with full features requires development files for several
external libraries. Below is a list of some important requirements.

For a list of the available build options use `./configure --help`. If
you think you have support for some feature installed but configure fails to
detect it, the file `build/config.log` may contain information about the
reasons for the failure.

NOTE: To avoid cluttering the output with unreadable spam, `--help` only shows
one of the many switches for each option. If the option is autodetected by
default, the `--disable-***` switch is printed; if the option is disabled by
default, the `--enable-***` switch is printed. Either way, you can use
`--enable-***` or `--disable-***` regardless of what is printed by `--help`.
By default, most features are auto-detected. You can use ``--with-***=option``
to get finer control over whether auto-detection is used for a feature.

Example:

    ./configure && make -j20

If everything goes well, the mpv binary is created in the ``build`` directory.

`make` alone can be used to rebuild parts of the player. On update, it's
recommended to run `make dist-clean` and to rerun configure.

See `./configure --help` for advanced usage.

Motivation & Requirements
=========================

It's unclear what the fuck the author of the new build system was thinking.

Big picture
===========

The configure script is written in Python. It generates config.h and config.mak
files (and possibly more). By default these are written to a newly created
"build" directory. It also writes a build.log.

The "actual" build system is based on GNU make (other make variants probably
won't work). The Makefile in the project root is manually created by the build
system "user" (i.e. the mpv developers), and is fixed and not changed by
configure. It includes the configured-generated build/config.mak file for the
variable parts. It also includes Header file dependencies are handled
automatically with the ``-MD`` option (which the compiler must support).

For out-of-tree builds, a small Makefile is generated that includes the one
from the source directory. Simply call configure from another directory.
(Note: this is broken, fails at generated files, and is also ugly.)

By default, it attempts not to write any build output to the source tree, except
to the "build" directory.

Comparison to previous waf build system
=======================================

The new configure uses the same concept as our custom layer above waf, which
made the checks generally declarative. In fact, most checks were ported
straight, changing only to the new syntax.

Some of the internal and user-visible conventions are extremely similar. For
example, the new system creates a build dir and writes to it by default.

The choice of Python as implementation language is unfortunate. Shell was
considered, but discarded for being too fragile, error prone, and PITA-ish.
Lua would be reasonable, but is too fragmented, and requires external
dependencies to do meaningful UNIX scripting. There is nothing else left that
is widely supported enough, does not require external dependencies, and which
isn't something that I would not touch without gloves. Bootstrapping a system
implemented in C was considered, but deemed too problematic.

mpv's custom configure
======================

All of the configuration process is handled with a mostly-declarative approach.
Each configure check is a call to a "check" function, which takes various named
arguments. The check function is obviously always called, even if the
corresponding feature is disabled.

A simple example using pkg-config would be::

check("-vdpau*",
      desc      = "VDPAU acceleration",
      deps      = "x11",
      fn        = lambda: check_pkg_config("vdpau >= 0.2"),
      sources   = ["video/filter/vf_vdpaupp.c",
                   "video/out/vo_vdpau.c",
                   "video/vdpau.c",
                   "video/vdpau_mixer.c"])

This defines a feature called ``vdpau`` which can be enabled or disabled by
the users with configure flags (that's the meaning of ``-``). This feature
depends on another feature whose name is ``x11``, and the autodetection check
consists of running ``pkg-config`` and looking for ``vdpau`` with version
``>= 0.2``. If the check succeeds a ``#define HAVE_VDPAU 1`` will be added to
``config.h``, if not ``#define HAVE_VDPAU 0`` will be added (the ``*`` on the
feature name triggers emitting of such defines).

The defines names are automatically prepended with ``HAVE_``, capitalized, and
some special characters are replaced with underscores.

If the test succeeds, the listed source files are added to the build.

Read the inline-documentation on the check function in configure_common.py for
details. The following text only gives a crude overview.

Configure tests
---------------

The check function has a ``fn`` parameter. This function is called when it's
time to perform actual configure checks. Most check calls in configure make
this a lambda, so the actual code to run can be passed inline as a function
argument. (This is similar to the old waf based system, just that functions
like check_pkg_config returned a function as result, which hid the indirection.)

One central function is ``check_cc``. It's quite similar to the waf-native
function with the same name. One difference is that there is no ``mandatory``
option - instead it always returns a bool for success. On success, the passed
build flags are appended to the check's build flags. This makes it easier to
compose checks. For example::

check(desc      = "C11/C99",
      fn        = lambda: check_cc(flags = "-std=c11") or
                          check_cc(flags = "-std=c99"),
      required  = "No C11 or C99 support.")

This tries to use -std=c11, but allows a fallback to -std=c99.

If the entire check fails, none of the added build flags are added. For example,
you could chain multiple tests like this::

check("-vapoursynth*",
      fn        = lambda: check_pkg_config("vapoursynth >= 24") and
                          check_pkg_config("vapoursynth-script >= 23"))

If the second check fails, the final executable won't link to ``vapoursynth``.
(Note that this test could just make a single check_pkg_config call, and pass
each dependency as separate argument.)

Source files
------------

configure generates the list of source files and writes it to config.mak. You
can add source files at any point in configure, but normally they're added with
the ``sources`` parameter in each feature check. This is done because a larger
number of source files depend on configure options, so having it all in the same
place as the check is slightly nicer than having a separate conditional mess in
the fixed Makefile.

Configure phases, non-declarative actions
-----------------------------------------

configure was written to be as single-pass as possible. It doesn't even put the
checks in any lists or so (except for the outcome). Handling of ``--enable-...``
etc. options is done while running configure. If you pass e.g.
``--enable-doesntexist``, configure will complain about an unknown
``doesntexist`` feature only once all checks have been actually run.

Although this is slightly weird, it is done so that the ``configure`` file
itself can be a flat file with simple top-down execution. It enables you to add
arbitrary non-declarative checks and such between the ``check`` calls.

One thing you need to be aware of is that if ``--help`` was passed to configure,
it will run in "help mode". You may have to use ``is_running()`` to check
whether it's in a mode where checks are actually executed. Outside of this mode,
``dep_enabled()`` will fail.

Makefile
--------

Although most source files are added from configure, this build system still
may require you to write some make. In particular, generated files are not
handled by configure.

make is bad. It's hard to use, hard to debug, and extremely fragile. It may be
replaced by something else in the future, including the possibility of turning
configure into waf-light.

Variables:

    ``BUILD``
        The directory for build output. Can be a relative path, usually set to
        ``build``.
    ``ROOT``
        The directory that contains ``configure``. Usually the root directory
        of the repository. Source files need to be addressed relative to this
        path. Can be a relative path, usually set to ``.``.
