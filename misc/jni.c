/*
 * JNI utility functions
 *
 * Copyright (c) 2015-2016 Matthieu Bouron <matthieu.bouron stupeflix.com>
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

#include <libavcodec/jni.h>
#include <libavutil/mem.h>
#include <libavutil/bprint.h>
#include <stdlib.h>

#include "jni.h"
#include "osdep/threads.h"

static JavaVM *java_vm;
static pthread_key_t current_env;
static mp_once once = MP_STATIC_ONCE_INITIALIZER;
static mp_static_mutex lock = MP_STATIC_MUTEX_INITIALIZER;

static void jni_detach_env(void *data)
{
    if (java_vm) {
        (*java_vm)->DetachCurrentThread(java_vm);
    }
}

static void jni_create_pthread_key(void)
{
    pthread_key_create(&current_env, jni_detach_env);
}

JNIEnv *mp_jni_get_env(struct mp_log *log)
{
    int ret = 0;
    JNIEnv *env = NULL;

    mp_mutex_lock(&lock);
    if (java_vm == NULL) {
        java_vm = av_jni_get_java_vm(NULL);
    }

    if (!java_vm) {
        mp_err(log, "No Java virtual machine has been registered\n");
        goto done;
    }

    mp_exec_once(&once, jni_create_pthread_key);

    if ((env = pthread_getspecific(current_env)) != NULL) {
        goto done;
    }

    ret = (*java_vm)->GetEnv(java_vm, (void **)&env, JNI_VERSION_1_6);
    switch(ret) {
    case JNI_EDETACHED:
        if ((*java_vm)->AttachCurrentThread(java_vm, &env, NULL) != 0) {
            mp_err(log, "Failed to attach the JNI environment to the current thread\n");
            env = NULL;
        } else {
            pthread_setspecific(current_env, env);
        }
        break;
    case JNI_OK:
        break;
    case JNI_EVERSION:
        mp_err(log, "The specified JNI version is not supported\n");
        break;
    default:
        mp_err(log, "Failed to get the JNI environment attached to this thread\n");
        break;
    }

done:
    mp_mutex_unlock(&lock);
    return env;
}

char *mp_jni_jstring_to_utf_chars(JNIEnv *env, jstring string, struct mp_log *log)
{
    char *ret = NULL;
    const char *utf_chars = NULL;

    jboolean copy = 0;

    if (!string) {
        return NULL;
    }

    utf_chars = (*env)->GetStringUTFChars(env, string, &copy);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        mp_err(log, "String.getStringUTFChars() threw an exception\n");
        return NULL;
    }

    ret = av_strdup(utf_chars);

    (*env)->ReleaseStringUTFChars(env, string, utf_chars);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        mp_err(log, "String.releaseStringUTFChars() threw an exception\n");
        return NULL;
    }

    return ret;
}

jstring mp_jni_utf_chars_to_jstring(JNIEnv *env, const char *utf_chars, struct mp_log *log)
{
    jstring ret;

    ret = (*env)->NewStringUTF(env, utf_chars);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        mp_err(log, "NewStringUTF() threw an exception\n");
        return NULL;
    }

    return ret;
}

int mp_jni_exception_get_summary(JNIEnv *env, jthrowable exception, char **error, struct mp_log *log)
{
    int ret = 0;

    AVBPrint bp;

    char *name = NULL;
    char *message = NULL;

    jclass class_class = NULL;
    jmethodID get_name_id = NULL;

    jclass exception_class = NULL;
    jmethodID get_message_id = NULL;

    jstring string = NULL;

    av_bprint_init(&bp, 0, AV_BPRINT_SIZE_AUTOMATIC);

    exception_class = (*env)->GetObjectClass(env, exception);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        mp_err(log, "Could not find Throwable class\n");
        ret = -1;
        goto done;
    }

    class_class = (*env)->GetObjectClass(env, exception_class);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        mp_err(log, "Could not find Throwable class's class\n");
        ret = -1;
        goto done;
    }

    get_name_id = (*env)->GetMethodID(env, class_class, "getName", "()Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        mp_err(log, "Could not find method Class.getName()\n");
        ret = -1;
        goto done;
    }

    string = (*env)->CallObjectMethod(env, exception_class, get_name_id);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        mp_err(log, "Class.getName() threw an exception\n");
        ret = -1;
        goto done;
    }

    if (string) {
        name = mp_jni_jstring_to_utf_chars(env, string, log);
        (*env)->DeleteLocalRef(env, string);
        string = NULL;
    }

    get_message_id = (*env)->GetMethodID(env, exception_class, "getMessage", "()Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        mp_err(log, "Could not find method java/lang/Throwable.getMessage()\n");
        ret = -1;
        goto done;
    }

    string = (*env)->CallObjectMethod(env, exception, get_message_id);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        mp_err(log, "Throwable.getMessage() threw an exception\n");
        ret = -1;
        goto done;
    }

    if (string) {
        message = mp_jni_jstring_to_utf_chars(env, string, log);
        (*env)->DeleteLocalRef(env, string);
        string = NULL;
    }

    if (name && message) {
        av_bprintf(&bp, "%s: %s", name, message);
    } else if (name && !message) {
        av_bprintf(&bp, "%s occurred", name);
    } else if (!name && message) {
        av_bprintf(&bp, "Exception: %s", message);
    } else {
        mp_warn(log, "Could not retrieve exception name and message\n");
        av_bprintf(&bp, "Exception occurred");
    }

    ret = av_bprint_finalize(&bp, error);
done:

    av_free(name);
    av_free(message);

    if (class_class) {
        (*env)->DeleteLocalRef(env, class_class);
    }

    if (exception_class) {
        (*env)->DeleteLocalRef(env, exception_class);
    }

    if (string) {
        (*env)->DeleteLocalRef(env, string);
    }

    return ret;
}

int mp_jni_exception_check(JNIEnv *env, int logging, struct mp_log *log)
{
    int ret;

    jthrowable exception;

    char *message = NULL;

    if (!(*(env))->ExceptionCheck((env))) {
        return 0;
    }

    if (!logging) {
        (*(env))->ExceptionClear((env));
        return -1;
    }

    exception = (*env)->ExceptionOccurred(env);
    (*(env))->ExceptionClear((env));

    if ((ret = mp_jni_exception_get_summary(env, exception, &message, log)) < 0) {
        (*env)->DeleteLocalRef(env, exception);
        return ret;
    }

    (*env)->DeleteLocalRef(env, exception);

    mp_err(log, "%s\n", message);
    av_free(message);

    return -1;
}

int mp_jni_init_jfields(JNIEnv *env, void *jfields, const struct MPJniField *jfields_mapping, int global, struct mp_log *log)
{
    int i, ret = 0;
    jclass last_clazz = NULL;

    for (i = 0; jfields_mapping[i].name; i++) {
        int mandatory = jfields_mapping[i].mandatory;
        enum MPJniFieldType type = jfields_mapping[i].type;

        if (type == MP_JNI_CLASS) {
            jclass clazz;

            last_clazz = NULL;

            clazz = (*env)->FindClass(env, jfields_mapping[i].name);
            if ((ret = mp_jni_exception_check(env, mandatory, log)) < 0 && mandatory) {
                goto done;
            }

            last_clazz = *(jclass*)((uint8_t*)jfields + jfields_mapping[i].offset) =
                    global ? (*env)->NewGlobalRef(env, clazz) : clazz;

            if (global) {
                (*env)->DeleteLocalRef(env, clazz);
            }

        } else {

            if (!last_clazz) {
                ret = -1;
                break;
            }

            switch(type) {
            case MP_JNI_FIELD: {
                jfieldID field_id = (*env)->GetFieldID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = mp_jni_exception_check(env, mandatory, log)) < 0 && mandatory) {
                    goto done;
                }

                *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = field_id;
                break;
            }
            case MP_JNI_STATIC_FIELD_AS_INT:
            case MP_JNI_STATIC_FIELD: {
                jfieldID field_id = (*env)->GetStaticFieldID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = mp_jni_exception_check(env, mandatory, log)) < 0 && mandatory) {
                    goto done;
                }

                if (type == MP_JNI_STATIC_FIELD_AS_INT) {
                    if (field_id) {
                        jint value = (*env)->GetStaticIntField(env, last_clazz, field_id);
                        if ((ret = mp_jni_exception_check(env, mandatory, log)) < 0 && mandatory) {
                            goto done;
                        }
                        *(jint*)((uint8_t*)jfields + jfields_mapping[i].offset) = value;
                    }
                } else {
                    *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = field_id;
                }
                break;
            }
            case MP_JNI_METHOD: {
                jmethodID method_id = (*env)->GetMethodID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = mp_jni_exception_check(env, mandatory, log)) < 0 && mandatory) {
                    goto done;
                }

                *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = method_id;
                break;
            }
            case MP_JNI_STATIC_METHOD: {
                jmethodID method_id = (*env)->GetStaticMethodID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = mp_jni_exception_check(env, mandatory, log)) < 0 && mandatory) {
                    goto done;
                }

                *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = method_id;
                break;
            }
            default:
                mp_err(log, "Unknown JNI field type\n");
                ret = -1;
                goto done;
            }

            ret = 0;
        }
    }

done:
    if (ret < 0) {
        /* reset jfields in case of failure so it does not leak references */
        mp_jni_reset_jfields(env, jfields, jfields_mapping, global, log);
    }

    return ret;
}

int mp_jni_reset_jfields(JNIEnv *env, void *jfields, const struct MPJniField *jfields_mapping, int global, struct mp_log *log)
{
    int i;

    for (i = 0; jfields_mapping[i].name; i++) {
        enum MPJniFieldType type = jfields_mapping[i].type;

        switch(type) {
        case MP_JNI_CLASS: {
            jclass clazz = *(jclass*)((uint8_t*)jfields + jfields_mapping[i].offset);
            if (!clazz)
                continue;

            if (global) {
                (*env)->DeleteGlobalRef(env, clazz);
            } else {
                (*env)->DeleteLocalRef(env, clazz);
            }

            *(jclass*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case MP_JNI_FIELD: {
            *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case MP_JNI_STATIC_FIELD: {
            *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case MP_JNI_STATIC_FIELD_AS_INT: {
            *(jint*)((uint8_t*)jfields + jfields_mapping[i].offset) = 0;
            break;
        }
        case MP_JNI_METHOD: {
            *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case MP_JNI_STATIC_METHOD: {
            *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        default:
            mp_err(log, "Unknown JNI field type\n");
        }
    }

    return 0;
}
