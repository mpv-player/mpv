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

#include <libavutil/frame.h>
#include <libavutil/mem.h>

#include "common/common.h"

#include "chmap.h"
#include "fmt-conversion.h"
#include "format.h"
#include "aframe.h"

struct mp_aframe {
    AVFrame *av_frame;
    // We support channel layouts different from AVFrame channel masks
    struct mp_chmap chmap;
    // We support spdif formats, which are allocated as AV_SAMPLE_FMT_S16.
    int format;
    double pts;
    double speed;
};

struct avframe_opaque {
    double speed;
};

static void free_frame(void *ptr)
{
    struct mp_aframe *frame = ptr;
    av_frame_free(&frame->av_frame);
}

struct mp_aframe *mp_aframe_create(void)
{
    struct mp_aframe *frame = talloc_zero(NULL, struct mp_aframe);
    frame->av_frame = av_frame_alloc();
    if (!frame->av_frame)
        abort();
    talloc_set_destructor(frame, free_frame);
    mp_aframe_reset(frame);
    return frame;
}

struct mp_aframe *mp_aframe_new_ref(struct mp_aframe *frame)
{
    if (!frame)
        return NULL;

    struct mp_aframe *dst = mp_aframe_create();

    dst->chmap = frame->chmap;
    dst->format = frame->format;
    dst->pts = frame->pts;
    dst->speed = frame->speed;

    if (mp_aframe_is_allocated(frame)) {
        if (av_frame_ref(dst->av_frame, frame->av_frame) < 0)
            abort();
    } else {
        // av_frame_ref() would fail.
        mp_aframe_config_copy(dst, frame);
    }

    return dst;
}

// Revert to state after mp_aframe_create().
void mp_aframe_reset(struct mp_aframe *frame)
{
    av_frame_unref(frame->av_frame);
    frame->chmap.num = 0;
    frame->format = 0;
    frame->pts = MP_NOPTS_VALUE;
    frame->speed = 1.0;
}

// Remove all actual audio data and leave only the metadata.
void mp_aframe_unref_data(struct mp_aframe *frame)
{
    // In a fucked up way, this is less complex than just unreffing the data.
    struct mp_aframe *tmp = mp_aframe_create();
    MPSWAP(struct mp_aframe, *tmp, *frame);
    mp_aframe_reset(frame);
    mp_aframe_config_copy(frame, tmp);
    talloc_free(tmp);
}

// Return a new reference to the data in av_frame. av_frame itself is not
// touched. Returns NULL if not representable, or if input is NULL.
// Does not copy the timestamps.
struct mp_aframe *mp_aframe_from_avframe(struct AVFrame *av_frame)
{
    if (!av_frame || av_frame->width > 0 || av_frame->height > 0)
        return NULL;

    int format = af_from_avformat(av_frame->format);
    if (!format && av_frame->format != AV_SAMPLE_FMT_NONE)
        return NULL;

    struct mp_aframe *frame = mp_aframe_create();

    // This also takes care of forcing refcounting.
    if (av_frame_ref(frame->av_frame, av_frame) < 0)
        abort();

    frame->format = format;
    mp_chmap_from_lavc(&frame->chmap, frame->av_frame->channel_layout);

#if LIBAVUTIL_VERSION_MICRO >= 100
    // FFmpeg being a stupid POS again
    if (frame->chmap.num != frame->av_frame->channels)
        mp_chmap_from_channels(&frame->chmap, av_frame->channels);
#endif

    if (av_frame->opaque_ref) {
        struct avframe_opaque *op = (void *)av_frame->opaque_ref->data;
        frame->speed = op->speed;
    }

    return frame;
}

// Return a new reference to the data in frame. Returns NULL is not
// representable (), or if input is NULL.
// Does not copy the timestamps.
struct AVFrame *mp_aframe_to_avframe(struct mp_aframe *frame)
{
    if (!frame)
        return NULL;

    if (af_to_avformat(frame->format) != frame->av_frame->format)
        return NULL;

    if (!mp_chmap_is_lavc(&frame->chmap))
        return NULL;

    if (!frame->av_frame->opaque_ref && frame->speed != 1.0) {
        frame->av_frame->opaque_ref =
            av_buffer_alloc(sizeof(struct avframe_opaque));
        if (!frame->av_frame->opaque_ref)
            return NULL;

        struct avframe_opaque *op = (void *)frame->av_frame->opaque_ref->data;
        op->speed = frame->speed;
    }

    return av_frame_clone(frame->av_frame);
}

struct AVFrame *mp_aframe_to_avframe_and_unref(struct mp_aframe *frame)
{
    AVFrame *av = mp_aframe_to_avframe(frame);
    talloc_free(frame);
    return av;
}

// You must not use this.
struct AVFrame *mp_aframe_get_raw_avframe(struct mp_aframe *frame)
{
    return frame->av_frame;
}

// Return whether it has associated audio data. (If not, metadata only.)
bool mp_aframe_is_allocated(struct mp_aframe *frame)
{
    return frame->av_frame->buf[0] || frame->av_frame->extended_data[0];
}

// Clear dst, and then copy the configuration to it.
void mp_aframe_config_copy(struct mp_aframe *dst, struct mp_aframe *src)
{
    mp_aframe_reset(dst);

    dst->chmap = src->chmap;
    dst->format = src->format;

    mp_aframe_copy_attributes(dst, src);

    dst->av_frame->sample_rate = src->av_frame->sample_rate;
    dst->av_frame->format = src->av_frame->format;
    dst->av_frame->channel_layout = src->av_frame->channel_layout;
#if LIBAVUTIL_VERSION_MICRO >= 100
    // FFmpeg being a stupid POS again
    dst->av_frame->channels = src->av_frame->channels;
#endif
}

// Copy "soft" attributes from src to dst, excluding things which affect
// frame allocation and organization.
void mp_aframe_copy_attributes(struct mp_aframe *dst, struct mp_aframe *src)
{
    dst->pts = src->pts;
    dst->speed = src->speed;

    int rate = dst->av_frame->sample_rate;

    if (av_frame_copy_props(dst->av_frame, src->av_frame) < 0)
        abort();

    dst->av_frame->sample_rate = rate;
}

// Return whether a and b use the same physical audio format. Extra metadata
// such as PTS, per-frame signalling, and AVFrame side data is not compared.
bool mp_aframe_config_equals(struct mp_aframe *a, struct mp_aframe *b)
{
    struct mp_chmap ca = {0}, cb = {0};
    mp_aframe_get_chmap(a, &ca);
    mp_aframe_get_chmap(b, &cb);
    return mp_chmap_equals(&ca, &cb) &&
           mp_aframe_get_rate(a) == mp_aframe_get_rate(b) &&
           mp_aframe_get_format(a) == mp_aframe_get_format(b);
}

// Return whether all required format fields have been set.
bool mp_aframe_config_is_valid(struct mp_aframe *frame)
{
    return frame->format && frame->chmap.num && frame->av_frame->sample_rate;
}

// Return the pointer to the first sample for each plane. The pointers stay
// valid until the next call that mutates frame somehow. You must not write to
// the audio data. Returns NULL if no frame allocated.
uint8_t **mp_aframe_get_data_ro(struct mp_aframe *frame)
{
    return mp_aframe_is_allocated(frame) ? frame->av_frame->extended_data : NULL;
}

// Like mp_aframe_get_data_ro(), but you can write to the audio data.
// Additionally, it will return NULL if copy-on-write fails.
uint8_t **mp_aframe_get_data_rw(struct mp_aframe *frame)
{
    if (!mp_aframe_is_allocated(frame))
        return NULL;
    if (av_frame_make_writable(frame->av_frame) < 0)
        return NULL;
    return frame->av_frame->extended_data;
}

int mp_aframe_get_format(struct mp_aframe *frame)
{
    return frame->format;
}

bool mp_aframe_get_chmap(struct mp_aframe *frame, struct mp_chmap *out)
{
    if (!mp_chmap_is_valid(&frame->chmap))
        return false;
    *out = frame->chmap;
    return true;
}

int mp_aframe_get_channels(struct mp_aframe *frame)
{
    return frame->chmap.num;
}

int mp_aframe_get_rate(struct mp_aframe *frame)
{
    return frame->av_frame->sample_rate;
}

int mp_aframe_get_size(struct mp_aframe *frame)
{
    return frame->av_frame->nb_samples;
}

double mp_aframe_get_pts(struct mp_aframe *frame)
{
    return frame->pts;
}

bool mp_aframe_set_format(struct mp_aframe *frame, int format)
{
    if (mp_aframe_is_allocated(frame))
        return false;
    enum AVSampleFormat av_format = af_to_avformat(format);
    if (av_format == AV_SAMPLE_FMT_NONE && format) {
        if (!af_fmt_is_spdif(format))
            return false;
        av_format = AV_SAMPLE_FMT_S16;
    }
    frame->format = format;
    frame->av_frame->format = av_format;
    return true;
}

bool mp_aframe_set_chmap(struct mp_aframe *frame, struct mp_chmap *in)
{
    if (!mp_chmap_is_valid(in) && !mp_chmap_is_empty(in))
        return false;
    if (mp_aframe_is_allocated(frame) && in->num != frame->chmap.num)
        return false;
    uint64_t lavc_layout = mp_chmap_to_lavc_unchecked(in);
    if (!lavc_layout && in->num)
        return false;
    frame->chmap = *in;
    frame->av_frame->channel_layout = lavc_layout;
#if LIBAVUTIL_VERSION_MICRO >= 100
    // FFmpeg being a stupid POS again
    frame->av_frame->channels = frame->chmap.num;
#endif
    return true;
}

bool mp_aframe_set_rate(struct mp_aframe *frame, int rate)
{
    if (rate < 1 || rate > 10000000)
        return false;
    frame->av_frame->sample_rate = rate;
    return true;
}

bool mp_aframe_set_size(struct mp_aframe *frame, int samples)
{
    if (!mp_aframe_is_allocated(frame) || mp_aframe_get_size(frame) < samples)
        return false;
    frame->av_frame->nb_samples = MPMAX(samples, 0);
    return true;
}

void mp_aframe_set_pts(struct mp_aframe *frame, double pts)
{
    frame->pts = pts;
}

// Set a speed factor. This is multiplied with the sample rate to get the
// "effective" samplerate (mp_aframe_get_effective_rate()), which will be used
// to do PTS calculations. If speed!=1.0, the PTS values always refer to the
// original PTS (before changing speed), and if you want reasonably continuous
// PTS between frames, you need to use the effective samplerate.
void mp_aframe_set_speed(struct mp_aframe *frame, double factor)
{
    frame->speed = factor;
}

// Adjust current speed factor.
void mp_aframe_mul_speed(struct mp_aframe *frame, double factor)
{
    frame->speed *= factor;
}

double mp_aframe_get_speed(struct mp_aframe *frame)
{
    return frame->speed;
}

// Matters for speed changed frames (such as a frame which has been resampled
// to play at a different speed).
// Return the sample rate at which the frame would have to be played to result
// in the same duration as the original frame before the speed change.
// This is used for A/V sync.
double mp_aframe_get_effective_rate(struct mp_aframe *frame)
{
    return mp_aframe_get_rate(frame) / frame->speed;
}

// Return number of data pointers.
int mp_aframe_get_planes(struct mp_aframe *frame)
{
    return af_fmt_is_planar(mp_aframe_get_format(frame))
           ? mp_aframe_get_channels(frame) : 1;
}

// Return number of bytes between 2 consecutive samples on the same plane.
size_t mp_aframe_get_sstride(struct mp_aframe *frame)
{
    int format = mp_aframe_get_format(frame);
    return af_fmt_to_bytes(format) *
           (af_fmt_is_planar(format) ? 1 : mp_aframe_get_channels(frame));
}

// Return total number of samples on each plane.
int mp_aframe_get_total_plane_samples(struct mp_aframe *frame)
{
    return frame->av_frame->nb_samples *
           (af_fmt_is_planar(mp_aframe_get_format(frame))
            ? 1 : mp_aframe_get_channels(frame));
}

char *mp_aframe_format_str_buf(char *buf, size_t buf_size, struct mp_aframe *fmt)
{
    char ch[128];
    mp_chmap_to_str_buf(ch, sizeof(ch), &fmt->chmap);
    char *hr_ch = mp_chmap_to_str_hr(&fmt->chmap);
    if (strcmp(hr_ch, ch) != 0)
        mp_snprintf_cat(ch, sizeof(ch), " (%s)", hr_ch);
    snprintf(buf, buf_size, "%dHz %s %dch %s", fmt->av_frame->sample_rate,
             ch, fmt->chmap.num, af_fmt_to_str(fmt->format));
    return buf;
}

// Set data to the audio after the given number of samples (i.e. slice it).
void mp_aframe_skip_samples(struct mp_aframe *f, int samples)
{
    assert(samples >= 0 && samples <= mp_aframe_get_size(f));

    int num_planes = mp_aframe_get_planes(f);
    size_t sstride = mp_aframe_get_sstride(f);
    for (int n = 0; n < num_planes; n++)
        f->av_frame->extended_data[n] += samples * sstride;

    f->av_frame->nb_samples -= samples;

    if (f->pts != MP_NOPTS_VALUE)
        f->pts += samples / mp_aframe_get_effective_rate(f);
}

// Return the timestamp of the sample just after the end of this frame.
double mp_aframe_end_pts(struct mp_aframe *f)
{
    double rate = mp_aframe_get_effective_rate(f);
    if (f->pts == MP_NOPTS_VALUE || rate <= 0)
        return MP_NOPTS_VALUE;
    return f->pts + f->av_frame->nb_samples / rate;
}

// Return the duration in seconds of the frame (0 if invalid).
double mp_aframe_duration(struct mp_aframe *f)
{
    double rate = mp_aframe_get_effective_rate(f);
    if (rate <= 0)
        return 0;
    return f->av_frame->nb_samples / rate;
}

// Clip the given frame to the given timestamp range. Adjusts the frame size
// and timestamp.
// Refuses to change spdif frames.
void mp_aframe_clip_timestamps(struct mp_aframe *f, double start, double end)
{
    double f_end = mp_aframe_end_pts(f);
    double rate = mp_aframe_get_effective_rate(f);
    if (f_end == MP_NOPTS_VALUE)
        return;
    if (af_fmt_is_spdif(mp_aframe_get_format(f)))
        return;
    if (end != MP_NOPTS_VALUE) {
        if (f_end >= end) {
            if (f->pts >= end) {
                f->av_frame->nb_samples = 0;
            } else {
                int new = (end - f->pts) * rate;
                f->av_frame->nb_samples = MPCLAMP(new, 0, f->av_frame->nb_samples);
            }
        }
    }
    if (start != MP_NOPTS_VALUE) {
        if (f->pts < start) {
            if (f_end <= start) {
                f->av_frame->nb_samples = 0;
                f->pts = f_end;
            } else {
                int skip = (start - f->pts) * rate;
                skip = MPCLAMP(skip, 0, f->av_frame->nb_samples);
                mp_aframe_skip_samples(f, skip);
            }
        }
    }
}

bool mp_aframe_copy_samples(struct mp_aframe *dst, int dst_offset,
                            struct mp_aframe *src, int src_offset,
                            int samples)
{
    if (!mp_aframe_config_equals(dst, src))
        return false;

    if (mp_aframe_get_size(dst) < dst_offset + samples ||
        mp_aframe_get_size(src) < src_offset + samples)
        return false;

    uint8_t **s = mp_aframe_get_data_ro(src);
    uint8_t **d = mp_aframe_get_data_rw(dst);
    if (!s || !d)
        return false;

    int planes = mp_aframe_get_planes(dst);
    size_t sstride = mp_aframe_get_sstride(dst);

    for (int n = 0; n < planes; n++) {
        memcpy(d[n] + dst_offset * sstride, s[n] + src_offset * sstride,
               samples * sstride);
    }

    return true;
}

bool mp_aframe_set_silence(struct mp_aframe *f, int offset, int samples)
{
    if (mp_aframe_get_size(f) < offset + samples)
        return false;

    int format = mp_aframe_get_format(f);
    uint8_t **d = mp_aframe_get_data_rw(f);
    if (!d)
        return false;

    int planes = mp_aframe_get_planes(f);
    size_t sstride = mp_aframe_get_sstride(f);

    for (int n = 0; n < planes; n++)
        af_fill_silence(d[n] + offset * sstride, samples * sstride, format);

    return true;
}

bool mp_aframe_reverse(struct mp_aframe *f)
{
    int format = mp_aframe_get_format(f);
    size_t bps = af_fmt_to_bytes(format);
    if (!af_fmt_is_pcm(format) || bps > 16)
        return false;

    uint8_t **d = mp_aframe_get_data_rw(f);
    if (!d)
        return false;

    int planes = mp_aframe_get_planes(f);
    int samples = mp_aframe_get_size(f);
    int channels = mp_aframe_get_channels(f);
    size_t sstride = mp_aframe_get_sstride(f);

    int plane_samples = channels;
    if (af_fmt_is_planar(format))
        plane_samples = 1;

    for (int p = 0; p < planes; p++) {
        for (int n = 0; n < samples / 2; n++) {
            int s1_offset = n * sstride;
            int s2_offset = (samples - 1 - n) * sstride;
            for (int c = 0; c < plane_samples; c++) {
                // Nobody said it'd be fast.
                char tmp[16];
                uint8_t *s1 = d[p] + s1_offset + c * bps;
                uint8_t *s2 = d[p] + s2_offset + c * bps;
                memcpy(tmp, s2, bps);
                memcpy(s2, s1, bps);
                memcpy(s1, tmp, bps);
            }
        }
    }

    return true;
}

int mp_aframe_approx_byte_size(struct mp_aframe *frame)
{
    // God damn, AVFrame is too fucking annoying. Just go with the size that
    // allocating a new frame would use.
    int planes = mp_aframe_get_planes(frame);
    size_t sstride = mp_aframe_get_sstride(frame);
    int samples = frame->av_frame->nb_samples;
    int plane_size = MP_ALIGN_UP(sstride * MPMAX(samples, 1), 32);
    return plane_size * planes + sizeof(*frame);
}

struct mp_aframe_pool {
    AVBufferPool *avpool;
    int element_size;
};

struct mp_aframe_pool *mp_aframe_pool_create(void *ta_parent)
{
    return talloc_zero(ta_parent, struct mp_aframe_pool);
}

static void mp_aframe_pool_destructor(void *p)
{
    struct mp_aframe_pool *pool = p;
    av_buffer_pool_uninit(&pool->avpool);
}

// Like mp_aframe_allocate(), but use the pool to allocate data.
int mp_aframe_pool_allocate(struct mp_aframe_pool *pool, struct mp_aframe *frame,
                            int samples)
{
    int planes = mp_aframe_get_planes(frame);
    size_t sstride = mp_aframe_get_sstride(frame);
    int plane_size = MP_ALIGN_UP(sstride * MPMAX(samples, 1), 32);
    int size = plane_size * planes;

    if (size <= 0 || mp_aframe_is_allocated(frame))
        return -1;

    if (!pool->avpool || size > pool->element_size) {
        size_t alloc = ta_calc_prealloc_elems(size);
        if (alloc >= INT_MAX)
            return -1;
        av_buffer_pool_uninit(&pool->avpool);
        pool->element_size = alloc;
        pool->avpool = av_buffer_pool_init(pool->element_size, NULL);
        if (!pool->avpool)
            return -1;
        talloc_set_destructor(pool, mp_aframe_pool_destructor);
    }

    // Yes, you have to do all this shit manually.
    // At least it's less stupid than av_frame_get_buffer(), which just wipes
    // the entire frame struct on error for no reason.
    AVFrame *av_frame = frame->av_frame;
    if (av_frame->extended_data != av_frame->data)
        av_freep(&av_frame->extended_data); // sigh
    av_frame->extended_data =
        av_mallocz_array(planes, sizeof(av_frame->extended_data[0]));
    if (!av_frame->extended_data)
        abort();
    av_frame->buf[0] = av_buffer_pool_get(pool->avpool);
    if (!av_frame->buf[0])
        return -1;
    av_frame->linesize[0] = samples * sstride;
    for (int n = 0; n < planes; n++)
        av_frame->extended_data[n] = av_frame->buf[0]->data + n * plane_size;
    av_frame->nb_samples = samples;

    return 0;
}
