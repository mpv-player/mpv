PYTHON SCRIPTING
================

..
    Useful links
    ASS markup: https://aegisub.org/docs/latest/ass_tags/

mpv can load Python scripts. (See `Script location`_.)


Python specific options
-----------------------

- enable-python

    Config option (/ runtime option) `enable-python` has the default value
    `false` (/ `no`) and hence by default Python doesn't initialize on runs of
    mpv, unable to load any python script. This option has been set to `false`
    because if there's no script need to run then having python on the heap is a
    waste of resource. To be able to run python scripts, set `enable-pyhton` to
    `yes` on mpv.conf file.


mpv_event as a python dictionary
--------------------------------

Has the following keys::

    {
        event_id: int;
        reply_userdata: int;
        error: int;
        error_message?: string;
        data: any;
    }

- `event_id`; represents an `mpv_event_id`. See: `Event id`_.
- `reply_userdata`; unique id number for a request event.
- `error`; one of mpv's error number or 0.
- `error_message`; (optional) description of the error.
- `data`; type varies depending on the `event_id`::

        (MPV_EVENT_CLIENT_MESSAGE)data: tuple[str];

        (MPV_EVENT_PROPERTY_CHANGE)data: {
            name: str;  # name of the property
            value: any;  # value of the property
        }

        (MPV_EVENT_HOOK)data: {
            name: str;  # hook_name
            id: int;  # id to use to call _mpv.hook_continue
        }


Event id
--------

::

    MPV_EVENT_NONE = 0
    MPV_EVENT_SHUTDOWN = 1
    MPV_EVENT_LOG_MESSAGE = 2
    MPV_EVENT_GET_PROPERTY_REPLY = 3
    MPV_EVENT_SET_PROPERTY_REPLY = 4
    MPV_EVENT_COMMAND_REPLY = 5
    MPV_EVENT_START_FILE = 6
    MPV_EVENT_END_FILE = 7
    MPV_EVENT_FILE_LOADED = 8
    MPV_EVENT_CLIENT_MESSAGE = 16
    MPV_EVENT_VIDEO_RECONFIG = 17
    MPV_EVENT_AUDIO_RECONFIG = 18
    MPV_EVENT_SEEK = 20
    MPV_EVENT_PLAYBACK_RESTART = 21
    MPV_EVENT_PROPERTY_CHANGE = 22
    MPV_EVENT_QUEUE_OVERFLOW = 24
    MPV_EVENT_HOOK = 25


The wrapper module
------------------

The `player/python/defaults.py` is the wrapper module to the internal c level
python extension contained in `player/py_extend.c`. It has the most useful class
`Mpv` giving away the functionalities for calling `mpv` API.

``class Mpv``:

``Mpv.register_event(self, event_name)``
    See: `Possible event names`_ section
    for the argument event_name. Returns a decorator that takes in a function having
    the following signature, with an example::

        @mpv.register_event(mpv.MPV_EVENT_COMMAND_REPLY)
        def command_reply(event):
            print(event["data"])

    So, the function put to the `register_event(en)` decorator receives one
    argument representing an `mpv_event as a python dictionary`_.

``Mpv.add_timeout(self, sec, func, *args, name=None, **kwargs)``
    Utilizes `threading.Timer` to schedule the `func` to run after `sec`
    seconds. `args` and `kwargs` are passed into the `func` while calling it.
    The keyword argument `name` is used to reference the `threading.Timer`
    instance to manipulate it, for example, for cancelling it.

``Mpv.timers``
    Type::

        Mpv.timers: dict[str, treading.Timer]

    Here the keyword argument `name` to `Mpv.add_timeout` used as key. If `name`
    is `None` then `name` is dynamically created.

    Uses::

        if "detect_crop" in mpv.timers:
            mpv.warn("Already cropdetecting!")

    Or::

        mpv.clear_timer("detect_crop")

``Mpv.clear_timer(self, name)``
    Given the name of a timer this function cancels it.

``Mpv.get_opt(self, key, default=None)``
    Returns the option value defined in section `options/script_opts`. If the
    key is not found it returns the `default`.

``Mpv.log(self, level, *args)``
    The following functions can be used to send log messages::

        Mpv.log(self, level, *args)
        Mpv.trace(self, *args)
        Mpv.debug(self, *args)
        Mpv.info(self, *args)
        Mpv.warn(self, *args)
        Mpv.error(self, *args)
        Mpv.fatal(self, *args)

``Mpv.osd_message(self, text, duration=-1, osd_level=None)``
    Displays osd messages. See: `OSD Commands`_ for more detail.

``Mpv.command_string(self, name)``
    Given the string representation of a command `command_string`, runs it.

``Mpv.commandv(self, *args)``
    Given a number of arguments as command, runs it. Arguments are parsed into
    string before running the command.

``Mpv.command(self, node)``
    Given an `mpv_node` representation in python data types. Runs the node as a
    command and returns it's result as another `mpv_node` python representation.

``Mpv.command_node_async_callback(self, node)``
    Returns a decorator which on invoke calls `Mpv.command_node_async` as the
    given `node` and registers a given function to call with the result when the
    async command returns. The decorator function return a `registry entry` of the
    following form::

        {"callback": callback_function, "id": async_command_id}

``Mpv.abort_async_command(self, registry_entry)``
    Given a `registry entry` described above, this function cancels an async
    command referenced by `registry_entry["id"]`.


``Mpv.find_config_file(self, filename)``
    Given the filename return an mpv configuration file. `None` if file not
    found.

``Mpv.request_event(self, event_name)``
    Given an `event_name` of the form `mpv.MPV_EVENT_CLIENT_MESSAGE` mpv enables
    messages when the event has occurred.

``Mpv.enable_messages(self, level)``
    Given a log `level`, mpv enables log messages above this `level` from the
    client.

``Mpv.observe_property(self, property_name, mpv_format)``
    Returns a decorator which takes in a function to invoke (with the property
    value) when the observed property has changed. Example use case::

        from mpvclient import mpv

        @mpv.observe_property("pause", mpv.MPV_FORMAT_NODE)
        def on_pause_change(data):
            if data:
                mpv.osd_message(f"Paused the video for you!")

``Mpv.unobserve_property(self, id)``
    Given the id of a property observer remove the observer.

``Mpv.set_property(self, property_name, mpv_format, data)``
    A set property call to a said mpv_format, set's the property. Property
    setters::

        Mpv.set_property_string(name, data)
        Mpv.set_property_osd(name, data)
        Mpv.set_property_bool(name, data)
        Mpv.set_property_int(name, data)
        Mpv.set_property_float(name, data)
        Mpv.set_property_node(name, data)

``Mpv.del_property(self, name)``
    Delete a previously set property.

``Mpv.get_property(self, property_name, mpv_format)``
    A get property call to a said mpv_format gets the value of the property.
    Property getters::

        Mpv.get_property_string(self, name)
        Mpv.get_property_osd(self, name)
        Mpv.get_property_bool(self, name)
        Mpv.get_property_int(self, name)
        Mpv.get_property_float(self, name)
        Mpv.get_property_node(self, name)


``Mpv.add_binding(self, key=None, name=None, builtin=False, **opts)``

    key

    name

    builtin
        whether to put the binding in the builtin section this means if the user
        defines bindings using "{name}", they won't be ignored or overwritten -
        instead, they are preferred to the bindings defined with this call

    opts
        boolean members (repeatable, complex)


Possible event names
--------------------

::

    mpv.MPV_EVENT_NONE
    mpv.MPV_EVENT_SHUTDOWN
    mpv.MPV_EVENT_LOG_MESSAGE
    mpv.MPV_EVENT_GET_PROPERTY_REPLY
    mpv.MPV_EVENT_SET_PROPERTY_REPLY
    mpv.MPV_EVENT_COMMAND_REPLY
    mpv.MPV_EVENT_START_FILE
    mpv.MPV_EVENT_END_FILE
    mpv.MPV_EVENT_FILE_LOADED
    mpv.MPV_EVENT_CLIENT_MESSAGE
    mpv.MPV_EVENT_VIDEO_RECONFIG
    mpv.MPV_EVENT_AUDIO_RECONFIG
    mpv.MPV_EVENT_SEEK
    mpv.MPV_EVENT_PLAYBACK_RESTART
    mpv.MPV_EVENT_PROPERTY_CHANGE
    mpv.MPV_EVENT_QUEUE_OVERFLOW
    mpv.MPV_EVENT_HOOK
