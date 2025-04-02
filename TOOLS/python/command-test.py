# Test script for some command API details.

from mpvclient import mpv


def join(sep, arr, count):
    r = ""
    if count == None:
        count = len(arr)

    for i in range(count):
        if i > 0:
            r += sep
        # r += utils.to_string(arr[i])
        # TODO: what is utils.to_string?
        r += str(arr[i])

    return r


@mpv.observe_property("vo-configured", mpv.MPV_FORMAT_FLAG)
def vo_configured(v):
    if not v:
        return

    mpv.info("async expand-text")
    @mpv.command_node_async_callback(["expand-text", "hello ${path}!"])
    def expand_text(success, data, error):
        mpv.info(f"done async expand-text: {success} {data} {error}")

    # make screenshot writing very slow
    mpv.set_property_string("screenshot-format", "png")
    mpv.set_property_string("screenshot-png-compression", "9")

    mpv.info("Slow screenshot command...")
    result = mpv.command(["screenshot"])
    mpv.info(f"done, res: {result}")

    mpv.info("Slow screenshot async command...")
    @mpv.command_node_async_callback(["screenshot"])
    def screenshot(res, result, error):
        mpv.info(f"done (async), res: {res}")

    mpv.info("Broken screenshot async command...")
    @mpv.command_node_async_callback(["screenshot-to-file", "/nonexistent/bogus.png"])
    def screenshot_to_file(res, val, err):
        mpv.info(f"done err scr: {res} {val} {err}")

    @mpv.command_node_async_callback({"name": "subprocess",
        "args": ["sh", "-c", "echo hi && sleep 10s"],
        "capture_stdout": True})
    def sub1(res, val, err):
        mpv.info(f"done subprocess: {res} {val} {err}")

    def sub2(res, val, err):
        mpv.info(f"done sleep inf subprocess: {res} {val} {err}")

    x = mpv.command_node_async_callback(
        {"name": "subprocess", "args": ["sleep", "inf"]})(sub2)

    def abort_sleep_sub():
        mpv.info("aborting sleep inf subprocess after timeout")
        mpv.abort_async_command(x)

    mpv.add_timeout(15, abort_sleep_sub)

    def subadd(res, val, err):
        mpv.info(f"done sub-add stdin: {res} {val} {err}")

    # (assuming this "freezes")
    y = mpv.command_node_async_callback({"name": "sub-add", "url": "-"})(subadd)

    def abort_subadd():
        mpv.info("aborting sub-add stdin after timeout")
        mpv.abort_async_command(y)

    mpv.add_timeout(20, abort_subadd)

    @mpv.command_node_async_callback({"name": "subprocess", "args": ["wc", "-c"],
                             "stdin_data": "hello", "capture_stdout": True})
    def wcc(res, val, err):
        mpv.info(f"Should be '5': {val["stdout"]}")

    # blocking stdin by default
    @mpv.command_node_async_callback({"name": "subprocess", "args": ["cat"],
                             "capture_stdout": True})
    def cat(_, val, err):
        mpv.info(f"Should be 0: {val}")  # + str(len(val["stdout"])))
        # mpv.info("Should be 0: " + str(len(val["stdout"])))

    # stdin + detached
    @mpv.command_node_async_callback({"name": "subprocess",
                             "args": ["bash", "-c", "(sleep 5s ; cat)"],
                             "stdin_data": "this should appear after 5s.\n",
                             "detach": True})
    def sleepycat(_, val, err):
        mpv.info(f"5s test: {val["status"]}")

    # This should get killed on script exit.
    @mpv.command_node_async_callback({"name": "subprocess", "playback_only": False,
                             "args": ["sleep", "inf"]})
    def sleepinf(_, val, err):
        pass

    # Runs detached; should be killed on player exit (forces timeout)
    mpv.command({"_flags": ["async"], "name": "subprocess",
                       "playback_only": False, "args": ["sleep", "inf"]})


counter = 0


def freeze_test(playback_only):
    # This "freezes" the script, should be killed via timeout.
    global counter
    counter = counter and counter + 1 or 0
    mpv.info("freeze! " + str(counter))
    x = mpv.command({"name": "subprocess", "playback_only": playback_only,
                     "args": ["sleep", "inf"]})
    mpv.info(f"done, killed={x["killed_by_us"]}\n")


@mpv.register_event(mpv.MPV_EVENT_SHUTDOWN)
def ft(event):
    freeze_test(False)


@mpv.observe_property("idle-active", mpv.MPV_FORMAT_NODE)
def ia(data):
    freeze_test(True)
