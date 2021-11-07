# Differences Between Meson and Waf

mpv currently supports two different build systems: waf and meson. In general,
option names between both build systems are mostly the same. In most cases,
``--enable-foo`` in waf becomes ``-Dfoo=enabled`` in meson. Likewise,
``--disable-foo`` becomes ``-Dfoo=disabled``.  For the rest of this document,
Waf options will be noted as ``--foo`` while meson options are noted as
``foo``.

## Universal Options

Meson has several [universal options](https://mesonbuild.com/Builtin-options.html#universal-options)
that you get for free. In some cases, these overlapped with custom waf options.

* ``--libmpv-static`` and ``--libmpv-shared`` were combined into one option:
  ``libmpv``. Use ``default_library`` to control if you want to build static or
  shared libraries.
* Waf had a boolean ``--optimize`` option. In meson, this is a universal option,
  ``optimization``, which can take several different values. In mpv's meson
  build, the default is ``2``.
* Instead of ``--debug-build``, meson simply calls it ``debug``. It is enabled
  by default.

## Changed Options

* The legacy lua names (``52``, ``52deb``, etc.) for ``--lua`` are not
  supported in the meson build. Instead, pass the generic pkg-config values
  such as ``lua52``, ``lua5.2``, etc.
* ``--lgpl`` was changed to ``gpl``. If ``gpl`` is false, the build is LGPL2.1+.

### Boolean Options

The following options are all booleans that accept ``true`` or ``false``
instead of ``enabled`` or ``disabled``.

* ``build-date``
* ``cplayer``
* ``gpl``
* ``libmpv``
* ``ta-leak-report``
* ``tests``

## Removed Options

There are options removed with no equivalent in the meson build.

* ``--asm`` was removed since it doesn't do anything.
* ``--android`` was removed since meson knows if the machine is android.
* ``--clang-compilation-database`` was removed. Meson can do this on its own
  by invoking ninja (``ninja -t compdb``).
* ``--tvos`` was removed since it doesn't do anything.
* ``--static-build`` was removed. Use ``default_library``.
* ``--swift-static`` was removed. The swift library always dynamically links.

## Renamed Options

These are some other options that were renamed.

* ``--gl-wayland`` was renamed to ``egl-wayland``.
* ``--swift`` was renamed to ``swift-build``.

## Other

* The meson build supports passing the ``SOURCE_DATE_EPOCH`` environment variable
during the compilation step for those who want reproducibility without having to
disable the build date.
* The ``Configuration`` line shown by ``mpv -v`` does not show everything passed on
cli since meson does not have any easy way to access a user's argv. Instead, it
simply shows whatever the value of ``prefix`` is regardless if it was specified
or not.
