/*
 * Copyright (c) 2018 Aman Gupta <aman@tmm1.net>
 *
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

#include <assert.h>

#include <libavcodec/jni.h>
#include <libavcodec/mediacodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_mediacodec.h>
#include <pthread.h>

#include "config.h"
#include "context_android.h"
#include "common/global.h"
#include "misc/jni.h"
#include "options/m_config.h"
#include "osdep/timer.h"
#include "video/out/gpu/hwdec.h"
#include "video/mp_image_pool.h"
#include "ra_gl.h"

extern const struct m_sub_options android_conf;

struct priv_owner {
    struct mp_hwdec_ctx hwctx;
    jobject surface;
    jobject texture;
    GLuint gl_texture;
};

struct priv {
    struct mp_log *log;
    const struct ra_format *tex_format;

    jfloatArray matrix;
    jfloat transform[16];

    jclass listener_class;
    jobject listener;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int on_frame_available;
};

struct JNISurfaceTexture {
    jclass clazz;
    jmethodID ctor;
    jmethodID release;
    jmethodID attachToGLContext;
    jmethodID detachFromGLContext;
    jmethodID updateTexImage;
    jmethodID releaseTexImage;
    jmethodID getTransformMatrix;
    jmethodID setOnFrameAvailableListener;
    struct MPJniField mapping[];
} SurfaceTexture = {.mapping = {
    #define OFFSET(member) offsetof(struct JNISurfaceTexture, member)
    {"android/graphics/SurfaceTexture", NULL, NULL, MP_JNI_CLASS, OFFSET(clazz), 1},
    {"android/graphics/SurfaceTexture", "<init>", "(I)V", MP_JNI_METHOD, OFFSET(ctor), 1},
    {"android/graphics/SurfaceTexture", "release", "()V", MP_JNI_METHOD, OFFSET(release), 1},
    {"android/graphics/SurfaceTexture", "attachToGLContext", "(I)V", MP_JNI_METHOD, OFFSET(attachToGLContext), 1},
    {"android/graphics/SurfaceTexture", "detachFromGLContext", "()V", MP_JNI_METHOD, OFFSET(detachFromGLContext), 1},
    {"android/graphics/SurfaceTexture", "updateTexImage", "()V", MP_JNI_METHOD, OFFSET(updateTexImage), 1},
    {"android/graphics/SurfaceTexture", "releaseTexImage", "()V", MP_JNI_METHOD, OFFSET(releaseTexImage), 1},
    {"android/graphics/SurfaceTexture", "getTransformMatrix", "([F)V", MP_JNI_METHOD, OFFSET(getTransformMatrix), 1},
    {"android/graphics/SurfaceTexture", "setOnFrameAvailableListener", "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;)V", MP_JNI_METHOD, OFFSET(setOnFrameAvailableListener), 1},
    {0}
    #undef OFFSET
}};

struct JNISurface {
    jclass clazz;
    jmethodID ctor;
    jmethodID release;
    struct MPJniField mapping[];
} Surface = {.mapping = {
    #define OFFSET(member) offsetof(struct JNISurface, member)
    {"android/view/Surface", NULL, NULL, MP_JNI_CLASS, OFFSET(clazz), 1},
    {"android/view/Surface", "<init>", "(Landroid/graphics/SurfaceTexture;)V", MP_JNI_METHOD, OFFSET(ctor), 1},
    {"android/view/Surface", "release", "()V", MP_JNI_METHOD, OFFSET(release), 1},
    {0}
    #undef OFFSET
}};

static void uninit_jni(struct ra_hwdec *hw)
{
    JNIEnv *env = MP_JNI_GET_ENV(hw);
    mp_jni_reset_jfields(env, &Surface, Surface.mapping, 1, hw->log);
    mp_jni_reset_jfields(env, &SurfaceTexture, SurfaceTexture.mapping, 1, hw->log);
}

static int init_jni(struct ra_hwdec *hw)
{
    JNIEnv *env = MP_JNI_GET_ENV(hw);
    if (mp_jni_init_jfields(env, &Surface, Surface.mapping, 1, hw->log) < 0 ||
        mp_jni_init_jfields(env, &SurfaceTexture, SurfaceTexture.mapping, 1, hw->log) < 0) {
            uninit_jni(hw);
            return -1;
    }

    return 0;
}

static AVBufferRef *create_mediacodec_device_ref(jobject surface)
{
    AVBufferRef *device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
    if (!device_ref)
        return NULL;

    AVHWDeviceContext *ctx = (void *)device_ref->data;
    AVMediaCodecDeviceContext *hwctx = ctx->hwctx;
    hwctx->surface = surface;

    if (av_hwdevice_ctx_init(device_ref) < 0)
        av_buffer_unref(&device_ref);

    return device_ref;
}

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    GL *gl = ra_gl_get(hw->ra);
    if (gl->es == 0) {
        MP_ERR(hw, "GLES required. Try --opengl-es=yes\n");
        return -1;
    }

    static const char *es2_exts[] = {"GL_OES_EGL_image_external", 0};
    static const char *es3_exts[] = {"GL_OES_EGL_image_external_essl3", 0};
    if (strstr(gl->extensions, es3_exts[0]))
        hw->glsl_extensions = es3_exts;
    else
        hw->glsl_extensions = es2_exts;

    if (init_jni(hw) < 0)
        return -1;

    gl->GenTextures(1, &p->gl_texture);
    gl->BindTexture(GL_TEXTURE_EXTERNAL_OES, p->gl_texture);
    gl->TexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    JNIEnv *env = MP_JNI_GET_ENV(hw);
    jobject texture = MP_JNI_NEW(SurfaceTexture.clazz, SurfaceTexture.ctor, p->gl_texture);
    if (!texture || MP_JNI_EXCEPTION_LOG(hw) < 0) {
        MP_ERR(hw, "SurfaceTexture Init failed\n");
        return -1;
    }
    p->texture = (*env)->NewGlobalRef(env, texture);
    (*env)->DeleteLocalRef(env, texture);

    jobject surface = MP_JNI_NEW(Surface.clazz, Surface.ctor, p->texture);
    if (!surface || MP_JNI_EXCEPTION_LOG(hw) < 0) {
        MP_ERR(hw, "Surface Init failed\n");
        (*env)->DeleteGlobalRef(env, p->texture);
        return -1;
    }
    p->surface = (*env)->NewGlobalRef(env, surface);
    (*env)->DeleteLocalRef(env, surface);

    p->hwctx = (struct mp_hwdec_ctx){
        .driver_name = hw->driver->name,
        .av_device_ref = create_mediacodec_device_ref(p->surface),
    };
    hwdec_devices_add(hw->devs, &p->hwctx);

    return 0;
}

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    JNIEnv *env = MP_JNI_GET_ENV(hw);

    if (p->gl_texture) {
        GL *gl = ra_gl_get(hw->ra);
        MP_JNI_CALL_VOID(p->texture, SurfaceTexture.detachFromGLContext);
        MP_JNI_EXCEPTION_LOG(hw);
        gl->DeleteTextures(1, &p->gl_texture);
    }

    if (p->texture) {
        MP_JNI_CALL_VOID(p->texture, SurfaceTexture.release);
        MP_JNI_EXCEPTION_LOG(hw);
        (*env)->DeleteGlobalRef(env, p->texture);
        p->texture = NULL;
    }

    if (p->surface) {
        MP_JNI_CALL_VOID(p->surface, Surface.release);
        MP_JNI_EXCEPTION_LOG(hw);
        (*env)->DeleteGlobalRef(env, p->surface);
        p->surface = NULL;
    }

    hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);
}

static void native_on_frame_available(JNIEnv *env, jobject object, jlong native_ptr)
{
    struct priv *p = (struct priv *)(intptr_t)native_ptr;
    if (!p)
        return;

    pthread_mutex_lock(&p->lock);
    p->on_frame_available = 1;
    pthread_cond_signal(&p->cond);
    pthread_mutex_unlock(&p->lock);
}

static jclass surface_listener_new(struct ra_hwdec_mapper *mapper, jclass listener_class, void *ctx)
{
    jobject listener = NULL;
    JNIEnv *env = MP_JNI_GET_ENV(mapper);
    if (!env)
        return NULL;

    static const JNINativeMethod methods[] = {
        {"nativeOnFrameAvailable", "(J)V", (void *)&native_on_frame_available},
    };
    (*env)->RegisterNatives(env, listener_class, methods, 1);
    if (MP_JNI_EXCEPTION_LOG(mapper) < 0)
        goto done;

    jmethodID init_id = (*env)->GetMethodID(env, listener_class, "<init>", "()V");
    if (MP_JNI_EXCEPTION_LOG(mapper) < 0)
        goto done;

    jmethodID set_native_ptr_id = (*env)->GetMethodID(env, listener_class, "setNativePtr", "(J)V");
    if (MP_JNI_EXCEPTION_LOG(mapper) < 0)
        goto done;

    listener = (*env)->NewObject(env, listener_class, init_id);
    if (MP_JNI_EXCEPTION_LOG(mapper) < 0)
        goto done;

    (*env)->CallVoidMethod(env, listener, set_native_ptr_id, (jlong)(intptr_t)ctx);
    if (MP_JNI_EXCEPTION_LOG(mapper) < 0)
        goto done;

done:
    return listener;
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    struct priv_owner *o = mapper->owner->priv;
    GL *gl = ra_gl_get(mapper->ra);
    JNIEnv *env = MP_JNI_GET_ENV(mapper);

    p->log = mapper->log;
    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->cond, NULL);

    void *tmp = talloc_new(NULL);
    struct android_opts *opts = mp_get_config_group(tmp, mapper->owner->global, &android_conf);
    p->listener_class = (jclass)(intptr_t)opts->listener_class;
    talloc_free(tmp);

    if (!p->listener_class) {
        MP_ERR(mapper, "SurfaceTexture mapping may not work reliably without a Java Surface.OnFrameAvailableListener.\n");
        MP_ERR(mapper, "Import the following class into your application and set `--android-surfacetexture-listener-class=(int64_t)(intptr_t)NewGlobalRef((jclass)io.mpv.NativeOnFrameAvailableListener)`:\n");
        MP_ERR(mapper, "    package io.mpv\n");
        MP_ERR(mapper, "    import android.graphics.SurfaceTexture\n");
        MP_ERR(mapper, "    class NativeOnFrameAvailableListener: SurfaceTexture.OnFrameAvailableListener {\n");
        MP_ERR(mapper, "        var nativePtr = 0L\n");
        MP_ERR(mapper, "        override fun onFrameAvailable(texture: SurfaceTexture?) {\n");
        MP_ERR(mapper, "            nativeOnFrameAvailable(nativePtr)\n");
        MP_ERR(mapper, "        }\n");
        MP_ERR(mapper, "        private external fun nativeOnFrameAvailable(nativePtr: Long)\n");
        MP_ERR(mapper, "    }\n");
    } else {
        jobject listener = surface_listener_new(mapper, p->listener_class, p);
        if (!listener) {
            MP_FATAL(mapper, "NativeOnFrameAvailableListener could not be created from `--android-surfacetexture-listener-class=(jclass)%p`.\n", p->listener_class);
            return -1;
        }
        p->listener = (*env)->NewGlobalRef(env, listener);
        (*env)->DeleteLocalRef(env, listener);

        MP_JNI_CALL_VOID(o->texture, SurfaceTexture.setOnFrameAvailableListener, p->listener);
        if (MP_JNI_EXCEPTION_LOG(mapper) < 0) return -1;
    }

    jfloatArray matrix = (*env)->NewFloatArray(env, 16);
    if (MP_JNI_EXCEPTION_LOG(mapper) < 0) return -1;
    p->matrix = (*env)->NewGlobalRef(env, matrix);
    (*env)->DeleteLocalRef(env, matrix);

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = IMGFMT_RGB0;
    mapper->dst_params.hw_subfmt = 0;

    p->tex_format = ra_find_unorm_format(mapper->ra, 1, 4);

    return 0;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    struct priv_owner *o = mapper->owner->priv;
    JNIEnv *env = MP_JNI_GET_ENV(mapper);

    if (p->listener) {
        jmethodID set_native_ptr_id = (*env)->GetMethodID(env, p->listener_class, "setNativePtr", "(J)V");
        MP_JNI_EXCEPTION_LOG(mapper);
        (*env)->CallVoidMethod(env, p->listener, set_native_ptr_id, (jlong)0);
        MP_JNI_EXCEPTION_LOG(mapper);

        MP_JNI_CALL_VOID(o->texture, SurfaceTexture.setOnFrameAvailableListener, NULL);
        MP_JNI_EXCEPTION_LOG(mapper);

        (*env)->DeleteGlobalRef(env, p->listener);
        p->listener = NULL;
    }

    (*env)->DeleteGlobalRef(env, p->matrix);

    pthread_mutex_destroy(&p->lock);
    pthread_cond_destroy(&p->cond);
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    ra_tex_free(mapper->ra, &mapper->tex[0]);
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    struct priv_owner *o = mapper->owner->priv;
    JNIEnv *env = MP_JNI_GET_ENV(mapper);

    if (!o->gl_texture)
        return -1;

    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)mapper->src->planes[3];
    int frame_available = 0;

    pthread_mutex_lock(&p->lock);
    p->on_frame_available = 0;
    av_mediacodec_release_buffer(buffer, 1);
    if (p->listener) {
        struct timespec ts = mp_rel_time_to_timespec(0.030);
        int ret = pthread_cond_timedwait(&p->cond, &p->lock, &ts);
        if (ret < 0)
            MP_WARN(mapper, "Failed to wait on signal: %s\n", mp_strerror(ret));
        frame_available = p->on_frame_available;
    } else {
        frame_available = 1;
    }
    pthread_mutex_unlock(&p->lock);

    if (!frame_available)
        MP_WARN(mapper, "no frame!\n");

    MP_JNI_CALL_VOID(o->texture, SurfaceTexture.updateTexImage);
    if (MP_JNI_EXCEPTION_LOG(mapper->owner)) return -1;

    MP_JNI_CALL_VOID(o->texture, SurfaceTexture.getTransformMatrix, p->matrix);
    if (MP_JNI_EXCEPTION_LOG(mapper->owner)) return -1;

    (*env)->GetFloatArrayRegion(env, p->matrix, 0, 16, p->transform);
    if (MP_JNI_EXCEPTION_LOG(mapper->owner)) return -1;

    mapper->transform = (struct gl_transform){
        .m = {{p->transform[0],  p->transform[1]},
              {p->transform[4], -p->transform[5]}},
        .t = {1.0 - p->transform[13], p->transform[14]}
    };

    struct ra_tex_params params = {
        .dimensions = 2,
        .w = mapper->src_params.w,
        .h = mapper->src_params.h,
        .d = 1,
        .format = p->tex_format,
        .render_src = true,
        .src_linear = true,
        .external_oes = true,
    };
    if (!params.format)
        return -1;

    mapper->tex[0] = ra_create_wrapped_tex(mapper->ra, &params, o->gl_texture);
    if (!mapper->tex[0])
        return -1;

    return 0;
}

const struct ra_hwdec_driver ra_hwdec_surfacetexture = {
    .name = "surfacetexture",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_MEDIACODEC, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
