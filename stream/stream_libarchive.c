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

#include "common/common.h"
#include "stream.h"

#include "stream_libarchive.h"

static ssize_t read_cb(struct archive *arch, void *priv, const void **buffer)
{
    struct mp_archive *mpa = priv;
    int res = stream_read_partial(mpa->src, mpa->buffer, sizeof(mpa->buffer));
    *buffer = mpa->buffer;
    return MPMAX(res, 0);
}

static int64_t seek_cb(struct archive *arch, void *priv,
                       int64_t offset, int whence)
{
    struct mp_archive *mpa = priv;
    switch (whence) {
    case SEEK_SET:
        break;
    case SEEK_CUR:
        offset += mpa->src->pos;
        break;
    case SEEK_END: ;
        int64_t size = stream_get_size(mpa->src);
        if (size < 0)
            return -1;
        offset += size;
        break;
    default:
        return -1;
    }
    return stream_seek(mpa->src, offset) ? offset : -1;
}

static int64_t skip_cb(struct archive *arch, void *priv, int64_t request)
{
    struct mp_archive *mpa = priv;
    int64_t old = stream_tell(mpa->src);
    stream_skip(mpa->src, request);
    return stream_tell(mpa->src) - old;
}

void mp_archive_free(struct mp_archive *mpa)
{
    if (mpa && mpa->arch) {
        archive_read_close(mpa->arch);
        archive_read_free(mpa->arch);
    }
    talloc_free(mpa);
}

struct mp_archive *mp_archive_new(struct mp_log *log, struct stream *src,
                                  int flags)
{
    struct mp_archive *mpa = talloc_zero(NULL, struct mp_archive);
    mpa->src = src;
    stream_seek(mpa->src, 0);
    mpa->arch = archive_read_new();
    if (!mpa->arch)
        goto err;

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

    archive_read_set_callback_data(mpa->arch, mpa);
    archive_read_set_read_callback(mpa->arch, read_cb);
    archive_read_set_skip_callback(mpa->arch, skip_cb);
    if (mpa->src->seekable)
        archive_read_set_seek_callback(mpa->arch, seek_cb);
    if (archive_read_open1(mpa->arch) < ARCHIVE_OK)
        goto err;
    return mpa;

err:
    mp_archive_free(mpa);
    return NULL;
}

struct priv {
    struct mp_archive *mpa;
    struct stream *src;
    int64_t entry_size;
    char *entry_name;
};

static int reopen_archive(stream_t *s)
{
    struct priv *p = s->priv;
    mp_archive_free(p->mpa);
    p->mpa = mp_archive_new(s->log, p->src, MP_ARCHIVE_FLAG_UNSAFE);
    if (!p->mpa)
        return STREAM_ERROR;

    // Follows the same logic as demux_libarchive.c.
    struct mp_archive *mpa = p->mpa;
    int num_files = 0;
    for (;;) {
        struct archive_entry *entry;
        int r = archive_read_next_header(mpa->arch, &entry);
        if (r == ARCHIVE_EOF) {
            MP_ERR(s, "archive entry not found. '%s'\n", p->entry_name);
            goto error;
        }
        if (r < ARCHIVE_OK)
            MP_ERR(s, "libarchive: %s\n", archive_error_string(mpa->arch));
        if (r < ARCHIVE_WARN)
            goto error;
        if (archive_entry_filetype(entry) != AE_IFREG)
            continue;
        const char *fn = archive_entry_pathname(entry);
        char buf[64];
        if (!fn) {
            snprintf(buf, sizeof(buf), "mpv_unknown#%d\n", num_files);
            fn = buf;
        }
        if (strcmp(p->entry_name, fn) == 0) {
            p->entry_size = -1;
            if (archive_entry_size_is_set(entry))
                p->entry_size = archive_entry_size(entry);
            return STREAM_OK;
        }
        num_files++;
    }

error:
    mp_archive_free(p->mpa);
    p->mpa = NULL;
    MP_ERR(s, "could not open archive\n");
    return STREAM_ERROR;
}

static int archive_entry_fill_buffer(stream_t *s, char *buffer, int max_len)
{
    struct priv *p = s->priv;
    if (!p->mpa)
        return 0;
    int r = archive_read_data(p->mpa->arch, buffer, max_len);
    if (r < 0)
        MP_ERR(s, "libarchive: %s\n", archive_error_string(p->mpa->arch));
    return r;
}

static int archive_entry_seek(stream_t *s, int64_t newpos)
{
    struct priv *p = s->priv;
    if (!p->mpa)
        return -1;
    if (archive_seek_data(p->mpa->arch, newpos, SEEK_SET) >= 0)
        return 1;
    // libarchive can't seek in most formats.
    if (newpos < s->pos) {
        // Hack seeking backwards into working by reopening the archive and
        // starting over.
        MP_VERBOSE(s, "trying to reopen archive for performing seek\n");
        if (reopen_archive(s) < STREAM_OK)
            return -1;
        s->pos = 0;
    }
    if (newpos > s->pos) {
        // For seeking forwards, just keep reading data (there's no libarchive
        // skip function either).
        char buffer[4096];
        while (newpos > s->pos) {
            int size = MPMIN(newpos - s->pos, sizeof(buffer));
            int r = archive_read_data(p->mpa->arch, buffer, size);
            if (r < 0) {
                MP_ERR(s, "libarchive: %s\n", archive_error_string(p->mpa->arch));
                return -1;
            }
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
    case STREAM_CTRL_GET_BASE_FILENAME:
        *(char **)arg = talloc_strdup(NULL, p->src->url);
        return STREAM_OK;
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

    return STREAM_OK;
}

const stream_info_t stream_info_libarchive = {
    .name = "libarchive",
    .open = archive_entry_open,
    .protocols = (const char*const[]){ "archive", NULL },
};
