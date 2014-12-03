#
# Copyright (c) 2012 Rudolf Polzer <divVerent@xonotic.org>
#
# This file is part of mpv's vf_dlopen examples.
#
# mpv's vf_dlopen examples are free software; you can redistribute them and/or
# modify them under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#
# mpv's vf_dlopen examples are distributed in the hope that they will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with mpv's vf_dlopen examples; if not, write to the Free
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301 USA
#

FILTERS = telecine tile rectangle framestep ildetect
COMMON = filterutils.o

OBJECTS = $(patsubst %,%.o,$(FILTERS)) $(COMMON)
HEADERS = $(wildcard *.h)
OUT = $(patsubst %,%.so,$(FILTERS))

CFLAGS ?= -Wall -Wextra -O3 -march=native -mtune=native -ffast-math

CPPFLAGS += -I../../video/filter
CFLAGS += -fPIC
LDFLAGS += -shared -fPIC

ifneq ($(LTO),)
CFLAGS += -flto
LDFLAGS += $(CFLAGS) -flto
endif

all: $(OUT)

clean:
	$(RM) $(OBJECTS) $(OUT)

%.so: %.o $(COMMON)
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $(COMMON) $<

# FIXME replace this by real dependency tracking
%.o: %.c $(HEADERS)

