
//#define DEBUG
#define PRINT_LINENUM
//	printf("%s(%d): ", cfgfile, line_num)


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

#define MALLOC_ADD	10

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
#define RET_OK		0

static FILE *fp;
static int line_num = 0;
static int line_pos;	/* line pos */
static int firstdef = 1;
static char *line;
static char *token;

static codecs_t *codecs=NULL;
static int nr_codecs = 0;

static int get_token(void)
{
	static int read_nextline = 1;

	if (read_nextline) {
		if (!fgets(line, MAX_LINE_LEN, fp))
			goto ret_eof;
		line_pos = 0;
		++line_num;
		read_nextline = 0;
	}
	while (isspace(line[line_pos]))
		++line_pos;
	if (line[line_pos] == '\0' || line[line_pos] == '#' ||
			line[line_pos] == ';') {
		read_nextline = 1;
		goto ret_eol;
	}
	token = line + line_pos;
	if (line[line_pos] == '"') {
		token++;
		for (/* NOTHING */; line[++line_pos] != '"' && line[line_pos];)
			/* NOTHING */;
		if (!line[line_pos]) {
			read_nextline = 1;
			goto ret_eol;
		}
	} else {
		for (/* NOTHING */; !isspace(line[line_pos]); line_pos++)
			/* NOTHING */;
	}
	line[line_pos] = '\0';
	line_pos++;
#ifdef DEBUG
	printf("get_token ok\n");
#endif
	return RET_OK;
ret_eof:
#ifdef DEBUG
	printf("get_token EOF\n");
#endif
	token = NULL;
	return RET_EOF;
ret_eol:
#ifdef DEBUG
	printf("get_token EOL\n");
#endif
	token = NULL;
	return RET_EOL;
}

static int add_to_fourcc(char *s, char *alias, unsigned int *fourcc,
		unsigned int *map)
{
	int i;
	char **aliasp;

	/* find first unused slot */
	for (i = 0; i < CODECS_MAX_FOURCC && fourcc[i] != 0xffffffff; i++)
		/* NOTHING */;
	if (i == CODECS_MAX_FOURCC) {
		printf("too many fourcc...\n");
		return 0;
	}
#if 1
	if (alias) {
		do {
			fourcc[i] = *((unsigned int *) s);
			map[i] = *((unsigned int *) alias);
			s += 4;
			i++;
		} while (*(s++) == ',');
	} else {
		do {
			fourcc[i] = *((unsigned int *) s);
			map[i] = *((unsigned int *) s);
			s += 4;
			i++;
		} while (*(s++) == ',');
	}
#else
	if (alias)
		aliasp = &alias;
	else
		aliasp = &s;
	do {
		fourcc[i] = *((unsigned int *) s);
		map[i++] = *((unsigned int *) (*aliasp));
		s += 4;
	} while (*(s++) == ',');
#endif
	if (*(--s) != '\0')
		return 0;
	return 1;
}

static int add_to_format(char *s, unsigned int *fourcc, unsigned int *fourccmap)
{
//        printf("\n-----[%s][%s]-----\n",s,format);

	int i;

	/* find first unused slot */
	for (i = 0; i < CODECS_MAX_FOURCC && fourcc[i] != 0xffffffff; i++)
		/* NOTHING */;
	if (i == CODECS_MAX_FOURCC) {
		printf("too many fourcc...\n");
		return 0;
	}
        
        fourcc[i]=fourccmap[i]=strtoul(s,NULL,0);

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

	int i, j;
	unsigned char flags;

	for (i = 0; i < CODECS_MAX_OUTFMT && outfmt[i] != 0xffffffff; i++)
		/* NOTHING */;
	if (i == CODECS_MAX_FOURCC) {
		printf("too many out...\n");
		return 0;
	}

	flags = 0; //get_flags(sflags);
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
	} while (*(sfmt++) == ',');
	if (*(--sfmt) != '\0') return 0;
        
	return 1;
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


codecs_t *parse_codec_cfg(char *cfgfile)
{

//	codecs_t *codecs = NULL;  // array of codecs
	codecs_t *codec = NULL;   // currect codec
	int free_slots = 0;
	int tmp, i;
	int state = 0;
	char *param1;

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
	line_pos = 0;
	line[0] = '\0';		/* forces get_token to read next line */

	for (;;) {
		tmp = get_token();
		if (tmp == RET_EOF)
			goto eof_out;
		if (tmp == RET_EOL)
			continue;
		if (!strcmp(token, "audiocodec") || !strcmp(token, "videocodec")) {
			PRINT_LINENUM;
                        
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
                        
			if (*token == 'a') {		/* audiocodec */
				//printf("audio");
				codec->flags |= CODECS_FLAG_AUDIO;
			} else if (*token == 'v') {	/* videocodec */
				//printf("video");
				codec->flags &= !CODECS_FLAG_AUDIO;
			} else {
				printf("itt valami nagyon el van baszva\n");
				goto err_out;
			}
			if (get_token() < 0)
				goto parse_error_out;
			codec->name = strdup(token);
			state |= GOT_NAME;
			//printf(" %s\n", codec->name);
		} else if (!strcmp(token, "info")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			//printf("info");
			if (state & GOT_INFO || get_token() < 0)
				goto parse_error_out;
			codec->info = strdup(token);
			state |= GOT_INFO;
			//printf(" %s\n", codec->info);
		} else if (!strcmp(token, "comment")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			//printf("comment");
			if (get_token() < 0)
				goto parse_error_out;
#if 1
			if (!codec->comment)
				codec->comment = strdup(token);
			//printf(" %s\n", codec->comment);
#else
			add_comment(token, &codec->comment);
			printf(" FIXMEEEEEEEEEEEEEEE\n");
#endif
		} else if (!strcmp(token, "fourcc")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			//printf("fourcc");
//			if (codec->flags & CODECS_FLAG_AUDIO) {
//				printf("\n'fourcc' in audiocodec definition!\n");
//				goto err_out;
//			}
			if (get_token() < 0)
				goto parse_error_out;
			param1 = strdup(token);
			get_token();
			if (!add_to_fourcc(param1, token,
						codec->fourcc,
						codec->fourccmap))
				goto err_out;
			state |= GOT_FOURCC;
			//printf(" %s: %s\n", param1, token);
			free(param1);
		} else if (!strcmp(token, "format")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			//printf("format");
//			if (!(codec->flags & CODECS_FLAG_AUDIO)) {
//				printf("\n'format' in videocodec definition!\n");
//				goto err_out;
//			}
			if (get_token() < 0)
				goto parse_error_out;
			if (!add_to_format(token, codec->fourcc,codec->fourccmap))
				goto err_out;
			state |= GOT_FORMAT;
			//printf(" %s\n", token);
		} else if (!strcmp(token, "driver")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			//printf("driver");
			if (get_token() < 0)
				goto parse_error_out;
			if ((codec->driver = get_driver(token,codec->flags&CODECS_FLAG_AUDIO)) == -1)
				goto err_out;
			//printf(" %s\n", token);
		} else if (!strcmp(token, "dll")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			//printf("dll");
			if (get_token() < 0)
				goto parse_error_out;
			codec->dll = strdup(token);
			//printf(" %s\n", codec->dll);
		} else if (!strcmp(token, "guid")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			//printf("guid");
			if (get_token() < 0) goto parse_error_out;
                        //printf("'%s'",token);
                        codec->guid.f1=strtoul(token,NULL,0);
			if (get_token() < 0) goto parse_error_out;
                        codec->guid.f2=strtoul(token,NULL,0);
			if (get_token() < 0) goto parse_error_out;
                        codec->guid.f3=strtoul(token,NULL,0);
			for (i = 0; i < 8; i++) {
			    if (get_token() < 0) goto parse_error_out;
                            codec->guid.f4[i]=strtoul(token,NULL,0);
			}
		} else if (!strcmp(token, "out")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			//printf("out");
			if (get_token() < 0)
				goto parse_error_out;
			param1 = strdup(token);
			get_token();
			if (!add_to_out(param1, token, codec->outfmt,
						codec->outflags))
				goto err_out;
			//printf(" %s: %s\n", param1, token);
			free(param1);
		} else if (!strcmp(token, "flags")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			//printf("flags");
			if (get_token() < 0)
				goto parse_error_out;
			//printf(" %s\n", token);
		} else if (!strcmp(token, "status")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			if (get_token() < 0)
				goto parse_error_out;
			if (!strcasecmp(token, "rulz"))
				codec->status = CODECS_STATUS_WORKING;
			else if (!strcasecmp(token, "suxx"))
				codec->status = CODECS_STATUS_NOT_WORKING;
			else if (!strcasecmp(token, "checkthiz"))
				codec->status = CODECS_STATUS_UNTESTED;
			else if (!strcasecmp(token, "notsogood"))
				codec->status = CODECS_STATUS_PROBLEMS;
			else
				goto parse_error_out;
		} else
			goto parse_error_out;
	}
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
eof_out:
	/* FIXME teljes az utolso config?? */
	goto out;
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

	codecs = parse_codec_cfg("DOCS/codecs.conf");
        
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

            printf("GUID: %08X %04X %04X",c->guid.f1,c->guid.f2,c->guid.f3);
            for(j=0;j<8;j++) printf(" %02X",c->guid.f4[j]);
            printf("\n");

            
        }
        
	return 0;
}

#endif
