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

#define MP_ARCHIVE_FLAG_MAYBE_ZIP       (MP_ARCHIVE_FLAG_PRIV << 0)
#define MP_ARCHIVE_FLAG_MAYBE_RAR       (MP_ARCHIVE_FLAG_PRIV << 1)
#define MP_ARCHIVE_FLAG_MAYBE_VOLUMES   (MP_ARCHIVE_FLAG_PRIV << 2)

struct mp_archive_volume {
    struct mp_archive *mpa;
    int index; // volume number (starting with 0, mp_archive.primary_src)
    struct stream *src; // NULL => not current volume, or 0 sized dummy stream
    int64_t seek_to;
    char *url;
};

static bool probe_rar(struct stream *s)
{
    static uint8_t rar_sig[] = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07};
    uint8_t buf[6];
    if (stream_read_peek(s, buf, sizeof(buf)) != sizeof(buf))
        return false;
    return memcmp(buf, rar_sig, 6) == 0;
}

static bool probe_multi_rar(struct stream *s)
{
    uint8_t hdr[14];
    if (stream_read_peek(s, hdr, sizeof(hdr)) == sizeof(hdr)) {
        // Look for rar mark head & main head (assume they're in order).
        if (hdr[6] == 0x00 && hdr[7 + 2] == 0x73) {
            int rflags = hdr[7 + 3] | (hdr[7 + 4] << 8);
            return rflags & 0x100;
        }
    }
    return false;
}

static bool probe_zip(struct stream *s)
{
    uint8_t p[4];
    if (stream_read_peek(s, p, sizeof(p)) != sizeof(p))
        return false;
    // Lifted from libarchive, BSD license.
    if (p[0] == 'P' && p[1] == 'K') {
        if ((p[2] == '\001' && p[3] == '\002') ||
            (p[2] == '\003' && p[3] == '\004') ||
            (p[2] == '\005' && p[3] == '\006') ||
            (p[2] == '\006' && p[3] == '\006') ||
            (p[2] == '\007' && p[3] == '\010') ||
            (p[2] == '0' && p[3] == '0'))
            return true;
    }
    return false;
}

static int mp_archive_probe(struct stream *src)
{
    int flags = 0;
    assert(stream_tell(src) == 0);
    if (probe_zip(src))
        flags |= MP_ARCHIVE_FLAG_MAYBE_ZIP;

    if (probe_rar(src)) {
        flags |= MP_ARCHIVE_FLAG_MAYBE_RAR;
        if (probe_multi_rar(src))
            flags |= MP_ARCHIVE_FLAG_MAYBE_VOLUMES;
    }
    return flags;
}

static bool volume_seek(struct mp_archive_volume *vol)
{
    if (!vol->src || vol->seek_to < 0)
        return true;
    bool r = stream_seek(vol->src, vol->seek_to);
    vol->seek_to = -1;
    return r;
}

static ssize_t read_cb(struct archive *arch, void *priv, const void **buffer)
{
    struct mp_archive_volume *vol = priv;
    if (!vol->src)
        return 0;
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
    if (!vol->src)
        return 0;
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
    if (!vol->src)
        return request;
    if (!volume_seek(vol))
        return -1;
    int64_t old = stream_tell(vol->src);
    stream_seek_skip(vol->src, old + request);
    return stream_tell(vol->src) - old;
}

static int open_cb(struct archive *arch, void *priv)
{
    struct mp_archive_volume *vol = priv;
    vol->seek_to = -1;
    if (!vol->src) {
        // Avoid annoying warnings/latency for known dummy volumes.
        if (vol->index >= vol->mpa->num_volumes)
            return ARCHIVE_OK;
        MP_INFO(vol->mpa, "Opening volume '%s'...\n", vol->url);
        vol->src = stream_create(vol->url,
                                 STREAM_READ |
                                    vol->mpa->primary_src->stream_origin,
                                 vol->mpa->primary_src->cancel,
                                 vol->mpa->primary_src->global);
        // We pretend that failure to open a stream means it was not found,
        // we assume in turn means that the volume doesn't exist (since
        // libarchive builds volumes as some sort of abstraction on top of its
        // stream layer, and its rar code cannot access volumes or signal
        // anything related to this). libarchive also encounters a fatal error
        // when a volume could not be opened. However, due to the way volume
        // support works, it is fine with 0-sized volumes, which we simulate
        // whenever vol->src==NULL for an opened volume.
        if (!vol->src) {
            vol->mpa->num_volumes = MPMIN(vol->mpa->num_volumes, vol->index);
            MP_INFO(vol->mpa, "Assuming the volume above was not needed.\n");
        }
        return ARCHIVE_OK;
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
    return ARCHIVE_OK;
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

static bool add_volume(struct mp_archive *mpa, struct stream *src,
                       const char* url, int index)
{
    struct mp_archive_volume *vol = talloc_zero(mpa, struct mp_archive_volume);
    vol->index = index;
    vol->mpa = mpa;
    vol->src = src;
    vol->url = talloc_strdup(vol, url);
    locale_t oldlocale = uselocale(mpa->locale);
    bool res = archive_read_append_callback_data(mpa->arch, vol) == ARCHIVE_OK;
    uselocale(oldlocale);
    return res;
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
    bool legacy;
};

static const struct file_pattern patterns[] = {
    { ".part1.rar",    "%.*s.part%.1d.rar", standard_volume_url, 2,    9 },
    { ".part01.rar",   "%.*s.part%.2d.rar", standard_volume_url, 2,   99 },
    { ".part001.rar",  "%.*s.part%.3d.rar", standard_volume_url, 2,  999 },
    { ".part0001.rar", "%.*s.part%.4d.rar", standard_volume_url, 2, 9999 },
    { ".rar",          "%.*s.%c%.2d",       old_rar_volume_url,  0,   99, true },
    { ".001",          "%.*s.%.3d",         standard_volume_url, 2, 9999 },
    { NULL, NULL, NULL, 0, 0 },
};

static bool find_volumes(struct mp_archive *mpa, int flags)
{
    struct bstr primary_url = bstr0(mpa->primary_src->url);

    const struct file_pattern *pattern = patterns;
    while (pattern->match) {
        if (bstr_endswith0(primary_url, pattern->match))
            break;
        pattern++;
    }

    if (!pattern->match)
        return true;
    if (pattern->legacy && !(flags & MP_ARCHIVE_FLAG_MAYBE_VOLUMES))
        return true;

    struct bstr base = bstr_splice(primary_url, 0, -(int)strlen(pattern->match));
    for (int i = pattern->start; i <= pattern->stop; i++) {
        char* url = pattern->volume_url(mpa, pattern->format, base, i);

        if (!add_volume(mpa, NULL, url, i + 1))
            return false;
    }

    MP_WARN(mpa, "This appears to be a multi-volume archive.\n"
            "Support is not very good due to libarchive limitations.\n"
            "There are known cases of libarchive crashing mpv on these.\n"
            "This is also an excessively inefficient and stupid way to distribute\n"
            "media files. People creating them should rethink this.\n");

    return true;
}

static struct mp_archive *mp_archive_new_raw(struct mp_log *log,
                                             struct stream *src,
                                             int flags, int max_volumes)
{
    struct mp_archive *mpa = talloc_zero(NULL, struct mp_archive);
    mpa->log = log;
    mpa->locale = newlocale(LC_CTYPE_MASK, "C.UTF-8", (locale_t)0);
    if (!mpa->locale) {
        mpa->locale = newlocale(LC_CTYPE_MASK, "", (locale_t)0);
        if (!mpa->locale)
            goto err;
    }
    mpa->arch = archive_read_new();
    mpa->primary_src = src;
    if (!mpa->arch)
        goto err;

    mpa->flags = flags;
    mpa->num_volumes = max_volumes ? max_volumes : INT_MAX;

    // first volume is the primary stream
    if (!add_volume(mpa, src, src->url, 0))
        goto err;

    if (!(flags & MP_ARCHIVE_FLAG_NO_VOLUMES)) {
        // try to open other volumes
        if (!find_volumes(mpa, flags))
            goto err;
    }

    locale_t oldlocale = uselocale(mpa->locale);

    archive_read_support_format_rar(mpa->arch);
    archive_read_support_format_rar5(mpa->arch);

    // Exclude other formats if it's probably RAR, because other formats may
    // behave suboptimal with multiple volumes exposed, such as opening every
    // single volume by seeking at the end of the file.
    if (!(flags & MP_ARCHIVE_FLAG_MAYBE_RAR)) {
        archive_read_support_format_7zip(mpa->arch);
        archive_read_support_format_iso9660(mpa->arch);
        archive_read_support_filter_bzip2(mpa->arch);
        archive_read_support_filter_gzip(mpa->arch);
        archive_read_support_filter_xz(mpa->arch);
        archive_read_support_format_zip_streamable(mpa->arch);

        // This zip reader is normally preferable. However, it seeks to the end
        // of the file, which may be annoying (HTTP reconnect, volume skipping),
        // so use it only as last resort, or if it's relatively likely that it's
        // really zip.
        if (flags & (MP_ARCHIVE_FLAG_UNSAFE | MP_ARCHIVE_FLAG_MAYBE_ZIP))
            archive_read_support_format_zip_seekable(mpa->arch);
    }

    archive_read_set_read_callback(mpa->arch, read_cb);
    archive_read_set_skip_callback(mpa->arch, skip_cb);
    archive_read_set_open_callback(mpa->arch, open_cb);
    // Allow it to close a volume.
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

struct mp_archive *mp_archive_new(struct mp_log *log, struct stream *src,
                                  int flags, int max_volumes)
{
    flags |= mp_archive_probe(src);
    return mp_archive_new_raw(log, src, flags, max_volumes);
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
    s->pos = 0;
    if (!p->mpa) {
        p->mpa = mp_archive_new(s->log, p->src, MP_ARCHIVE_FLAG_UNSAFE, 0);
    } else {
        int flags = p->mpa->flags;
        int num_volumes = p->mpa->num_volumes;
        mp_archive_free(p->mpa);
        p->mpa = mp_archive_new_raw(s->log, p->src, flags, num_volumes);
    }

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

static int archive_entry_fill_buffer(stream_t *s, void *buffer, int max_len)
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

static int64_t archive_entry_get_size(stream_t *s)
{
    struct priv *p = s->priv;
    return p->entry_size;
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
    if (name[0] == '/')
        name += 1;
    p->entry_name = name;
    mp_url_unescape_inplace(base);

    p->src = stream_create(base, STREAM_READ | stream->stream_origin,
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
    stream->get_size = archive_entry_get_size;
    stream->streaming = true;

    return STREAM_OK;
}

const stream_info_t stream_info_libarchive = {
    .name = "libarchive",
    .open = archive_entry_open,
    .protocols = (const char*const[]){ "archive", NULL },
};
