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

#include <libavcodec/packet.h>

#include "config.h"

#include "common/global.h"
#include "osdep/threads.h"
#include "packet.h"

struct demux_packet_pool {
    mp_mutex lock;
    struct demux_packet *packets;
};

static void uninit(void *p)
{
    struct demux_packet_pool *pool = p;
    demux_packet_pool_clear(pool);
    mp_mutex_destroy(&pool->lock);
}

static void free_demux_packets(struct demux_packet *dp)
{
    while (dp) {
        struct demux_packet *next = dp->next;
        free_demux_packet(dp);
        dp = next;
    }
}

void demux_packet_pool_init(struct mpv_global *global)
{
    struct demux_packet_pool *pool = talloc(global, struct demux_packet_pool);
    talloc_set_destructor(pool, uninit);
    mp_mutex_init(&pool->lock);
    pool->packets = NULL;

    mp_assert(!global->packet_pool);
    global->packet_pool = pool;
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
    pool->packets = NULL;
    mp_mutex_unlock(&pool->lock);
    free_demux_packets(dp);
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
    mp_assert(tail);
    mp_assert(head != tail ? !!head->next : !head->next);

    mp_mutex_lock(&pool->lock);
    tail->next = pool->packets;
    pool->packets = head;
#if HAVE_DISABLE_PACKET_POOL
    struct demux_packet *dp = pool->packets;
    pool->packets = NULL;
#endif
    mp_mutex_unlock(&pool->lock);

#if HAVE_DISABLE_PACKET_POOL
    free_demux_packets(dp);
#endif
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

    // Clear the packet from possible external references. This is done in the
    // pop function instead of prepend to distribute the load of clearing packets.
    // packet_create() is called at a reasonable rate, so it's fine to clear
    // a single packet at a time. This avoids the need to clear potentially
    // hundreds of thousands of packets at once when file playback is stopped,
    // which would require a significant amount of time to iterate over all packets.
    if (dp) {
        if (dp->avpacket)
            av_packet_unref(dp->avpacket);
        ta_free_children(dp);
    }

    return dp;
}
