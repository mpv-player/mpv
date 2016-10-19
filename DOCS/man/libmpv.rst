EMBEDDING INTO OTHER PROGRAMS (LIBMPV)
======================================

mpv can be embedded into other programs as video/audio playback backend. The
recommended way to do so is using libmpv. See ``libmpv/client.h`` in the mpv
source code repository. This provides a C API. Bindings for other languages
might be available (see wiki).

Since libmpv merely allows access to underlying mechanisms that can control
mpv, further documentation is spread over a few places:

- https://github.com/mpv-player/mpv/blob/master/libmpv/client.h
- http://mpv.io/manual/master/#options
- http://mpv.io/manual/master/#list-of-input-commands
- http://mpv.io/manual/master/#properties
- https://github.com/mpv-player/mpv-examples/tree/master/libmpv
