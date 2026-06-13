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
    // On single packet push, clear instantly. Unlike prepend, where multiple
    // packets may be prepended at once, there is no performance issue with
    // clearing a single packet here.
    demux_packet_unref(dp);
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

// Number of most-recently-pushed packets kept at the head of the freelist for
// fast reuse. Pop walks at most this many cold-cache nodes per call, so we keep
// it small.
#define POOL_HOT_RESERVE 16

// Maximum number of packets freed per pop, starting after the hot reserve.
// Acts as a self-balancing GC. After a large push, for example on cache reset,
// the pool will shrink across multiple subsequent pops. This value has to be
// reasonably higher than POOL_HOT_RESERVE to make the clearing effective, but
// not too high to avoid const of big GC baches on each pop.
#define POOL_GC_BATCH 64

// Detach up to POOL_GC_BATCH packets from the pool, starting after the
// first POOL_HOT_RESERVE entries. Must be called locked.
static inline struct demux_packet *pool_gc_detach(struct demux_packet_pool *pool)
{
    struct demux_packet *cat_reserve = pool->packets;
    for (int i = 1; cat_reserve && i < POOL_HOT_RESERVE; i++)
        cat_reserve = cat_reserve->next;

    if (!cat_reserve || !cat_reserve->next)
        return NULL;

    struct demux_packet *gc = cat_reserve->next;
    struct demux_packet *cut_rest = gc;
    for (int i = 1; cut_rest->next && i < POOL_GC_BATCH; i++)
        cut_rest = cut_rest->next;

    cat_reserve->next = cut_rest->next;
    cut_rest->next = NULL;
    return gc;
}

struct demux_packet *demux_packet_pool_pop(struct demux_packet_pool *pool)
{
    mp_mutex_lock(&pool->lock);
    struct demux_packet *dp = pool->packets;
    if (dp) {
        pool->packets = dp->next;
        dp->next = NULL;
    }
    struct demux_packet *gc = pool_gc_detach(pool);
    mp_mutex_unlock(&pool->lock);

    // Clear the popped packet's external references and drain a small batch
    // of old pooled packets. Both costs are amortized across packet_create()
    // calls so EOF / cache reset never has to free a huge list at once.
    demux_packet_unref(dp);
    // GC is done unlocked, this makes packets be iterated again, but that's
    // should be fine, with POOL_GC_BATCH elements only.
    free_demux_packets(gc);

    return dp;
}
