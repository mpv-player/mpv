CONTEXT MENU SCRIPT
===================

The context menu is a menu that pops up on the video window. By default, it is
bound to right click.

menu.conf
---------

You can define your own menu in ``~~/menu.conf`` (see `FILES`_). It is
recommended to use the default ``menu.conf`` from
https://github.com/mpv-player/mpv/blob/master/etc/menu.conf as an example to get
started.

Each line of ``menu.conf`` is a menu item with fields separated by 1 or more
tabs. The first field is the text shown in the menu. The second field is usually
the command that is run when that item is selected. Fields from the third
onwards can specify ``checked=``, ``disabled=`` and ``hidden=`` states in the
same way as `Conditional auto profiles`_.

When there is no command, the item will open a submenu. Fields below indented
with leading whitespace are added to this submenu. Nested submenu items are
defined by adding more leading whitespace than the parent menu entry.

Empty lines are interpreted as separators.

The second field can also be one of the following tokens to make that entry a
submenu with the relative items: ``$playlist``, ``$tracks``, ``$video-tracks``,
``$audio-tracks``, ``$sub-tracks``, ``$secondary-sub-tracks``, ``$chapters``,
``$editions``, ``$audio-devices``, ``$profiles``. These menus are automatically
disabled when empty.

Script messages
---------------

``open``
    Show the context menu.

``select``
    Select the focused item when there is one.

Configuration
-------------

This script can be customized through a config file
``script-opts/context_menu.conf`` placed in mpv's user directory and through
the ``--script-opts`` command-line option. The configuration syntax is
described in `mp.options functions`_.

Configurable Options
~~~~~~~~~~~~~~~~~~~~

``font_size``
    Default: 14

    The font size.

``gap``
    Default: 0.2

    The gap between menu items, specified as a percentage the font size.

``padding_x``
    Default: 8

    The horizontal padding of the menu.

``padding_y``
    Default: 4

    The vertical padding of the menu.

``menu_outline_size``
    Default: 0

    The size of the menu's border.

``menu_outline_color``
    Default: ``#FFFFFF``

    The color of the menu's border.

``corner_radius``
    Default: 5

    The radius of the menu's corners.

``scale_with_window``
    Default: auto

    Whether to scale sizes with the window height. Can be ``yes``, ``no``, or
    ``auto``, which follows the value of ``--osd-scale-by-window``.

    When sizes aren't scaled with the window, they are scaled by
    ``display-hidpi-scale``.

``focused_color``
    Default: ``#222222``

    The color of the focused item.

``focused_back_color``
    Default: ``#FFFFFF``

    The background color of the focused item.

``disabled_color``
    Default: ``#555555``

    The color of disabled items.

``seconds_to_open_submenus``
    Default: 0.2

    The number of seconds to open submenus after the cursor enters items
    associated with one.

``seconds_to_close_submenus``
    Default: 0.2

    The number of seconds to close submenus after the cursor enters a parent
    menu.
