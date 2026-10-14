CALIBRATE
=========

This script finds values of ``--target-peak``, ``--target-contrast``,
``--hdr-reference-white`` and ``--treat-srgb-as-power22`` that match the
display in its viewing environment, by adjusting test patterns by eye. It
requires ``--vo=gpu-next``.

Usage
-----

Run ``script-binding calibrate/start``, or select "Calibrate display" in the
Tools submenu of the context menu. Playback is replaced by the test patterns,
in fullscreen by default, and resumes at the same position afterwards.

The pages shown depend on whether the output is SDR or HDR. Each page shows
instructions and the value found. The text hides while a level is being
adjusted and returns when the input is idle.

The last page lists the results, which are also written to the log. They can
be applied to the running player with ``a``. Nothing is saved, add the values
to ``mpv.conf`` to keep them.

Calibrate in the lighting the display is normally watched in, the black level
includes the ambient light it reflects.

The following keys are active while calibrating:

================================  ============================================
Up, Down, mouse wheel             Adjust the level
PgUp, PgDn, Shift+Up, Shift+Down  Adjust the level in coarse steps
Enter                             Go to the next page, finish on the last one
Left, Right                       Go to the previous or next page
w                                 Cycle the size of the pattern window
r                                 Reset the level of the current page
h                                 Hide or show the instructions
a                                 Apply the results, on the last page
Esc, q                            Finish
================================  ============================================

Pages
-----

Transfer function
    Only with sRGB or gamma 2.2 SDR output. Adjust the disc until it blends
    into the stripes around it when viewed from a normal distance, then
    continue. It may blend in without any adjustment. This tells whether the
    display decodes with the sRGB piecewise curve or a pure 2.2 power
    function, and gives ``--treat-srgb-as-power22``. The stripes have to reach
    the panel unscaled, use the native resolution and disable sharpening in
    the display.

Peak luminance
    Only with HDR output. Raise the level until the disc is no longer visible
    inside the white window, which is where the display clips. This gives
    ``--target-peak``. The peak of many displays depends on the size of the
    bright area, ``w`` changes the size of the window.

Black level
    Lower the level until the patch is no longer visible against the black
    around it. Raise it first if the patch is not visible. This gives
    ``--target-contrast``. With SDR output the patch is HDR content at the
    black point, rendered onto the black of the display, and the level found
    is the black point.

White luminance
    Only with SDR output. The white luminance of a display cannot be found by
    eye. Enter a value measured with a meter or taken from the specification
    of the display, or continue to keep the current one. This gives
    ``--hdr-reference-white``, which is used when HDR content is mapped to the
    SDR output.

SDR reference white
    Only with HDR output and ``--target-colorspace-hint-mode=target``, the
    default. Set the brightness of the SDR picture shown to a comfortable
    level. This gives ``--hdr-reference-white``. It is a preference rather
    than a measurement.

Script bindings
---------------

``start``
    Start the calibration, or finish it if it is running.

Script messages
---------------

``start [hdr|sdr]``
    Like the ``start`` binding. With ``--target-colorspace-hint-mode=source``
    the output follows the content, and the argument selects whether HDR or
    SDR output is calibrated, e.g. ``script-message-to calibrate start hdr``.
    Without it the mode of the current output is used.

Configuration
-------------

This script can be customized through a config file
``script-opts/calibrate.conf`` placed in mpv's user directory and through the
``--script-opts`` command-line option. The configuration syntax is described in
`mp.options functions`_.

Configurable Options
~~~~~~~~~~~~~~~~~~~~

``fullscreen``
    Default: yes

    Whether to switch to fullscreen while calibrating. What surrounds the
    pattern affects the perceived black level and, on displays that limit
    their brightness, the peak luminance.

``black_window``
    Default: 20

    Initial size of the pattern window on the transfer function and black
    level pages, in percent of the screen area.

``peak_window``
    Default: 10

    Initial size of the pattern window on the peak and white luminance pages,
    in percent of the screen area.

``hide_time``
    Default: 1

    Seconds without input after which the text hidden while adjusting returns.
