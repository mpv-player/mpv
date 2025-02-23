LUA SCRIPTING
=============

mpv can load Lua scripts. (See `Script location`_.)

mpv provides the built-in module ``mp``, which contains functions to send
commands to the mpv core and to retrieve information about playback state, user
settings, file information, and so on.

These scripts can be used to control mpv in a similar way to slave mode.
Technically, the Lua code uses the client API internally.

Example
-------

A script which leaves fullscreen mode when the player is paused:

::

    function on_pause_change(name, value)
        if value == true then
            mp.set_property("fullscreen", "no")
        end
    end
    mp.observe_property("pause", "bool", on_pause_change)


Script location
---------------

Scripts can be passed to the ``--script`` option, and are automatically loaded
from the ``scripts`` subdirectory of the mpv configuration directory (usually
``~/.config/mpv/scripts/``).

A script can be a single file. The file extension is used to select the
scripting backend to use for it. For Lua, it is ``.lua``. If the extension is
not recognized, an error is printed. (If an error happens, the extension is
either mistyped, or the backend was not compiled into your mpv binary.)

mpv internally loads the script's name by stripping the ``.lua`` extension and
replacing all nonalphanumeric characters with ``_``. E.g., ``my-tools.lua``
becomes ``my_tools``. If there are several scripts with the same name, it is
made unique by appending a number. This is the name returned by
``mp.get_script_name()``.

Entries with ``.disable`` extension are always ignored.

If a script is a directory (either if a directory is passed to ``--script``,
or any sub-directories in the script directory, such as for example
``~/.config/mpv/scripts/something/``), then the directory represents a single
script. The player will try to load a file named ``main.x``, where ``x`` is
replaced with the file extension. For example, if ``main.lua`` exists, it is
loaded with the Lua scripting backend.

You must not put any other files or directories that start with ``main.`` into
the script's top level directory. If the script directory contains for example
both ``main.lua`` and ``main.js``, only one of them will be loaded (and which
one depends on mpv internals that may change any time). Likewise, if there is
for example ``main.foo``, your script will break as soon as mpv adds a backend
that uses the ``.foo`` file extension.

mpv also appends the top level directory of the script to the start of Lua's
package path so you can import scripts from there too. Be aware that this will
shadow Lua libraries that use the same package path. (Single file scripts do not
include mpv specific directories in the Lua package path. This was silently
changed in mpv 0.32.0.)

Using a script directory is the recommended way to package a script that
consists of multiple source files, or requires other files (you can use
``mp.get_script_directory()`` to get the location and e.g. load data files).

Making a script a git repository, basically a repository which contains a
``main.lua`` file in the root directory, makes scripts easily updateable
(without the dangers of auto-updates). Another suggestion is to use git
submodules to share common files or libraries.

Details on the script initialization and lifecycle
--------------------------------------------------

Your script will be loaded by the player at program start from the ``scripts``
configuration subdirectory, or from a path specified with the ``--script``
option. Some scripts are loaded internally (like ``--osc``). Each script runs in
its own thread. Your script is first run "as is", and once that is done, the event loop
is entered. This event loop will dispatch events received by mpv and call your
own event handlers which you have registered with ``mp.register_event``, or
timers added with ``mp.add_timeout`` or similar. Note that since the
script starts execution concurrently with player initialization, some properties
may not be populated with meaningful values until the relevant subsystems have
initialized. Rather than retrieving these properties at the top of scripts, you
should use ``mp.observe_property`` or read them within event handlers.

When the player quits, all scripts will be asked to terminate. This happens via
a ``shutdown`` event, which by default will make the event loop return. If your
script got into an endless loop, mpv will probably behave fine during playback,
but it won't terminate when quitting, because it's waiting on your script.

Internally, the C code will call the Lua function ``mp_event_loop`` after
loading a Lua script. This function is normally defined by the default prelude
loaded before your script (see ``player/lua/defaults.lua`` in the mpv sources).
The event loop will wait for events and dispatch events registered with
``mp.register_event``. It will also handle timers added with ``mp.add_timeout``
and similar (by waiting with a timeout).

Since mpv 0.6.0, the player will wait until the script is fully loaded before
continuing normal operation. The player considers a script as fully loaded as
soon as it starts waiting for mpv events (or it exits). In practice this means
the player will more or less hang until the script returns from the main chunk
(and ``mp_event_loop`` is called), or the script calls ``mp_event_loop`` or
``mp.dispatch_events`` directly. This is done to make it possible for a script
to fully setup event handlers etc. before playback actually starts. In older
mpv versions, this happened asynchronously. With mpv 0.29.0, this changes
slightly, and it merely waits for scripts to be loaded in this manner before
starting playback as part of the player initialization phase. Scripts run though
initialization in parallel. This might change again.

mp functions
------------

The ``mp`` module is preloaded, although it can be loaded manually with
``require 'mp'``. It provides the core client API.

``mp.command(string)``
    Run the given command. This is similar to the commands used in input.conf.
    See `List of Input Commands`_.

    By default, this will show something on the OSD (depending on the command),
    as if it was used in ``input.conf``. See `Input Command Prefixes`_ how
    to influence OSD usage per command.

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

    Note that properties are *not* expanded.  You can use either ``mp.command``,
    the ``expand-properties`` prefix, or the ``mp.get_property`` family of
    functions.

    Unlike ``mp.command``, this will not use OSD by default either (except
    for some OSD-specific commands).

``mp.command_native(table [,def])``
    Similar to ``mp.commandv``, but pass the argument list as table. This has
    the advantage that in at least some cases, arguments can be passed as
    native types. It also allows you to use named argument.

    If the table is an array, each array item is like an argument in
    ``mp.commandv()`` (but can be a native type instead of a string).

    If the table contains string keys, it's interpreted as command with named
    arguments. This requires at least an entry with the key ``name`` to be
    present, which must be a string, and contains the command name. The special
    entry ``_flags`` is optional, and if present, must be an array of
    `Input Command Prefixes`_ to apply. All other entries are interpreted as
    arguments.

    Returns a result table on success (usually empty), or ``def, error`` on
    error. ``def`` is the second parameter provided to the function, and is
    nil if it's missing.

``mp.command_native_async(table [,fn])``
    Like ``mp.command_native()``, but the command is ran asynchronously (as far
    as possible), and upon completion, fn is called. fn has three arguments:
    ``fn(success, result, error)``:

         ``success``
            Always a Boolean and is true if the command was successful,
            otherwise false.

        ``result``
            The result value (can be nil) in case of success, nil otherwise (as
            returned by ``mp.command_native()``).

        ``error``
            The error string in case of an error, nil otherwise.

    Returns a table with undefined contents, which can be used as argument for
    ``mp.abort_async_command``.

    If starting the command failed for some reason, ``nil, error`` is returned,
    and ``fn`` is called indicating failure, using the same error value.

    ``fn`` is always called asynchronously, even if the command failed to start.

``mp.abort_async_command(t)``
    Abort a ``mp.command_native_async`` call. The argument is the return value
    of that command (which starts asynchronous execution of the command).
    Whether this works and how long it takes depends on the command and the
    situation. The abort call itself is asynchronous. Does not return anything.

``mp.del_property(name)``
    Delete the given property. See ``mp.get_property`` and `Properties`_ for more
    information about properties. Most properties cannot be deleted.

    Returns true on success, or ``nil, error`` on error.

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
    Similar to ``mp.get_property``, but return the property value as Boolean.

    Returns a Boolean on success, or ``def, error`` on error.

``mp.get_property_number(name [,def])``
    Similar to ``mp.get_property``, but return the property value as number.

    Note that while Lua does not distinguish between integers and floats,
    mpv internals do. This function simply request a double float from mpv,
    and mpv will usually convert integer property values to float.

    Returns a number on success, or ``def, error`` on error.

``mp.get_property_native(name [,def])``
    Similar to ``mp.get_property``, but return the property value using the best
    Lua type for the property. Most time, this will return a string, Boolean,
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
    Boolean value.

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

    Since there are several data types which cannot represented natively in
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

``mp.add_key_binding(key, name|fn [,fn [,flags]])``
    Register callback to be run on a key binding. The binding will be mapped to
    the given ``key``, which is a string describing the physical key. This uses
    the same key names as in input.conf, and also allows combinations
    (e.g. ``ctrl+a``). If the key is empty or ``nil``, no physical key is
    registered, but the user still can create own bindings (see below).

    After calling this function, key presses will cause the function ``fn`` to
    be called (unless the user remapped the key with another binding).
    However, if the key binding is canceled , the function will not be called,
    unless ``complex`` flag is set to ``true``, where the function will be
    called with the ``canceled`` entry set to ``true``.

    For example, a canceled key binding can happen in the following situations:

    - If key A is pressed while key B is being held down, key B is logically
      released ("canceled" by key A), which stops the current autorepeat
      action key B has.
    - If key A is pressed while a mouse button is being held down, the mouse
      button is logically released, but the mouse button's action will not be
      called, unless ``complex`` flag is set to ``true``.

    The ``name`` argument should be a short symbolic string. It allows the user
    to remap the key binding via input.conf using the ``script-message``
    command, and the name of the key binding (see below for
    an example). The name should be unique across other bindings in the same
    script - if not, the previous binding with the same name will be
    overwritten. You can omit the name, in which case a random name is generated
    internally. (Omitting works as follows: either pass ``nil`` for ``name``,
    or pass the ``fn`` argument in place of the name. The latter is not
    recommended and is handled for compatibility only.)

    The ``flags`` argument is used for optional parameters. This is a table,
    which can have the following entries:

        ``repeatable``
            If set to ``true``, enables key repeat for this specific binding.
            This option only makes sense when ``complex`` is not set to ``true``.

        ``scalable``
            If set to ``true``, enables key scaling for this specific binding.
            This option only makes sense when ``complex`` is set to ``true``.
            Note that this has no effect if the key binding is invoked by
            ``script-binding`` command, where the scalability of the command
            takes precedence.

        ``complex``
            If set to ``true``, then ``fn`` is called on key down, repeat and up
            events, with the first argument being a table. This table has the
            following entries (and may contain undocumented ones):

                ``event``
                    Set to one of the strings ``down``, ``repeat``, ``up`` or
                    ``press`` (the latter if key up/down/repeat can't be
                    tracked), which indicates the key's logical state.

                ``is_mouse``
                    Boolean: Whether the event was caused by a mouse button.

                ``canceled``
                    Boolean: Whether the event was canceled.
                    Not all types of cancellations set this flag.

                ``key_name``
                    The name of they key that triggered this, or ``nil`` if
                    invoked artificially. If the key name is unknown, it's an
                    empty string.

                ``key_text``
                    Text if triggered by a text key, otherwise ``nil``. See
                    description of ``script-binding`` command for details (this
                    field is equivalent to the 5th argument).

                ``scale``
                    The scale of the key, such as the ones produced by ``WHEEL_*``
                    keys. The scale is 1 if the key is nonscalable.

                ``arg``
                    User-provided string in the ``arg`` argument in the
                    ``script-binding`` command if the key binding is invoked
                    by that command.

    Internally, key bindings are dispatched via the ``script-message-to`` or
    ``script-binding`` input commands and ``mp.register_script_message``.

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
    following into their input.conf to remap the command to the ``y`` key:

    ::

        y script-binding something


    This will print the message when the key ``y`` is pressed. (``x`` will
    still work, unless the user remaps it.)

    You can also explicitly send a message to a named script only. Assume the
    above script was using the filename ``fooscript.lua``:

    ::

        y script-binding fooscript/something

``mp.add_forced_key_binding(...)``
    This works almost the same as ``mp.add_key_binding``, but registers the
    key binding in a way that will overwrite the user's custom bindings in their
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

    If a property is unavailable, or on error, the value argument to ``fn`` is
    ``nil``. (The ``observe_property()`` call always succeeds, even if a
    property does not exist.)

    In some cases the function is not called even if the property changes.
    This depends on the property, and it's a valid feature request to ask for
    better update handling of a specific property.

    If the ``type`` is ``none`` or ``nil``, the change function ``fn`` will be
    called sporadically even if the property doesn't actually change. You should
    therefore avoid using these types.

    You always get an initial change notification. This is meant to initialize
    the user's state to the current value of the property.

``mp.unobserve_property(fn)``
    Undo ``mp.observe_property(..., fn)``. This removes all property handlers
    that are equal to the ``fn`` parameter. This uses normal Lua ``==``
    comparison, so be careful when dealing with closures.

``mp.add_timeout(seconds, fn [, disabled])``
    Call the given function fn when the given number of seconds has elapsed.
    Note that the number of seconds can be fractional. For now, the timer's
    resolution may be as low as 50 ms, although this will be improved in the
    future.

    If the ``disabled`` argument is set to ``true`` or a truthy value, the
    timer will wait to be manually started with a call to its ``resume()``
    method.

    This is a one-shot timer: it will be removed when it's fired.

    Returns a timer object. See ``mp.add_periodic_timer`` for details.

``mp.add_periodic_timer(seconds, fn [, disabled])``
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

        ``is_enabled()``
            Whether the timer is currently enabled or was previously disabled
            (e.g. by ``stop()`` or ``kill()``).

        ``timeout`` (RW)
            This field contains the current timeout period. This value is not
            updated as time progresses. It's only used to calculate when the
            timer should fire next when the timer expires.

            If you write this, you can call ``t:kill() ; t:resume()`` to reset
            the current timeout to the new one. (``t:stop()`` won't use the
            new timeout.)

        ``oneshot`` (RW)
            Whether the timer is periodic (``false``) or fires just once
            (``true``). This value is used when the timer expires (but before
            the timer callback function fn is run).

    Note that these are methods, and you have to call them using ``:`` instead
    of ``.`` (Refer to https://www.lua.org/manual/5.2/manual.html#3.4.9 .)

    Example:

    ::

        seconds = 0
        timer = mp.add_periodic_timer(1, function()
            print("called every second")
            -- stop it after 10 seconds
            seconds = seconds + 1
            if seconds >= 10 then
                timer:kill()
            end
        end)


``mp.get_opt(key)``
    Return a setting from the ``--script-opts`` option. It's up to the user and
    the script how this mechanism is used. Currently, all scripts can access
    this equally, so you should be careful about collisions.

``mp.get_script_name()``
    Return the name of the current script. The name is usually made of the
    filename of the script, with directory and file extension removed. If
    there are several scripts which would have the same name, it's made unique
    by appending a number. Any nonalphanumeric characters are replaced with ``_``.

    .. admonition:: Example

        The script ``/path/to/foo-script.lua`` becomes ``foo_script``.

``mp.get_script_directory()``
    Return the directory if this is a script packaged as directory (see
    `Script location`_ for a description). Return nothing if this is a single
    file script.

``mp.osd_message(text [,duration])``
    Show an OSD message on the screen. ``duration`` is in seconds, and is
    optional (uses ``--osd-duration`` by default).

Advanced mp functions
---------------------

These also live in the ``mp`` module, but are documented separately as they
are useful only in special situations.

``mp.get_wakeup_pipe()``
    Calls ``mpv_get_wakeup_pipe()`` and returns the read end of the wakeup
    pipe. This is deprecated, but still works. (See ``client.h`` for details.)

``mp.get_next_timeout()``
    Return the relative time in seconds when the next timer (``mp.add_timeout``
    and similar) expires. If there is no timer, return ``nil``.

``mp.dispatch_events([allow_wait])``
    This can be used to run custom event loops. If you want to have direct
    control what the Lua script does (instead of being called by the default
    event loop), you can set the global variable ``mp_event_loop`` to your
    own function running the event loop. From your event loop, you should call
    ``mp.dispatch_events()`` to dequeue and dispatch mpv events.

    If the ``allow_wait`` parameter is set to ``true``, the function will block
    until the next event is received or the next timer expires. Otherwise (and
    this is the default behavior), it returns as soon as the event loop is
    emptied. It's strongly recommended to use ``mp.get_next_timeout()`` and
    ``mp.get_wakeup_pipe()`` if you're interested in properly working
    notification of new events and working timers.

``mp.register_idle(fn)``
    Register an event loop idle handler. Idle handlers are called before the
    script goes to sleep after handling all new events. This can be used for
    example to delay processing of property change events: if you're observing
    multiple properties at once, you might not want to act on each property
    change, but only when all change notifications have been received.

``mp.unregister_idle(fn)``
    Undo ``mp.register_idle(fn)``. This removes all idle handlers that
    are equal to the ``fn`` parameter. This uses normal Lua ``==`` comparison,
    so be careful when dealing with closures.

``mp.enable_messages(level)``
    Set the minimum log level of which mpv message output to receive. These
    messages are normally printed to the terminal. By calling this function,
    you can set the minimum log level of messages which should be received with
    the ``log-message`` event. See the description of this event for details.
    The level is a string, see ``msg.log`` for allowed log levels.

``mp.register_script_message(name, fn)``
    This is a helper to dispatch ``script-message`` or ``script-message-to``
    invocations to Lua functions. ``fn`` is called if ``script-message`` or
    ``script-message-to`` (with this script as destination) is run
    with ``name`` as first parameter. The other parameters are passed to ``fn``.
    If a message with the given name is already registered, it's overwritten.

    Used by ``mp.add_key_binding``, so be careful about name collisions.

``mp.unregister_script_message(name)``
    Undo a previous registration with ``mp.register_script_message``. Does
    nothing if the ``name`` wasn't registered.

``mp.create_osd_overlay(format)``
    Create an OSD overlay. This is a very thin wrapper around the ``osd-overlay``
    command. The function returns a table, which mostly contains fields that
    will be passed to ``osd-overlay``. The ``format`` parameter is used to
    initialize the ``format`` field. The ``data`` field contains the text to
    be used as overlay. For details, see the ``osd-overlay`` command.

    In addition, it provides the following methods:

    ``update()``
        Commit the OSD overlay to the screen, or in other words, run the
        ``osd-overlay`` command with the current fields of the overlay table.
        Returns the result of the ``osd-overlay`` command itself.

    ``remove()``
        Remove the overlay from the screen. A ``update()`` call will add it
        again.

    Example:

    ::

        ov = mp.create_osd_overlay("ass-events")
        ov.data = "{\\an5}{\\b1}hello world!"
        ov:update()

    The advantage of using this wrapper (as opposed to running ``osd-overlay``
    directly) is that the ``id`` field is allocated automatically.

``mp.get_osd_size()``
    Returns a tuple of ``osd_width, osd_height, osd_par``. The first two give
    the size of the OSD in pixels (for video outputs like ``--vo=xv``, this may
    be "scaled" pixels). The third is the display pixel aspect ratio.

    May return invalid/nonsense values if OSD is not initialized yet.

``exit()`` (global)
    Make the script exit at the end of the current event loop iteration. This
    does not terminate mpv itself or other scripts.

    This can be polyfilled to support mpv versions older than 0.40 with:

    ::

        if not _G.exit then
            function exit()
                mp.keep_running = false
            end
        end

mp.msg functions
----------------

This module allows outputting messages to the terminal, and can be loaded
with ``require 'mp.msg'``.

``msg.log(level, ...)``
    The level parameter is the message priority. It's a string and one of
    ``fatal``, ``error``, ``warn``, ``info``, ``v``, ``debug``, ``trace``. The
    user's settings will determine which of these messages will be
    visible. Normally, all messages are visible, except ``v``, ``debug`` and
    ``trace``.

    The parameters after that are all converted to strings. Spaces are inserted
    to separate multiple parameters.

    You don't need to add newlines.

``msg.fatal(...)``, ``msg.error(...)``, ``msg.warn(...)``, ``msg.info(...)``, ``msg.verbose(...)``, ``msg.debug(...)``, ``msg.trace(...)``
    All of these are shortcuts and equivalent to the corresponding
    ``msg.log(level, ...)`` call.

mp.options functions
--------------------

mpv comes with a built-in module to manage options from config-files and the
command-line. All you have to do is to supply a table with default options to
the read_options function. The function will overwrite the default values
with values found in the config-file and the command-line (in that order).

``options.read_options(table [, identifier [, on_update]])``
    A ``table`` with key-value pairs. The type of the default values is
    important for converting the values read from the config file or
    command-line back. Do not use ``nil`` as a default value!

    The ``identifier`` is used to identify the config-file and the command-line
    options. These needs to unique to avoid collisions with other scripts.
    Defaults to ``mp.get_script_name()`` if the parameter is ``nil`` or missing.

    The ``on_update`` parameter enables run-time updates of all matching option
    values via the ``script-opts`` option/property. If any of the matching
    options changes, the values in the ``table`` (which was originally passed to
    the function) are changed, and ``on_update(list)`` is called. ``list`` is
    a table where each updated option has a ``list[option_name] = true`` entry.
    There is no initial ``on_update()`` call. This never re-reads the config file.
    ``script-opts`` is always applied on the original config file, ignoring
    previous ``script-opts`` values (for example, if an option is removed from
    ``script-opts`` at runtime, the option will have the value in the config
    file). ``table`` entries are only written for option values whose values
    effectively change (this is important if the script changes ``table``
    entries independently).


Example implementation::

    local options = {
        optionA = "defaultvalueA",
        optionB = -0.5,
        optionC = true,
    }

    require "mp.options".read_options(options, "myscript")
    print(options.optionA)


The config file will be stored in ``script-opts/identifier.conf`` in mpv's user
folder. Comment lines can be started with # and stray spaces are not removed.
Boolean values will be represented with yes/no.

Example config::

    # comment
    optionA=Hello World
    optionB=9999
    optionC=no


Command-line options are read from the ``--script-opts`` parameter. To avoid
collisions, all keys have to be prefixed with ``identifier-``.

Example command-line::

     --script-opts=myscript-optionA=TEST,myscript-optionB=0,myscript-optionC=yes


mp.utils functions
------------------

This built-in module provides generic helper functions for Lua, and have
strictly speaking nothing to do with mpv or video/audio playback. They are
provided for convenience. Most compensate for Lua's scarce standard library.

Be warned that any of these functions might disappear any time. They are not
strictly part of the guaranteed API.

``utils.getcwd()``
    Returns the directory that mpv was launched from. On error, ``nil, error``
    is returned.

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

``utils.file_info(path)``
    Stats the given path for information and returns a table with the
    following entries:

        ``mode``
            protection bits (on Windows, always 755 (octal) for directories
            and 644 (octal) for files)
        ``size``
            size in bytes
        ``atime``
            time of last access
        ``mtime``
            time of last modification
        ``ctime``
            time of last metadata change
        ``is_file``
            Whether ``path`` is a regular file (boolean)
        ``is_dir``
            Whether ``path`` is a directory (boolean)

    ``mode`` and ``size`` are integers.
    Timestamps (``atime``, ``mtime`` and ``ctime``) are integer seconds since
    the Unix epoch (Unix time).
    The booleans ``is_file`` and ``is_dir`` are provided as a convenience;
    they can be and are derived from ``mode``.

    On error (e.g. path does not exist), ``nil, error`` is returned.

``utils.split_path(path)``
    Split a path into directory component and filename component, and return
    them. The first return value is always the directory. The second return
    value is the trailing part of the path, the directory entry.

``utils.join_path(p1, p2)``
    Return the concatenation of the 2 paths. Tries to be clever. For example,
    if ``p2`` is an absolute path, ``p2`` is returned without change.

``utils.subprocess(t)``
    Runs an external process and waits until it exits. Returns process status
    and the captured output. This is a legacy wrapper around calling the
    ``subprocess`` command with ``mp.command_native``. It does the following
    things:

    - copy the table ``t``
    - rename ``cancellable`` field to ``playback_only``
    - rename ``max_size`` to ``capture_size``
    - set ``capture_stdout`` field to ``true`` if unset
    - set ``name`` field to ``subprocess``
    - call ``mp.command_native(copied_t)``
    - if the command failed, create a dummy result table
    - copy ``error_string`` to ``error`` field if the string is non-empty
    - return the result table

    It is recommended to use ``mp.command_native`` or ``mp.command_native_async``
    directly, instead of calling this legacy wrapper. It is for compatibility
    only.

    See the ``subprocess`` documentation for semantics and further parameters.

``utils.subprocess_detached(t)``
    Runs an external process and detaches it from mpv's control.

    The parameter ``t`` is a table. The function reads the following entries:

        ``args``
            Array of strings of the same semantics as the ``args`` used in the
            ``subprocess`` function.

    The function returns ``nil``.

    This is a legacy wrapper around calling the ``run`` command with
    ``mp.commandv`` and other functions.

``utils.getpid()``
    Returns the process ID of the running mpv process. This can be used to identify
    the calling mpv when launching (detached) subprocesses.

``utils.get_env_list()``
    Returns the C environment as a list of strings. (Do not confuse this with
    the Lua "environment", which is an unrelated concept.)

``utils.parse_json(str [, trail])``
    Parses the given string argument as JSON, and returns it as a Lua table. On
    error, returns ``nil, error``. (Currently, ``error`` is just a string
    reading ``error``, because there is no fine-grained error reporting of any
    kind.)

    The returned value uses similar conventions as ``mp.get_property_native()``
    to distinguish empty objects and arrays.

    If the ``trail`` parameter is ``true`` (or any value equal to ``true``),
    then trailing non-whitespace text is tolerated by the function, and the
    trailing text is returned as 3rd return value. (The 3rd return value is
    always there, but with ``trail`` set, no error is raised.)

``utils.format_json(v)``
    Format the given Lua table (or value) as a JSON string and return it. On
    error, returns ``nil, error``. (Errors usually only happen on value types
    incompatible with JSON.)

    The argument value uses similar conventions as ``mp.set_property_native()``
    to distinguish empty objects and arrays.

``utils.to_string(v)``
    Turn the given value into a string. Formats tables and their contents. This
    doesn't do anything special; it is only needed because Lua is terrible.

mp.input functions
--------------------

This module lets scripts get textual input from the user using the console
REPL.

``input.get(table)``
    Show the console to let the user enter text.

    The following entries of ``table`` are read:

    ``prompt``
        The string to be displayed before the input field.

    ``submit``
        A callback invoked when the user presses Enter. The first argument is
        the text in the console.

    ``keep_open``
        Whether to keep the console open on submit. Defaults to ``false``.

    ``opened``
        A callback invoked when the console is shown. This can be used to
        present a list of options with ``input.set_log()``.

    ``edited``
        A callback invoked when the text changes. The first argument is the text
        in the console.

    ``complete``
        A callback invoked when the user presses TAB. The first argument is the
        text before the cursor. The callback should return a table of the string
        candidate completion values and the 1-based cursor position from which
        the completion starts. console.lua will filter the suggestions beginning
        with the the text between this position and the cursor, sort them
        alphabetically, insert their longest common prefix, and show them when
        there are multiple ones.

    ``closed``
        A callback invoked when the console is hidden, either because
        ``input.terminate()`` was invoked from the other callbacks, or because
        the user closed it with a key binding. The first argument is the text in
        the console, and the second argument is the cursor position.

    ``default_text``
        A string to pre-fill the input field with.

    ``cursor_position``
        The initial cursor position, starting from 1.

    ``id``
        An identifier that determines which input history and log buffer to use
        among the ones stored for ``input.get()`` calls. The input histories
        and logs are stored in memory and do not persist across different mpv
        invocations. Defaults to the calling script name with ``prompt``
        appended.

``input.terminate()``
    Close the console.

``input.log(message, style, terminal_style)``
    Add a line to the log buffer. ``style`` can contain additional ASS tags to
    apply to ``message``, and ``terminal_style`` can contain escape sequences
    that are used when the console is displayed in the terminal.

``input.log_error(message)``
    Helper to add a line to the log buffer with the same color as the one the
    console uses for errors. Useful when the user submits invalid input.

``input.set_log(log)``
    Replace the entire log buffer.

    ``log`` is a table of strings, or tables with ``text``, ``style`` and
    ``terminal_style`` keys.

    Example:

    ::

        input.set_log({
            "regular text",
            {
                text = "error text",
                style = "{\\c&H7a77f2&}",
                terminal_style = "\027[31m",
            }
        })

``input.select(table)``
    Specify a list of items that are presented to the user for selection.

    The following entries of ``table`` are read:

    ``prompt``
        The string to be displayed before the input field.

    ``items``
        The table of the entries to choose from.

    ``default_item``
        The 1-based integer index of the preselected item.

    ``submit``
        The callback invoked when the user presses Enter. The first argument is
        the 1-based index of the selected item.

    ``keep_open``
        Whether to keep the console open on submit. Defaults to ``false``.

    Example:

        ::

            input.select({
                items = {
                    "First playlist entry",
                    "Second playlist entry",
                },
                submit = function (id)
                    mp.commandv("playlist-play-index", id - 1)
                end,
            })

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

    mp.register_event("file-loaded", my_fn)

For the existing event types, see `List of events`_.

Extras
------

This documents experimental features, or features that are "too special" to
guarantee a stable interface.

``mp.add_hook(type, priority, fn)``
    Add a hook callback for ``type`` (a string identifying a certain kind of
    hook). These hooks allow the player to call script functions and wait for
    their result (normally, the Lua scripting interface is asynchronous from
    the point of view of the player core). ``priority`` is an arbitrary integer
    that allows ordering among hooks of the same kind. Using the value 50 is
    recommended as neutral default value.

    ``fn(hook)`` is the function that will be called during execution of the
    hook. The parameter passed to it (``hook``) is a Lua object that can control
    further aspects about the currently invoked hook. It provides the following
    methods:

        ``defer()``
            Returning from the hook function should not automatically continue
            the hook. Instead, the API user wants to call ``hook:cont()`` on its
            own at a later point in time (before or after the function has
            returned).

        ``cont()``
            Continue the hook. Doesn't need to be called unless ``defer()`` was
            called.

    See `Hooks`_ for currently existing hooks and what they do - only the hook
    list is interesting; handling hook execution is done by the Lua script
    function automatically.
