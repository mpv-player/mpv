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

#include "packet_pool.h"

#include "common/global.h"
#include "osdep/threads.h"
#include "player/core.h"
#include "packet.h"

struct demux_packet_pool {
    mp_mutex lock;
    struct demux_packet *packets;
    struct MPContext *parent;
};

void demux_packet_pool_init(struct mpv_global *global, struct MPContext *parent)
{
    struct demux_packet_pool *pool = talloc(global, struct demux_packet_pool);
    talloc_set_destructor(pool, demux_packet_pool_uninit);
    mp_mutex_init(&pool->lock);
    pool->packets = NULL;
    pool->parent = parent;

    assert(!global->packet_pool);
    global->packet_pool = pool;
}

void demux_packet_pool_uninit(void *p)
{
    struct demux_packet_pool *pool = p;
    if (!pool->parent->quit_fast)
        demux_packet_pool_clear(pool);
    mp_mutex_destroy(&pool->lock);
}

struct demux_packet_pool *demux_packet_pool_get(struct mpv_global *global)
{
    // Currently all clients use the same packet pool. There is no additional
    // state for each client, may be extended in the future.
    return global->packet_pool;
}

void demux_packet_pool_clear(struct demux_packet_pool *pool)
{
    mp_mutex_lock(&pool->lock);
    struct demux_packet *dp = pool->packets;
    while (dp) {
        struct demux_packet *next = dp->next;
        free_demux_packet(dp);
        dp = next;
    }
    pool->packets = NULL;
    mp_mutex_unlock(&pool->lock);
}

void demux_packet_pool_push(struct demux_packet_pool *pool,
                            struct demux_packet *dp)
{
    if (!dp)
        return;
    dp->next = NULL;
    demux_packet_pool_prepend(pool, dp, dp);
}

void demux_packet_pool_prepend(struct demux_packet_pool *pool,
                               struct demux_packet *head, struct demux_packet *tail)
{
    if (!head)
        return;
    assert(tail);
    assert(head != tail ? !!head->next : !head->next);

    mp_mutex_lock(&pool->lock);
    tail->next = pool->packets;
    pool->packets = head;
    mp_mutex_unlock(&pool->lock);
}

struct demux_packet *demux_packet_pool_pop(struct demux_packet_pool *pool)
{
    mp_mutex_lock(&pool->lock);
    struct demux_packet *dp = pool->packets;
    if (dp) {
        pool->packets = dp->next;
        dp->next = NULL;
    }
    mp_mutex_unlock(&pool->lock);
    return dp;
}
