#ifndef MP_MSG_CONTROL_H
#define MP_MSG_CONTROL_H

#include <stdbool.h>

struct mpv_global;
void mp_msg_init(struct mpv_global *global);
void mp_msg_uninit(struct mpv_global *global);
void mp_msg_update_msglevels(struct mpv_global *global);
void mp_msg_force_stderr(struct mpv_global *global, bool force_stderr);
bool mp_msg_has_status_line(struct mpv_global *global);

void mp_msg_flush_status_line(struct mp_log *log);

struct mp_log_buffer_entry {
    char *prefix;
    int level;
    char *text;
};

struct mp_log_buffer;
struct mp_log_buffer *mp_msg_log_buffer_new(struct mpv_global *global,
                                            int size, int level,
                                            void (*wakeup_cb)(void *ctx),
                                            void *wakeup_cb_ctx);
void mp_msg_log_buffer_destroy(struct mp_log_buffer *buffer);
struct mp_log_buffer_entry *mp_msg_log_buffer_read(struct mp_log_buffer *buffer);

int mp_msg_open_stats_file(struct mpv_global *global, const char *path);
int mp_msg_find_level(const char *s);

extern const char *const mp_log_levels[MSGL_MAX + 1];
extern const int mp_mpv_log_levels[MSGL_MAX + 1];

#endif
