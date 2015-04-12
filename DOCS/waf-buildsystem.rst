waf build system overview
=========================

mpv's new build system is based on waf and it should completly replace the
custom ./configure + Makefile based system inherited from MPlayer.

Goals and the choice of waf
===========================

The new system comes with some goals, which can be summed up as: be as good as
the old one at what it did well (customizability) and fix some of it's major
shortcomings:

1) The build system must be uniform in how it handles any single feature check.
   Repetition and boilerplate have to be avoided.

   When adding a new feature using the old configure, one had to add a fair
   amount of code to the shell script to do option parsing, detection of the
   feature and declaration of variables for the Makefile to pickup. The worst
   part is this pieces are spread apart in the configure and copy pasted for
   any single case. That brings us to..

2) --enable-feature has to be overridden by the user and helps them understand that
   they have libraries missing and should install them for the feature to be
   enabled.

3) Must be customizable, hackable, pleasant to the developer eyes and to work
   with in general.

4) Must have separate configuration and build steps.

Goal 2 comes as a given on pretty much any build system, since autotools made
this behaviour very popular among users (and rightly so).

Goal 1+3 were somewhat harder to accomplish as it looks like all of the build
systems we evaluated (waf included!) had problems with them. For reference we
had proof of concept build systems with waf, CMake and autotools.

What puts waf apart from CMake and autotools, is that projects using it use
Python to program their build system. Also while the Waf Book shows really
simple API usages, you can write your own build system on top of waf that is
tailored to the project's specific needs.

mpv's custom configure step on top of waf
=========================================

To some extents mpv has a custom build system written on top of waf. This
document will not go over the standard waf behaviour as that is documented in
the `Waf book <http://docs.waf.googlecode.com/git/book_17/single.html>`_.

All of the configuration process is handled with a declarative approach. Lists
of dictionaries define the checks, and some custom Python code traverses these
lists and depending on the check definition it calls into the actual waf API.

A simple example using pkg-config would be::

  {
      'name': '--vdpau',
      'desc': 'VDPAU acceleration',
      'deps': [ 'x11' ],
      'func': check_pkg_config('vdpau', '>= 0.2'),
  }

This defines a feature called ``vdpau`` which can be enabled or disabled by
the users with configure flags (that's the meaning of ``--``). This feature
depends on another feature whose name is ``x11``, and the autodetection check
consists of running ``pkg-config`` and looking for ``vdpau`` with version
``>= 0.2``. If the check succeds a ``#define HAVE_VDPAU 1`` will be added to
``config.h``, if not ``#define HAVE_VDPAU 0`` will be added.

The defines names are automatically prepended with ``HAVE_``, capitalized and
special characters are replaced with underscores. This happens in
``waftools/inflectors.py``.

Mandatory fields:
-----------------

``name``: indicates the unique identifier used by the custom dependency code
to refer to a feature. If the unique identifier is prepended with ``--``
the build system will also generate options for ``./waf configure`` so that
the feature can be enabled and disabled.

``desc``: this is the textual representation of the feature used in the
interactions with the users.

``func``: function that will perform the check. These functions are defined in
``waftools/checks``. The reusable checks are all functions that return
functions. The return functions will then be applied using waf's configuration
context.

The source code for the reusable checks is a bit convoluted, but it should be
easy to pick up their usage from the ``wscript``. Their signature mirrors
the semantics of some of the shell functions used in mplayer.

If someone expresses some interest, I will extend this document with official
documentation for each check function.

Optional fields
---------------

``deps``: list of dependencies of this feature. It is a list of names of
other features as defined in the ``name`` field (minus the eventual leading
``--``). All of the dependencies must be satisfied. If they are not the check
will be skipped without even running ``func``.

``deps_any``: like deps but it is satisfied even if only one of the dependencies
is satisfied. You can think of ``deps`` as a 'and' condition and ``deps_any``
as a 'or' condition.

``deps_neg``: like deps but it is satisfied when none of the dependencies is
satisfied.

``req``: defaults to False. If set to True makes this feature a hard
dependency of mpv (configuration will fail if autodetection fails). If set to
True you must also provide ``fmsg``.

``fmsg``: string with the failure message in case a required dependency is not
satisfied.

``os_specific_checks``: this takes a dictionary that has ``os-`` dependencies
as keys (such as ``os-win32``), and by values has another dictionary that is
merged on top of the current feature definition only for that specific OS.
For example::

  {
      'name': '--pthreads',
      'desc': 'POSIX threads',
      'func': check_pthreads,
      'os_specific_checks': {
          'os-win32': {
              'func': check_pthreads_w32_static.
          }
      }
  }

will override the value of ``func`` with ``check_pthreads_w32_static`` only
if the target OS of the build is Windows.

``groups``: groups a dependency with another one. This can be used to disabled
all the grouped dependencies with one ``--disable-``. At the moment this is
only used for OpenGL backends, where you want to disable them when
``--disable-gl`` is passed to the configure.

mpv's custom build step on top of waf
=====================================

Build step is pretty much vanilla waf. The only difference being that the list
of source files can contain both strings or tuples. If a tuple is found,
the second element in the tuple will the used to match the features detected
in the configure step (the ``name`` field described above). If this feature
was not enabled during configure, the source file will not be compiled in.

All of the custom Python for this is inside the function ``filtered_sources``
contained in the file ``waftools/dependencies.py``.

Also ``dependencies_use`` and ``dependencies_includes`` collect cflags and
ldflags that were generated from the features checks in the configure step.
