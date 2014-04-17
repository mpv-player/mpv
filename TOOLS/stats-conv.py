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

    'start' <name>          start of the named event
    'end' <name>            end of the named event
    'value' <float> <name>  a normal value (as opposed to event)
    <event>                 singular event

"""

class G:
    events = {}
    sevents = []  # events, deterministically sorted
    start = None

class Event:
    pass

def get_event(event):
    if event not in G.events:
        e = Event()
        G.events[event] = e
        e.name = event
        e.vals = []
        e.type = "unknown"
        G.sevents = list(G.events.values())
        G.sevents.sort(key=lambda x: x.name)
    return G.events[event]

for line in [line.split("#")[0].strip() for line in open(filename, "r")]:
    ts, event = line.split(" ", 1)
    ts = int(ts) / 1000 # milliseconds
    if G.start is None:
        G.start = ts
    ts = ts - G.start
    if event.startswith("start "):
        e = get_event(event[6:])
        e.type = "event"
        e.vals.append((ts, 0))
        e.vals.append((ts, 1))
    elif event.startswith("end "):
        e = get_event(event[4:])
        e.type = "event"
        e.vals.append((ts, 1))
        e.vals.append((ts, 0))
    elif event.startswith("value "):
        _, val, name = event.split(" ", 2)
        val = float(val)
        e = get_event(name)
        e.type = "value"
        e.vals.append((ts, val))
    else:
        e = get_event(event)
        e.type = "event-signal"
        e.vals.append((ts, 1))

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
        plot.setp(pl, marker = "o", linestyle = "None")
    legend.append(pl)
plot.subplot(2, 1, 1)
plot.legend(legend, [pl.get_label() for pl in legend])
plot.show()
