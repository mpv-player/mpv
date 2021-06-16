OSD HELP
========

This builtin script will show the currently active input bindings if asked
to. It can be disabled entirely using the ``--load-help=no`` option.

Keybindings
-----------

= =======================
h Show input bindings
H Show raw input bindings
= =======================

Configuration
-------------

This script can be customized through a config file ``script-opts/help.conf``
placed in mpv's user directory and through the ``--script-opts`` command-line
option. The configuration syntax is described in `ON SCREEN CONTROLLER`_.

Configurable Options
~~~~~~~~~~~~~~~~~~~~

====================== ======= ================================================
Option                 Default Description
====================== ======= ================================================
show_unkown            no      Show unknown commands in pretty view.
show_prefixes          yes     Show command prefixes for raw commands.
timeout                8       OSD timeout.
margin                 0.05    OSD margin as a ratio of screen size.
horizontal_offset      0.5     Horizontal offset of OSD as a ratio of screen size.
max_lines              30      Maximum number of lines shown on OSD. The font size
                               is adjusted accordingly but will not go below ``min_font_size``.
min_font_size          16      Minimum OSD font size, subject to ``display-hidpi-scale``
                               property. Reduces number of lines shown if necessary.
group_font_ratio       1.5     OSD group font size is adjusted by this ratio.
font_border_ratio      0.075   OSD font border size as a ratio of the font size.
font_shadow_ratio      0       OSD font shadow size as a ratio of the font size.
group_ass_style
key_ass_style
prefix_ass_style
command_ass_style
argument_ass_style
page_ass_style                 Additional ASS styles to apply.
====================== ======= ================================================

Custom Command Descriptions
---------------------------

Most simple commands from ``input.conf`` will automatically get a pretty description
to be displayed. More advanced commands may be described by ensuring they contain a
``print-text`` or ``show-text`` command; their display message will then be used.
Additionally, a JSON-formatted comment for the command may be provided for full
custom command descriptions.

::

    m cycle mute; cycle ao-mute; print-text "Mute audio"
    M cycle mute; seek 10; show-text "Mute audio and seek forward 10s"
    ESC set fullscreen no; set pause yes #{"group":"Example Group","text":"Exit fullscreen and pause"}


Valid properties for JSON comments are:

============ =============================================================
text         Description for this input binding
group        Command group
group_weight Integer to adjust the group position
sort_key     String to addjust the input binding position within the group
============ =============================================================

Scripts may also provide additional input binding desciptions by adding a shared script
property starting with ``help-`` and ending in the script's name. The value of that
property must be a JSON map of command names to descriptors. The descriptors are the same
as the ones provided through comments.

Example lua snippet:

::

    utils.shared_script_property_set('help-' .. mp.get_script_name(), utils.format_json({
        ['command-1'] = { text = 'Description 1' },
        ['command-2'] = { text = 'Description 2', group = 'Custom Group' },
    }))

