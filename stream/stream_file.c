/*
 * Original authors: Albeu, probably Arpi
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

#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#ifndef __MINGW32__
#include <poll.h>
#endif

#include "osdep/io.h"

#include "common/common.h"
#include "common/msg.h"
#include "misc/thread_tools.h"
#include "stream.h"
#include "options/m_option.h"
#include "options/path.h"

#if HAVE_BSD_FSTATFS
#include <sys/param.h>
#include <sys/mount.h>
#endif

#if HAVE_LINUX_FSTATFS
#include <sys/vfs.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <winternl.h>
#include <io.h>

#ifndef FILE_REMOTE_DEVICE
#define FILE_REMOTE_DEVICE (0x10)
#endif
#endif

struct priv {
    int fd;
    bool close;
    bool use_poll;
    bool regular_file;
    bool appending;
    int64_t orig_size;
    struct mp_cancel *cancel;
};

// Total timeout = RETRY_TIMEOUT * MAX_RETRIES
#define RETRY_TIMEOUT 0.2
#define MAX_RETRIES 10

static int64_t get_size(stream_t *s)
{
    struct priv *p = s->priv;
    off_t size = lseek(p->fd, 0, SEEK_END);
    lseek(p->fd, s->pos, SEEK_SET);
    return size == (off_t)-1 ? -1 : size;
}

static int fill_buffer(stream_t *s, char *buffer, int max_len)
{
    struct priv *p = s->priv;

#ifndef __MINGW32__
    if (p->use_poll) {
        int c = mp_cancel_get_fd(p->cancel);
        struct pollfd fds[2] = {
            {.fd = p->fd, .events = POLLIN},
            {.fd = c, .events = POLLIN},
        };
        poll(fds, c >= 0 ? 2 : 1, -1);
        if (fds[1].revents & POLLIN)
            return -1;
    }
#endif

    for (int retries = 0; retries < MAX_RETRIES; retries++) {
        int r = read(p->fd, buffer, max_len);
        if (r > 0)
            return r;

        // Try to detect and handle files being appended during playback.
        int64_t size = get_size(s);
        if (p->regular_file && size > p->orig_size && !p->appending) {
            MP_WARN(s, "File is apparently being appended to, will keep "
                    "retrying with timeouts.\n");
            p->appending = true;
        }

        if (!p->appending || p->use_poll)
            break;

        if (mp_cancel_wait(p->cancel, RETRY_TIMEOUT))
            break;
    }

    return 0;
}

static int write_buffer(stream_t *s, char *buffer, int len)
{
    struct priv *p = s->priv;
    int r = len;
    int wr;
    while (r > 0) {
        wr = write(p->fd, buffer, r);
        if (wr <= 0)
            return -1;
        r -= wr;
        buffer += wr;
    }
    return len - r;
}

static int seek(stream_t *s, int64_t newpos)
{
    struct priv *p = s->priv;
    return lseek(p->fd, newpos, SEEK_SET) != (off_t)-1;
}

static int control(stream_t *s, int cmd, void *arg)
{
    switch (cmd) {
    case STREAM_CTRL_GET_SIZE: {
        int64_t size = get_size(s);
        if (size >= 0) {
            *(int64_t *)arg = size;
            return 1;
        }
        break;
    }
    }
    return STREAM_UNSUPPORTED;
}

static void s_close(stream_t *s)
{
    struct priv *p = s->priv;
    if (p->close)
        close(p->fd);
    talloc_free(p->cancel);
}

// If url is a file:// URL, return the local filename, otherwise return NULL.
char *mp_file_url_to_filename(void *talloc_ctx, bstr url)
{
    bstr proto = mp_split_proto(url, &url);
    if (bstrcasecmp0(proto, "file") != 0)
        return NULL;
    char *filename = bstrto0(talloc_ctx, url);
    mp_url_unescape_inplace(filename);
#if HAVE_DOS_PATHS
    // extract '/' from '/x:/path'
    if (filename[0] == '/' && filename[1] && filename[2] == ':')
        memmove(filename, filename + 1, strlen(filename)); // including \0
#endif
    return filename;
}

// Return talloc_strdup's filesystem path if local, otherwise NULL.
// Unlike mp_file_url_to_filename(), doesn't return NULL if already local.
char *mp_file_get_path(void *talloc_ctx, bstr url)
{
    if (mp_split_proto(url, &(bstr){0}).len) {
        return mp_file_url_to_filename(talloc_ctx, url);
    } else {
        return bstrto0(talloc_ctx, url);
    }
}

#if HAVE_BSD_FSTATFS
static bool check_stream_network(int fd)
{
    struct statfs fs;
    const char *stypes[] = { "afpfs", "nfs", "smbfs", "webdav", "osxfusefs",
                             "fuse", "fusefs.sshfs", NULL };
    if (fstatfs(fd, &fs) == 0)
        for (int i=0; stypes[i]; i++)
            if (strcmp(stypes[i], fs.f_fstypename) == 0)
                return true;
    return false;

}
#elif HAVE_LINUX_FSTATFS
static bool check_stream_network(int fd)
{
    struct statfs fs;
    const uint32_t stypes[] = {
        0x5346414F  /*AFS*/,    0x61756673  /*AUFS*/,   0x00C36400  /*CEPH*/,
        0xFF534D42  /*CIFS*/,   0x73757245  /*CODA*/,   0x19830326  /*FHGFS*/,
        0x65735546  /*FUSEBLK*/,0x65735543  /*FUSECTL*/,0x1161970   /*GFS*/,
        0x47504653  /*GPFS*/,   0x6B414653  /*KAFS*/,   0x0BD00BD0  /*LUSTRE*/,
        0x564C      /*NCP*/,    0x6969      /*NFS*/,    0x6E667364  /*NFSD*/,
        0xAAD7AAEA  /*PANFS*/,  0x50495045  /*PIPEFS*/, 0x517B      /*SMB*/,
        0xBEEFDEAD  /*SNFS*/,   0xBACBACBC  /*VMHGFS*/, 0x7461636f  /*OCFS2*/,
        0xFE534D42  /*SMB2*/,   0x61636673  /*ACFS*/,   0x013111A8  /*IBRIX*/,
        0
    };
    if (fstatfs(fd, &fs) == 0) {
        for (int i=0; stypes[i]; i++) {
            if (stypes[i] == fs.f_type)
                return true;
        }
    }
    return false;

}
#elif defined(_WIN32)
static bool check_stream_network(int fd)
{
    NTSTATUS (NTAPI *pNtQueryVolumeInformationFile)(HANDLE,
        PIO_STATUS_BLOCK, PVOID, ULONG, FS_INFORMATION_CLASS) = NULL;

    // NtQueryVolumeInformationFile is an internal Windows function. It has
    // been present since Windows XP, however this code should fail gracefully
    // if it's removed from a future version of Windows.
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    pNtQueryVolumeInformationFile = (NTSTATUS (NTAPI*)(HANDLE,
        PIO_STATUS_BLOCK, PVOID, ULONG, FS_INFORMATION_CLASS))
        GetProcAddress(ntdll, "NtQueryVolumeInformationFile");

    if (!pNtQueryVolumeInformationFile)
        return false;

    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    FILE_FS_DEVICE_INFORMATION info = { 0 };
    IO_STATUS_BLOCK io;
    NTSTATUS status = pNtQueryVolumeInformationFile(h, &io, &info,
        sizeof(info), FileFsDeviceInformation);
    if (!NT_SUCCESS(status))
        return false;

    return info.DeviceType == FILE_DEVICE_NETWORK_FILE_SYSTEM ||
           (info.Characteristics & FILE_REMOTE_DEVICE);
}
#else
static bool check_stream_network(int fd)
{
    return false;
}
#endif

static int open_f(stream_t *stream)
{
    struct priv *p = talloc_ptrtype(stream, p);
    *p = (struct priv) {
        .fd = -1
    };
    stream->priv = p;
    stream->is_local_file = true;

    bool write = stream->mode == STREAM_WRITE;
    int m = O_CLOEXEC | (write ? O_RDWR | O_CREAT | O_TRUNC : O_RDONLY);

    char *filename = mp_file_url_to_filename(stream, bstr0(stream->url));
    if (filename) {
        stream->path = filename;
    } else {
        filename = stream->path;
    }

    bool is_fdclose = strncmp(stream->url, "fdclose://", 10) == 0;
    if (strncmp(stream->url, "fd://", 5) == 0 || is_fdclose) {
        char *begin = strstr(stream->url, "://") + 3, *end = NULL;
        p->fd = strtol(begin, &end, 0);
        if (!end || end == begin || end[0]) {
            MP_ERR(stream, "Invalid FD: %s\n", stream->url);
            return STREAM_ERROR;
        }
        if (is_fdclose)
            p->close = true;
    } else if (!strcmp(filename, "-")) {
        if (!write) {
            MP_INFO(stream, "Reading from stdin...\n");
            p->fd = 0;
        } else {
            MP_INFO(stream, "Writing to stdout...\n");
            p->fd = 1;
        }
    } else {
        if (bstr_startswith0(bstr0(stream->url), "appending://"))
            p->appending = true;

        mode_t openmode = S_IRUSR | S_IWUSR;
#ifndef __MINGW32__
        openmode |= S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        if (!write)
            m |= O_NONBLOCK;
#endif
        p->fd = open(filename, m | O_BINARY, openmode);
        if (p->fd < 0) {
            MP_ERR(stream, "Cannot open file '%s': %s\n",
                   filename, mp_strerror(errno));
            return STREAM_ERROR;
        }
        p->close = true;
    }

    struct stat st;
    if (fstat(p->fd, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            stream->is_directory = true;
            MP_INFO(stream, "This is a directory - adding to playlist.\n");
        } else if (S_ISREG(st.st_mode)) {
            p->regular_file = true;
#ifndef __MINGW32__
            // O_NONBLOCK has weird semantics on file locks; remove it.
            int val = fcntl(p->fd, F_GETFL) & ~(unsigned)O_NONBLOCK;
            fcntl(p->fd, F_SETFL, val);
#endif
        } else {
            p->use_poll = true;
        }
    }

#ifdef __MINGW32__
    setmode(p->fd, O_BINARY);
#endif

    off_t len = lseek(p->fd, 0, SEEK_END);
    lseek(p->fd, 0, SEEK_SET);
    if (len != (off_t)-1) {
        stream->seek = seek;
        stream->seekable = true;
    }

    stream->fast_skip = true;
    stream->fill_buffer = fill_buffer;
    stream->write_buffer = write_buffer;
    stream->control = control;
    stream->read_chunk = 64 * 1024;
    stream->close = s_close;

    if (check_stream_network(p->fd))
        stream->streaming = true;

    p->orig_size = get_size(stream);

    p->cancel = mp_cancel_new(p);
    if (stream->cancel)
        mp_cancel_set_parent(p->cancel, stream->cancel);

    return STREAM_OK;
}

const stream_info_t stream_info_file = {
    .name = "file",
    .open = open_f,
    .protocols = (const char*const[]){ "file", "", "fd", "fdclose",
                                       "appending", NULL },
    .can_write = true,
    .is_safe = true,
};
