import threading
import mpvmainloop


class State(object):
    pause = False


state = State()


class MainLoop(object):
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

    def _log(self, level, *args):
        if not args:
            return
        return mpvmainloop.handle_log(level, f' '.join([str(msg) for msg in args]) + "\n")

    def info(self, *args):
        return self._log("info", *args)

    def debug(self, *args):
        return self._log("debug", *args)

    def warn(self, *args):
        return self._log("warn", *args)

    def error(self, *a):
        return self._log("error", *args)

    def fatal(self, *a):
        return self._log("fatal", *args)

    clients = {}

    def __init__(self, clients=None):
        self.clients = clients

    def get_client_index(self, client_name):
        if client_name not in self.clients:
            raise Exception
        return self.clients[client_name].index

    def handle_event(self, event_id, data):
        """
        Returns:
            boolean specifing whether some event loop breaking
            condition has been satisfied.
        """
        self.debug(f"event_id: {event_id}")
        if event_id == self.MPV_EVENT_SHUTDOWN:
            self.info("shutting down python")
            return True
        elif event_id == self.MPV_EVENT_NONE:
            return False
        elif event_id == self.MPV_EVENT_PROPERTY_CHANGE:
            self.debug(data)
            setattr(state, *data)
            return False
        else:
            mpvmainloop.notify_clients(event_id, data)
        return False

    initialized = False

    def wait_events(self):
        while True:
            event_id, data = mpvmainloop.wait_event(-1)
            if self.handle_event(event_id, data):
                break

    def request_event(self, name, enable):
        return mpvmainloop.request_event(name, enable)

    def enable_client_message(self):
        self.debug("enabling client message")
        if self.request_event(self.MPV_EVENT_CLIENT_MESSAGE, 1):
            self.debug("client-message enabled")
        else:
            self.debug("failed to enable client-message")

    def observe_property(self, property_name, mpv_format, reply_userdata=0):
        return mpvmainloop.observe_property(property_name, mpv_format, reply_userdata)

    def run(self):
        self.clients = mpvmainloop.clients
        # cm = False
        for client in self.clients.values():
            if client.has_binding():
                self.enable_client_message()
                break;
            #     cm = True
            # for property_name, meta in client.observe_properties.items():
            #     self.observe_property(property_name.split("___")[1], meta['mpv_format'], meta['reply_userdata'])
        self.wait_events()


ml = MainLoop()
