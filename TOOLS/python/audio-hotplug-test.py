from mpvclient import mpv  # type: ignore


@mpv.observe_property("audio-device-list", mpv.MPV_FORMAT_NODE)
def on_audio_device_list_change(data):
    mpv.info("Audio device list changed:")
    for d in data:
        mpv.info("  - '" + d['name'] + "' (" + d["description"] + ")")
