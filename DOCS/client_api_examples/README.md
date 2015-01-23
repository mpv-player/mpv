Client API examples
===================

All these examples use the mpv client API through libmpv.

cocoa
-----

Shows how to embed the mpv video window in Objective-C/Cocoa.

qt
--

Shows how to embed the mpv video window in Qt (using normal desktop widgets).

qml
---

Shows how to use mpv's OpenGL video renderer in QtQuick2 with QML.

qml_direct
----------

Alternative example, which typically avoids a FBO indirection. Might be
slightly faster, but is less flexible and harder to use.

simple
------

Very primitive terminal-only example. Shows some most basic API usage.
