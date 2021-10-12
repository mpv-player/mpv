JAVASCRIPT
==========

JavaScript support in mpv is near identical to its Lua support. Use this section
as reference on differences and availability of APIs, but otherwise you should
refer to the Lua documentation for API details and general scripting in mpv.

Example
-------

JavaScript code which leaves fullscreen mode when the player is paused:

::

    function on_pause_change(name, value) {
        if (value == true)
            mp.set_property("fullscreen", "no");
    }
    mp.observe_property("pause", "bool", on_pause_change);


Similarities with Lua
---------------------

mpv tries to load a script file as JavaScript if it has a ``.js`` extension, but
otherwise, the documented Lua options, script directories, loading, etc apply to
JavaScript files too.

Script initialization and lifecycle is the same as with Lua, and most of the Lua
functions at the modules ``mp``, ``mp.utils``, ``mp.msg`` and ``mp.options`` are
available to JavaScript with identical APIs - including running commands,
getting/setting properties, registering events/key-bindings/hooks, etc.

Differences from Lua
--------------------

No need to load modules. ``mp``, ``mp.utils``,  ``mp.msg`` and ``mp.options``
are preloaded, and you can use e.g. ``var cwd = mp.utils.getcwd();`` without
prior setup.

Errors are slightly different. Where the Lua APIs return ``nil`` for error,
the JavaScript ones return ``undefined``. Where Lua returns ``something, error``
JavaScript returns only ``something`` - and makes ``error`` available via
``mp.last_error()``. Note that only some of the functions have this additional
``error`` value - typically the same ones which have it in Lua.

Standard APIs are preferred. For instance ``setTimeout`` and ``JSON.stringify``
are available, but ``mp.add_timeout`` and ``mp.utils.format_json`` are not.

No standard library. This means that interaction with anything outside of mpv is
limited to the available APIs, typically via ``mp.utils``. However, some file
functions were added, and CommonJS ``require`` is available too - where the
loaded modules have the same privileges as normal scripts.

Language features - ECMAScript 5
--------------------------------

The scripting backend which mpv currently uses is MuJS - a compatible minimal
ES5 interpreter. As such, ``String.substring`` is implemented for instance,
while the common but non-standard ``String.substr`` is not. Please consult the
MuJS pages on language features and platform support - https://mujs.com .

Unsupported Lua APIs and their JS alternatives
----------------------------------------------

``mp.add_timeout(seconds, fn)``  JS: ``id = setTimeout(fn, ms)``

``mp.add_periodic_timer(seconds, fn)``  JS: ``id = setInterval(fn, ms)``

``utils.parse_json(str [, trail])``  JS: ``JSON.parse(str)``

``utils.format_json(v)``  JS: ``JSON.stringify(v)``

``utils.to_string(v)``  see ``dump`` below.

``mp.suspend()`` JS: none (deprecated).

``mp.resume()`` JS: none (deprecated).

``mp.resume_all()`` JS: none (deprecated).

``mp.get_next_timeout()`` see event loop below.

``mp.dispatch_events([allow_wait])`` see event loop below.

Scripting APIs - identical to Lua
---------------------------------

(LE) - Last-Error, indicates that ``mp.last_error()`` can be used after the
call to test for success (empty string) or failure (non empty reason string).
Where the Lua APIs use ``nil`` to indicate error, JS APIs use ``undefined``.

``mp.command(string)`` (LE)

``mp.commandv(arg1, arg2, ...)`` (LE)

``mp.command_native(table [,def])`` (LE)

``id = mp.command_native_async(table [,fn])`` (LE) Notes: ``id`` is true-thy on
success, ``fn`` is called always a-sync, ``error`` is empty string on success.

``mp.abort_async_command(id)``

``mp.get_property(name [,def])`` (LE)

``mp.get_property_osd(name [,def])`` (LE)

``mp.get_property_bool(name [,def])`` (LE)

``mp.get_property_number(name [,def])`` (LE)

``mp.get_property_native(name [,def])`` (LE)

``mp.set_property(name, value)`` (LE)

``mp.set_property_bool(name, value)`` (LE)

``mp.set_property_number(name, value)`` (LE)

``mp.set_property_native(name, value)`` (LE)

``mp.get_time()``

``mp.add_key_binding(key, name|fn [,fn [,flags]])``

``mp.add_forced_key_binding(...)``

``mp.remove_key_binding(name)``

``mp.register_event(name, fn)``

``mp.unregister_event(fn)``

``mp.observe_property(name, type, fn)``

``mp.unobserve_property(fn)``

``mp.get_opt(key)``

``mp.get_script_name()``

``mp.get_script_directory()``

``mp.osd_message(text [,duration])``

``mp.get_wakeup_pipe()``

``mp.register_idle(fn)``

``mp.unregister_idle(fn)``

``mp.enable_messages(level)``

``mp.register_script_message(name, fn)``

``mp.unregister_script_message(name)``

``mp.create_osd_overlay(format)``

``mp.get_osd_size()``  (returned object has properties: width, height, aspect)

``mp.msg.log(level, ...)``

``mp.msg.fatal(...)``

``mp.msg.error(...)``

``mp.msg.warn(...)``

``mp.msg.info(...)``

``mp.msg.verbose(...)``

``mp.msg.debug(...)``

``mp.msg.trace(...)``

``mp.utils.getcwd()`` (LE)

``mp.utils.readdir(path [, filter])`` (LE)

``mp.utils.file_info(path)`` (LE) Note: like lua - this does NOT expand
meta-paths like ``~~/foo`` (other JS file functions do expand meta paths).

``mp.utils.split_path(path)``

``mp.utils.join_path(p1, p2)``

``mp.utils.subprocess(t)``

``mp.utils.subprocess_detached(t)``

``mp.utils.get_env_list()``

``mp.utils.getpid()`` (LE)

``mp.add_hook(type, priority, fn(hook))``

``mp.options.read_options(obj [, identifier [, on_update]])`` (types:
string/boolean/number)

Additional utilities
--------------------

``mp.last_error()``
    If used after an API call which updates last error, returns an empty string
    if the API call succeeded, or a non-empty error reason string otherwise.

``Error.stack`` (string)
    When using ``try { ... } catch(e) { ... }``, then ``e.stack`` is the stack
    trace of the error - if it was created using the ``Error(...)`` constructor.

``print`` (global)
    A convenient alias to ``mp.msg.info``.

``dump`` (global)
    Like ``print`` but also expands objects and arrays recursively.

``mp.utils.getenv(name)``
    Returns the value of the host environment variable ``name``, or
    ``undefined`` if the variable is not defined.

``mp.utils.get_user_path(path)``
    Expands (mpv) meta paths like ``~/x``, ``~~/y``, ``~~desktop/z`` etc.
    ``read_file``, ``write_file``, ``append_file`` and ``require`` already use
    this internally.

``mp.utils.read_file(fname [,max])``
    Returns the content of file ``fname`` as string. If ``max`` is provided and
    not negative, limit the read to ``max`` bytes.

``mp.utils.write_file(fname, str)``
    (Over)write file ``fname`` with text content ``str``. ``fname`` must be
    prefixed with ``file://`` as simple protection against accidental arguments
    switch, e.g. ``mp.utils.write_file("file://~/abc.txt", "hello world")``.

``mp.utils.append_file(fname, str)``
    Same as ``mp.utils.write_file`` if the file ``fname`` does not exist. If it
    does exist then append instead of overwrite.

Note: ``read_file``, ``write_file`` and ``append_file`` throw on errors, allow
text content only.

``mp.get_time_ms()``
    Same as ``mp.get_time()`` but in ms instead of seconds.

``mp.get_script_file()``
    Returns the file name of the current script.

``exit()`` (global)
    Make the script exit at the end of the current event loop iteration.
    Note: please remove added key bindings before calling ``exit()``.

``mp.utils.compile_js(fname, content_str)``
    Compiles the JS code ``content_str`` as file name ``fname`` (without loading
    anything from the filesystem), and returns it as a function. Very similar
    to a ``Function`` constructor, but shows at stack traces as ``fname``.

``mp.module_paths``
    Global modules search paths array for the ``require`` function (see below).

Timers (global)
---------------

The standard HTML/node.js timers are available:

``id = setTimeout(fn [,duration [,arg1 [,arg2...]]])``

``id = setTimeout(code_string [,duration])``

``clearTimeout(id)``

``id = setInterval(fn [,duration [,arg1 [,arg2...]]])``

``id = setInterval(code_string [,duration])``

``clearInterval(id)``

``setTimeout`` and ``setInterval`` return id, and later call ``fn`` (or execute
``code_string``) after ``duration`` ms. Interval also repeat every ``duration``.

``duration`` has a minimum and default value of 0, ``code_string`` is
a plain string which is evaluated as JS code, and ``[,arg1 [,arg2..]]`` are used
as arguments (if provided) when calling back ``fn``.

The ``clear...(id)`` functions cancel timer ``id``, and are irreversible.

Note: timers always call back asynchronously, e.g. ``setTimeout(fn)`` will never
call ``fn`` before returning. ``fn`` will be called either at the end of this
event loop iteration or at a later event loop iteration. This is true also for
intervals - which also never call back twice at the same event loop iteration.

Additionally, timers are processed after the event queue is empty, so it's valid
to use ``setTimeout(fn)`` as a one-time idle observer.

CommonJS modules and ``require(id)``
------------------------------------

CommonJS Modules are a standard system where scripts can export common functions
for use by other scripts. Specifically, a module is a script which adds
properties (functions, etc) to its pre-existing ``exports`` object, which
another script can access with ``require(module-id)``. This runs the module and
returns its ``exports`` object. Further calls to ``require`` for the same module
will return its cached ``exports`` object without running the module again.

Modules and ``require`` are supported, standard compliant, and generally similar
to node.js. However, most node.js modules won't run due to missing modules such
as ``fs``, ``process``, etc, but some node.js modules with minimal dependencies
do work. In general, this is for mpv modules and not a node.js replacement.

A ``.js`` file extension is always added to ``id``, e.g. ``require("./foo")``
will load the file ``./foo.js`` and return its ``exports`` object.

An id which starts with ``./`` or ``../`` is relative to the script or module
which ``require`` it. Otherwise it's considered a top-level id (CommonJS term).

Top-level id is evaluated as absolute filesystem path if possible, e.g. ``/x/y``
or ``~/x``. Otherwise it's considered a global module id and searched according
to ``mp.module_paths`` in normal array order, e.g. ``require("x")`` tries to
load ``x.js`` at one of the array paths, and id ``foo/x`` tries to load ``x.js``
inside dir ``foo`` at one of the paths.

The ``mp.module_paths`` array is empty by default except for scripts which are
loaded as a directory where it contains one item - ``<directory>/modules/`` .
The array may be updated from a script (or using custom init - see below) which
will affect future calls to ``require`` for global module id's which are not
already loaded/cached.

No ``global`` variable, but a module's ``this`` at its top lexical scope is the
global object - also in strict mode. If you have a module which needs ``global``
as the global object, you could do ``this.global = this;`` before ``require``.

Functions and variables declared at a module don't pollute the global object.

Custom initialization
---------------------

After mpv initializes the JavaScript environment for a script but before it
loads the script - it tries to run the file ``init.js`` at the root of the mpv
configuration directory. Code at this file can update the environment further
for all scripts. E.g. if it contains ``mp.module_paths.push("/foo")`` then
``require`` at all scripts will search global module id's also at ``/foo``
(do NOT do ``mp.module_paths = ["/foo"];`` because this will remove existing
paths - like ``<script-dir>/modules`` for scripts which load from a directory).

The custom-init file is ignored if mpv is invoked with ``--no-config``.

Before mpv 0.34, the file name was ``.init.js`` (with dot) at the same dir.

The event loop
--------------

The event loop poll/dispatch mpv events as long as the queue is not empty, then
processes the timers, then waits for the next event, and repeats this forever.

You could put this code at your script to replace the built-in event loop, and
also print every event which mpv sends to your script:

::

    function mp_event_loop() {
        var wait = 0;
        do {
            var e = mp.wait_event(wait);
            dump(e);  // there could be a lot of prints...
            if (e.event != "none") {
                mp.dispatch_event(e);
                wait = 0;
            } else {
                wait = mp.process_timers() / 1000;
                if (wait != 0) {
                    mp.notify_idle_observers();
                    wait = mp.peek_timers_wait() / 1000;
                }
            }
        } while (mp.keep_running);
    }


``mp_event_loop`` is a name which mpv tries to call after the script loads.
The internal implementation is similar to this (without ``dump`` though..).

``e = mp.wait_event(wait)`` returns when the next mpv event arrives, or after
``wait`` seconds if positive and no mpv events arrived. ``wait`` value of 0
returns immediately (with ``e.event == "none"`` if the queue is empty).

``mp.dispatch_event(e)`` calls back the handlers registered for ``e.event``,
if there are such (event handlers, property observers, script messages, etc).

``mp.process_timers()`` calls back the already-added, non-canceled due timers,
and returns the duration in ms till the next due timer (possibly 0), or -1 if
there are no pending timers. Must not be called recursively.

``mp.notify_idle_observers()`` calls back the idle observers, which we do when
we're about to sleep (wait != 0), but the observers may add timers or take
non-negligible duration to complete, so we re-calculate ``wait`` afterwards.

``mp.peek_timers_wait()`` returns the same values as ``mp.process_timers()``
but without doing anything. Invalid result if called from a timer callback.

Note: ``exit()`` is also registered for the ``shutdown`` event, and its
implementation is a simple ``mp.keep_running = false``.
