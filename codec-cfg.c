/*
 * codec.conf parser
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 */

#define DEBUG

#ifdef DEBUG
#define DBG(str, args...) printf(str, ##args)
#else
#define DBG(str, args...) do {} while (0)
#endif

#define PRINT_LINENUM printf("%s(%d): ", cfgfile, line_num)

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "libvo/video_out.h"
#include "codec-cfg.h"

#define MAX_NR_TOKEN	16

#define MAX_LINE_LEN	1000

#define STATE_MASK	((1<<7)-1)

#define GOT_NAME	(1<<0)
#define GOT_INFO	(1<<1)
#define GOT_FOURCC	(1<<2)
#define GOT_FORMAT	(1<<3)
#define GOT_DRIVER	(1<<4)
#define GOT_DLL		(1<<5)
#define GOT_OUT		(1<<6)

#define RET_EOF		-1
#define RET_EOL		-2

static FILE *fp;
static int line_num = 0;
static int line_pos;	/* line pos */
static char *line;
static char *token[MAX_NR_TOKEN];

static codecs_t *codecs=NULL;
static int nr_codecs = 0;

static int get_token(int min, int max)
{
	static int read_nextline = 1;
	int i;
	char c;

	if (max >= MAX_NR_TOKEN) {
		printf("\nget_token(): max >= MAX_NR_TOKEN!\n");
		goto ret_eof;
	}

	memset(token, 0x00, sizeof(*token) * max);

	if (read_nextline) {
		if (!fgets(line, MAX_LINE_LEN, fp))
			goto ret_eof;
		line_pos = 0;
		++line_num;
		read_nextline = 0;
	}
	for (i = 0; i < max; i++) {
		while (isspace(line[line_pos]))
			++line_pos;
		if (line[line_pos] == '\0' || line[line_pos] == '#' ||
				line[line_pos] == ';') {
			read_nextline = 1;
			if (i >= min)
				goto ret_ok;
			goto ret_eol;
		}
		token[i] = line + line_pos;
		c = line[line_pos];
		if (c == '"' || c == '\'') {
			token[i]++;
			while (line[++line_pos] != c && line[line_pos])
				/* NOTHING */;
		} else {
			for (/* NOTHING */; !isspace(line[line_pos]) &&
					line[line_pos]; line_pos++)
				/* NOTHING */;
		}
		if (!line[line_pos]) {
			read_nextline = 1;
			if (i >= min - 1)
				goto ret_ok;
			goto ret_eol;
		}
		line[line_pos] = '\0';
		line_pos++;
	}
ret_ok:
	return i;
ret_eof:
	return RET_EOF;
ret_eol:
	return RET_EOL;
}

static int add_to_fourcc(char *s, char *alias, unsigned int *fourcc,
		unsigned int *map)
{
	int i, j, freeslots;
	char **aliasp;
	unsigned int tmp;

	/* find first unused slot */
	for (i = 0; i < CODECS_MAX_FOURCC && fourcc[i] != 0xffffffff; i++)
		/* NOTHING */;
	freeslots = CODECS_MAX_FOURCC - i;
	if (!freeslots)
		goto too_many_error_out;

	aliasp = (alias) ? &alias : &s;
	do {
		tmp = *((unsigned int *) s);
		for (j = 0; j < i; j++)
			if (tmp == fourcc[j])
				goto duplicated_error_out;
		fourcc[i] = tmp;
		map[i] = *((unsigned int *) (*aliasp));
		s += 4;
		i++;
	} while ((*(s++) == ',') && --freeslots);

	if (!freeslots)
		goto too_many_error_out;
	if (*(--s) != '\0')
		return 0;
	return 1;
duplicated_error_out:
	printf("\nduplicated fourcc/format\n");
	return 0;
too_many_error_out:
	printf("\ntoo many fourcc/format...\n");
	return 0;
}

static int add_to_format(char *s, unsigned int *fourcc, unsigned int *fourccmap)
{
	//printf("\n-----[%s][%s]-----\n",s,format);

	int i, j;

	/* find first unused slot */
	for (i = 0; i < CODECS_MAX_FOURCC && fourcc[i] != 0xffffffff; i++)
		/* NOTHING */;
	if (i == CODECS_MAX_FOURCC) {
		printf("\ntoo many fourcc/format...\n");
		return 0;
	}

        fourcc[i]=fourccmap[i]=strtoul(s,NULL,0);
	for (j = 0; j < i; j++)
		if (fourcc[j] == fourcc[i]) {
			printf("\nduplicated fourcc/format\n");
			return 0;
		}

	return 1;
}


static int add_to_out(char *sfmt, char *sflags, unsigned int *outfmt,
		unsigned char *outflags)
{
	static char *fmtstr[] = {
		"YUY2",
		"YV12",
		"RGB8",
		"RGB15",
		"RGB16",
		"RGB24",
		"RGB32",
		"BGR8",
		"BGR15",
		"BGR16",
		"BGR24",
		"BGR32",
		NULL
	};
	static unsigned int fmtnum[] = {
		IMGFMT_YUY2,
		IMGFMT_YV12,
		IMGFMT_RGB|8,
		IMGFMT_RGB|15,
		IMGFMT_RGB|16,
		IMGFMT_RGB|24,
		IMGFMT_RGB|32,
		IMGFMT_BGR|8,
		IMGFMT_BGR|15,
		IMGFMT_BGR|16,
		IMGFMT_BGR|24,
		IMGFMT_BGR|32
	};
	static char *flagstr[] = {
		"flip",
		"noflip",
		"yuvhack",
		NULL
	};

	int i, j, freeslots;
	unsigned char flags;

	for (i = 0; i < CODECS_MAX_OUTFMT && outfmt[i] != 0xffffffff; i++)
		/* NOTHING */;
	freeslots = CODECS_MAX_OUTFMT - i;
	if (!freeslots)
		goto too_many_error_out;

	flags = 0;
	if(sflags) do {
		for (j = 0; flagstr[j] != NULL; j++)
			if (!strncmp(sflags, flagstr[j], strlen(flagstr[j])))
				break;
		if (flagstr[j] == NULL) return 0; // error!
		flags|=(1<<j);
		sflags+=strlen(flagstr[j]);
	} while (*(sflags++) == ',');

	do {
		for (j = 0; fmtstr[j] != NULL; j++)
			if (!strncmp(sfmt, fmtstr[j], strlen(fmtstr[j])))
				break;
		if (fmtstr[j] == NULL)
			return 0;
		outfmt[i] = fmtnum[j];
		outflags[i] = flags;
                ++i;
		sfmt+=strlen(fmtstr[j]);
	} while ((*(sfmt++) == ',') && --freeslots);

	if (!freeslots)
		goto too_many_error_out;

	if (*(--sfmt) != '\0') return 0;
        
	return 1;
too_many_error_out:
	printf("\ntoo many out...\n");
	return 0;
}

static short get_driver(char *s,int audioflag)
{
	static char *audiodrv[] = {
		"mp3lib",
		"pcm",
		"libac3",
		"acm",
		"alaw",
		"msgsm",
		"dshow",
		NULL
	};
	static char *videodrv[] = {
		"libmpeg2",
		"vfw",
		"odivx",
		"dshow",
		NULL
	};
        char **drv=audioflag?audiodrv:videodrv;
        int i;
        
        for(i=0;drv[i];i++) if(!strcmp(s,drv[i])) return i+1;

	return 0;
}

static int valid_codec(codecs_t *codec)
{
#warning FIXME mi is kell egy codec-be?
	return 1;
}

static int add_comment(char *s, char **d)
{
	int pos;

	if (!*d)
		pos = 0;
	else {
		pos = strlen(*d);
		(*d)[pos++] = '\n';
	}
	if (!(*d = (char *) realloc(*d, pos + strlen(s) + 1))) {
		printf("can't allocate mem for comment\n");
		return 0;
	}
	strcpy(*d + pos, s);
	return 1;
}

codecs_t *parse_codec_cfg(char *cfgfile)
{
	codecs_t *codec = NULL;   // current codec
	int tmp, i;
	int state = 0;

#ifdef DEBUG
	assert(cfgfile != NULL);
#endif

	printf("Reading codec config file: %s\n", cfgfile);

	if ((fp = fopen(cfgfile, "r")) == NULL) {
		printf("parse_codec_cfg: can't open '%s': %s\n", cfgfile, strerror(errno));
		return NULL;
	}

	if ((line = (char *) malloc(MAX_LINE_LEN + 1)) == NULL) {
		perror("parse_codec_cfg: can't get memory for 'line'");
		return NULL;
	}

	while ((tmp = get_token(1, 1)) != RET_EOF) {
		if (tmp == RET_EOL)
			continue;
		if (!strcmp(token[0], "audiocodec") || !strcmp(token[0], "videocodec")) {
			if (nr_codecs)
				if (!valid_codec(codec))
					goto not_valid_error_out;
		        if (!(codecs = (codecs_t *) realloc(codecs,
				sizeof(codecs_t) * (nr_codecs + 1)))) {
			    perror("parse_codec_cfg: can't realloc 'codecs'");
			    goto err_out;
		        }
			codec=&codecs[nr_codecs];
			nr_codecs++;
                        memset(codec,0,sizeof(codecs_t));
			memset(codec->fourcc, 0xff, sizeof(codec->fourcc));
			memset(codec->outfmt, 0xff, sizeof(codec->outfmt));
			state = 0;
                        
			if (*token[0] == 'a') {		/* audiocodec */
				codec->flags |= CODECS_FLAG_AUDIO;
			} else if (*token[0] == 'v') {	/* videocodec */
				codec->flags &= !CODECS_FLAG_AUDIO;
			} else {
				printf("itt valami nagyon el van baszva\n");
				goto err_out;
			}
			if (get_token(1, 1) < 0)
				goto parse_error_out;
			for (i = 0; i < nr_codecs - 1; i++) {
#warning audio meg videocodecnek lehet ugyanaz a neve?
				if ((codec->flags & CODECS_FLAG_AUDIO) !=
						(codecs[i].flags & CODECS_FLAG_AUDIO))
					continue;
				if (!strcmp(token[0], codecs[i].name)) {
					PRINT_LINENUM;
					printf("codec name '%s' isn't unique\n", token[0]);
					goto err_out;
				}
			}
			codec->name = strdup(token[0]);
			state |= GOT_NAME;
		} else if (!strcmp(token[0], "info")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (state & GOT_INFO || get_token(1, 1) < 0)
				goto parse_error_out;
			codec->info = strdup(token[0]);
			state |= GOT_INFO;
		} else if (!strcmp(token[0], "comment")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (get_token(1, 1) < 0)
				goto parse_error_out;
			if (!add_comment(token[0], &codec->comment)) {
				PRINT_LINENUM;
				printf("add_comment()-tel valami sux\n");
			}
		} else if (!strcmp(token[0], "fourcc")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (get_token(1, 2) < 0)
				goto parse_error_out;
			if (!add_to_fourcc(token[0], token[1],
						codec->fourcc,
						codec->fourccmap))
				goto parse_error_out;
			state |= GOT_FOURCC;
		} else if (!strcmp(token[0], "format")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (get_token(1, 1) < 0)
				goto parse_error_out;
			if (!add_to_format(token[0], codec->fourcc,codec->fourccmap))
				goto parse_error_out;
			state |= GOT_FORMAT;
		} else if (!strcmp(token[0], "driver")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (get_token(1, 1) < 0)
				goto parse_error_out;
			if ((codec->driver = get_driver(token[0],codec->flags&CODECS_FLAG_AUDIO)) == -1)
				goto err_out;
		} else if (!strcmp(token[0], "dll")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (get_token(1, 1) < 0)
				goto parse_error_out;
			codec->dll = strdup(token[0]);
		} else if (!strcmp(token[0], "guid")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (get_token(11, 11) < 0) goto parse_error_out;
                        codec->guid.f1=strtoul(token[0],NULL,0);
                        codec->guid.f2=strtoul(token[1],NULL,0);
                        codec->guid.f3=strtoul(token[2],NULL,0);
			for (i = 0; i < 8; i++) {
                            codec->guid.f4[i]=strtoul(token[i + 3],NULL,0);
			}
		} else if (!strcmp(token[0], "out")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (get_token(1, 2) < 0)
				goto parse_error_out;
			if (!add_to_out(token[0], token[1], codec->outfmt,
						codec->outflags))
				goto err_out;
		} else if (!strcmp(token[0], "flags")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (get_token(1, 1) < 0)
				goto parse_error_out;
			if (!strcmp(token[0], "seekable"))
				codec->flags |= CODECS_FLAG_SEEKABLE;
			else
				goto parse_error_out;
		} else if (!strcmp(token[0], "status")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (get_token(1, 1) < 0)
				goto parse_error_out;
			if (!strcasecmp(token[0], ":-)"))
				codec->status = CODECS_STATUS_WORKING;
			else if (!strcasecmp(token[0], ":-("))
				codec->status = CODECS_STATUS_NOT_WORKING;
			else if (!strcasecmp(token[0], "X-("))
				codec->status = CODECS_STATUS_UNTESTED;
			else if (!strcasecmp(token[0], ":-|"))
				codec->status = CODECS_STATUS_PROBLEMS;
			else
				goto parse_error_out;
		} else
			goto parse_error_out;
	}
	if (!valid_codec(codec))
		goto not_valid_error_out;
out:
	free(line);
	fclose(fp);
	return codecs;
parse_error_out:
	PRINT_LINENUM;
	printf("parse error\n");
err_out:
	printf("\nOops\n");
	if (codecs)
		free(codecs);
	codecs = NULL;
	goto out;
not_valid_error_out:
	PRINT_LINENUM;
	printf("codec is not definied correctly\n");
	goto err_out;
}

codecs_t* find_codec(unsigned int fourcc,unsigned int *fourccmap,int audioflag){
  int i,j;
  for(i=0;i<nr_codecs;i++){
    codecs_t *c=&codecs[i];
    if(!audioflag && (c->flags&CODECS_FLAG_AUDIO)) continue;
    if(audioflag && !(c->flags&CODECS_FLAG_AUDIO)) continue;
    for(j=0;j<CODECS_MAX_FOURCC;j++){
      if(c->fourcc[j]==fourcc){
        if(fourccmap) *fourccmap=c->fourccmap[j];
        return c;
      }
    }
  }
  return NULL;
}


#ifdef TESTING
int main(void)
{
	codecs_t *codecs;
        int i,j;

	if (!(codecs = parse_codec_cfg("DOCS/codecs.conf")))
		return 0;
        
        printf("total %d codecs parsed\n",nr_codecs);
        for(i=0;i<nr_codecs;i++){
            codecs_t *c=&codecs[i];
            printf("\n============== codec %02d ===============\n",i);
            printf("name='%s'\n",c->name);
            printf("info='%s'\n",c->info);
            printf("comment='%s'\n",c->comment);
            printf("dll='%s'\n",c->dll);
            printf("flags=%X  driver=%d\n",c->flags,c->driver);

            for(j=0;j<CODECS_MAX_FOURCC;j++){
              if(c->fourcc[j]!=0xFFFFFFFF){
                  printf("fourcc %02d:  %08X (%.4s) ===> %08X (%.4s)\n",j,c->fourcc[j],&c->fourcc[j],c->fourccmap[j],&c->fourccmap[j]);
              }
            }

            for(j=0;j<CODECS_MAX_OUTFMT;j++){
              if(c->outfmt[j]!=0xFFFFFFFF){
                  printf("outfmt %02d:  %08X (%.4s)  flags: %d\n",j,c->outfmt[j],&c->outfmt[j],c->outflags[j]);
              }
            }

            printf("GUID: %08lX %04X %04X",c->guid.f1,c->guid.f2,c->guid.f3);
            for(j=0;j<8;j++) printf(" %02X",c->guid.f4[j]);
            printf("\n");

            
        }
        
	return 0;
}

#endif
