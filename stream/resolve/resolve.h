/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MP_RESOLVE_H
#define MP_RESOLVE_H

struct mpv_global;

struct mp_resolve_result {
    char *url;
    char *title;

    struct mp_resolve_src **srcs;
    int num_srcs;

    double start_time;

    struct mp_resolve_sub **subs;
    int num_subs;

    struct playlist *playlist;
};

struct mp_resolve_src {
    char *url;       // can be NULL; otherwise it's the exact video URL
    char *encid;     // indicates quality level, contents are libquvi specific
};

struct mp_resolve_sub {
    char *url;
    char *data;
    char *lang;
};

struct mp_resolve_result *mp_resolve_quvi(const char *url,
                                          struct mpv_global *global);

#endif
