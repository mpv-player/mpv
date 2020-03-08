#ifndef MP_AV_LOG_H
#define MP_AV_LOG_H

#include <stdbool.h>

struct mpv_global;
struct mp_log;
void init_libav(struct mpv_global *global);
void uninit_libav(struct mpv_global *global);
void check_library_versions(struct mp_log *log, int v);
#endif
