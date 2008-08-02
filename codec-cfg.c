/*
 * codec.conf parser
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 *
 * to compile test application:
 *  cc -I. -DTESTING -o codec-cfg-test codec-cfg.c mp_msg.o osdep/getch2.o -ltermcap
 * to compile CODECS2HTML:
 *   gcc -DCODECS2HTML -o codecs2html codec-cfg.c mp_msg.o
 *
 * TODO: implement informat in CODECS2HTML too
 */

#define DEBUG

//disable asserts
#define NDEBUG 

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#ifdef CODECS2HTML
#ifdef __GNUC__
#define mp_msg(t, l, m, args...) fprintf(stderr, m, ##args)
#else
#define mp_msg(t, l, ...) fprintf(stderr, __VA_ARGS__)
#endif
#endif

#include "help_mp.h"

// for mmioFOURCC:
#include "libmpdemux/aviheader.h"

#include "libmpcodecs/img_format.h"
#include "codec-cfg.h"

#ifndef CODECS2HTML
#include "codecs.conf.h"
#endif

#define PRINT_LINENUM mp_msg(MSGT_CODECCFG,MSGL_ERR," at line %d\n", line_num)

#define MAX_NR_TOKEN	16

#define MAX_LINE_LEN	1000

#define RET_EOF		-1
#define RET_EOL		-2

#define TYPE_VIDEO	0
#define TYPE_AUDIO	1

char * codecs_file = NULL;

static int add_to_fourcc(char *s, char *alias, unsigned int *fourcc,
		unsigned int *map)
{
	int i, j, freeslots;
	unsigned int tmp;

	/* find first unused slot */
	for (i = 0; i < CODECS_MAX_FOURCC && fourcc[i] != 0xffffffff; i++)
		/* NOTHING */;
	freeslots = CODECS_MAX_FOURCC - i;
	if (!freeslots)
		goto err_out_too_many;

	do {
		tmp = mmioFOURCC(s[0], s[1], s[2], s[3]);
		for (j = 0; j < i; j++)
			if (tmp == fourcc[j])
				goto err_out_duplicated;
		fourcc[i] = tmp;
		map[i] = alias ? mmioFOURCC(alias[0], alias[1], alias[2], alias[3]) : tmp;
		s += 4;
		i++;
	} while ((*(s++) == ',') && --freeslots);

	if (!freeslots)
		goto err_out_too_many;
	if (*(--s) != '\0')
		goto err_out_parse_error;
	return 1;
err_out_duplicated:
	mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_DuplicateFourcc);
	return 0;
err_out_too_many:
	mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_TooManyFourccs);
	return 0;
err_out_parse_error:
	mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_ParseError);
	return 0;
}

static int add_to_format(char *s, char *alias,unsigned int *fourcc, unsigned int *fourccmap)
{
	int i, j;
	char *endptr;

	/* find first unused slot */
	for (i = 0; i < CODECS_MAX_FOURCC && fourcc[i] != 0xffffffff; i++)
		/* NOTHING */;
	if (i == CODECS_MAX_FOURCC) {
		mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_TooManyFourccs);
		return 0;
	}

        fourcc[i]=strtoul(s,&endptr,0);
	if (*endptr != '\0') {
		mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_ParseErrorFIDNotNumber);
		return 0;
	}

	if(alias){
	    fourccmap[i]=strtoul(alias,&endptr,0);
	    if (*endptr != '\0') {
		mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_ParseErrorFIDAliasNotNumber);
		return 0;
	    }
	} else
	    fourccmap[i]=fourcc[i];

	for (j = 0; j < i; j++)
		if (fourcc[j] == fourcc[i]) {
			mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_DuplicateFID);
			return 0;
		}

	return 1;
}

        static struct {
	        const char *name;
	        const unsigned int num;
	} fmt_table[] = {
		{"YV12",  IMGFMT_YV12},
		{"I420",  IMGFMT_I420},
		{"IYUV",  IMGFMT_IYUV},
		{"NV12",  IMGFMT_NV12},
		{"NV21",  IMGFMT_NV21},
		{"YVU9",  IMGFMT_YVU9},
		{"IF09",  IMGFMT_IF09},
		{"444P",  IMGFMT_444P},
		{"422P",  IMGFMT_422P},
		{"411P",  IMGFMT_411P},
		{"Y800",  IMGFMT_Y800},
		{"Y8",    IMGFMT_Y8},

		{"YUY2",  IMGFMT_YUY2},
		{"UYVY",  IMGFMT_UYVY},
		{"YVYU",  IMGFMT_YVYU},

	        {"RGB4",  IMGFMT_RGB|4},
	        {"RGB8",  IMGFMT_RGB|8},
		{"RGB15", IMGFMT_RGB|15}, 
		{"RGB16", IMGFMT_RGB|16},
		{"RGB24", IMGFMT_RGB|24},
		{"RGB32", IMGFMT_RGB|32},
		{"BGR4",  IMGFMT_BGR|4},
		{"BGR8",  IMGFMT_BGR|8},
		{"BGR15", IMGFMT_BGR|15},
		{"BGR16", IMGFMT_BGR|16},
		{"BGR24", IMGFMT_BGR|24},
		{"BGR32", IMGFMT_BGR|32},
	        {"RGB1",  IMGFMT_RGB|1},
		{"BGR1",  IMGFMT_BGR|1},

		{"MPES",  IMGFMT_MPEGPES},
		{"ZRMJPEGNI", IMGFMT_ZRMJPEGNI},
		{"ZRMJPEGIT", IMGFMT_ZRMJPEGIT},
		{"ZRMJPEGIB", IMGFMT_ZRMJPEGIB},

		{"IDCT_MPEG2",IMGFMT_XVMC_IDCT_MPEG2},
		{"MOCO_MPEG2",IMGFMT_XVMC_MOCO_MPEG2},

		{NULL,    0}
	};


static int add_to_inout(char *sfmt, char *sflags, unsigned int *outfmt,
		unsigned char *outflags)
{

	static char *flagstr[] = {
		"flip",
		"noflip",
		"yuvhack",
		"query",
		"static",
		NULL
	};

	int i, j, freeslots;
	unsigned char flags;

	for (i = 0; i < CODECS_MAX_OUTFMT && outfmt[i] != 0xffffffff; i++)
		/* NOTHING */;
	freeslots = CODECS_MAX_OUTFMT - i;
	if (!freeslots)
		goto err_out_too_many;

	flags = 0;
	if(sflags) {
		do {
			for (j = 0; flagstr[j] != NULL; j++)
				if (!strncmp(sflags, flagstr[j],
							strlen(flagstr[j])))
					break;
			if (flagstr[j] == NULL)
				goto err_out_parse_error;
			flags|=(1<<j);
			sflags+=strlen(flagstr[j]);
		} while (*(sflags++) == ',');

		if (*(--sflags) != '\0')
			goto err_out_parse_error;
	}

	do {
		for (j = 0; fmt_table[j].name != NULL; j++)
			if (!strncmp(sfmt, fmt_table[j].name, strlen(fmt_table[j].name)))
				break;
		if (fmt_table[j].name == NULL)
			goto err_out_parse_error;
		outfmt[i] = fmt_table[j].num;
		outflags[i] = flags;
                ++i;
		sfmt+=strlen(fmt_table[j].name);
	} while ((*(sfmt++) == ',') && --freeslots);

	if (!freeslots)
		goto err_out_too_many;

	if (*(--sfmt) != '\0')
		goto err_out_parse_error;
        
	return 1;
err_out_too_many:
	mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_TooManyOut);
	return 0;
err_out_parse_error:
	mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_ParseError);
	return 0;
}

#if 0
static short get_driver(char *s,int audioflag)
{
	static char *audiodrv[] = {
		"null",
		"mp3lib",
		"pcm",
		"libac3",
		"acm",
		"alaw",
		"msgsm",
		"dshow",
		"dvdpcm",
		"hwac3",
		"libvorbis",
		"ffmpeg",
		"libmad",
		"msadpcm",
		"liba52",
		"g72x",
		"imaadpcm",
		"dk4adpcm",
		"dk3adpcm",
		"roqaudio",
		"faad",
		"realaud",
		"libdv",
		NULL
	};
	static char *videodrv[] = {
		"null",
		"libmpeg2",
		"vfw",
		"dshow",
		"ffmpeg",
		"vfwex",
		"raw",
		"msrle",
		"xanim",
		"msvidc",
		"fli",
		"cinepak",
		"qtrle",
		"nuv",
		"cyuv",
		"qtsmc",
		"ducktm1",
		"roqvideo",
		"qtrpza",
		"mpng",
		"ijpg",
		"zlib",
		"mpegpes",
		"zrmjpeg",
		"realvid",
		"xvid",
		"libdv",
		NULL
	};
        char **drv=audioflag?audiodrv:videodrv;
        int i;

        for(i=0;drv[i];i++) if(!strcmp(s,drv[i])) return i;

	return -1;
}
#endif

static int validate_codec(codecs_t *c, int type)
{
	unsigned int i;
	char *tmp_name = c->name;

	for (i = 0; i < strlen(tmp_name) && isalnum(tmp_name[i]); i++)
		/* NOTHING */;

	if (i < strlen(tmp_name)) {
		mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_InvalidCodecName, c->name);
		return 0;
	}

	if (!c->info)
		c->info = strdup(c->name);

#if 0
	if (c->fourcc[0] == 0xffffffff) {
		mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_CodecLacksFourcc, c->name);
		return 0;
	}
#endif

	if (!c->drv) {
		mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_CodecLacksDriver, c->name);
		return 0;
	}

#if 0
#warning codec->driver == 4;... <- this should not be put in here...
#warning Where are they defined ????????????
	if (!c->dll && (c->driver == 4 ||
				(c->driver == 2 && type == TYPE_VIDEO))) {
		mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_CodecNeedsDLL, c->name);
		return 0;
	}
#warning Can guid.f1 be 0? How does one know that it was not given?
//	if (!(codec->flags & CODECS_FLAG_AUDIO) && codec->driver == 4)

	if (type == TYPE_VIDEO)
		if (c->outfmt[0] == 0xffffffff) {
			mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_CodecNeedsOutfmt, c->name);
			return 0;
		}
#endif
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
	if (!(*d = realloc(*d, pos + strlen(s) + 1))) {
		mp_msg(MSGT_CODECCFG,MSGL_FATAL,MSGTR_CantAllocateComment);
		return 0;
	}
	strcpy(*d + pos, s);
	return 1;
}

static short get_cpuflags(char *s)
{
	static char *flagstr[] = {
		"mmx",
		"sse",
		"3dnow",
		NULL
	};
        int i;
	short flags = 0;

	do {
		for (i = 0; flagstr[i]; i++)
			if (!strncmp(s, flagstr[i], strlen(flagstr[i])))
				break;
		if (!flagstr[i])
			goto err_out_parse_error;
		flags |= 1<<i;
		s += strlen(flagstr[i]);
	} while (*(s++) == ',');

	if (*(--s) != '\0')
		goto err_out_parse_error;

	return flags;
err_out_parse_error:
	return 0;
}

static FILE *fp;
static int line_num = 0;
static char *line;
static char *token[MAX_NR_TOKEN];
static int read_nextline = 1;

static int get_token(int min, int max)
{
	static int line_pos;
	int i;
	char c;

	if (max >= MAX_NR_TOKEN) {
		mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN);
		goto out_eof;
	}

	memset(token, 0x00, sizeof(*token) * max);

	if (read_nextline) {
		if (!fgets(line, MAX_LINE_LEN, fp))
			goto out_eof;
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
				goto out_ok;
			goto out_eol;
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
				goto out_ok;
			goto out_eol;
		}
		line[line_pos] = '\0';
		line_pos++;
	}
out_ok:
	return i;
out_eof:
	read_nextline = 1;
	return RET_EOF;
out_eol:
	return RET_EOL;
}

static codecs_t *video_codecs=NULL;
static codecs_t *audio_codecs=NULL;
static int nr_vcodecs = 0;
static int nr_acodecs = 0;

int parse_codec_cfg(const char *cfgfile)
{
	codecs_t *codec = NULL; // current codec
	codecs_t **codecsp = NULL;// points to audio_codecs or to video_codecs
	char *endptr;	// strtoul()...
	int *nr_codecsp;
	int codec_type;		/* TYPE_VIDEO/TYPE_AUDIO */
	int tmp, i;
	
	// in case we call it a second time
	codecs_uninit_free();
	
	nr_vcodecs = 0;
	nr_acodecs = 0;

	if(cfgfile==NULL) {
#ifdef CODECS2HTML
	  	return 0; 
#else
		video_codecs = builtin_video_codecs;
		audio_codecs = builtin_audio_codecs;
		nr_vcodecs = sizeof(builtin_video_codecs)/sizeof(codecs_t);
		nr_acodecs = sizeof(builtin_audio_codecs)/sizeof(codecs_t);
		return 1;
#endif
	}
	
	mp_msg(MSGT_CODECCFG,MSGL_V,MSGTR_ReadingFile, cfgfile);

	if ((fp = fopen(cfgfile, "r")) == NULL) {
		mp_msg(MSGT_CODECCFG,MSGL_V,MSGTR_CantOpenFileError, cfgfile, strerror(errno));
		return 0;
	}

	if ((line = malloc(MAX_LINE_LEN + 1)) == NULL) {
		mp_msg(MSGT_CODECCFG,MSGL_FATAL,MSGTR_CantGetMemoryForLine, strerror(errno));
		return 0;
	}
	read_nextline = 1;

	/*
	 * this only catches release lines at the start of 
	 * codecs.conf, before audiocodecs and videocodecs.
	 */
	while ((tmp = get_token(1, 1)) == RET_EOL)
		/* NOTHING */;
	if (tmp == RET_EOF)
		goto out;
	if (!strcmp(token[0], "release")) {
		if (get_token(1, 2) < 0)
			goto err_out_parse_error;
		tmp = atoi(token[0]);
		if (tmp < CODEC_CFG_MIN)
			goto err_out_release_num;
		while ((tmp = get_token(1, 1)) == RET_EOL)
			/* NOTHING */;
		if (tmp == RET_EOF)
			goto out;
	} else
		goto err_out_release_num;

	/*
	 * check if the next block starts with 'audiocodec' or
	 * with 'videocodec'
	 */
	if (!strcmp(token[0], "audiocodec") || !strcmp(token[0], "videocodec"))
		goto loop_enter;
	goto err_out_parse_error;

	while ((tmp = get_token(1, 1)) != RET_EOF) {
		if (tmp == RET_EOL)
			continue;
		if (!strcmp(token[0], "audiocodec") ||
				!strcmp(token[0], "videocodec")) {
			if (!validate_codec(codec, codec_type))
				goto err_out_not_valid;
		loop_enter:
			if (*token[0] == 'v') {
				codec_type = TYPE_VIDEO;
				nr_codecsp = &nr_vcodecs;
				codecsp = &video_codecs;
			} else if (*token[0] == 'a') {
				codec_type = TYPE_AUDIO;
				nr_codecsp = &nr_acodecs;
				codecsp = &audio_codecs;
#ifdef DEBUG
			} else {
				mp_msg(MSGT_CODECCFG,MSGL_ERR,"picsba\n");
				goto err_out;
#endif
			}
		        if (!(*codecsp = realloc(*codecsp,
				sizeof(codecs_t) * (*nr_codecsp + 2)))) {
			    mp_msg(MSGT_CODECCFG,MSGL_FATAL,MSGTR_CantReallocCodecsp, strerror(errno));
			    goto err_out;
		        }
			codec=*codecsp + *nr_codecsp;
			++*nr_codecsp;
                        memset(codec,0,sizeof(codecs_t));
			memset(codec->fourcc, 0xff, sizeof(codec->fourcc));
			memset(codec->outfmt, 0xff, sizeof(codec->outfmt));
			memset(codec->infmt, 0xff, sizeof(codec->infmt));
                        
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			for (i = 0; i < *nr_codecsp - 1; i++) {
				if(( (*codecsp)[i].name!=NULL) && 
				    (!strcmp(token[0], (*codecsp)[i].name)) ) {
					mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_CodecNameNotUnique, token[0]);
					goto err_out_print_linenum;
				}
			}
			if (!(codec->name = strdup(token[0]))) {
				mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_CantStrdupName, strerror(errno));
				goto err_out;
			}
		} else if (!strcmp(token[0], "info")) {
			if (codec->info || get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!(codec->info = strdup(token[0]))) {
				mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_CantStrdupInfo, strerror(errno));
				goto err_out;
			}
		} else if (!strcmp(token[0], "comment")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			add_comment(token[0], &codec->comment);
		} else if (!strcmp(token[0], "fourcc")) {
			if (get_token(1, 2) < 0)
				goto err_out_parse_error;
			if (!add_to_fourcc(token[0], token[1],
						codec->fourcc,
						codec->fourccmap))
				goto err_out_print_linenum;
		} else if (!strcmp(token[0], "format")) {
			if (get_token(1, 2) < 0)
				goto err_out_parse_error;
			if (!add_to_format(token[0], token[1],
					codec->fourcc,codec->fourccmap))
				goto err_out_print_linenum;
		} else if (!strcmp(token[0], "driver")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!(codec->drv = strdup(token[0]))) {
				mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_CantStrdupDriver, strerror(errno));
				goto err_out;
			}
		} else if (!strcmp(token[0], "dll")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!(codec->dll = strdup(token[0]))) {
				mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_CantStrdupDLL, strerror(errno));
				goto err_out;
			}
		} else if (!strcmp(token[0], "guid")) {
			if (get_token(11, 11) < 0)
				goto err_out_parse_error;
                        codec->guid.f1=strtoul(token[0],&endptr,0);
			if ((*endptr != ',' || *(endptr + 1) != '\0') &&
					*endptr != '\0')
				goto err_out_parse_error;
                        codec->guid.f2=strtoul(token[1],&endptr,0);
			if ((*endptr != ',' || *(endptr + 1) != '\0') &&
					*endptr != '\0')
				goto err_out_parse_error;
                        codec->guid.f3=strtoul(token[2],&endptr,0);
			if ((*endptr != ',' || *(endptr + 1) != '\0') &&
					*endptr != '\0')
				goto err_out_parse_error;
			for (i = 0; i < 8; i++) {
                            codec->guid.f4[i]=strtoul(token[i + 3],&endptr,0);
				if ((*endptr != ',' || *(endptr + 1) != '\0') &&
						*endptr != '\0')
					goto err_out_parse_error;
			}
		} else if (!strcmp(token[0], "out")) {
			if (get_token(1, 2) < 0)
				goto err_out_parse_error;
			if (!add_to_inout(token[0], token[1], codec->outfmt,
						codec->outflags))
				goto err_out_print_linenum;
		} else if (!strcmp(token[0], "in")) {
			if (get_token(1, 2) < 0)
				goto err_out_parse_error;
			if (!add_to_inout(token[0], token[1], codec->infmt,
						codec->inflags))
				goto err_out_print_linenum;
		} else if (!strcmp(token[0], "flags")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "seekable"))
				codec->flags |= CODECS_FLAG_SEEKABLE;
			else
			if (!strcmp(token[0], "align16"))
				codec->flags |= CODECS_FLAG_ALIGN16;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "status")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!strcasecmp(token[0], "working"))
				codec->status = CODECS_STATUS_WORKING;
			else if (!strcasecmp(token[0], "crashing"))
				codec->status = CODECS_STATUS_NOT_WORKING;
			else if (!strcasecmp(token[0], "untested"))
				codec->status = CODECS_STATUS_UNTESTED;
			else if (!strcasecmp(token[0], "buggy"))
				codec->status = CODECS_STATUS_PROBLEMS;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "cpuflags")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!(codec->cpuflags = get_cpuflags(token[0])))
				goto err_out_parse_error;
		} else
			goto err_out_parse_error;
	}
	if (!validate_codec(codec, codec_type))
		goto err_out_not_valid;
	mp_msg(MSGT_CODECCFG,MSGL_INFO,MSGTR_AudioVideoCodecTotals, nr_acodecs, nr_vcodecs);
	if(video_codecs) video_codecs[nr_vcodecs].name = NULL;
	if(audio_codecs) audio_codecs[nr_acodecs].name = NULL;
out:
	free(line);
	line=NULL;
	fclose(fp);
	return 1;

err_out_parse_error:
	mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_ParseError);
err_out_print_linenum:
	PRINT_LINENUM;
err_out:
	codecs_uninit_free();

	free(line);
	line=NULL;
	line_num = 0;
	fclose(fp);
	return 0;
err_out_not_valid:
	mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_CodecDefinitionIncorrect);
	goto err_out_print_linenum;
err_out_release_num:
	mp_msg(MSGT_CODECCFG,MSGL_ERR,MSGTR_OutdatedCodecsConf);
	goto err_out_print_linenum;
}

static void codecs_free(codecs_t* codecs,int count) {
	int i;
		for ( i = 0; i < count; i++)
			if ( codecs[i].name ) {
				if( codecs[i].name )
					free(codecs[i].name);
				if( codecs[i].info )
					free(codecs[i].info);
				if( codecs[i].comment )
					free(codecs[i].comment);
				if( codecs[i].dll )
					free(codecs[i].dll);
				if( codecs[i].drv )
					free(codecs[i].drv);
			}
		if (codecs)
			free(codecs);
}

void codecs_uninit_free(void) {
	if (video_codecs)
	codecs_free(video_codecs,nr_vcodecs);
	video_codecs=NULL;
	if (audio_codecs)
	codecs_free(audio_codecs,nr_acodecs);
	audio_codecs=NULL;
}

codecs_t *find_audio_codec(unsigned int fourcc, unsigned int *fourccmap,
		codecs_t *start, int force)
{
	return find_codec(fourcc, fourccmap, start, 1, force);
}

codecs_t *find_video_codec(unsigned int fourcc, unsigned int *fourccmap,
		codecs_t *start, int force)
{
	return find_codec(fourcc, fourccmap, start, 0, force);
}

codecs_t* find_codec(unsigned int fourcc,unsigned int *fourccmap,
		codecs_t *start, int audioflag, int force)
{
	int i, j;
	codecs_t *c;

#if 0
	if (start) {
		for (/* NOTHING */; start->name; start++) {
			for (j = 0; j < CODECS_MAX_FOURCC; j++) {
				if (start->fourcc[j] == fourcc) {
					if (fourccmap)
						*fourccmap = start->fourccmap[j];
					return start;
				}
			}
		}
	} else 
#endif
        {
		if (audioflag) {
			i = nr_acodecs;
			c = audio_codecs;
		} else {
			i = nr_vcodecs;
			c = video_codecs;
		}
		if(!i) return NULL;
		for (/* NOTHING */; i--; c++) {
                        if(start && c<=start) continue;
			for (j = 0; j < CODECS_MAX_FOURCC; j++) {
				// FIXME: do NOT hardwire 'null' name here:
				if (c->fourcc[j]==fourcc || !strcmp(c->drv,"null")) {
					if (fourccmap)
						*fourccmap = c->fourccmap[j];
					return c;
				}
			}
			if (force) return c;
		}
	}
	return NULL;
}

void stringset_init(stringset_t *set) {
  *set = calloc(1, sizeof(char *));
}

void stringset_free(stringset_t *set) {
  int count = 0;
  while ((*set)[count]) free((*set)[count++]);
  free(*set);
  *set = NULL;
}

void stringset_add(stringset_t *set, const char *str) {
  int count = 0;
  while ((*set)[count]) count++;
  count++;
  *set = realloc(*set, sizeof(char *) * (count + 1));
  (*set)[count - 1] = strdup(str);
  (*set)[count] = NULL;
}

int stringset_test(stringset_t *set, const char *str) {
  stringset_t s;
  for (s = *set; *s; s++)
    if (strcmp(*s, str) == 0)
      return 1;
  return 0;
}

void list_codecs(int audioflag){
	int i;
	codecs_t *c;

		if (audioflag) {
			i = nr_acodecs;
			c = audio_codecs;
			mp_msg(MSGT_CODECCFG,MSGL_INFO,"ac:         afm:      status:   info:  [lib/dll]\n");
		} else {
			i = nr_vcodecs;
			c = video_codecs;
			mp_msg(MSGT_CODECCFG,MSGL_INFO,"vc:         vfm:      status:   info:  [lib/dll]\n");
		}
		if(!i) return;
		for (/* NOTHING */; i--; c++) {
			char* s="unknown ";
			switch(c->status){
			  case CODECS_STATUS_WORKING:     s="working ";break;
			  case CODECS_STATUS_PROBLEMS:    s="problems";break;
			  case CODECS_STATUS_NOT_WORKING: s="crashing";break;
			  case CODECS_STATUS_UNTESTED:    s="untested";break;
			}
			if(c->dll)
			  mp_msg(MSGT_CODECCFG,MSGL_INFO,"%-11s %-9s %s  %s  [%s]\n",c->name,c->drv,s,c->info,c->dll);
			else
			  mp_msg(MSGT_CODECCFG,MSGL_INFO,"%-11s %-9s %s  %s\n",c->name,c->drv,s,c->info);
			
		}

}



#ifdef CODECS2HTML
/*
 * Fake out GUI references when building the codecs2html utility.
 */
#ifdef CONFIG_GUI
void gtkMessageBox( int type,char * str ) { return; }
int use_gui = 0;
#endif

void wrapline(FILE *f2,char *s){
    int c;
    if(!s){
        fprintf(f2,"-");
        return;
    }
    while((c=*s++)){
        if(c==',') fprintf(f2,"<br>"); else fputc(c,f2);
    }
}

void parsehtml(FILE *f1,FILE *f2,codecs_t *codec,int section,int dshow){
        int c,d;
        while((c=fgetc(f1))>=0){
            if(c!='%'){
                fputc(c,f2);
                continue;
            }
            d=fgetc(f1);
            
            switch(d){
            case '.':
                return; // end of section
            case 'n':
                wrapline(f2,codec->name); break;
            case 'i':
                wrapline(f2,codec->info); break;
            case 'c':
                wrapline(f2,codec->comment); break;
            case 'd':
                wrapline(f2,codec->dll); break;
            case 'D':
                fprintf(f2,"%c",!strcmp(codec->drv,"dshow")?'+':'-'); break;
            case 'F':
                for(d=0;d<CODECS_MAX_FOURCC;d++)
                    if(!d || codec->fourcc[d]!=0xFFFFFFFF)
                        fprintf(f2,"%s%.4s",d?"<br>":"",(codec->fourcc[d]==0xFFFFFFFF || codec->fourcc[d]<0x20202020)?!d?"-":"":(char*) &codec->fourcc[d]);
                break;
            case 'f':
                for(d=0;d<CODECS_MAX_FOURCC;d++)
                    if(codec->fourcc[d]!=0xFFFFFFFF)
                        fprintf(f2,"%s0x%X",d?"<br>":"",codec->fourcc[d]);
                break;
            case 'Y':
                for(d=0;d<CODECS_MAX_OUTFMT;d++)
                    if(codec->outfmt[d]!=0xFFFFFFFF){
		        for (c=0; fmt_table[c].name; c++)
                            if(fmt_table[c].num==codec->outfmt[d]) break;
                        if(fmt_table[c].name)
                            fprintf(f2,"%s%s",d?"<br>":"",fmt_table[c].name);
                    }
                break;
            default:
                fputc(c,f2);
                fputc(d,f2);
            }
        }

}

void skiphtml(FILE *f1){
        int c,d;
        while((c=fgetc(f1))>=0){
            if(c!='%'){
                continue;
            }
            d=fgetc(f1);
            if(d=='.') return; // end of section
        }
}

static void print_int_array(const int* a, int size)
{
	printf("{ ");
	while (size--)
	    if(abs(*a)<256)
		printf("%d%s", *a++, size?", ":"");
	    else
		printf("0x%X%s", *a++, size?", ":"");
	printf(" }");
}

static void print_char_array(const unsigned char* a, int size)
{
	printf("{ ");
	while (size--) 
	    if((*a)<10)
		printf("%d%s", *a++, size?", ":"");
	    else
		printf("0x%02x%s", *a++, size?", ":"");
	printf(" }");
}

static void print_string(const char* s)
{
	if (!s) printf("NULL");
	else printf("\"%s\"", s);
}

int main(int argc, char* argv[])
{
	codecs_t *cl;
        FILE *f1;
        FILE *f2;
        int c,d,i;
        int pos;
        int section=-1;
        int nr_codecs;
        int win32=-1;
        int dshow=-1;
        int win32ex=-1;

	/*
	 * Take path to codecs.conf from command line, or fall back on
	 * etc/codecs.conf
	 */
	if (!(nr_codecs = parse_codec_cfg((argc>1)?argv[1]:"etc/codecs.conf")))
		exit(1);

	if (argc > 1) {
		int i, j;
		const char* nm[2];
		codecs_t* cod[2];
		int nr[2];

		nm[0] = "builtin_video_codecs";
		cod[0] = video_codecs;
		nr[0] = nr_vcodecs;
		
		nm[1] = "builtin_audio_codecs";
		cod[1] = audio_codecs;
		nr[1] = nr_acodecs;
		
		printf("/* GENERATED FROM %s, DO NOT EDIT! */\n\n",argv[1]);
		printf("#include <stddef.h>\n",argv[1]);
		printf("#include \"codec-cfg.h\"\n\n",argv[1]);
		
		for (i=0; i<2; i++) {
		  	printf("const codecs_t %s[] = {\n", nm[i]);
			for (j = 0; j < nr[i]; j++) {
			  	printf("{");

				print_int_array(cod[i][j].fourcc, CODECS_MAX_FOURCC);
				printf(", /* fourcc */\n");
				
				print_int_array(cod[i][j].fourccmap, CODECS_MAX_FOURCC);
				printf(", /* fourccmap */\n");
				
				print_int_array(cod[i][j].outfmt, CODECS_MAX_OUTFMT);
				printf(", /* outfmt */\n");
				
				print_char_array(cod[i][j].outflags, CODECS_MAX_OUTFMT);
				printf(", /* outflags */\n");
				
				print_int_array(cod[i][j].infmt, CODECS_MAX_INFMT);
				printf(", /* infmt */\n");
				
				print_char_array(cod[i][j].inflags, CODECS_MAX_INFMT);
				printf(", /* inflags */\n");
				
				print_string(cod[i][j].name);    printf(", /* name */\n");
				print_string(cod[i][j].info);    printf(", /* info */\n");
				print_string(cod[i][j].comment); printf(", /* comment */\n");
				print_string(cod[i][j].dll);     printf(", /* dll */\n");
				print_string(cod[i][j].drv);     printf(", /* drv */\n");
				
				printf("{ 0x%08lx, %hu, %hu,",
				       cod[i][j].guid.f1,
				       cod[i][j].guid.f2,
				       cod[i][j].guid.f3);
				print_char_array(cod[i][j].guid.f4, sizeof(cod[i][j].guid.f4));
				printf(" }, /* GUID */\n");
				printf("%hd /* flags */, %hd /* status */, %hd /* cpuflags */ }\n",
				       cod[i][j].flags,
				       cod[i][j].status,
				       cod[i][j].cpuflags);
				if (j < nr[i]) printf(",\n");
			}
			printf("};\n\n");
		}
		exit(0);
	}

        f1=fopen("DOCS/tech/codecs-in.html","rb"); if(!f1) exit(1);
        f2=fopen("DOCS/codecs-status.html","wb"); if(!f2) exit(1);
        
        while((c=fgetc(f1))>=0){
            if(c!='%'){
                fputc(c,f2);
                continue;
            }
            d=fgetc(f1);
            if(d>='0' && d<='9'){
                // begin section
                section=d-'0';
                //printf("BEGIN %d\n",section);
                if(section>=5){
                    // audio
		    cl = audio_codecs;
		    nr_codecs = nr_acodecs;
                    dshow=7;win32=4;
                } else {
                    // video
		    cl = video_codecs;
		    nr_codecs = nr_vcodecs;
                    dshow=4;win32=2;win32ex=6;
                }
                pos=ftell(f1);
                for(i=0;i<nr_codecs;i++){
                    fseek(f1,pos,SEEK_SET);
                    switch(section){
                    case 0:
                    case 5:
                        if(cl[i].status==CODECS_STATUS_WORKING)
//                            if(!(!strcmp(cl[i].drv,"vfw") || !strcmp(cl[i].drv,"dshow") || !strcmp(cl[i].drv,"vfwex") || !strcmp(cl[i].drv,"acm")))
                                parsehtml(f1,f2,&cl[i],section,dshow);
                        break;
#if 0
                    case 1:
                    case 6:
                        if(cl[i].status==CODECS_STATUS_WORKING)
                            if((!strcmp(cl[i].drv,"vfw") || !strcmp(cl[i].drv,"dshow") || !strcmp(cl[i].drv,"vfwex") || !strcmp(cl[i].drv,"acm")))
                                parsehtml(f1,f2,&cl[i],section,dshow);
                        break;
#endif
                    case 2:
                    case 7:
                        if(cl[i].status==CODECS_STATUS_PROBLEMS)
                            parsehtml(f1,f2,&cl[i],section,dshow);
                        break;
                    case 3:
                    case 8:
                        if(cl[i].status==CODECS_STATUS_NOT_WORKING)
                            parsehtml(f1,f2,&cl[i],section,dshow);
                        break;
                    case 4:
                    case 9:
                        if(cl[i].status==CODECS_STATUS_UNTESTED)
                            parsehtml(f1,f2,&cl[i],section,dshow);
                        break;
                    default:
                        printf("Warning! unimplemented section: %d\n",section);
                    }
                }
                fseek(f1,pos,SEEK_SET);
                skiphtml(f1);
//void parsehtml(FILE *f1,FILE *f2,codecs_t *codec,int section,int dshow){
                
                continue;
            }
            fputc(c,f2);
            fputc(d,f2);
        }
        
        fclose(f2);
        fclose(f1);
	return 0;
}

#endif

#ifdef TESTING
int main(void)
{
	codecs_t *c;
        int i,j, nr_codecs, state;

	if (!(parse_codec_cfg("etc/codecs.conf")))
		return 0;
	if (!video_codecs)
		printf("no videoconfig.\n");
	if (!audio_codecs)
		printf("no audioconfig.\n");

	printf("videocodecs:\n");
	c = video_codecs;
	nr_codecs = nr_vcodecs;
	state = 0;
next:
	if (c) {
		printf("number of %scodecs: %d\n", state==0?"video":"audio",
		    nr_codecs);
		for(i=0;i<nr_codecs;i++, c++){
		    printf("\n============== %scodec %02d ===============\n",
			state==0?"video":"audio",i);
		    printf("name='%s'\n",c->name);
		    printf("info='%s'\n",c->info);
		    printf("comment='%s'\n",c->comment);
		    printf("dll='%s'\n",c->dll);
		    /* printf("flags=%X  driver=%d status=%d cpuflags=%d\n",
		              c->flags, c->driver, c->status, c->cpuflags); */
		    printf("flags=%X status=%d cpuflags=%d\n",
				    c->flags, c->status, c->cpuflags);

		    for(j=0;j<CODECS_MAX_FOURCC;j++){
		      if(c->fourcc[j]!=0xFFFFFFFF){
			  printf("fourcc %02d:  %08X (%.4s) ===> %08X (%.4s)\n",j,c->fourcc[j],(char *) &c->fourcc[j],c->fourccmap[j],(char *) &c->fourccmap[j]);
		      }
		    }

		    for(j=0;j<CODECS_MAX_OUTFMT;j++){
		      if(c->outfmt[j]!=0xFFFFFFFF){
			  printf("outfmt %02d:  %08X (%.4s)  flags: %d\n",j,c->outfmt[j],(char *) &c->outfmt[j],c->outflags[j]);
		      }
		    }

		    for(j=0;j<CODECS_MAX_INFMT;j++){
		      if(c->infmt[j]!=0xFFFFFFFF){
			  printf("infmt %02d:  %08X (%.4s)  flags: %d\n",j,c->infmt[j],(char *) &c->infmt[j],c->inflags[j]);
		      }
		    }

		    printf("GUID: %08lX %04X %04X",c->guid.f1,c->guid.f2,c->guid.f3);
		    for(j=0;j<8;j++) printf(" %02X",c->guid.f4[j]);
		    printf("\n");

		    
		}
	}
	if (!state) {
		printf("audiocodecs:\n");
		c = audio_codecs;
		nr_codecs = nr_acodecs;
		state = 1;
		goto next;
	}
	return 0;
}

#endif
