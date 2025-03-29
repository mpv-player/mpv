# This script adds control to the dynamic range compression ffmpeg
# filter including key bindings for adjusting parameters.
#
# See https://ffmpeg.org/ffmpeg-filters.html#acompressor for explanation
# of the parameters.

import re
import typing
from mpvclient import mpv  # type: ignore

o = dict(
    default_enable = False,
    show_osd = True,
    osd_timeout = 4000,
    filter_label = mpv.name,

    key_toggle = "n",
    key_increase_threshold = "F1",
    key_decrease_threshold = "Shift+F1",
    key_increase_ratio = "F2",
    key_decrease_ratio = "Shift+F2",
    key_increase_knee = "F3",
    key_decrease_knee = "Shift+F3",
    key_increase_makeup = "F4",
    key_decrease_makeup = "Shift+F4",
    key_increase_attack = "F5",
    key_decrease_attack = "Shift+F5",
    key_increase_release = "F6",
    key_decrease_release = "Shift+F6",

    default_threshold = -25.0,
    default_ratio = 3.0,
    default_knee = 2.0,
    default_makeup = 8.0,
    default_attack = 20.0,
    default_release = 250.0,

    step_threshold = -2.5,
    step_ratio = 1.0,
    step_knee = 1.0,
    step_makeup = 1.0,
    step_attack = 10.0,
    step_release = 10.0,
)
mpv.options.read_options(o)

params = [
    dict(name = "attack",    min=0.01, max=2000, hide_default=True,  dB=""   ),
    dict(name = "release",   min=0.01, max=9000, hide_default=True,  dB=""   ),
    dict(name = "threshold", min= -30, max=   0, hide_default=False, dB="dB" ),
    dict(name = "ratio",     min=   1, max=  20, hide_default=False, dB=""   ),
    dict(name = "knee",      min=   1, max=  10, hide_default=True,  dB="dB" ),
    dict(name = "makeup",    min=   0, max=  24, hide_default=False, dB="dB" ),
]

def parse_value(value):
    try:
        return float(re.sub("dB$", "", value))
    except ValueError:
        return None


def format_value(value, dB):
    return f"{value}{dB}"


def show_osd(filter):
    global o
    if not o["show_osd"]:
        return

    if not filter["enabled"]:
        mpv.commandv("show-text", "Dynamic range compressor: disabled", o["osd_timeout"])
        return

    pretty: typing.Union[str, list] = []
    for param in params:
        value = parse_value(filter["params"][param["name"]])
        if not (param["hide_default"] and value == o["default_" + param["name"]]):  # type: ignore
            pretty.append(f"{param["name"].capitalize()}: {value}{param["dB"]}")  # type: ignore

    if not pretty:
        pretty = ""
    else:
        pretty = "\n(" + ", ".join(pretty) + ")"  # type: ignore

    mpv.commandv("show-text", "Dynamic range compressor: enabled" + pretty, o["osd_timeout"])


def get_filter():
    af = mpv.get_property_node("af")

    for i in range(len(af)):
        if af[i]["label"] == o["filter_label"]:
            return af, i

    af.append(dict(
        name = "acompressor",
        label = o["filter_label"],
        enabled = False,
        params = {},
    ))

    for param in params:
        af[len(af) - 1]["params"][param["name"]] = format_value(o["default_" + param["name"]], param["dB"])  # type: ignore

    return af, len(af)


def toggle_acompressor():
    af, i = get_filter()
    af[-1]["enabled"] = not af[-1]["enabled"]
    mpv.set_property_node("af", af)
    show_osd(af[-1])


def update_param(name, increment):
    for param in params:
        if param["name"] == name.lower():
            af, i = get_filter()
            value = parse_value(af[-1]["params"][param["name"]])
            value = max(param["min"], min(value + increment, param["max"]))
            af[-1]["params"][param["name"]] = format_value(value, param["dB"])
            af[-1]["enabled"] = True
            mpv.set_property_node("af", af)
            show_osd(af[-1])
            return

    mpv.error("Unknown parameter '" + name + "'")


mpv.add_binding(o["key_toggle"], name="toggle-acompressor")(toggle_acompressor)


@mpv.add_binding(o["key_increase_threshold"],
    name="acompressor-increase-threshold", repeatable=True)
def increase_threshold():
    update_param("threshold", o["step_threshold"])


@mpv.add_binding(o["key_decrease_threshold"],
    name="acompressor-decrease-threshold", repeatable=True)
def decrease_threshold():
    update_param("threshold", -1 * o["step_threshold"])


@mpv.add_binding(o["key_increase_ratio"],
    name="acompressor-increase-ratio", repeatable=True)
def increase_ratio():
    update_param("ratio", o["step_ratio"])


@mpv.add_binding(o["key_decrease_ratio"],
    name="acompressor-decrease-ratio", repeatable=True)
def decrease_ratio():
    update_param("ratio", -1 * o["step_ratio"])


@mpv.add_binding(o["key_increase_knee"],
    name="acompressor-increase-knee", repeatable=True)
def increase_knee():
    update_param("knee", o["step_knee"])


@mpv.add_binding(o["key_decrease_knee"],
    name="acompressor-decrease-knee", repeatable=True)
def decrease_knee():
    update_param("knee", -1 * o["step_knee"])


@mpv.add_binding(o["key_increase_makeup"],
    name="acompressor-increase-makeup", repeatable=True)
def increase_makeup():
    update_param("makeup", o["step_makeup"])


@mpv.add_binding(o["key_decrease_makeup"],
    name="acompressor-decrease-makeup", repeatable=True)
def decrease_makeup():
    update_param("makeup", -1 * o["step_makeup"])


@mpv.add_binding(o["key_increase_attack"],
    name="acompressor-increase-attack", repeatable=True)
def increase_attack():
    update_param("attack", o["step_attack"])


@mpv.add_binding(o["key_decrease_attack"],
    name="acompressor-decrease-attack", repeatable=True)
def decrease_attack():
    update_param("attack", -1 * o["step_attack"])


@mpv.add_binding(o["key_increase_release"],
    name="acompressor-increase-release", repeatable=True)
def increase_release():
    update_param("release", o["step_release"])


@mpv.add_binding(o["key_decrease_release"],
    name="acompressor-decrease-release", repeatable=True)
def decrease_release():
    update_param("release", -1 * o["step_release"])


if o["default_enable"]:
    af, i = get_filter()
    af[-1]["enabled"] = True
    mpv.set_property_node("af", af)
