/*
 * avisubdump
 *
 * avi vobsub subtitle stream dumper (c) 2004 Tobias Diedrich
 * Licensed under GNU GPLv2 or (at your option) any later version.
 *
 * The subtitles are dumped to stdout.
 */

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define FCC(a,b,c,d)  (((a))|((b)<<8)|((c)<<16)|((d)<<24))

#define FCC_RIFF FCC('R','I','F','F')
#define FCC_LIST FCC('L','I','S','T')
#define FCC_strh FCC('s','t','r','h')
#define FCC_txts FCC('t','x','t','s')
#define FCC_GAB2 FCC('G','A','B','2')

#define GAB_LANGUAGE            0
#define GAB_ENTRY               1
#define GAB_LANGUAGE_UNICODE    2
#define GAB_ENTRY_UNICODE       3
#define GAB_RAWTEXTSUBTITLE     4

static unsigned int getle16(FILE* f){
	unsigned int res;

	res  = fgetc(f);
	res |= fgetc(f) << 8;

	return res;
}

static unsigned int getle(FILE* f){
	unsigned int res;

	res  = fgetc(f);
	res |= fgetc(f) << 8;
	res |= fgetc(f) << 16;
	res |= fgetc(f) << 24;

	return res;
}

static void skip(FILE *f, int len)
{
	if (f != stdin) {
		fseek(f,len,SEEK_CUR);
	} else {
		void *buf = malloc(len);
		fread(buf,len,1,f);
		free(buf);
	}
}

static int stream_id(unsigned int id)
{
	char c1,c2;
	c1 = (char)(id & 0xff);
	c2 = (char)((id >> 8) & 0xff);
	if (c1 >= '0' && c1 <= '9' &&
	    c2 >= '0' && c2 <= '9') {
		c1 -= '0';
		c2 -= '0';
		return c1*10+c2;
	}
	return -1;
}

static int dumpsub_gab2(FILE *f, int size) {
	int ret = 0;

	while (ret + 6 <= size) {
		unsigned int len, id;
		char *buf;
		int i;

		id = getle16(f); ret += 2;
		len = getle(f); ret += 4;
		if (ret + len > size) break;

		buf = malloc(len);
		ret += fread(buf, 1, len, f);

		switch (id) {
		case GAB_LANGUAGE_UNICODE: /* FIXME: convert to utf-8; endianness */
			for (i=0; i<len; i++) buf[i] = buf[i*2];
		case GAB_LANGUAGE:
			fprintf(stderr, "LANGUAGE: %s\n", buf);
			break;
		case GAB_ENTRY_UNICODE: /* FIXME: convert to utf-8; endianness */
			for (i=0; i<len; i++) buf[i] = buf[i*2];
		case GAB_ENTRY:
			fprintf(stderr, "ENTRY: %s\n", buf);
			break;
		case GAB_RAWTEXTSUBTITLE:
			printf("%s", buf);
			break;
		default:
			fprintf(stderr, "Unknown type %d, len %d\n", id, len);
			break;
		}
		free(buf);
	}

	return ret;
}

static void dump(FILE *f) {
	unsigned int id, len;
	int stream = 0;
	int substream = -2;

	while (1) {
		id = getle(f);
		len = getle(f);

		if(feof(f)) break;

		if (id == FCC_RIFF ||
		    id == FCC_LIST) {
			getle(f);
			continue;
		} else if (id == FCC_strh) {
			id = getle(f); len -= 4;
			fprintf(stderr, "Stream %d is %c%c%c%c",
			       stream,
			       id,
			       id >> 8,
			       id >> 16,
			       id >> 24);
			if (id == FCC_txts) {
				substream = stream;
				fprintf(stderr, " (subtitle stream)");
			}
			fprintf(stderr, ".\n");
			stream++;
		} else if (stream_id(id) == substream) {
			unsigned int subid;
			subid = getle(f); len -= 4;
			if (subid != FCC_GAB2) {
				fprintf(stderr,
				        "Unknown subtitle chunk %c%c%c%c (%08x).\n",
				        id, id >> 8, id >> 16, id >> 24, subid);
			} else {
				skip(f,1); len -= 1;
				len -= dumpsub_gab2(f, len);
			}
		}
		len+=len&1;
		skip(f,len);
	}
}

int main(int argc,char* argv[])
{
	FILE* f;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <avi>\n", argv[0]);
		exit(1);
	}

	if (strcmp(argv[argc-1], "-") == 0) f=stdin;
	else f=fopen(argv[argc-1],"rb");

	if (!f) {
		fprintf(stderr, "Could not open '%s': %s\n",
		        argv[argc-1], strerror(errno));
		exit(-errno);
	}

	dump(f);

	return 0;
}

