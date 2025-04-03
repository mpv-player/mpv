"""
The python wrapper module for the embedded and extended functionalities
"""

# extension module, see: player/py_extend.c
import mpv as _mpv  # type: ignore

import sys
import traceback
import typing
from threading import Timer
from io import StringIO
from pathlib import Path

__all__ = ["mpv", "Mpv"]


def read_exception(excinfo):
    f = StringIO()
    traceback.print_exception(*excinfo, file=f)
    f.seek(0)
    try:
        return f.read()
    finally:
        f.close()


class State:
    pause = False


state = State()


class Options:

    def typeconv(self, desttypeval, val):
        if isinstance(desttypeval, bool):
            if val == "yes":
                val = True
            elif val == "no":
                val = False
            else:
                mpv.error("Error: Can't convert '" + val + "' to boolean!")
                val = None
        elif isinstance(desttypeval, int):
            try:
                val = int(val)
            except ValueError:
                mpv.error("Error: Can't convert '" + val + "' to number!")
                val = None
        return val

    def read_options(self, options: dict, identifier: typing.Optional[str] = None,
        on_update: typing.Optional[typing.Callable[[typing.Any], typing.Any]] = None,
    ):
        option_types = options.copy()

        if identifier is None:
            identifier = mpv.name

        mpv.debug(f"reading options for {identifier}")

        # read config file
        conffilename = f"script-opts/{identifier}.conf"
        conffile = mpv.find_config_file(conffilename)
        if conffile is None:
            mpv.debug(f"{conffilename} not found.")
            conffilename = f"lua-settings/{identifier}.conf"
            conffile = mpv.find_config_file(conffilename)
            if conffile:
                mpv.warn("lua-settings/ is deprecated, use directory script-opts/")

        f = conffile and open(conffile)
        if not f:
            mpv.debug(f"{conffilename} not found.")
        else:
            # config exists, read values
            mpv.info("Opened config file " + conffilename + ".")
            linecounter = 1
            for line in f.readlines():
                if line[-1:] == "\r":
                    line = line[:-1]

                if not line.startswith("#"):
                    eqpos = line.find("=")
                    if eqpos != -1:
                        key = line[:eqpos].strip()
                        val = line[eqpos+1:].strip()

                        if (desttypeval := option_types.get(key, None)) is None:
                            mpv.warn(f"{conffilename}:{linecounter} unknown key '{key}', ignoring")
                        else:
                            convval = self.typeconv(desttypeval, val)
                            if convval is None:
                                mpv.error(conffilename+":"+str(linecounter)+
                                    " error converting value '" + val +
                                    "' for key '" + key + "'")
                            else:
                                options[key] = convval
                linecounter += 1
            f.close()

        # parse command-line options
        prefix = identifier + "-"
        # command line options are always applied on top of these
        conf_and_default_opts = options.copy()

        def parse_opts(full, opt):
            for key, val in full.items():
                if key.find(prefix) == 0:
                    key = key[len(prefix):]

                    # match found values with defaults
                    if (desttypeval := option_types.get(key)) is None:
                        mpv.warn("script-opts: unknown key " + key + ", ignoring")
                    else:
                        convval = self.typeconv(desttypeval, val)
                        if convval is None:
                            mpv.error("script-opts: error converting value '" + val +
                                "' for key '" + key + "'")
                        else:
                            opt[key] = convval

        # initial
        parse_opts(mpv.get_property_node("options/script-opts"), options)

        # runtime updates
        if on_update:

            @mpv.observe_property("options/script-opts", mpv.MPV_FORMAT_NODE)
            def on_option_change(val):
                last_opts = options.copy()
                new_opts = conf_and_default_opts.copy()
                parse_opts(val, new_opts)
                changelist = {}
                for k, v in new_opts.items():
                    if last_opts[k] != v:
                        # copy to user
                        options[k] = v
                        changelist[k] = True
                if changelist.keys() and on_update is not None:
                    on_update(changelist)


class Mpv:
    """

    This class wraps the mpv client (/libmpv) API hooks defined in the
    embedded/extended python. See: player/py_extend.c

    """
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

    observe_properties: dict = {}

    options = Options()

    messages: dict = {}

    def register_script_message(self, name):
        def decorate(fn):
            self.messages[name] = fn
        return decorate

    def unregister_script_message(self, name):
        del self.messages[name]

    enabled_events: list = []
    event_handlers: dict = {}

    def register_event(self, name):

        def decorate(fn):
            self.request_event(name)
            l = self.event_handlers.get(name, [])
            l.append(fn)
            self.event_handlers[name] = l

        return decorate

    threads: dict = {}

    @property
    def timers(self):
        return self.threads

    def add_timeout(self, sec, func, *args, name=None, **kwargs):
        t = Timer(sec, func, args=args, kwargs=kwargs)
        t.start()
        if name is None:
            self.next_bid += 1
            name = "timer" + str(self.next_bid)
        self.threads[name] = t

    def clear_timer(self, name):
        self.threads.pop(name).cancel()

    def get_opt(self, key, default=None):
        return self.get_property_node("options/script-opts").get(key, default)

    def print_ref_count(self, obj):
        self.info(f"refcount ({repr(obj)}): {sys.getrefcount(obj)}")

    def log(self, level, *args):
        if not args:
            return
        msg = " ".join([str(msg) for msg in args])
        _mpv.handle_log(self, level, f"{msg}\n")

    def trace(self, *args):
        self.log("trace", *args)

    def debug(self, *args):
        self.log("debug", *args)

    def info(self, *args):
        self.log("info", *args)

    def warn(self, *args):
        self.log("warn", *args)

    def error(self, *a):
        self.log("error", *a)

    def fatal(self, *a):
        self.log("fatal", *a)

    def osd_message(self, text, duration=-1, osd_level=None):
        args = [text, duration]
        if osd_level is not None:
            args.append(osd_level)
        self.commandv("show-text", *args)

    _name = None

    @property
    def name(self):
        return self._name

    def read_script(self, filename):
        file_path = Path(filename).resolve()
        if file_path.is_dir():
            file_path = file_path / "__init__.py"
        with file_path.open("r") as f:
            return str(file_path), f.read()

    def compile_script(self, filename, client_name):
        self._name = client_name
        file_path, source = self.read_script(filename)
        return file_path, compile(source, file_path, "exec")

    def extension_ok(self) -> bool:
        return _mpv.extension_ok()

    def read_ex(self):
        return read_exception(sys.exc_info())

    def call_catch_ex(self, func, *args, **kwargs):
        try:
            func(*args, **kwargs)
        except Exception:
            try:
                self.error(read_exception(sys.exc_info()))
            except Exception:
                pass

    def process_event(self, event_id, event):
        data = event["data"]
        if event_id == self.MPV_EVENT_CLIENT_MESSAGE:
            if data[0] != "key-binding":
                self.call_catch_ex(self.messages[data[0]])
            else:
                cb_name = data[1]
                binding = self.binds[cb_name]
                if data[2][0] == "u" or (binding.get("repeatable", False) \
                        and data[2][0] == "r"):
                    self.call_catch_ex(binding["cb"])

        elif event_id == self.MPV_EVENT_PROPERTY_CHANGE:
            self.notify_observer(event)

        elif event_id in self.enabled_events:
            for cb in self.event_handlers.get(event_id, []):
                self.call_catch_ex(cb, event)

    def command_string(self, command_string):
        return _mpv.command_string(self, command_string)

    def commandv(self, name, *args):
        if len(args) > 50:
            raise ValueError("Too many arguments")
        return _mpv.commandv(self, name, *args)

    def command(self, node):
        """
        :param node: data that resembles an mpv_node; can be a list of such nodes.
        """
        return _mpv.command(self, node)

    async_call_table: dict = {}
    async_last_id = 0

    def command_node_async(self, node):
        self.async_last_id += 1
        return _mpv.command_node_async(self, self.async_last_id, node)

    def command_node_async_callback(self, node):
        def decorate(callback):
            res = self.command_node_async(node)
            if not res:
                error = None  # TODO: figure out error
                self.add_timeout(0, callback, False, None, error)
                return res
            t = {"callback": callback, "id": self.async_last_id}
            self.async_call_table[self.async_last_id] = t
            return t
        return decorate

    def abort_async_command(self, t):
        if id := t["id"]:
            _mpv.abort_async_command(self, id)

    def find_config_file(self, filename):
        return _mpv.find_config_file(self, filename)

    def request_event(self, name):
        if name not in self.enabled_events:
            try:
                self.debug(f"Requesting event {name} (enable)")
                return _mpv.request_event(self, name, 1)  # _mpv.request_event(mpv, event, enable)
            finally:
                self.enabled_events.append(name)

    def enable_messages(self, level):
        return _mpv.enable_messages(self, level)

    def observe_property(self, property_name, mpv_format):
        def decorate(func):
            self.next_bid += 1
            self.observe_properties[self.next_bid] = {
                "callback": func, "id": self.next_bid,
            }
            _mpv.observe_property(self, self.next_bid, property_name, mpv_format)
        return decorate


    def unobserve_property(self, id):
        if id not in self.observe_properties:
            self.error(f"Unknown property observer id: {id}")
            return
        _mpv.unobserve_property(self, id)
        del self.observe_properties[id]

    def set_property(self, property_name, mpv_format, data):
        """
        :param str name: name of the property.

        """
        if not (isinstance(property_name, str) and mpv_format in range(1, 10)):
            self.error("TODO: have a pointer to doc string")
            return
        return _mpv.set_property(self, property_name, mpv_format, data)

    MPV_FORMAT_NODE = 0
    MPV_FORMAT_STRING = 1
    MPV_FORMAT_OSD_STRING = 2
    MPV_FORMAT_FLAG = 3
    MPV_FORMAT_INT64 = 4
    MPV_FORMAT_DOUBLE = 5
    MPV_FORMAT_NODE = 6
    MPV_FORMAT_NODE_ARRAY = 7
    MPV_FORMAT_NODE_MAP = 8
    MPV_FORMAT_BYTE_ARRAY = 9

    def set_property_string(self, name, data):
        return self.set_property(name, self.MPV_FORMAT_STRING, str(data))

    def set_property_osd(self, name, data):
        return self.set_property(name, self.MPV_FORMAT_OSD_STRING, str(data))

    def set_property_bool(self, name, data):
        return self.set_property(name, self.MPV_FORMAT_FLAG, 1 if bool(data) else 0)

    def set_property_int(self, name, data):
        return self.set_property(name, self.MPV_FORMAT_INT64, int(data))

    def set_property_float(self, name, data):
        return self.set_property(name, self.MPV_FORMAT_DOUBLE, float(data))

    def set_property_node(self, name, data):
        return self.set_property(name, self.MPV_FORMAT_NODE, data)

    def del_property(self, name):
        return _mpv.del_property(self, name)

    def get_property(self, property_name, mpv_format):
        if not (isinstance(property_name, str) and mpv_format in range(1, 10)):
            self.error("TODO: have a pointer to doc string")
            return
        return _mpv.get_property(self, property_name, mpv_format)

    def get_property_string(self, name):
        return self.get_property(name, self.MPV_FORMAT_STRING)

    def get_property_osd(self, name):
        return self.get_property(name, self.MPV_FORMAT_OSD_STRING)

    def get_property_bool(self, name):
        return bool(self.get_property(name, self.MPV_FORMAT_FLAG))

    def get_property_int(self, name):
        return self.get_property(name, self.MPV_FORMAT_INT64)

    def get_property_float(self, name):
        return self.get_property(name, self.MPV_FORMAT_DOUBLE)

    def get_property_node(self, name):
        return self.get_property(name, self.MPV_FORMAT_NODE)

    def mpv_input_define_section(self, name, location, contents, builtin, owner):
        self.debug("define_section args:", name, location, contents, builtin, owner)
        return _mpv.mpv_input_define_section(self, name, location, contents, builtin, owner)

    # If a key binding is not defined in the current section, do not search the
    # other sections for it (like the default section). Instead, an unbound
    # key warning will be printed.
    MP_INPUT_EXCLUSIVE = 1
    # Prefer it to other sections.
    MP_INPUT_ON_TOP = 2
    # Let mp_input_test_dragging() return true, even if inside the mouse area.
    MP_INPUT_ALLOW_VO_DRAGGING = 4
    # Don't force mouse pointer visible, even if inside the mouse area.
    MP_INPUT_ALLOW_HIDE_CURSOR = 8

    def mpv_input_enable_section(self, name, flags):
        """
        Args:
            flags: bitwise flags from the values self.MP_INPUT_*
                    `or` (|) them to pass multiple flags.
        """
        return _mpv.mpv_input_enable_section(self, name, flags)

    def set_key_bindings_input_section(self):
        location = f"py_{self.name}_bs"

        builtin_binds = "\n".join(sorted(
            [binding["input"] for binding in self.binds.values() \
                if binding["builtin"] and binding.get("input")]))
        if builtin_binds:
            name = f"py_{self.name}_kbs_builtin"
            self.mpv_input_define_section(name, location, "\n" + builtin_binds, True, self.name)
            self.mpv_input_enable_section(name, self.MP_INPUT_ON_TOP)

        reg_binds = "\n".join(sorted(
            [binding["input"] for binding in self.binds.values() \
                if not binding["builtin"] and binding.get("input")]))
        if reg_binds:
            name = f"py_{self.name}_kbs"
            self.mpv_input_define_section(name, location, "\n" + reg_binds, False, self.name)
            self.mpv_input_enable_section(name, self.MP_INPUT_ON_TOP)

    def set_input_sections(self):
        self.set_key_bindings_input_section()

    def flush(self):
        self.debug(f"Flushing {self.name}")
        self.set_input_sections()
        self.debug(f"Flushed {self.name}")

    binds: dict = {}
    next_bid = 1

    def add_binding(self, key=None, name=None, builtin=False, **opts):
        # copied from defaults.js (not sure what e and emit is yet)
        self.debug(f"loading binding {key}")
        key_data = opts
        self.next_bid += 1
        key_data.update(id=self.next_bid, builtin=builtin)
        if name is None:
            name = f"keybinding_{key}"  # unique name

        def decorate(fn):
            key_data["cb"] = fn

        if key is not None:
            key_data["input"] = key + " script-binding " + self.name + "/" + name
        self.binds[name] = key_data

        return decorate

    def has_binding(self):
        return bool(self.binds)

    def enable_client_message(self):
        self.debug("enabling client message")
        if self.request_event(self.MPV_EVENT_CLIENT_MESSAGE):
            self.debug("client-message enabled")
        else:
            self.debug("failed to enable client-message")

    def notify_observer(self, event):
        setattr(state, event["data"]["name"], event["data"]["value"])
        if (p := self.observe_properties.get(event["reply_userdata"], None)) is not None:
            self.call_catch_ex(p["callback"], event["data"]["value"])

    def handle_event(self, event):
        """
        Returns:
            boolean specifying whether some event loop breaking
            condition has been satisfied.
        """
        event_id = event["event_id"]

        if event_id != self.MPV_EVENT_NONE:
            self.debug(f"event: {event}\n")
        else:
            return True

        self.process_event(event_id, event)

        if event_id == self.MPV_EVENT_SHUTDOWN:
            return False

        return True

    def command_reply(self, event):
        item_id, data = event["reply_userdata"], event["data"]
        t = self.async_call_table[item_id]
        cb = t["callback"]
        del self.async_call_table[item_id]

        if event["error"]:
            self.call_catch_ex(cb, False, None, event["error_message"])
        else:
            self.call_catch_ex(cb, True, data, None)

    def run(self):
        self.flush()
        self.enable_client_message()

        self.register_event(self.MPV_EVENT_COMMAND_REPLY)(self.command_reply)

        self.debug(f"Running {self.name}")
        _mpv.run_event_loop(self)
        self.clean_up()

    def clean_up(self):
        self.clear_timers()

    def clear_timers(self):
        for name in self.threads.keys():
            self.clear_timer(name)


mpv = Mpv()
