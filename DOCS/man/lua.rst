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

    Returns ``true`` on success, or ``nil, error`` on error.

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

``mp.get_property_bool(name [,def])``
    Similar to ``mp.get_property``, but return the property value as boolean.

    Returns a boolean on success, or ``def, error`` on error.

``mp.get_property_number(name [,def])``
    Similar to ``mp.get_property``, but return the property value as number.

    Note that while Lua does not distinguish between integers and floats,
    mpv internals do. This function simply request a double float from mpv,
    and mpv will usually convert integer property values to float.

    Returns a number on success, or ``def, error`` on error.

``mp.get_property_native(name [,def])``
    Similar to ``mp.get_property``, but return the property value using the best
    Lua type for the property. Most time, this will return a string, boolean,
    or number. Some properties (for example ``chapter-list``) are returned as
    tables.

    Returns a value on success, or ``def, error`` on error. Note that ``nil``
    might be a possible, valid value too in some corner cases.

``mp.set_property(name, value)``
    Set the given property to the given string value. See ``mp.get_property``
    and `Properties`_ for more information about properties.

    Returns true on success, or ``nil, error`` on error.

``mp.set_property_bool(name, value)``
    Similar to ``mp.set_property``, but set the given property to the given
    boolean value.

``mp.set_property_number(name, value)``
    Similar to ``mp.set_property``, but set the given property to the given
    numeric value.

    Note that while Lua does not distinguish between integers and floats,
    mpv internals do. This function will test whether the number can be
    represented as integer, and if so, it will pass an integer value to mpv,
    otherwise a double float.

``mp.set_property_native(name, value)``
    Similar to ``mp.set_property``, but set the given property using its native
    type.

    Since there are several data types which can not represented natively in
    Lua, this might not always work as expected. For example, while the Lua
    wrapper can do some guesswork to decide whether a Lua table is an array
    or a map, this would fail with empty tables. Also, there are not many
    properties for which it makes sense to use this, instead of
    ``set_property``, ``set_property_bool``, ``set_property_number``.
    For these reasons, this function should probably be avoided for now, except
    for properties that use tables natively.

``mp.get_time()``
    Return the current mpv internal time in seconds as a number. This is
    basically the system time, with an arbitrary offset.

``mp.add_key_binding(key, name|fn [,fn])``
    Register callback to be run on a key binding. The binding will be mapped to
    the given ``key``, which is a string describing the physical key. This uses
    the same key names as in input.conf, and also allows combinations
    (e.g. ``ctrl+a``).

    After calling this function, key presses will cause the function ``fn`` to
    be called (unless the user overmapped the key with another binding).

    The ``name`` argument should be a short symbolic string. It allows the user
    to remap the key binding via input.conf using the ``script_message``
    command, and the name of the key binding (see below for
    an example). The name should be unique across other bindings in the same
    script - if not, the previous binding with the same name will be
    overwritten. You can omit the name, in which case a random name is generated
    internally.

    Internally, key bindings are dispatched via the ``script_message_to`` input
    command and ``mp.register_script_message``.

    Trying to map multiple commands to a key will essentially prefer a random
    binding, while the other bindings are not called. It is guaranteed that
    user defined bindings in the central input.conf are preferred over bindings
    added with this function (but see ``mp.add_forced_key_binding``).

    Example:

    ::

        function something_handler()
            print("the key was pressed")
        end
        mp.add_key_binding("x", "something", something_handler)

    This will print the message ``the key was pressed`` when ``x`` was pressed.

    The user can remap these key bindings. Then the user has to put the
    following into his input.conf to remap the command to the ``y`` key:

    ::

        y script_message something


    This will print the message when the key ``y`` is pressed. (``x`` will
    still work, unless the user overmaps it.)

    You can also explicitly send a message to a named script only. Assume the
    above script was using the filename ``fooscript.lua``:

    ::

        y script_message_to fooscript something

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

``mp.unregister_event(fn)``
    Undo ``mp.register_event(..., fn)``. This removes all event handlers that
    are equal to the ``fn`` parameter. This uses normal Lua ``==`` comparison,
    so be careful when dealing with closures.

``mp.observe_property(name, type, fn)``
    Watch a property for changes. If the property ``name`` is changed, then
    the function ``fn(name)`` will be called. ``type`` can be ``nil``, or be
    set to one of ``none``, ``native``, ``bool``, ``string``, or ``number``.
    ``none`` is the same as ``nil``. For all other values, the new value of
    the property will be passed as second argument to ``fn``, using
    ``mp.get_property_<type>`` to retrieve it. This means if ``type`` is for
    example ``string``, ``fn`` is roughly called as in
    ``fn(name, mp.get_property_string(name))``.

    If possible, change events are coalesced. If a property is changed a bunch
    of times in a row, only the last change triggers the change function. (The
    exact behavior depends on timing and other things.)

    In some cases the function is not called even if the property changes.
    Whether this can happen depends on the property.

    If the ``type`` is ``none`` or ``nil``, sporadic property change events are
    possible. This means the change function ``fn`` can be called even if the
    property doesn't actually change.

``mp.unobserve_property(fn)``
    Undo ``mp.observe_property(..., fn)``. This removes all property handlers
    that are equal to the ``fn`` parameter. This uses normal Lua ``==``
    comparison, so be careful when dealing with closures.

``mp.add_timeout(seconds, fn)``
    Call the given function fn when the given number of seconds has elapsed.
    Note that the number of seconds can be fractional. For now, the timer's
    resolution may be as low as 50 ms, although this will be improved in the
    future.

    This is a one-shot timer: it will be removed when it's fired.

    Returns a timer object. See ``mp.add_periodic_timer`` for details.

``mp.add_periodic_timer(seconds, fn)``
    Call the given function periodically. This is like ``mp.add_timeout``, but
    the timer is re-added after the function fn is run.

    Returns a timer object. The timer object provides the following methods:

        ``stop()``
            Disable the timer. Does nothing if the timer is already disabled.
            This will remember the current elapsed time when stopping, so that
            ``resume()`` essentially unpauses the timer.

        ``kill()``
            Disable the timer. Resets the elapsed time. ``resume()`` will
            restart the timer.

        ``resume()``
            Restart the timer. If the timer was disabled with ``stop()``, this
            will resume at the time it was stopped. If the timer was disabled
            with ``kill()``, or if it's a previously fired one-shot timer (added
            with ``add_timeout()``), this starts the timer from the beginning,
            using the initially configured timeout.


``mp.get_opt(key)``
    Return a setting from the ``--lua-opts`` option. It's up to the user and
    the script how this mechanism is used. Currently, all scripts can access
    this equally, so you should be careful about collisions.

``mp.get_script_name()``
    Return the name of the current script. The name is usually made of the
    filename of the script, with directory and file extension removed. If
    there are several script which would have the same name, it's made unique
    by appending a number.

    .. admonition:: Example

        The script ``/path/to/fooscript.lua`` becomes ``fooscript``.

``mp.osd_message(text [,duration])``
    Show an OSD message on the screen. ``duration`` is in seconds, and is
    optional (uses ``--osd-duration`` by default).

Advanced mp functions
---------------------

These also live in the ``mp`` module, but are documented separately as they
are useful only in special situations.

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

``mp.get_wakeup_pipe()``
    Calls ``mpv_get_wakeup_pipe()`` and returns the read end of the wakeup
    pipe. (See ``client.h`` for details.)

``mp.get_next_timeout()``
    Return the relative time in seconds when the next timer (``mp.add_timeout``
    and similar) expires. If there is no timer, return ``nil``.

``mp.dispatch_events([allow_wait])``
    This can be used to run custom event loops. If you want to have direct
    control what the Lua script does (instead of being called by the default
    event loop), you can set the global variable ``mp_event_loop`` to your
    own function running the event loop. From your event loop, you should call
    ``mp.dispatch_events()`` to unqueue and dispatch mpv events.

    If the ``allow_wait`` parameter is set to ``true``, the function will block
    until the next event is received or the next timer expires. Otherwise (and
    this is the default behavior), it returns as soon as the event loop is
    emptied. It's strongly recommended to use ``mp.get_next_timeout()`` and
    ``mp.get_wakeup_pipe()`` if you're interested in properly working
    notification of new events and working timers.

    This function calls ``mp.suspend()`` and ``mp.resume_all()`` on its own.

``mp.enable_messages(level)``
    Set the minimum log level of which mpv message output to receive. These
    messages are normally printed to the terminal. By calling this function,
    you can set the minimum log level of messages which should be received with
    the ``log-message`` event. See the description of this event for details.
    The level is a string, see ``msg.log`` for allowed log levels.

``mp.register_script_message(name, fn)``
    This is a helper to dispatch ``script_message`` or ``script_message_to``
    invocations to Lua functions. ``fn`` is called if ``script_message`` or
    ``script_message_to`` (with this script as destination) is run
    with ``name`` as first parameter. The other parameters are passed to ``fn``.
    If a message with the given name is already registered, it's overwritten.

    Used by ``mp.add_key_binding``, so be careful about name collisions.

``mp.unregister_script_message(name)``
    Undo a previous registration with ``mp.register_script_message``. Does
    nothing if the ``name`` wasn't registered.

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

mp.options functions
--------------------

mpv comes with a built-in module to manage options from config-files and the
command-line. All you have to do is to supply a table with default options to
the read_options function. The function will overwrite the default values
with values found in the config-file and the command-line (in that order).

``options.read_options(table [, identifier])``
    A ``table`` with key-value pairs. The type of the default values is
    important for converting the values read from the config file or
    command-line back. Do not use ``nil`` as a default value!

    The ``identifier`` is used to identify the config-file and the command-line
    options. These needs to unique to avoid collisions with other scripts.
    Defaults to ``mp.get_script_name()``.


Example implementation::

    require 'mp.options'
    local options = {
        optionA = "defaultvalueA",
        optionB = -0.5,
        optionC = true,
    }
    options.read_options(options, "myscript")
    print(option.optionA)


The config file will be stored in ``lua-settings/identifier.conf`` in mpv's user
folder. Comment lines can be started with # and stray spaces are not removed.
Boolean values will be represented with yes/no.

Example config::

    # comment
    optionA=Hello World
    optionB=9999
    optionC=no


Command-line options are read from the ``--lua-opts`` parameter. To avoid
collisions, all keys have to be prefixed with ``identifier-``.

Example command-line::

     --lua-opts=myscript-optionA=TEST:myscript-optionB=0:myscript-optionC=yes


mp.utils options
----------------

This built-in module provides generic helper functions for Lua, and have
strictly speaking nothing to do with mpv or video/audio playback. They are
provided for convenience. Most compensate for Lua's scarce standard library.

``utils.readdir(path [, filter])``
    Enumerate all entries at the given path on the filesystem, and return them
    as array. Each entry is a directory entry (without the path).
    The list is unsorted (in whatever order the operating system returns it).

    If the ``filter`` argument is given, it must be one of the following
    strings:

        ``files``
            List regular files only. This excludes directories, special files
            (like UNIX device files or FIFOs), and dead symlinks. It includes
            UNIX symlinks to regular files.

        ``dirs``
            List directories only, or symlinks to directories. ``.`` and ``..``
            are not included.

        ``normal``
            Include the results of both ``files`` and ``dirs``. (This is the
            default.)

        ``all``
            List all entries, even device files, dead symlinks, FIFOs, and the
            ``.`` and ``..`` entries.

    On error, ``nil, error`` is returned.

``utils.split_path(path)``
    Split a path into directory component and filename component, and return
    them. The first return value is always the directory. The second return
    value is the trailing part of the path, the directory entry.

``utils.join_path(p1, p2)``
    Return the concatenation of the 2 paths. Tries to be clever. For example,
    if ```p2`` is an absolute path, p2 is returned without change.

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

``file-loaded``
    Happens after a file was loaded and begins playback.

``seek``
    Happens on seeking (including ordered chapter segment changes).

``playback-restart``
    Start of playback after seek or after file was loaded.

``tracks-changed``
    The list of video/audio/sub tracks was updated. (This happens on playback
    start, and very rarely during playback.)

``track-switched``
    A video/audio/subtitle track was switched on or off. This usually happens
    when the user (or a script) changes the subtitle track and so on.

``idle``
    Idle mode is entered. This happens when playback ended, and the player was
    started with ``--idle`` or ``--force-window``. This mode is implicitly ended
    when the ``start-file`` or ``shutdown`` events happen.

``pause``
    Playback was paused. This also happens when for example the player is
    paused on low network cache. Then the event type indicates the pause state
    (like the property "pause" as opposed to the "core-idle" property), and you
    might receive multiple ``pause`` events in a row.

``unpause``
    Playback was unpaused. See above for details.

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
        ``--v`` option, and is also what is used for ``--msg-level``.

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

``metadata-update``
    Metadata (like file tags) was updated.

``chapter-change``
    The current chapter possibly changed.
