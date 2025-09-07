CONTEXT MENU SCRIPT
===================

This script provides a context menu for platforms without integration with a
native context menu. On these platforms, it can be disabled entirely using the
``--load-context-menu=no`` option. On platforms where the integration is
implemented, it is already disabled by default.

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
    Default: 3

    The gap between menu items.

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
