# This script pauses playback when minimizing the window, and resumes playback
# if it's brought back again. If the player was already paused when minimizing,
# then try not to mess with the pause state.
from mpvclient import mpv  # type: ignore

did_pause_at_minimize = False

@mpv.observe_property("window-minimized", mpv.MPV_FORMAT_NODE)
def on_window_minimized(value):
    pause = mpv.get_property_bool("pause")
    global did_pause_at_minimize

    if value:
        if not pause:
            mpv.set_property_bool("pause", True)
            did_pause_at_minimize = True
    else:
        if did_pause_at_minimize and pause:
            mpv.set_property_bool("pause", False)
            did_pause_at_minimize = False  # Reset to False for probable next cycle
