#!/usr/bin/env python3
import matplotlib.pyplot as plot
import sys

filename = sys.argv[1]

"""
This script is meant to display stats written by mpv --dump-stats=filename.
In general, each line in that file is an event of the form:

    <timestamp in microseconds> <text> '#' <comment>

e.g.:

    10474959 start flip #cplayer

<text> is what MP_STATS(log, "...") writes. The rest is added by msg.c.

Currently, the following event types are supported:

    'start' <name>              start of the named event
    'end' <name>                end of the named event
    'value' <float> <name>      a normal value (as opposed to event)
    'event-timed' <ts> <name>   singular event at the given timestamp
    <name>                      singular event

"""

class G:
    events = {}
    sevents = []  # events, deterministically sorted
    start = None
    # http://matplotlib.org/api/markers_api.html#module-matplotlib.markers
    markers = ["o", "8", "s", "p", "*", "h", "+", "x", "D"]

def find_marker():
    if len(G.markers) == 0:
        return "o"
    m = G.markers[0]
    G.markers = G.markers[1:]
    return m

class Event:
    pass

def get_event(event, evtype):
    if event not in G.events:
        e = Event()
        G.events[event] = e
        e.name = event
        e.vals = []
        e.type = evtype
        e.marker = "o"
        e.numid = len(G.events)
        G.sevents = list(G.events.values())
        G.sevents.sort(key=lambda x: x.name)
        if e.type == "event-signal":
            e.marker = find_marker()
    return G.events[event]

for line in [line.split("#")[0].strip() for line in open(filename, "r")]:
    line = line.strip()
    if not line:
        continue
    ts, event = line.split(" ", 1)
    ts = int(ts) / 1000 # milliseconds
    if G.start is None:
        G.start = ts
    ts = ts - G.start
    if event.startswith("start "):
        e = get_event(event[6:], "event")
        e.vals.append((ts, 0))
        e.vals.append((ts, 1))
    elif event.startswith("end "):
        e = get_event(event[4:], "event")
        e.vals.append((ts, 1))
        e.vals.append((ts, 0))
    elif event.startswith("value "):
        _, val, name = event.split(" ", 2)
        val = float(val)
        e = get_event(name, "value")
        e.vals.append((ts, val))
    elif event.startswith("event-timed "):
        _, val, name = event.split(" ", 2)
        val = int(val) / 1000 - G.start
        e = get_event(name, "event-signal")
        e.vals.append((val, 1))
    else:
        e = get_event(event, "event-signal")
        e.vals.append((ts, 1))

for e in G.sevents:
    e.vals = [(x, y * e.numid / len(G.events)) for (x, y) in e.vals]

plot.hold(True)
mainpl = plot.subplot(2, 1, 1)
legend = []
for e in G.sevents:
    if e.type == "value":
        plot.subplot(2, 1, 2, sharex=mainpl)
    else:
        plot.subplot(2, 1, 1)
    pl, = plot.plot([x for x,y in e.vals], [y for x,y in e.vals], label=e.name)
    if e.type == "event-signal":
        plot.setp(pl, marker = e.marker, linestyle = "None")
    legend.append(pl)
plot.subplot(2, 1, 1)
plot.legend(legend, [pl.get_label() for pl in legend])
plot.show()
