#pragma once

struct mpv_global;
struct mpv_node;
struct stats_ctx;

void stats_global_init(struct mpv_global *global);
void stats_global_query(struct mpv_global *global, struct mpv_node *out);

// stats_ctx can be free'd with ta_free(), or by using the ta_parent.
struct stats_ctx *stats_ctx_create(void *ta_parent, struct mpv_global *global,
                                   const char *prefix);

// A static numeric value.
void stats_value(struct stats_ctx *ctx, const char *name, double val);

// Like stats_value(), but render as size in bytes.
void stats_size_value(struct stats_ctx *ctx, const char *name, double val);

// Report the real time and CPU time in seconds between _start and _end calls
// as value, and report the average and number of all times.
void stats_time_start(struct stats_ctx *ctx, const char *name);
void stats_time_end(struct stats_ctx *ctx, const char *name);

// Display number of events per poll period.
void stats_event(struct stats_ctx *ctx, const char *name);

// Report the thread's CPU time. This needs to be called only once per thread.
// The current thread is assumed to stay valid until the stats_ctx is destroyed
// or stats_unregister_thread() is called, otherwise UB will occur.
void stats_register_thread_cputime(struct stats_ctx *ctx, const char *name);

// Remove reference to pthread_self().
void stats_unregister_thread(struct stats_ctx *ctx, const char *name);
