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

#include "common/common.h"

struct clipboard_ctx;
struct mp_image;
struct MPContext;
struct mpv_global;

#define CLIPBOARD_INIT_ENABLE_MONITORING (1 << 0)

enum clipboard_data_type {
    CLIPBOARD_DATA_TEXT,
    CLIPBOARD_DATA_IMAGE,
};

enum clipboard_target {
    CLIPBOARD_TARGET_CLIPBOARD,
    CLIPBOARD_TARGET_PRIMARY_SELECTION,
};

enum clipboard_result {
    CLIPBOARD_SUCCESS = 0,
    CLIPBOARD_FAILED = -1,
    CLIPBOARD_UNAVAILABLE = -2,
};

struct clipboard_data {
    enum clipboard_data_type type;
    union {
        char *text;
        struct mp_image *image;
    } u;
};

struct clipboard_init_params {
    int flags;
    struct MPContext *mpctx; // For clipboard_vo only
    struct m_obj_settings *backends;
};

struct clipboard_access_params {
    int flags;
    enum clipboard_data_type type;
    enum clipboard_target target;
};

struct clipboard_backend {
    const char *name;
    const char *desc;

    // Return 0 on success, otherwise -1
    int (*init)(struct clipboard_ctx *cl, struct clipboard_init_params *params);
    void (*uninit)(struct clipboard_ctx *cl);
    bool (*data_changed)(struct clipboard_ctx *cl);
    int (*get_data)(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                    struct clipboard_data *out, void *talloc_ctx);
    int (*set_data)(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                    struct clipboard_data *data);
};

struct clipboard_ctx {
    const struct clipboard_backend *backend; // clipboard description structure
    struct mp_log *log;
    void *priv;   // backend-specific internal data
    bool monitor;
};

struct clipboard_ctx *mp_clipboard_create(struct clipboard_init_params *params,
                                          struct mpv_global *global);
void mp_clipboard_destroy(struct clipboard_ctx *cl);
bool mp_clipboard_data_changed(struct clipboard_ctx *cl);
int mp_clipboard_get_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                          struct clipboard_data *out, void *talloc_ctx);
int mp_clipboard_set_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                          struct clipboard_data *data);
const char *mp_clipboard_get_backend_name(struct clipboard_ctx *cl);

void reinit_clipboard(struct MPContext *mpctx);
