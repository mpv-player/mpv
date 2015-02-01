#!/usr/bin/env python3
import matplotlib.pyplot as plot
import sys
import re

filename = sys.argv[1]

event_regex = re.compile(".*")

"""
This script is meant to display stats written by mpv --dump-stats=filename.
In general, each line in that file is an event of the form:

    <timestamp in microseconds> <text> '#' <comment>

e.g.:

    10474959 start flip #cplayer

<text> is what MP_STATS(log, "...") writes. The rest is added by msg.c.

Currently, the following event types are supported:

    'signal' <name>             singular event
    'start' <name>              start of the named event
    'end' <name>                end of the named event
    'value' <float> <name>      a normal value (as opposed to event)
    'event-timed' <ts> <name>   singular event at the given timestamp
    'value-timed' <ts> <float> <name>
                                a value for an event at the given timestamp
    <name>                      singular event (same as 'signal')

"""

class G:
    events = {}
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
        e.name = event
        e.vals = []
        e.type = evtype
        e.marker = "o"
        if e.type == "event-signal":
            e.marker = find_marker()
        if not event_regex.match(e.name):
            return e
        G.events[event] = e
    return G.events[event]

SCALE = 1e6 # microseconds to seconds

for line in [line.split("#")[0].strip() for line in open(filename, "r")]:
    line = line.strip()
    if not line:
        continue
    ts, event = line.split(" ", 1)
    ts = int(ts) / SCALE
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
        val = int(val) / SCALE - G.start
        e = get_event(name, "event-signal")
        e.vals.append((val, 1))
    elif event.startswith("value-timed "):
        _, tsval, val, name = event.split(" ", 3)
        tsval = int(tsval) / SCALE - G.start
        val = float(val)
        e = get_event(name, "value")
        e.vals.append((tsval, val))
    elif event.startswith("signal "):
        name = event.split(" ", 2)[1]
        e = get_event(name, "event-signal")
        e.vals.append((ts, 1))
    else:
        e = get_event(event, "event-signal")
        e.vals.append((ts, 1))

# deterministically sort them; make sure the legend is sorted too
G.sevents = list(G.events.values())
G.sevents.sort(key=lambda x: x.name)
hasval = False
for e, index in zip(G.sevents, range(len(G.sevents))):
    m = len(G.sevents)
    if e.type == "value":
        hasval = True
    else:
        e.vals = [(x, y * (m - index) / m) for (x, y) in e.vals]

fig = plot.figure()
fig.hold(True)
ax = [None, None]
plots = 2 if hasval else 1
ax[0] = fig.add_subplot(plots, 1, 1)
if hasval:
    ax[1] = fig.add_subplot(plots, 1, 2, sharex=ax[0])
legends = [[], []]
for e in G.sevents:
    cur = ax[1 if e.type == "value" else 0]
    pl, = cur.plot([x for x,y in e.vals], [y for x,y in e.vals], label=e.name)
    if e.type == "event-signal":
        plot.setp(pl, marker = e.marker, linestyle = "None")
for cur in ax:
    if cur is not None:
        cur.legend()
plot.show()
