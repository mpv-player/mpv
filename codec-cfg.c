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

FILE *fp;
int line_num = 0;
int line_pos;	/* line pos */
int firstdef = 1;
char *line;
char *token;

int nr_codecs = 0;

int get_token(void)
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

int add_to_fourcc(char *s, char *alias, unsigned int *fourcc,
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

int add_to_format(char *s, unsigned int *format)
{
	return 1;
}

short get_flags(char *s)
{
	if (!s)
		return 0;
	return 1;
}

int add_to_out(char *sfmt, char *sflags, unsigned int *outfmt,
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
	int i, j;
	unsigned char flags;

	for (i = 0; i < CODECS_MAX_OUTFMT && outfmt[i] != 0xffffffff; i++)
		/* NOTHING */;
	if (i == CODECS_MAX_FOURCC) {
		printf("too many out...\n");
		return 0;
	}

	flags = get_flags(sflags);

	do {
		for (j = 0; fmtstr[i] != NULL; j++)
			if (!strncmp(sfmt, fmtstr[j], strlen(fmtstr[j])))
				break;
		if (fmtstr[j] == NULL)
			return 0;
		outfmt[i] = fmtnum[j];
		outflags[i] = flags;
		sfmt+=strlen(fmtstr[j]);
	} while (*(sfmt++) == ',');
	if (*(--sfmt) != '\0')
		return 0;
	return 1;
}

short get_driver(char *s)
{
	return 0;
}

#define DEBUG

codecs_t *parse_codec_cfg(char *cfgfile)
{
#define PRINT_LINENUM	printf("%s(%d): ", cfgfile, line_num)
#define GET_MEM\
	do {\
		if (!(codecs = (codecs_t *) realloc(codecs,\
				sizeof(codecs_t) * (nr_codecs + 1)))) {\
			perror("parse_codec_cfg: can't realloc 'codecs'");\
			goto err_out;\
		}\
	} while (0)

	codecs_t *codecs = NULL;
	int free_slots = 0;
	int tmp, i;
	int state = 0;
	char *param1;

#ifdef DEBUG
	assert(cfgfile != NULL);
#endif

	printf("Reading codec config file: %s\n", cfgfile);

	if ((line = (char *) malloc(MAX_LINE_LEN + 1)) == NULL) {
		perror("parse_codec_cfg: can't get memory for 'line'");
		return NULL;
	}

	if ((fp = fopen(cfgfile, "r")) == NULL) {
		printf("parse_codec_cfg: can't open '%s': %s\n", cfgfile, strerror(errno));
		free(line);
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
			if (codecs) {
//				tmp = ~state & STATE_MASK;
//				if (tmp != GOT_FOURCC && tmp != GOT_FORMAT)
//					goto parse_error_out;
				nr_codecs++;
			}
			PRINT_LINENUM;
			GET_MEM;
			state = 0;
			codecs[nr_codecs].comment = NULL;
			memset(codecs[nr_codecs].fourcc, 0xff,
					sizeof(codecs[nr_codecs].fourcc) );
/*
			memset(codecs[nr_codecs].fourccmap, 0xff,
					sizeof(codecs[nr_codecs].fourccmap) *
					CODECS_MAX_FOURCC);
*/
			memset(codecs[nr_codecs].outfmt, 0xff,
					sizeof(codecs[nr_codecs].outfmt) );
			if (*token == 'a') {		/* audiocodec */
				printf("audio");
				codecs[nr_codecs].flags |= CODECS_FLAG_AUDIO;
			} else if (*token == 'v') {	/* videocodec */
				printf("video");
				codecs[nr_codecs].flags &= !CODECS_FLAG_AUDIO;
			} else {
				printf("itt valami nagyon el van baszva\n");
				goto err_out;
			}
			if (get_token() < 0)
				goto parse_error_out;
			codecs[nr_codecs].name = strdup(token);
			state |= GOT_NAME;
			printf(" %s\n", codecs[nr_codecs].name);
		} else if (!strcmp(token, "info")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			printf("info");
			if (state & GOT_INFO || get_token() < 0)
				goto parse_error_out;
			codecs[nr_codecs].info = strdup(token);
			state |= GOT_INFO;
			printf(" %s\n", codecs[nr_codecs].info);
		} else if (!strcmp(token, "comment")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			printf("comment");
			if (get_token() < 0)
				goto parse_error_out;
#if 1
			if (!codecs[nr_codecs].comment)
				codecs[nr_codecs].comment = strdup(token);
			printf(" %s\n", codecs[nr_codecs].comment);
#else
			add_comment(token, &codecs[nr_codecs].comment);
			printf(" FIXMEEEEEEEEEEEEEEE\n");
#endif
		} else if (!strcmp(token, "fourcc")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			printf("fourcc");
			if (codecs[nr_codecs].flags & CODECS_FLAG_AUDIO) {
				printf("\n'fourcc' in audiocodec definition!\n");
				goto err_out;
			}
			if (get_token() < 0)
				goto parse_error_out;
			param1 = strdup(token);
			get_token();
			if (!add_to_fourcc(param1, token,
						codecs[nr_codecs].fourcc,
						codecs[nr_codecs].fourccmap))
				goto err_out;
			state |= GOT_FOURCC;
			printf(" %s: %s\n", param1, token);
			free(param1);
		} else if (!strcmp(token, "format")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			printf("format");
			if (!(codecs[nr_codecs].flags & CODECS_FLAG_AUDIO)) {
				printf("\n'format' in videocodec definition!\n");
				goto err_out;
			}
			if (get_token() < 0)
				goto parse_error_out;
			if (!add_to_format(token, codecs[nr_codecs].fourcc))
				goto err_out;
			state |= GOT_FORMAT;
			printf(" %s\n", token);
		} else if (!strcmp(token, "driver")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			printf("driver");
			if (get_token() < 0)
				goto parse_error_out;
			if ((codecs[nr_codecs].driver = get_driver(token)) == -1)
				goto err_out;
			printf(" %s\n", token);
		} else if (!strcmp(token, "dll")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			printf("dll");
			if (get_token() < 0)
				goto parse_error_out;
			codecs[nr_codecs].dll = strdup(token);
			printf(" %s\n", codecs[nr_codecs].dll);
		} else if (!strcmp(token, "guid")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			printf("guid");
			if (get_token() < 0)
				goto parse_error_out;
			sscanf(token, "%ld,", &codecs[nr_codecs].guid.f1);
			if (get_token() < 0)
				goto parse_error_out;
			sscanf(token, "%d,", &tmp);
			codecs[nr_codecs].guid.f2 = tmp;
			if (get_token() < 0)
				goto parse_error_out;
			sscanf(token, "%d,", &tmp);
			codecs[nr_codecs].guid.f3 = tmp;
			for (i = 0; i < 7; i++) {
				if (get_token() < 0)
					goto parse_error_out;
				sscanf(token, "%d,", &tmp);
				codecs[nr_codecs].guid.f4[i] = tmp;
			}
			if (get_token() < 0)
				goto parse_error_out;
			sscanf(token, "%d", &tmp);
			codecs[nr_codecs].guid.f4[7] = tmp;
			printf(" %s\n", token);
		} else if (!strcmp(token, "out")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			printf("out");
			if (get_token() < 0)
				goto parse_error_out;
			param1 = strdup(token);
			get_token();
			if (!add_to_out(param1, token, codecs[nr_codecs].outfmt,
						codecs[nr_codecs].outflags))
				goto err_out;
			printf(" %s: %s\n", param1, token);
			free(param1);
		} else if (!strcmp(token, "flags")) {
			if (!(state & GOT_NAME))
				goto parse_error_out;
			PRINT_LINENUM;
			printf("flags");
			if (get_token() < 0)
				goto parse_error_out;
			printf(" %s\n", token);
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

#ifdef TESTING
int main(void)
{
	codecs_t *codecs;
        int i,j;

	codecs = parse_codec_cfg("codecs.conf");
        
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
        }
        
	return 0;
}

#endif
