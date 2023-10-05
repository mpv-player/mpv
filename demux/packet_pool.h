/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "osdep/threads.h"

struct demux_packet;

struct demux_packet_pool {
    mp_mutex lock;
    struct demux_packet *packets;
};

void demux_packet_pool_init(struct demux_packet_pool *pool);
void demux_packet_pool_uninit(void *p);
void demux_packet_pool_clear(struct demux_packet_pool *pool);
void demux_packet_pool_push(struct demux_packet_pool *pool,
                            struct demux_packet *dp);
void demux_packet_pool_prepend(struct demux_packet_pool *pool,
                               struct demux_packet *head, struct demux_packet *tail);
struct demux_packet *demux_packet_pool_pop(struct demux_packet_pool *pool);
