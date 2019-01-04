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

#include <archive.h>
#include <archive_entry.h>

#include "misc/bstr.h"
#include "common/common.h"
#include "misc/thread_tools.h"
#include "stream.h"

#include "stream_libarchive.h"

struct mp_archive_volume {
    struct mp_archive *mpa;
    struct stream *src;
    int64_t seek_to;
    char *url;
};

static bool volume_seek(struct mp_archive_volume *vol)
{
    if (vol->seek_to < 0)
        return true;
    bool r = stream_seek(vol->src, vol->seek_to);
    vol->seek_to = -1;
    return r;
}

static ssize_t read_cb(struct archive *arch, void *priv, const void **buffer)
{
    struct mp_archive_volume *vol = priv;
    if (!volume_seek(vol))
        return -1;
    int res = stream_read_partial(vol->src, vol->mpa->buffer,
                                  sizeof(vol->mpa->buffer));
    *buffer = vol->mpa->buffer;
    return MPMAX(res, 0);
}

// lazy seek to avoid problems with end seeking over http
static int64_t seek_cb(struct archive *arch, void *priv,
                       int64_t offset, int whence)
{
    struct mp_archive_volume *vol = priv;
    switch (whence) {
    case SEEK_SET:
        vol->seek_to = offset;
        break;
    case SEEK_CUR:
        if (vol->seek_to < 0)
            vol->seek_to = stream_tell(vol->src);
        vol->seek_to += offset;
        break;
    case SEEK_END: ;
        int64_t size = stream_get_size(vol->src);
        if (size < 0)
            return -1;
        vol->seek_to = size + offset;
        break;
    default:
        return -1;
    }
    return vol->seek_to;
}

static int64_t skip_cb(struct archive *arch, void *priv, int64_t request)
{
    struct mp_archive_volume *vol = priv;
    if (!volume_seek(vol))
        return -1;
    int64_t old = stream_tell(vol->src);
    stream_skip(vol->src, request);
    return stream_tell(vol->src) - old;
}

static int open_cb(struct archive *arch, void *priv)
{
    struct mp_archive_volume *vol = priv;
    vol->seek_to = -1;
    if (!vol->src) {
        vol->src = stream_create(vol->url, STREAM_READ,
                                 vol->mpa->primary_src->cancel,
                                 vol->mpa->primary_src->global);
        return vol->src ? ARCHIVE_OK : ARCHIVE_FATAL;
    }

    // just rewind the primary stream
    return stream_seek(vol->src, 0) ? ARCHIVE_OK : ARCHIVE_FATAL;
}

static void volume_close(struct mp_archive_volume *vol)
{
    // don't close the primary stream
    if (vol->src && vol->src != vol->mpa->primary_src) {
        free_stream(vol->src);
        vol->src = NULL;
    }
}

static int close_cb(struct archive *arch, void *priv)
{
    struct mp_archive_volume *vol = priv;
    volume_close(vol);
    talloc_free(vol);
    return ARCHIVE_OK;
}

static int switch_cb(struct archive *arch, void *oldpriv, void *newpriv)
{
    struct mp_archive_volume *oldvol = oldpriv;
    volume_close(oldvol);
    return open_cb(arch, newpriv);
}

static void mp_archive_close(struct mp_archive *mpa)
{
    if (mpa && mpa->arch) {
        archive_read_close(mpa->arch);
        archive_read_free(mpa->arch);
        mpa->arch = NULL;
    }
}

// Supposedly we're not allowed to continue reading on FATAL returns. Otherwise
// crashes and other UB is possible. Assume calling the close/free functions is
// still ok. Return true if it was fatal and the archive was closed.
static bool mp_archive_check_fatal(struct mp_archive *mpa, int r)
{
    if (r > ARCHIVE_FATAL)
        return false;
    MP_FATAL(mpa, "fatal error received - closing archive\n");
    mp_archive_close(mpa);
    return true;
}

void mp_archive_free(struct mp_archive *mpa)
{
    mp_archive_close(mpa);
    if (mpa && mpa->locale)
        freelocale(mpa->locale);
    talloc_free(mpa);
}

static char *standard_volume_url(void *ctx, const char *format,
                                 struct bstr base, int index)
{
    return talloc_asprintf(ctx, format, BSTR_P(base), index);
}

static char *old_rar_volume_url(void *ctx, const char *format,
                                struct bstr base, int index)
{
    return talloc_asprintf(ctx, format, BSTR_P(base),
                           'r' + index / 100, index % 100);
}

struct file_pattern {
    const char *match;
    const char *format;
    char *(*volume_url)(void *ctx, const char *format,
                        struct bstr base, int index);
    int start;
    int stop;
};

static const struct file_pattern patterns[] = {
    { ".part1.rar",    "%.*s.part%.1d.rar", standard_volume_url, 2,    9 },
    { ".part01.rar",   "%.*s.part%.2d.rar", standard_volume_url, 2,   99 },
    { ".part001.rar",  "%.*s.part%.3d.rar", standard_volume_url, 2,  999 },
    { ".part0001.rar", "%.*s.part%.4d.rar", standard_volume_url, 2, 9999 },
    { ".rar",          "%.*s.%c%.2d",       old_rar_volume_url,  0, 9999 },
    { ".001",          "%.*s.%.3d",         standard_volume_url, 2, 9999 },
    { NULL, NULL, NULL, 0, 0 },
};

static char **find_volumes(struct stream *primary_stream)
{
    char **res = talloc_new(NULL);
    int    num = 0;
    struct bstr primary_url = bstr0(primary_stream->url);

    const struct file_pattern *pattern = patterns;
    while (pattern->match) {
        if (bstr_endswith0(primary_url, pattern->match))
            break;
        pattern++;
    }

    if (!pattern->match)
        goto done;

    struct bstr base = bstr_splice(primary_url, 0, -strlen(pattern->match));
    for (int i = pattern->start; i <= pattern->stop; i++) {
        char* url = pattern->volume_url(res, pattern->format, base, i);
        struct stream *s = stream_create(url, STREAM_READ | STREAM_SAFE_ONLY,
                                         primary_stream->cancel,
                                         primary_stream->global);
        if (!s) {
            talloc_free(url);
            goto done;
        }
        free_stream(s);
        MP_TARRAY_APPEND(res, res, num, url);
    }

done:
    MP_TARRAY_APPEND(res, res, num, NULL);
    return res;
}


static bool add_volume(struct mp_log *log, struct mp_archive *mpa,
                       struct stream *src, const char* url)
{
    struct mp_archive_volume *vol = talloc_zero(mpa, struct mp_archive_volume);
    mp_verbose(log, "Adding volume %s\n", url);
    vol->mpa = mpa;
    vol->src = src;
    vol->url = talloc_strdup(vol, url);
    locale_t oldlocale = uselocale(mpa->locale);
    bool res = archive_read_append_callback_data(mpa->arch, vol) == ARCHIVE_OK;
    uselocale(oldlocale);
    return res;
}

struct mp_archive *mp_archive_new(struct mp_log *log, struct stream *src,
                                  int flags)
{
    struct mp_archive *mpa = talloc_zero(NULL, struct mp_archive);
    mpa->log = log;
    mpa->locale = newlocale(LC_ALL_MASK, "C.UTF-8", (locale_t)0);
    if (!mpa->locale)
        goto err;
    mpa->arch = archive_read_new();
    mpa->primary_src = src;
    if (!mpa->arch)
        goto err;

    // first volume is the primary streame
    if (!add_volume(log ,mpa, src, src->url))
        goto err;

    // try to open other volumes
    char** volumes = find_volumes(src);
    for (int i = 0; volumes[i]; i++) {
        if (!add_volume(log, mpa, NULL, volumes[i])) {
            talloc_free(volumes);
            goto err;
        }
    }
    talloc_free(volumes);

    locale_t oldlocale = uselocale(mpa->locale);

    archive_read_support_format_7zip(mpa->arch);
    archive_read_support_format_iso9660(mpa->arch);
    archive_read_support_format_rar(mpa->arch);
    archive_read_support_format_zip(mpa->arch);
    archive_read_support_filter_bzip2(mpa->arch);
    archive_read_support_filter_gzip(mpa->arch);
    archive_read_support_filter_xz(mpa->arch);
    if (flags & MP_ARCHIVE_FLAG_UNSAFE) {
        archive_read_support_format_gnutar(mpa->arch);
        archive_read_support_format_tar(mpa->arch);
    }

    archive_read_set_read_callback(mpa->arch, read_cb);
    archive_read_set_skip_callback(mpa->arch, skip_cb);
    archive_read_set_switch_callback(mpa->arch, switch_cb);
    archive_read_set_open_callback(mpa->arch, open_cb);
    archive_read_set_close_callback(mpa->arch, close_cb);
    if (mpa->primary_src->seekable)
        archive_read_set_seek_callback(mpa->arch, seek_cb);
    bool fail = archive_read_open1(mpa->arch) < ARCHIVE_OK;

    uselocale(oldlocale);

    if (fail)
        goto err;
    return mpa;

err:
    mp_archive_free(mpa);
    return NULL;
}

// Iterate entries. The first call establishes the first entry. Returns false
// if no entry found, otherwise returns true and sets mpa->entry/entry_filename.
bool mp_archive_next_entry(struct mp_archive *mpa)
{
    mpa->entry = NULL;
    talloc_free(mpa->entry_filename);
    mpa->entry_filename = NULL;

    if (!mpa->arch)
        return false;

    locale_t oldlocale = uselocale(mpa->locale);
    bool success = false;

    while (!mp_cancel_test(mpa->primary_src->cancel)) {
        struct archive_entry *entry;
        int r = archive_read_next_header(mpa->arch, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r < ARCHIVE_OK)
            MP_ERR(mpa, "%s\n", archive_error_string(mpa->arch));
        if (r < ARCHIVE_WARN) {
            MP_FATAL(mpa, "could not read archive entry\n");
            mp_archive_check_fatal(mpa, r);
            break;
        }
        if (archive_entry_filetype(entry) != AE_IFREG)
            continue;
        // Some archives may have no filenames, or libarchive won't return some.
        const char *fn = archive_entry_pathname(entry);
        char buf[64];
        if (!fn || bstr_validate_utf8(bstr0(fn)) < 0) {
            snprintf(buf, sizeof(buf), "mpv_unknown#%d", mpa->entry_num);
            fn = buf;
        }
        mpa->entry = entry;
        mpa->entry_filename = talloc_strdup(mpa, fn);
        mpa->entry_num += 1;
        success = true;
        break;
    }

    uselocale(oldlocale);

    return success;
}

struct priv {
    struct mp_archive *mpa;
    bool broken_seek;
    struct stream *src;
    int64_t entry_size;
    char *entry_name;
};

static int reopen_archive(stream_t *s)
{
    struct priv *p = s->priv;
    mp_archive_free(p->mpa);
    s->pos = 0;
    p->mpa = mp_archive_new(s->log, p->src, MP_ARCHIVE_FLAG_UNSAFE);
    if (!p->mpa)
        return STREAM_ERROR;

    // Follows the same logic as demux_libarchive.c.
    struct mp_archive *mpa = p->mpa;
    while (mp_archive_next_entry(mpa)) {
        if (strcmp(p->entry_name, mpa->entry_filename) == 0) {
            locale_t oldlocale = uselocale(mpa->locale);
            p->entry_size = -1;
            if (archive_entry_size_is_set(mpa->entry))
                p->entry_size = archive_entry_size(mpa->entry);
            uselocale(oldlocale);
            return STREAM_OK;
        }
    }

    mp_archive_free(p->mpa);
    p->mpa = NULL;
    MP_ERR(s, "archive entry not found. '%s'\n", p->entry_name);
    return STREAM_ERROR;
}

static int archive_entry_fill_buffer(stream_t *s, char *buffer, int max_len)
{
    struct priv *p = s->priv;
    if (!p->mpa)
        return 0;
    locale_t oldlocale = uselocale(p->mpa->locale);
    int r = archive_read_data(p->mpa->arch, buffer, max_len);
    if (r < 0) {
        MP_ERR(s, "%s\n", archive_error_string(p->mpa->arch));
        if (mp_archive_check_fatal(p->mpa, r)) {
            mp_archive_free(p->mpa);
            p->mpa = NULL;
        }
    }
    uselocale(oldlocale);
    return r;
}

static int archive_entry_seek(stream_t *s, int64_t newpos)
{
    struct priv *p = s->priv;
    if (p->mpa && !p->broken_seek) {
        locale_t oldlocale = uselocale(p->mpa->locale);
        int r = archive_seek_data(p->mpa->arch, newpos, SEEK_SET);
        uselocale(oldlocale);
        if (r >= 0)
            return 1;
        MP_WARN(s, "possibly unsupported seeking - switching to reopening\n");
        p->broken_seek = true;
        if (reopen_archive(s) < STREAM_OK)
            return -1;
    }
    // libarchive can't seek in most formats.
    if (newpos < s->pos) {
        // Hack seeking backwards into working by reopening the archive and
        // starting over.
        MP_VERBOSE(s, "trying to reopen archive for performing seek\n");
        if (reopen_archive(s) < STREAM_OK)
            return -1;
    }
    if (newpos > s->pos) {
        if (!p->mpa && reopen_archive(s) < STREAM_OK)
            return -1;
        // For seeking forwards, just keep reading data (there's no libarchive
        // skip function either).
        char buffer[4096];
        while (newpos > s->pos) {
            if (mp_cancel_test(s->cancel))
                return -1;

            int size = MPMIN(newpos - s->pos, sizeof(buffer));
            locale_t oldlocale = uselocale(p->mpa->locale);
            int r = archive_read_data(p->mpa->arch, buffer, size);
            if (r <= 0) {
                if (r == 0 && newpos > p->entry_size) {
                    MP_ERR(s, "demuxer trying to seek beyond end of archive "
                           "entry\n");
                } else if (r == 0) {
                    MP_ERR(s, "end of archive entry reached while seeking\n");
                } else {
                    MP_ERR(s, "%s\n", archive_error_string(p->mpa->arch));
                }
                uselocale(oldlocale);
                if (mp_archive_check_fatal(p->mpa, r)) {
                    mp_archive_free(p->mpa);
                    p->mpa = NULL;
                }
                return -1;
            }
            uselocale(oldlocale);
            s->pos += r;
        }
    }
    return 1;
}

static void archive_entry_close(stream_t *s)
{
    struct priv *p = s->priv;
    mp_archive_free(p->mpa);
    free_stream(p->src);
}

static int archive_entry_control(stream_t *s, int cmd, void *arg)
{
    struct priv *p = s->priv;
    switch (cmd) {
    case STREAM_CTRL_GET_SIZE:
        if (p->entry_size < 0)
            break;
        *(int64_t *)arg = p->entry_size;
        return STREAM_OK;
    }
    return STREAM_UNSUPPORTED;
}

static int archive_entry_open(stream_t *stream)
{
    struct priv *p = talloc_zero(stream, struct priv);
    stream->priv = p;

    if (!strchr(stream->path, '|'))
        return STREAM_ERROR;

    char *base = talloc_strdup(p, stream->path);
    char *name = strchr(base, '|');
    *name++ = '\0';
    p->entry_name = name;
    mp_url_unescape_inplace(base);

    p->src = stream_create(base, STREAM_READ | STREAM_SAFE_ONLY,
                           stream->cancel, stream->global);
    if (!p->src) {
        archive_entry_close(stream);
        return STREAM_ERROR;
    }

    int r = reopen_archive(stream);
    if (r < STREAM_OK) {
        archive_entry_close(stream);
        return r;
    }

    stream->fill_buffer = archive_entry_fill_buffer;
    if (p->src->seekable) {
        stream->seek = archive_entry_seek;
        stream->seekable = true;
    }
    stream->close = archive_entry_close;
    stream->control = archive_entry_control;
    stream->streaming = true;

    return STREAM_OK;
}

const stream_info_t stream_info_libarchive = {
    .name = "libarchive",
    .open = archive_entry_open,
    .protocols = (const char*const[]){ "archive", NULL },
};
