/*
 * Use with CVS JOCR/GOCR.
 *
 * You will have to change 'vobsub_id' value if you want another subtitle than number 0.
 *
 * HINT: you can view the subtitle that is being decoded with "display subtitle-*.pgm"
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Make sure this accesses the CVS version of JOCR/GOCR */
#define GOCR_PROGRAM "gocr"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "libvo/video_out.h"
#include "vobsub.h"
#include "spudec.h"

/* XXX Kludge ahead, this MUST be the same as the definitions found in ../spudec.c */
typedef struct packet_t packet_t;
struct packet_t {
  unsigned char *packet;
  unsigned int palette[4];
  unsigned int alpha[4];
  unsigned int control_start;	/* index of start of control data */
  unsigned int current_nibble[2]; /* next data nibble (4 bits) to be
                                     processed (for RLE decoding) for
                                     even and odd lines */
  int deinterlace_oddness;	/* 0 or 1, index into current_nibble */
  unsigned int start_col, end_col;
  unsigned int start_row, end_row;
  unsigned int width, height, stride;
  unsigned int start_pts, end_pts;
  packet_t *next;
};
typedef struct {
  packet_t *queue_head;
  packet_t *queue_tail;
  unsigned int global_palette[16];
  unsigned int orig_frame_width, orig_frame_height;
  unsigned char* packet;
  size_t packet_reserve;	/* size of the memory pointed to by packet */
  unsigned int packet_offset;	/* end of the currently assembled fragment */
  unsigned int packet_size;	/* size of the packet once all fragments are assembled */
  unsigned int packet_pts;	/* PTS for this packet */
  unsigned int palette[4];
  unsigned int alpha[4];
  unsigned int cuspal[4];
  unsigned int custom;
  unsigned int now_pts;
  unsigned int start_pts, end_pts;
  unsigned int start_col, end_col;
  unsigned int start_row, end_row;
  unsigned int width, height, stride;
  size_t image_size;		/* Size of the image buffer */
  unsigned char *image;		/* Grayscale value */
  unsigned char *aimage;	/* Alpha value */
  unsigned int scaled_frame_width, scaled_frame_height;
  unsigned int scaled_start_col, scaled_start_row;
  unsigned int scaled_width, scaled_height, scaled_stride;
  size_t scaled_image_size;
  unsigned char *scaled_image;
  unsigned char *scaled_aimage;
  int auto_palette; /* 1 if we lack a palette and must use an heuristic. */
  int font_start_level;  /* Darkest value used for the computed font */
  const vo_functions_t *hw_spu;
  int spu_changed;
} spudec_handle_t;

int vobsub_id=0;
int sub_pos=0;

static spudec_handle_t *spudec;
static FILE *fsub = NULL;
static unsigned int sub_idx = 0;

static void
process_gocr_output(const char *const fname, unsigned int start, unsigned int end)
{
    FILE *file;
    int temp, h, m, s, ms;
    int c, bol;
    file = fopen(fname, "r");
    if (file == NULL) {
	perror("fopen failed");
	return;
    }
    temp = start;
    temp /= 90;
    h = temp / 3600000;
    temp %= 3600000;
    m = temp / 60000;
    temp %= 60000;
    s = temp / 1000;
    temp %= 1000;
    ms = temp;
    fprintf(fsub, "%d\n%02d:%02d:%02d,%03d --> ", ++sub_idx, h, m, s, ms);
    temp = end;
    temp /= 90;
    h = temp / 3600000;
    temp %= 3600000;
    m = temp / 60000;
    temp %= 60000;
    s = temp / 1000;
    temp %= 1000;
    ms = temp;
    fprintf(fsub, "%02d:%02d:%02d,%03d\n", h, m, s, ms);
    bol = 1;
    while ((c = getc(file)) != EOF) {
	if (bol) {
	    if (!isspace(c)) {
		putc(c, fsub);
		bol=0;
	    }
	}
	else if (!bol) {
	    putc(c, fsub);
	    bol = c == '\n';
	}
    }
    putc('\n', fsub);
    fflush(fsub);
    fclose(file);
}

static void
output_pgm(FILE *f, int w, int h, unsigned char *src, unsigned char *srca, int stride)
{
    int x, y;
    fprintf(f,
	    "P5\n"
	    "%d %d\n"
	    "255\n",
	    w, h);
    for (y = 0; y < h; ++y) {
	for (x = 0; x < w; ++x) {
	    int res;
	    if (srca[x])
		res = src[x] * (256 - srca[x]);
	    else
		res = 0;
	    res = (65535 - res) >> 8;
	    putc(res&0xff, f);

	}
	src += stride;
	srca += stride;
    }
    putc('\n', f);
}

static void
draw_alpha(int x0, int y0, int w, int h, unsigned char *src, unsigned char *srca, int stride)
{
    FILE *f;
    char buf[128];
    char cmd[512];
    int cmdres;
    const char *const tmpfname = tmpnam(NULL);
    sprintf(buf, "subtitle-%d-%d.pgm", spudec->start_pts / 90, spudec->end_pts / 90);
    f = fopen(buf, "w");
    output_pgm(f, w, h, src, srca, stride);
    fclose(f);
    /* see <URL:http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/subtitleripper/subtitleripper/src/README.gocr?rev=HEAD&content-type=text/vnd.viewcvs-markup> */
    sprintf(cmd, GOCR_PROGRAM" -v 1 -s 7 -d 0 -m 130 -m 256 -m 32 -i %s -o %s", buf, tmpfname);
    cmdres = system(cmd);
    if (cmdres < 0) {
	perror("system failed");
	exit(EXIT_FAILURE);
    }
    else if (cmdres) {
	fprintf(stderr, GOCR_PROGRAM" returned %d\n", cmdres);
	exit(cmdres);
    }
    process_gocr_output(tmpfname, spudec->start_pts, spudec->end_pts);
    unlink(buf);
    unlink(tmpfname);
}

int
main(int argc, char **argv)
{
    const char *vobsubname, *subripname;
    void *vobsub;
    void *packet;
    int packet_len;
    unsigned int pts100;

    if (argc < 2 || 4 < argc) {
	fprintf(stderr, "Usage: %s <vobsub basename> [<subid> [<output filename>] ]\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    vobsubname = argv[1];
    subripname = NULL;
    fsub = stdout;
    if (argc >= 3)
	vobsub_id = atoi(argv[2]);
    if (argc >= 4) {
	subripname = argv[3];
	fsub = fopen(subripname, "w");
    }

    vobsub = vobsub_open(vobsubname, NULL, 0, &spudec);
    while ((packet_len=vobsub_get_next_packet(vobsub, &packet, &pts100)) >= 0) {
	spudec_assemble(spudec, packet, packet_len, pts100);
	if (spudec->queue_head) {
		spudec_heartbeat(spudec, spudec->queue_head->start_pts);
	if (spudec_changed(spudec))
	    spudec_draw(spudec, draw_alpha);
	}
    }

    if (vobsub)
	vobsub_close(vobsub);
    exit(EXIT_SUCCESS);
}
