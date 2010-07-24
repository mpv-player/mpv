/*
 * Some code freely inspired from VobSub <URL:http://vobsub.edensrising.com>,
 * with kind permission from Gabest <gabest@freemail.hu>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include "mpcommon.h"
#include "vobsub.h"
#include "spudec.h"
#include "mp_msg.h"
#include "unrar_exec.h"
#include "libavutil/common.h"

extern int vobsub_id;
// Record the original -vobsubid set by commandline, since vobsub_id will be
// overridden if slang match any of vobsub streams.
static int vobsubid = -2;

/**********************************************************************
 * RAR stream handling
 * The RAR file must have the same basename as the file to open
 **********************************************************************/
#ifdef CONFIG_UNRAR_EXEC
typedef struct {
    FILE *file;
    unsigned char *data;
    unsigned long size;
    unsigned long pos;
} rar_stream_t;

static rar_stream_t *rar_open(const char *const filename,
                              const char *const mode)
{
    rar_stream_t *stream;
    /* unrar_exec can only read */
    if (strcmp("r", mode) && strcmp("rb", mode)) {
        errno = EINVAL;
        return NULL;
    }
    stream = malloc(sizeof(rar_stream_t));
    if (stream == NULL)
        return NULL;
    /* first try normal access */
    stream->file = fopen(filename, mode);
    if (stream->file == NULL) {
        char *rar_filename;
        const char *p;
        int rc;
        /* Guess the RAR archive filename */
        rar_filename = NULL;
        p = strrchr(filename, '.');
        if (p) {
            ptrdiff_t l = p - filename;
            rar_filename = malloc(l + 5);
            if (rar_filename == NULL) {
                free(stream);
                return NULL;
            }
            strncpy(rar_filename, filename, l);
            strcpy(rar_filename + l, ".rar");
        } else {
            rar_filename = malloc(strlen(filename) + 5);
            if (rar_filename == NULL) {
                free(stream);
                return NULL;
            }
            strcpy(rar_filename, filename);
            strcat(rar_filename, ".rar");
        }
        /* get rid of the path if there is any */
        if ((p = strrchr(filename, '/')) == NULL) {
            p = filename;
        } else {
            p++;
        }
        rc = unrar_exec_get(&stream->data, &stream->size, p, rar_filename);
        if (!rc) {
            /* There is no matching filename in the archive. However, sometimes
             * the files we are looking for have been given arbitrary names in the archive.
             * Let's look for a file with an exact match in the extension only. */
            int i, num_files, name_len;
            ArchiveList_struct *list, *lp;
            num_files = unrar_exec_list(rar_filename, &list);
            if (num_files > 0) {
                char *demanded_ext;
                demanded_ext = strrchr (p, '.');
                if (demanded_ext) {
                    int demanded_ext_len = strlen (demanded_ext);
                    for (i = 0, lp = list; i < num_files; i++, lp = lp->next) {
                        name_len = strlen (lp->item.Name);
                        if (name_len >= demanded_ext_len && !strcasecmp (lp->item.Name + name_len - demanded_ext_len, demanded_ext)) {
                            rc = unrar_exec_get(&stream->data, &stream->size,
                                                lp->item.Name, rar_filename);
                            if (rc)
                                break;
                        }
                    }
                }
                unrar_exec_freelist(list);
            }
            if (!rc) {
                free(rar_filename);
                free(stream);
                return NULL;
            }
        }

        free(rar_filename);
        stream->pos = 0;
    }
    return stream;
}

static int rar_close(rar_stream_t *stream)
{
    if (stream->file)
        return fclose(stream->file);
    free(stream->data);
    return 0;
}

static int rar_eof(rar_stream_t *stream)
{
    if (stream->file)
        return feof(stream->file);
    return stream->pos >= stream->size;
}

static long rar_tell(rar_stream_t *stream)
{
    if (stream->file)
        return ftell(stream->file);
    return stream->pos;
}

static int rar_seek(rar_stream_t *stream, long offset, int whence)
{
    if (stream->file)
        return fseek(stream->file, offset, whence);
    switch (whence) {
    case SEEK_SET:
        if (offset < 0) {
            errno = EINVAL;
            return -1;
        }
        stream->pos = offset;
        break;
    case SEEK_CUR:
        if (offset < 0 && stream->pos < (unsigned long) -offset) {
            errno = EINVAL;
            return -1;
        }
        stream->pos += offset;
        break;
    case SEEK_END:
        if (offset < 0 && stream->size < (unsigned long) -offset) {
            errno = EINVAL;
            return -1;
        }
        stream->pos = stream->size + offset;
        break;
    default:
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static int rar_getc(rar_stream_t *stream)
{
    if (stream->file)
        return getc(stream->file);
    if (rar_eof(stream))
        return EOF;
    return stream->data[stream->pos++];
}

static size_t rar_read(void *ptr, size_t size, size_t nmemb,
                       rar_stream_t *stream)
{
    size_t res;
    unsigned long remain;
    if (stream->file)
        return fread(ptr, size, nmemb, stream->file);
    if (rar_eof(stream))
        return 0;
    res = size * nmemb;
    remain = stream->size - stream->pos;
    if (res > remain)
        res = remain / size * size;
    memcpy(ptr, stream->data + stream->pos, res);
    stream->pos += res;
    res /= size;
    return res;
}

#else
typedef FILE rar_stream_t;
#define rar_open        fopen
#define rar_close       fclose
#define rar_eof         feof
#define rar_tell        ftell
#define rar_seek        fseek
#define rar_getc        getc
#define rar_read        fread
#endif

/**********************************************************************/

static ssize_t vobsub_getline(char **lineptr, size_t *n, rar_stream_t *stream)
{
    size_t res = 0;
    int c;
    if (*lineptr == NULL) {
        *lineptr = malloc(4096);
        if (*lineptr)
            *n = 4096;
    } else if (*n == 0) {
        char *tmp = realloc(*lineptr, 4096);
        if (tmp) {
            *lineptr = tmp;
            *n = 4096;
        }
    }
    if (*lineptr == NULL || *n == 0)
        return -1;

    for (c = rar_getc(stream); c != EOF; c = rar_getc(stream)) {
        if (res + 1 >= *n) {
            char *tmp = realloc(*lineptr, *n * 2);
            if (tmp == NULL)
                return -1;
            *lineptr = tmp;
            *n *= 2;
        }
        (*lineptr)[res++] = c;
        if (c == '\n') {
            (*lineptr)[res] = 0;
            return res;
        }
    }
    if (res == 0)
        return -1;
    (*lineptr)[res] = 0;
    return res;
}

/**********************************************************************
 * MPEG parsing
 **********************************************************************/

typedef struct {
    rar_stream_t *stream;
    unsigned int pts;
    int aid;
    unsigned char *packet;
    unsigned int packet_reserve;
    unsigned int packet_size;
    int padding_was_here;
    int merge;
} mpeg_t;

static mpeg_t *mpeg_open(const char *filename)
{
    mpeg_t *res = malloc(sizeof(mpeg_t));
    int err = res == NULL;
    if (!err) {
        res->pts            = 0;
        res->aid            = -1;
        res->packet         = NULL;
        res->packet_size    = 0;
        res->packet_reserve = 0;
        res->padding_was_here = 1;
        res->merge          = 0;
        res->stream         = rar_open(filename, "rb");
        err = res->stream == NULL;
        if (err)
            perror("fopen Vobsub file failed");
        if (err)
            free(res);
    }
    return err ? NULL : res;
}

static void mpeg_free(mpeg_t *mpeg)
{
    if (mpeg->packet)
        free(mpeg->packet);
    if (mpeg->stream)
        rar_close(mpeg->stream);
    free(mpeg);
}

static int mpeg_eof(mpeg_t *mpeg)
{
    return rar_eof(mpeg->stream);
}

static off_t mpeg_tell(mpeg_t *mpeg)
{
    return rar_tell(mpeg->stream);
}

static int mpeg_run(mpeg_t *mpeg)
{
    unsigned int len, idx, version;
    int c;
    /* Goto start of a packet, it starts with 0x000001?? */
    const unsigned char wanted[] = { 0, 0, 1 };
    unsigned char buf[5];

    mpeg->aid = -1;
    mpeg->packet_size = 0;
    if (rar_read(buf, 4, 1, mpeg->stream) != 1)
        return -1;
    while (memcmp(buf, wanted, sizeof(wanted)) != 0) {
        c = rar_getc(mpeg->stream);
        if (c < 0)
            return -1;
        memmove(buf, buf + 1, 3);
        buf[3] = c;
    }
    switch (buf[3]) {
    case 0xb9:                  /* System End Code */
        break;
    case 0xba:                  /* Packet start code */
        c = rar_getc(mpeg->stream);
        if (c < 0)
            return -1;
        if ((c & 0xc0) == 0x40)
            version = 4;
        else if ((c & 0xf0) == 0x20)
            version = 2;
        else {
            mp_msg(MSGT_VOBSUB, MSGL_ERR, "VobSub: Unsupported MPEG version: 0x%02x\n", c);
            return -1;
        }
        if (version == 4) {
            if (rar_seek(mpeg->stream, 9, SEEK_CUR))
                return -1;
        } else if (version == 2) {
            if (rar_seek(mpeg->stream, 7, SEEK_CUR))
                return -1;
        } else
            abort();
        if (!mpeg->padding_was_here)
            mpeg->merge = 1;
        break;
    case 0xbd:                  /* packet */
        if (rar_read(buf, 2, 1, mpeg->stream) != 1)
            return -1;
        mpeg->padding_was_here = 0;
        len = buf[0] << 8 | buf[1];
        idx = mpeg_tell(mpeg);
        c = rar_getc(mpeg->stream);
        if (c < 0)
            return -1;
        if ((c & 0xC0) == 0x40) { /* skip STD scale & size */
            if (rar_getc(mpeg->stream) < 0)
                return -1;
            c = rar_getc(mpeg->stream);
            if (c < 0)
                return -1;
        }
        if ((c & 0xf0) == 0x20) { /* System-1 stream timestamp */
            /* Do we need this? */
            abort();
        } else if ((c & 0xf0) == 0x30) {
            /* Do we need this? */
            abort();
        } else if ((c & 0xc0) == 0x80) { /* System-2 (.VOB) stream */
            unsigned int pts_flags, hdrlen, dataidx;
            c = rar_getc(mpeg->stream);
            if (c < 0)
                return -1;
            pts_flags = c;
            c = rar_getc(mpeg->stream);
            if (c < 0)
                return -1;
            hdrlen = c;
            dataidx = mpeg_tell(mpeg) + hdrlen;
            if (dataidx > idx + len) {
                mp_msg(MSGT_VOBSUB, MSGL_ERR, "Invalid header length: %d (total length: %d, idx: %d, dataidx: %d)\n",
                       hdrlen, len, idx, dataidx);
                return -1;
            }
            if ((pts_flags & 0xc0) == 0x80) {
                if (rar_read(buf, 5, 1, mpeg->stream) != 1)
                    return -1;
                if (!(((buf[0] & 0xf0) == 0x20) && (buf[0] & 1) && (buf[2] & 1) &&  (buf[4] & 1))) {
                    mp_msg(MSGT_VOBSUB, MSGL_ERR, "vobsub PTS error: 0x%02x %02x%02x %02x%02x \n",
                           buf[0], buf[1], buf[2], buf[3], buf[4]);
                    mpeg->pts = 0;
                } else
                    mpeg->pts = ((buf[0] & 0x0e) << 29 | buf[1] << 22 | (buf[2] & 0xfe) << 14
                        | buf[3] << 7 | (buf[4] >> 1));
            } else /* if ((pts_flags & 0xc0) == 0xc0) */ {
                /* what's this? */
                /* abort(); */
            }
            rar_seek(mpeg->stream, dataidx, SEEK_SET);
            mpeg->aid = rar_getc(mpeg->stream);
            if (mpeg->aid < 0) {
                mp_msg(MSGT_VOBSUB, MSGL_ERR, "Bogus aid %d\n", mpeg->aid);
                return -1;
            }
            mpeg->packet_size = len - ((unsigned int) mpeg_tell(mpeg) - idx);
            if (mpeg->packet_reserve < mpeg->packet_size) {
                if (mpeg->packet)
                    free(mpeg->packet);
                mpeg->packet = malloc(mpeg->packet_size);
                if (mpeg->packet)
                    mpeg->packet_reserve = mpeg->packet_size;
            }
            if (mpeg->packet == NULL) {
                mp_msg(MSGT_VOBSUB, MSGL_FATAL, "malloc failure");
                mpeg->packet_reserve = 0;
                mpeg->packet_size = 0;
                return -1;
            }
            if (rar_read(mpeg->packet, mpeg->packet_size, 1, mpeg->stream) != 1) {
                mp_msg(MSGT_VOBSUB, MSGL_ERR, "fread failure");
                mpeg->packet_size = 0;
                return -1;
            }
            idx = len;
        }
        break;
    case 0xbe:                  /* Padding */
        if (rar_read(buf, 2, 1, mpeg->stream) != 1)
            return -1;
        len = buf[0] << 8 | buf[1];
        if (len > 0 && rar_seek(mpeg->stream, len, SEEK_CUR))
            return -1;
        mpeg->padding_was_here = 1;
        break;
    default:
        if (0xc0 <= buf[3] && buf[3] < 0xf0) {
            /* MPEG audio or video */
            if (rar_read(buf, 2, 1, mpeg->stream) != 1)
                return -1;
            len = buf[0] << 8 | buf[1];
            if (len > 0 && rar_seek(mpeg->stream, len, SEEK_CUR))
                return -1;
        } else {
            mp_msg(MSGT_VOBSUB, MSGL_ERR, "unknown header 0x%02X%02X%02X%02X\n",
                   buf[0], buf[1], buf[2], buf[3]);
            return -1;
        }
    }
    return 0;
}

/**********************************************************************
 * Packet queue
 **********************************************************************/

typedef struct {
    unsigned int pts100;
    off_t filepos;
    unsigned int size;
    unsigned char *data;
} packet_t;

typedef struct {
    char *id;
    packet_t *packets;
    unsigned int packets_reserve;
    unsigned int packets_size;
    unsigned int current_index;
} packet_queue_t;

static void packet_construct(packet_t *pkt)
{
    pkt->pts100 = 0;
    pkt->filepos = 0;
    pkt->size = 0;
    pkt->data = NULL;
}

static void packet_destroy(packet_t *pkt)
{
    if (pkt->data)
        free(pkt->data);
}

static void packet_queue_construct(packet_queue_t *queue)
{
    queue->id = NULL;
    queue->packets = NULL;
    queue->packets_reserve = 0;
    queue->packets_size = 0;
    queue->current_index = 0;
}

static void packet_queue_destroy(packet_queue_t *queue)
{
    if (queue->packets) {
        while (queue->packets_size--)
            packet_destroy(queue->packets + queue->packets_size);
        free(queue->packets);
    }
    return;
}

/* Make sure there is enough room for needed_size packets in the
   packet queue. */
static int packet_queue_ensure(packet_queue_t *queue, unsigned int needed_size)
{
    if (queue->packets_reserve < needed_size) {
        if (queue->packets) {
            packet_t *tmp = realloc(queue->packets, 2 * queue->packets_reserve * sizeof(packet_t));
            if (tmp == NULL) {
                mp_msg(MSGT_VOBSUB, MSGL_FATAL, "realloc failure");
                return -1;
            }
            queue->packets = tmp;
            queue->packets_reserve *= 2;
        } else {
            queue->packets = malloc(sizeof(packet_t));
            if (queue->packets == NULL) {
                mp_msg(MSGT_VOBSUB, MSGL_FATAL, "malloc failure");
                return -1;
            }
            queue->packets_reserve = 1;
        }
    }
    return 0;
}

/* add one more packet */
static int packet_queue_grow(packet_queue_t *queue)
{
    if (packet_queue_ensure(queue, queue->packets_size + 1) < 0)
        return -1;
    packet_construct(queue->packets + queue->packets_size);
    ++queue->packets_size;
    return 0;
}

/* insert a new packet, duplicating pts from the current one */
static int packet_queue_insert(packet_queue_t *queue)
{
    packet_t *pkts;
    if (packet_queue_ensure(queue, queue->packets_size + 1) < 0)
        return -1;
    /* XXX packet_size does not reflect the real thing here, it will be updated a bit later */
    memmove(queue->packets + queue->current_index + 2,
            queue->packets + queue->current_index + 1,
            sizeof(packet_t) * (queue->packets_size - queue->current_index - 1));
    pkts = queue->packets + queue->current_index;
    ++queue->packets_size;
    ++queue->current_index;
    packet_construct(pkts + 1);
    pkts[1].pts100 = pkts[0].pts100;
    pkts[1].filepos = pkts[0].filepos;
    return 0;
}

/**********************************************************************
 * Vobsub
 **********************************************************************/

typedef struct {
    unsigned int palette[16];
    int delay;
    unsigned int have_palette;
    unsigned int orig_frame_width, orig_frame_height;
    unsigned int origin_x, origin_y;
    /* index */
    packet_queue_t *spu_streams;
    unsigned int spu_streams_size;
    unsigned int spu_streams_current;
    unsigned int spu_valid_streams_size;
} vobsub_t;

/* Make sure that the spu stream idx exists. */
static int vobsub_ensure_spu_stream(vobsub_t *vob, unsigned int index)
{
    if (index >= vob->spu_streams_size) {
        /* This is a new stream */
        if (vob->spu_streams) {
            packet_queue_t *tmp = realloc(vob->spu_streams, (index + 1) * sizeof(packet_queue_t));
            if (tmp == NULL) {
                mp_msg(MSGT_VOBSUB, MSGL_ERR, "vobsub_ensure_spu_stream: realloc failure");
                return -1;
            }
            vob->spu_streams = tmp;
        } else {
            vob->spu_streams = malloc((index + 1) * sizeof(packet_queue_t));
            if (vob->spu_streams == NULL) {
                mp_msg(MSGT_VOBSUB, MSGL_ERR, "vobsub_ensure_spu_stream: malloc failure");
                return -1;
            }
        }
        while (vob->spu_streams_size <= index) {
            packet_queue_construct(vob->spu_streams + vob->spu_streams_size);
            ++vob->spu_streams_size;
        }
    }
    return 0;
}

static int vobsub_add_id(vobsub_t *vob, const char *id, size_t idlen,
                         const unsigned int index)
{
    if (vobsub_ensure_spu_stream(vob, index) < 0)
        return -1;
    if (id && idlen) {
        if (vob->spu_streams[index].id)
            free(vob->spu_streams[index].id);
        vob->spu_streams[index].id = malloc(idlen + 1);
        if (vob->spu_streams[index].id == NULL) {
            mp_msg(MSGT_VOBSUB, MSGL_FATAL, "vobsub_add_id: malloc failure");
            return -1;
        }
        vob->spu_streams[index].id[idlen] = 0;
        memcpy(vob->spu_streams[index].id, id, idlen);
    }
    vob->spu_streams_current = index;
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VOBSUB_ID=%d\n", index);
    if (id && idlen)
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VSID_%d_LANG=%s\n", index, vob->spu_streams[index].id);
    mp_msg(MSGT_VOBSUB, MSGL_V, "[vobsub] subtitle (vobsubid): %d language %s\n",
           index, vob->spu_streams[index].id);
    return 0;
}

static int vobsub_add_timestamp(vobsub_t *vob, off_t filepos, int ms)
{
    packet_queue_t *queue;
    packet_t *pkt;
    if (vob->spu_streams == 0) {
        mp_msg(MSGT_VOBSUB, MSGL_WARN, "[vobsub] warning, binning some index entries.  Check your index file\n");
        return -1;
    }
    queue = vob->spu_streams + vob->spu_streams_current;
    if (packet_queue_grow(queue) >= 0) {
        pkt = queue->packets + (queue->packets_size - 1);
        pkt->filepos = filepos;
        pkt->pts100 = ms < 0 ? UINT_MAX : (unsigned int)ms * 90;
        return 0;
    }
    return -1;
}

static int vobsub_parse_id(vobsub_t *vob, const char *line)
{
    // id: xx, index: n
    size_t idlen;
    const char *p, *q;
    p  = line;
    while (isspace(*p))
        ++p;
    q = p;
    while (isalpha(*q))
        ++q;
    idlen = q - p;
    if (idlen == 0)
        return -1;
    ++q;
    while (isspace(*q))
        ++q;
    if (strncmp("index:", q, 6))
        return -1;
    q += 6;
    while (isspace(*q))
        ++q;
    if (!isdigit(*q))
        return -1;
    return vobsub_add_id(vob, p, idlen, atoi(q));
}

static int vobsub_parse_timestamp(vobsub_t *vob, const char *line)
{
    // timestamp: HH:MM:SS.mmm, filepos: 0nnnnnnnnn
    const char *p;
    int h, m, s, ms;
    off_t filepos;
    while (isspace(*line))
        ++line;
    p = line;
    while (isdigit(*p))
        ++p;
    if (p - line != 2)
        return -1;
    h = atoi(line);
    if (*p != ':')
        return -1;
    line = ++p;
    while (isdigit(*p))
        ++p;
    if (p - line != 2)
        return -1;
    m = atoi(line);
    if (*p != ':')
        return -1;
    line = ++p;
    while (isdigit(*p))
        ++p;
    if (p - line != 2)
        return -1;
    s = atoi(line);
    if (*p != ':')
        return -1;
    line = ++p;
    while (isdigit(*p))
        ++p;
    if (p - line != 3)
        return -1;
    ms = atoi(line);
    if (*p != ',')
        return -1;
    line = p + 1;
    while (isspace(*line))
        ++line;
    if (strncmp("filepos:", line, 8))
        return -1;
    line += 8;
    while (isspace(*line))
        ++line;
    if (! isxdigit(*line))
        return -1;
    filepos = strtol(line, NULL, 16);
    return vobsub_add_timestamp(vob, filepos, vob->delay + ms + 1000 * (s + 60 * (m + 60 * h)));
}

static int vobsub_parse_origin(vobsub_t *vob, const char *line)
{
    // org: X,Y
    char *p;
    while (isspace(*line))
        ++line;
    if (!isdigit(*line))
        return -1;
    vob->origin_x = strtoul(line, &p, 10);
    if (*p != ',')
        return -1;
    ++p;
    vob->origin_y = strtoul(p, NULL, 10);
    return 0;
}

unsigned int vobsub_palette_to_yuv(unsigned int pal)
{
    int r, g, b, y, u, v;
    // Palette in idx file is not rgb value, it was calculated by wrong formula.
    // Here's reversed formula of the one used to generate palette in idx file.
    r = pal >> 16 & 0xff;
    g = pal >> 8 & 0xff;
    b = pal & 0xff;
    y = av_clip_uint8( 0.1494  * r + 0.6061 * g + 0.2445 * b);
    u = av_clip_uint8( 0.6066  * r - 0.4322 * g - 0.1744 * b + 128);
    v = av_clip_uint8(-0.08435 * r - 0.3422 * g + 0.4266 * b + 128);
    y = y * 219 / 255 + 16;
    return y << 16 | u << 8 | v;
}

unsigned int vobsub_rgb_to_yuv(unsigned int rgb)
{
    int r, g, b, y, u, v;
    r = rgb >> 16 & 0xff;
    g = rgb >> 8 & 0xff;
    b = rgb & 0xff;
    y = ( 0.299   * r + 0.587   * g + 0.114   * b) * 219 / 255 + 16.5;
    u = (-0.16874 * r - 0.33126 * g + 0.5     * b) * 224 / 255 + 128.5;
    v = ( 0.5     * r - 0.41869 * g - 0.08131 * b) * 224 / 255 + 128.5;
    return y << 16 | u << 8 | v;
}

static int vobsub_parse_delay(vobsub_t *vob, const char *line)
{
    int h, m, s, ms;
    int forward = 1;
    if (*(line + 7) == '+') {
        forward = 1;
        line++;
    } else if (*(line + 7) == '-') {
        forward = -1;
        line++;
    }
    mp_msg(MSGT_SPUDEC, MSGL_V, "forward=%d", forward);
    h = atoi(line + 7);
    mp_msg(MSGT_VOBSUB, MSGL_V, "h=%d,", h);
    m = atoi(line + 10);
    mp_msg(MSGT_VOBSUB, MSGL_V, "m=%d,", m);
    s = atoi(line + 13);
    mp_msg(MSGT_VOBSUB, MSGL_V, "s=%d,", s);
    ms = atoi(line + 16);
    mp_msg(MSGT_VOBSUB, MSGL_V, "ms=%d", ms);
    vob->delay = (ms + 1000 * (s + 60 * (m + 60 * h))) * forward;
    return 0;
}

static int vobsub_set_lang(const char *line)
{
    if (vobsub_id == -1)
        vobsub_id = atoi(line + 8);
    return 0;
}

static int vobsub_parse_one_line(vobsub_t *vob, rar_stream_t *fd,
                                 unsigned char **extradata,
                                 unsigned int *extradata_len)
{
    ssize_t line_size;
    int res = -1;
        size_t line_reserve = 0;
        char *line = NULL;
    do {
        line_size = vobsub_getline(&line, &line_reserve, fd);
        if (line_size < 0 || line_size > 1000000 ||
            *extradata_len+line_size > 10000000) {
            break;
        }

        *extradata = realloc(*extradata, *extradata_len+line_size+1);
        memcpy(*extradata+*extradata_len, line, line_size);
        *extradata_len += line_size;
        (*extradata)[*extradata_len] = 0;

        if (*line == 0 || *line == '\r' || *line == '\n' || *line == '#')
            continue;
        else if (strncmp("langidx:", line, 8) == 0)
            res = vobsub_set_lang(line);
        else if (strncmp("delay:", line, 6) == 0)
            res = vobsub_parse_delay(vob, line);
        else if (strncmp("id:", line, 3) == 0)
            res = vobsub_parse_id(vob, line + 3);
        else if (strncmp("org:", line, 4) == 0)
            res = vobsub_parse_origin(vob, line + 4);
        else if (strncmp("timestamp:", line, 10) == 0)
            res = vobsub_parse_timestamp(vob, line + 10);
        else {
            mp_msg(MSGT_VOBSUB, MSGL_V, "vobsub: ignoring %s", line);
            continue;
        }
        if (res < 0)
            mp_msg(MSGT_VOBSUB, MSGL_ERR,  "ERROR in %s", line);
        break;
    } while (1);
    if (line)
      free(line);
    return res;
}

int vobsub_parse_ifo(void* this, const char *const name, unsigned int *palette,
                     unsigned int *width, unsigned int *height, int force,
                     int sid, char *langid)
{
    vobsub_t *vob = (vobsub_t*)this;
    int res = -1;
    rar_stream_t *fd = rar_open(name, "rb");
    if (fd == NULL) {
        if (force)
            mp_msg(MSGT_VOBSUB, MSGL_WARN, "VobSub: Can't open IFO file\n");
    } else {
        // parse IFO header
        unsigned char block[0x800];
        const char *const ifo_magic = "DVDVIDEO-VTS";
        if (rar_read(block, sizeof(block), 1, fd) != 1) {
            if (force)
                mp_msg(MSGT_VOBSUB, MSGL_ERR, "VobSub: Can't read IFO header\n");
        } else if (memcmp(block, ifo_magic, strlen(ifo_magic) + 1))
            mp_msg(MSGT_VOBSUB, MSGL_ERR, "VobSub: Bad magic in IFO header\n");
        else {
            unsigned long pgci_sector = block[0xcc] << 24 | block[0xcd] << 16
                | block[0xce] << 8 | block[0xcf];
            int standard = (block[0x200] & 0x30) >> 4;
            int resolution = (block[0x201] & 0x0c) >> 2;
            *height = standard ? 576 : 480;
            *width = 0;
            switch (resolution) {
            case 0x0:
                *width = 720;
                break;
            case 0x1:
                *width = 704;
                break;
            case 0x2:
                *width = 352;
                break;
            case 0x3:
                *width = 352;
                *height /= 2;
                break;
            default:
                mp_msg(MSGT_VOBSUB, MSGL_WARN, "Vobsub: Unknown resolution %d \n", resolution);
            }
            if (langid && 0 <= sid && sid < 32) {
                unsigned char *tmp = block + 0x256 + sid * 6 + 2;
                langid[0] = tmp[0];
                langid[1] = tmp[1];
                langid[2] = 0;
            }
            if (rar_seek(fd, pgci_sector * sizeof(block), SEEK_SET)
                || rar_read(block, sizeof(block), 1, fd) != 1)
                mp_msg(MSGT_VOBSUB, MSGL_ERR, "VobSub: Can't read IFO PGCI\n");
            else {
                unsigned long idx;
                unsigned long pgc_offset = block[0xc] << 24 | block[0xd] << 16
                    | block[0xe] << 8 | block[0xf];
                for (idx = 0; idx < 16; ++idx) {
                    unsigned char *p = block + pgc_offset + 0xa4 + 4 * idx;
                    palette[idx] = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
                }
                if (vob)
                    vob->have_palette = 1;
                res = 0;
            }
        }
        rar_close(fd);
    }
    return res;
}

void *vobsub_open(const char *const name, const char *const ifo,
                  const int force, void** spu)
{
    unsigned char *extradata = NULL;
    unsigned int extradata_len = 0;
    vobsub_t *vob = calloc(1, sizeof(vobsub_t));
    if (spu)
        *spu = NULL;
    if (vobsubid == -2)
        vobsubid = vobsub_id;
    if (vob) {
        char *buf;
        buf = malloc(strlen(name) + 5);
        if (buf) {
            rar_stream_t *fd;
            mpeg_t *mpg;
            /* read in the info file */
            if (!ifo) {
                strcpy(buf, name);
                strcat(buf, ".ifo");
                vobsub_parse_ifo(vob, buf, vob->palette, &vob->orig_frame_width, &vob->orig_frame_height, force, -1, NULL);
            } else
                vobsub_parse_ifo(vob, ifo, vob->palette, &vob->orig_frame_width, &vob->orig_frame_height, force, -1, NULL);
            /* read in the index */
            strcpy(buf, name);
            strcat(buf, ".idx");
            fd = rar_open(buf, "rb");
            if (fd == NULL) {
                if (force)
                    mp_msg(MSGT_VOBSUB, MSGL_ERR, "VobSub: Can't open IDX file\n");
                else {
                    free(buf);
                    free(vob);
                    return NULL;
                }
            } else {
                while (vobsub_parse_one_line(vob, fd, &extradata, &extradata_len) >= 0)
                    /* NOOP */ ;
                rar_close(fd);
            }
            if (spu)
                *spu = spudec_new_scaled(vob->palette, vob->orig_frame_width, vob->orig_frame_height, extradata, extradata_len);
            if (extradata)
                free(extradata);

            /* read the indexed mpeg_stream */
            strcpy(buf, name);
            strcat(buf, ".sub");
            mpg = mpeg_open(buf);
            if (mpg == NULL) {
                if (force)
                    mp_msg(MSGT_VOBSUB, MSGL_ERR, "VobSub: Can't open SUB file\n");
                else {
                    free(buf);
                    free(vob);
                    return NULL;
                }
            } else {
                long last_pts_diff = 0;
                while (!mpeg_eof(mpg)) {
                    off_t pos = mpeg_tell(mpg);
                    if (mpeg_run(mpg) < 0) {
                        if (!mpeg_eof(mpg))
                            mp_msg(MSGT_VOBSUB, MSGL_ERR, "VobSub: mpeg_run error\n");
                        break;
                    }
                    if (mpg->packet_size) {
                        if ((mpg->aid & 0xe0) == 0x20) {
                            unsigned int sid = mpg->aid & 0x1f;
                            if (vobsub_ensure_spu_stream(vob, sid) >= 0)  {
                                packet_queue_t *queue = vob->spu_streams + sid;
                                /* get the packet to fill */
                                if (queue->packets_size == 0 && packet_queue_grow(queue)  < 0)
                                  abort();
                                while (queue->current_index + 1 < queue->packets_size
                                       && queue->packets[queue->current_index + 1].filepos <= pos)
                                    ++queue->current_index;
                                if (queue->current_index < queue->packets_size) {
                                    packet_t *pkt;
                                    if (queue->packets[queue->current_index].data) {
                                        /* insert a new packet and fix the PTS ! */
                                        packet_queue_insert(queue);
                                        queue->packets[queue->current_index].pts100 =
                                            mpg->pts + last_pts_diff;
                                    }
                                    pkt = queue->packets + queue->current_index;
                                    if (pkt->pts100 != UINT_MAX) {
                                        if (queue->packets_size > 1)
                                            last_pts_diff = pkt->pts100 - mpg->pts;
                                        else
                                            pkt->pts100 = mpg->pts;
                                        if (mpg->merge && queue->current_index > 0) {
                                            packet_t *last = &queue->packets[queue->current_index - 1];
                                            pkt->pts100 = last->pts100;
                                        }
                                        mpg->merge = 0;
                                        /* FIXME: should not use mpg_sub internal informations, make a copy */
                                        pkt->data = mpg->packet;
                                        pkt->size = mpg->packet_size;
                                        mpg->packet = NULL;
                                        mpg->packet_reserve = 0;
                                        mpg->packet_size = 0;
                                    }
                                }
                            } else
                                mp_msg(MSGT_VOBSUB, MSGL_WARN, "don't know what to do with subtitle #%u\n", sid);
                        }
                    }
                }
                vob->spu_streams_current = vob->spu_streams_size;
                while (vob->spu_streams_current-- > 0) {
                    vob->spu_streams[vob->spu_streams_current].current_index = 0;
                    if (vobsubid == vob->spu_streams_current ||
                        vob->spu_streams[vob->spu_streams_current].packets_size > 0)
                        ++vob->spu_valid_streams_size;
                }
                mpeg_free(mpg);
            }
            free(buf);
        }
    }
    return vob;
}

void vobsub_close(void *this)
{
    vobsub_t *vob = (vobsub_t *)this;
    if (vob->spu_streams) {
        while (vob->spu_streams_size--)
            packet_queue_destroy(vob->spu_streams + vob->spu_streams_size);
        free(vob->spu_streams);
    }
    free(vob);
}

unsigned int vobsub_get_indexes_count(void *vobhandle)
{
    vobsub_t *vob = (vobsub_t *) vobhandle;
    return vob->spu_valid_streams_size;
}

char *vobsub_get_id(void *vobhandle, unsigned int index)
{
    vobsub_t *vob = (vobsub_t *) vobhandle;
    return (index < vob->spu_streams_size) ? vob->spu_streams[index].id : NULL;
}

int vobsub_get_id_by_index(void *vobhandle, unsigned int index)
{
    vobsub_t *vob = vobhandle;
    int i, j;
    if (vob == NULL)
        return -1;
    for (i = 0, j = 0; i < vob->spu_streams_size; ++i)
        if (i == vobsubid || vob->spu_streams[i].packets_size > 0) {
            if (j == index)
                return i;
            ++j;
        }
    return -1;
}

int vobsub_get_index_by_id(void *vobhandle, int id)
{
    vobsub_t *vob = vobhandle;
    int i, j;
    if (vob == NULL || id < 0 || id >= vob->spu_streams_size)
        return -1;
    if (id != vobsubid && !vob->spu_streams[id].packets_size)
        return -1;
    for (i = 0, j = 0; i < id; ++i)
        if (i == vobsubid || vob->spu_streams[i].packets_size > 0)
            ++j;
    return j;
}

int vobsub_set_from_lang(void *vobhandle, unsigned char * lang)
{
    int i;
    vobsub_t *vob= (vobsub_t *) vobhandle;
    while (lang && strlen(lang) >= 2) {
        for (i = 0; i < vob->spu_streams_size; i++)
            if (vob->spu_streams[i].id)
                if ((strncmp(vob->spu_streams[i].id, lang, 2) == 0)) {
                    vobsub_id = i;
                    mp_msg(MSGT_VOBSUB, MSGL_INFO, "Selected VOBSUB language: %d language: %s\n", i, vob->spu_streams[i].id);
                    return 0;
                }
        lang+=2;while (lang[0]==',' || lang[0]==' ') ++lang;
    }
    mp_msg(MSGT_VOBSUB, MSGL_WARN, "No matching VOBSUB language found!\n");
    return -1;
}

/// make sure we seek to the first packet of packets having same pts values.
static void vobsub_queue_reseek(packet_queue_t *queue, unsigned int pts100)
{
    int reseek_count = 0;
    unsigned int lastpts = 0;

    if (queue->current_index > 0
        && (queue->packets[queue->current_index].pts100 == UINT_MAX
            ||  queue->packets[queue->current_index].pts100 > pts100)) {
      // possible pts seek previous, try to check it.
      int i = 1;
      while (queue->current_index >= i
             && queue->packets[queue->current_index-i].pts100 == UINT_MAX)
          ++i;
      if (queue->current_index >= i
          && queue->packets[queue->current_index-i].pts100 > pts100)
          // pts seek previous confirmed, reseek from beginning
          queue->current_index = 0;
    }
    while (queue->current_index < queue->packets_size
           && queue->packets[queue->current_index].pts100 <= pts100) {
        lastpts = queue->packets[queue->current_index].pts100;
        ++queue->current_index;
        ++reseek_count;
    }
    while (reseek_count-- && --queue->current_index) {
        if (queue->packets[queue->current_index-1].pts100 != UINT_MAX &&
            queue->packets[queue->current_index-1].pts100 != lastpts)
            break;
    }
}

int vobsub_get_packet(void *vobhandle, float pts, void** data, int* timestamp)
{
    vobsub_t *vob = (vobsub_t *)vobhandle;
    unsigned int pts100 = 90000 * pts;
    if (vob->spu_streams && 0 <= vobsub_id && (unsigned) vobsub_id < vob->spu_streams_size) {
        packet_queue_t *queue = vob->spu_streams + vobsub_id;

        vobsub_queue_reseek(queue, pts100);

        while (queue->current_index < queue->packets_size) {
            packet_t *pkt = queue->packets + queue->current_index;
            if (pkt->pts100 != UINT_MAX)
                if (pkt->pts100 <= pts100) {
                    ++queue->current_index;
                    *data = pkt->data;
                    *timestamp = pkt->pts100;
                    return pkt->size;
                } else
                    break;
            else
                ++queue->current_index;
        }
    }
    return -1;
}

int vobsub_get_next_packet(void *vobhandle, void** data, int* timestamp)
{
    vobsub_t *vob = (vobsub_t *)vobhandle;
    if (vob->spu_streams && 0 <= vobsub_id && (unsigned) vobsub_id < vob->spu_streams_size) {
        packet_queue_t *queue = vob->spu_streams + vobsub_id;
        if (queue->current_index < queue->packets_size) {
            packet_t *pkt = queue->packets + queue->current_index;
            ++queue->current_index;
            *data = pkt->data;
            *timestamp = pkt->pts100;
            return pkt->size;
        }
    }
    return -1;
}

void vobsub_seek(void * vobhandle, float pts)
{
    vobsub_t * vob = (vobsub_t *)vobhandle;
    packet_queue_t * queue;
    int seek_pts100 = pts * 90000;

    if (vob->spu_streams && 0 <= vobsub_id && (unsigned) vobsub_id < vob->spu_streams_size) {
        /* do not seek if we don't know the id */
        if (vobsub_get_id(vob, vobsub_id) == NULL)
            return;
        queue = vob->spu_streams + vobsub_id;
        queue->current_index = 0;
        vobsub_queue_reseek(queue, seek_pts100);
    }
}

void vobsub_reset(void *vobhandle)
{
    vobsub_t *vob = (vobsub_t *)vobhandle;
    if (vob->spu_streams) {
        unsigned int n = vob->spu_streams_size;
        while (n-- > 0)
            vob->spu_streams[n].current_index = 0;
    }
}

/**********************************************************************
 * Vobsub output
 **********************************************************************/

typedef struct {
    FILE *fsub;
    FILE *fidx;
    unsigned int aid;
} vobsub_out_t;

static void create_idx(vobsub_out_t *me, const unsigned int *palette,
                       unsigned int orig_width, unsigned int orig_height)
{
    int i;
    fprintf(me->fidx,
            "# VobSub index file, v7 (do not modify this line!)\n"
            "#\n"
            "# Generated by %s\n"
            "# See <URL:http://www.mplayerhq.hu/> for more information about MPlayer\n"
            "# See <URL:http://wiki.multimedia.cx/index.php?title=VOBsub> for more information about Vobsub\n"
            "#\n"
            "size: %ux%u\n",
            mplayer_version, orig_width, orig_height);
    if (palette) {
        fputs("palette:", me->fidx);
        for (i = 0; i < 16; ++i) {
            const double y =  palette[i] >> 16 & 0xff,
                         u = (palette[i] >>  8 & 0xff) - 128.0,
                         v = (palette[i]       & 0xff) - 128.0;
            if (i)
                putc(',', me->fidx);
            fprintf(me->fidx, " %02x%02x%02x",
                    av_clip_uint8(y + 1.4022 * u),
                    av_clip_uint8(y - 0.3456 * u - 0.7145 * v),
                    av_clip_uint8(y + 1.7710 * v));
        }
        putc('\n', me->fidx);
    }

    fprintf(me->fidx, "# ON: displays only forced subtitles, OFF: shows everything\n"
            "forced subs: OFF\n");
}

void *vobsub_out_open(const char *basename, const unsigned int *palette,
                      unsigned int orig_width, unsigned int orig_height,
                      const char *id, unsigned int index)
{
    vobsub_out_t *result = NULL;
    char *filename;
    filename = malloc(strlen(basename) + 5);
    if (filename) {
        result = malloc(sizeof(vobsub_out_t));
        if (result) {
            result->aid = index;
            strcpy(filename, basename);
            strcat(filename, ".sub");
            result->fsub = fopen(filename, "ab");
            if (result->fsub == NULL)
                perror("Error: vobsub_out_open subtitle file open failed");
            strcpy(filename, basename);
            strcat(filename, ".idx");
            result->fidx = fopen(filename, "ab");
            if (result->fidx) {
                if (ftell(result->fidx) == 0) {
                    create_idx(result, palette, orig_width, orig_height);
                    /* Make the selected language the default language */
                    fprintf(result->fidx, "\n# Language index in use\nlangidx: %u\n", index);
                }
                fprintf(result->fidx, "\nid: %s, index: %u\n", id ? id : "xx", index);
                /* So that we can check the file now */
                fflush(result->fidx);
            } else
                perror("Error: vobsub_out_open index file open failed");
            free(filename);
        }
    }
    return result;
}

void vobsub_out_close(void *me)
{
    vobsub_out_t *vob = (vobsub_out_t*)me;
    if (vob->fidx)
        fclose(vob->fidx);
    if (vob->fsub)
        fclose(vob->fsub);
    free(vob);
}

void vobsub_out_output(void *me, const unsigned char *packet,
                       int len, double pts)
{
    static double last_pts;
    static int last_pts_set = 0;
    vobsub_out_t *vob = (vobsub_out_t*)me;
    if (vob->fsub) {
        /*  Windows' Vobsub require that every packet is exactly 2kB long */
        unsigned char buffer[2048];
        unsigned char *p;
        int remain = 2048;
        /* Do not output twice a line with the same timestamp, this
           breaks Windows' Vobsub */
        if (vob->fidx && (!last_pts_set || last_pts != pts)) {
            static unsigned int last_h = 9999, last_m = 9999, last_s = 9999, last_ms = 9999;
            unsigned int h, m, ms;
            double s;
            s = pts;
            h = s / 3600;
            s -= h * 3600;
            m = s / 60;
            s -= m * 60;
            ms = (s - (unsigned int) s) * 1000;
            if (ms >= 1000)     /* prevent overfolws or bad float stuff */
                ms = 0;
            if (h != last_h || m != last_m || (unsigned int) s != last_s || ms != last_ms) {
                fprintf(vob->fidx, "timestamp: %02u:%02u:%02u:%03u, filepos: %09lx\n",
                        h, m, (unsigned int) s, ms, ftell(vob->fsub));
                last_h = h;
                last_m = m;
                last_s = (unsigned int) s;
                last_ms = ms;
            }
        }
        last_pts = pts;
        last_pts_set = 1;

        /* Packet start code: Windows' Vobsub needs this */
        p = buffer;
        *p++ = 0;               /* 0x00 */
        *p++ = 0;
        *p++ = 1;
        *p++ = 0xba;
        *p++ = 0x40;
        memset(p, 0, 9);
        p += 9;
        {   /* Packet */
            static unsigned char last_pts[5] = { 0, 0, 0, 0, 0};
            unsigned char now_pts[5];
            int pts_len, pad_len, datalen = len;
            pts *= 90000;
            now_pts[0] = 0x21 | (((unsigned long)pts >> 29) & 0x0e);
            now_pts[1] = ((unsigned long)pts >> 22) & 0xff;
            now_pts[2] = 0x01 | (((unsigned long)pts >> 14) & 0xfe);
            now_pts[3] = ((unsigned long)pts >> 7) & 0xff;
            now_pts[4] = 0x01 | (((unsigned long)pts << 1) & 0xfe);
            pts_len = memcmp(last_pts, now_pts, sizeof(now_pts)) ? sizeof(now_pts) : 0;
            memcpy(last_pts, now_pts, sizeof(now_pts));

            datalen += 3;       /* Version, PTS_flags, pts_len */
            datalen += pts_len;
            datalen += 1;       /* AID */
            pad_len = 2048 - (p - buffer) - 4 /* MPEG ID */ - 2 /* payload len */ - datalen;
            /* XXX - Go figure what should go here!  In any case the
               packet has to be completly filled.  If I can fill it
               with padding (0x000001be) latter I'll do that.  But if
               there is only room for 6 bytes then I can not write a
               padding packet.  So I add some padding in the PTS
               field.  This looks like a dirty kludge.  Oh well... */
            if (pad_len < 0) {
                /* Packet is too big.  Let's try ommiting the PTS field */
                datalen -= pts_len;
                pts_len = 0;
                pad_len = 0;
            } else if (pad_len > 6)
                pad_len = 0;
            datalen += pad_len;

            *p++ = 0;           /* 0x0e */
            *p++ = 0;
            *p++ = 1;
            *p++ = 0xbd;

            *p++ = (datalen >> 8) & 0xff; /* length of payload */
            *p++ = datalen & 0xff;
            *p++ = 0x80;                /* System-2 (.VOB) stream */
            *p++ = pts_len ? 0x80 : 0x00; /* pts_flags */
            *p++ = pts_len + pad_len;
            memcpy(p, now_pts, pts_len);
            p += pts_len;
            memset(p, 0, pad_len);
            p += pad_len;
        }
        *p++ = 0x20 |  vob->aid; /* aid */
        if (fwrite(buffer, p - buffer, 1, vob->fsub) != 1
            || fwrite(packet, len, 1, vob->fsub) != 1)
            perror("ERROR: vobsub write failed");
        else
            remain -= p - buffer + len;

        /* Padding */
        if (remain >= 6) {
            p = buffer;
            *p++ = 0x00;
            *p++ = 0x00;
            *p++ = 0x01;
            *p++ = 0xbe;
            *p++ = (remain - 6) >> 8;
            *p++ = (remain - 6) & 0xff;
            /* for better compression, blank this */
            memset(buffer + 6, 0, remain - (p - buffer));
            if (fwrite(buffer, remain, 1, vob->fsub) != 1)
                perror("ERROR: vobsub padding write failed");
        } else if (remain > 0) {
            /* I don't know what to output.  But anyway the block
               needs to be 2KB big */
            memset(buffer, 0, remain);
            if (fwrite(buffer, remain, 1, vob->fsub) != 1)
                perror("ERROR: vobsub blank padding write failed");
        } else if (remain < 0)
            fprintf(stderr,
                    "\nERROR: wrong thing happenned...\n"
                    "  I wrote a %i data bytes spu packet and that's too long\n", len);
    }
}
