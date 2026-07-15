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

#pragma once

#include <stdbool.h>

struct m_sub_options;
struct mp_tags;
struct stream;
typedef struct bstr bstr;

struct mp_network_opts {
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

extern const struct m_sub_options mp_network_conf;

// Build mp_tags from accumulated ICY metadata. `headers` is a buffer of
// "Icy-*: value\n" lines collected from the response. `packet` is the most
// recent in-band metadata payload. Returns NULL when both are empty.
// Returned value has to be freed by the caller.
struct mp_tags *mp_parse_icy_metadata(struct stream *s, bstr headers,
                                      bstr packet);

// Opaque state for receiving ICY (Shoutcast/Icecast) metadata over an HTTP
// stream. This wrapper is not thread safe.
struct mp_icy;

// Allocate a new ICY context as a talloc child of `ta_parent`.
struct mp_icy *mp_icy_new(void *ta_parent);

// Reset all ICY state. Use between responses (e.g. across redirects).
void mp_icy_reset(struct mp_icy *icy);

// Feed a single response header line (without trailing CRLF). Lines that
// don't start with "Icy-" are silently ignored.
void mp_icy_add_header(struct mp_icy *icy, bstr line);

// True if Icy-MetaInt was seen and the body must be filtered.
bool mp_icy_active(const struct mp_icy *icy);

// Body callback used by mp_icy_process(). Invoked once per contiguous
// stretch of non-metadata bytes.
typedef void (*mp_icy_write_fn)(void *ctx, const char *data, size_t len);

// Process a body chunk. When ICY is active, metadata bytes are stripped and
// stashed internally, remaining data is delivered via `write_cb`. Otherwise
// the chunk is forwarded as a single call to `write_cb`.
void mp_icy_process(struct mp_icy *icy, const char *buf, size_t len,
                    mp_icy_write_fn write_cb, void *ctx);

// Returns talloc-allocated mp_tags.
struct mp_tags *mp_icy_get_metadata(struct mp_icy *icy, struct stream *s);
