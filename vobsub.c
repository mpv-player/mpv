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
#include "version.h"

#include "vobsub.h"
#include "libvo/video_out.h"
#include "spudec.h"
#include "mp_msg.h"

#define MIN(a, b)	((a)<(b)?(a):(b))
#define MAX(a, b)	((a)>(b)?(a):(b))

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
    FILE *stream;
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
	res->pts = 0;
	res->aid = -1;
	res->packet = NULL;
	res->packet_size = 0;
	res->packet_reserve = 0;
	res->stream = fopen(filename, "r");
	err = res->stream == NULL;
	if (err)
	    perror("fopen Vobsub file failed");
	if (err)
	    free(res);
    }
    return err ? NULL : res;
}

static void
mpeg_free(mpeg_t *mpeg)
{
    if (mpeg->packet)
	free(mpeg->packet);
    if (mpeg->stream)
	fclose(mpeg->stream);
    free(mpeg);
}

static int
mpeg_eof(mpeg_t *mpeg)
{
    return feof(mpeg->stream);
}

static off_t
mpeg_tell(mpeg_t *mpeg)
{
    return ftell(mpeg->stream);
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
    if (fread(buf, 4, 1, mpeg->stream) != 1)
	return -1;
    while (memcmp(buf, wanted, sizeof(wanted)) != 0) {
	c = getc(mpeg->stream);
	if (c < 0)
	    return -1;
	memmove(buf, buf + 1, 3);
	buf[3] = c;
    }
    switch (buf[3]) {
    case 0xb9:			/* System End Code */
	break;
    case 0xba:			/* Packet start code */
	c = getc(mpeg->stream);
	if (c < 0)
	    return -1;
	if ((c & 0xc0) == 0x40)
	    version = 4;
	else if ((c & 0xf0) == 0x20)
	    version = 2;
	else {
	    mp_msg(MSGT_VOBSUB,MSGL_ERR, "Unsupported MPEG version: 0x%02x", c);
	    return -1;
	}
	if (version == 4) {
	    if (fseek(mpeg->stream, 9, SEEK_CUR))
		return -1;
	}
	else if (version == 2) {
	    if (fseek(mpeg->stream, 7, SEEK_CUR))
		return -1;
	}
	else
	    abort();
	break;
    case 0xbd:			/* packet */
	if (fread(buf, 2, 1, mpeg->stream) != 1)
	    return -1;
	len = buf[0] << 8 | buf[1];
	idx = mpeg_tell(mpeg);
	c = getc(mpeg->stream);
	if (c < 0)
	    return -1;
	if ((c & 0xC0) == 0x40) { /* skip STD scale & size */
	    if (getc(mpeg->stream) < 0)
		return -1;
	    c = getc(mpeg->stream);
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
	    c = getc(mpeg->stream);
	    if (c < 0)
		return -1;
	    pts_flags = c;
	    c = getc(mpeg->stream);
	    if (c < 0)
		return -1;
	    hdrlen = c;
	    dataidx = mpeg_tell(mpeg) + hdrlen;
	    if (dataidx > idx + len) {
		mp_msg(MSGT_VOBSUB,MSGL_ERR, "Invalid header length: %d (total length: %d, idx: %d, dataidx: %d)\n",
			hdrlen, len, idx, dataidx);
		return -1;
	    }
	    if ((pts_flags & 0xc0) == 0x80) {
		if (fread(buf, 5, 1, mpeg->stream) != 1)
		    return -1;
		if (!(((buf[0] & 0xf0) == 0x20) && (buf[0] & 1) && (buf[2] & 1) &&  (buf[4] & 1))) {
		    mp_msg(MSGT_VOBSUB,MSGL_ERR, "vobsub PTS error: 0x%02x %02x%02x %02x%02x \n",
			    buf[0], buf[1], buf[2], buf[3], buf[4]);
		    mpeg->pts = 0;
		}
		else
		    mpeg->pts = ((buf[0] & 0x0e) << 29 | buf[1] << 22 | (buf[2] & 0xfe) << 14
			| buf[3] << 7 | (buf[4] >> 1));
	    }
	    else /* if ((pts_flags & 0xc0) == 0xc0) */ {
		/* what's this? */
		/* abort(); */
	    }
	    fseek(mpeg->stream, dataidx, SEEK_SET);
	    mpeg->aid = getc(mpeg->stream);
	    if (mpeg->aid < 0) {
		mp_msg(MSGT_VOBSUB,MSGL_ERR, "Bogus aid %d\n", mpeg->aid);
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
		mp_msg(MSGT_VOBSUB,MSGL_FATAL,"malloc failure");
		mpeg->packet_reserve = 0;
		mpeg->packet_size = 0;
		return -1;
	    }
	    if (fread(mpeg->packet, mpeg->packet_size, 1, mpeg->stream) != 1) {
		mp_msg(MSGT_VOBSUB,MSGL_ERR,"fread failure");
		mpeg->packet_size = 0;
		return -1;
	    }
	    idx = len;
	}
	break;
    case 0xbe:			/* Padding */
	if (fread(buf, 2, 1, mpeg->stream) != 1)
	    return -1;
	len = buf[0] << 8 | buf[1];
	if (len > 0 && fseek(mpeg->stream, len, SEEK_CUR))
	    return -1;
	break;
    default:
	if (0xc0 <= buf[3] && buf[3] < 0xf0) {
	    /* MPEG audio or video */
	    if (fread(buf, 2, 1, mpeg->stream) != 1)
		return -1;
	    len = buf[0] << 8 | buf[1];
	    if (len > 0 && fseek(mpeg->stream, len, SEEK_CUR))
		return -1;
		
	}
	else {
	    mp_msg(MSGT_VOBSUB,MSGL_ERR,"unknown header 0x%02X%02X%02X%02X\n",
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
		mp_msg(MSGT_VOBSUB,MSGL_FATAL,"realloc failure");
		return -1;
	    }
	    queue->packets = tmp;
	    queue->packets_reserve *= 2;
	}
	else {
	    queue->packets = malloc(sizeof(packet_t));
	    if (queue->packets == NULL) {
		mp_msg(MSGT_VOBSUB,MSGL_FATAL,"malloc failure");
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
 * Vobsub
 **********************************************************************/

typedef struct {
    unsigned int palette[16];
    unsigned int cuspal[4];
    int delay;
    unsigned int custom;
    unsigned int have_palette;
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
		mp_msg(MSGT_VOBSUB,MSGL_ERR,"vobsub_ensure_spu_stream: realloc failure");
		return -1;
	    }
	    vob->spu_streams = tmp;
	}
	else {
	    vob->spu_streams = malloc((index + 1) * sizeof(packet_queue_t));
	    if (vob->spu_streams == NULL) {
		mp_msg(MSGT_VOBSUB,MSGL_ERR,"vobsub_ensure_spu_stream: malloc failure");
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
	    mp_msg(MSGT_VOBSUB,MSGL_FATAL,"vobsub_add_id: malloc failure");
	    return -1;
	}
	vob->spu_streams[index].id[idlen] = 0;
	memcpy(vob->spu_streams[index].id, id, idlen);
    }
    vob->spu_streams_current = index;
    if (verbose)
	mp_msg(MSGT_VOBSUB,MSGL_V,"[vobsub] subtitle (vobsubid): %d language %s\n",
		index, vob->spu_streams[index].id);
    return 0;
}

static int
vobsub_add_timestamp(vobsub_t *vob, off_t filepos, unsigned int ms)
{
    packet_queue_t *queue;
    packet_t *pkt;
    if (vob->spu_streams == 0) {
	mp_msg(MSGT_VOBSUB,MSGL_WARN,"[vobsub] warning, binning some index entries.  Check your index file\n");
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
    return vobsub_add_timestamp(vob, filepos, vob->delay + ms + 1000 * (s + 60 * (m + 60 * h)));
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
	int r, g, b, y, u, v, tmp;
	while (isspace(*line))
	    ++line;
	p = line;
	while (isxdigit(*p))
	    ++p;
	if (p - line != 6)
	    return -1;
	tmp = strtoul(line, NULL, 16);
	r = tmp >> 16 & 0xff;
	g = tmp >> 8 & 0xff;
	b = tmp & 0xff;
	y = MIN(MAX((int)(0.1494 * r + 0.6061 * g + 0.2445 * b), 0), 0xff);
	u = MIN(MAX((int)(0.6066 * r - 0.4322 * g - 0.1744 * b) + 128, 0), 0xff);
	v = MIN(MAX((int)(-0.08435 * r - 0.3422 * g + 0.4266 * b) + 128, 0), 0xff);
	vob->palette[n++] = y << 16 | u << 8 | v;
	if (n == 16)
	    break;
	if (*p == ',')
	    ++p;
	line = p;
    }
    vob->have_palette = 1;
    return 0;
}

static int
vobsub_parse_custom(vobsub_t *vob, const char *line)
{
    //custom colors: OFF/ON(0/1)
    if ((strncmp("ON", line + 15, 2) == 0)||strncmp("1", line + 15, 1) == 0)
        vob->custom=1;
    else if ((strncmp("OFF", line + 15, 3) == 0)||strncmp("0", line + 15, 1) == 0)
        vob->custom=0;
    else
        return -1;
    return 0;
}

static int
vobsub_parse_cuspal(vobsub_t *vob, const char *line)
{
    //colors: XXXXXX, XXXXXX, XXXXXX, XXXXXX
    unsigned int n;
    n = 0;
    line += 40;
    while(1){
    	const char *p;
	while (isspace(*line))
	    ++line;
	p=line;
	while (isxdigit(*p))
	    ++p;
	if (p - line !=6)
	    return -1;
	vob->cuspal[n++] = strtoul(line, NULL,16);
	if (n==4)
	    break;
	if(*p == ',')
	    ++p;
	line = p;
    }
    return 0;
}

/* don't know how to use tridx */
static int
vobsub_parse_tridx(const char *line)
{
    //tridx: XXXX
    int tridx;
    tridx = strtoul((line + 26), NULL, 16);
    tridx = ((tridx&0x1000)>>12) | ((tridx&0x100)>>7) | ((tridx&0x10)>>2) | ((tridx&1)<<3);
    return tridx;
}

static int
vobsub_parse_delay(vobsub_t *vob, const char *line)
{
    int h, m, s, ms;
    int forward = 1;
    if (*(line + 7) == '+'){
    	forward = 1;
	line++;
    }
    else if (*(line + 7) == '-'){
    	forward = -1;
	line++;
    }
    mp_msg(MSGT_SPUDEC,MSGL_V, "forward=%d", forward);
    h = atoi(line + 7);
    mp_msg(MSGT_VOBSUB,MSGL_V, "h=%d," ,h);
    m = atoi(line + 10);
    mp_msg(MSGT_VOBSUB,MSGL_V, "m=%d,", m);
    s = atoi(line + 13);
    mp_msg(MSGT_VOBSUB,MSGL_V, "s=%d,", s);
    ms = atoi(line + 16);
    mp_msg(MSGT_VOBSUB,MSGL_V, "ms=%d", ms);
    vob->delay = ms + 1000 * (s + 60 * (m + 60 * h)) * forward;
    return 0;
}

static int
vobsub_set_lang(const char *line)
{
    if (vobsub_id == -1)
        vobsub_id = atoi(line + 8);
    return 0;
}

static int
vobsub_parse_one_line(vobsub_t *vob, FILE *fd)
{
    ssize_t line_size;
    int res = -1;
    do {
	size_t line_reserve = 0;
	char *line = NULL;
	line_size = getline(&line, &line_reserve, fd);
	if (line_size < 0) {
	    if (line)
		free(line);
	    break;
	}
	if (*line == 0 || *line == '\r' || *line == '\n' || *line == '#')
	    continue;
	else if (strncmp("langidx:", line, 8) == 0)
	    res = vobsub_set_lang(line);
	else if (strncmp("delay:", line, 6) == 0)
	    res = vobsub_parse_delay(vob, line);
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
	else if (strncmp("custom colors:", line, 14) == 0)
	    //custom colors: ON/OFF, tridx: XXXX, colors: XXXXXX, XXXXXX, XXXXXX,XXXXXX
	    res = vobsub_parse_cuspal(vob, line) + vobsub_parse_tridx(line) + vobsub_parse_custom(vob, line);
	else {
	    if (verbose)
		mp_msg(MSGT_VOBSUB,MSGL_V, "vobsub: ignoring %s", line);
	    continue;
	}
	if (res < 0)
	    mp_msg(MSGT_VOBSUB,MSGL_ERR,  "ERROR in %s", line);
	break;
    } while (1);
    return res;
}

int
vobsub_parse_ifo(void* this, const char *const name, unsigned int *palette, unsigned int *width, unsigned int *height, int force,
		 int sid, char *langid)
{
    vobsub_t *vob = (vobsub_t*)this;
    int res = -1;
    FILE *fd = fopen(name, "rb");
    if (fd == NULL) {
        if (force)
	    mp_msg(MSGT_VOBSUB,MSGL_ERR, "Can't open IFO file");
    } else {
	// parse IFO header
	unsigned char block[0x800];
	const char *const ifo_magic = "DVDVIDEO-VTS";
	if (fread(block, sizeof(block), 1, fd) != 1) {
	    if (force)
		mp_msg(MSGT_VOBSUB,MSGL_ERR, "Can't read IFO header");
	} else if (memcmp(block, ifo_magic, strlen(ifo_magic) + 1))
	    mp_msg(MSGT_VOBSUB,MSGL_ERR, "Bad magic in IFO header\n");
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
		mp_msg(MSGT_VOBSUB,MSGL_WARN,"Vobsub: Unknown resolution %d \n", resolution);
	    }
	    if (langid && 0 <= sid && sid < 32) {
		unsigned char *tmp = block + 0x256 + sid * 6 + 2;
		langid[0] = tmp[0];
		langid[1] = tmp[1];
		langid[2] = 0;
	    }
	    if (fseek(fd, pgci_sector * sizeof(block), SEEK_SET)
		|| fread(block, sizeof(block), 1, fd) != 1)
		mp_msg(MSGT_VOBSUB,MSGL_ERR, "Can't read IFO PGCI");
	    else {
		unsigned long idx;
		unsigned long pgc_offset = block[0xc] << 24 | block[0xd] << 16
		    | block[0xe] << 8 | block[0xf];
		for (idx = 0; idx < 16; ++idx) {
		    unsigned char *p = block + pgc_offset + 0xa4 + 4 * idx;
		    palette[idx] = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
		}
		if(vob)
		  vob->have_palette = 1;
		res = 0;
	    }
	}
	fclose(fd);
    }
    return res;
}

void *
vobsub_open(const char *const name,const char *const ifo,const int force,void** spu)
{
    vobsub_t *vob = malloc(sizeof(vobsub_t));
    if(spu)
      *spu = NULL;
    if (vob) {
	char *buf;
	vob->custom = 0;
	vob->have_palette = 0;
	vob->orig_frame_width = 0;
	vob->orig_frame_height = 0;
	vob->spu_streams = NULL;
	vob->spu_streams_size = 0;
	vob->spu_streams_current = 0;
	vob->delay = 0;
	buf = malloc((strlen(name) + 5) * sizeof(char));
	if (buf) {
	    FILE *fd;
	    mpeg_t *mpg;
	    /* read in the info file */
	    if(!ifo) {
	      strcpy(buf, name);
	      strcat(buf, ".ifo");
	      vobsub_parse_ifo(vob,buf, vob->palette, &vob->orig_frame_width, &vob->orig_frame_height, force, -1, NULL);
	    } else
	      vobsub_parse_ifo(vob,ifo, vob->palette, &vob->orig_frame_width, &vob->orig_frame_height, force, -1, NULL);
	    /* read in the index */
	    strcpy(buf, name);
	    strcat(buf, ".idx");
	    fd = fopen(buf, "rb");
	    if (fd == NULL) {
		if(force)
		  mp_msg(MSGT_VOBSUB,MSGL_ERR,"VobSub: Can't open IDX file");
		else {
		  free(buf);
		  free(vob);
		  return NULL;
		}
	    } else {
		while (vobsub_parse_one_line(vob, fd) >= 0)
		    /* NOOP */ ;
		fclose(fd);
	    }
	    /* if no palette in .idx then use custom colors */
	    if ((vob->custom == 0)&&(vob->have_palette!=1))
		vob->custom = 1;
	    if (spu && vob->orig_frame_width && vob->orig_frame_height)
	      *spu = spudec_new_scaled_vobsub(vob->palette, vob->cuspal, vob->custom, vob->orig_frame_width, vob->orig_frame_height);

	    /* read the indexed mpeg_stream */
	    strcpy(buf, name);
	    strcat(buf, ".sub");
	    mpg = mpeg_open(buf);
	    if (mpg == NULL) {
	      if(force)
		mp_msg(MSGT_VOBSUB,MSGL_ERR,"VobSub: Can't open SUB file");
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
			    mp_msg(MSGT_VOBSUB,MSGL_ERR,"mpeg_run error");
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
				    if (queue->packets_size > 1)
					last_pts_diff = pkt->pts100 - mpg->pts;
				    else
					pkt->pts100 = mpg->pts;
				    /* FIXME: should not use mpg_sub internal informations, make a copy */
				    pkt->data = mpg->packet;
				    pkt->size = mpg->packet_size;
				    mpg->packet = NULL;
				    mpg->packet_reserve = 0;
				    mpg->packet_size = 0;
				}
			    }
			    else
				mp_msg(MSGT_VOBSUB,MSGL_WARN, "don't know what to do with subtitle #%u\n", sid);
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
    if (vob->spu_streams) {
	while (vob->spu_streams_size--)
	    packet_queue_destroy(vob->spu_streams + vob->spu_streams_size);
	free(vob->spu_streams);
    }
    free(vob);
}

int
vobsub_get_packet(void *vobhandle, float pts,void** data, int* timestamp) {
  vobsub_t *vob = (vobsub_t *)vobhandle;
  unsigned int pts100 = 90000 * pts;
  if (vob->spu_streams && 0 <= vobsub_id && (unsigned) vobsub_id < vob->spu_streams_size) {
    packet_queue_t *queue = vob->spu_streams + vobsub_id;
    while (queue->current_index < queue->packets_size) {
      packet_t *pkt = queue->packets + queue->current_index;
      if (pkt->pts100 <= pts100) {
	++queue->current_index;
	*data = pkt->data;
	*timestamp = pkt->pts100;
	return pkt->size;
      } else break;
    }
  }
  return -1;
}

int
vobsub_get_next_packet(void *vobhandle, void** data, int* timestamp)
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

void
vobsub_reset(void *vobhandle)
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

static void
create_idx(vobsub_out_t *me, const unsigned int *palette, unsigned int orig_width, unsigned int orig_height)
{
    int i;
    fprintf(me->fidx,
	    "# VobSub index file, v7 (do not modify this line!)\n"
	    "#\n"
	    "# Generated by MPlayer " VERSION "\n"
	    "# See <URL:http://www.mplayerhq.hu/> for more information about MPlayer\n"
	    "# See <URL:http://vobsub.edensrising.com/> for more information about Vobsub\n"
	    "#\n"
	    "size: %ux%u\n",
	    orig_width, orig_height);
    if (palette) {
	fputs("palette:", me->fidx);
	for (i = 0; i < 16; ++i) {
	    const double y = palette[i] >> 16 & 0xff,
		u = (palette[i] >> 8 & 0xff) - 128.0,
		v = (palette[i] & 0xff) - 128.0;
	    if (i)
		putc(',', me->fidx);
	    fprintf(me->fidx, " %02x%02x%02x",
		    MIN(MAX((int)(y + 1.4022 * u), 0), 0xff),
		    MIN(MAX((int)(y - 0.3456 * u - 0.7145 * v), 0), 0xff),
		    MIN(MAX((int)(y + 1.7710 * v), 0), 0xff));
	}
	putc('\n', me->fidx);
    }
}

void *
vobsub_out_open(const char *basename, const unsigned int *palette,
		unsigned int orig_width, unsigned int orig_height,
		const char *id, unsigned int index)
{
    vobsub_out_t *result = NULL;
    char *filename;
    filename = malloc(strlen(basename) + 5);
    if (filename) {
	result = malloc(sizeof(vobsub_out_t));
	result->fsub = NULL;
	result->fidx = NULL;
	result->aid = 0;
	if (result) {
	    result->aid = index;
	    strcpy(filename, basename);
	    strcat(filename, ".sub");
	    result->fsub = fopen(filename, "a");
	    if (result->fsub == NULL)
		perror("Error: vobsub_out_open subtitle file open failed");
	    strcpy(filename, basename);
	    strcat(filename, ".idx");
	    result->fidx = fopen(filename, "a");
	    if (result->fidx) {
		if (ftell(result->fidx) == 0)
		    create_idx(result, palette, orig_width, orig_height);
		fprintf(result->fidx, "\nid: %s, index: %u\n", id ? id : "xx", index);
		/* So that we can check the file now */
		fflush(result->fidx);
	    }
	    else
		perror("Error: vobsub_out_open index file open failed");
	    free(filename);
	}
    }
    return result;
}

void
vobsub_out_close(void *me)
{
    vobsub_out_t *vob = (vobsub_out_t*)me;
    if (vob->fidx)
	fclose(vob->fidx);
    if (vob->fsub)
	fclose(vob->fsub);
    free(vob);
}

void
vobsub_out_output(void *me, const unsigned char *packet, int len, double pts)
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
	    if (ms >= 1000)	/* prevent overfolws or bad float stuff */
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
	*p++ = 0;		/* 0x00 */
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

	    datalen += 3;	/* Version, PTS_flags, pts_len */
	    datalen += pts_len;
	    datalen += 1;	/* AID */
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
	    }
	    else if (pad_len > 6)
		pad_len = 0;
	    datalen += pad_len;

	    *p++ = 0;		/* 0x0e */
	    *p++ = 0;
	    *p++ = 1;
	    *p++ = 0xbd;

	    *p++ = (datalen >> 8) & 0xff; /* length of payload */
	    *p++ = datalen & 0xff;
	    *p++ = 0x80;		/* System-2 (.VOB) stream */
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
	}
	else if (remain > 0) {
	    /* I don't know what to output.  But anyway the block
	       needs to be 2KB big */
	    memset(buffer, 0, remain);
	    if (fwrite(buffer, remain, 1, vob->fsub) != 1)
		perror("ERROR: vobsub blank padding write failed");
	}
	else if (remain < 0)
	    fprintf(stderr,
		    "\nERROR: wrong thing happenned...\n"
		    "  I wrote a %i data bytes spu packet and that's too long\n", len);
    }
}
