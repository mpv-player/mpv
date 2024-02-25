/*
 * Android AudioTrack audio output driver.
 * Copyright (C) 2018 Aman Gupta <aman@tmm1.net>
 * Copyright (C) 2012-2015 VLC authors and VideoLAN, VideoLabs
 * Authors: Thomas Guillem <thomas@gllm.fr>
 *          Ming Hu <tewilove@gmail.com>
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

#include "ao.h"
#include "internal.h"
#include "common/msg.h"
#include "audio/format.h"
#include "options/m_option.h"
#include "osdep/threads.h"
#include "osdep/timer.h"
#include "misc/jni.h"

struct priv {
    jobject audiotrack;
    jint samplerate;
    jint channel_config;
    jint format;
    jint size;

    jobject timestamp;
    int64_t timestamp_fetched;
    bool timestamp_set;
    int timestamp_stable;

    uint32_t written_frames; /* requires uint32_t rollover semantics */
    uint32_t playhead_pos;
    uint32_t playhead_offset;
    bool reset_pending;

    void *chunk;
    int chunksize;
    jbyteArray bytearray;
    jshortArray shortarray;
    jfloatArray floatarray;
    jobject bbuf;

    bool cfg_pcm_float;
    int cfg_session_id;

    bool needs_timestamp_offset;
    int64_t timestamp_offset;

    bool thread_terminate;
    bool thread_created;
    mp_thread thread;
    mp_mutex lock;
    mp_cond wakeup;
};

struct JNIByteBuffer {
    jclass clazz;
    jmethodID clear;
    struct MPJniField mapping[];
} ByteBuffer = {.mapping = {
    #define OFFSET(member) offsetof(struct JNIByteBuffer, member)
    {"java/nio/ByteBuffer", NULL, NULL, MP_JNI_CLASS, OFFSET(clazz), 1},
    {"java/nio/ByteBuffer", "clear", "()Ljava/nio/Buffer;", MP_JNI_METHOD, OFFSET(clear), 1},
    {0},
    #undef OFFSET
}};

struct JNIAudioTrack {
    jclass clazz;
    jmethodID ctor;
    jmethodID ctorV21;
    jmethodID release;
    jmethodID getState;
    jmethodID getPlayState;
    jmethodID play;
    jmethodID stop;
    jmethodID flush;
    jmethodID pause;
    jmethodID write;
    jmethodID writeFloat;
    jmethodID writeShortV23;
    jmethodID writeBufferV21;
    jmethodID getBufferSizeInFramesV23;
    jmethodID getPlaybackHeadPosition;
    jmethodID getTimestamp;
    jmethodID getLatency;
    jmethodID getMinBufferSize;
    jmethodID getNativeOutputSampleRate;
    jint STATE_INITIALIZED;
    jint PLAYSTATE_STOPPED;
    jint PLAYSTATE_PAUSED;
    jint PLAYSTATE_PLAYING;
    jint MODE_STREAM;
    jint ERROR;
    jint ERROR_BAD_VALUE;
    jint ERROR_INVALID_OPERATION;
    jint WRITE_BLOCKING;
    jint WRITE_NON_BLOCKING;
    struct MPJniField mapping[];
} AudioTrack = {.mapping = {
    #define OFFSET(member) offsetof(struct JNIAudioTrack, member)
    {"android/media/AudioTrack", NULL, NULL, MP_JNI_CLASS, OFFSET(clazz), 1},
    {"android/media/AudioTrack", "<init>", "(IIIIIII)V", MP_JNI_METHOD, OFFSET(ctor), 1},
    {"android/media/AudioTrack", "<init>", "(Landroid/media/AudioAttributes;Landroid/media/AudioFormat;III)V", MP_JNI_METHOD, OFFSET(ctorV21), 0},
    {"android/media/AudioTrack", "release", "()V", MP_JNI_METHOD, OFFSET(release), 1},
    {"android/media/AudioTrack", "getState", "()I", MP_JNI_METHOD, OFFSET(getState), 1},
    {"android/media/AudioTrack", "getPlayState", "()I", MP_JNI_METHOD, OFFSET(getPlayState), 1},
    {"android/media/AudioTrack", "play", "()V", MP_JNI_METHOD, OFFSET(play), 1},
    {"android/media/AudioTrack", "stop", "()V", MP_JNI_METHOD, OFFSET(stop), 1},
    {"android/media/AudioTrack", "flush", "()V", MP_JNI_METHOD, OFFSET(flush), 1},
    {"android/media/AudioTrack", "pause", "()V", MP_JNI_METHOD, OFFSET(pause), 1},
    {"android/media/AudioTrack", "write", "([BII)I", MP_JNI_METHOD, OFFSET(write), 1},
    {"android/media/AudioTrack", "write", "([FIII)I", MP_JNI_METHOD, OFFSET(writeFloat), 1},
    {"android/media/AudioTrack", "write", "([SIII)I", MP_JNI_METHOD, OFFSET(writeShortV23), 0},
    {"android/media/AudioTrack", "write", "(Ljava/nio/ByteBuffer;II)I", MP_JNI_METHOD, OFFSET(writeBufferV21), 1},
    {"android/media/AudioTrack", "getBufferSizeInFrames", "()I", MP_JNI_METHOD, OFFSET(getBufferSizeInFramesV23), 0},
    {"android/media/AudioTrack", "getTimestamp", "(Landroid/media/AudioTimestamp;)Z", MP_JNI_METHOD, OFFSET(getTimestamp), 1},
    {"android/media/AudioTrack", "getPlaybackHeadPosition", "()I", MP_JNI_METHOD, OFFSET(getPlaybackHeadPosition), 1},
    {"android/media/AudioTrack", "getLatency", "()I", MP_JNI_METHOD, OFFSET(getLatency), 1},
    {"android/media/AudioTrack", "getMinBufferSize", "(III)I", MP_JNI_STATIC_METHOD, OFFSET(getMinBufferSize), 1},
    {"android/media/AudioTrack", "getNativeOutputSampleRate", "(I)I", MP_JNI_STATIC_METHOD, OFFSET(getNativeOutputSampleRate), 1},
    {"android/media/AudioTrack", "WRITE_BLOCKING", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(WRITE_BLOCKING), 0},
    {"android/media/AudioTrack", "WRITE_NON_BLOCKING", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(WRITE_NON_BLOCKING), 0},
    {"android/media/AudioTrack", "STATE_INITIALIZED", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(STATE_INITIALIZED), 1},
    {"android/media/AudioTrack", "PLAYSTATE_STOPPED", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(PLAYSTATE_STOPPED), 1},
    {"android/media/AudioTrack", "PLAYSTATE_PAUSED", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(PLAYSTATE_PAUSED), 1},
    {"android/media/AudioTrack", "PLAYSTATE_PLAYING", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(PLAYSTATE_PLAYING), 1},
    {"android/media/AudioTrack", "MODE_STREAM", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(MODE_STREAM), 1},
    {"android/media/AudioTrack", "ERROR", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(ERROR), 1},
    {"android/media/AudioTrack", "ERROR_BAD_VALUE", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(ERROR_BAD_VALUE), 1},
    {"android/media/AudioTrack", "ERROR_INVALID_OPERATION", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(ERROR_INVALID_OPERATION), 1},
    {0}
    #undef OFFSET
}};

struct JNIAudioAttributes {
    jclass clazz;
    jint CONTENT_TYPE_MOVIE;
    jint CONTENT_TYPE_MUSIC;
    jint USAGE_MEDIA;
    struct MPJniField mapping[];
} AudioAttributes = {.mapping = {
    #define OFFSET(member) offsetof(struct JNIAudioAttributes, member)
    {"android/media/AudioAttributes", NULL, NULL, MP_JNI_CLASS, OFFSET(clazz), 0},
    {"android/media/AudioAttributes", "CONTENT_TYPE_MOVIE", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(CONTENT_TYPE_MOVIE), 0},
    {"android/media/AudioAttributes", "CONTENT_TYPE_MUSIC", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(CONTENT_TYPE_MUSIC), 0},
    {"android/media/AudioAttributes", "USAGE_MEDIA", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(USAGE_MEDIA), 0},
    {0}
    #undef OFFSET
}};

struct JNIAudioAttributesBuilder {
    jclass clazz;
    jmethodID ctor;
    jmethodID setUsage;
    jmethodID setContentType;
    jmethodID build;
    struct MPJniField mapping[];
} AudioAttributesBuilder = {.mapping = {
    #define OFFSET(member) offsetof(struct JNIAudioAttributesBuilder, member)
    {"android/media/AudioAttributes$Builder", NULL, NULL, MP_JNI_CLASS, OFFSET(clazz), 0},
    {"android/media/AudioAttributes$Builder", "<init>", "()V", MP_JNI_METHOD, OFFSET(ctor), 0},
    {"android/media/AudioAttributes$Builder", "setUsage", "(I)Landroid/media/AudioAttributes$Builder;", MP_JNI_METHOD, OFFSET(setUsage), 0},
    {"android/media/AudioAttributes$Builder", "setContentType", "(I)Landroid/media/AudioAttributes$Builder;", MP_JNI_METHOD, OFFSET(setContentType), 0},
    {"android/media/AudioAttributes$Builder", "build", "()Landroid/media/AudioAttributes;", MP_JNI_METHOD, OFFSET(build), 0},
    {0}
    #undef OFFSET
}};

struct JNIAudioFormat {
    jclass clazz;
    jint ENCODING_PCM_8BIT;
    jint ENCODING_PCM_16BIT;
    jint ENCODING_PCM_FLOAT;
    jint ENCODING_IEC61937;
    jint CHANNEL_OUT_MONO;
    jint CHANNEL_OUT_STEREO;
    jint CHANNEL_OUT_FRONT_CENTER;
    jint CHANNEL_OUT_QUAD;
    jint CHANNEL_OUT_5POINT1;
    jint CHANNEL_OUT_BACK_CENTER;
    jint CHANNEL_OUT_7POINT1_SURROUND;
    struct MPJniField mapping[];
} AudioFormat = {.mapping = {
    #define OFFSET(member) offsetof(struct JNIAudioFormat, member)
    {"android/media/AudioFormat", NULL, NULL, MP_JNI_CLASS, OFFSET(clazz), 1},
    {"android/media/AudioFormat", "ENCODING_PCM_8BIT", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(ENCODING_PCM_8BIT), 1},
    {"android/media/AudioFormat", "ENCODING_PCM_16BIT", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(ENCODING_PCM_16BIT), 1},
    {"android/media/AudioFormat", "ENCODING_PCM_FLOAT", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(ENCODING_PCM_FLOAT), 1},
    {"android/media/AudioFormat", "ENCODING_IEC61937", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(ENCODING_IEC61937), 0},
    {"android/media/AudioFormat", "CHANNEL_OUT_MONO", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(CHANNEL_OUT_MONO), 1},
    {"android/media/AudioFormat", "CHANNEL_OUT_STEREO", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(CHANNEL_OUT_STEREO), 1},
    {"android/media/AudioFormat", "CHANNEL_OUT_FRONT_CENTER", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(CHANNEL_OUT_FRONT_CENTER), 1},
    {"android/media/AudioFormat", "CHANNEL_OUT_QUAD", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(CHANNEL_OUT_QUAD), 1},
    {"android/media/AudioFormat", "CHANNEL_OUT_5POINT1", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(CHANNEL_OUT_5POINT1), 1},
    {"android/media/AudioFormat", "CHANNEL_OUT_BACK_CENTER", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(CHANNEL_OUT_BACK_CENTER), 1},
    {"android/media/AudioFormat", "CHANNEL_OUT_7POINT1_SURROUND", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(CHANNEL_OUT_7POINT1_SURROUND), 0},
    {0}
    #undef OFFSET
}};

struct JNIAudioFormatBuilder {
    jclass clazz;
    jmethodID ctor;
    jmethodID setEncoding;
    jmethodID setSampleRate;
    jmethodID setChannelMask;
    jmethodID build;
    struct MPJniField mapping[];
} AudioFormatBuilder = {.mapping = {
    #define OFFSET(member) offsetof(struct JNIAudioFormatBuilder, member)
    {"android/media/AudioFormat$Builder", NULL, NULL, MP_JNI_CLASS, OFFSET(clazz), 0},
    {"android/media/AudioFormat$Builder", "<init>", "()V", MP_JNI_METHOD, OFFSET(ctor), 0},
    {"android/media/AudioFormat$Builder", "setEncoding", "(I)Landroid/media/AudioFormat$Builder;", MP_JNI_METHOD, OFFSET(setEncoding), 0},
    {"android/media/AudioFormat$Builder", "setSampleRate", "(I)Landroid/media/AudioFormat$Builder;", MP_JNI_METHOD, OFFSET(setSampleRate), 0},
    {"android/media/AudioFormat$Builder", "setChannelMask", "(I)Landroid/media/AudioFormat$Builder;", MP_JNI_METHOD, OFFSET(setChannelMask), 0},
    {"android/media/AudioFormat$Builder", "build", "()Landroid/media/AudioFormat;", MP_JNI_METHOD, OFFSET(build), 0},
    {0}
    #undef OFFSET
}};


struct JNIAudioManager {
    jclass clazz;
    jint ERROR_DEAD_OBJECT;
    jint STREAM_MUSIC;
    struct MPJniField mapping[];
} AudioManager = {.mapping = {
    #define OFFSET(member) offsetof(struct JNIAudioManager, member)
    {"android/media/AudioManager", NULL, NULL, MP_JNI_CLASS, OFFSET(clazz), 1},
    {"android/media/AudioManager", "STREAM_MUSIC", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(STREAM_MUSIC), 1},
    {"android/media/AudioManager", "ERROR_DEAD_OBJECT", "I", MP_JNI_STATIC_FIELD_AS_INT, OFFSET(ERROR_DEAD_OBJECT), 0},
    {0}
    #undef OFFSET
}};

struct JNIAudioTimestamp {
    jclass clazz;
    jmethodID ctor;
    jfieldID framePosition;
    jfieldID nanoTime;
    struct MPJniField mapping[];
} AudioTimestamp = {.mapping = {
    #define OFFSET(member) offsetof(struct JNIAudioTimestamp, member)
    {"android/media/AudioTimestamp", NULL, NULL, MP_JNI_CLASS, OFFSET(clazz), 1},
    {"android/media/AudioTimestamp", "<init>", "()V", MP_JNI_METHOD, OFFSET(ctor), 1},
    {"android/media/AudioTimestamp", "framePosition", "J", MP_JNI_FIELD, OFFSET(framePosition), 1},
    {"android/media/AudioTimestamp", "nanoTime", "J", MP_JNI_FIELD, OFFSET(nanoTime), 1},
    {0}
    #undef OFFSET
}};

#define MP_JNI_DELETELOCAL(o) (*env)->DeleteLocalRef(env, o)

static int AudioTrack_New(struct ao *ao)
{
    struct priv *p = ao->priv;
    JNIEnv *env = MP_JNI_GET_ENV(ao);
    jobject audiotrack = NULL;

    if (AudioTrack.ctorV21) {
        MP_VERBOSE(ao, "Using API21 initializer\n");
        jobject tmp = NULL;

        jobject format_builder = MP_JNI_NEW(AudioFormatBuilder.clazz, AudioFormatBuilder.ctor);
        MP_JNI_EXCEPTION_LOG(ao);
        tmp = MP_JNI_CALL_OBJECT(format_builder, AudioFormatBuilder.setEncoding, p->format);
        MP_JNI_DELETELOCAL(tmp);
        tmp = MP_JNI_CALL_OBJECT(format_builder, AudioFormatBuilder.setSampleRate, p->samplerate);
        MP_JNI_DELETELOCAL(tmp);
        tmp = MP_JNI_CALL_OBJECT(format_builder, AudioFormatBuilder.setChannelMask, p->channel_config);
        MP_JNI_DELETELOCAL(tmp);
        jobject format = MP_JNI_CALL_OBJECT(format_builder, AudioFormatBuilder.build);
        MP_JNI_DELETELOCAL(format_builder);

        jobject attr_builder = MP_JNI_NEW(AudioAttributesBuilder.clazz, AudioAttributesBuilder.ctor);
        MP_JNI_EXCEPTION_LOG(ao);
        tmp = MP_JNI_CALL_OBJECT(attr_builder, AudioAttributesBuilder.setUsage, AudioAttributes.USAGE_MEDIA);
        MP_JNI_DELETELOCAL(tmp);
        jint content_type = (ao->init_flags & AO_INIT_MEDIA_ROLE_MUSIC) ?
            AudioAttributes.CONTENT_TYPE_MUSIC : AudioAttributes.CONTENT_TYPE_MOVIE;
        tmp = MP_JNI_CALL_OBJECT(attr_builder, AudioAttributesBuilder.setContentType, content_type);
        MP_JNI_DELETELOCAL(tmp);
        jobject attr = MP_JNI_CALL_OBJECT(attr_builder, AudioAttributesBuilder.build);
        MP_JNI_DELETELOCAL(attr_builder);

        audiotrack = MP_JNI_NEW(
            AudioTrack.clazz,
            AudioTrack.ctorV21,
            attr,
            format,
            p->size,
            AudioTrack.MODE_STREAM,
            p->cfg_session_id
        );

        MP_JNI_DELETELOCAL(format);
        MP_JNI_DELETELOCAL(attr);
    } else {
        MP_VERBOSE(ao, "Using legacy initializer\n");
        audiotrack = MP_JNI_NEW(
            AudioTrack.clazz,
            AudioTrack.ctor,
            AudioManager.STREAM_MUSIC,
            p->samplerate,
            p->channel_config,
            p->format,
            p->size,
            AudioTrack.MODE_STREAM,
            p->cfg_session_id
        );
    }
    if (MP_JNI_EXCEPTION_LOG(ao) < 0 || !audiotrack) {
        MP_FATAL(ao, "AudioTrack Init failed\n");
        return -1;
    }

    if (MP_JNI_CALL_INT(audiotrack, AudioTrack.getState) != AudioTrack.STATE_INITIALIZED) {
        MP_JNI_CALL_VOID(audiotrack, AudioTrack.release);
        MP_JNI_EXCEPTION_LOG(ao);
        (*env)->DeleteLocalRef(env, audiotrack);
        MP_ERR(ao, "AudioTrack.getState failed\n");
        return -1;
    }

    if (AudioTrack.getBufferSizeInFramesV23) {
        int bufferSize = MP_JNI_CALL_INT(audiotrack, AudioTrack.getBufferSizeInFramesV23);
        if (bufferSize > 0) {
            MP_VERBOSE(ao, "AudioTrack.getBufferSizeInFrames = %d\n", bufferSize);
            ao->device_buffer = bufferSize;
        }
    }

    p->audiotrack = (*env)->NewGlobalRef(env, audiotrack);
    (*env)->DeleteLocalRef(env, audiotrack);
    if (!p->audiotrack)
        return -1;

    return 0;
}

static int AudioTrack_Recreate(struct ao *ao)
{
    struct priv *p = ao->priv;
    JNIEnv *env = MP_JNI_GET_ENV(ao);

    MP_JNI_CALL_VOID(p->audiotrack, AudioTrack.release);
    MP_JNI_EXCEPTION_LOG(ao);
    (*env)->DeleteGlobalRef(env, p->audiotrack);
    p->audiotrack = NULL;
    return AudioTrack_New(ao);
}

static uint32_t AudioTrack_getPlaybackHeadPosition(struct ao *ao)
{
    struct priv *p = ao->priv;
    if (!p->audiotrack)
        return 0;
    JNIEnv *env = MP_JNI_GET_ENV(ao);
    uint32_t pos = 0;
    int64_t now = mp_raw_time_ns();
    int state = MP_JNI_CALL_INT(p->audiotrack, AudioTrack.getPlayState);

    int stable_count = 20;
    int64_t wait = p->timestamp_stable < stable_count ? 50000000 : 3000000000;

    if (state == AudioTrack.PLAYSTATE_PLAYING && p->format != AudioFormat.ENCODING_IEC61937 &&
        (p->timestamp_fetched == 0 || now - p->timestamp_fetched >= wait)) {
        if (!p->timestamp_fetched)
            p->timestamp_stable = 0;

        int64_t time1 = MP_JNI_GET_LONG(p->timestamp, AudioTimestamp.nanoTime);
        if (MP_JNI_CALL_BOOL(p->audiotrack, AudioTrack.getTimestamp, p->timestamp)) {
            p->timestamp_set = true;
            p->timestamp_fetched = now;
            if (p->timestamp_stable < stable_count) {
                uint32_t fpos = 0xFFFFFFFFL & MP_JNI_GET_LONG(p->timestamp, AudioTimestamp.framePosition);
                int64_t time2 = MP_JNI_GET_LONG(p->timestamp, AudioTimestamp.nanoTime);
                //MP_VERBOSE(ao, "getTimestamp: fpos= %u / time= %"PRId64" / now= %"PRId64" / stable= %d\n", fpos, time2, now, p->timestamp_stable);
                if (time1 != time2 && time2 != 0 && fpos != 0) {
                    p->timestamp_stable++;
                }
            }
        }
    }

    /* AudioTrack's framePosition and playbackHeadPosition return a signed integer,
     * but documentation states it should be interpreted as a 32-bit unsigned integer.
     */
    if (p->timestamp_set) {
        pos = 0xFFFFFFFFL & MP_JNI_GET_LONG(p->timestamp, AudioTimestamp.framePosition);
        uint32_t fpos = pos;
        int64_t time = MP_JNI_GET_LONG(p->timestamp, AudioTimestamp.nanoTime);
        if (time == 0)
            fpos = pos = 0;
        if (p->needs_timestamp_offset) {
            if (time != 0 && !p->timestamp_offset)
                p->timestamp_offset = now - time;
            time += p->timestamp_offset;
        }
        if (fpos != 0 && time != 0 && state == AudioTrack.PLAYSTATE_PLAYING) {
            double diff = (double)(now - time) / 1e9;
            pos += diff * ao->samplerate;
        }
        //MP_VERBOSE(ao, "position = %u via getTimestamp (state = %d / fpos= %u / time= %"PRId64")\n", pos, state, fpos, time);
    } else {
        pos = 0xFFFFFFFFL & MP_JNI_CALL_INT(p->audiotrack, AudioTrack.getPlaybackHeadPosition);
        //MP_VERBOSE(ao, "playbackHeadPosition = %u (reset_pending=%d)\n", pos, p->reset_pending);
    }


    if (p->format == AudioFormat.ENCODING_IEC61937) {
        if (p->reset_pending) {
            // after a flush(), playbackHeadPosition will not reset to 0 right away.
            // sometimes, it will never reset at all.
            // save the initial offset after the reset, to subtract it going forward.
            if (p->playhead_offset == 0)
                p->playhead_offset = pos;
            p->reset_pending = false;
            MP_VERBOSE(ao, "IEC/playbackHead offset = %d\n", pos);
        }

        // usually shortly after a flush(), playbackHeadPosition will reset to 0.
        // clear out the position and offset to avoid regular "rollover" below
        if (pos == 0 && p->playhead_offset != 0) {
            MP_VERBOSE(ao, "IEC/playbackHeadPosition %d -> %d (flush)\n", p->playhead_pos, pos);
            p->playhead_offset = 0;
            p->playhead_pos = 0;
        }

        // sometimes on a new AudioTrack instance, playbackHeadPosition will reset
        // to 0 shortly after playback starts for no reason.
        if (pos == 0 && p->playhead_pos != 0) {
            MP_VERBOSE(ao, "IEC/playbackHeadPosition %d -> %d (reset)\n", p->playhead_pos, pos);
            p->playhead_offset = 0;
            p->playhead_pos = 0;
            p->written_frames = 0;
        }
    }

    p->playhead_pos = pos;
    return p->playhead_pos - p->playhead_offset;
}

static double AudioTrack_getLatency(struct ao *ao)
{
    JNIEnv *env = MP_JNI_GET_ENV(ao);
    struct priv *p = ao->priv;
    if (!p->audiotrack)
        return 0;

    uint32_t playhead = AudioTrack_getPlaybackHeadPosition(ao);
    uint32_t diff = p->written_frames - playhead;
    double delay = diff / (double)(ao->samplerate);
    if (!p->timestamp_set &&
        p->format != AudioFormat.ENCODING_IEC61937)
        delay += (double)MP_JNI_CALL_INT(p->audiotrack, AudioTrack.getLatency)/1000.0;
    if (delay > 2.0) {
        //MP_WARN(ao, "getLatency: written=%u playhead=%u diff=%u delay=%f\n", p->written_frames, playhead, diff, delay);
        p->timestamp_fetched = 0;
        return 0;
    }
    return MPCLAMP(delay, 0.0, 2.0);
}

static int AudioTrack_write(struct ao *ao, int len)
{
    struct priv *p = ao->priv;
    if (!p->audiotrack)
        return -1;
    JNIEnv *env = MP_JNI_GET_ENV(ao);
    void *buf = p->chunk;

    jint ret;
    if (p->format == AudioFormat.ENCODING_IEC61937) {
        (*env)->SetShortArrayRegion(env, p->shortarray, 0, len / 2, buf);
        if (MP_JNI_EXCEPTION_LOG(ao) < 0) return -1;
        ret = MP_JNI_CALL_INT(p->audiotrack, AudioTrack.writeShortV23, p->shortarray, 0, len / 2, AudioTrack.WRITE_BLOCKING);
        if (MP_JNI_EXCEPTION_LOG(ao) < 0) return -1;
        if (ret > 0) ret *= 2;

    } else if (AudioTrack.writeBufferV21) {
        // reset positions for reading
        jobject bbuf = MP_JNI_CALL_OBJECT(p->bbuf, ByteBuffer.clear);
        if (MP_JNI_EXCEPTION_LOG(ao) < 0) return -1;
        (*env)->DeleteLocalRef(env, bbuf);
        ret = MP_JNI_CALL_INT(p->audiotrack, AudioTrack.writeBufferV21, p->bbuf, len, AudioTrack.WRITE_BLOCKING);
        if (MP_JNI_EXCEPTION_LOG(ao) < 0) return -1;

    } else if (p->format == AudioFormat.ENCODING_PCM_FLOAT) {
        (*env)->SetFloatArrayRegion(env, p->floatarray, 0, len / sizeof(float), buf);
        if (MP_JNI_EXCEPTION_LOG(ao) < 0) return -1;
        ret = MP_JNI_CALL_INT(p->audiotrack, AudioTrack.writeFloat, p->floatarray, 0, len / sizeof(float), AudioTrack.WRITE_BLOCKING);
        if (MP_JNI_EXCEPTION_LOG(ao) < 0) return -1;
        if (ret > 0) ret *= sizeof(float);

    } else {
        (*env)->SetByteArrayRegion(env, p->bytearray, 0, len, buf);
        if (MP_JNI_EXCEPTION_LOG(ao) < 0) return -1;
        ret = MP_JNI_CALL_INT(p->audiotrack, AudioTrack.write, p->bytearray, 0, len);
        if (MP_JNI_EXCEPTION_LOG(ao) < 0) return -1;
    }

    return ret;
}

static void uninit_jni(struct ao *ao)
{
    JNIEnv *env = MP_JNI_GET_ENV(ao);
    mp_jni_reset_jfields(env, &AudioTrack, AudioTrack.mapping, 1, ao->log);
    mp_jni_reset_jfields(env, &AudioTimestamp, AudioTimestamp.mapping, 1, ao->log);
    mp_jni_reset_jfields(env, &AudioManager, AudioManager.mapping, 1, ao->log);
    mp_jni_reset_jfields(env, &AudioFormat, AudioFormat.mapping, 1, ao->log);
    mp_jni_reset_jfields(env, &AudioFormatBuilder, AudioFormatBuilder.mapping, 1, ao->log);
    mp_jni_reset_jfields(env, &AudioAttributes, AudioAttributes.mapping, 1, ao->log);
    mp_jni_reset_jfields(env, &AudioAttributesBuilder, AudioAttributesBuilder.mapping, 1, ao->log);
    mp_jni_reset_jfields(env, &ByteBuffer, ByteBuffer.mapping, 1, ao->log);
}

static int init_jni(struct ao *ao)
{
    JNIEnv *env = MP_JNI_GET_ENV(ao);
    if (mp_jni_init_jfields(env, &AudioTrack, AudioTrack.mapping, 1, ao->log) < 0 ||
        mp_jni_init_jfields(env, &ByteBuffer, ByteBuffer.mapping, 1, ao->log) < 0 ||
        mp_jni_init_jfields(env, &AudioTimestamp, AudioTimestamp.mapping, 1, ao->log) < 0 ||
        mp_jni_init_jfields(env, &AudioManager, AudioManager.mapping, 1, ao->log) < 0 ||
        mp_jni_init_jfields(env, &AudioAttributes, AudioAttributes.mapping, 1, ao->log) < 0 ||
        mp_jni_init_jfields(env, &AudioAttributesBuilder, AudioAttributesBuilder.mapping, 1, ao->log) < 0 ||
        mp_jni_init_jfields(env, &AudioFormatBuilder, AudioFormatBuilder.mapping, 1, ao->log) < 0 ||
        mp_jni_init_jfields(env, &AudioFormat, AudioFormat.mapping, 1, ao->log) < 0) {
            uninit_jni(ao);
            return -1;
    }

    return 0;
}

static MP_THREAD_VOID playthread(void *arg)
{
    struct ao *ao = arg;
    struct priv *p = ao->priv;
    JNIEnv *env = MP_JNI_GET_ENV(ao);
    mp_thread_set_name("ao/audiotrack");
    mp_mutex_lock(&p->lock);
    while (!p->thread_terminate) {
        int state = AudioTrack.PLAYSTATE_PAUSED;
        if (p->audiotrack) {
            state = MP_JNI_CALL_INT(p->audiotrack, AudioTrack.getPlayState);
        }
        if (state == AudioTrack.PLAYSTATE_PLAYING) {
            int read_samples = p->chunksize / ao->sstride;
            int64_t ts = mp_time_ns();
            ts += MP_TIME_S_TO_NS(read_samples / (double)(ao->samplerate));
            ts += MP_TIME_S_TO_NS(AudioTrack_getLatency(ao));
            int samples = ao_read_data_nonblocking(ao, &p->chunk, read_samples, ts);
            int ret = AudioTrack_write(ao, samples * ao->sstride);
            if (ret >= 0) {
                p->written_frames += ret / ao->sstride;
            } else if (ret == AudioManager.ERROR_DEAD_OBJECT) {
                MP_WARN(ao, "AudioTrack.write failed with ERROR_DEAD_OBJECT. Recreating AudioTrack...\n");
                if (AudioTrack_Recreate(ao) < 0) {
                    MP_ERR(ao, "AudioTrack_Recreate failed\n");
                }
            } else {
                MP_ERR(ao, "AudioTrack.write failed with %d\n", ret);
            }
        } else {
            mp_cond_timedwait(&p->wakeup, &p->lock, MP_TIME_MS_TO_NS(300));
        }
    }
    mp_mutex_unlock(&p->lock);
    MP_THREAD_RETURN();
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;
    JNIEnv *env = MP_JNI_GET_ENV(ao);
    if (p->audiotrack) {
        MP_JNI_CALL_VOID(p->audiotrack, AudioTrack.stop);
        MP_JNI_EXCEPTION_LOG(ao);
        MP_JNI_CALL_VOID(p->audiotrack, AudioTrack.flush);
        MP_JNI_EXCEPTION_LOG(ao);
    }

    mp_mutex_lock(&p->lock);
    p->thread_terminate = true;
    mp_cond_signal(&p->wakeup);
    mp_mutex_unlock(&p->lock);

    if (p->thread_created)
        mp_thread_join(p->thread);

    if (p->audiotrack) {
        MP_JNI_CALL_VOID(p->audiotrack, AudioTrack.release);
        MP_JNI_EXCEPTION_LOG(ao);
        (*env)->DeleteGlobalRef(env, p->audiotrack);
        p->audiotrack = NULL;
    }

    if (p->bytearray) {
        (*env)->DeleteGlobalRef(env, p->bytearray);
        p->bytearray = NULL;
    }

    if (p->shortarray) {
        (*env)->DeleteGlobalRef(env, p->shortarray);
        p->shortarray = NULL;
    }

    if (p->floatarray) {
        (*env)->DeleteGlobalRef(env, p->floatarray);
        p->floatarray = NULL;
    }

    if (p->bbuf) {
        (*env)->DeleteGlobalRef(env, p->bbuf);
        p->bbuf = NULL;
    }

    if (p->timestamp) {
        (*env)->DeleteGlobalRef(env, p->timestamp);
        p->timestamp = NULL;
    }

    mp_cond_destroy(&p->wakeup);
    mp_mutex_destroy(&p->lock);

    uninit_jni(ao);
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    JNIEnv *env = MP_JNI_GET_ENV(ao);
    if (!env)
        return -1;

    mp_mutex_init(&p->lock);
    mp_cond_init(&p->wakeup);

    if (init_jni(ao) < 0)
        return -1;

    if (af_fmt_is_spdif(ao->format)) {
        p->format = AudioFormat.ENCODING_IEC61937;
        if (!p->format || !AudioTrack.writeShortV23) {
            MP_ERR(ao, "spdif passthrough not supported by API\n");
            return -1;
        }
    } else if (ao->format == AF_FORMAT_U8) {
        p->format = AudioFormat.ENCODING_PCM_8BIT;
    } else if (p->cfg_pcm_float && af_fmt_is_float(ao->format)) {
        ao->format = AF_FORMAT_FLOAT;
        p->format = AudioFormat.ENCODING_PCM_FLOAT;
    } else {
        ao->format = AF_FORMAT_S16;
        p->format = AudioFormat.ENCODING_PCM_16BIT;
    }

    if (AudioTrack.getNativeOutputSampleRate) {
        jint samplerate = MP_JNI_CALL_STATIC_INT(
            AudioTrack.clazz,
            AudioTrack.getNativeOutputSampleRate,
            AudioManager.STREAM_MUSIC
        );
        if (MP_JNI_EXCEPTION_LOG(ao) == 0) {
            MP_VERBOSE(ao, "AudioTrack.nativeOutputSampleRate = %d\n", samplerate);
            ao->samplerate = MPMIN(samplerate, ao->samplerate);
        }
    }
    p->samplerate = ao->samplerate;

    /* https://developer.android.com/reference/android/media/AudioFormat#channelPositionMask */
    static const struct mp_chmap layouts[] = {
        {0},                                        // empty
        MP_CHMAP_INIT_MONO,                         // mono
        MP_CHMAP_INIT_STEREO,                       // stereo
        MP_CHMAP3(FL, FR, FC),                      // 3.0
        MP_CHMAP4(FL, FR, BL, BR),                  // quad
        MP_CHMAP5(FL, FR, FC, BL, BR),              // 5.0
        MP_CHMAP6(FL, FR, FC, LFE, BL, BR),         // 5.1
        MP_CHMAP7(FL, FR, FC, LFE, BL, BR, BC),     // 6.1
        MP_CHMAP8(FL, FR, FC, LFE, BL, BR, SL, SR), // 7.1
    };
    const jint layout_map[] = {
        0,
        AudioFormat.CHANNEL_OUT_MONO,
        AudioFormat.CHANNEL_OUT_STEREO,
        AudioFormat.CHANNEL_OUT_STEREO | AudioFormat.CHANNEL_OUT_FRONT_CENTER,
        AudioFormat.CHANNEL_OUT_QUAD,
        AudioFormat.CHANNEL_OUT_QUAD | AudioFormat.CHANNEL_OUT_FRONT_CENTER,
        AudioFormat.CHANNEL_OUT_5POINT1,
        AudioFormat.CHANNEL_OUT_5POINT1 | AudioFormat.CHANNEL_OUT_BACK_CENTER,
        AudioFormat.CHANNEL_OUT_7POINT1_SURROUND,
    };
    static_assert(MP_ARRAY_SIZE(layout_map) == MP_ARRAY_SIZE(layouts), "");
    if (p->format == AudioFormat.ENCODING_IEC61937) {
        p->channel_config = AudioFormat.CHANNEL_OUT_STEREO;
    } else {
        struct mp_chmap_sel sel = {0};
        for (int i = 0; i < MP_ARRAY_SIZE(layouts); i++) {
            if (layout_map[i])
                mp_chmap_sel_add_map(&sel, &layouts[i]);
        }
        if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
            goto error;
        p->channel_config = layout_map[ao->channels.num];
        assert(p->channel_config);
    }

    jint buffer_size = MP_JNI_CALL_STATIC_INT(
        AudioTrack.clazz,
        AudioTrack.getMinBufferSize,
        p->samplerate,
        p->channel_config,
        p->format
    );
    if (MP_JNI_EXCEPTION_LOG(ao) < 0 || buffer_size <= 0) {
        MP_FATAL(ao, "AudioTrack.getMinBufferSize returned an invalid size: %d", buffer_size);
        return -1;
    }

    // Choose double of the minimum buffer size suggested by the driver, but not
    // less than 75ms or more than 150ms.
    const int bps = af_fmt_to_bytes(ao->format);
    int min = 0.075 * p->samplerate * bps * ao->channels.num;
    int max = min * 2;
    min = MP_ALIGN_UP(min, bps);
    max = MP_ALIGN_UP(max, bps);
    p->size = MPCLAMP(buffer_size * 2, min, max);
    MP_VERBOSE(ao, "Setting bufferSize = %d (driver=%d, min=%d, max=%d)\n", p->size, buffer_size, min, max);
    assert(p->size % bps == 0);
    ao->device_buffer = p->size / bps;

    p->chunksize = p->size;
    p->chunk = talloc_size(ao, p->size);

    jobject timestamp = MP_JNI_NEW(AudioTimestamp.clazz, AudioTimestamp.ctor);
    if (MP_JNI_EXCEPTION_LOG(ao) < 0 || !timestamp) {
        MP_FATAL(ao, "AudioTimestamp could not be created\n");
        return -1;
    }
    p->timestamp = (*env)->NewGlobalRef(env, timestamp);
    (*env)->DeleteLocalRef(env, timestamp);

    // decide and create buffer of right type
    if (p->format == AudioFormat.ENCODING_IEC61937) {
        jshortArray shortarray = (*env)->NewShortArray(env, p->chunksize / 2);
        p->shortarray = (*env)->NewGlobalRef(env, shortarray);
        (*env)->DeleteLocalRef(env, shortarray);
    } else if (AudioTrack.writeBufferV21) {
        MP_VERBOSE(ao, "Using NIO ByteBuffer\n");
        jobject bbuf = (*env)->NewDirectByteBuffer(env, p->chunk, p->chunksize);
        p->bbuf = (*env)->NewGlobalRef(env, bbuf);
        (*env)->DeleteLocalRef(env, bbuf);
    } else if (p->format == AudioFormat.ENCODING_PCM_FLOAT) {
        jfloatArray floatarray = (*env)->NewFloatArray(env, p->chunksize / sizeof(float));
        p->floatarray = (*env)->NewGlobalRef(env, floatarray);
        (*env)->DeleteLocalRef(env, floatarray);
    } else {
        jbyteArray bytearray = (*env)->NewByteArray(env, p->chunksize);
        p->bytearray = (*env)->NewGlobalRef(env, bytearray);
        (*env)->DeleteLocalRef(env, bytearray);
    }

    /* create AudioTrack object */
    if (AudioTrack_New(ao) != 0) {
        MP_FATAL(ao, "Failed to create AudioTrack\n");
        goto error;
    }

    if (mp_thread_create(&p->thread, playthread, ao)) {
        MP_ERR(ao, "pthread creation failed\n");
        goto error;
    }
    p->thread_created = true;

    return 1;

error:
    uninit(ao);
    return -1;
}

static void stop(struct ao *ao)
{
    struct priv *p = ao->priv;
    if (!p->audiotrack) {
        MP_ERR(ao, "AudioTrack does not exist to stop!\n");
        return;
    }

    JNIEnv *env = MP_JNI_GET_ENV(ao);
    MP_JNI_CALL_VOID(p->audiotrack, AudioTrack.pause);
    MP_JNI_EXCEPTION_LOG(ao);
    MP_JNI_CALL_VOID(p->audiotrack, AudioTrack.flush);
    MP_JNI_EXCEPTION_LOG(ao);

    p->playhead_offset = 0;
    p->reset_pending = true;
    p->written_frames = 0;
    p->timestamp_fetched = 0;
    p->timestamp_set = false;
    p->timestamp_offset = 0;
}

static void start(struct ao *ao)
{
    struct priv *p = ao->priv;
    if (!p->audiotrack) {
        MP_ERR(ao, "AudioTrack does not exist to start!\n");
        return;
    }

    JNIEnv *env = MP_JNI_GET_ENV(ao);
    MP_JNI_CALL_VOID(p->audiotrack, AudioTrack.play);
    MP_JNI_EXCEPTION_LOG(ao);

    mp_cond_signal(&p->wakeup);
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_audiotrack = {
    .description = "Android AudioTrack audio output",
    .name      = "audiotrack",
    .init      = init,
    .uninit    = uninit,
    .reset     = stop,
    .start     = start,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const OPT_BASE_STRUCT) {
        .cfg_pcm_float = 1,
    },
    .options   = (const struct m_option[]) {
        {"pcm-float", OPT_BOOL(cfg_pcm_float)},
        {"session-id", OPT_INT(cfg_session_id)},
        {0}
    },
    .options_prefix = "audiotrack",
};
