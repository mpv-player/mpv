Building mpv with Javascript scripting support
==============================================

General
-------
mpv supports both Duktape and MuJS JS backends, and will auto-detect both.
Both have minimal dependencies (Duktape needs `libm` and MuJS not even that),
but neither of them uses pkg-config and both have to be fetched manually.

If both of them are present then mpv will pick Duktape. If you prefer MuJS
then use `waf configure --disable-duktape`.

Duktape - http://duktape.org/
-----------------------------

In order to build mpv with Duktape, mpv needs `duktape.c` and `duktape.h`
from the release distribution of Duktape at directory `<mpv-src>/duktape` -
which you'll have to create manually and then copy these files into it.

The latest Duktape release distribution can be downloaded from
http://duktape.org/download.html as a `.tar.xz` archive or by cloning
https://github.com/svaarala/duktape-releases.git and then `git checkout`
the latest tag - which will have `src/duktape.*` files which mpv uses.

- Duktape currently has an MIT license.

MuJS - http://mujs.com/
-----------------------

Clone the MuJS sources - `git://git.ghostscript.com/mujs.git` (a mirror is
available at https://github.com/ccxvii/mujs ).

At the MuJS sources directory use `make && make install` (it copies
`libmujs.a` and `mujs.h` into `/usr/local/lib` and `/usr/local/include`).

If you cross-compile, have `CC`, `AR` and `prefix` set accordingly.

- MuJS currently has an AGPLv3+ license.
