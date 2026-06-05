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

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>

#include "waveform_scanner.h"
#include "waveform_cache.h"
#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "misc/dispatch.h"
#include "osdep/threads.h"
#include "mpv_talloc.h"

// ── Queue entry ──────────────────────────────────────────────────────────────

struct scan_entry {
    char             *file_path;     // talloc'd under the entry
    int               fps;
    int               priority;
    waveform_scan_cb  callback;
    void             *callback_ctx;
    uint64_t          file_hash;     // pre-computed on enqueue
    atomic_bool       cancelled;
    struct scan_entry *next;         // intrusive linked list
};

// ── Active scan tracking (for progress + cancellation) ───────────────────────

struct active_scan {
    char        *file_path;   // talloc'd
    int          fps;
    atomic_int   progress_pct; // 0-100; -1 = not started
};

// ── Scanner ───────────────────────────────────────────────────────────────────

struct waveform_scanner {
    struct mp_log            *log;
    struct mpv_global        *global;
    struct mp_dispatch_queue *dispatch;

    // Queue (protected by queue_lock)
    mp_mutex      queue_lock;
    mp_cond       queue_cond;
    struct scan_entry *queue_head;  // sorted: higher priority first
    int           queue_len;

    // Active scans
    mp_mutex         active_lock;
    struct active_scan **active;    // array, length = num_threads
    int               num_active;

    // Worker threads
    mp_thread    *threads;
    int           num_threads;
    atomic_bool   shutdown;
};

// ── Callback dispatch ─────────────────────────────────────────────────────────

struct callback_payload {
    waveform_scan_cb  callback;
    void             *ctx;
    struct waveform_data *result;
    char             *error;  // static string or NULL
};

static void dispatch_callback(void *parg)
{
    struct callback_payload *p = parg;
    p->callback(p->ctx, p->result, p->error);
    talloc_free(p);
}

static void fire_callback(struct waveform_scanner *ws,
                          waveform_scan_cb cb, void *ctx,
                          struct waveform_data *result, const char *error)
{
    struct callback_payload *p = talloc_zero(NULL, struct callback_payload);
    p->callback = cb;
    p->ctx      = ctx;
    p->result   = result;
    p->error    = error ? talloc_strdup(p, error) : NULL;
    mp_dispatch_enqueue(ws->dispatch, dispatch_callback, p);
}

// ── RMS calculation ───────────────────────────────────────────────────────────

static float calc_frame_rms(AVFrame *frame)
{
    double sum = 0.0;
    int    channels = frame->ch_layout.nb_channels;
    int    samples  = frame->nb_samples;

    if (channels <= 0 || samples <= 0)
        return 0.0f;

    // Planar float (most common after resampling)
    if (frame->format == AV_SAMPLE_FMT_FLTP) {
        for (int ch = 0; ch < channels; ch++) {
            float *data = (float *)frame->data[ch];
            for (int i = 0; i < samples; i++)
                sum += data[i] * data[i];
        }
    }
    // Packed float
    else if (frame->format == AV_SAMPLE_FMT_FLT) {
        float *data = (float *)frame->data[0];
        for (int i = 0; i < samples * channels; i++)
            sum += data[i] * data[i];
    }
    // Planar int16
    else if (frame->format == AV_SAMPLE_FMT_S16P) {
        for (int ch = 0; ch < channels; ch++) {
            int16_t *data = (int16_t *)frame->data[ch];
            for (int i = 0; i < samples; i++) {
                double v = data[i] / 32768.0;
                sum += v * v;
            }
        }
    }
    // Packed int16
    else if (frame->format == AV_SAMPLE_FMT_S16) {
        int16_t *data = (int16_t *)frame->data[0];
        for (int i = 0; i < samples * channels; i++) {
            double v = data[i] / 32768.0;
            sum += v * v;
        }
    }
    // Packed int32
    else if (frame->format == AV_SAMPLE_FMT_S32) {
        int32_t *data = (int32_t *)frame->data[0];
        for (int i = 0; i < samples * channels; i++) {
            double v = data[i] / 2147483648.0;
            sum += v * v;
        }
    }
    // Unsupported — return 0 (will be filtered as silence)
    else {
        return 0.0f;
    }

    return (float)sqrt(sum / (samples * channels));
}

// ── Audio scanning via libavcodec ─────────────────────────────────────────────

static struct waveform_data *scan_audio_file(struct waveform_scanner *ws,
                                              struct scan_entry *entry)
{
    const char *path = entry->file_path;
    int fps = entry->fps;
    double sample_interval = 1.0 / fps;

    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext  *dec_ctx = NULL;
    AVPacket        *pkt     = NULL;
    AVFrame         *frame   = NULL;
    struct waveform_data *result = NULL;

    if (avformat_open_input(&fmt_ctx, path, NULL, NULL) < 0) {
        mp_warn(ws->log, "waveform: cannot open '%s'\n", path);
        goto done;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        mp_warn(ws->log, "waveform: cannot read stream info for '%s'\n", path);
        goto done;
    }

    int audio_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO,
                                        -1, -1, NULL, 0);
    if (audio_idx < 0) {
        mp_warn(ws->log, "waveform: no audio stream in '%s'\n", path);
        goto done;
    }

    AVStream *stream = fmt_ctx->streams[audio_idx];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        mp_warn(ws->log, "waveform: no decoder for audio codec in '%s'\n", path);
        goto done;
    }

    dec_ctx = avcodec_alloc_context3(codec);
    if (!dec_ctx) goto done;

    avcodec_parameters_to_context(dec_ctx, stream->codecpar);
    if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
        mp_warn(ws->log, "waveform: codec open failed for '%s'\n", path);
        goto done;
    }

    pkt   = av_packet_alloc();
    frame = av_frame_alloc();
    if (!pkt || !frame) goto done;

    // Estimate capacity from duration
    double duration = fmt_ctx->duration > 0
        ? (double)fmt_ctx->duration / AV_TIME_BASE : 600.0;
    int cap = (int)(duration * fps) + 64;

    result = talloc_zero(NULL, struct waveform_data);
    result->samples = talloc_array(result, struct waveform_sample, cap);
    result->fps     = fps;

    double last_sample_pts = -1.0;
    double peak = 0.0;

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        // Check cancellation periodically
        if (atomic_load(&entry->cancelled)) {
            talloc_free(result);
            result = NULL;
            av_packet_unref(pkt);
            break;
        }

        if (pkt->stream_index != audio_idx) {
            av_packet_unref(pkt);
            continue;
        }

        avcodec_send_packet(dec_ctx, pkt);
        av_packet_unref(pkt);

        while (avcodec_receive_frame(dec_ctx, frame) >= 0) {
            double pts = frame->pts == AV_NOPTS_VALUE
                ? last_sample_pts + sample_interval
                : frame->pts * av_q2d(stream->time_base);

            if (pts < 0) pts = 0;

            if (pts - last_sample_pts >= sample_interval) {
                float rms = calc_frame_rms(frame);

                if (result->count >= cap) {
                    cap *= 2;
                    result->samples = talloc_realloc(result, result->samples,
                                                     struct waveform_sample, cap);
                }

                result->samples[result->count++] = (struct waveform_sample){
                    .time = (float)pts,
                    .rms  = rms,
                };
                if (rms > peak) peak = rms;
                last_sample_pts = pts;

                // Update progress (0-99; 100 set on normalise)
                // Phase 2 will wire this to a proper active_scan record
                (void)duration;  // suppress unused warning until Phase 2
            }
            av_frame_unref(frame);
        }
    }

    if (result) {
        // Normalise so peak == 1.0
        if (peak > 1e-9) {
            float scale = 1.0f / (float)peak;
            for (int i = 0; i < result->count; i++)
                result->samples[i].rms = fminf(result->samples[i].rms * scale, 1.0f);
        }
        result->duration = last_sample_pts > 0 ? last_sample_pts : duration;

        if (result->count < 10 || peak < 1e-9) {
            mp_verbose(ws->log, "waveform: scan produced no usable data for '%s'\n", path);
            talloc_free(result);
            result = NULL;
        } else {
            mp_verbose(ws->log, "waveform: scanned '%s' — %d samples @ %d fps\n",
                       path, result->count, fps);
        }
    }

done:
    if (frame)   av_frame_free(&frame);
    if (pkt)     av_packet_free(&pkt);
    if (dec_ctx) avcodec_free_context(&dec_ctx);
    if (fmt_ctx) avformat_close_input(&fmt_ctx);
    return result;
}

// ── Worker thread ─────────────────────────────────────────────────────────────

static MP_THREAD_VOID worker_thread(void *arg)
{
    struct waveform_scanner *ws = arg;
    mp_thread_set_name("waveform-scan");

    while (!atomic_load(&ws->shutdown)) {
        // Dequeue the highest-priority pending entry
        mp_mutex_lock(&ws->queue_lock);

        while (!ws->queue_head && !atomic_load(&ws->shutdown))
            mp_cond_wait(&ws->queue_cond, &ws->queue_lock);

        struct scan_entry *entry = ws->queue_head;
        if (entry)
            ws->queue_head = entry->next;

        mp_mutex_unlock(&ws->queue_lock);

        if (!entry || atomic_load(&ws->shutdown))
            break;

        if (atomic_load(&entry->cancelled)) {
            talloc_free(entry);
            continue;
        }

        mp_verbose(ws->log, "waveform: starting scan '%s' @ %d fps\n",
                   entry->file_path, entry->fps);

        // Check cache first
        struct waveform_data *cached =
            waveform_cache_load(ws->log, ws->global,
                                entry->file_path, entry->fps, entry->file_hash);
        if (cached) {
            mp_verbose(ws->log, "waveform: cache hit for '%s'\n", entry->file_path);
            if (entry->callback)
                fire_callback(ws, entry->callback, entry->callback_ctx, cached, NULL);
            else
                talloc_free(cached);
            talloc_free(entry);
            continue;
        }

        // Scan the file
        struct waveform_data *result = scan_audio_file(ws, entry);

        if (result && !atomic_load(&entry->cancelled)) {
            // Save to cache
            waveform_cache_save(ws->log, ws->global,
                                entry->file_path, result, entry->fps);
            if (entry->callback)
                fire_callback(ws, entry->callback, entry->callback_ctx, result, NULL);
            else
                talloc_free(result);
        } else if (!atomic_load(&entry->cancelled)) {
            if (entry->callback)
                fire_callback(ws, entry->callback, entry->callback_ctx,
                              NULL, "scan failed");
            if (result)
                talloc_free(result);
        }

        talloc_free(entry);
    }

    MP_THREAD_RETURN();
}

// ── Public API ────────────────────────────────────────────────────────────────

struct waveform_scanner *waveform_scanner_create(struct mp_log *log,
                                                 struct mpv_global *global,
                                                 struct mp_dispatch_queue *dispatch,
                                                 int max_threads)
{
    if (max_threads < 1) max_threads = 1;
    if (max_threads > 8) max_threads = 8;

    struct waveform_scanner *ws = talloc_zero(NULL, struct waveform_scanner);
    ws->log      = log;
    ws->global   = global;
    ws->dispatch = dispatch;
    ws->num_threads = max_threads;

    mp_mutex_init(&ws->queue_lock);
    mp_cond_init(&ws->queue_cond);
    mp_mutex_init(&ws->active_lock);
    atomic_init(&ws->shutdown, false);

    ws->threads = talloc_array(ws, mp_thread, max_threads);
    for (int i = 0; i < max_threads; i++) {
        if (mp_thread_create(&ws->threads[i], worker_thread, ws) != 0) {
            mp_err(log, "waveform: failed to create worker thread %d\n", i);
            ws->num_threads = i;
            break;
        }
    }

    mp_verbose(log, "waveform: scanner created with %d worker thread(s)\n",
               ws->num_threads);
    return ws;
}

void waveform_scanner_enqueue(struct waveform_scanner *scanner,
                              const char *file_path,
                              int target_fps,
                              int priority,
                              waveform_scan_cb callback,
                              void *callback_ctx)
{
    if (!scanner || !file_path || target_fps <= 0)
        return;
    // callback may be NULL — the scanner will still run and cache the result;
    // no notification will be sent to the caller.

    // Compute file hash up front (main thread, no lock needed)
    struct stat st;
    uint64_t hash = 0;
    if (stat(file_path, &st) == 0)
        hash = waveform_file_hash(file_path, (int64_t)st.st_size,
                                  (int64_t)st.st_mtime);

    struct scan_entry *entry = talloc_zero(NULL, struct scan_entry);
    entry->file_path    = talloc_strdup(entry, file_path);
    entry->fps          = target_fps;
    entry->priority     = priority;
    entry->callback     = callback;
    entry->callback_ctx = callback_ctx;
    entry->file_hash    = hash;
    atomic_init(&entry->cancelled, false);

    mp_mutex_lock(&scanner->queue_lock);

    // Insert sorted by priority descending
    struct scan_entry **pp = &scanner->queue_head;
    while (*pp && (*pp)->priority >= priority)
        pp = &(*pp)->next;
    entry->next = *pp;
    *pp = entry;
    scanner->queue_len++;

    mp_cond_signal(&scanner->queue_cond);
    mp_mutex_unlock(&scanner->queue_lock);

    mp_verbose(scanner->log,
               "waveform: enqueued '%s' @ %d fps (priority=%d, queue=%d)\n",
               file_path, target_fps, priority, scanner->queue_len);
}

void waveform_scanner_cancel_file(struct waveform_scanner *scanner,
                                  const char *file_path)
{
    if (!scanner || !file_path)
        return;

    mp_mutex_lock(&scanner->queue_lock);

    struct scan_entry *e = scanner->queue_head;
    while (e) {
        if (strcmp(e->file_path, file_path) == 0)
            atomic_store(&e->cancelled, true);
        e = e->next;
    }

    mp_mutex_unlock(&scanner->queue_lock);
}

double waveform_scanner_get_progress(struct waveform_scanner *scanner,
                                     const char *file_path,
                                     int fps)
{
    (void)scanner; (void)file_path; (void)fps;
    // Phase 2 will track per-scan progress; return -1 for now
    return -1.0;
}

void waveform_scanner_destroy(struct waveform_scanner *scanner)
{
    if (!scanner)
        return;

    // Signal all workers to exit
    atomic_store(&scanner->shutdown, true);

    mp_mutex_lock(&scanner->queue_lock);
    mp_cond_broadcast(&scanner->queue_cond);
    mp_mutex_unlock(&scanner->queue_lock);

    // Join all threads
    for (int i = 0; i < scanner->num_threads; i++)
        mp_thread_join(scanner->threads[i]);

    // Free remaining queue entries
    struct scan_entry *e = scanner->queue_head;
    while (e) {
        struct scan_entry *next = e->next;
        talloc_free(e);
        e = next;
    }

    mp_cond_destroy(&scanner->queue_cond);
    mp_mutex_destroy(&scanner->queue_lock);
    mp_mutex_destroy(&scanner->active_lock);

    mp_verbose(scanner->log, "waveform: scanner destroyed\n");
    talloc_free(scanner);
}
