LUA SCRIPTING
=============

mpv can load Lua scripts. Scripts in ``~/.mpv/lua/`` will be loaded on program
start, or if passed to ``--lua``. mpv provides the builtin module ``mp``, which
provides functions to send commands to the mpv core and to retrieve information
about playback state, user settings, file information, and so on.

These scripts can be used to control mpv in a similar way to slave mode.
Technically, the Lua code uses the client API internally.

Example
-------

A script which leaves fullscreen mode when the player is paused:

::

    function on_pause()
        mp.set_property("fullscreen", "no")
    end
    mp.register_event("pause", on_pause)

This script provides a pretty weird feature, but Lua scripting was made to
allow users implement features which are not going to be added to the mpv core.

Mode of operation
-----------------

Your script will be loaded by the player at program start from ``~/.mpv/lua/``,
or ``--lua``, or in some cases, internally (like ``--osc``). Each script runs
in its own thread. Your script is first run "as is", and once that is done,
the event loop is entered. This event loop will dispatch events received by mpv
and call your own event handlers which you have registered with
``mp.register_event``, or timers added with ``mp.add_timeout`` or similar.

When the player quits, all scripts will be asked to terminate. This happens via
a ``shutdown`` event, which by default will make the event loop return. If your
script got into an endless loop, mpv will probably behave fine during playback
(unless the player is suspended, see ``mp.suspend``), but it won't terminate
when quitting, because it's waiting on your script.

Internally, the C code will call the Lua function ``mp_event_loop`` after
loading a Lua script. This function is normally defined by the default prelude
loaded before your script (see ``player/lua/defaults.lua`` in the mpv sources).
The event loop will wait for events and dispatch events registered with
``mp.register_event``. It will also handle timers added with ``mp.add_timeout``
and similar (by waiting with a timeout).

mp functions
------------

The ``mp`` module is preloaded, although it can be loaded manually with
``require 'mp'``. It provides the core client API.

``mp.command(string)``
    Run the given command. This is similar to the commands used in input.conf.
    See `List of Input Commands`_.

    Returns true on success, or ``nil, error`` on error.

``mp.commandv(arg1, arg2, ...)``
    Similar to ``mp.command``, but pass each command argument as separate
    parameter. This has the advantage that you don't have to care about
    quoting and escaping in some cases.

    Example:

    ::

        mp.command("loadfile " .. filename .. " append")
        mp.commandv("loadfile", filename, "append")

    These two commands are equivalent, except that the first version breaks
    if the filename contains spaces or certain special characters.

``mp.get_property(name [,def])``
    Return the value of the given property as string. These are the same
    properties as used in input.conf. See `Properties`_ for a list of
    properties. The returned string is formatted similar to ``${=name}``
    (see `Property Expansion`_).

    Returns the string on success, or ``def, error`` on error. ``def`` is the
    second parameter provided to the function, and is nil if it's missing.

``mp.get_property_osd(name [,def])``
    Similar to ``mp.get_property``, but return the property value formatted for
    OSD. This is the same string as printed with ``${name}`` when used in
    input.conf.

    Returns the string on success, or ``def, error`` on error. ``def`` is the
    second parameter provided to the function, and is an empty string if it's
    missing. Unlike ``get_property()``, assigning the return value to a variable
    will always result in a string.

``mp.set_property(name, value)``
    Set the given property to the given value. See ``mp.get_property`` and
    `Properties`_ for more information about properties.

    Returns true on success, or ``nil, error`` on error.

``mp.get_time()``
    Return the current mpv internal time in seconds as a number. This is
    basically the system time, with an arbitrary offset.

``mp.register_script_command(name, fn)``
    Register a command named ``name``. If the script receives a message
    with the given name as first argument, ``fn(...)`` is called with the
    rest of the script commands.

    If a command with the given name already exists, it's overwritten.

    This is intended for allowing users to interact the script in some ways
    using the ``script_message`` input command.

    Example:

    In a script, say ``fooscript.lua``:

    ::

        function something_handler(arg1, arg2)
            print("arg1=" .. arg1)
            print("arg2=" .. arg2)
        end
        mp.register_script_command("something", something_handler)

    input.conf:

    ::

        x script_message lua/fooscript something "hello" "you"

    This will print the lines ``arg1=hello`` and ``arg2=you`` when the
    key ``x`` is pressed.

    Also see ``mp.add_key_binding`` how to add key bindings by default.

``mp.unregister_script_command(name)``
    Undo a previous registration with ``mp.register_script_command``. Does
    nothing if the ``name`` wasn't registered.

``mp.add_key_binding(key, name|fn [,fn])``
    Register a key binding. The binding will be mapped to the given ``key``,
    which is a string describing the physical key. This uses the same key
    names as in input.conf, and also allows combinations (e.g. ``ctrl+a``).

    Key bindings are dispatched as script commands. The ``name`` argument is
    the name used to invoke command handlers as registered by
    ``mp.register_script_command``. The name can also be used by users to remap
    the bindings provided by your script (see below).

    If a key binding or a command with the given name already exists, it's
    overwritten.

    The ``fn`` parameter is optional. If provided, it must be a function, and
    will be called when the key is pressed. Actually, this just for
    convenience, and literally calls ``mp.register_script_command(name, fn)``.

    You can also omit the name and only provide a function ``fn`` instead. Then
    a random name is generated internally.

    Example:

    ::

        function something_handler()
            print("the key was pressed")
        end
        mp.add_key_binding("x", "something", something_handler)

    This will print the message ``the key was pressed`` when ``x`` was pressed.

    The user can remap these key bindings. Assume the above script was using
    the filename ``fooscript.lua``, then the user has to put the following
    into his input.conf to remap the command to the ``y`` key:

    ::

        y script_message lua/fooscript something

    This will print the message when the key ``y`` is pressed. (``x`` will
    still work, unless the user overmaps it.)

``mp.add_forced_key_binding(...)``
    This works almost the same as ``mp.add_key_binding``, but registers the
    key binding in a way that will overwrite the user's custom bindings in his
    input.conf. (``mp.add_key_binding`` overwrites default key bindings only,
    but not those by the user's input.conf.)

``mp.remove_key_binding(name)``
    Remove a key binding added with ``mp.add_key_binding`` or
    ``mp.add_forced_key_binding``. Use the same name as you used when adding
    the bindings. It's not possible to remove bindings for which you omitted
    the name.

``mp.register_event(name, fn)``
    Call a specific function when an event happens. The event name is a string,
    and the function fn is a Lua function value.

    Some events have associated data. This is put into a Lua table and passed
    as argument to fn. The Lua table by default contains a ``name`` field,
    which is a string containing the event name. If the event has an error
    associated, the ``error`` field is set to a string describing the error,
    on success it's not set.

    If multiple functions are registered for the same event, they are run in
    registration order, which the first registered function running before all
    the other ones.

    Returns true if such an event exists, false otherwise.

    See `Events`_ and `List of events`_ for details.

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
    Terminate the given timer. t is a timer handle (value returned by
    ``mp.add_timeout`` or ``mp.add_periodic_timer``).

``mp.get_opt(key)``
    Return a setting from the ``--lua-opts`` option. It's up to the user and
    the script how this mechanism is used. Currently, all scripts can access
    this equally, so you should be careful about collisions.

``mp.get_script_name()``
    Return the name of the current script. The name is usually made of the
    filename of the script, with directory and file extension removed, and
    prefixed with ``lua/``. If there are several script which would have the
    same name, it's made unique by appending a number.

    .. admonition:: Example

        The script ``/path/to/fooscript.lua`` becomes ``lua/fooscript``.

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

``mp.enable_messages(level)``
    Set the minimum log level of which mpv message output to receive. These
    messages are normally printed to the terminal. By calling this function,
    you can set the minimum log level of messages which should be received with
    the ``log-message`` event. See the description of this event for details.
    The level is a string, see ``msg.log`` for allowed log levels.

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

Events
------

Events are notifications from player core to scripts. You can register an
event handler with ``mp.register_event``.

Note that all scripts (and other parts of the player) receive events equally,
and there's no such thing as blocking other scripts from receiving events.

Example:

::

    function my_fn(event)
        print("start of playback!")
    end

    mp.register_event("playback-start", my_fn)



List of events
--------------

``start-file``
    Happens right before a new file is loaded. When you receive this, the
    player is loading the file (or possibly already done with it).

``end-file``
    Happens after a file was unloaded. Typically, the player will load the
    next file right away, or quit if this was the last file.

``playback-start``
    Happens atfer a file was loaded and begins playback.

``tracks-changed``
    List of video/audio/sub tracks was updated. (This happens on playback start,
    and very rarely during playback.)

``track-switched``
    A video/audio/sub track was switched. This usually happens on user
    interaction, or if a script changes track.

``idle``
    Idle mode is entered. This happens when playback ended, and the player was
    started with ``--idle`` or ``--force-window``. This mode is implicitly ended
    when the ``start-file`` or ``shutdown`` events happen.

``pause``
    Playback was paused.

``unpause``
    Playback was unpaused.

``tick``
    Called after a video frame was displayed. This is a hack, and you should
    avoid using it. Use timers instead and maybe watch pausing/unpausing events
    to avoid wasting CPU when the player is paused.

``shutdown``
    Sent when the player quits, and the script should terminate. Normally
    handled automatically. See `Mode of operation`_.

``log-message``
    Receives messages enabled with ``mp.enable_messages``. The message data
    is contained in the table passed as first parameter to the event handler.
    The table contains, in addition to the default event fields, the following
    fields:

    ``prefix``
        The module prefix, identifies the sender of the message. This is what
        the terminal player puts in front of the message text when using the
        ``--v`` option, and is also what is used for ``--msglevel``.

    ``level``
        The log level as string. See ``msg.log`` for possible log level names.
        Note that later versions of mpv might add new levels or remove
        (undocumented) existing ones.

    ``text``
        The log message. Note that this is the direct output of a printf()
        style output API. The text will contain embedded newlines, and it's
        possible that a single message contains multiple lines, or that a
        message contains a partial line.

        It's safe to display messages only if they end with a newline character,
        and to buffer them otherwise.

    Keep in mind that these messages are meant to be hints for humans. You
    should not parse them, and prefix/level/text of messages might change
    any time.

``get-property-reply``
    Undocumented (not useful for Lua scripts).

``set-property-reply``
    Undocumented (not useful for Lua scripts).

``command-reply``
    Undocumented (not useful for Lua scripts).

``script-input-dispatch``
    Undocumented (used internally).

``client-message``
    Undocumented (used internally).

``video-reconfig``
    Happens on video output or filter reconfig.

``audio-reconfig``
    Happens on audio output or filter reconfig.
