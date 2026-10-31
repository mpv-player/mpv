import time
from mpvclient import mpv  # type: ignore

def hardsleep():
    # os.execute("sleep 1s")
    time.sleep(1)


hooks = ["on_before_start_file", "on_load", "on_load_fail",
               "on_preloaded", "on_unload", "on_after_end_file"]


def add_hook(name, priority):
    @mpv.add_hook(name, priority)
    def func():
        print("--- hook: " + name)
        hardsleep()
        print("    ... continue")


for name in hooks:
    add_hook(name, 0)

events = [
    mpv.MPV_EVENT_START_FILE,
    mpv.MPV_EVENT_END_FILE,
    mpv.MPV_EVENT_FILE_LOADED,
    mpv.MPV_EVENT_SEEK,
    mpv.MPV_EVENT_PLAYBACK_RESTART,
    mpv.MPV_EVENT_SHUTDOWN,
]


def register_to_event(name):
    @mpv.register_event(name)
    def func(event):
        print(f"--- event: {name}")


for name in events:
    register_to_event(name)


def observe(name):
    @mpv.observe_property(name, mpv.MPV_FORMAT_NODE)
    def func(value):
        print(f"property '{name}' change to '{value}'")


props = ["path", "metadata"]
for name in props:
    observe(name)
