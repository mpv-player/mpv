/*
 * Some code freely inspired from VobSub <URL:http://vobsub.edensrising.com>,
 * with kind permission from Gabest <gabest@freemail.hu>
 */
/* #define HAVE_GETLINE */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"

#include "stream.h"
#include "vobsub.h"
#include "spudec.h"
#include "mp_msg.h"

extern int vobsub_id;

extern int verbose;

#ifdef HAVE_GETLINE
extern ssize_t getline(char **, size_t *, FILE *);
#else
/* FIXME This should go into a general purpose library or even a
   separate file. */
static ssize_t
getline (char **lineptr, size_t *n, FILE *stream)
{
    size_t res = 0;
    int c;
    if (*lineptr == NULL) {
	*lineptr = malloc(4096);
	if (*lineptr)
	    *n = 4096;
    }
    else if (*n == 0) {
	char *tmp = realloc(*lineptr, 4096);
	if (tmp) {
	    *lineptr = tmp;
	    *n = 4096;
	}
    }
    if (*lineptr == NULL || *n == 0)
	return -1;

    for (c = fgetc(stream); c != EOF; c = fgetc(stream)) {
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
#endif

/**********************************************************************
 * MPEG parsing
 **********************************************************************/

typedef struct {
    stream_t *stream;
    unsigned int pts;
    int aid;
    unsigned char *packet;
    unsigned int packet_reserve;
    unsigned int packet_size;
} mpeg_t;

static mpeg_t *
mpeg_open(const char *filename)
{
    mpeg_t *res = malloc(sizeof(mpeg_t));
    int err = res == NULL;
    if (!err) {
	int fd;
	res->pts = 0;
	res->aid = -1;
	res->packet = NULL;
	res->packet_size = 0;
	res->packet_reserve = 0;
	fd = open(filename, O_RDONLY);
	err = fd < 0;
	if (!err) {
	    res->stream = new_stream(fd, STREAMTYPE_FILE);
	    err = res->stream == NULL;
	    if (err)
		close(fd);
	}
	if (err)
	    free(res);
    }
    return err ? NULL : res;
}

static void
mpeg_free(mpeg_t *mpeg)
{
    int fd;
    if (mpeg->packet)
	free(mpeg->packet);
    fd = mpeg->stream->fd;
    free_stream(mpeg->stream);
    close(fd);
    free(mpeg);
}

static int
mpeg_eof(mpeg_t *mpeg)
{
    return stream_eof(mpeg->stream);
}

static off_t
mpeg_tell(mpeg_t *mpeg)
{
    return stream_tell(mpeg->stream);
}

static int
mpeg_run(mpeg_t *mpeg)
{
    unsigned int len, idx, version;
    int c;
    /* Goto start of a packet, it starts with 0x000001?? */
    const unsigned char wanted[] = { 0, 0, 1 };
    unsigned char buf[5];

    mpeg->aid = -1;
    mpeg->packet_size = 0;
    if (stream_read(mpeg->stream, buf, 4) != 4)
	return -1;
    while (memcmp(buf, wanted, sizeof(wanted)) != 0) {
	c = stream_read_char(mpeg->stream);
	if (c < 0)
	    return -1;
	memmove(buf, buf + 1, 3);
	buf[3] = c;
    }
    switch (buf[3]) {
    case 0xb9:			/* System End Code */
	break;
    case 0xba:			/* Packet start code */
	c = stream_read_char(mpeg->stream);
	if (c < 0)
	    return -1;
	if ((c & 0xc0) == 0x40)
	    version = 4;
	else if ((c & 0xf0) == 0x20)
	    version = 2;
	else {
	    fprintf(stderr, "Unsupported MPEG version: 0x%02x", c);
	    return -1;
	}
	if (version == 4) {
	    if (!stream_skip(mpeg->stream, 9))
		return -1;
	}
	else if (version == 2) {
	    if (!stream_skip(mpeg->stream, 7))
		return -1;
	}
	else
	    abort();
	break;
    case 0xbd:			/* packet */
	if (stream_read(mpeg->stream, buf, 2) != 2)
	    return -1;
	len = buf[0] << 8 | buf[1];
	idx = mpeg_tell(mpeg);
	c = stream_read_char(mpeg->stream);
	if (c < 0)
	    return -1;
	if ((c & 0xC0) == 0x40) { /* skip STD scale & size */
	    if (stream_read_char(mpeg->stream) < 0)
		return -1;
	    c = stream_read_char(mpeg->stream);
	    if (c < 0)
		return -1;
	}
	if ((c & 0xf0) == 0x20) { /* System-1 stream timestamp */
	    /* Do we need this? */
	    abort();
	}
	else if ((c & 0xf0) == 0x30) {
	    /* Do we need this? */
	    abort();
	}
	else if ((c & 0xc0) == 0x80) { /* System-2 (.VOB) stream */
	    unsigned int pts_flags, hdrlen, dataidx;
	    c = stream_read_char(mpeg->stream);
	    if (c < 0)
		return -1;
	    pts_flags = c;
	    c = stream_read_char(mpeg->stream);
	    if (c < 0)
		return -1;
	    hdrlen = c;
	    dataidx = mpeg_tell(mpeg) + hdrlen;
	    if (dataidx > idx + len) {
		fprintf(stderr, "Invalid header length: %d (total length: %d, idx: %d, dataidx: %d)\n",
			hdrlen, len, idx, dataidx);
		return -1;
	    }
	    if ((pts_flags & 0xc0) == 0x80) {
		if (stream_read(mpeg->stream, buf, 5) != 5)
		    return -1;
		if (!(((buf[0] & 0xf0) == 0x20) && (buf[0] & 1) && (buf[2] & 1) &&  (buf[4] & 1))) {
		    fprintf(stderr, "vobsub PTS error: 0x%02x %02x%02x %02x%02x \n",
			    buf[0], buf[1], buf[2], buf[3], buf[4]);
		    mpeg->pts = 0;
		}
		else
		    mpeg->pts = ((buf[0] & 0x0e) << 29 | buf[1] << 22 | (buf[2] & 0xfe) << 14
			| buf[3] << 7 | (buf[4] >> 1)) / 900;
	    }
	    else /* if ((pts_flags & 0xc0) == 0xc0) */ {
		/* what's this? */
		/* abort(); */
	    }
	    stream_seek(mpeg->stream, dataidx);
	    mpeg->aid = stream_read_char(mpeg->stream);
	    if (mpeg->aid < 0) {
		fprintf(stderr, "Bogus aid %d\n", mpeg->aid);
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
		perror("malloc failure");
		mpeg->packet_reserve = 0;
		mpeg->packet_size = 0;
		return -1;
	    }
	    if (stream_read(mpeg->stream, mpeg->packet, mpeg->packet_size) != mpeg->packet_size) {
		perror("stream_read failure");
		mpeg->packet_size = 0;
		return -1;
	    }
	    idx = len;
	}
	break;
    case 0xbe:			/* Padding */
	if (stream_read(mpeg->stream, buf, 2) != 2)
	    return -1;
	len = buf[0] << 8 | buf[1];
	if (len > 0 && !stream_skip(mpeg->stream, len))
	    return -1;
	break;
    default:
	if (0xc0 <= buf[3] && buf[3] < 0xf0) {
	    /* MPEG audio or video */
	    if (stream_read(mpeg->stream, buf, 2) != 2)
		return -1;
	    len = buf[0] << 8 | buf[1];
	    if (len > 0 && !stream_skip(mpeg->stream, len))
		return -1;
		
	}
	else {
	    fprintf(stderr, "unknown header 0x%02X%02X%02X%02X\n",
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

static void
packet_construct(packet_t *pkt)
{
    pkt->pts100 = 0;
    pkt->filepos = 0;
    pkt->size = 0;
    pkt->data = NULL;
}

static void
packet_destroy(packet_t *pkt)
{
    if (pkt->data)
	free(pkt->data);
}

static void
packet_queue_construct(packet_queue_t *queue)
{
    queue->id = NULL;
    queue->packets = NULL;
    queue->packets_reserve = 0;
    queue->packets_size = 0;
    queue->current_index = 0;
}

static void
packet_queue_destroy(packet_queue_t *queue)
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
static int
packet_queue_ensure(packet_queue_t *queue, unsigned int needed_size)
{
    if (queue->packets_reserve < needed_size) {
	if (queue->packets) {
	    packet_t *tmp = realloc(queue->packets, 2 * queue->packets_reserve * sizeof(packet_t));
	    if (tmp == NULL) {
		perror("realloc failure");
		return -1;
	    }
	    queue->packets = tmp;
	    queue->packets_reserve *= 2;
	}
	else {
	    queue->packets = malloc(sizeof(packet_t));
	    if (queue->packets == NULL) {
		perror("malloc failure");
		return -1;
	    }
	    queue->packets_reserve = 1;
	}
    }
    return 0;
}

/* add one more packet */
static int
packet_queue_grow(packet_queue_t *queue)
{
    if (packet_queue_ensure(queue, queue->packets_size + 1) < 0) 
	return -1;
    packet_construct(queue->packets + queue->packets_size);
    ++queue->packets_size;
    return 0;
}

/* insert a new packet, duplicating pts from the current one */
static int
packet_queue_insert(packet_queue_t *queue)
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
 * Vosub
 **********************************************************************/

typedef struct {
    void *spudec;
    unsigned int palette[16];
    unsigned int orig_frame_width, orig_frame_height;
    unsigned int origin_x, origin_y;
    /* index */
    packet_queue_t *spu_streams;
    unsigned int spu_streams_size;
    unsigned int spu_streams_current;
} vobsub_t;

/* Make sure that the spu stream idx exists. */
static int
vobsub_ensure_spu_stream(vobsub_t *vob, unsigned int index)
{
    if (index >= vob->spu_streams_size) {
	/* This is a new stream */
	if (vob->spu_streams) {
	    packet_queue_t *tmp = realloc(vob->spu_streams, (index + 1) * sizeof(packet_queue_t));
	    if (tmp == NULL) {
		perror("vobsub_ensure_spu_stream: realloc failure");
		return -1;
	    }
	    vob->spu_streams = tmp;
	}
	else {
	    vob->spu_streams = malloc((index + 1) * sizeof(packet_queue_t));
	    if (vob->spu_streams == NULL) {
		perror("vobsub_ensure_spu_stream: malloc failure");
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

static int
vobsub_add_id(vobsub_t *vob, const char *id, size_t idlen, const unsigned int index)
{
    if (vobsub_ensure_spu_stream(vob, index) < 0)
	return -1;
    if (id && idlen) {
	if (vob->spu_streams[index].id)
	    free(vob->spu_streams[index].id);
	vob->spu_streams[index].id = malloc(idlen + 1);
	if (vob->spu_streams[index].id == NULL) {
	    perror("vobsub_add_id: malloc failure");
	    return -1;
	}
	vob->spu_streams[index].id[idlen] = 0;
	memcpy(vob->spu_streams[index].id, id, idlen);
    }
    vob->spu_streams_current = index;
    if (verbose)
	fprintf(stderr, "[vobsub] subtitle (vobsubid): %d language %s\n",
		index, vob->spu_streams[index].id);
    return 0;
}

static int
vobsub_add_timestamp(vobsub_t *vob, off_t filepos, unsigned int ms)
{
    packet_queue_t *queue;
    packet_t *pkt;
    if (vob->spu_streams == 0) {
	fprintf(stderr, "[vobsub] warning, binning some index entries.  Check your index file\n");
	return -1;
    }
    queue = vob->spu_streams + vob->spu_streams_current;
    if (packet_queue_grow(queue) >= 0) {
	pkt = queue->packets + (queue->packets_size - 1);
	pkt->filepos = filepos;
	pkt->pts100 = ms * 90;
	return 0;
    }
    return -1;
}

static int
vobsub_parse_id(vobsub_t *vob, const char *line)
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

static int
vobsub_parse_timestamp(vobsub_t *vob, const char *line)
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
    return vobsub_add_timestamp(vob, filepos, ms + 1000 * (s + 60 * (m + 60 * h)));
}

static int
vobsub_parse_size(vobsub_t *vob, const char *line)
{
    // size: WWWxHHH
    char *p;
    while (isspace(*line))
	++line;
    if (!isdigit(*line))
	return -1;
    vob->orig_frame_width = strtoul(line, &p, 10);
    if (*p != 'x')
	return -1;
    ++p;
    vob->orig_frame_height = strtoul(p, NULL, 10);
    return 0;
}

static int
vobsub_parse_origin(vobsub_t *vob, const char *line)
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

static int
vobsub_parse_palette(vobsub_t *vob, const char *line)
{
    // palette: XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX
    unsigned int n;
    n = 0;
    while (1) {
	const char *p;
	while (isspace(*line))
	    ++line;
	p = line;
	while (isxdigit(*p))
	    ++p;
	if (p - line != 6)
	    return -1;
	vob->palette[n++] = strtoul(line, NULL, 16);
	if (n == 16)
	    break;
	if (*p == ',')
	    ++p;
	line = p;
    }
    return 0;
}

static int
vobsub_parse_one_line(vobsub_t *vob, FILE *fd)
{
    ssize_t line_size;
    int res = -1;
    do {
	int line_reserve = 0;
	char *line = NULL;
	line_size = getline(&line, &line_reserve, fd);
	if (line_size < 0) {
	    if (line)
		free(line);
	    break;
	}
	if (*line == 0 || *line == '\r' || *line == '\n' || *line == '#')
	    continue;
	else if (strncmp("id:", line, 3) == 0)
	    res = vobsub_parse_id(vob, line + 3);
	else if (strncmp("palette:", line, 8) == 0)
	    res = vobsub_parse_palette(vob, line + 8);
	else if (strncmp("size:", line, 5) == 0)
	    res = vobsub_parse_size(vob, line + 5);
	else if (strncmp("org:", line, 4) == 0)
	    res = vobsub_parse_origin(vob, line + 4);
	else if (strncmp("timestamp:", line, 10) == 0)
	    res = vobsub_parse_timestamp(vob, line + 10);
	else {
	    if (verbose)
		fprintf(stderr, "vobsub: ignoring %s", line);
	    continue;
	}
	if (res < 0)
	    fprintf(stderr, "ERROR in %s", line);
	break;
    } while (1);
    return res;
}

int
vobsub_parse_ifo(const char *const name, unsigned int *palette, unsigned int *width, unsigned int *height, int force)
{
    int res = -1;
    FILE *fd = fopen(name, "rb");
    if (fd == NULL)
	perror("Can't open IFO file");
    else {
	// parse IFO header
	unsigned char block[0x800];
	const char *const ifo_magic = "DVDVIDEO-VTS";
	if (fread(block, sizeof(block), 1, fd) != 1) {
	    if (force)
		perror("Can't read IFO header");
	} else if (memcmp(block, ifo_magic, strlen(ifo_magic) + 1))
	    fprintf(stderr, "Bad magic in IFO header\n");
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
		fprintf(stderr, "Unknown resolution %d \n", resolution);
	    }
	    if (fseek(fd, pgci_sector * sizeof(block), SEEK_SET)
		|| fread(block, sizeof(block), 1, fd) != 1)
		perror("Can't read IFO PGCI");
	    else {
		unsigned long idx;
		unsigned long pgc_offset = block[0xc] << 24 | block[0xd] << 16
		    | block[0xe] << 8 | block[0xf];
		for (idx = 0; idx < 16; ++idx) {
		    unsigned char *p = block + pgc_offset + 0xa4 + 4 * idx;
		    palette[idx] = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
		}
		res = 0;
	    }
	}
	fclose(fd);
    }
    return res;
}

void *
vobsub_open(const char *const name, const int force)
{
    vobsub_t *vob = malloc(sizeof(vobsub_t));
    if (vob) {
	char *buf;
	vob->spudec = NULL;
	vob->orig_frame_width = 0;
	vob->orig_frame_height = 0;
	vob->spu_streams = NULL;
	vob->spu_streams_size = 0;
	vob->spu_streams_current = 0;
	buf = malloc((strlen(name) + 5) * sizeof(char));
	if (buf) {
	    FILE *fd;
	    mpeg_t *mpg;
	    /* read in the info file */
	    strcpy(buf, name);
	    strcat(buf, ".ifo");
	    vobsub_parse_ifo(buf, vob->palette, &vob->orig_frame_width, &vob->orig_frame_height, force);
	    /* read in the index */
	    strcpy(buf, name);
	    strcat(buf, ".idx");
	    fd = fopen(buf, "rb");
	    if (fd == NULL) {
		if(force)
                    perror("VobSub: Can't open IDX file");
                else
                    return NULL;
	    } else {
		while (vobsub_parse_one_line(vob, fd) >= 0)
		    /* NOOP */ ;
		fclose(fd);
	    }
	    if (vob->orig_frame_width && vob->orig_frame_height)
		vob->spudec = spudec_new_scaled(vob->palette, vob->orig_frame_width, vob->orig_frame_height);

	    /* read the indexed mpeg_stream */
	    strcpy(buf, name);
	    strcat(buf, ".sub");
	    mpg = mpeg_open(buf);
	    if (mpg == NULL) {
                if(force)
		    perror("VobSub: Can't open SUB file");
	    } else {
		long last_pts_diff = 0;
		while (!mpeg_eof(mpg)) {
		    off_t pos = mpeg_tell(mpg);
		    if (mpeg_run(mpg) < 0) {
			if (!mpeg_eof(mpg))
			    perror("mpeg_run error");
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
				    last_pts_diff = pkt->pts100 - mpg->pts;
				    /* FIXME: should not use mpg_sub internal informations, make a copy */
				    pkt->data = mpg->packet;
				    pkt->size = mpg->packet_size;
				    mpg->packet = NULL;
				    mpg->packet_reserve = 0;
				    mpg->packet_size = 0;
				}
			    }
			    else
				fprintf(stderr, "don't know what to do with subtitle #%u\n", sid);
			}
		    }
		}
		vob->spu_streams_current = vob->spu_streams_size;
		while (vob->spu_streams_current-- > 0)
		    vob->spu_streams[vob->spu_streams_current].current_index = 0;
		mpeg_free(mpg);
	    }
	    free(buf);
	}
    }
    return vob;
}

void
vobsub_close(void *this)
{
    vobsub_t *vob = (vobsub_t *)this;
    if (vob->spudec)
	spudec_free(vob->spudec);
    if (vob->spu_streams) {
	while (vob->spu_streams_size--)
	    packet_queue_destroy(vob->spu_streams + vob->spu_streams_size);
	free(vob->spu_streams);
    }
    free(vob);
}

void vobsub_draw(void *this, int dxs, int dys, void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride))
{
    vobsub_t *vob = (vobsub_t *)this;
    if (vob->spudec) {
	spudec_draw_scaled(vob->spudec, dxs, dys, draw_alpha);
    }	
}

void
vobsub_process(void *vobhandle, float pts)
{
    vobsub_t *vob = (vobsub_t *)vobhandle;
    unsigned int pts100 = 90000 * pts;
    if (vob->spudec) {
	spudec_heartbeat(vob->spudec, pts100);
	if (vob->spu_streams && 0 <= vobsub_id && (unsigned) vobsub_id < vob->spu_streams_size) {
	    packet_queue_t *queue = vob->spu_streams + vobsub_id;
	    while (queue->current_index < queue->packets_size) {
		packet_t *pkt = queue->packets + queue->current_index;
		if (pkt->pts100 <= pts100) {
		    spudec_assemble(vob->spudec, pkt->data, pkt->size, pkt->pts100);
		    ++queue->current_index;
		}
		else
		    break;
	    }
	}
    }
}

void
vobsub_reset(void *vobhandle)
{
    vobsub_t *vob = (vobsub_t *)vobhandle;
    if (vob->spu_streams) {
	unsigned int n = vob->spu_streams_size;
	while (n-- > 0)
	    vob->spu_streams[n].current_index = 0;
    }
    if (vob->spudec)
	spudec_reset(vob->spudec);
}
