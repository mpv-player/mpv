#!/bin/sh
build/mpv --script=player/mruby/test.mrb --idle --msg-level=all=error $1
