# This script cycles between deinterlacing, pullup (inverse
# telecine), and both filters off. It uses the "deinterlace" property
# so that a hardware deinterlacer will be used if available.
#
# It overrides the default deinterlace toggle keybinding "D"
# (shift+d), so that rather than merely cycling the "deinterlace" property
# between on and off, it adds a "pullup" step to the cycle.
#
# It provides OSD feedback as to the actual state of the two filters
# after each cycle step/keypress.
#
# Note: if hardware decoding is enabled, pullup filter will likely
# fail to insert.
#
# TODO: It might make sense to use hardware assisted vdpaupp=pullup,
# if available, but I don't have hardware to test it. Patch welcome.

from mpvclient import mpv  # type: ignore

script_name = mpv.name
pullup_label = f"{script_name}-pullup"


def pullup_on():
    for vf in mpv.get_property_node("vf"):
        if vf["label"] == pullup_label:
            return "yes"
    return "no"


def do_cycle():
    if pullup_on() == "yes":
        # if pullup is on remove it
        mpv.command_string(f"vf remove @{pullup_label}:pullup")
        return
    elif mpv.get_property_string("deinterlace") == "yes":
        # if deinterlace is on, turn it off and insert pullup filter
        mpv.set_property_string("deinterlace", "no")
        mpv.command_string(f"vf add @{pullup_label}:pullup")
        return
    else:
        # if neither is on, turn on deinterlace
        mpv.set_property_string("deinterlace", "yes")
        return


@mpv.add_binding(key="D", name="cycle-deinterlace-pullup")
def cycle_deinterlace_pullup_handler():
    do_cycle()
    # independently determine current state and give user feedback
    mpv.osd_message("deinterlace: {}\npullup: {}".format(
            mpv.get_property_string("deinterlace"), pullup_on()))
