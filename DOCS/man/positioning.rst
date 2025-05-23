POSITIONING
===========

This script provides script bindings to pan videos and images. It can be
disabled using the ``--load-positioning=no`` option.

Script bindings
---------------

``pan-x <amount>``
    Adjust ``--video-align-x`` relatively to the OSD width, rather than
    relatively to the video width like the option. This is useful to pan large
    images consistently.

    ``amount`` is a number such that an amount of 1 scrolls as much as the OSD
    width.

``pan-y <amount>``
    Adjust ``--video-align-y`` relatively to the OSD height, rather than
    relatively to the video height like the option.

    ``amount`` is a number such that an amount of 1 scrolls as much as the OSD
    height.

``drag-to-pan``
    Pan the video while holding a mouse button, keeping the clicked part of the
    video under the cursor.

``align-to-cursor``
    Pan through the whole video while holding a mouse button, or after clicking
    once if ``toggle_align_to_cursor`` is ``yes``.

``cursor-centric-zoom <amount>``
    Increase ``--video-zoom`` by ``amount`` while keeping the part of the video
    hovered by the cursor under it, or the average position of touch points if
    known.

Configuration
-------------

This script can be customized through a config file
``script-opts/positioning.conf`` placed in mpv's user directory and through the
``--script-opts`` command-line option. The configuration syntax is described in
`mp.options functions`_.

Configurable Options
~~~~~~~~~~~~~~~~~~~~

``toggle_align_to_cursor``
    Default: no

    Whether ``align-to-cursor`` requires holding down a mouse button to pan. If
    ``no``, dragging pans. If ``yes``, clicking the first time makes pan follow
    the cursor, and clicking a second time disables this.

``suppress_osd``
    Default: no

    Whether to not print the new value of ``--video-zoom`` when using
    ``cursor-centric-zoom``.
