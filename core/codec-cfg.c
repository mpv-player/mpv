/*
 * codecs.conf parser
 *
 * Copyright (C) 2001 Szabolcs Berecz <szabi@inf.elte.hu>
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "config.h"
#include "core/mp_msg.h"
#include "video/img_format.h"
#include "codec-cfg.h"
#include "core/bstr.h"
#include "stream/stream.h"
#include "core/path.h"

static const char embedded_file[] =
#include "codecs.conf.h"
    ;
static const struct bstr builtin_codecs_conf = {
    .start = (char *)embedded_file, .len = sizeof(embedded_file) - 1
};

#define mmioFOURCC( ch0, ch1, ch2, ch3 )				\
		( (uint32_t)(uint8_t)(ch0) | ( (uint32_t)(uint8_t)(ch1) << 8 ) |	\
		( (uint32_t)(uint8_t)(ch2) << 16 ) | ( (uint32_t)(uint8_t)(ch3) << 24 ) )

#define PRINT_LINENUM mp_msg(MSGT_CODECCFG,MSGL_ERR," at line %d\n", line_num)

#define MAX_NR_TOKEN    16

#define RET_EOF         -1
#define RET_EOL         -2

#define TYPE_VIDEO      0
#define TYPE_AUDIO      1

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
        if (strlen(s) < 4)
            goto err_out_parse_error;
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
    mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"duplicated FourCC");
    return 0;
err_out_too_many:
    mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"too many FourCCs/formats...");
    return 0;
err_out_parse_error:
    mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"parse error");
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
        mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"too many FourCCs/formats...");
        return 0;
    }

    fourcc[i]=strtoul(s,&endptr,0);
    if (*endptr != '\0') {
        mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"parse error (format ID not a number?)");
        return 0;
    }

    if(alias){
        fourccmap[i]=strtoul(alias,&endptr,0);
        if (*endptr != '\0') {
            mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"parse error (format ID alias not a number?)");
            return 0;
        }
    } else
        fourccmap[i]=fourcc[i];

    for (j = 0; j < i; j++)
        if (fourcc[j] == fourcc[i]) {
            mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"duplicated format ID");
            return 0;
        }

    return 1;
}

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
        for (j = 0; isalnum(sfmt[j]) || sfmt[j] == '_'; j++);
        unsigned int fmt = mp_imgfmt_from_name((bstr) {sfmt, j}, true);
        if (!fmt)
            goto err_out_parse_error;
        outfmt[i] = fmt;
        outflags[i] = flags;
        ++i;
        sfmt += j;
    } while ((*(sfmt++) == ',') && --freeslots);

    if (!freeslots)
        goto err_out_too_many;

    if (*(--sfmt) != '\0')
        goto err_out_parse_error;

    return 1;
err_out_too_many:
    mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"too many out...");
    return 0;
err_out_parse_error:
    mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"parse error");
    return 0;
}

static int validate_codec(codecs_t *c, int type)
{
    unsigned int i;
    char *tmp_name = c->name;

    for (i = 0; i < strlen(tmp_name) && isalnum(tmp_name[i]); i++)
        /* NOTHING */;

    if (i < strlen(tmp_name)) {
        mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"\ncodec(%s) name is not valid!\n", c->name);
        return 0;
    }

    if (!c->info)
        c->info = strdup(c->name);

    if (!c->drv) {
        mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"\ncodec(%s) does not have a driver!\n", c->name);
        return 0;
    }

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
        mp_tmsg(MSGT_CODECCFG,MSGL_FATAL,"Can't allocate memory for comment. ");
        return 0;
    }
    strcpy(*d + pos, s);
    return 1;
}

static struct bstr filetext;
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
        mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"get_token(): max >= MAX_MR_TOKEN!");
        goto out_eof;
    }

    memset(token, 0x00, sizeof(*token) * max);

    if (read_nextline) {
        if (!filetext.len)
            goto out_eof;
        struct bstr nextline = bstr_getline(filetext, &filetext);
        line = nextline.start;
        line[nextline.len - 1] = 0;
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
    char *endptr;   // strtoul()...
    int *nr_codecsp;
    int codec_type;     /* TYPE_VIDEO/TYPE_AUDIO */
    int tmp, i;
    int codec_cfg_min;

    for (struct bstr s = builtin_codecs_conf; ; bstr_getline(s, &s)) {
        if (!s.len)
            abort();
        if (bstr_eatstart0(&s, "release ")) {
            codec_cfg_min = atoi(s.start);
            break;
        }
    }

    // in case we call it a second time
    codecs_uninit_free();

    nr_vcodecs = 0;
    nr_acodecs = 0;

    if (cfgfile) {
        // Avoid printing errors from open_stream when trying optional files
        if (!mp_path_exists(cfgfile)) {
            mp_tmsg(MSGT_CODECCFG, MSGL_V,
                    "No optional codecs config file: %s\n", cfgfile);
            return 0;
        }
        mp_msg(MSGT_CODECCFG, MSGL_V, "Reading codec config file: %s\n",
                cfgfile);
        struct stream *s = open_stream(cfgfile, NULL, NULL);
        if (!s)
            return 0;
        filetext = stream_read_complete(s, NULL, 10000000, 1);
        free_stream(s);
        if (!filetext.start)
            return 0;
    } else
        // Parsing modifies the data
        filetext = bstrdup(NULL, builtin_codecs_conf);
    void *tmpmem = filetext.start;

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
        if (tmp < codec_cfg_min)
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
            } else {
                assert(*token[0] == 'a');
                codec_type = TYPE_AUDIO;
                nr_codecsp = &nr_acodecs;
                codecsp = &audio_codecs;
            }
            if (!(*codecsp = realloc(*codecsp,
                                     sizeof(codecs_t) * (*nr_codecsp + 2)))) {
                mp_tmsg(MSGT_CODECCFG,MSGL_FATAL,"Can't realloc '*codecsp': %s\n", strerror(errno));
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
                    mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"Codec name '%s' isn't unique.", token[0]);
                    goto err_out_print_linenum;
                }
            }
            if (!(codec->name = strdup(token[0]))) {
                mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"Can't strdup -> 'name': %s\n", strerror(errno));
                goto err_out;
            }
        } else if (!strcmp(token[0], "info")) {
            if (codec->info || get_token(1, 1) < 0)
                goto err_out_parse_error;
            if (!(codec->info = strdup(token[0]))) {
                mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"Can't strdup -> 'info': %s\n", strerror(errno));
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
                mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"Can't strdup -> 'driver': %s\n", strerror(errno));
                goto err_out;
            }
        } else if (!strcmp(token[0], "dll")) {
            if (get_token(1, 1) < 0)
                goto err_out_parse_error;
            if (!(codec->dll = strdup(token[0]))) {
                mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"Can't strdup -> 'dll': %s", strerror(errno));
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
            else if (!strcmp(token[0], "align16"))
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
        } else if (!strcmp(token[0], "anyinput")) {
            codec->anyinput = true;
        } else
            goto err_out_parse_error;
    }
    if (!validate_codec(codec, codec_type))
        goto err_out_not_valid;
    mp_tmsg(MSGT_CODECCFG, MSGL_V, "%d audio & %d video codecs\n", nr_acodecs,
            nr_vcodecs);
    if(video_codecs) video_codecs[nr_vcodecs].name = NULL;
    if(audio_codecs) audio_codecs[nr_acodecs].name = NULL;
out:
    talloc_free(tmpmem);
    line=NULL;
    return 1;

err_out_parse_error:
    mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"parse error");
err_out_print_linenum:
    PRINT_LINENUM;
err_out:
    codecs_uninit_free();

    talloc_free(tmpmem);
    line=NULL;
    line_num = 0;
    return 0;
err_out_not_valid:
    mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"Codec is not defined correctly.");
    goto err_out_print_linenum;
err_out_release_num:
    mp_tmsg(MSGT_CODECCFG,MSGL_ERR,"This codecs.conf is too old and incompatible with this MPlayer release!");
    goto err_out_print_linenum;
}

static void codecs_free(codecs_t* codecs,int count) {
    int i;
    for ( i = 0; i < count; i++)
        if ( codecs[i].name ) {
            free(codecs[i].name);
            free(codecs[i].info);
            free(codecs[i].comment);
            free(codecs[i].dll);
            free(codecs[i].drv);
        }
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

struct codecs *find_codec(unsigned int fourcc, unsigned int *fourccmap,
                          codecs_t *start, int audioflag, int force)
{
    struct codecs *c, *end;

    if (audioflag) {
        c = audio_codecs;
        end = c + nr_acodecs;
    } else {
        c = video_codecs;
        end = c + nr_vcodecs;
    }
    if (start)
        c = start + 1; // actually starts from the next one after the given one
    for (; c < end; c++) {
        for (int j = 0; j < CODECS_MAX_FOURCC; j++) {
            if (c->fourcc[j] == -1)
                break;
            if (c->fourcc[j] == fourcc) {
                if (fourccmap)
                    *fourccmap = c->fourccmap[j];
                return c;
            }
        }
        if (c->anyinput || force)
            return c;
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
        mp_msg(MSGT_CODECCFG,MSGL_INFO,"ac:     afm:      status:   info:  [lib/dll]\n");
    } else {
        i = nr_vcodecs;
        c = video_codecs;
        mp_msg(MSGT_CODECCFG,MSGL_INFO,"vc:     vfm:      status:   info:  [lib/dll]\n");
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
