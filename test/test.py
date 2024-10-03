#!/usr/bin/env python3

from contextlib import contextmanager
import datetime
import json
import socket
import subprocess

mpv_path = "./mpv"
anull = "av://lavfi:anullsrc"


class IpcError(Exception):
    pass


@contextmanager
def assert_error(s):
    try:
        yield
        assert False
    except IpcError as e:
        assert str(e) == s


class Mpv:
    @classmethod
    @contextmanager
    def create(cls, *args):
        self = cls()
        self.request_id = 0
        self.observe_property_id = 0
        self.args = [
            "--no-config",
            "--input-ipc-client=fd://0",
            "--idle",
            "--vo=null",
            "--ao=null",
            "--no-ytdl",
            "--load-stats-overlay=no",
            "--load-osd-console=no",
            "--load-auto-profiles=no",
            *args,
        ]

        our_end, mpv_end = socket.socketpair()

        self.stream = our_end.makefile("rwb")

        self.process = subprocess.Popen(
            args=[mpv_path, *self.args],
            stdin=mpv_end,
        )

        our_end.close()
        mpv_end.close()

        yield self

        self.stream.close()

        print("* Waiting for exit")
        status = self.process.wait()

        print("* Exit %d" % status)
        assert status == 0

    def command(self, *args):
        self.request_id += 1
        request_id = self.request_id

        data = {
            "request_id": request_id,
            "command": list(args),
        }
        s = json.dumps(data, separators=(",", ":"))
        print(">", s[:1000])
        line = bytes(s.encode("utf-8")) + b"\n"

        self.stream.write(line)
        self.stream.flush()

        msg = self.wait_message(lambda msg: msg.get("request_id") == request_id)

        if msg["error"] != "success":
            raise IpcError(msg["error"])

        return msg.get("data")

    def set_property(self, name, value):
        return self.command("set_property", name, value)

    def get_property(self, name):
        return self.command("get_property", name)

    def wait_message(self, pred):
        while True:
            msg = self.read_message()
            if pred(msg):
                return msg

    def read_message(self):
        s = self.stream.readline()
        print("<", s.decode("utf-8").strip()[:1000])
        return json.loads(s)

    def wait_event(self, name):
        print("* Waiting for %r event" % name)
        return self.wait_message(lambda msg: msg.get("event") == name)

    def wait_idle(self):
        return self.wait_event("idle")

    def wait_file_loaded(self):
        return self.wait_event("file-loaded")

    def wait_start_file(self):
        return self.wait_event("start-file")

    @contextmanager
    def wait_property_change(self, name):
        self.observe_property_id += 1
        id = self.observe_property_id

        def pred(msg):
            return msg.get("event") == "property-change" and msg["id"] == id

        try:
            self.command("observe_property", id, name)
            # Skip the first event.
            self.wait_message(pred)

            msg = {}
            yield msg

            print("* Waiting for %r property to change" % name)
            msg.update(self.wait_message(pred))
        finally:
            self.command("unobserve_property", id)

    def __repr__(self):
        return f"Mpv(args={self.args!r})"


mpv = Mpv.create


def test_new_entry():
    with mpv() as m:
        m.set_property("playlist", [anull, {"filename": anull}])
        assert m.get_property("playlist") == [
            {"id": 1, "filename": anull},
            {"id": 2, "filename": anull},
        ]


def test_nonexisting_and_duplicated_entries():
    with mpv() as m:
        m.set_property("playlist", [-1, 0, 1, {}, {"id": 0}, {"id": -1}])
        assert m.get_property("playlist") == []

        m.set_property("playlist", [anull])

        m.set_property("playlist", [{"id": 1}, {"id": 1}])
        assert m.get_property("playlist") == [{"id": 1, "filename": anull}]

        m.set_property("playlist", [0, 0])
        assert m.get_property("playlist") == [{"id": 1, "filename": anull}]


def test_move_entries():
    with mpv() as m:
        m.wait_idle()
        m.set_property("playlist", ["a", "b", "c"])
        m.set_property("playlist", ["d", {"id": 2}, {"id": 1}])
        assert m.get_property("playlist") == [
            {"id": 4, "filename": "d"},
            {"id": 2, "filename": "b"},
            {"id": 1, "filename": "a"},
        ]

    with mpv() as m:
        m.wait_idle()
        m.set_property("playlist", ["a", "b", "c"])
        m.set_property("playlist", ["d", 1, 0])
        assert m.get_property("playlist") == [
            {"id": 4, "filename": "d"},
            {"id": 2, "filename": "b"},
            {"id": 1, "filename": "a"},
        ]


def test_move_playing_entry():
    with mpv(anull) as m:
        m.wait_file_loaded()
        with m.wait_property_change("time-pos") as a:
            pass

        m.set_property("playlist", ["x", 0, "x"])
        m.set_property("playlist", [0, 1])
        with m.wait_property_change("time-pos") as b:
            pass

        m.set_property("playlist", [1])
        with m.wait_property_change("time-pos") as c:
            pass

        # Playback went uninterrupted.
        assert a["data"] < b["data"] < c["data"]


def test_remove_playing_entry():
    with mpv(*((anull,) * 5), "--keep-open", "--playlist-start=1") as m:
        assert m.wait_start_file()["playlist_entry_id"] == 2
        assert m.get_property("playlist-playing-pos") == 1

        # Remove one.
        m.set_property("playlist", [0, 2, 3, 4])
        assert m.wait_start_file()["playlist_entry_id"] == 3
        assert m.get_property("playlist-playing-pos") == 1

        # Remove two.
        m.set_property("playlist", [0, 3])
        assert m.wait_start_file()["playlist_entry_id"] == 5
        assert m.get_property("playlist-playing-pos") == 1

        # Nothing left on its right.
        m.set_property("playlist", [0])
        m.wait_idle()
        assert len(m.get_property("playlist")) == 1
        assert m.get_property("playlist-playing-pos") == -1

        # Can restarting playback with "loadfile".
        m.command("loadfile", anull)
        m.wait_start_file()


def test_entry_filename():
    with mpv() as m:
        m.set_property("playlist", ["original"])
        m.set_property("playlist", [{"id": 1, "filename": "ignored"}])
        # Cannot be changed.
        assert m.get_property("playlist") == [{"id": 1, "filename": "original"}]


def test_entry_title():
    with mpv() as m:

        def title(**entry):
            m.set_property("playlist", [entry])
            return m.get_property("playlist")[0].get("title")

        assert title(filename=anull, title="original") == "original"
        assert title(id=1) == "original"
        assert title(id=1, title="new") == "new"
        assert title(id=1, title=None) == None
        assert title(id=1, title="newer") == "newer"
        assert title(id=1, title="") == None


def test_entry_current():
    # On new entry.
    with mpv() as m:
        m.wait_idle()
        m.set_property("playlist", [{"filename": anull, "current": True}])
        m.wait_file_loaded()

    # Set on existing entry.
    with mpv() as m:
        m.set_property("playlist", [{"filename": anull}, {"filename": anull}])
        m.wait_idle()
        m.set_property("playlist", [0, {"id": 2, "current": True}])
        assert m.wait_start_file()["playlist_entry_id"] == 2


def test_playlist_change_event():
    with mpv() as m:
        m.set_property("playlist", [anull, anull])
        # Wait for `current=True` to be set.
        with m.wait_property_change("playlist"):
            pass
        assert m.get_property("playlist")[0]["current"]

        with m.wait_property_change("playlist"):
            m.set_property("playlist", [0])


def test_stress():
    N = 10000
    with mpv() as m:
        m.wait_idle()

        m.set_property("playlist", list(map(str, range(0, N))))
        pl = m.get_property("playlist")
        assert len(pl) == N

        m.set_property("playlist", list(reversed(range(0, N))))
        assert m.get_property("playlist") == list(reversed(pl))

        m.set_property("playlist", list({"id": i + 1} for i in range(0, N)))
        assert m.get_property("playlist") == pl


def test_invalid_format():
    with mpv(anull) as m:
        original = m.get_property("playlist")

        with assert_error("unsupported format for accessing property"):
            m.set_property("playlist", 0)
        assert m.get_property("playlist") == original

        m.set_property("playlist", [{"filename": 0, "no such key": "x"}, False, 0])
        assert m.get_property("playlist") == original


def main():
    import inspect
    import sys
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("pattern", nargs="?", default="")

    args = parser.parse_args()

    test_fns = [
        obj
        for name, obj in inspect.getmembers(sys.modules[__name__])
        if inspect.isfunction(obj) and name.startswith("test_") and args.pattern in name
    ]

    suite_start = datetime.datetime.now()

    for f in test_fns:
        n = max(80, len(f.__name__))
        print("=" * n)
        print(f.__name__)
        print("=" * n)

        start = datetime.datetime.now()
        f()
        elapsed = datetime.datetime.now() - start
        print("* OK. %.2fs" % elapsed.total_seconds())

    suite_elapsed = datetime.datetime.now() - suite_start
    print("ALL OK. %.2fs" % suite_elapsed.total_seconds())


main()
