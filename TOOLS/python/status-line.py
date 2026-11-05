# Rebuild the terminal status line as a python script
# Be aware that this will require more cpu power!
# Also, this is based on a rather old version of the
# builtin mpv status line.

from mpvclient import mpv  # type: ignore


new_status = ""

# Add a string to the status line
def atsl(s):
    global new_status
    new_status += s


def update_status_line():
    # Reset the status line
    global new_status
    new_status = ""

    if mpv.get_property_bool("pause"):
        atsl("(Paused) ")
    elif mpv.get_property_bool("paused-for-cache"):
        atsl("(Buffering) ")

    if mpv.get_property_string("aid") != "no":
        atsl("A")

    if mpv.get_property_string("vid") != "no":
        atsl("V")

    atsl(": ")

    atsl(mpv.get_property_osd("time-pos"))

    atsl(" / ")
    atsl(mpv.get_property_osd("duration"))

    atsl(" (")
    atsl(mpv.get_property_osd("percent-pos"))
    atsl("%)")

    r = mpv.get_property_float("speed")
    if r != 1:
        atsl(f" x{r:4.2f}")

    r = mpv.get_property_float("avsync")
    if r is not None:
        atsl(f" A-V: {r}")

    r = mpv.get_property_float("total-avsync-change")
    if abs(r) > 0.05:
        atsl(f" ct:{r:7.3f}")

    r = mpv.get_property_float("decoder-drop-frame-count")
    if r is not None and r > 0:
        atsl(" Late: ")
        atsl(str(r))

    r = mpv.get_property_osd("video-bitrate")
    if r is not None and r != "":
        atsl(" Vb: ")
        atsl(r)

    r = mpv.get_property_osd("audio-bitrate")
    if r is not None and r != "":
        atsl(" Ab: ")
        atsl(r)

    r = mpv.get_property_int("cache")
    if r is not None and r > 0:
        atsl(f" Cache: {r}%% ")

    # Set the new status line
    mpv.set_property_string("options/term-status-msg", new_status)

mpv.add_periodic_timer(1, update_status_line, name="usl")


@mpv.observe_property("pause", mpv.MPV_FORMAT_FLAG)
def on_pause_change(value):
    if value:
        mpv.disable_timer("usl")
    else:
        mpv.add_periodic_timer(1, update_status_line, name="usl")
    mpv.add_timeout(0.1, update_status_line)


def wrap_usl(event):
    update_status_line()

mpv.register_event(mpv.MPV_EVENT_SEEK)(wrap_usl)
