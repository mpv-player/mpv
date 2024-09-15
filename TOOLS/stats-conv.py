#!/usr/bin/env python3
import re
import sys

import pyqtgraph as pg
from PyQt6 import QtWidgets

filename = sys.argv[1]

events = ".*"
if len(sys.argv) > 2:
    events = sys.argv[2]
event_regex = re.compile(events)

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
    'value-timed' <ts> <float> <name>       a value for an event at the given timestamp
    'range-timed' <ts1> <ts2> <name>        like start/end, but explicit times
    <name>                      singular event (same as 'signal')

"""

class G:
    events = {}
    start = 0.0
    markers = ["o", "s", "t", "d"]
    curveno = {}
    sevents = []

def find_marker():
    if len(G.markers) == 0:
        return "o"
    m = G.markers[0]
    G.markers = G.markers[1:]
    return m

class Event:
    name = None
    vals = []
    type = None
    marker = ""

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

colors = [
    (0.0, 0.5, 0.0),
    (0.0, 0.0, 1.0),
    (0.0, 0.0, 0.0),
    (1.0, 0.0, 0.0),
    (0.75, 0.75, 0),
    (0.0, 0.75, 0.75),
    (0.75, 0, 0.75),
]

def mk_color(t):
    return pg.mkColor(int(t[0] * 255), int(t[1] * 255), int(t[2] * 255))

SCALE = 1e6 # microseconds to seconds

with open(filename) as file:
    for line in file:
        line = line.split("#")[0].strip()
        if not line:
            continue
        ts, event = line.split(" ", 1)
        ts = int(ts) / SCALE
        if G.start is None:
            G.start = ts
        ts -= G.start

        match event.split(" ", 1):
            case ["start", name]:
                e = get_event(name, "event")
                e.vals.append((ts, 0))
                e.vals.append((ts, 1))
            case ["end", name]:
                e = get_event(name, "event")
                e.vals.append((ts, 1))
                e.vals.append((ts, 0))
            case ["value", rest]:
                val, name = rest.split(" ", 1)
                val = float(val)
                e = get_event(name, "value")
                e.vals.append((ts, val))
            case ["event-timed", rest]:
                val, name = rest.split(" ", 1)
                val = int(val) / SCALE - G.start
                e = get_event(name, "event-signal")
                e.vals.append((val, 1))
            case ["range-timed", rest]:
                ts1, ts2, name = rest.split(" ", 2)
                ts1 = int(ts1) / SCALE - G.start
                ts2 = int(ts2) / SCALE - G.start
                e = get_event(name, "event")
                e.vals.append((ts1, 0))
                e.vals.append((ts1, 1))
                e.vals.append((ts2, 1))
                e.vals.append((ts2, 0))
            case ["value-timed", rest]:
                tsval, val, name = rest.split(" ", 2)
                tsval = int(tsval) / SCALE - G.start
                val = float(val)
                e = get_event(name, "value")
                e.vals.append((tsval, val))
            case ["signal", name]:
                e = get_event(name, "event-signal")
                e.vals.append((ts, 1))
            case _:
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

pg.setConfigOption("background", "w")
pg.setConfigOption("foreground", "k")
app = QtWidgets.QApplication([])
win = pg.GraphicsLayoutWidget()
win.show()

ax = [None, None]
plots = 2 if hasval else 1
ax[0] = win.addPlot()

if hasval:
    win.nextRow()
    ax[1] = win.addPlot()
    ax[1].setXLink(ax[0])

for cur in ax:
    if cur is not None:
        cur.addLegend(offset = (-1, 1))

for e in G.sevents:
    cur = ax[1 if e.type == "value" else 0]
    if cur not in G.curveno:
        G.curveno[cur] = 0
    args = {"name": e.name,"antialias":True}
    color = mk_color(colors[G.curveno[cur] % len(colors)])
    if e.type == "event-signal":
        args["symbol"] = e.marker
        args["symbolBrush"] = pg.mkBrush(color, width=0)
    else:
        args["pen"] = pg.mkPen(color, width=0)
    G.curveno[cur] += 1
    cur.plot(*zip(*e.vals), **args)

app.exec()
