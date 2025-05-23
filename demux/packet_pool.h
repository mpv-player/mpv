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

struct demux_packet_pool;
struct demux_packet;
struct mpv_global;

/**
 * Initializes the demux packet pool.
 *
 * This function creates a new shaderd demux packet pool. Should be done only
 * once per mpv context.
 *
 * @param global Pointer to the global context.
 */
void demux_packet_pool_init(struct mpv_global *global);

/**
 * Returns the demux packet pool context for client use.
 *
 * @param global Pointer to the global context.
 * @return Pointer to the demux packet context.
 */
struct demux_packet_pool *demux_packet_pool_get(struct mpv_global *global);

/**
 * Clears the demux packet pool.
 *
 * This function frees all the packets in the pool.
 * This function is thread-safe.
 *
 * @param pool Pointer to the demux packet pool.
 */
void demux_packet_pool_clear(struct demux_packet_pool *pool);

/**
 * Pushes a packet into the demux packet pool.
 *
 * This function pushes a new demux packet to the pool by appending
 * it to the list. If the packet is NULL, the function returns immediately.
 * This function is thread-safe.
 *
 * @param pool Pointer to the demux packet pool.
 * @param dp Pointer to the demux packet to be added.
 */
void demux_packet_pool_push(struct demux_packet_pool *pool,
                            struct demux_packet *dp);

/**
 * Prepends a linked list of demux packets to the pool.
 *
 * This function prepends a list of demux packets to the packet pool.
 * The head is the first packet to be inserted, and the tail is the
 * last one. This function is thread-safe.
 *
 * @param pool Pointer to the demux packet pool.
 * @param head Pointer to the head of the list of packets to be added.
 * @param tail Pointer to the tail of the list of packets to be added.
 */
void demux_packet_pool_prepend(struct demux_packet_pool *pool,
                               struct demux_packet *head, struct demux_packet *tail);

/**
 * Pops a packet from the demux packet pool.
 *
 * This function removes and returns the first packet from the pool's
 * linked list. This function is thread-safe.
 *
 * @param pool Pointer to the demux packet pool.
 * @return Pointer to the demux packet, or NULL if the pool is empty.
 */
struct demux_packet *demux_packet_pool_pop(struct demux_packet_pool *pool);
