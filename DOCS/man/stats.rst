STATS
=====

This builtin script displays information and statistics for the currently
played file. It is enabled by default if mpv was compiled with Lua support.
It can be disabled entirely using the ``--load-stats-overlay=no`` option.

Usage
-----

The following key bindings are active by default unless something else is
already bound to them:

====   ==============================================
i      Show stats for a fixed duration
I      Toggle stats (shown until toggled again)
====   ==============================================

While the stats are visible on screen the following key bindings are active,
regardless of existing bindings. They allow you to switch between *pages* of
stats:

====   ==================
1      Show usual stats
2      Show frame timings
====   ==================

Font
~~~~

For optimal visual experience, a font with support for many font weights and
monospaced digits is recommended. By default, the open source font
`Source Sans Pro <https://github.com/adobe-fonts/source-sans-pro>`_ is used.

Configuration
-------------

This script can be customized through a config file ``script-opts/stats.conf``
placed in mpv's user directory and through the ``--script-opts`` command-line
option. The configuration syntax is described in `ON SCREEN CONTROLLER`_.

Configurable Options
~~~~~~~~~~~~~~~~~~~~

``key_oneshot``
    Default: i
``key_toggle``
    Default: I

    Key bindings to display stats.

``key_page_1``
    Default: 1
``key_page_2``
    Default: 2

    Key bindings for page switching while stats are displayed.

``duration``
    Default: 4

    How long the stats are shown in seconds (oneshot).

``redraw_delay``
    Default: 1

    How long it takes to refresh the displayed stats in seconds (toggling).

``persistent_overlay``
    Default: no

    When `no`, other scripts printing text to the screen can overwrite the
    displayed stats. When `yes`, displayed stats are persistently shown for the
    respective duration. This can result in overlapping text when multiple
    scripts decide to print text at the same time.

``plot_perfdata``
    Default: yes

    Show graphs for performance data (page 2).

``plot_vsync_ratio``
    Default: yes
``plot_vsync_jitter``
    Default: yes

    Show graphs for vsync and jitter values (page 1). Only when toggled.

``flush_graph_data``
    Default: yes

    Clear data buffers used for drawing graphs when toggling.

``font``
    Default: Source Sans Pro

    Font name. Should support as many font weights as possible for optimal
    visual experience.

``font_mono``
    Default: Source Sans Pro

    Font name for parts where monospaced characters are necessary to align
    text. Currently, monospaced digits are sufficient.

``font_size``
    Default: 8

    Font size used to render text.

``font_color``
    Default: FFFFFF

    Font color.

``border_size``
    Default: 0.8

    Size of border drawn around the font.

``border_color``
    Default: 262626

    Color of drawn border.

``alpha``
    Default: 11

    Transparency for drawn text.

``plot_bg_border_color``
    Default: 0000FF

    Border color used for drawing graphs.

``plot_bg_color``
    Default: 262626

    Background color used for drawing graphs.

``plot_color``
    Default: FFFFFF

    Color used for drawing graphs.

Note: colors are given as hexadecimal values and use ASS tag order: BBGGRR
(blue green red).

Different key bindings
~~~~~~~~~~~~~~~~~~~~~~

A different key binding can be defined with the aforementioned options
``key_oneshot`` and ``key_toggle`` but also with commands in ``input.conf``,
for example::

    e script-binding stats/display-stats
    E script-binding stats/display-stats-toggle

Using ``input.conf``, it is also possible to directly display a certain page::

    i script-binding stats/display-page-1
    e script-binding stats/display-page-2
