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

#ifndef MP_JNI_H
#define MP_JNI_H

#include <jni.h>
#include "common/msg.h"

/* Convenience macros */
#define MP_JNI_GET_ENV(obj) mp_jni_get_env((obj)->log)
#define MP_JNI_EXCEPTION_CHECK() mp_jni_exception_check(env, 0, NULL)
#define MP_JNI_EXCEPTION_LOG(obj) mp_jni_exception_check(env, 1, (obj)->log)
#define MP_JNI_DO(what, obj, method, ...) (*env)->what(env, obj, method, ##__VA_ARGS__)
#define MP_JNI_NEW(clazz, method, ...) MP_JNI_DO(NewObject, clazz, method, ##__VA_ARGS__)
#define MP_JNI_CALL_INT(obj, method, ...) MP_JNI_DO(CallIntMethod, obj, method, ##__VA_ARGS__)
#define MP_JNI_CALL_BOOL(obj, method, ...) MP_JNI_DO(CallBooleanMethod, obj, method, ##__VA_ARGS__)
#define MP_JNI_CALL_VOID(obj, method, ...) MP_JNI_DO(CallVoidMethod, obj, method, ##__VA_ARGS__)
#define MP_JNI_CALL_STATIC_INT(clazz, method, ...) MP_JNI_DO(CallStaticIntMethod, clazz, method, ##__VA_ARGS__)
#define MP_JNI_CALL_OBJECT(obj, method, ...) MP_JNI_DO(CallObjectMethod, obj, method, ##__VA_ARGS__)
#define MP_JNI_GET_INT(obj, field) MP_JNI_DO(GetIntField, obj, field)
#define MP_JNI_GET_LONG(obj, field) MP_JNI_DO(GetLongField, obj, field)
#define MP_JNI_GET_BOOL(obj, field) MP_JNI_DO(GetBoolField, obj, field)

/*
 * Attach permanently a JNI environment to the current thread and retrieve it.
 *
 * If successfully attached, the JNI environment will automatically be detached
 * at thread destruction.
 *
 * @param attached pointer to an integer that will be set to 1 if the
 * environment has been attached to the current thread or 0 if it is
 * already attached.
 * @param log context used for logging
 * @return the JNI environment on success, NULL otherwise
 */
JNIEnv *mp_jni_get_env(struct mp_log *log);

/*
 * Convert a jstring to its utf characters equivalent.
 *
 * @param env JNI environment
 * @param string Java string to convert
 * @param log context used for logging
 * @return a pointer to an array of unicode characters on success, NULL
 * otherwise
 */
char *mp_jni_jstring_to_utf_chars(JNIEnv *env, jstring string, struct mp_log *log);

/*
 * Convert utf chars to its jstring equivalent.
 *
 * @param env JNI environment
 * @param utf_chars a pointer to an array of unicode characters
 * @param log context used for logging
 * @return a Java string object on success, NULL otherwise
 */
jstring mp_jni_utf_chars_to_jstring(JNIEnv *env, const char *utf_chars, struct mp_log *log);

/*
 * Extract the error summary from a jthrowable in the form of "className: errorMessage"
 *
 * @param env JNI environment
 * @param exception exception to get the summary from
 * @param error address pointing to the error, the value is updated if a
 * summary can be extracted
 * @param log context used for logging
 * @return 0 on success, < 0 otherwise
 */
int mp_jni_exception_get_summary(JNIEnv *env, jthrowable exception, char **error, struct mp_log *log);

/*
 * Check if an exception has occurred,log it using av_log and clear it.
 *
 * @param env JNI environment
 * @param value used to enable logging if an exception has occurred,
 * 0 disables logging, != 0 enables logging
 * @param log context used for logging
 */
int mp_jni_exception_check(JNIEnv *env, int logging, struct mp_log *log);

/*
 * Jni field type.
 */
enum MPJniFieldType {

    MP_JNI_CLASS,
    MP_JNI_FIELD,
    MP_JNI_STATIC_FIELD,
    MP_JNI_STATIC_FIELD_AS_INT,
    MP_JNI_METHOD,
    MP_JNI_STATIC_METHOD

};

/*
 * Jni field describing a class, a field or a method to be retrieved using
 * the mp_jni_init_jfields method.
 */
struct MPJniField {

    const char *name;
    const char *method;
    const char *signature;
    enum MPJniFieldType type;
    int offset;
    int mandatory;

};

/*
 * Retrieve class references, field ids and method ids to an arbitrary structure.
 *
 * @param env JNI environment
 * @param jfields a pointer to an arbitrary structure where the different
 * fields are declared and where the MPJNIField mapping table offsets are
 * pointing to
 * @param jfields_mapping null terminated array of MPJNIFields describing
 * the class/field/method to be retrieved
 * @param global make the classes references global. It is the caller
 * responsibility to properly release global references.
 * @param log_ctx context used for logging, can be NULL
 * @return 0 on success, < 0 otherwise
 */
int mp_jni_init_jfields(JNIEnv *env, void *jfields, const struct MPJniField *jfields_mapping, int global, struct mp_log *log);

/*
 * Delete class references, field ids and method ids of an arbitrary structure.
 *
 * @param env JNI environment
 * @param jfields a pointer to an arbitrary structure where the different
 * fields are declared and where the MPJNIField mapping table offsets are
 * pointing to
 * @param jfields_mapping null terminated array of MPJNIFields describing
 * the class/field/method to be deleted
 * @param global treat the classes references as global and delete them
 * accordingly
 * @param log_ctx context used for logging, can be NULL
 * @return 0 on success, < 0 otherwise
 */
int mp_jni_reset_jfields(JNIEnv *env, void *jfields, const struct MPJniField *jfields_mapping, int global, struct mp_log *log);

#endif
