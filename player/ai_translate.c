/*
 * AI translator: lookahead speech-to-text + translation into live subtitles.
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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "config.h"
#include "ai_translate.h"
#include "common/common.h"
#include "common/msg.h"
#include "mpv_talloc.h"
#include "options/m_option.h"

// Option group (always available, even without libcurl, so options.c links).
#define OPT_BASE_STRUCT struct ai_translate_opts
const struct m_sub_options ai_translate_conf = {
    .opts = (const struct m_option[]) {
        {"ai-translate", OPT_BOOL(enabled), .flags = UPDATE_AI_TRANSLATE},
        {"ai-translate-lang", OPT_STRING(lang), .flags = UPDATE_AI_TRANSLATE},
        {"ai-translate-src", OPT_STRING(src), .flags = UPDATE_AI_TRANSLATE},
        {"ai-translate-stt-url", OPT_STRING(stt_url), .flags = UPDATE_AI_TRANSLATE},
        {"ai-translate-tr-url", OPT_STRING(tr_url), .flags = UPDATE_AI_TRANSLATE},
        {"ai-translate-stt-key", OPT_STRING(stt_key), .flags = UPDATE_AI_TRANSLATE},
        {"ai-translate-model", OPT_STRING(model), .flags = UPDATE_AI_TRANSLATE},
        {"ai-translate-translate", OPT_BOOL(translate), .flags = UPDATE_AI_TRANSLATE},
        {"ai-translate-window", OPT_DOUBLE(window), M_RANGE(2, 15), .flags = UPDATE_AI_TRANSLATE},
        {"ai-translate-lookahead", OPT_DOUBLE(lookahead), M_RANGE(5, 60), .flags = UPDATE_AI_TRANSLATE},
        {"ai-translate-api-key", OPT_STRING(api_key), .flags = UPDATE_AI_TRANSLATE},
        {0}
    },
    .defaults = &(const struct ai_translate_opts) {
        .enabled = false,
        .lang = "en",
        .src = "",
        .stt_url = "http://127.0.0.1:8080",
        .tr_url = "https://api.anthropic.com",
        .stt_key = "",
        .api_key = "",
        .model = "claude-opus-4-8",
        .translate = true,
        .window = 5,
        .lookahead = 10,
    },
    .size = sizeof(struct ai_translate_opts),
};

#if HAVE_LIBCURL

#include <stdatomic.h>

#include <curl/curl.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libswresample/swresample.h>

#include "osdep/threads.h"
#include "osdep/timer.h"

#define OUT_RATE 16000
#define CTX_MAX 4               // rolling translation-context pairs
#define STT_FAIL_BACKOFF 3      // failures before backing off
#define STT_BACKOFF_SEC 15
#define TR_FAIL_BACKOFF 3
#define TR_BACKOFF_SEC 30

struct result_line {
    double start, end;
    char *text;                 // talloc child of ai
};

struct ctx_pair {
    char *src, *tr;             // talloc children of ai
};

struct ai_translate {
    struct mp_log *log;
    struct ai_translate_opts opts;  // string fields are talloc children of ai
    char *filename;
    char *api_key;                  // from $ANTHROPIC_API_KEY (may be NULL)

    mp_thread thread;
    bool thread_started;
    mp_mutex lock;
    mp_cond wakeup;

    atomic_bool quit;
    atomic_int  seek_gen;           // bumped to interrupt an in-progress window

    // --- protected by lock ---
    double playhead;
    double processed_pts;           // decode/STT frontier (seconds)
    bool   seek_pending;
    double seek_target;
    bool   done;                    // whole file processed or fatal open failure
    struct result_line *results;
    int num_results;

    // --- worker-thread only ---
    int win_samples;                // samples per STT window
    struct ctx_pair ctx[CTX_MAX];
    int ctx_len;
    int stt_fail, tr_fail;
    double stt_backoff_until, tr_backoff_until;
    bool tr_disabled;
    bool warned_no_key;
};

// ---------------------------------------------------------------------------
// Small growable byte buffer (worker thread only, talloc-backed).

struct membuf {
    void *ta;
    char *data;
    size_t len;
};

static size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *ud)
{
    struct membuf *b = ud;
    size_t add = size * nmemb;
    b->data = talloc_realloc(b->ta, b->data, char, b->len + add + 1);
    memcpy(b->data + b->len, ptr, add);
    b->len += add;
    b->data[b->len] = '\0';
    return add;
}

// ---------------------------------------------------------------------------
// Minimal JSON helpers.

// Append `s` to `out` (talloc string) with JSON string escaping (no quotes).
static char *json_escape_append(void *ta, char *out, const char *s)
{
    for (; s && *s; s++) {
        unsigned char c = *s;
        switch (c) {
        case '"':  out = talloc_strdup_append(out, "\\\""); break;
        case '\\': out = talloc_strdup_append(out, "\\\\"); break;
        case '\n': out = talloc_strdup_append(out, "\\n");  break;
        case '\r': out = talloc_strdup_append(out, "\\r");  break;
        case '\t': out = talloc_strdup_append(out, "\\t");  break;
        default:
            if (c < 0x20) {
                out = talloc_asprintf_append(out, "\\u%04x", c);
            } else {
                out = talloc_strndup_append(out, s, 1);
            }
        }
    }
    return out;
}

// Find the first "key":"value" string and return an unescaped copy of value
// (talloc child of ta), or NULL. Bounds-checked, never reads past the NUL.
static char *json_get_string(void *ta, const char *json, const char *key)
{
    char *pat = talloc_asprintf(NULL, "\"%s\"", key);
    char *p = json ? strstr(json, pat) : NULL;
    talloc_free(pat);
    if (!p)
        return NULL;
    p += strlen(key) + 2;
    while (*p && *p != ':')
        p++;
    if (*p != ':')
        return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    if (*p != '"')
        return NULL;
    p++;

    char *out = talloc_strdup(ta, "");
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            if (!*p)
                break;
            char c = *p;
            switch (c) {
            case 'n': out = talloc_strdup_append(out, "\n"); break;
            case 'r': break;
            case 't': out = talloc_strdup_append(out, "\t"); break;
            case 'u': {
                if (!p[1] || !p[2] || !p[3] || !p[4])
                    goto done;
                char hex[5] = { p[1], p[2], p[3], p[4], 0 };
                unsigned cp = (unsigned)strtol(hex, NULL, 16);
                char utf8[4];
                int n = 0;
                if (cp < 0x80) {
                    utf8[n++] = cp;
                } else if (cp < 0x800) {
                    utf8[n++] = 0xC0 | (cp >> 6);
                    utf8[n++] = 0x80 | (cp & 0x3F);
                } else {
                    utf8[n++] = 0xE0 | (cp >> 12);
                    utf8[n++] = 0x80 | ((cp >> 6) & 0x3F);
                    utf8[n++] = 0x80 | (cp & 0x3F);
                }
                out = talloc_strndup_append(out, utf8, n);
                p += 4;
                break;
            }
            default:
                out = talloc_strndup_append(out, &c, 1);
            }
            p++;
        } else {
            out = talloc_strndup_append(out, p, 1);
            p++;
        }
    }
done:
    return out;
}

// Collapse runs of whitespace/newlines and trim (in place, talloc string).
static char *tidy_text(void *ta, const char *in)
{
    if (!in)
        return NULL;
    char *out = talloc_size(ta, strlen(in) + 1);
    int j = 0;
    bool sp = true; // leading -> trim
    for (const char *p = in; *p; p++) {
        char c = *p;
        if (c == '\n' || c == '\r' || c == '\t')
            c = ' ';
        if (c == ' ') {
            if (sp)
                continue;
            sp = true;
        } else {
            sp = false;
        }
        out[j++] = c;
    }
    while (j > 0 && out[j - 1] == ' ')
        j--;
    out[j] = '\0';
    return out;
}

// ---------------------------------------------------------------------------
// HTTP: speech-to-text via whisper-server's /inference.

static void build_wav_header(uint8_t hdr[44], int nsamples)
{
    int data_bytes = nsamples * 2;      // mono s16
    int byte_rate = OUT_RATE * 2;
    uint32_t sizes[] = {0x46464952 /*RIFF*/, 36 + data_bytes, 0x45564157 /*WAVE*/,
                        0x20746d66 /*fmt */, 16};
    memcpy(hdr, sizes, 20);
    hdr[20] = 1; hdr[21] = 0;           // PCM
    hdr[22] = 1; hdr[23] = 0;           // mono
    hdr[24] = OUT_RATE & 0xff; hdr[25] = (OUT_RATE >> 8) & 0xff;
    hdr[26] = (OUT_RATE >> 16) & 0xff; hdr[27] = (OUT_RATE >> 24) & 0xff;
    hdr[28] = byte_rate & 0xff; hdr[29] = (byte_rate >> 8) & 0xff;
    hdr[30] = (byte_rate >> 16) & 0xff; hdr[31] = (byte_rate >> 24) & 0xff;
    hdr[32] = 2; hdr[33] = 0;           // block align
    hdr[34] = 16; hdr[35] = 0;          // bits per sample
    hdr[36] = 'd'; hdr[37] = 'a'; hdr[38] = 't'; hdr[39] = 'a';
    hdr[40] = data_bytes & 0xff; hdr[41] = (data_bytes >> 8) & 0xff;
    hdr[42] = (data_bytes >> 16) & 0xff; hdr[43] = (data_bytes >> 24) & 0xff;
}

// Returns talloc'd transcript (child of ta) on success, else NULL. On success
// with empty/blank content returns an empty string.
static char *stt_transcribe(struct ai_translate *ai, void *ta,
                            const int16_t *pcm, int nsamples)
{
    void *tmp = talloc_new(NULL);

    uint8_t hdr[44];
    build_wav_header(hdr, nsamples);
    int wav_len = 44 + nsamples * 2;
    uint8_t *wav = talloc_size(tmp, wav_len);
    memcpy(wav, hdr, 44);
    memcpy(wav + 44, pcm, nsamples * 2);

    char *url = talloc_asprintf(tmp, "%s/inference", ai->opts.stt_url);

    CURL *c = curl_easy_init();
    if (!c) {
        talloc_free(tmp);
        return NULL;
    }

    struct membuf resp = { .ta = tmp };
    struct curl_slist *headers = NULL;
    if (ai->opts.stt_key && ai->opts.stt_key[0]) {
        char *h = talloc_asprintf(tmp, "Authorization: Bearer %s", ai->opts.stt_key);
        headers = curl_slist_append(headers, h);
    }

    curl_mime *mime = curl_mime_init(c);
    curl_mimepart *part = curl_mime_addpart(mime);
    curl_mime_name(part, "file");
    curl_mime_data(part, (const char *)wav, wav_len);
    curl_mime_filename(part, "chunk.wav");
    curl_mime_type(part, "audio/wav");
    part = curl_mime_addpart(mime);
    curl_mime_name(part, "response_format");
    curl_mime_data(part, "json", CURL_ZERO_TERMINATED);
    part = curl_mime_addpart(mime);
    curl_mime_name(part, "temperature");
    curl_mime_data(part, "0.0", CURL_ZERO_TERMINATED);
    if (ai->opts.src && ai->opts.src[0]) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "language");
        curl_mime_data(part, ai->opts.src, CURL_ZERO_TERMINATED);
    }

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_MIMEPOST, mime);
    if (headers)
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(c);
    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);

    char *out = NULL;
    if (res == CURLE_OK && code == 200) {
        char *text = json_get_string(tmp, resp.data, "text");
        out = talloc_strdup(ta, text ? text : "");
        ai->stt_fail = 0;
    } else if (code == 401 || code == 403) {
        mp_warn(ai->log, "[ai-translate] STT auth failed (%ld); set "
                "--ai-translate-stt-key or $MPLAYER_STT_KEY.\n", code);
        ai->stt_fail++;
    } else {
        mp_warn(ai->log, "[ai-translate] STT request failed: %s (http %ld)\n",
                res != CURLE_OK ? curl_easy_strerror(res) : "server error", code);
        ai->stt_fail++;
    }

    curl_mime_free(mime);
    curl_slist_free_all(headers);
    curl_easy_cleanup(c);
    talloc_free(tmp);
    return out;
}

// ---------------------------------------------------------------------------
// HTTP: translation via the Anthropic Messages API.

// Returns talloc'd translation (child of ta), or NULL on failure (caller then
// falls back to the source text).
static char *claude_translate(struct ai_translate *ai, void *ta, const char *src)
{
    if (!ai->opts.translate || ai->tr_disabled)
        return NULL;
    if (!ai->api_key || !ai->api_key[0]) {
        if (!ai->warned_no_key) {
            mp_warn(ai->log, "[ai-translate] $ANTHROPIC_API_KEY not set; "
                    "showing untranslated text.\n");
            ai->warned_no_key = true;
        }
        return NULL;
    }

    void *tmp = talloc_new(NULL);

    char *sys = talloc_asprintf(tmp,
        "You are a live subtitle translator. Translate each new line into %s. "
        "Use previous lines only as context. Output ONLY the translated text.",
        ai->opts.lang);

    char *body = talloc_strdup(tmp, "{\"model\":\"");
    body = json_escape_append(tmp, body, ai->opts.model);
    body = talloc_strdup_append(body, "\",\"max_tokens\":1024,\"system\":\"");
    body = json_escape_append(tmp, body, sys);
    body = talloc_strdup_append(body, "\",\"messages\":[");
    for (int i = 0; i < ai->ctx_len; i++) {
        body = talloc_strdup_append(body, "{\"role\":\"user\",\"content\":\"");
        body = json_escape_append(tmp, body, ai->ctx[i].src);
        body = talloc_strdup_append(body, "\"},{\"role\":\"assistant\",\"content\":\"");
        body = json_escape_append(tmp, body, ai->ctx[i].tr);
        body = talloc_strdup_append(body, "\"},");
    }
    body = talloc_strdup_append(body, "{\"role\":\"user\",\"content\":\"");
    body = json_escape_append(tmp, body, src);
    body = talloc_strdup_append(body, "\"}]}");

    char *url = talloc_asprintf(tmp, "%s/v1/messages", ai->opts.tr_url);

    CURL *c = curl_easy_init();
    if (!c) {
        talloc_free(tmp);
        return NULL;
    }

    struct membuf resp = { .ta = tmp };
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "content-type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    char *keyh = talloc_asprintf(tmp, "x-api-key: %s", ai->api_key);
    headers = curl_slist_append(headers, keyh);

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(c);
    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);

    char *out = NULL;
    if (res == CURLE_OK && code == 200) {
        char *text = json_get_string(ta, resp.data, "text");
        if (text && text[0]) {
            out = text;
            ai->tr_fail = 0;
        }
    } else if (code == 401 || code == 403) {
        mp_warn(ai->log, "[ai-translate] translation auth failed (%ld); "
                "disabling translation for this session.\n", code);
        ai->tr_disabled = true;
    } else {
        mp_warn(ai->log, "[ai-translate] translation request failed: %s (http %ld)\n",
                res != CURLE_OK ? curl_easy_strerror(res) : "server error", code);
        ai->tr_fail++;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(c);
    talloc_free(tmp);
    return out;
}

static void push_context(struct ai_translate *ai, const char *src, const char *tr)
{
    if (ai->ctx_len == CTX_MAX) {
        talloc_free(ai->ctx[0].src);
        talloc_free(ai->ctx[0].tr);
        memmove(&ai->ctx[0], &ai->ctx[1], (CTX_MAX - 1) * sizeof(ai->ctx[0]));
        ai->ctx_len--;
    }
    ai->ctx[ai->ctx_len].src = talloc_strdup(ai, src);
    ai->ctx[ai->ctx_len].tr = talloc_strdup(ai, tr);
    ai->ctx_len++;
}

// ---------------------------------------------------------------------------
// Result store (main thread reads, worker writes; both hold ai->lock).

static void store_result(struct ai_translate *ai, double start, double end,
                         const char *text)
{
    mp_mutex_lock(&ai->lock);
    MP_TARRAY_GROW(ai, ai->results, ai->num_results + 1);
    ai->results[ai->num_results++] = (struct result_line){
        .start = start,
        .end = end,
        .text = talloc_strdup(ai, text),
    };
    mp_mutex_unlock(&ai->lock);
}

// ---------------------------------------------------------------------------
// Decode context (worker thread only).

struct dec_ctx {
    AVFormatContext *fmt;
    AVCodecContext *dec;
    SwrContext *swr;
    AVPacket *pkt;
    AVFrame *frame;
    int stream;
    double start_off;       // container start_time in seconds
    double next_pts;        // running estimate for frames without pts
    // window accumulator
    int16_t *acc;
    int acc_len;            // samples
    int acc_cap;
    double acc_start;
    bool acc_has_start;
};

static bool dec_open(struct ai_translate *ai, struct dec_ctx *d)
{
    if (avformat_open_input(&d->fmt, ai->filename, NULL, NULL) < 0) {
        mp_warn(ai->log, "[ai-translate] cannot reopen '%s' for lookahead; "
                "disabling.\n", ai->filename);
        return false;
    }
    if (avformat_find_stream_info(d->fmt, NULL) < 0)
        return false;

    d->stream = av_find_best_stream(d->fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (d->stream < 0) {
        mp_warn(ai->log, "[ai-translate] no audio stream; disabling.\n");
        return false;
    }
    AVStream *st = d->fmt->streams[d->stream];
    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec)
        return false;
    d->dec = avcodec_alloc_context3(codec);
    if (!d->dec || avcodec_parameters_to_context(d->dec, st->codecpar) < 0)
        return false;
    if (avcodec_open2(d->dec, codec, NULL) < 0)
        return false;

    d->start_off = d->fmt->start_time != AV_NOPTS_VALUE
                 ? d->fmt->start_time / (double)AV_TIME_BASE : 0;

    d->pkt = av_packet_alloc();
    d->frame = av_frame_alloc();
    if (!d->pkt || !d->frame)
        return false;
    return true;
}

static void dec_close(struct dec_ctx *d)
{
    if (d->swr)
        swr_free(&d->swr);
    if (d->frame)
        av_frame_free(&d->frame);
    if (d->pkt)
        av_packet_free(&d->pkt);
    if (d->dec)
        avcodec_free_context(&d->dec);
    if (d->fmt)
        avformat_close_input(&d->fmt);
    talloc_free(d->acc);
    memset(d, 0, sizeof(*d));
}

static void acc_reset(struct dec_ctx *d)
{
    d->acc_len = 0;
    d->acc_has_start = false;
}

static bool ensure_swr(struct dec_ctx *d)
{
    if (d->swr)
        return true;
    AVChannelLayout out_ch = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
    if (swr_alloc_set_opts2(&d->swr, &out_ch, AV_SAMPLE_FMT_S16, OUT_RATE,
                            &d->frame->ch_layout, d->frame->format,
                            d->frame->sample_rate, 0, NULL) < 0)
        return false;
    return swr_init(d->swr) >= 0;
}

static void acc_append_frame(struct dec_ctx *d, AVStream *st)
{
    if (!ensure_swr(d))
        return;

    if (!d->acc_has_start) {
        double t = d->frame->pts != AV_NOPTS_VALUE
                 ? d->frame->pts * av_q2d(st->time_base) : d->next_pts;
        d->acc_start = t - d->start_off;
        d->acc_has_start = true;
    }

    int max_out = swr_get_out_samples(d->swr, d->frame->nb_samples);
    if (d->acc_len + max_out > d->acc_cap) {
        d->acc_cap = d->acc_len + max_out + OUT_RATE;
        d->acc = talloc_realloc(NULL, d->acc, int16_t, d->acc_cap);
    }
    uint8_t *outp = (uint8_t *)(d->acc + d->acc_len);
    int got = swr_convert(d->swr, &outp, max_out,
                          (const uint8_t **)d->frame->extended_data,
                          d->frame->nb_samples);
    if (got > 0)
        d->acc_len += got;

    if (d->frame->pts != AV_NOPTS_VALUE)
        d->next_pts = d->frame->pts * av_q2d(st->time_base) +
                      d->frame->nb_samples / (double)d->frame->sample_rate;
}

// ---------------------------------------------------------------------------
// Worker.

static bool worker_check_control(struct ai_translate *ai, struct dec_ctx *d,
                                 int *seek_gen)
{
    // Returns false if the caller should abort the current window (quit/seek).
    if (atomic_load(&ai->quit))
        return false;
    if (atomic_load(&ai->seek_gen) != *seek_gen)
        return false;
    return true;
}

// Perform a pending seek if any (worker thread). Returns updated seek_gen.
static void handle_seek(struct ai_translate *ai, struct dec_ctx *d,
                        int *seek_gen, bool *eof)
{
    mp_mutex_lock(&ai->lock);
    bool pending = ai->seek_pending;
    double target = ai->seek_target;
    ai->seek_pending = false;
    *seek_gen = atomic_load(&ai->seek_gen);
    if (pending)
        ai->processed_pts = target;
    mp_mutex_unlock(&ai->lock);

    if (!pending)
        return;

    double seek_to = MPMAX(0, target - 2.0);
    int64_t ts = (int64_t)((seek_to + d->start_off) * AV_TIME_BASE);
    if (av_seek_frame(d->fmt, -1, ts, AVSEEK_FLAG_BACKWARD) >= 0) {
        avcodec_flush_buffers(d->dec);
        acc_reset(d);
        d->next_pts = seek_to;
        *eof = false;
        mp_msg(ai->log, MSGL_V, "[ai-translate] lookahead decoder seek to %.1fs\n",
               seek_to);
    }
}

static MP_THREAD_VOID worker_thread(void *arg)
{
    struct ai_translate *ai = arg;
    mp_thread_set_name("ai-translate");

    struct dec_ctx d = {0};
    if (!dec_open(ai, &d)) {
        dec_close(&d);
        mp_mutex_lock(&ai->lock);
        ai->done = true;                // never stall the playloop
        mp_mutex_unlock(&ai->lock);
        MP_THREAD_RETURN();
    }

    AVStream *st = d.fmt->streams[d.stream];
    int seek_gen = atomic_load(&ai->seek_gen);
    bool eof = false;

    while (!atomic_load(&ai->quit)) {
        handle_seek(ai, &d, &seek_gen, &eof);
        if (atomic_load(&ai->quit))
            break;

        // Pacing: don't run too far ahead of the playhead.
        mp_mutex_lock(&ai->lock);
        double playhead = ai->playhead;
        double processed = ai->processed_pts;
        double max_ahead = MPMAX(30.0, ai->opts.lookahead * 3);
        bool wait = (eof || processed > playhead + max_ahead) && !ai->seek_pending;
        if (wait)
            mp_cond_timedwait(&ai->wakeup, &ai->lock, MP_TIME_MS_TO_NS(200));
        mp_mutex_unlock(&ai->lock);
        if (wait)
            continue;

        // STT/translation backoff windows.
        double now = mp_time_sec();
        if (now < ai->stt_backoff_until)
            continue;

        // Fill one window.
        bool hit_eof = false;
        while (d.acc_len < ai->win_samples) {
            if (!worker_check_control(ai, &d, &seek_gen))
                break;
            int r = av_read_frame(d.fmt, d.pkt);
            if (r < 0) {
                avcodec_send_packet(d.dec, NULL); // drain
                while (avcodec_receive_frame(d.dec, d.frame) >= 0)
                    acc_append_frame(&d, st);
                hit_eof = true;
                break;
            }
            if (d.pkt->stream_index != d.stream) {
                av_packet_unref(d.pkt);
                continue;
            }
            if (avcodec_send_packet(d.dec, d.pkt) >= 0) {
                while (avcodec_receive_frame(d.dec, d.frame) >= 0)
                    acc_append_frame(&d, st);
            }
            av_packet_unref(d.pkt);
        }

        if (atomic_load(&ai->quit))
            break;
        if (atomic_load(&ai->seek_gen) != seek_gen) {
            acc_reset(&d);
            continue;
        }

        if (d.acc_len > 0 && d.acc_has_start) {
            double wstart = d.acc_start;
            double wend = wstart + d.acc_len / (double)OUT_RATE;

            void *tmp = talloc_new(NULL);
            char *raw = stt_transcribe(ai, tmp, d.acc, d.acc_len);
            if (raw) {
                char *src = tidy_text(tmp, raw);
                bool blank = !src || !src[0] ||
                             strstr(src, "[BLANK_AUDIO]") ||
                             strstr(src, "(blank audio)");
                if (!blank) {
                    char *tr = claude_translate(ai, tmp, src);
                    const char *shown = tr ? tr : src;
                    if (tr)
                        push_context(ai, src, tr);
                    store_result(ai, wstart, wend, shown);
                    mp_msg(ai->log, MSGL_V, "[ai-translate] %.1f-%.1f \"%s\" -> \"%s\"\n",
                           wstart, wend, src, shown);
                }
            } else if (ai->stt_fail >= STT_FAIL_BACKOFF) {
                ai->stt_backoff_until = mp_time_sec() + STT_BACKOFF_SEC;
                ai->stt_fail = 0;
                mp_warn(ai->log, "[ai-translate] backing off STT for %ds.\n",
                        STT_BACKOFF_SEC);
            }
            talloc_free(tmp);

            mp_mutex_lock(&ai->lock);
            if (wend > ai->processed_pts)
                ai->processed_pts = wend;
            mp_mutex_unlock(&ai->lock);
        }

        acc_reset(&d);

        if (hit_eof) {
            mp_mutex_lock(&ai->lock);
            ai->done = true;
            ai->processed_pts = INFINITY;
            eof = true;
            mp_mutex_unlock(&ai->lock);
        }
    }

    dec_close(&d);
    MP_THREAD_RETURN();
}

// ---------------------------------------------------------------------------
// Public API.

static bool source_is_reopenable(const char *fn)
{
    if (!fn || !fn[0])
        return false;
    static const char *bad[] = {"-", "stdin://", "dvd://", "dvdnav://", "bd://",
                                "bluray://", "tv://", "pvr://", "cdda://", NULL};
    for (int i = 0; bad[i]; i++) {
        if (strncmp(fn, bad[i], strlen(bad[i])) == 0)
            return false;
    }
    return true;
}

struct ai_translate *ai_translate_create(struct mpv_global *global,
                                         struct mp_log *log,
                                         const struct ai_translate_opts *opts,
                                         const char *filename)
{
    if (!opts || !opts->enabled)
        return NULL;
    if (!source_is_reopenable(filename)) {
        mp_warn(log, "[ai-translate] source '%s' cannot be reopened for "
                "lookahead; disabling.\n", filename ? filename : "?");
        return NULL;
    }

    struct ai_translate *ai = talloc_zero(NULL, struct ai_translate);
    ai->log = mp_log_new(ai, log, "ai-translate");
    ai->filename = talloc_strdup(ai, filename);
    ai->opts = *opts;
    ai->opts.lang = talloc_strdup(ai, opts->lang ? opts->lang : "en");
    ai->opts.src = talloc_strdup(ai, opts->src ? opts->src : "");
    ai->opts.stt_url = talloc_strdup(ai, opts->stt_url ? opts->stt_url : "");
    ai->opts.tr_url = talloc_strdup(ai, opts->tr_url ? opts->tr_url : "");
    ai->opts.stt_key = talloc_strdup(ai, opts->stt_key ? opts->stt_key : "");
    ai->opts.model = talloc_strdup(ai, opts->model ? opts->model : "");
    ai->api_key = talloc_strdup(ai, opts->api_key ? opts->api_key : "");

    // Env overrides (match the documented distribution knobs).
    const char *e;
    if ((e = getenv("MPLAYER_STT_URL")) && e[0])
        ai->opts.stt_url = talloc_strdup(ai, e);
    if ((e = getenv("MPLAYER_TR_URL")) && e[0])
        ai->opts.tr_url = talloc_strdup(ai, e);
    if ((e = getenv("MPLAYER_STT_KEY")) && e[0])
        ai->opts.stt_key = talloc_strdup(ai, e);

    ai->win_samples = (int)(MPCLAMP(opts->window, 2, 15) * OUT_RATE);
    atomic_init(&ai->quit, false);
    atomic_init(&ai->seek_gen, 0);

    if (mp_mutex_init(&ai->lock) || mp_cond_init(&ai->wakeup)) {
        talloc_free(ai);
        return NULL;
    }

    curl_global_init(CURL_GLOBAL_ALL);
    avformat_network_init();

    if (mp_thread_create(&ai->thread, worker_thread, ai) != 0) {
        mp_warn(ai->log, "[ai-translate] failed to start worker thread.\n");
        avformat_network_deinit();
        curl_global_cleanup();
        mp_mutex_destroy(&ai->lock);
        mp_cond_destroy(&ai->wakeup);
        talloc_free(ai);
        return NULL;
    }
    ai->thread_started = true;

    mp_info(ai->log, "[ai-translate] lookahead translation to '%s' enabled "
            "(STT %s).\n", ai->opts.lang, ai->opts.stt_url);
    return ai;
}

void ai_translate_destroy(struct ai_translate *ai)
{
    if (!ai)
        return;
    if (ai->thread_started) {
        atomic_store(&ai->quit, true);
        mp_mutex_lock(&ai->lock);
        mp_cond_signal(&ai->wakeup);
        mp_mutex_unlock(&ai->lock);
        mp_thread_join(ai->thread);
        avformat_network_deinit();
        curl_global_cleanup();
    }
    mp_mutex_destroy(&ai->lock);
    mp_cond_destroy(&ai->wakeup);
    talloc_free(ai);
}

void ai_translate_set_playhead(struct ai_translate *ai, double pts)
{
    if (!ai || pts == MP_NOPTS_VALUE)
        return;
    mp_mutex_lock(&ai->lock);
    ai->playhead = pts;
    mp_cond_signal(&ai->wakeup);
    mp_mutex_unlock(&ai->lock);
}

void ai_translate_seek(struct ai_translate *ai, double pts)
{
    if (!ai || pts == MP_NOPTS_VALUE)
        return;
    mp_mutex_lock(&ai->lock);
    ai->playhead = pts;
    // Only re-seek the decoder when jumping into unprocessed future; backward
    // seeks are served from the cached results.
    if (pts > ai->processed_pts + 5.0) {
        ai->seek_pending = true;
        ai->seek_target = pts;
        ai->done = false;
        atomic_fetch_add(&ai->seek_gen, 1);
    }
    mp_cond_signal(&ai->wakeup);
    mp_mutex_unlock(&ai->lock);
}

double ai_translate_get_frontier(struct ai_translate *ai)
{
    if (!ai)
        return INFINITY;
    mp_mutex_lock(&ai->lock);
    double f = ai->done ? INFINITY : ai->processed_pts;
    mp_mutex_unlock(&ai->lock);
    return f;
}

bool ai_translate_get_line(struct ai_translate *ai, double pts, char **out)
{
    *out = NULL;
    if (!ai || pts == MP_NOPTS_VALUE)
        return false;
    mp_mutex_lock(&ai->lock);
    // Latest line whose interval contains pts.
    for (int i = ai->num_results - 1; i >= 0; i--) {
        struct result_line *r = &ai->results[i];
        if (pts >= r->start && pts < r->end) {
            *out = talloc_strdup(NULL, r->text);
            break;
        }
    }
    mp_mutex_unlock(&ai->lock);
    return *out != NULL;
}

#else /* !HAVE_LIBCURL */

struct ai_translate *ai_translate_create(struct mpv_global *global,
                                         struct mp_log *log,
                                         const struct ai_translate_opts *opts,
                                         const char *filename)
{
    if (opts && opts->enabled)
        mp_warn(log, "[ai-translate] mpv was built without libcurl; "
                "AI translation is unavailable.\n");
    return NULL;
}

void ai_translate_destroy(struct ai_translate *ai) { }
void ai_translate_set_playhead(struct ai_translate *ai, double pts) { }
void ai_translate_seek(struct ai_translate *ai, double pts) { }
double ai_translate_get_frontier(struct ai_translate *ai) { return 1e18; }
bool ai_translate_get_line(struct ai_translate *ai, double pts, char **out)
{
    *out = NULL;
    return false;
}

#endif /* HAVE_LIBCURL */
