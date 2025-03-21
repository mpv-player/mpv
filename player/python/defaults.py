"""
The python wrapper module for the embedded and extended functionalities
"""

# extension module, see: player/py_extend.c
import mpv as _mpv  # type: ignore

import sys
import traceback
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


class Registry:
    script_message: dict = {}
    binds: dict = {}
    red_flags: list[str] = []


registry = Registry()


class State:
    pause = False


state = State()


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

    def get_opt(self, key, default):
        return self.get_property_node("options/script-opts").get(key, default)

    def print_ref_count(self, obj):
        self.info(f"refcount ({repr(obj)}): {sys.getrefcount(obj)}")

    def _log(self, level, *args):
        if not args:
            return
        msg = " ".join([str(msg) for msg in args])
        _mpv.handle_log(self, level, f"{msg}\n")

    def trace(self, *args):
        self._log("trace", *args)

    def debug(self, *args):
        self._log("debug", *args)

    def info(self, *args):
        self._log("info", *args)

    def warn(self, *args):
        self._log("warn", *args)

    def error(self, *a):
        self._log("error", *a)

    def fatal(self, *a):
        self._log("fatal", *a)

    def osd_message(self, text, duration=None):
        args = [text]
        if duration is not None:
            args.append(duration)
        self.commandv("show-text", text)

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

    def process_event(self, event_id, data):
        if event_id == self.MPV_EVENT_CLIENT_MESSAGE:
            cb_name = data[1]
            if data[2][0] == "u" and cb_name not in registry.red_flags:
                self.debug(f"calling callback {cb_name}")
                try:
                    registry.script_message[cb_name]()
                except Exception:
                    try:
                        self.error(read_exception(sys.exc_info()))
                    except Exception:
                        pass
                    registry.red_flags.append(cb_name)
                self.debug(f"invoked {cb_name}")

    def command_string(self, name):
        return _mpv.command_string(self, name)

    def commandv(self, name, *args):
        return _mpv.commandv(self, name, *args)

    def command(self, node):
        """
        :param node: data that resembles an mpv_node; can be a list of such nodes.
        """
        return _mpv.command(self, node)

    def find_config_file(self, filename):
        return _mpv.find_config_file(self, filename)

    def request_event(self, name, enable):
        return _mpv.request_event(self, name, enable)

    def enable_messages(self, level):
        return _mpv.enable_messages(self, level)

    def observe_property(self, property_name, mpv_format):
        def decorate(func):
            self.next_bid += 1
            self.observe_properties[property_name] = {
                "callback": func, "id": self.next_bid
            }
            _mpv.observe_property(self, self.next_bid, property_name, mpv_format)
        return decorate


    def unobserve_property(self, property_name):
        _mpv.unobserve_property(self, self.observe_properties[property_name]["id"])
        del self.observe_properties[property_name]

    def _set_property(self, property_name, mpv_format, data):
        """
        :param str name: name of the property.

        """
        if not (isinstance(property_name, str) and mpv_format in range(1, 7)):
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
        return self._set_property(name, self.MPV_FORMAT_STRING, str(data))

    def set_property_osd(self, name, data):
        return self._set_property(name, self.MPV_FORMAT_OSD_STRING, str(data))

    def set_property_bool(self, name, data):
        return self._set_property(name, self.MPV_FORMAT_FLAG, 1 if bool(data) else 0)

    def set_property_int(self, name, data):
        return self._set_property(name, self.MPV_FORMAT_INT64, int(data))

    def set_property_float(self, name, data):
        return self._set_property(name, self.MPV_FORMAT_DOUBLE, float(data))

    def set_property_node(self, name, data):
        return self._set_property(name, self.MPV_FORMAT_NODE, data)

    def del_property(self, name):
        return _mpv.del_property(self, name)

    def get_property(self, property_name, mpv_format):
        if not (isinstance(property_name, str) and mpv_format in range(1, 7)):
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
            [binding["input"] for binding in registry.binds.values() \
                if binding["builtin"] and binding.get("input")]))
        if builtin_binds:
            name = f"py_{self.name}_kbs_builtin"
            self.mpv_input_define_section(name, location, "\n" + builtin_binds, True, self.name)
            self.mpv_input_enable_section(name, self.MP_INPUT_ON_TOP)

        reg_binds = "\n".join(sorted(
            [binding["input"] for binding in registry.binds.values() \
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

    next_bid = 1

    def add_binding(self, key=None, name=None, builtin=False, **opts):
        """
        :param str key:
        :param str name:
        :param bool builtin: whether to put the binding in the builtin section;
            this means if the user defines bindings
            using "{name}", they won't be ignored or overwritten - instead,
            they are preferred to the bindings defined with this call
        :param dict opts: boolean members (repeatable, complex)
        """
        # copied from defaults.js (not sure what e and emit is yet)
        self.debug(f"loading binding {key}")
        key_data = opts
        self.next_bid += 1
        key_data.update(id=self.next_bid, builtin=builtin)
        if name is None:
            name = f"keybinding_{key}"  # unique name

        def decorate(fn):
            registry.script_message[name] = fn

            def key_cb(state):
                # mpv.debug(state)
                # emit = state[1] == "m" if e == "u" else e == "d"
                # if (emit or e == "p" or e == "r") and key_data.get("repeatable", False):
                #     fn()
                fn()
            key_data["cb"] = key_cb

        if key is not None:
            key_data["input"] = key + " script-binding " + self.name + "/" + name
        registry.binds[name] = key_data

        return decorate

    def has_binding(self):
        return bool(registry.binds)

    def enable_client_message(self):
        if registry.binds:
            self.debug("enabling client message")
            if self.request_event(self.MPV_EVENT_CLIENT_MESSAGE, 1):
                self.debug("client-message enabled")
            else:
                self.debug("failed to enable client-message")

    def notify_observer(self, name, data):
        if (p := self.observe_properties.get(name, None)) is not None:
            try:
                p['callback'](data)
            except Exception:
                try:
                    self.error(read_exception(sys.exc_info()))
                except Exception:
                    pass

    def handle_event(self, arg):
        """
        Returns:
            boolean specifying whether some event loop breaking
            condition has been satisfied.
        """
        event_id, data = arg
        self.debug(f"event_id: {event_id} data: {data}\n")
        if event_id == self.MPV_EVENT_SHUTDOWN:
            raise ValueError("MPV_EVENT_SHUTDOWN must be handled in lower level API.")
        elif event_id == self.MPV_EVENT_NONE:
            return False
        elif event_id == self.MPV_EVENT_PROPERTY_CHANGE:
            name, data = data
            setattr(state, name, data)
            self.notify_observer(name, data)
            return False
        else:
            self.process_event(event_id, data)
        return False

    def run(self):
        self.flush()
        self.enable_client_message()
        self.debug(f"Running {self.name}")
        _mpv.run_event_loop(self)

    def do_clean_up(self):
        raise NotImplementedError


mpv = Mpv()
