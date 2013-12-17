/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <quvi/quvi.h>

#include "talloc.h"
#include "common/msg.h"
#include "options/options.h"
#include "resolve.h"

static void add_source(struct mp_resolve_result *res, const char *url,
                       const char *encid)
{
    struct mp_resolve_src *src = talloc_ptrtype(res, src);
    *src = (struct mp_resolve_src) {
        .url = talloc_strdup(src, url),
        .encid = talloc_strdup(src, encid),
    };
    MP_TARRAY_APPEND(res, res->srcs, res->num_srcs, src);
}

struct mp_resolve_result *mp_resolve_quvi(const char *url, struct MPOpts *opts)
{
    QUVIcode rc;
    bool mp_url = false;

    quvi_t q;
    rc = quvi_init(&q);
    if (rc != QUVI_OK)
        return NULL;

    if (!strncmp(url, "mp_", 3)) {
        url += 3;
        mp_url = true;
    }

    // Don't try to use quvi on an URL that's not directly supported, since
    // quvi will do a network access anyway in order to check for HTTP
    // redirections etc.
    // The documentation says this will fail on "shortened" URLs.
    if (quvi_supported(q, (char *)url) != QUVI_OK) {
        quvi_close(&q);
        return NULL;
    }

    mp_msg(MSGT_OPEN, MSGL_INFO, "[quvi] Checking URL...\n");

    const char *req_format = opts->quvi_format ? opts->quvi_format : "best";

    // Can use quvi_query_formats() to get a list of formats like this:
    // "fmt05_240p|fmt18_360p|fmt34_360p|fmt35_480p|fmt43_360p|fmt44_480p"
    // (This example is youtube specific.)
    // That call requires an extra net access. quvi_next_media_url() doesn't
    // seem to do anything useful. So we can't really do anything useful
    // except pass through the user's format setting.
    quvi_setopt(q, QUVIOPT_FORMAT, req_format);

    quvi_media_t m;
    rc = quvi_parse(q, (char *)url, &m);
    if (rc != QUVI_OK) {
        mp_msg(MSGT_OPEN, MSGL_ERR, "[quvi] %s\n", quvi_strerror(q, rc));
        quvi_close(&q);
        return NULL;
    }

    struct mp_resolve_result *result
        = talloc_zero(NULL, struct mp_resolve_result);

    char *val;

    if (quvi_getprop(m, QUVIPROP_MEDIAURL, &val) == QUVI_OK) {
        if (mp_url)
            result->url = talloc_asprintf(result, "mp_%s", val);
        else
            result->url = talloc_strdup(result, val);
    }

    if (quvi_getprop(m, QUVIPROP_PAGETITLE, &val) == QUVI_OK)
        result->title = talloc_strdup(result, val);

    quvi_parse_close(&m);
    quvi_close(&q);

    if (!result->url) {
        talloc_free(result);
        result = NULL;
    }

    // Useful for quvi-format cycling
    add_source(result, NULL, "default");
    add_source(result, NULL, "best");
    if (strcmp(req_format, "best") != 0 && strcmp(req_format, "default") != 0)
        add_source(result, NULL, req_format);

    return result;
}
