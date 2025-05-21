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

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/opt.h>

#include "options/path.h"
#include "common/common.h"
#include "common/msg.h"
#include "common/tags.h"
#include "common/av_common.h"
#include "demux/demux.h"
#include "misc/charset_conv.h"
#include "misc/thread_tools.h"
#include "stream.h"
#include "options/m_config.h"
#include "options/m_option.h"

#include "cookies.h"

#include "misc/bstr.h"
#include "mpv_talloc.h"

#define OPT_BASE_STRUCT struct stream_lavf_params
struct stream_lavf_params {
    char **avopts;
    bool cookies_enabled;
    char *cookies_file;
    char *useragent;
    char *referrer;
    char **http_header_fields;
    bool tls_verify;
    char *tls_ca_file;
    char *tls_cert_file;
    char *tls_key_file;
    double timeout;
    char *http_proxy;
};

const struct m_sub_options stream_lavf_conf = {
    .opts = (const m_option_t[]) {
        {"stream-lavf-o", OPT_KEYVALUELIST(avopts)},
        {"http-header-fields", OPT_STRINGLIST(http_header_fields)},
        {"user-agent", OPT_STRING(useragent)},
        {"referrer", OPT_STRING(referrer)},
        {"cookies", OPT_BOOL(cookies_enabled)},
        {"cookies-file", OPT_STRING(cookies_file), .flags = M_OPT_FILE},
        {"tls-verify", OPT_BOOL(tls_verify)},
        {"tls-ca-file", OPT_STRING(tls_ca_file), .flags = M_OPT_FILE},
        {"tls-cert-file", OPT_STRING(tls_cert_file), .flags = M_OPT_FILE},
        {"tls-key-file", OPT_STRING(tls_key_file), .flags = M_OPT_FILE},
        {"network-timeout", OPT_DOUBLE(timeout), M_RANGE(0, DBL_MAX)},
        {"http-proxy", OPT_STRING(http_proxy)},
        {0}
    },
    .size = sizeof(struct stream_lavf_params),
    .defaults = &(const struct stream_lavf_params){
        .useragent = "libmpv",
        .timeout = 60,
    },
};

static const char *const http_like[] =
    {"http", "https", "mmsh", "mmshttp", "httproxy", NULL};

static int open_f(stream_t *stream);
static struct mp_tags *read_icy(stream_t *stream);

static int fill_buffer(stream_t *s, void *buffer, int max_len)
{
    AVIOContext *avio = s->priv;
    int r = avio_read_partial(avio, buffer, max_len);
    return (r <= 0) ? -1 : r;
}

static int write_buffer(stream_t *s, void *buffer, int len)
{
    AVIOContext *avio = s->priv;
    avio_write(avio, buffer, len);
    avio_flush(avio);
    if (avio->error)
        return -1;
    return len;
}

static int seek(stream_t *s, int64_t newpos)
{
    AVIOContext *avio = s->priv;
    if (avio_seek(avio, newpos, SEEK_SET) < 0) {
        return 0;
    }
    return 1;
}

static int64_t get_size(stream_t *s)
{
    AVIOContext *avio = s->priv;
    return avio_size(avio);
}

static void close_f(stream_t *stream)
{
    AVIOContext *avio = stream->priv;
    /* NOTE: As of 2011 write streams must be manually flushed before close.
     * Currently write_buffer() always flushes them after writing.
     * avio_close() could return an error, but we have no way to return that
     * with the current stream API.
     */
    if (avio)
        avio_close(avio);
}

static int control(stream_t *s, int cmd, void *arg)
{
    AVIOContext *avio = s->priv;
    switch(cmd) {
    case STREAM_CTRL_AVSEEK: {
        struct stream_avseek *c = arg;
        int64_t r = avio_seek_time(avio, c->stream_index, c->timestamp, c->flags);
        if (r >= 0) {
            stream_drop_buffers(s);
            return 1;
        }
        break;
    }
    case STREAM_CTRL_HAS_AVSEEK: {
        // Starting at some point, read_seek is always available, and runtime
        // behavior decides whether it exists or not. FFmpeg's API doesn't
        // return anything helpful to determine seekability upfront, so here's
        // a hardcoded whitelist. Not our fault.
        // In addition we also have to jump through ridiculous hoops just to
        // get the fucking protocol name.
        const char *proto = NULL;
        if (avio->av_class && avio->av_class->child_next) {
            // This usually yields the URLContext (why does it even exist?),
            // which holds the name of the actual protocol implementation.
            void *child = avio->av_class->child_next(avio, NULL);
            AVClass *cl = *(AVClass **)child;
            if (cl && cl->item_name)
                proto = cl->item_name(child);
        }
        static const char *const has_read_seek[] = {
            "rtmp", "rtmpt", "rtmpe", "rtmpte", "rtmps", "rtmpts", "mmsh", 0};
        for (int n = 0; has_read_seek[n]; n++) {
            if (avio->read_seek && proto && strcmp(proto, has_read_seek[n]) == 0)
                return 1;
        }
        break;
    }
    case STREAM_CTRL_GET_METADATA: {
        *(struct mp_tags **)arg = read_icy(s);
        if (!*(struct mp_tags **)arg)
            break;
        return 1;
    }
    }
    return STREAM_UNSUPPORTED;
}

static int interrupt_cb(void *ctx)
{
    struct stream *stream = ctx;
    return mp_cancel_test(stream->cancel);
}

static const char * const prefix[] = { "lavf://", "ffmpeg://" };

void mp_setup_av_network_options(AVDictionary **dict, const char *target_fmt,
                                 struct mpv_global *global, struct mp_log *log)
{
    void *temp = talloc_new(NULL);
    struct stream_lavf_params *opts =
        mp_get_config_group(temp, global, &stream_lavf_conf);

    // HTTP specific options (other protocols ignore them)
    if (opts->useragent)
        av_dict_set(dict, "user_agent", opts->useragent, 0);
    if (opts->cookies_enabled) {
        char *file = opts->cookies_file;
        if (file && file[0])
            file = mp_get_user_path(temp, global, file);
        char *cookies = cookies_lavf(temp, global, log, file);
        if (cookies && cookies[0])
            av_dict_set(dict, "cookies", cookies, 0);
    }
    av_dict_set(dict, "tls_verify", opts->tls_verify ? "1" : "0", 0);
    if (opts->tls_ca_file) {
        char *file = mp_get_user_path(temp, global, opts->tls_ca_file);
        av_dict_set(dict, "ca_file", file, 0);
    }
    if (opts->tls_cert_file) {
        char *file = mp_get_user_path(temp, global, opts->tls_cert_file);
        av_dict_set(dict, "cert_file", file, 0);
    }
    if (opts->tls_key_file) {
        char *file = mp_get_user_path(temp, global, opts->tls_key_file);
        av_dict_set(dict, "key_file", file, 0);
    }
    char *cust_headers = talloc_strdup(temp, "");
    if (opts->referrer) {
        cust_headers = talloc_asprintf_append(cust_headers, "Referer: %s\r\n",
                                              opts->referrer);
    }
    if (opts->http_header_fields) {
        for (int n = 0; opts->http_header_fields[n]; n++) {
            cust_headers = talloc_asprintf_append(cust_headers, "%s\r\n",
                                                  opts->http_header_fields[n]);
        }
    }
    if (strlen(cust_headers))
        av_dict_set(dict, "headers", cust_headers, 0);
    av_dict_set(dict, "icy", "1", 0);
    // So far, every known protocol uses microseconds for this
    // Except rtsp.
    if (opts->timeout > 0) {
        if (target_fmt && strcmp(target_fmt, "rtsp") == 0) {
            mp_verbose(log, "Broken FFmpeg RTSP API => not setting timeout.\n");
        } else {
            char buf[80];
            snprintf(buf, sizeof(buf), "%lld", (long long)(opts->timeout * 1e6));
            av_dict_set(dict, "timeout", buf, 0);
        }
    }
    if (opts->http_proxy && opts->http_proxy[0])
        av_dict_set(dict, "http_proxy", opts->http_proxy, 0);

    mp_set_avdict(dict, opts->avopts);

    talloc_free(temp);
}

#define PROTO(...) (const char *[]){__VA_ARGS__, NULL}

// List of safe protocols and their aliases
static const char **safe_protos[] = {
    PROTO("data"),
    PROTO("gopher"),
    PROTO("gophers"),
    PROTO("http", "dav", "webdav"),
    PROTO("httpproxy"),
    PROTO("https", "davs", "webdavs"),
    PROTO("ipfs"),
    PROTO("ipns"),
    PROTO("mmsh", "mms", "mmshttp"),
    PROTO("mmst"),
    PROTO("rist"),
    PROTO("rtmp"),
    PROTO("rtmpe"),
    PROTO("rtmps"),
    PROTO("rtmpt"),
    PROTO("rtmpte"),
    PROTO("rtmpts"),
    PROTO("rtp"),
    PROTO("srt"),
    PROTO("srtp"),
    NULL,
};

static char **get_safe_protocols(void)
{
    int num = 0;
    char **protocols = NULL;
    char **ffmpeg_demuxers = mp_get_lavf_demuxers();
    char **ffmpeg_protos = mp_get_lavf_protocols();

    for (int i = 0; ffmpeg_protos[i]; i++) {
        for (int j = 0; safe_protos[j]; j++) {
            if (strcmp(ffmpeg_protos[i], safe_protos[j][0]) != 0)
                continue;
            for (int k = 0; safe_protos[j][k]; k++)
                MP_TARRAY_APPEND(NULL, protocols, num, talloc_strdup(protocols, safe_protos[j][k]));
            break;
        }
    }

    // rtsp is a demuxer not protocol in ffmpeg so it is handled separately
    for (int i = 0; ffmpeg_demuxers[i]; i++) {
        if (strcmp("rtsp", ffmpeg_demuxers[i]) == 0) {
            MP_TARRAY_APPEND(NULL, protocols, num, talloc_strdup(protocols, "rtsp"));
            MP_TARRAY_APPEND(NULL, protocols, num, talloc_strdup(protocols, "rtsps"));
            break;
        }
    }

    MP_TARRAY_APPEND(NULL, protocols, num, NULL);

    talloc_free(ffmpeg_demuxers);
    talloc_free(ffmpeg_protos);

    return protocols;
}

static char **get_unsafe_protocols(void)
{
    int num = 0;
    char **protocols = NULL;
    char **safe_protocols = get_safe_protocols();
    char **ffmpeg_protos = mp_get_lavf_protocols();

    for (int i = 0; ffmpeg_protos[i]; i++) {
        bool safe_protocol = false;
        for (int j = 0; safe_protocols[j]; j++) {
            if (strcmp(ffmpeg_protos[i], safe_protocols[j]) == 0) {
                safe_protocol = true;
                break;
            }
        }
        // Skip to avoid name conflict with builtin mpv protocol.
        if (strcmp(ffmpeg_protos[i], "bluray") == 0 || strcmp(ffmpeg_protos[i], "dvd") == 0)
            continue;

        if (!safe_protocol)
            MP_TARRAY_APPEND(NULL, protocols, num, talloc_strdup(protocols, ffmpeg_protos[i]));
    }

    MP_TARRAY_APPEND(NULL, protocols, num, talloc_strdup(protocols, "ffmpeg"));
    MP_TARRAY_APPEND(NULL, protocols, num, talloc_strdup(protocols, "lavf"));

    MP_TARRAY_APPEND(NULL, protocols, num, NULL);

    talloc_free(ffmpeg_protos);
    talloc_free(safe_protocols);
    return protocols;
}

// Escape http URLs with unescaped, invalid characters in them.
// libavformat's http protocol does not do this, and a patch to add this
// in a 100% safe case (spaces only) was rejected.
static char *normalize_url(void *ta_parent, const char *filename)
{
    bstr proto = mp_split_proto(bstr0(filename), NULL);
    for (int n = 0; http_like[n]; n++) {
        if (bstr_equals0(proto, http_like[n]))
            // Escape everything but reserved characters.
            // Also don't double-scape, so include '%'.
            return mp_url_escape(ta_parent, filename, ":/?#[]@!$&'()*+,;=%");
    }
    return (char *)filename;
}

static int open_f(stream_t *stream)
{
    AVIOContext *avio = NULL;
    int res = STREAM_ERROR;
    AVDictionary *dict = NULL;
    void *temp = talloc_new(NULL);

    stream->seek = NULL;
    stream->seekable = false;

    int flags = stream->mode == STREAM_WRITE ? AVIO_FLAG_WRITE : AVIO_FLAG_READ;

    const char *filename = stream->url;
    if (!filename) {
        MP_ERR(stream, "No URL\n");
        goto out;
    }
    for (int i = 0; i < sizeof(prefix) / sizeof(prefix[0]); i++)
        if (!strncmp(filename, prefix[i], strlen(prefix[i])))
            filename += strlen(prefix[i]);
    if (!strncmp(filename, "rtsp:", 5) || !strncmp(filename, "rtsps:", 6)) {
        /* This is handled as a special demuxer, without a separate
         * stream layer. demux_lavf will do all the real work. Note
         * that libavformat doesn't even provide a protocol entry for
         * this (the rtsp demuxer's probe function checks for a "rtsp:"
         * filename prefix), so it has to be handled specially here.
         */
        stream->demuxer = "lavf";
        stream->lavf_type = "rtsp";
        talloc_free(temp);
        return STREAM_OK;
    }

    // Replace "mms://" with "mmsh://", so that most mms:// URLs just work.
    // Replace "dav://" or "webdav://" with "http://" and "davs://" or "webdavs://" with "https://"
    bstr b_filename = bstr0(filename);
    if (bstr_eatstart0(&b_filename, "mms://") ||
        bstr_eatstart0(&b_filename, "mmshttp://"))
    {
        filename = talloc_asprintf(temp, "mmsh://%.*s", BSTR_P(b_filename));
    } else if (bstr_eatstart0(&b_filename, "dav://") || bstr_eatstart0(&b_filename, "webdav://"))
    {
        filename = talloc_asprintf(temp, "http://%.*s", BSTR_P(b_filename));
    } else if (bstr_eatstart0(&b_filename, "davs://") || bstr_eatstart0(&b_filename, "webdavs://"))
    {
        filename = talloc_asprintf(temp, "https://%.*s", BSTR_P(b_filename));
    }

    av_dict_set(&dict, "reconnect", "1", 0);
    av_dict_set(&dict, "reconnect_delay_max", "7", 0);

    mp_setup_av_network_options(&dict, NULL, stream->global, stream->log);

    AVIOInterruptCB cb = {
        .callback = interrupt_cb,
        .opaque = stream,
    };

    filename = normalize_url(stream, filename);

    if (strncmp(filename, "rtmp", 4) == 0) {
        stream->demuxer = "lavf";
        stream->lavf_type = "flv";
        // Setting timeout enables listen mode - force it to disabled.
        av_dict_set(&dict, "timeout", "0", 0);
    }

    int err = avio_open2(&avio, filename, flags, &cb, &dict);
    if (err < 0) {
        if (err == AVERROR_PROTOCOL_NOT_FOUND)
            MP_ERR(stream, "Protocol not found. Make sure"
                   " FFmpeg is compiled with networking support.\n");
        goto out;
    }

    mp_avdict_print_unset(stream->log, MSGL_V, dict);

    if (avio->av_class) {
        uint8_t *mt = NULL;
        if (av_opt_get(avio, "mime_type", AV_OPT_SEARCH_CHILDREN, &mt) >= 0) {
            stream->mime_type = talloc_strdup(stream, mt);
            av_free(mt);
        }
    }

    stream->priv = avio;
    stream->seekable = avio->seekable & AVIO_SEEKABLE_NORMAL;
    stream->seek = stream->seekable ? seek : NULL;
    stream->fill_buffer = fill_buffer;
    stream->write_buffer = write_buffer;
    stream->get_size = get_size;
    stream->control = control;
    stream->close = close_f;
    // enable cache (should be avoided for files, but no way to detect this)
    stream->streaming = true;
    if (stream->info->stream_origin == STREAM_ORIGIN_NET)
        stream->is_network = true;
    res = STREAM_OK;

out:
    av_dict_free(&dict);
    talloc_free(temp);
    return res;
}

static struct mp_tags *read_icy(stream_t *s)
{
    AVIOContext *avio = s->priv;

    if (!avio->av_class)
        return NULL;

    uint8_t *icy_header = NULL;
    if (av_opt_get(avio, "icy_metadata_headers", AV_OPT_SEARCH_CHILDREN,
                   &icy_header) < 0)
        icy_header = NULL;

    uint8_t *icy_packet;
    if (av_opt_get(avio, "icy_metadata_packet", AV_OPT_SEARCH_CHILDREN,
                   &icy_packet) < 0)
        icy_packet = NULL;

    // Send a metadata update only 1. on start, and 2. on a new metadata packet.
    // To detect new packages, set the icy_metadata_packet to "-" once we've
    // read it (a bit hacky, but works).

    struct mp_tags *res = NULL;
    if ((!icy_header || !icy_header[0]) && (!icy_packet || !icy_packet[0]))
        goto done;

    bstr packet = bstr0(icy_packet);
    if (bstr_equals0(packet, "-"))
        goto done;

    res = talloc_zero(NULL, struct mp_tags);

    bstr header = bstr0(icy_header);
    while (header.len) {
        bstr line = bstr_strip_linebreaks(bstr_getline(header, &header));
        bstr name, val;
        if (bstr_split_tok(line, ": ", &name, &val))
            mp_tags_set_bstr(res, name, val);
    }

    bstr head = bstr0("StreamTitle='");
    int i = bstr_find(packet, head);
    if (i >= 0) {
        packet = bstr_cut(packet, i + head.len);
        int end = bstr_find(packet, bstr0("\';"));
        packet = bstr_splice(packet, 0, end);

        bool allocated = false;
        struct demux_opts *opts = mp_get_config_group(NULL, s->global, &demux_conf);
        const char *charset = mp_charset_guess(s, s->log, packet, opts->meta_cp, 0);
        if (charset && !mp_charset_is_utf8(charset)) {
            bstr conv = mp_iconv_to_utf8(s->log, packet, charset, 0);
            if (conv.start && conv.start != packet.start) {
                allocated = true;
                packet = conv;
            }
        }
        mp_tags_set_bstr(res, bstr0("icy-title"), packet);
        talloc_free(opts);
        if (allocated)
            talloc_free(packet.start);
    }

    av_opt_set(avio, "icy_metadata_packet", "-", AV_OPT_SEARCH_CHILDREN);

done:
    av_free(icy_header);
    av_free(icy_packet);
    return res;
}

const stream_info_t stream_info_ffmpeg = {
    .name = "ffmpeg",
    .open = open_f,
    .get_protocols = get_safe_protocols,
    .can_write = true,
    .stream_origin = STREAM_ORIGIN_NET,
};

// Unlike above, this is not marked as safe, and can contain protocols which
// may do insecure things. (Such as "ffmpeg", which can access the "lavfi"
// pseudo-demuxer, which in turn gives access to filters that can access the
// local filesystem.)
const stream_info_t stream_info_ffmpeg_unsafe = {
    .name = "ffmpeg",
    .open = open_f,
    .get_protocols = get_unsafe_protocols,
    .stream_origin = STREAM_ORIGIN_UNSAFE,
    .can_write = true,
};
