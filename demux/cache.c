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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cache.h"
#include "common/msg.h"
#include "common/av_common.h"
#include "demux.h"
#include "misc/io_utils.h"
#include "options/path.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "osdep/io.h"

struct demux_cache_opts {
    char *cache_dir;
    int unlink_files;
};

#define OPT_BASE_STRUCT struct demux_cache_opts

const struct m_sub_options demux_cache_conf = {
    .opts = (const struct m_option[]){
        {"demuxer-cache-dir", OPT_STRING(cache_dir), .flags = M_OPT_FILE},
        {"demuxer-cache-unlink-files", OPT_CHOICE(unlink_files,
            {"immediate", 2}, {"whendone", 1}, {"no", 0}),
        },
        {"cache-dir", OPT_REPLACED("demuxer-cache-dir")},
        {"cache-unlink-files", OPT_REPLACED("demuxer-cache-unlink-files")},
        {0}
    },
    .size = sizeof(struct demux_cache_opts),
    .defaults = &(const struct demux_cache_opts){
        .unlink_files = 2,
    },
};

struct demux_cache {
    struct mp_log *log;
    struct demux_cache_opts *opts;

    char *filename;
    bool need_unlink;
    int fd;
    int64_t file_pos;
    uint64_t file_size;
};

struct pkt_header {
    uint32_t data_len;
    uint32_t av_flags;
    uint32_t num_sd;
};

struct sd_header {
    uint32_t av_type;
    uint32_t len;
};

static void cache_destroy(void *p)
{
    struct demux_cache *cache = p;

    if (cache->fd >= 0)
        close(cache->fd);

    if (cache->need_unlink && cache->opts->unlink_files >= 1) {
        if (unlink(cache->filename))
            MP_ERR(cache, "Failed to delete cache temporary file.\n");
    }
}

// Create a cache. This also initializes the cache file from the options. The
// log parameter must stay valid until demux_cache is destroyed.
// Free with talloc_free().
struct demux_cache *demux_cache_create(struct mpv_global *global,
                                       struct mp_log *log)
{
    struct demux_cache *cache = talloc_zero(NULL, struct demux_cache);
    talloc_set_destructor(cache, cache_destroy);
    cache->opts = mp_get_config_group(cache, global, &demux_cache_conf);
    cache->log = log;
    cache->fd = -1;

    char *cache_dir = cache->opts->cache_dir;
    if (cache_dir && cache_dir[0]) {
        cache_dir = mp_get_user_path(NULL, global, cache_dir);
    } else {
        cache_dir = mp_find_user_file(NULL, global, "cache", "");
    }

    if (!cache_dir || !cache_dir[0])
        goto fail;

    mp_mkdirp(cache_dir);
    cache->filename = mp_path_join(cache, cache_dir, "mpv-cache-XXXXXX.dat");
    cache->fd = mp_mkostemps(cache->filename, 4, O_CLOEXEC);
    if (cache->fd < 0) {
        MP_ERR(cache, "Failed to create cache temporary file.\n");
        goto fail;
    }
    cache->need_unlink = true;
    if (cache->opts->unlink_files >= 2) {
        if (unlink(cache->filename)) {
            MP_ERR(cache, "Failed to unlink cache temporary file after creation.\n");
        } else {
            cache->need_unlink = false;
        }
    }

    return cache;
fail:
    talloc_free(cache);
    return NULL;
}

uint64_t demux_cache_get_size(struct demux_cache *cache)
{
    return cache->file_size;
}

static bool do_seek(struct demux_cache *cache, uint64_t pos)
{
    if (cache->file_pos == pos)
        return true;

    off_t res = lseek(cache->fd, pos, SEEK_SET);

    if (res == (off_t)-1) {
        MP_ERR(cache, "Failed to seek in cache file.\n");
        cache->file_pos = -1;
    } else {
        cache->file_pos = res;
    }

    return cache->file_pos >= 0;
}

static bool write_raw(struct demux_cache *cache, void *ptr, size_t len)
{
    ssize_t res = write(cache->fd, ptr, len);

    if (res < 0) {
        MP_ERR(cache, "Failed to write to cache file: %s\n", mp_strerror(errno));
        return false;
    }

    cache->file_pos += res;
    cache->file_size = MPMAX(cache->file_size, cache->file_pos);

    // Should never happen, unless the disk is full, or someone succeeded to
    // trick us to write into a pipe or a socket.
    if (res != len) {
        MP_ERR(cache, "Could not write all data.\n");
        return false;
    }

    return true;
}

static bool read_raw(struct demux_cache *cache, void *ptr, size_t len)
{
    ssize_t res = read(cache->fd, ptr, len);

    if (res < 0) {
        MP_ERR(cache, "Failed to read cache file: %s\n", mp_strerror(errno));
        return false;
    }

    cache->file_pos += res;

    // Should never happen, unless the file was cut short, or someone succeeded
    // to rick us to write into a pipe or a socket.
    if (res != len) {
        MP_ERR(cache, "Could not read all data.\n");
        return false;
    }

    return true;
}

// Serialize a packet to the cache file. Returns the packet position, which can
// be passed to demux_cache_read() to read the packet again.
// Returns a negative value on errors, i.e. writing the file failed.
int64_t demux_cache_write(struct demux_cache *cache, struct demux_packet *dp)
{
    assert(dp->avpacket);

    // AV_PKT_FLAG_TRUSTED usually means there are embedded pointers and such
    // in the packet data. The pointer will become invalid if the packet is
    // unreferenced.
    if (dp->avpacket->flags & AV_PKT_FLAG_TRUSTED) {
        MP_ERR(cache, "Cannot serialize this packet to cache file.\n");
        return -1;
    }

    assert(!dp->is_cached);
    assert(dp->len <= INT32_MAX);
    assert(dp->avpacket->flags >= 0 && dp->avpacket->flags <= INT32_MAX);
    assert(dp->avpacket->side_data_elems >= 0 &&
           dp->avpacket->side_data_elems <= INT32_MAX);

    if (!do_seek(cache, cache->file_size))
        return -1;

    uint64_t pos = cache->file_pos;

    struct pkt_header hd = {
        .data_len  = dp->len,
        .av_flags = dp->avpacket->flags,
        .num_sd = dp->avpacket->side_data_elems,
    };

    if (!write_raw(cache, &hd, sizeof(hd)))
        goto fail;

    if (!write_raw(cache, dp->buffer, dp->len))
        goto fail;

    // The handling of FFmpeg side data requires an extra long comment to
    // explain why this code is fragile and insane.
    // FFmpeg packet side data is per-packet out of band data, that contains
    // further information for the decoder (extra metadata and such), which is
    // not part of the codec itself and thus isn't contained in the packet
    // payload. All types use a flat byte array. The format of this byte array
    // is non-standard and FFmpeg-specific, and depends on the side data type
    // field. The side data type is of course a FFmpeg ABI artifact.
    // In some cases, the format is described as fixed byte layout. In others,
    // it contains a struct, i.e. is bound to FFmpeg ABI. Some newer types make
    // the format explicitly internal (and _not_ part of the ABI), and you need
    // to use separate accessors to turn it into complex data structures.
    // As of now, FFmpeg fortunately adheres to the idea that side data can not
    // contain embedded pointers (due to API rules, but also because they forgot
    // adding a refcount field, and can't change this until they break ABI).
    // We rely on this. We hope that FFmpeg won't silently change their
    // semantics, and add refcounting and embedded pointers. This way we can
    // for example dump the data in a disk cache, even though we can't use the
    // data from another process or if this process is restarted (unless we're
    // absolutely sure the FFmpeg internals didn't change). The data has to be
    // treated as a memory dump.
    for (int n = 0; n < dp->avpacket->side_data_elems; n++) {
        AVPacketSideData *sd = &dp->avpacket->side_data[n];

        assert(sd->size <= INT32_MAX);
        assert(sd->type >= 0 && sd->type <= INT32_MAX);

        struct sd_header sd_hd = {
            .av_type = sd->type,
            .len = sd->size,
        };

        if (!write_raw(cache, &sd_hd, sizeof(sd_hd)))
            goto fail;
        if (!write_raw(cache, sd->data, sd->size))
            goto fail;
    }

    return pos;

fail:
    // Reset file_size (try not to append crap forever).
    do_seek(cache, pos);
    cache->file_size = cache->file_pos;
    return -1;
}

struct demux_packet *demux_cache_read(struct demux_cache *cache, uint64_t pos)
{
    if (!do_seek(cache, pos))
        return NULL;

    struct pkt_header hd;

    if (!read_raw(cache, &hd, sizeof(hd)))
        return NULL;

    struct demux_packet *dp = new_demux_packet(hd.data_len);
    if (!dp)
        goto fail;

    if (!read_raw(cache, dp->buffer, dp->len))
        goto fail;

    dp->avpacket->flags = hd.av_flags;

    for (uint32_t n = 0; n < hd.num_sd; n++) {
        struct sd_header sd_hd;

        if (!read_raw(cache, &sd_hd, sizeof(sd_hd)))
            goto fail;

        if (sd_hd.len > INT_MAX)
            goto fail;

        uint8_t *sd = av_packet_new_side_data(dp->avpacket, sd_hd.av_type,
                                              sd_hd.len);
        if (!sd)
            goto fail;

        if (!read_raw(cache, sd, sd_hd.len))
            goto fail;
    }

    return dp;

fail:
    talloc_free(dp);
    return NULL;
}
