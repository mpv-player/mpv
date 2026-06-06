# makes mpv disable ontop when pausing and re-enable it again when resuming playback
# please note that this won't do anything if ontop was not enabled before pausing

from mpvclient import mpv  # type: ignore

was_ontop = False


@mpv.observe_property("pause", mpv.MPV_FORMAT_FLAG)
def func(value):
    ontop = mpv.get_property_node("ontop")
    global was_ontop
    if value:
        if ontop:
            mpv.set_property_node("ontop", False)
            was_ontop = True
    else:
        if was_ontop and not ontop:
            mpv.set_property_node("ontop", True)

        was_ontop = False
