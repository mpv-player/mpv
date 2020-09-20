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

#include <libavcodec/jni.h>
#include <android/native_window_jni.h>

#include "android_common.h"
#include "common/msg.h"
#include "misc/jni.h"
#include "options/m_config.h"
#include "vo.h"

struct vo_android_state {
    struct mp_log *log;
    ANativeWindow *native_window;
};

int vo_android_init(struct vo *vo)
{
    vo->android = talloc_zero(vo, struct vo_android_state);
    struct vo_android_state *ctx = vo->android;

    *ctx = (struct vo_android_state){
        .log = mp_log_new(ctx, vo->log, "android"),
    };

    JNIEnv *env = MP_JNI_GET_ENV(ctx);
    if (!env) {
        MP_FATAL(ctx, "Could not attach java VM.\n");
        goto fail;
    }

    jobject surface = (jobject)(intptr_t)vo->opts->WinID;
    ctx->native_window = ANativeWindow_fromSurface(env, surface);
    if (!ctx->native_window) {
        MP_FATAL(ctx, "Failed to create ANativeWindow\n");
        goto fail;
    }

    return 1;
fail:
    talloc_free(ctx);
    vo->android = NULL;
    return 0;
}

void vo_android_uninit(struct vo *vo)
{
    struct vo_android_state *ctx = vo->android;
    if (!ctx)
        return;

    if (ctx->native_window)
        ANativeWindow_release(ctx->native_window);

    talloc_free(ctx);
    vo->android = NULL;
}

ANativeWindow *vo_android_native_window(struct vo *vo)
{
    struct vo_android_state *ctx = vo->android;
    return ctx->native_window;
}

bool vo_android_surface_size(struct vo *vo, int *out_w, int *out_h)
{
    struct vo_android_state *ctx = vo->android;

    int w = vo->opts->android_surface_size.w,
        h = vo->opts->android_surface_size.h;
    if (!w)
        w = ANativeWindow_getWidth(ctx->native_window);
    if (!h)
        h = ANativeWindow_getHeight(ctx->native_window);

    if (w <= 0 || h <= 0) {
        MP_ERR(ctx, "Failed to get height and width.\n");
        return false;
    }
    *out_w = w;
    *out_h = h;
    return true;
}
