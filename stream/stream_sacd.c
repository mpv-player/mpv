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

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "osdep/io.h"

#include "common/common.h"
#include "common/msg.h"
#include "misc/path_utils.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/path.h"
#include "stream.h"

#define SECTOR_SIZE 2048

struct sacd_opts {
    char *device;
};

#define OPT_BASE_STRUCT struct sacd_opts
const struct m_sub_options stream_sacd_conf = {
    .opts = (const m_option_t[]) {
        {"device", OPT_STRING(device), .flags = M_OPT_FILE},
        {0}
    },
    .size = sizeof(struct sacd_opts),
};

struct priv {
    int fd;
    uint8_t sector[SECTOR_SIZE];
};

static int read_at(int fd, void *buffer, int64_t pos, int len)
{
    if (lseek(fd, pos, SEEK_SET) == (off_t)-1)
        return -1;
    return read(fd, buffer, len);
}

static int fill_buffer(stream_t *s, void *buffer, int max_len)
{
    struct priv *p = s->priv;
    int offset = s->pos % SECTOR_SIZE;

    // A raw device only reads whole sectors at sector offsets.
    if (!offset && max_len >= SECTOR_SIZE)
        return read_at(p->fd, buffer, s->pos, max_len - max_len % SECTOR_SIZE);

    int len = read_at(p->fd, p->sector, s->pos - offset, SECTOR_SIZE) - offset;
    if (len > 0) {
        len = MPMIN(len, max_len);
        memcpy(buffer, p->sector + offset, len);
    }
    return len;
}

// fill_buffer() reads at the position of the stream.
static int seek(stream_t *s, int64_t newpos)
{
    return 1;
}

static int64_t get_size(stream_t *s)
{
    struct priv *p = s->priv;
    return lseek(p->fd, 0, SEEK_END);
}

static void close_sacd(stream_t *s)
{
    struct priv *p = s->priv;
    close(p->fd);
}

static void init_stream(stream_t *st, int fd)
{
    struct priv *p = talloc_zero(st, struct priv);
    p->fd = fd;

    st->priv = p;
    st->fill_buffer = fill_buffer;
    st->seek = seek;
    st->seekable = true;
    st->get_size = get_size;
    st->close = close_sacd;

    st->demuxer = "lavf";
    st->lavf_type = "sacd";
}

static int open_sacd(stream_t *st)
{
    struct sacd_opts *opts = mp_get_config_group(st, st->global, &stream_sacd_conf);

    char *device = DEFAULT_OPTICAL_DEVICE;
    if (st->path[0]) {
        device = st->path;
    } else if (opts->device && opts->device[0]) {
        device = mp_get_user_path(st, st->global, opts->device);
    }

#ifdef _WIN32
    // Convert the device path to a volume, for raw sector access.
    wchar_t volume[50]; // \\?\Volume{GUID}\ is 49 characters
    char *root = mp_normalize_path(st, talloc_asprintf(st, "%s\\", device));
    if (GetVolumeNameForVolumeMountPointW(mp_from_utf8(st, root), volume, MP_ARRAY_SIZE(volume))) {
        volume[wcslen(volume) - 1] = L'\0';
        device = mp_to_utf8(st, volume);
    }
#endif

    int fd = open(device, O_RDONLY | O_BINARY | O_CLOEXEC);
    if (fd < 0) {
        MP_ERR(st, "Cannot open '%s': %s\n", device, mp_strerror(errno));
        return STREAM_ERROR;
    }

    init_stream(st, fd);
    return STREAM_OK;
}

const stream_info_t stream_info_sacd = {
    .name = "sacd",
    .open = open_sacd,
    .protocols = (const char*const[]){ "sacd", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};
