#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "global.h"
#include "misc/linked_list.h"
#include "misc/node.h"
#include "msg.h"
#include "options/m_option.h"
#include "osdep/threads.h"
#include "osdep/timer.h"
#include "stats.h"

struct stats_base {
    struct mpv_global *global;

    atomic_bool active;

    mp_mutex lock;

    struct {
        struct stats_ctx *head, *tail;
    } list;

    struct stat_entry **entries;
    int num_entries;

    int64_t last_time;
};

struct stats_ctx {
    struct stats_base *base;
    const char *prefix;

    struct {
        struct stats_ctx *prev, *next;
    } list;

    struct stat_entry **entries;
    int num_entries;
};

enum val_type {
    VAL_UNSET = 0,
    VAL_STATIC,
    VAL_STATIC_SIZE,
    VAL_INC,
    VAL_TIME,
    VAL_THREAD_CPU_TIME,
};

struct stat_entry {
    char name[32];
    const char *full_name; // including stats_ctx.prefix

    enum val_type type;
    double val_d;
    int64_t val_rt;
    int64_t val_th;
    int64_t time_start_ns;
    int64_t cpu_start_ns;
    mp_thread_id thread_id;
};

#define IS_ACTIVE(ctx) \
    (atomic_load_explicit(&(ctx)->base->active, memory_order_relaxed))

static void stats_destroy(void *p)
{
    struct stats_base *stats = p;

    // All entries must have been destroyed before this.
    assert(!stats->list.head);

    mp_mutex_destroy(&stats->lock);
}

void stats_global_init(struct mpv_global *global)
{
    assert(!global->stats);
    struct stats_base *stats = talloc_zero(global, struct stats_base);
    ta_set_destructor(stats, stats_destroy);
    mp_mutex_init(&stats->lock);

    global->stats = stats;
    stats->global = global;
}

static void add_stat(struct mpv_node *list, struct stat_entry *e,
                     const char *suffix, double num_val, char *text)
{
    struct mpv_node *ne = node_array_add(list, MPV_FORMAT_NODE_MAP);

    node_map_add_string(ne, "name", suffix ?
        mp_tprintf(80, "%s/%s", e->full_name, suffix) : e->full_name);
    node_map_add_double(ne, "value", num_val);
    if (text)
        node_map_add_string(ne, "text", text);
}

static int cmp_entry(const void *p1, const void *p2)
{
    struct stat_entry **e1 = (void *)p1;
    struct stat_entry **e2 = (void *)p2;
    return strcmp((*e1)->full_name, (*e2)->full_name);
}

void stats_global_query(struct mpv_global *global, struct mpv_node *out)
{
    struct stats_base *stats = global->stats;
    assert(stats);

    mp_mutex_lock(&stats->lock);

    atomic_store(&stats->active, true);

    if (!stats->num_entries) {
        for (struct stats_ctx *ctx = stats->list.head; ctx; ctx = ctx->list.next)
        {
            for (int n = 0; n < ctx->num_entries; n++) {
                MP_TARRAY_APPEND(stats, stats->entries, stats->num_entries,
                                 ctx->entries[n]);
            }
        }
        if (stats->num_entries) {
            qsort(stats->entries, stats->num_entries, sizeof(stats->entries[0]),
                  cmp_entry);
        }
    }

    node_init(out, MPV_FORMAT_NODE_ARRAY, NULL);

    int64_t now = mp_time_ns();
    if (stats->last_time) {
        double t_ms = MP_TIME_NS_TO_MS(now - stats->last_time);
        struct mpv_node *ne = node_array_add(out, MPV_FORMAT_NODE_MAP);
        node_map_add_string(ne, "name", "poll-time");
        node_map_add_double(ne, "value", t_ms);
        node_map_add_string(ne, "text", mp_tprintf(80, "%.2f ms", t_ms));

        // Very dirty way to reset everything if the stats.lua page was probably
        // closed. Not enough energy left for clean solution. Fuck it.
        if (t_ms > 2000) {
            for (int n = 0; n < stats->num_entries; n++) {
                struct stat_entry *e = stats->entries[n];

                e->cpu_start_ns = 0;
                e->val_rt = e->val_th = 0;
                if (e->type != VAL_THREAD_CPU_TIME)
                    e->type = 0;
            }
        }
    }
    stats->last_time = now;

    for (int n = 0; n < stats->num_entries; n++) {
        struct stat_entry *e = stats->entries[n];

        switch (e->type) {
        case VAL_STATIC:
            add_stat(out, e, NULL, e->val_d, NULL);
            break;
        case VAL_STATIC_SIZE: {
            char *s = format_file_size(e->val_d);
            add_stat(out, e, NULL, e->val_d, s);
            talloc_free(s);
            break;
        }
        case VAL_INC:
            add_stat(out, e, NULL, e->val_d, NULL);
            e->val_d = 0;
            break;
        case VAL_TIME: {
            double t_cpu = MP_TIME_NS_TO_MS(e->val_th);
            add_stat(out, e, "cpu", t_cpu, mp_tprintf(80, "%.2f ms", t_cpu));
            double t_rt = MP_TIME_NS_TO_MS(e->val_rt);
            add_stat(out, e, "time", t_rt, mp_tprintf(80, "%.2f ms", t_rt));
            e->val_rt = e->val_th = 0;
            break;
        }
        case VAL_THREAD_CPU_TIME: {
            int64_t t = mp_thread_cpu_time_ns(e->thread_id);
            if (!e->cpu_start_ns)
                e->cpu_start_ns = t;
            double t_msec = MP_TIME_NS_TO_MS(t - e->cpu_start_ns);
            add_stat(out, e, NULL, t_msec, mp_tprintf(80, "%.2f ms", t_msec));
            e->cpu_start_ns = t;
            break;
        }
        default: ;
        }
    }

    mp_mutex_unlock(&stats->lock);
}

static void stats_ctx_destroy(void *p)
{
    struct stats_ctx *ctx = p;

    mp_mutex_lock(&ctx->base->lock);
    LL_REMOVE(list, &ctx->base->list, ctx);
    ctx->base->num_entries = 0; // invalidate
    mp_mutex_unlock(&ctx->base->lock);
}

struct stats_ctx *stats_ctx_create(void *ta_parent, struct mpv_global *global,
                                   const char *prefix)
{
    struct stats_base *base = global->stats;
    assert(base);

    struct stats_ctx *ctx = talloc_zero(ta_parent, struct stats_ctx);
    ctx->base = base;
    ctx->prefix = talloc_strdup(ctx, prefix);
    ta_set_destructor(ctx, stats_ctx_destroy);

    mp_mutex_lock(&base->lock);
    LL_APPEND(list, &base->list, ctx);
    base->num_entries = 0; // invalidate
    mp_mutex_unlock(&base->lock);

    return ctx;
}

static struct stat_entry *find_entry(struct stats_ctx *ctx, const char *name)
{
    for (int n = 0; n < ctx->num_entries; n++) {
        if (strcmp(ctx->entries[n]->name, name) == 0)
            return ctx->entries[n];
    }

    struct stat_entry *e = talloc_zero(ctx, struct stat_entry);
    snprintf(e->name, sizeof(e->name), "%s", name);
    assert(strcmp(e->name, name) == 0); // make e->name larger and don't complain

    e->full_name = talloc_asprintf(e, "%s/%s", ctx->prefix, e->name);

    MP_TARRAY_APPEND(ctx, ctx->entries, ctx->num_entries, e);
    ctx->base->num_entries = 0; // invalidate

    return e;
}

static void static_value(struct stats_ctx *ctx, const char *name, double val,
                         enum val_type type)
{
    if (!IS_ACTIVE(ctx))
        return;
    mp_mutex_lock(&ctx->base->lock);
    struct stat_entry *e = find_entry(ctx, name);
    e->val_d = val;
    e->type = type;
    mp_mutex_unlock(&ctx->base->lock);
}

void stats_value(struct stats_ctx *ctx, const char *name, double val)
{
    static_value(ctx, name, val, VAL_STATIC);
}

void stats_size_value(struct stats_ctx *ctx, const char *name, double val)
{
    static_value(ctx, name, val, VAL_STATIC_SIZE);
}

void stats_time_start(struct stats_ctx *ctx, const char *name)
{
    MP_STATS(ctx->base->global, "start %s", name);
    if (!IS_ACTIVE(ctx))
        return;
    mp_mutex_lock(&ctx->base->lock);
    struct stat_entry *e = find_entry(ctx, name);
    e->cpu_start_ns = mp_thread_cpu_time_ns(mp_thread_current_id());
    e->time_start_ns = mp_time_ns();
    mp_mutex_unlock(&ctx->base->lock);
}

void stats_time_end(struct stats_ctx *ctx, const char *name)
{
    MP_STATS(ctx->base->global, "end %s", name);
    if (!IS_ACTIVE(ctx))
        return;
    mp_mutex_lock(&ctx->base->lock);
    struct stat_entry *e = find_entry(ctx, name);
    if (e->time_start_ns) {
        e->type = VAL_TIME;
        e->val_rt += mp_time_ns() - e->time_start_ns;
        e->val_th += mp_thread_cpu_time_ns(mp_thread_current_id()) - e->cpu_start_ns;
        e->time_start_ns = 0;
    }
    mp_mutex_unlock(&ctx->base->lock);
}

void stats_event(struct stats_ctx *ctx, const char *name)
{
    if (!IS_ACTIVE(ctx))
        return;
    mp_mutex_lock(&ctx->base->lock);
    struct stat_entry *e = find_entry(ctx, name);
    e->val_d += 1;
    e->type = VAL_INC;
    mp_mutex_unlock(&ctx->base->lock);
}

static void register_thread(struct stats_ctx *ctx, const char *name,
                            enum val_type type)
{
    mp_mutex_lock(&ctx->base->lock);
    struct stat_entry *e = find_entry(ctx, name);
    e->type = type;
    e->thread_id = mp_thread_current_id();
    mp_mutex_unlock(&ctx->base->lock);
}

void stats_register_thread_cputime(struct stats_ctx *ctx, const char *name)
{
    register_thread(ctx, name, VAL_THREAD_CPU_TIME);
}

void stats_unregister_thread(struct stats_ctx *ctx, const char *name)
{
    register_thread(ctx, name, 0);
}
