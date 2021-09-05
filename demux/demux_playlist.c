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

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

#include <libavutil/common.h>

#include "config.h"
#include "common/common.h"
#include "options/options.h"
#include "common/msg.h"
#include "common/playlist.h"
#include "misc/thread_tools.h"
#include "options/path.h"
#include "stream/stream.h"
#include "osdep/io.h"
#include "misc/natural_sort.h"
#include "demux.h"

#define PROBE_SIZE (8 * 1024)

static bool check_mimetype(struct stream *s, const char *const *list)
{
    if (s->mime_type) {
        for (int n = 0; list && list[n]; n++) {
            if (strcasecmp(s->mime_type, list[n]) == 0)
                return true;
        }
    }
    return false;
}

struct pl_parser {
    struct mp_log *log;
    struct stream *s;
    char buffer[2 * 1024 * 1024];
    int utf16;
    struct playlist *pl;
    bool error;
    bool probing;
    bool force;
    bool add_base;
    enum demux_check check_level;
    struct stream *real_stream;
    char *format;
};


static uint16_t stream_read_word_endian(stream_t *s, bool big_endian)
{
    unsigned int y = stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    if (!big_endian)
        y = ((y >> 8) & 0xFF) | (y << 8);
    return y;
}

// Read characters until the next '\n' (including), or until the buffer in s is
// exhausted.
static int read_characters(stream_t *s, uint8_t *dst, int dstsize, int utf16)
{
    if (utf16 == 1 || utf16 == 2) {
        uint8_t *cur = dst;
        while (1) {
            if ((cur - dst) + 8 >= dstsize) // PUT_UTF8 writes max. 8 bytes
                return -1; // line too long
            uint32_t c;
            uint8_t tmp;
            GET_UTF16(c, stream_read_word_endian(s, utf16 == 2), return -1;)
            if (s->eof)
                break; // legitimate EOF; ignore the case of partial reads
            PUT_UTF8(c, tmp, *cur++ = tmp;)
            if (c == '\n')
                break;
        }
        return cur - dst;
    } else {
        uint8_t buf[1024];
        int buf_len = stream_read_peek(s, buf, sizeof(buf));
        uint8_t *end = memchr(buf, '\n', buf_len);
        int len = end ? end - buf + 1 : buf_len;
        if (len > dstsize)
            return -1; // line too long
        memcpy(dst, buf, len);
        stream_seek_skip(s, stream_tell(s) + len);
        return len;
    }
}

// On error, or if the line is larger than max-1, return NULL and unset s->eof.
// On EOF, return NULL, and s->eof will be set.
// Otherwise, return the line (including \n or \r\n at the end of the line).
// If the return value is non-NULL, it's always the same as mem.
// utf16: 0: UTF8 or 8 bit legacy, 1: UTF16-LE, 2: UTF16-BE
static char *read_line(stream_t *s, char *mem, int max, int utf16)
{
    if (max < 1)
        return NULL;
    int read = 0;
    while (1) {
        // Reserve 1 byte of ptr for terminating \0.
        int l = read_characters(s, &mem[read], max - read - 1, utf16);
        if (l < 0 || memchr(&mem[read], '\0', l)) {
            MP_WARN(s, "error reading line\n");
            return NULL;
        }
        read += l;
        if (l == 0 || (read > 0 && mem[read - 1] == '\n'))
            break;
    }
    mem[read] = '\0';
    if (!stream_read_peek(s, &(char){0}, 1) && read == 0) // legitimate EOF
        return NULL;
    return mem;
}

static char *pl_get_line0(struct pl_parser *p)
{
    char *res = read_line(p->s, p->buffer, sizeof(p->buffer), p->utf16);
    if (res) {
        int len = strlen(res);
        if (len > 0 && res[len - 1] == '\n')
            res[len - 1] = '\0';
    } else {
        p->error |= !p->s->eof;
    }
    return res;
}

static bstr pl_get_line(struct pl_parser *p)
{
    return bstr0(pl_get_line0(p));
}

static void pl_add(struct pl_parser *p, bstr entry)
{
    char *s = bstrto0(NULL, entry);
    playlist_add_file(p->pl, s);
    talloc_free(s);
}

static bool pl_eof(struct pl_parser *p)
{
    return p->error || p->s->eof;
}

static bool maybe_text(bstr d)
{
    for (int n = 0; n < d.len; n++) {
        unsigned char c = d.start[n];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t')
            return false;
    }
    return true;
}

static int parse_m3u(struct pl_parser *p)
{
    bstr line = bstr_strip(pl_get_line(p));
    if (p->probing && !bstr_equals0(line, "#EXTM3U")) {
        // Last resort: if the file extension is m3u, it might be headerless.
        if (p->check_level == DEMUX_CHECK_UNSAFE) {
            char *ext = mp_splitext(p->real_stream->url, NULL);
            char probe[PROBE_SIZE];
            int len = stream_read_peek(p->real_stream, probe, sizeof(probe));
            bstr data = {probe, len};
            if (ext && data.len > 10 && maybe_text(data)) {
                const char *exts[] = {"m3u", "m3u8", NULL};
                for (int n = 0; exts[n]; n++) {
                    if (strcasecmp(ext, exts[n]) == 0)
                        goto ok;
                }
            }
        }
        return -1;
    }

ok:
    if (p->probing)
        return 0;

    char *title = NULL;
    while (line.len || !pl_eof(p)) {
        if (bstr_eatstart0(&line, "#EXTINF:")) {
            bstr duration, btitle;
            if (bstr_split_tok(line, ",", &duration, &btitle) && btitle.len) {
                talloc_free(title);
                title = bstrto0(NULL, btitle);
            }
        } else if (bstr_startswith0(line, "#EXT-X-")) {
            p->format = "hls";
        } else if (line.len > 0 && !bstr_startswith0(line, "#")) {
            char *fn = bstrto0(NULL, line);
            struct playlist_entry *e = playlist_entry_new(fn);
            talloc_free(fn);
            e->title = talloc_steal(e, title);
            title = NULL;
            playlist_add(p->pl, e);
        }
        line = bstr_strip(pl_get_line(p));
    }
    talloc_free(title);
    return 0;
}

static int parse_ref_init(struct pl_parser *p)
{
    bstr line = bstr_strip(pl_get_line(p));
    if (!bstr_equals0(line, "[Reference]"))
        return -1;

    // ASF http streaming redirection - this is needed because ffmpeg http://
    // and mmsh:// can not automatically switch automatically between each
    // others. Both protocols use http - MMSH requires special http headers
    // to "activate" it, and will in other cases return this playlist.
    static const char *const mmsh_types[] = {"audio/x-ms-wax",
        "audio/x-ms-wma", "video/x-ms-asf", "video/x-ms-afs", "video/x-ms-wmv",
        "video/x-ms-wma", "application/x-mms-framed",
        "application/vnd.ms.wms-hdr.asfv1", NULL};
    bstr burl = bstr0(p->s->url);
    if (bstr_eatstart0(&burl, "http://") && check_mimetype(p->s, mmsh_types)) {
        MP_INFO(p, "Redirecting to mmsh://\n");
        playlist_add_file(p->pl, talloc_asprintf(p, "mmsh://%.*s", BSTR_P(burl)));
        return 0;
    }

    while (!pl_eof(p)) {
        line = bstr_strip(pl_get_line(p));
        if (bstr_case_startswith(line, bstr0("Ref"))) {
            bstr_split_tok(line, "=", &(bstr){0}, &line);
            if (line.len)
                pl_add(p, line);
        }
    }
    return 0;
}

static int parse_ini_thing(struct pl_parser *p, const char *header,
                           const char *entry)
{
    bstr line = {0};
    while (!line.len && !pl_eof(p))
        line = bstr_strip(pl_get_line(p));
    if (bstrcasecmp0(line, header) != 0)
        return -1;
    if (p->probing)
        return 0;
    while (!pl_eof(p)) {
        line = bstr_strip(pl_get_line(p));
        bstr key, value;
        if (bstr_split_tok(line, "=", &key, &value) &&
            bstr_case_startswith(key, bstr0(entry)))
        {
            value = bstr_strip(value);
            if (bstr_startswith0(value, "\"") && bstr_endswith0(value, "\""))
                value = bstr_splice(value, 1, -1);
            pl_add(p, value);
        }
    }
    return 0;
}

static int parse_pls(struct pl_parser *p)
{
    return parse_ini_thing(p, "[playlist]", "File");
}

static int parse_url(struct pl_parser *p)
{
    return parse_ini_thing(p, "[InternetShortcut]", "URL");
}

static int parse_txt(struct pl_parser *p)
{
    if (!p->force)
        return -1;
    if (p->probing)
        return 0;
    MP_WARN(p, "Reading plaintext playlist.\n");
    while (!pl_eof(p)) {
        bstr line = bstr_strip(pl_get_line(p));
        if (line.len == 0)
            continue;
        pl_add(p, line);
    }
    return 0;
}

#define MAX_DIR_STACK 20

static bool same_st(struct stat *st1, struct stat *st2)
{
    return st1->st_dev == st2->st_dev && st1->st_ino == st2->st_ino;
}

// Return true if this was a readable directory.
static bool scan_dir(struct pl_parser *p, char *path,
                     struct stat *dir_stack, int num_dir_stack,
                     char ***files, int *num_files)
{
    if (strlen(path) >= 8192 || num_dir_stack == MAX_DIR_STACK)
        return false; // things like mount bind loops

    DIR *dp = opendir(path);
    if (!dp) {
        MP_ERR(p, "Could not read directory.\n");
        return false;
    }

    struct dirent *ep;
    while ((ep = readdir(dp))) {
        if (ep->d_name[0] == '.')
            continue;

        if (mp_cancel_test(p->s->cancel))
            break;

        char *file = mp_path_join(p, path, ep->d_name);

        struct stat st;
        if (stat(file, &st) == 0 && S_ISDIR(st.st_mode)) {
            for (int n = 0; n < num_dir_stack; n++) {
                if (same_st(&dir_stack[n], &st)) {
                    MP_VERBOSE(p, "Skip recursive entry: %s\n", file);
                    goto skip;
                }
            }

            dir_stack[num_dir_stack] = st;
            scan_dir(p, file, dir_stack, num_dir_stack + 1, files, num_files);
        } else {
            MP_TARRAY_APPEND(p, *files, *num_files, file);
        }

        skip: ;
    }

    closedir(dp);
    return true;
}

static int cmp_filename(const void *a, const void *b)
{
    return mp_natural_sort_cmp(*(char **)a, *(char **)b);
}

static int parse_dir(struct pl_parser *p)
{
    if (!p->real_stream->is_directory)
        return -1;
    if (p->probing)
        return 0;

    char *path = mp_file_get_path(p, bstr0(p->real_stream->url));
    if (!path)
        return -1;

    char **files = NULL;
    int num_files = 0;
    struct stat dir_stack[MAX_DIR_STACK];

    scan_dir(p, path, dir_stack, 0, &files, &num_files);

    if (files)
        qsort(files, num_files, sizeof(files[0]), cmp_filename);

    for (int n = 0; n < num_files; n++)
        playlist_add_file(p->pl, files[n]);

    p->add_base = false;

    return num_files > 0 ? 0 : -1;
}

#define MIME_TYPES(...) \
    .mime_types = (const char*const[]){__VA_ARGS__, NULL}

struct pl_format {
    const char *name;
    int (*parse)(struct pl_parser *p);
    const char *const *mime_types;
};

static const struct pl_format formats[] = {
    {"directory", parse_dir},
    {"m3u", parse_m3u,
     MIME_TYPES("audio/mpegurl", "audio/x-mpegurl", "application/x-mpegurl")},
    {"ini", parse_ref_init},
    {"pls", parse_pls,
     MIME_TYPES("audio/x-scpls")},
    {"url", parse_url},
    {"txt", parse_txt},
};

static const struct pl_format *probe_pl(struct pl_parser *p)
{
    int64_t start = stream_tell(p->s);
    for (int n = 0; n < MP_ARRAY_SIZE(formats); n++) {
        const struct pl_format *fmt = &formats[n];
        stream_seek(p->s, start);
        if (check_mimetype(p->s, fmt->mime_types)) {
            MP_VERBOSE(p, "forcing format by mime-type.\n");
            p->force = true;
            return fmt;
        }
        if (fmt->parse(p) >= 0)
            return fmt;
    }
    return NULL;
}

static int open_file(struct demuxer *demuxer, enum demux_check check)
{
    if (!demuxer->access_references)
        return -1;

    bool force = check < DEMUX_CHECK_UNSAFE || check == DEMUX_CHECK_REQUEST;

    struct pl_parser *p = talloc_zero(NULL, struct pl_parser);
    p->log = demuxer->log;
    p->pl = talloc_zero(p, struct playlist);
    p->real_stream = demuxer->stream;
    p->add_base = true;

    char probe[PROBE_SIZE];
    int probe_len = stream_read_peek(p->real_stream, probe, sizeof(probe));
    p->s = stream_memory_open(demuxer->global, probe, probe_len);
    p->s->mime_type = demuxer->stream->mime_type;
    p->utf16 = stream_skip_bom(p->s);
    p->force = force;
    p->check_level = check;
    p->probing = true;
    const struct pl_format *fmt = probe_pl(p);
    free_stream(p->s);
    playlist_clear(p->pl);
    if (!fmt) {
        talloc_free(p);
        return -1;
    }

    p->probing = false;
    p->error = false;
    p->s = demuxer->stream;
    p->utf16 = stream_skip_bom(p->s);
    bool ok = fmt->parse(p) >= 0 && !p->error;
    if (p->add_base)
        playlist_add_base_path(p->pl, mp_dirname(demuxer->filename));
    playlist_set_stream_flags(p->pl, demuxer->stream_origin);
    demuxer->playlist = talloc_steal(demuxer, p->pl);
    demuxer->filetype = p->format ? p->format : fmt->name;
    demuxer->fully_read = true;
    talloc_free(p);
    if (ok)
        demux_close_stream(demuxer);
    return ok ? 0 : -1;
}

const struct demuxer_desc demuxer_desc_playlist = {
    .name = "playlist",
    .desc = "Playlist file",
    .open = open_file,
};
