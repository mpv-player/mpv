#ifndef MP_MSG_CONTROL_H
#define MP_MSG_CONTROL_H

#include <stdbool.h>

struct mpv_global;
void mp_msg_init(struct mpv_global *global);
void mp_msg_uninit(struct mpv_global *global);
void mp_msg_update_msglevels(struct mpv_global *global);
void mp_msg_mute(struct mpv_global *global, bool mute);
void mp_msg_force_stderr(struct mpv_global *global, bool force_stderr);
void mp_msg_flush_status_line(struct mpv_global *global);
bool mp_msg_has_status_line(struct mpv_global *global);

struct mp_log_buffer_entry {
    char *prefix;
    int level;
    char *text;
};

struct mp_log_buffer;
struct mp_log_buffer *mp_msg_log_buffer_new(struct mpv_global *global,
                                            int size, int level);
void mp_msg_log_buffer_destroy(struct mp_log_buffer *buffer);
struct mp_log_buffer_entry *mp_msg_log_buffer_read(struct mp_log_buffer *buffer);

int mp_msg_open_stats_file(struct mpv_global *global, const char *path);

struct bstr;
int mp_msg_split_msglevel(struct bstr *s, struct bstr *out_mod, int *out_level);

extern char *mp_log_levels[MSGL_MAX + 1];

#endif
