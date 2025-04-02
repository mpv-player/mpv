PYTHON SCRIPTING
================

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

Has the following keys:

```
{
    event_id: int;
    reply_userdata: int;
    error: int;
    error_message?: string;
    data: any;
}
```

- `event_id` represents an `mpv_event`. See: `Event id`_.
- `reply_userdata` unique id number for a request event.
- `error` on of mpv's error number or 0.
- `error_message` (optional) description of the error.
- `data` varies depending on the `event_id`.


Event id
--------

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

``Mpv.register_event(event_name)``: See: `Possible event names`_ section.
Returns a decorator that takes in a function having the following signature,
with an example:

```
@mpv.register_event(mpv.MPV_EVENT_COMMAND_REPLY)
def command_reply(event):
    print(event["data"])
```

So, the function put to the `register_event(en)` decorator receives one
argument representing an `mpv_event as a python dictionary`_.


Possible event names
--------------------

`mpv.MPV_EVENT_NONE`
`mpv.MPV_EVENT_SHUTDOWN`
`mpv.MPV_EVENT_LOG_MESSAGE`
`mpv.MPV_EVENT_GET_PROPERTY_REPLY`
`mpv.MPV_EVENT_SET_PROPERTY_REPLY`
`mpv.MPV_EVENT_COMMAND_REPLY`
`mpv.MPV_EVENT_START_FILE`
`mpv.MPV_EVENT_END_FILE`
`mpv.MPV_EVENT_FILE_LOADED`
`mpv.MPV_EVENT_CLIENT_MESSAGE`
`mpv.MPV_EVENT_VIDEO_RECONFIG`
`mpv.MPV_EVENT_AUDIO_RECONFIG`
`mpv.MPV_EVENT_SEEK`
`mpv.MPV_EVENT_PLAYBACK_RESTART`
`mpv.MPV_EVENT_PROPERTY_CHANGE`
`mpv.MPV_EVENT_QUEUE_OVERFLOW`
`mpv.MPV_EVENT_HOOK`
