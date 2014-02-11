LUA SCRIPTING
=============

mpv can load Lua scripts. These scripts can be used to control mpv in a similar
way to slave mode. mpv provides the builtin module ``mp`` (can be loaded
with ``require 'mp'``), which provides functions to send commands to the
mpv core and to retrieve information about playback state, user settings,
file information, and so on.

.. admonition:: Warning

    Lua scripting is work in progress, and it's in a very early stage. When
    writing scripts, rely only on the features and functions documented here.
    Everything else is subject to change.

Mode of operation
-----------------

Your script will be loaded by the player at program start if you pass it to
the ``--lua`` option. Each script runs in its own thread. Your script is
first run "as is", and once that is done, the event loop is entered. This
event loop will dispatch events received by mpv and call your own event
handlers which you have registered with ``mp.register_event``, or timers
added with ``mp.add_timeout`` or similar. When the player quits, all scripts
will be asked to terminate. This happens via a ``shutdown`` event, which by
default will make the event loop return. If your script got into an endless
loop, mpv will probably behave fine during playback, but it won't terminate
when quitting because it's waiting on your script.

Internally, the C code will call the Lua function ``mp_event_loop`` after
loading a Lua script. This function is normally defined by the default prelude
loaded before your script (see ``player/lua/defaults.lua`` in the mpv sources).

mp functions
------------

The ``mp`` module is preloaded, although it can be loaded manually with
``require 'mp'``. It provides the core client API.

``mp.command(string)``
    Run the command the given command. This is similar to the commands used in
    input.conf. See `List of Input Commands`_.

    Returns true on success, or ``nil, error`` on error.

``mp.get_property(name)``
    Return the value of the given property as string. These are the same
    properties as used in input.conf. See `Properties`_ for a list of
    properties. The returned string is formatted similar to ``${=name}``
    (see `Property Expansion`_).

    Returns the string on success, or ``nil, error`` on error.

``mp.get_property_osd(name)``
    Similar to ``mp.get_property``, but return the property value formatted for
    OSD. This is the same string as printed with ``${name}`` when used in
    input.conf.

    Returns the string on success, or ``"", error`` on error.
    Unlike ``get_property()``, assigning the return value to a variable will
    always result in a string.

``mp.set_property(name, value)``
    Set the given property to the given value. See ``mp.get_property`` and
    `Properties`_ for more information about properties.

    Returns true on success, or ``nil, error`` on error.

``mp.get_time()``
    Return the current mpv internal time in seconds as a number. This is
    basically the system time, with an arbitrary offset.

``mp.register_event(name, fn)``
    Call a specific function when an event happens. The event name is a string,
    and the function is a Lua function value.

    Returns true if such an event exists, false otherwise.

    ====================== =====================================================
    Name                   Comment
    ====================== =====================================================
    ``shutdown``
    ``log-message``        for ``mp.enable_messages`` (undocumented)
    ``get-property-reply`` (undocumented)
    ``set-property-reply`` (undocumented)
    ``command-reply``      (undocumented)
    ``start-file``         happens right before a new file is loaded
    ``end-file``           happens after a file was unloaded
    ``playback-start``     happens atfer a file was loaded and begins playback
    ``tracks-changed``     list of tracks was updated
    ``track-switched``     a video/audio/sub track was switched
    ``idle``               idle mode is entered (no file is loaded, ``--idle``)
    ``pause``              player was paused
    ``unpause``            player was unpaused
    ``tick``               called after a video frame was displayed
    ====================== =====================================================

    Example:

    ```
    function my_fn()
        print("start of playback!")
    end

    mp.register_event("playback-start", my_fn)
    ```

``mp.add_timeout(seconds, fn)``
    Call the given function fn when the given number of seconds has elapsed.
    Note that the number of seconds can be fractional. As of now, the timer
    precision may be as worse as 50 ms, though. (This will be improved in the
    future.)

    This is a one-shot timer: it will be removed when it's fired.

    Returns a timer handle. See ``mp.cancel_timer``.

``mp.add_periodic_timer(seconds, fn)``
    Call the given function periodically. This is like ``mp.add_timeout``, but
    the timer is re-added after the function fn is run.

    Returns a timer handle. See ``mp.cancel_timer``.

``mp.cancel_timer(t)``
    Terminate the given timer. t is the value returned by ``mp.add_timeout``
    or ``mp.add_periodic_timer``.

``mp.get_opt(key)``
    Return a setting from the ``--lua-opts`` option. It's up to the user and
    the script how this mechanism is used. Currently, all scripts can access
    this equally, so you should be careful about collisions.

``mp.get_script_name()``
    Return the name of the current script.

``mp.suspend()``
    Suspend the mpv main loop. There is a long-winded explanation of this in
    the C API function ``mpv_suspend()``. In short, this prevents the player
    from displaying the next video frame, so that you don't get blocked when
    trying to access the player.

    This is automatically called by the event handler.

``mp.resume()``
    Undo one ``mp.suspend()`` call. ``mp.suspend()`` increments an internal
    counter, and ``mp.resume()`` decrements it. When 0 is reached, the player
    is actually resumed.

``mp.resume_all()``
    This resets the internal suspend counter and resumes the player. (It's
    like calling ``mp.resume()`` until the player is actually resumed.)

    You might want to call this if you're about to do something that takes a
    long time, but doesn't really need access to the player (like a network
    operation). Note that you still can access the player at any time.

mp.msg functions
----------------

This module allows outputting messages to the terminal, and can be loaded
with ``require 'mp.msg'``.

``msg.log(level, ...)``
    The level parameter is the message priority. It's a string and one of
    ``fatal``, ``error``, ``warn``, ``info``, ``v``, ``debug``. The user's
    settings will determine which of these messages will be visible. Normally,
    all messages are visible, except ``v`` and ``debug``.

    The parameters after that are all converted to strings. Spaces are inserted
    to separate multiple parameters.

    You don't need to add newlines.

``msg.fatal(...)``, ``msg.error(...)``, ``msg.warn(...)``, ``msg.info(...)``, ``msg.verbose(...)``, ``msg.debug(...)``
    All of these are shortcuts and equivalent to the corresponding
    ``msg.log(level, ...)`` call.
