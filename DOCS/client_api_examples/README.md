# Client API examples

All these examples use the mpv client API through libmpv.

## Where are the docs?

The libmpv C API is documented directly in the header files (on normal Unix
systems, this is in `/usr/include/mpv/client.h`.

libmpv merely gives you access to mpv's command interface, which is documented
here:
* Options (`mpv_set_option()` and friends): http://mpv.io/manual/master/#options
* Commands (`mpv_command()` and friends): http://mpv.io/manual/master/#list-of-input-commands
* Properties (`mpv_set_property()` and friends): http://mpv.io/manual/master/#properties

Essentially everything is done with them, including loading a file, retrieving
playback progress, and so on.

## Methods of embedding the video window

All of these examples concentrate on how to integrate mpv's video renderers
with your own GUI. This is generally the hardest part. libmpv enforces a
somewhat efficient video output method, rather than e.g. returning a RGBA
surface in memory for each frame. The latter would be prohibitively inefficient,
because it would require conversion on the CPU. The goal is also not requiring
the API users to reinvent their own video rendering/scaling/presentation
mechanisms.

There are currently 2 methods of embedding video.

### Native window embedding

This uses the platform's native method of nesting multiple windows. For example,
Linux/X11 can nest a window from a completely different process. The nested
window can redraw contents on its own, and receive user input if the user
interacts with this window.

libmpv lets you specify a parent window for its own video window via the `wid`
option. Then libmpv will create its window with your window as parent, and
render its video inside of it.

This method is highly OS-dependent. Some behavior is OS-specific. There are
problems with focusing on X11 (the ancient X11 focus policy mismatches with
that of modern UI toolkits - it's normally worked around, but this is not
easily possible with raw window embedding). It seems to have stability problems
on OSX when using the Qt toolkit.

### OpenGL embedding

This method lets you use libmpv's OpenGL renderer directly. You create an
OpenGL context, and then use `mpv_opengl_cb_draw()` to render the video on
each frame.

This is more complicated, because libmpv will work directly on your own OpenGL
state. It's also not possible to have mpv automatically receive user input.
You will have to simulate this with the `mouse`/`keypress`/`keydown`/`keyup`
commands.

You also get much more flexibility. For example, you can actually render your
own OSD on top of the video, something that is not possible with raw window
embedding.

### Which one to use?

Due to the various platform-specific behavior and problems (in particular on
OSX), OpenGL embedding is recommended. If you're not comfortable with requiring
OpenGL, or want to support "direct" video output such as vdpau (which might
win when it comes to performance and energy-saving), you should probably
support both methods if possible.

## List of examples

### simple

Very primitive terminal-only example. Shows some most basic API usage.

### cocoa

Shows how to embed the mpv video window in Objective-C/Cocoa.

### cocoa-openglcb

Similar to the cocoa sample, but shows how to integrate mpv's OpenGL renderer
using libmpv's opengl-cb API. Since it does not require complicated interaction
with Cocoa elements from different libraries, it's more robust.

### qt

Shows how to embed the mpv video window in Qt (using normal desktop widgets).

### qt_opengl

Shows how to use mpv's OpenGL video renderer in Qt. This uses the opengl-cb API
for video. Since it does not rely on embedding "foreign" native Windows, it's
usually more robust, potentially faster, and it's easier to control how your
GUI interacts with the video. You can do your own OpenGL rendering on top of
the video as well.

### qml

Shows how to use mpv's OpenGL video renderer in QtQuick2 with QML. Uses the
opengl-cb API for video. Since the video is a normal QML element, it's trivial
to create OSD overlays with QML-native graphical elements as well.

### qml_direct

Alternative example, which typically avoids a FBO indirection. Might be
slightly faster, but is less flexible and harder to use. In particular, the
video is not a normal QML element. Uses the opengl-cb API for video.

### sdl

Show how to embed the mpv OpenGL renderer in SDL. Uses the opengl-cb API for
video.
