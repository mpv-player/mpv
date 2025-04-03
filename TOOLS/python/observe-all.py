# Test script for property change notification mechanism.
# Note that watching/reading some properties can be very expensive, or
# require the player to synchronously wait on network (when playing
# remote files), so you should in general only watch properties you
# are interested in.

from mpvclient import mpv


def observe(name):
    @mpv.observe_property(name, mpv.MPV_FORMAT_NODE)
    def func(val):
        mpv.info("property '" + name + "' changed to '" +
              str(val) + "'")


for name in mpv.get_property_node("property-list"):
    if name not in ["osd-sym-cc", "osd-ass-cc", "term-clip-cc", "screenshot-template"]:
        observe(name)


for name in mpv.get_property_node("options"):
    if name not in ["screenshot-template"]:
        observe("options/" + name)
