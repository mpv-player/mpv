# This script uses the lavfi idet filter to automatically insert the
# appropriate deinterlacing filter based on a short section of the
# currently playing video.
#
# It registers the key-binding ctrl+d, which when pressed, inserts the filters
# ``vf=idet,lavfi-pullup,idet``. After 4 seconds, it removes these
# filters and decides whether the content is progressive, interlaced, or
# telecined and the interlacing field dominance.
#
# Based on this information, it may set mpv's ``deinterlace`` property (which
# usually inserts the bwdif filter), or insert the ``pullup`` filter if the
# content is telecined.  It also sets field dominance with lavfi setfield.
#
# OPTIONS:
# The default detection time may be overridden by adding
#
# --script-opts=autodeint.detect_seconds=<number of seconds>
#
# to mpv's arguments. This may be desirable to allow idet more
# time to collect data.
#
# To see counts of the various types of frames for each detection phase,
# the verbosity can be increased with
#
# --msg-level=autodeint=v

from mpvclient import mpv  # type: ignore

script_name = mpv.name
detect_label = "%s-detect" % script_name
pullup_label = "%s" % script_name
dominance_label = "%s-dominance" % script_name
ivtc_detect_label = "%s-ivtc-detect" % script_name

progressive, interlaced_tff, interlaced_bff, interlaced = 0, 1, 2, 3

# number of seconds to gather cropdetect data
try:
    detect_seconds = float(mpv.get_opt("%s.detect_seconds" % script_name, 4))
except ValueError:
    detect_seconds = 4


def del_filter_if_present(label):
    # necessary because mp.command('vf del @label:filter') raises an
    # error if the filter doesn't exist
    vfs = mpv.get_property_node("vf")

    for i, vf in enumerate(vfs):
        if vf["label"] == label:
            vfs.pop(i)
            mpv.set_property_node("vf", vfs)
            return True
    return False


def add_vf(label, filter):
    return mpv.command_string('vf add @%s:%s' % (label, filter))


def stop_detect():
    del_filter_if_present(detect_label)
    del_filter_if_present(ivtc_detect_label)


def judge(label):
    # get the metadata
    result = mpv.get_property_node("vf-metadata/%s" % label)
    num_tff          = float(result["lavfi.idet.multiple.tff"])
    num_bff          = float(result["lavfi.idet.multiple.bff"])
    num_progressive  = float(result["lavfi.idet.multiple.progressive"])
    num_undetermined = float(result["lavfi.idet.multiple.undetermined"])
    num_interlaced   = num_tff + num_bff
    num_determined   = num_interlaced + num_progressive

    mpv.info(label + " progressive    = " + str(num_progressive))
    mpv.info(label + " interlaced-tff = " + str(num_tff))
    mpv.info(label + " interlaced-bff = " + str(num_bff))
    mpv.info(label + " undetermined   = " + str(num_undetermined))

    if num_determined < num_undetermined:
        mpv.warn("majority undetermined frames")

    if num_progressive > 20*num_interlaced:
        return progressive
    elif num_tff > 10*num_bff:
        return interlaced_tff
    elif num_bff > 10*num_tff:
        return interlaced_bff
    else:
        return interlaced


def select_filter():
    # handle the first detection filter results
    verdict = judge(detect_label)
    ivtc_verdict = judge(ivtc_detect_label)
    dominance = "auto"
    if verdict == progressive:
        mpv.info("progressive: doing nothing")
        stop_detect()
        del_filter_if_present(dominance_label)
        del_filter_if_present(pullup_label)
        return
    else:
        if verdict == interlaced_tff:
            dominance = "tff"
            add_vf(dominance_label, "setfield=mode=" + dominance)
        elif verdict == interlaced_bff:
            dominance = "bff"
            add_vf(dominance_label, "setfield=mode=" + dominance)
        else:
            del_filter_if_present(dominance_label)

    # handle the ivtc detection filter results
    if ivtc_verdict == progressive:
        mpv.info("telecined with %s field dominance: using pullup" % dominance)
        stop_detect()
    else:
        mpv.info("interlaced with " + dominance +
                    " field dominance: setting deinterlace property")
        del_filter_if_present(pullup_label)
        mpv.set_property_string("deinterlace","yes")
        stop_detect()


@mpv.add_binding("ctrl+d")
def start_detect():
    # exit if detection is already in progress
    if "select_filter" in mpv.timers:
        mpv.warn("already detecting!")
        return

    mpv.set_property_string("deinterlace", "no")
    del_filter_if_present(pullup_label)
    del_filter_if_present(dominance_label)

    # insert the detection filters
    if not (add_vf(detect_label, 'idet') and
            add_vf(dominance_label, 'setfield=mode=auto') and
            add_vf(pullup_label, 'lavfi-pullup') and
            add_vf(ivtc_detect_label, 'idet')):
        mpv.error("failed to insert detection filters")
        return

    def wrap_select_filter():
        select_filter()
        mpv.clear_timer("select_filter")

    # wait to gather data
    mpv.add_timeout(detect_seconds, wrap_select_filter, name="select_filter")
