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

#include <float.h>
#include <string.h>

#include <libavformat/version.h>

#include "common/common.h"
#include "common/tags.h"
#include "demux/demux.h"
#include "misc/charset_conv.h"
#include "mpv_talloc.h"
#include "network.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "stream.h"

#define OPT_BASE_STRUCT struct mp_network_opts

const struct m_sub_options mp_network_conf = {
    .opts = (const m_option_t[]) {
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
    .size = sizeof(struct mp_network_opts),
    .defaults = &(const struct mp_network_opts){
        .useragent = "libmpv",
        .timeout = 60,
#if HAVE_LIBCURL || LIBAVFORMAT_VERSION_MAJOR >= 63
        .tls_verify = true,
#endif
    },
};

struct mp_icy {
    uint64_t metaint;       // bytes between metadata blocks (0 = no ICY)
    uint64_t data_read;     // data bytes since last metadata block
    enum {
        ICY_DATA = 0,
        ICY_LEN,
        ICY_META,
    } state;
    size_t meta_pending;    // bytes left in the current metadata block
    size_t meta_pos;        // bytes already accumulated in meta_buf
    char meta_buf[255 * 16 + 1];
    bstr headers;           // accumulated "Icy-Name: value\n" lines
    bstr packet;            // last metadata payload
    bool dirty;             // new metadata to deliver
};

struct mp_icy *mp_icy_new(void *ta_parent)
{
    return talloc_zero(ta_parent, struct mp_icy);
}

void mp_icy_reset(struct mp_icy *i)
{
    i->metaint = 0;
    i->data_read = 0;
    i->state = ICY_DATA;
    i->meta_pending = 0;
    i->meta_pos = 0;
    i->headers.len = 0;
    i->packet.len = 0;
    i->dirty = false;
}

void mp_icy_add_header(struct mp_icy *i, bstr line)
{
    bstr name, val;
    if (!bstr_split_tok(line, ": ", &name, &val))
        return;
    if (!bstr_case_startswith(name, bstr0("Icy-")))
        return;

    if (bstrcasecmp0(name, "Icy-MetaInt") == 0) {
        long long mi = bstrtoll(val, NULL, 10);
        if (mi > 0)
            i->metaint = mi;
        return;
    }
    // This may look a bit weird, that we join headers again, but it's done
    // to share common parse function with lavf format later.
    bstr_xappend_asprintf(i, &i->headers, "%.*s: %.*s\n", BSTR_P(name), BSTR_P(val));
    i->dirty = true;
}

bool mp_icy_active(const struct mp_icy *i)
{
    return i->metaint > 0;
}

void mp_icy_process(struct mp_icy *i, const char *buf, size_t len,
                    mp_icy_write_fn write_cb, void *ctx)
{
    if (!mp_icy_active(i)) {
        if (len)
            write_cb(ctx, buf, len);
        return;
    }
    size_t pos = 0;
    while (pos < len) {
        switch (i->state) {
        case ICY_DATA: {
            size_t budget = i->metaint - i->data_read;
            size_t take = MPMIN(len - pos, budget);
            write_cb(ctx, buf + pos, take);
            pos += take;
            i->data_read += take;
            if (i->data_read == i->metaint)
                i->state = ICY_LEN;
            break;
        }
        case ICY_LEN: {
            uint8_t n = (uint8_t)buf[pos++];
            if (n == 0) {
                i->state = ICY_DATA;
                i->data_read = 0;
            } else {
                i->meta_pending = (size_t)n * 16;
                i->meta_pos = 0;
                i->state = ICY_META;
            }
            break;
        }
        case ICY_META: {
            size_t take = MPMIN(len - pos, i->meta_pending);
            memcpy(i->meta_buf + i->meta_pos, buf + pos, take);
            i->meta_pos += take;
            i->meta_pending -= take;
            pos += take;
            if (i->meta_pending == 0) {
                i->meta_buf[i->meta_pos] = '\0';
                i->packet.len = 0;
                bstr_xappend_asprintf(i, &i->packet, "%s\n", i->meta_buf);
                i->dirty = true;
                i->state = ICY_DATA;
                i->data_read = 0;
            }
            break;
        }
        }
    }
}

struct mp_tags *mp_icy_get_metadata(struct mp_icy *i, struct stream *s)
{
    if (!i->dirty)
        return NULL;
    i->dirty = false;
    return mp_parse_icy_metadata(s, i->headers, i->packet);
}

struct mp_tags *mp_parse_icy_metadata(struct stream *s, bstr headers, bstr packet)
{
    if (!headers.len && !packet.len)
        return NULL;

    struct mp_tags *res = talloc_zero(NULL, struct mp_tags);

    while (headers.len) {
        bstr line = bstr_strip_linebreaks(bstr_getline(headers, &headers));
        bstr name, val;
        if (bstr_split_tok(line, ": ", &name, &val))
            mp_tags_set_bstr(res, name, val);
    }

    bstr head = bstr0("StreamTitle='");
    int i = bstr_find(packet, head);
    if (i >= 0) {
        packet = bstr_cut(packet, i + head.len);
        int end = bstr_find(packet, bstr0("\';"));
        if (end >= 0)
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

    return res;
}
