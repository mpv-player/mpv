#ifndef MP_CLIENT_H_
#define MP_CLIENT_H_

#include <stdint.h>
#include <stdbool.h>

#include "libmpv/client.h"

struct MPContext;
struct mpv_handle;
struct mp_client_api;
struct mp_log;

// Includes space for \0
#define MAX_CLIENT_NAME 64

void mp_clients_init(struct MPContext *mpctx);
void mp_clients_destroy(struct MPContext *mpctx);
int mp_clients_num(struct MPContext *mpctx);
bool mp_clients_all_initialized(struct MPContext *mpctx);

bool mp_client_exists(struct MPContext *mpctx, const char *client_name);
void mp_client_broadcast_event(struct MPContext *mpctx, int event, void *data);
int mp_client_send_event(struct MPContext *mpctx, const char *client_name,
                         int event, void *data);
int mp_client_send_event_dup(struct MPContext *mpctx, const char *client_name,
                             int event, void *data);
bool mp_client_event_is_registered(struct MPContext *mpctx, int event);
void mp_client_property_change(struct MPContext *mpctx, const char *name);

struct mpv_handle *mp_new_client(struct mp_client_api *clients, const char *name);
struct mp_log *mp_client_get_log(struct mpv_handle *ctx);
struct MPContext *mp_client_get_core(struct mpv_handle *ctx);

void mp_resume_all(struct mpv_handle *ctx);

// m_option.c
void *node_get_alloc(struct mpv_node *node);

// vo_opengl_cb.c
struct mpv_opengl_cb_context;
struct mpv_global;
struct osd_state;
struct mpv_opengl_cb_context *mp_opengl_create(struct mpv_global *g,
                                               struct mp_client_api *client_api);
void kill_video(struct mp_client_api *client_api);

#endif
