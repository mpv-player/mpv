/*
 * Subtitle reader with format autodetection
 *
 * Written by laaz
 * Some code cleanup & realloc() by A'rpi/ESP-team
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <dirent.h>

#include "config.h"
#include "mp_msg.h"
#include "subreader.h"
#include "stream/stream.h"

#ifdef CONFIG_ENCA
#include <enca.h>
#endif

#define ERR ((void *) -1)

#ifdef CONFIG_ICONV
#include <iconv.h>
char *sub_cp=NULL;
#endif
#ifdef CONFIG_FRIBIDI
#include <fribidi/fribidi.h>
char *fribidi_charset = NULL;   ///character set that will be passed to FriBiDi
int flip_hebrew = 1;            ///flip subtitles using fribidi
int fribidi_flip_commas = 0;    ///flip comma when fribidi is used
#endif

extern char* dvdsub_lang;

/* Maximal length of line of a subtitle */
#define LINE_LEN 1000
static float mpsub_position=0;
static float mpsub_multiplier=1.;
static int sub_slacktime = 20000; //20 sec

int sub_no_text_pp=0;   // 1 => do not apply text post-processing
                        // like {\...} elimination in SSA format.

int sub_match_fuzziness=0; // level of sub name matching fuzziness

/* Use the SUB_* constant defined in the header file */
int sub_format=SUB_INVALID;
#ifdef CONFIG_SORTSUB
/* 
   Some subtitling formats, namely AQT and Subrip09, define the end of a
   subtitle as the beginning of the following. Since currently we read one
   subtitle at time, for these format we keep two global *subtitle,
   previous_aqt_sub and previous_subrip09_sub, pointing to previous subtitle,
   so we can change its end when we read current subtitle starting time.
   When CONFIG_SORTSUB is defined, we use a single global unsigned long,
   previous_sub_end, for both (and even future) formats, to store the end of
   the previous sub: it is initialized to 0 in sub_read_file and eventually
   modified by sub_read_aqt_line or sub_read_subrip09_line.
 */
unsigned long previous_sub_end;
#endif

static int eol(char p) {
	return p=='\r' || p=='\n' || p=='\0';
}

/* Remove leading and trailing space */
static void trail_space(char *s) {
	int i = 0;
	while (isspace(s[i])) ++i;
	if (i) strcpy(s, s + i);
	i = strlen(s) - 1;
	while (i > 0 && isspace(s[i])) s[i--] = '\0';
}

static char *stristr(const char *haystack, const char *needle) {
    int len = 0;
    const char *p = haystack;

    if (!(haystack && needle)) return NULL;

    len=strlen(needle);
    while (*p != '\0') {
	if (strncasecmp(p, needle, len) == 0) return (char*)p;
	p++;
    }

    return NULL;
}

static subtitle *sub_read_line_sami(stream_t* st, subtitle *current) {
    static char line[LINE_LEN+1];
    static char *s = NULL, *slacktime_s;
    char text[LINE_LEN+1], *p=NULL, *q;
    int state;

    current->lines = current->start = current->end = 0;
    current->alignment = SUB_ALIGNMENT_BOTTOMCENTER;
    state = 0;

    /* read the first line */
    if (!s)
	    if (!(s = stream_read_line(st, line, LINE_LEN))) return 0;

    do {
	switch (state) {

	case 0: /* find "START=" or "Slacktime:" */
	    slacktime_s = stristr (s, "Slacktime:");
	    if (slacktime_s) 
                sub_slacktime = strtol (slacktime_s+10, NULL, 0) / 10;

	    s = stristr (s, "Start=");
	    if (s) {
		current->start = strtol (s + 6, &s, 0) / 10;
                /* eat '>' */
                for (; *s != '>' && *s != '\0'; s++);
                s++;
		state = 1; continue;
	    }
	    break;
 
	case 1: /* find (optionnal) "<P", skip other TAGs */
	    for  (; *s == ' ' || *s == '\t'; s++); /* strip blanks, if any */
	    if (*s == '\0') break;
	    if (*s != '<') { state = 3; p = text; continue; } /* not a TAG */
	    s++;
	    if (*s == 'P' || *s == 'p') { s++; state = 2; continue; } /* found '<P' */
	    for (; *s != '>' && *s != '\0'; s++); /* skip remains of non-<P> TAG */
	    if (s == '\0')
	      break;
	    s++;
	    continue;
 
	case 2: /* find ">" */
	    if ((s = strchr (s, '>'))) { s++; state = 3; p = text; continue; }
	    break;
 
	case 3: /* get all text until '<' appears */
	    if (*s == '\0') break;
	    else if (!strncasecmp (s, "<br>", 4)) {
		*p = '\0'; p = text; trail_space (text);
		if (text[0] != '\0')
		    current->text[current->lines++] = strdup (text);
		s += 4;
	    }
	    else if ((*s == '{') && !sub_no_text_pp) { state = 5; ++s; continue; }
	    else if (*s == '<') { state = 4; }
	    else if (!strncasecmp (s, "&nbsp;", 6)) { *p++ = ' '; s += 6; }
	    else if (*s == '\t') { *p++ = ' '; s++; }
	    else if (*s == '\r' || *s == '\n') { s++; }
	    else *p++ = *s++;

	    /* skip duplicated space */
	    if (p > text + 2) if (*(p-1) == ' ' && *(p-2) == ' ') p--;
	    
	    continue;

	case 4: /* get current->end or skip <TAG> */
	    q = stristr (s, "Start=");
	    if (q) {
		current->end = strtol (q + 6, &q, 0) / 10 - 1;
		*p = '\0'; trail_space (text);
		if (text[0] != '\0')
		    current->text[current->lines++] = strdup (text);
		if (current->lines > 0) { state = 99; break; }
		state = 0; continue;
	    }
	    s = strchr (s, '>');
	    if (s) { s++; state = 3; continue; }
	    break;
       case 5: /* get rid of {...} text, but read the alignment code */
	    if ((*s == '\\') && (*(s + 1) == 'a') && !sub_no_text_pp) {
               if (stristr(s, "\\a1") != NULL) {
                   current->alignment = SUB_ALIGNMENT_BOTTOMLEFT;
                   s = s + 3;
               }
               if (stristr(s, "\\a2") != NULL) {
                   current->alignment = SUB_ALIGNMENT_BOTTOMCENTER;
                   s = s + 3;
               } else if (stristr(s, "\\a3") != NULL) {
                   current->alignment = SUB_ALIGNMENT_BOTTOMRIGHT;
                   s = s + 3;
               } else if ((stristr(s, "\\a4") != NULL) || (stristr(s, "\\a5") != NULL) || (stristr(s, "\\a8") != NULL)) {
                   current->alignment = SUB_ALIGNMENT_TOPLEFT;
                   s = s + 3;
               } else if (stristr(s, "\\a6") != NULL) {
                   current->alignment = SUB_ALIGNMENT_TOPCENTER;
                   s = s + 3;
               } else if (stristr(s, "\\a7") != NULL) {
                   current->alignment = SUB_ALIGNMENT_TOPRIGHT;
                   s = s + 3;
               } else if (stristr(s, "\\a9") != NULL) {
                   current->alignment = SUB_ALIGNMENT_MIDDLELEFT;
                   s = s + 3;
               } else if (stristr(s, "\\a10") != NULL) {
                   current->alignment = SUB_ALIGNMENT_MIDDLECENTER;
                   s = s + 4;
               } else if (stristr(s, "\\a11") != NULL) {
                   current->alignment = SUB_ALIGNMENT_MIDDLERIGHT;
                   s = s + 4;
               }
	    }
	    if (*s == '}') state = 3;
	    ++s;
	    continue;
	}

	/* read next line */
	if (state != 99 && !(s = stream_read_line (st, line, LINE_LEN))) {
	    if (current->start > 0) {
		break; // if it is the last subtitle
	    } else {
		return 0;
	    }
	}
	    
    } while (state != 99);

    // For the last subtitle
    if (current->end <= 0) {
        current->end = current->start + sub_slacktime;
	*p = '\0'; trail_space (text);
	if (text[0] != '\0')
	    current->text[current->lines++] = strdup (text);
    }
    
    return current;
}


static char *sub_readtext(char *source, char **dest) {
    int len=0;
    char *p=source;
    
//    printf("src=%p  dest=%p  \n",source,dest);

    while ( !eol(*p) && *p!= '|' ) {
	p++,len++;
    }
    
    *dest= malloc (len+1);
    if (!dest) {return ERR;}
    
    strncpy(*dest, source, len);
    (*dest)[len]=0;
    
    while (*p=='\r' || *p=='\n' || *p=='|') p++;
    
    if (*p) return p;  // not-last text field
    else return NULL;  // last text field
}

static subtitle *sub_read_line_microdvd(stream_t *st,subtitle *current) {
    char line[LINE_LEN+1];
    char line2[LINE_LEN+1];
    char *p, *next;
    int i;

    do {
	if (!stream_read_line (st, line, LINE_LEN)) return NULL;
    } while ((sscanf (line,
		      "{%ld}{}%[^\r\n]",
		      &(current->start), line2) < 2) &&
	     (sscanf (line,
		      "{%ld}{%ld}%[^\r\n]",
		      &(current->start), &(current->end), line2) < 3));
	
    p=line2;

    next=p, i=0;
    while ((next =sub_readtext (next, &(current->text[i])))) {
        if (current->text[i]==ERR) {return ERR;}
	i++;
	if (i>=SUB_MAX_TEXT) { mp_msg(MSGT_SUBREADER,MSGL_WARN,"Too many lines in a subtitle\n");current->lines=i;return current;}
    }
    current->lines= ++i;

    return current;
}

static subtitle *sub_read_line_mpl2(stream_t *st,subtitle *current) {
    char line[LINE_LEN+1];
    char line2[LINE_LEN+1];
    char *p, *next;
    int i;

    do {
	if (!stream_read_line (st, line, LINE_LEN)) return NULL;
    } while ((sscanf (line,
		      "[%ld][%ld]%[^\r\n]",
		      &(current->start), &(current->end), line2) < 3));
    current->start *= 10;
    current->end *= 10;
    p=line2;

    next=p, i=0;
    while ((next =sub_readtext (next, &(current->text[i])))) {
        if (current->text[i]==ERR) {return ERR;}
	i++;
	if (i>=SUB_MAX_TEXT) { mp_msg(MSGT_SUBREADER,MSGL_WARN,"Too many lines in a subtitle\n");current->lines=i;return current;}
    }
    current->lines= ++i;

    return current;
}

static subtitle *sub_read_line_subrip(stream_t* st, subtitle *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL, *q=NULL;
    int len;
    
    while (1) {
	if (!stream_read_line (st, line, LINE_LEN)) return NULL;
	if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4) < 8) continue;
	current->start = a1*360000+a2*6000+a3*100+a4;
	current->end   = b1*360000+b2*6000+b3*100+b4;

	if (!stream_read_line (st, line, LINE_LEN)) return NULL;

	p=q=line;
	for (current->lines=1; current->lines < SUB_MAX_TEXT; current->lines++) {
	    for (q=p,len=0; *p && *p!='\r' && *p!='\n' && *p!='|' && strncmp(p,"[br]",4); p++,len++);
	    current->text[current->lines-1]=malloc (len+1);
	    if (!current->text[current->lines-1]) return ERR;
	    strncpy (current->text[current->lines-1], q, len);
	    current->text[current->lines-1][len]='\0';
	    if (!*p || *p=='\r' || *p=='\n') break;
	    if (*p=='|') p++;
	    else while (*p++!=']');
	}
	break;
    }
    return current;
}

static subtitle *sub_read_line_subviewer(stream_t *st,subtitle *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL;
    int i,len;
    
    while (!current->text[0]) {
	if (!stream_read_line (st, line, LINE_LEN)) return NULL;
	if ((len=sscanf (line, "%d:%d:%d%[,.:]%d --> %d:%d:%d%[,.:]%d",&a1,&a2,&a3,(char *)&i,&a4,&b1,&b2,&b3,(char *)&i,&b4)) < 10)
	    continue;
	current->start = a1*360000+a2*6000+a3*100+a4/10;
	current->end   = b1*360000+b2*6000+b3*100+b4/10;
	for (i=0; i<SUB_MAX_TEXT;) {
	    int blank = 1;
	    if (!stream_read_line (st, line, LINE_LEN)) break;
	    len=0;
	    for (p=line; *p!='\n' && *p!='\r' && *p; p++,len++)
		if (*p != ' ' && *p != '\t')
		    blank = 0;
	    if (len && !blank) {
                int j=0,skip=0;
		char *curptr=current->text[i]=malloc (len+1);
		if (!current->text[i]) return ERR;
		//strncpy (current->text[i], line, len); current->text[i][len]='\0';
                for(; j<len; j++) {
		    /* let's filter html tags ::atmos */
		    if(line[j]=='>') {
			skip=0;
			continue;
		    }
		    if(line[j]=='<') {
			skip=1;
			continue;
		    }
		    if(skip) {
			continue;
		    }
		    *curptr=line[j];
		    curptr++;
		}
		*curptr='\0';

		i++;
	    } else {
		break;
	    }
	}
	current->lines=i;
    }
    return current;
}

static subtitle *sub_read_line_subviewer2(stream_t *st,subtitle *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3,a4;
    char *p=NULL;
    int i,len;
   
    while (!current->text[0]) {
        if (!stream_read_line (st, line, LINE_LEN)) return NULL;
	if (line[0]!='{')
	    continue;
        if ((len=sscanf (line, "{T %d:%d:%d:%d",&a1,&a2,&a3,&a4)) < 4)
            continue;
        current->start = a1*360000+a2*6000+a3*100+a4/10;
        for (i=0; i<SUB_MAX_TEXT;) {
            if (!stream_read_line (st, line, LINE_LEN)) break;
            if (line[0]=='}') break;
            len=0;
            for (p=line; *p!='\n' && *p!='\r' && *p; ++p,++len);
            if (len) {
                current->text[i]=malloc (len+1);
                if (!current->text[i]) return ERR;
                strncpy (current->text[i], line, len); current->text[i][len]='\0';
                ++i;
            } else {
                break;
            }
        }
        current->lines=i;
    }
    return current;
}


static subtitle *sub_read_line_vplayer(stream_t *st,subtitle *current) {
	char line[LINE_LEN+1];
	int a1,a2,a3;
	char *p=NULL, *next,separator;
	int i,len,plen;

	while (!current->text[0]) {
		if (!stream_read_line (st, line, LINE_LEN)) return NULL;
		if ((len=sscanf (line, "%d:%d:%d%c%n",&a1,&a2,&a3,&separator,&plen)) < 4)
			continue;
		
		if (!(current->start = a1*360000+a2*6000+a3*100))
			continue;
                /* removed by wodzu
		p=line;	
 		// finds the body of the subtitle
 		for (i=0; i<3; i++){              
		   p=strchr(p,':');
		   if (p==NULL) break;
		   ++p;
		} 
		if (p==NULL) {
		    printf("SUB: Skipping incorrect subtitle line!\n");
		    continue;
		}
                */
                // by wodzu: hey! this time we know what length it has! what is
                // that magic for? it can't deal with space instead of third
                // colon! look, what simple it can be:
                p = &line[ plen ];

 		i=0;
		if (*p!='|') {
			//
			next = p,i=0;
			while ((next =sub_readtext (next, &(current->text[i])))) {
				if (current->text[i]==ERR) {return ERR;}
				i++;
				if (i>=SUB_MAX_TEXT) { mp_msg(MSGT_SUBREADER,MSGL_WARN,"Too many lines in a subtitle\n");current->lines=i;return current;}
			}
			current->lines=i+1;
		}
	}
	return current;
}

static subtitle *sub_read_line_rt(stream_t *st,subtitle *current) {
	//TODO: This format uses quite rich (sub/super)set of xhtml 
	// I couldn't check it since DTD is not included.
	// WARNING: full XML parses can be required for proper parsing 
    char line[LINE_LEN+1];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL,*next=NULL;
    int i,len,plen;
    
    while (!current->text[0]) {
	if (!stream_read_line (st, line, LINE_LEN)) return NULL;
	//TODO: it seems that format of time is not easily determined, it may be 1:12, 1:12.0 or 0:1:12.0
	//to describe the same moment in time. Maybe there are even more formats in use.
	//if ((len=sscanf (line, "<Time Begin=\"%d:%d:%d.%d\" End=\"%d:%d:%d.%d\"",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4)) < 8)
	plen=a1=a2=a3=a4=b1=b2=b3=b4=0;
	if (
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d.%d\" %*[Ee]nd=\"%d.%d\"%*[^<]<clear/>%n",&a3,&a4,&b3,&b4,&plen)) < 4) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d.%d\" %*[Ee]nd=\"%d:%d.%d\"%*[^<]<clear/>%n",&a3,&a4,&b2,&b3,&b4,&plen)) < 5) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d\" %*[Ee]nd=\"%d:%d\"%*[^<]<clear/>%n",&a2,&a3,&b2,&b3,&plen)) < 4) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d\" %*[Ee]nd=\"%d:%d.%d\"%*[^<]<clear/>%n",&a2,&a3,&b2,&b3,&b4,&plen)) < 5) &&
//	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d.%d\" %*[Ee]nd=\"%d:%d\"%*[^<]<clear/>%n",&a2,&a3,&a4,&b2,&b3,&plen)) < 5) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d.%d\" %*[Ee]nd=\"%d:%d.%d\"%*[^<]<clear/>%n",&a2,&a3,&a4,&b2,&b3,&b4,&plen)) < 6) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d:%d.%d\" %*[Ee]nd=\"%d:%d:%d.%d\"%*[^<]<clear/>%n",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4,&plen)) < 8) &&
	//now try it without end time
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d.%d\"%*[^<]<clear/>%n",&a3,&a4,&plen)) < 2) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d\"%*[^<]<clear/>%n",&a2,&a3,&plen)) < 2) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d.%d\"%*[^<]<clear/>%n",&a2,&a3,&a4,&plen)) < 3) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d:%d.%d\"%*[^<]<clear/>%n",&a1,&a2,&a3,&a4,&plen)) < 4) 
	)
	    continue;
	current->start = a1*360000+a2*6000+a3*100+a4/10;
	current->end   = b1*360000+b2*6000+b3*100+b4/10;
	if (b1 == 0 && b2 == 0 && b3 == 0 && b4 == 0)
	  current->end = current->start+200;
	p=line;	p+=plen;i=0;
	// TODO: I don't know what kind of convention is here for marking multiline subs, maybe <br/> like in xml?
	next = strstr(line,"<clear/>");
	if(next && strlen(next)>8){
	  next+=8;i=0;
	  while ((next =sub_readtext (next, &(current->text[i])))) {
		if (current->text[i]==ERR) {return ERR;}
		i++;
		if (i>=SUB_MAX_TEXT) { mp_msg(MSGT_SUBREADER,MSGL_WARN,"Too many lines in a subtitle\n");current->lines=i;return current;}
	  }
	}
			current->lines=i+1;
    }
    return current;
}

static subtitle *sub_read_line_ssa(stream_t *st,subtitle *current) {
/*
 * Sub Station Alpha v4 (and v2?) scripts have 9 commas before subtitle
 * other Sub Station Alpha scripts have only 8 commas before subtitle
 * Reading the "ScriptType:" field is not reliable since many scripts appear
 * w/o it
 *
 * http://www.scriptclub.org is a good place to find more examples
 * http://www.eswat.demon.co.uk is where the SSA specs can be found
 */
        int comma;
        static int max_comma = 32; /* let's use 32 for the case that the */
                    /*  amount of commas increase with newer SSA versions */

	int hour1, min1, sec1, hunsec1,
	    hour2, min2, sec2, hunsec2, nothing;
	int num;
	
	char line[LINE_LEN+1],
	     line3[LINE_LEN+1],
	     *line2;
	char *tmp;

	do {
		if (!stream_read_line (st, line, LINE_LEN)) return NULL;
	} while (sscanf (line, "Dialogue: Marked=%d,%d:%d:%d.%d,%d:%d:%d.%d,"
			"%[^\n\r]", &nothing,
			&hour1, &min1, &sec1, &hunsec1, 
			&hour2, &min2, &sec2, &hunsec2,
			line3) < 9
		 &&
		 sscanf (line, "Dialogue: %d,%d:%d:%d.%d,%d:%d:%d.%d,"
			 "%[^\n\r]", &nothing,
			 &hour1, &min1, &sec1, &hunsec1, 
			 &hour2, &min2, &sec2, &hunsec2,
			 line3) < 9	    );

        line2=strchr(line3, ',');

        for (comma = 4; comma < max_comma; comma ++)
          {
            tmp = line2;
            if(!(tmp=strchr(++tmp, ','))) break;
            if(*(++tmp) == ' ') break; 
                  /* a space after a comma means we're already in a sentence */
            line2 = tmp;
          }

        if(comma < max_comma)max_comma = comma;
	/* eliminate the trailing comma */
	if(*line2 == ',') line2++;

	current->lines=0;num=0;
	current->start = 360000*hour1 + 6000*min1 + 100*sec1 + hunsec1;
	current->end   = 360000*hour2 + 6000*min2 + 100*sec2 + hunsec2;
	
        while (((tmp=strstr(line2, "\\n")) != NULL) || ((tmp=strstr(line2, "\\N")) != NULL) ){
		current->text[num]=malloc(tmp-line2+1);
		strncpy (current->text[num], line2, tmp-line2);
		current->text[num][tmp-line2]='\0';
		line2=tmp+2;
		num++;
		current->lines++;
		if (current->lines >=  SUB_MAX_TEXT) return current;
	}

	current->text[num]=strdup(line2);
	current->lines++;

	return current;
}

static void sub_pp_ssa(subtitle *sub) {
	int l=sub->lines;
	char *so,*de,*start;

	while (l){
            	/* eliminate any text enclosed with {}, they are font and color settings */
            	so=de=sub->text[--l];
            	while (*so) {
            		if(*so == '{' && so[1]=='\\') {
            			for (start=so; *so && *so!='}'; so++);
            			if(*so) so++; else so=start;
            		}
            		if(*so) {
            			*de=*so;
            			so++; de++;
            		}
            	}
            	*de=*so;
        }
}

/*
 * PJS subtitles reader.
 * That's the "Phoenix Japanimation Society" format.
 * I found some of them in http://www.scriptsclub.org/ (used for anime).
 * The time is in tenths of second.
 *
 * by set, based on code by szabi (dunnowhat sub format ;-)
 */
static subtitle *sub_read_line_pjs(stream_t *st,subtitle *current) {
    char line[LINE_LEN+1];
    char text[LINE_LEN+1], *s, *d;

    if (!stream_read_line (st, line, LINE_LEN))
	return NULL;
    /* skip spaces */
    for (s=line; *s && isspace(*s); s++);
    /* allow empty lines at the end of the file */
    if (*s==0)
	return NULL;
    /* get the time */
    if (sscanf (s, "%ld,%ld,", &(current->start),
		&(current->end)) <2) {
	return ERR;
    }
    /* the files I have are in tenths of second */
    current->start *= 10;
    current->end *= 10;
    /* walk to the beggining of the string */
    for (; *s; s++) if (*s==',') break;
    if (*s) {
	for (s++; *s; s++) if (*s==',') break;
	if (*s) s++;
    }
    if (*s!='"') {
	return ERR;
    }
    /* copy the string to the text buffer */
    for (s++, d=text; *s && *s!='"'; s++, d++)
	*d=*s;
    *d=0;
    current->text[0] = strdup(text);
    current->lines = 1;

    return current;
}

static subtitle *sub_read_line_mpsub(stream_t *st, subtitle *current) {
	char line[LINE_LEN+1];
	float a,b;
	int num=0;
	char *p, *q;

	do
	{
		if (!stream_read_line(st, line, LINE_LEN)) return NULL;
	} while (sscanf (line, "%f %f", &a, &b) !=2);

	mpsub_position += a*mpsub_multiplier; 
	current->start=(int) mpsub_position;
	mpsub_position += b*mpsub_multiplier; 
	current->end=(int) mpsub_position;

	while (num < SUB_MAX_TEXT) {
		if (!stream_read_line (st, line, LINE_LEN)) {
			if (num == 0) return NULL;
			else return current;
		}
		p=line;
		while (isspace(*p)) p++;
		if (eol(*p) && num > 0) return current;
		if (eol(*p)) return NULL;

		for (q=p; !eol(*q); q++);
		*q='\0';
		if (strlen(p)) {
			current->text[num]=strdup(p);
//			printf (">%s<\n",p);
			current->lines = ++num;
		} else {
			if (num) return current;
			else return NULL;
		}
	}
	return NULL; // we should have returned before if it's OK
}

#ifndef CONFIG_SORTSUB
//we don't need this if we use previous_sub_end
subtitle *previous_aqt_sub = NULL;
#endif

static subtitle *sub_read_line_aqt(stream_t *st,subtitle *current) {
    char line[LINE_LEN+1];
    char *next;
    int i;

    while (1) {
    // try to locate next subtitle
        if (!stream_read_line (st, line, LINE_LEN))
		return NULL;
        if (!(sscanf (line, "-->> %ld", &(current->start)) <1))
		break;
    }
    
#ifdef CONFIG_SORTSUB
    previous_sub_end = (current->start) ? current->start - 1 : 0; 
#else
    if (previous_aqt_sub != NULL) 
	previous_aqt_sub->end = current->start-1;
    
    previous_aqt_sub = current;
#endif

    if (!stream_read_line (st, line, LINE_LEN))
	return NULL;

    sub_readtext((char *) &line,&current->text[0]);
    current->lines = 1;
    current->end = current->start; // will be corrected by next subtitle

    if (!stream_read_line (st, line, LINE_LEN))
	return current;

    next = line,i=1;
    while ((next =sub_readtext (next, &(current->text[i])))) {
	if (current->text[i]==ERR) {return ERR;}
	i++;
	if (i>=SUB_MAX_TEXT) { mp_msg(MSGT_SUBREADER,MSGL_WARN,"Too many lines in a subtitle\n");current->lines=i;return current;}
	}
    current->lines=i+1;

    if (!strlen(current->text[0]) && !strlen(current->text[1])) {
#ifdef CONFIG_SORTSUB
	previous_sub_end = 0;
#else
	// void subtitle -> end of previous marked and exit
	previous_aqt_sub = NULL;
#endif	
	return NULL;
	}

    return current;
}

#ifndef CONFIG_SORTSUB
subtitle *previous_subrip09_sub = NULL;
#endif

static subtitle *sub_read_line_subrip09(stream_t *st,subtitle *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3;
    char * next=NULL;
    int i,len;
   
    while (1) {
    // try to locate next subtitle
        if (!stream_read_line (st, line, LINE_LEN))
		return NULL;
        if (!((len=sscanf (line, "[%d:%d:%d]",&a1,&a2,&a3)) < 3))
		break;
    }

    current->start = a1*360000+a2*6000+a3*100;
    
#ifdef CONFIG_SORTSUB
    previous_sub_end = (current->start) ? current->start - 1 : 0; 
#else
    if (previous_subrip09_sub != NULL) 
	previous_subrip09_sub->end = current->start-1;
    
    previous_subrip09_sub = current;
#endif

    if (!stream_read_line (st, line, LINE_LEN))
	return NULL;

    next = line,i=0;
    
    current->text[0]=""; // just to be sure that string is clear
    
    while ((next =sub_readtext (next, &(current->text[i])))) {
	if (current->text[i]==ERR) {return ERR;}
	i++;
	if (i>=SUB_MAX_TEXT) { mp_msg(MSGT_SUBREADER,MSGL_WARN,"Too many lines in a subtitle\n");current->lines=i;return current;}
	}
    current->lines=i+1;

    if (!strlen(current->text[0]) && (i==0)) {
#ifdef CONFIG_SORTSUB
	previous_sub_end = 0;
#else
	// void subtitle -> end of previous marked and exit
	previous_subrip09_sub = NULL;
#endif	
	return NULL;
	}

    return current;
}

static subtitle *sub_read_line_jacosub(stream_t* st, subtitle * current)
{
    char line1[LINE_LEN], line2[LINE_LEN], directive[LINE_LEN], *p, *q;
    unsigned a1, a2, a3, a4, b1, b2, b3, b4, comment = 0;
    static unsigned jacoTimeres = 30;
    static int jacoShift = 0;

    memset(current, 0, sizeof(subtitle));
    memset(line1, 0, LINE_LEN);
    memset(line2, 0, LINE_LEN);
    memset(directive, 0, LINE_LEN);
    while (!current->text[0]) {
	if (!stream_read_line(st, line1, LINE_LEN)) {
	    return NULL;
	}
	if (sscanf
	    (line1, "%u:%u:%u.%u %u:%u:%u.%u %[^\n\r]", &a1, &a2, &a3, &a4,
	     &b1, &b2, &b3, &b4, line2) < 9) {
	    if (sscanf(line1, "@%u @%u %[^\n\r]", &a4, &b4, line2) < 3) {
		if (line1[0] == '#') {
		    int hours = 0, minutes = 0, seconds, delta, inverter =
			1;
		    unsigned units = jacoShift;
		    switch (toupper(line1[1])) {
		    case 'S':
			if (isalpha(line1[2])) {
			    delta = 6;
			} else {
			    delta = 2;
			}
			if (sscanf(&line1[delta], "%d", &hours)) {
			    if (hours < 0) {
				hours *= -1;
				inverter = -1;
			    }
			    if (sscanf(&line1[delta], "%*d:%d", &minutes)) {
				if (sscanf
				    (&line1[delta], "%*d:%*d:%d",
				     &seconds)) {
				    sscanf(&line1[delta], "%*d:%*d:%*d.%d",
					   &units);
				} else {
				    hours = 0;
				    sscanf(&line1[delta], "%d:%d.%d",
					   &minutes, &seconds, &units);
				    minutes *= inverter;
				}
			    } else {
				hours = minutes = 0;
				sscanf(&line1[delta], "%d.%d", &seconds,
				       &units);
				seconds *= inverter;
			    }
			    jacoShift =
				((hours * 3600 + minutes * 60 +
				  seconds) * jacoTimeres +
				 units) * inverter;
			}
			break;
		    case 'T':
			if (isalpha(line1[2])) {
			    delta = 8;
			} else {
			    delta = 2;
			}
			sscanf(&line1[delta], "%u", &jacoTimeres);
			break;
		    }
		}
		continue;
	    } else {
		current->start =
		    (unsigned long) ((a4 + jacoShift) * 100.0 /
				     jacoTimeres);
		current->end =
		    (unsigned long) ((b4 + jacoShift) * 100.0 /
				     jacoTimeres);
	    }
	} else {
	    current->start =
		(unsigned
		 long) (((a1 * 3600 + a2 * 60 + a3) * jacoTimeres + a4 +
			 jacoShift) * 100.0 / jacoTimeres);
	    current->end =
		(unsigned
		 long) (((b1 * 3600 + b2 * 60 + b3) * jacoTimeres + b4 +
			 jacoShift) * 100.0 / jacoTimeres);
	}
	current->lines = 0;
	p = line2;
	while ((*p == ' ') || (*p == '\t')) {
	    ++p;
	}
	if (isalpha(*p)||*p == '[') {
	    int cont, jLength;

	    if (sscanf(p, "%s %[^\n\r]", directive, line1) < 2)
		return (subtitle *) ERR;
	    jLength = strlen(directive);
	    for (cont = 0; cont < jLength; ++cont) {
		if (isalpha(*(directive + cont)))
		    *(directive + cont) = toupper(*(directive + cont));
	    }
	    if ((strstr(directive, "RDB") != NULL)
		|| (strstr(directive, "RDC") != NULL)
		|| (strstr(directive, "RLB") != NULL)
		|| (strstr(directive, "RLG") != NULL)) {
		continue;
	    }
	    if (strstr(directive, "JL") != NULL) {
		current->alignment = SUB_ALIGNMENT_BOTTOMLEFT;
	    } else if (strstr(directive, "JR") != NULL) {
		current->alignment = SUB_ALIGNMENT_BOTTOMRIGHT;
	    } else {
		current->alignment = SUB_ALIGNMENT_BOTTOMCENTER;
	    }
	    strcpy(line2, line1);
	    p = line2;
	}
	for (q = line1; (!eol(*p)) && (current->lines < SUB_MAX_TEXT); ++p) {
	    switch (*p) {
	    case '{':
		comment++;
		break;
	    case '}':
		if (comment) {
		    --comment;
		    //the next line to get rid of a blank after the comment
		    if ((*(p + 1)) == ' ')
			p++;
		}
		break;
	    case '~':
		if (!comment) {
		    *q = ' ';
		    ++q;
		}
		break;
	    case ' ':
	    case '\t':
		if ((*(p + 1) == ' ') || (*(p + 1) == '\t'))
		    break;
		if (!comment) {
		    *q = ' ';
		    ++q;
		}
		break;
	    case '\\':
		if (*(p + 1) == 'n') {
		    *q = '\0';
		    q = line1;
		    current->text[current->lines++] = strdup(line1);
		    ++p;
		    break;
		}
		if ((toupper(*(p + 1)) == 'C')
		    || (toupper(*(p + 1)) == 'F')) {
		    ++p,++p;
		    break;
		}
		if ((*(p + 1) == 'B') || (*(p + 1) == 'b') || (*(p + 1) == 'D') ||	//actually this means "insert current date here"
		    (*(p + 1) == 'I') || (*(p + 1) == 'i') || (*(p + 1) == 'N') || (*(p + 1) == 'T') ||	//actually this means "insert current time here"
		    (*(p + 1) == 'U') || (*(p + 1) == 'u')) {
		    ++p;
		    break;
		}
		if ((*(p + 1) == '\\') ||
		    (*(p + 1) == '~') || (*(p + 1) == '{')) {
		    ++p;
		} else if (eol(*(p + 1))) {
		    if (!stream_read_line(st, directive, LINE_LEN))
			return NULL;
		    trail_space(directive);
		    strncat(line2, directive,
			    (LINE_LEN > 511) ? LINE_LEN : 511);
		    break;
		}
	    default:
		if (!comment) {
		    *q = *p;
		    ++q;
		}
	    }			//-- switch
	}			//-- for
	*q = '\0';
	current->text[current->lines] = strdup(line1);
    }				//-- while
    current->lines++;
    return current;
}

static int sub_autodetect (stream_t* st, int *uses_time) {
    char line[LINE_LEN+1];
    int i,j=0;
    char p;
    
    while (j < 100) {
	j++;
	if (!stream_read_line (st, line, LINE_LEN))
	    return SUB_INVALID;

	if (sscanf (line, "{%d}{%d}", &i, &i)==2)
		{*uses_time=0;return SUB_MICRODVD;}
	if (sscanf (line, "{%d}{}", &i)==1)
		{*uses_time=0;return SUB_MICRODVD;}
	if (sscanf (line, "[%d][%d]", &i, &i)==2)
		{*uses_time=1;return SUB_MPL2;}
	if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",     &i, &i, &i, &i, &i, &i, &i, &i)==8)
		{*uses_time=1;return SUB_SUBRIP;}
	if (sscanf (line, "%d:%d:%d%[,.:]%d --> %d:%d:%d%[,.:]%d", &i, &i, &i, (char *)&i, &i, &i, &i, &i, (char *)&i, &i)==10)
		{*uses_time=1;return SUB_SUBVIEWER;}
	if (sscanf (line, "{T %d:%d:%d:%d",&i, &i, &i, &i)==4)
		{*uses_time=1;return SUB_SUBVIEWER2;}
	if (strstr (line, "<SAMI>"))
		{*uses_time=1; return SUB_SAMI;}
	if (sscanf(line, "%d:%d:%d.%d %d:%d:%d.%d", &i, &i, &i, &i, &i, &i, &i, &i) == 8)
		{*uses_time = 1; return SUB_JACOSUB;}
	if (sscanf(line, "@%d @%d", &i, &i) == 2)
		{*uses_time = 1; return SUB_JACOSUB;}
	if (sscanf (line, "%d:%d:%d:",     &i, &i, &i )==3)
		{*uses_time=1;return SUB_VPLAYER;}
	if (sscanf (line, "%d:%d:%d ",     &i, &i, &i )==3)
		{*uses_time=1;return SUB_VPLAYER;}
	//TODO: just checking if first line of sub starts with "<" is WAY
	// too weak test for RT
	// Please someone who knows the format of RT... FIX IT!!!
	// It may conflict with other sub formats in the future (actually it doesn't)
	if ( *line == '<' )
		{*uses_time=1;return SUB_RT;}

	if (!memcmp(line, "Dialogue: Marked", 16))
		{*uses_time=1; return SUB_SSA;}
	if (!memcmp(line, "Dialogue: ", 10))
		{*uses_time=1; return SUB_SSA;}
	if (sscanf (line, "%d,%d,\"%c", &i, &i, (char *) &i) == 3)
		{*uses_time=1;return SUB_PJS;}
	if (sscanf (line, "FORMAT=%d", &i) == 1)
		{*uses_time=0; return SUB_MPSUB;}
	if (sscanf (line, "FORMAT=TIM%c", &p)==1 && p=='E')
		{*uses_time=1; return SUB_MPSUB;}
	if (strstr (line, "-->>"))
		{*uses_time=0; return SUB_AQTITLE;}
	if (sscanf (line, "[%d:%d:%d]", &i, &i, &i)==3)
		{*uses_time=1;return SUB_SUBRIP09;}
    }

    return SUB_INVALID;  // too many bad lines
}

extern int sub_utf8;
int sub_utf8_prev=0;

extern float sub_delay;
extern float sub_fps;

#ifdef CONFIG_ICONV
static iconv_t icdsc = (iconv_t)(-1);

void	subcp_open (stream_t *st)
{
	char *tocp = "UTF-8";

	if (sub_cp){
		const char *cp_tmp = sub_cp;
#ifdef CONFIG_ENCA
		char enca_lang[3], enca_fallback[100];
		if (sscanf(sub_cp, "enca:%2s:%99s", enca_lang, enca_fallback) == 2
		     || sscanf(sub_cp, "ENCA:%2s:%99s", enca_lang, enca_fallback) == 2) {
		  if (st && st->flags & STREAM_SEEK ) {
		    cp_tmp = guess_cp(st, enca_lang, enca_fallback);
		  } else {
		    cp_tmp = enca_fallback;
		    if (st)
		      mp_msg(MSGT_SUBREADER,MSGL_WARN,"SUB: enca failed, stream must be seekable.\n"); 
		  }
		}
#endif
		if ((icdsc = iconv_open (tocp, cp_tmp)) != (iconv_t)(-1)){
			mp_msg(MSGT_SUBREADER,MSGL_V,"SUB: opened iconv descriptor.\n");
			sub_utf8 = 2;
		} else
			mp_msg(MSGT_SUBREADER,MSGL_ERR,"SUB: error opening iconv descriptor.\n");
	}
}

void	subcp_close (void)
{
	if (icdsc != (iconv_t)(-1)){
		(void) iconv_close (icdsc);
		icdsc = (iconv_t)(-1);
	   	mp_msg(MSGT_SUBREADER,MSGL_V,"SUB: closed iconv descriptor.\n");
	}
}

subtitle* subcp_recode (subtitle *sub)
{
	int l=sub->lines;
	size_t ileft, oleft;
	char *op, *ip, *ot;
	if(icdsc == (iconv_t)(-1)) return sub;

	while (l){
		ip = sub->text[--l];
		ileft = strlen(ip);
		oleft = 4 * ileft;

		if (!(ot = malloc(oleft + 1))){
			mp_msg(MSGT_SUBREADER,MSGL_WARN,"SUB: error allocating mem.\n");
		   	continue;
		}
		op = ot;
		if (iconv(icdsc, &ip, &ileft,
			  &op, &oleft) == (size_t)(-1)) {
			mp_msg(MSGT_SUBREADER,MSGL_WARN,"SUB: error recoding line.\n");
			free(ot);
			continue;
		}
		// In some stateful encodings, we must clear the state to handle the last character
		if (iconv(icdsc, NULL, NULL,
			  &op, &oleft) == (size_t)(-1)) {
			mp_msg(MSGT_SUBREADER,MSGL_WARN,"SUB: error recoding line, can't clear encoding state.\n");
		}
		*op='\0' ;
		free (sub->text[l]);
		sub->text[l] = ot;
	}
	return sub;
}
#endif

#ifdef CONFIG_FRIBIDI
#ifndef max
#define max(a,b)  (((a)>(b))?(a):(b))
#endif
subtitle* sub_fribidi (subtitle *sub, int sub_utf8)
{
  FriBidiChar logical[LINE_LEN+1], visual[LINE_LEN+1]; // Hopefully these two won't smash the stack
  char        *ip      = NULL, *op     = NULL;
  FriBidiCharType base;
  size_t len,orig_len;
  int l=sub->lines;
  int char_set_num;
  fribidi_boolean log2vis;
  if(flip_hebrew) { // Please fix the indentation someday
  fribidi_set_mirroring(1);
  fribidi_set_reorder_nsm(0);
   
  if( sub_utf8 == 0 ) {
    char_set_num = fribidi_parse_charset (fribidi_charset?fribidi_charset:"ISO8859-8");
  }else {
    char_set_num = fribidi_parse_charset ("UTF-8");
  }
  while (l) {
    ip = sub->text[--l];
    orig_len = len = strlen( ip ); // We assume that we don't use full unicode, only UTF-8 or ISO8859-x
    if(len > LINE_LEN) {
      mp_msg(MSGT_SUBREADER,MSGL_WARN,"SUB: sub->text is longer than LINE_LEN.\n");
      l++;
      break;
    }
    len = fribidi_charset_to_unicode (char_set_num, ip, len, logical);
    base = fribidi_flip_commas?FRIBIDI_TYPE_ON:FRIBIDI_TYPE_L;
    log2vis = fribidi_log2vis (logical, len, &base,
			       /* output */
			       visual, NULL, NULL, NULL);
    if(log2vis) {
      len = fribidi_remove_bidi_marks (visual, len, NULL, NULL,
				       NULL);
      if((op = malloc((max(2*orig_len,2*len) + 1))) == NULL) {
	mp_msg(MSGT_SUBREADER,MSGL_WARN,"SUB: error allocating mem.\n");
	l++;
	break;	
      }
      fribidi_unicode_to_charset ( char_set_num, visual, len,op);
      free (ip);
      sub->text[l] = op;
    }
  }
  if (l){
    for (l = sub->lines; l;)
      free (sub->text[--l]);
    return ERR;
  }
  }
  return sub;
}

#endif

static void adjust_subs_time(subtitle* sub, float subtime, float fps, int block,
                             int sub_num, int sub_uses_time) {
	int n,m;
	subtitle* nextsub;
	int i = sub_num;
	unsigned long subfms = (sub_uses_time ? 100 : fps) * subtime;
	unsigned long overlap = (sub_uses_time ? 100 : fps) / 5; // 0.2s
	
	n=m=0;
	if (i)	for (;;){
		if (sub->end <= sub->start){
			sub->end = sub->start + subfms;
			m++;
			n++;
		}
		if (!--i) break;
		nextsub = sub + 1;
	    if(block){
		if ((sub->end > nextsub->start) && (sub->end <= nextsub->start + overlap)) {
		    // these subtitles overlap for less than 0.2 seconds
		    // and would result in very short overlapping subtitle
		    // so let's fix the problem here, before overlapping code
		    // get its hands on them
		    unsigned delta = sub->end - nextsub->start, half = delta / 2;
		    sub->end -= half + 1;
		    nextsub->start += delta - half;
		}
		if (sub->end >= nextsub->start){
			sub->end = nextsub->start - 1;
			if (sub->end - sub->start > subfms)
				sub->end = sub->start + subfms;
			if (!m)
				n++;
		}
	    }

		/* Theory:
		 * Movies are often converted from FILM (24 fps)
		 * to PAL (25) by simply speeding it up, so we
		 * to multiply the original timestmaps by
		 * (Movie's FPS / Subtitle's (guessed) FPS)
		 * so eg. for 23.98 fps movie and PAL time based
		 * subtitles we say -subfps 25 and we're fine!
		 */

		/* timed sub fps correction ::atmos */
		/* the frame-based case is handled in mpcommon.c
		 * where find_sub is called */
		if(sub_uses_time && sub_fps) {	
			sub->start *= sub_fps/fps;
			sub->end   *= sub_fps/fps;
		}

		sub = nextsub;
		m = 0;
	}
	if (n) mp_msg(MSGT_SUBREADER,MSGL_INFO,"SUB: Adjusted %d subtitle(s).\n", n);
}

struct subreader {
    subtitle * (*read)(stream_t *st,subtitle *dest);
    void       (*post)(subtitle *dest);
    const char *name;
};

#ifdef CONFIG_ENCA
const char* guess_buffer_cp(unsigned char* buffer, int buflen, const char *preferred_language, const char *fallback)
{
    const char **languages;
    size_t langcnt;
    EncaAnalyser analyser;
    EncaEncoding encoding;
    const char *detected_sub_cp = NULL;
    int i;

    languages = enca_get_languages(&langcnt);
    mp_msg(MSGT_SUBREADER, MSGL_V, "ENCA supported languages: ");
    for (i = 0; i < langcnt; i++) {
	mp_msg(MSGT_SUBREADER, MSGL_V, "%s ", languages[i]);
    }
    mp_msg(MSGT_SUBREADER, MSGL_V, "\n");
    
    for (i = 0; i < langcnt; i++) {
	if (strcasecmp(languages[i], preferred_language) != 0) continue;
	analyser = enca_analyser_alloc(languages[i]);
	encoding = enca_analyse_const(analyser, buffer, buflen);
	enca_analyser_free(analyser);
	if (encoding.charset != ENCA_CS_UNKNOWN) {
	    detected_sub_cp = enca_charset_name(encoding.charset, ENCA_NAME_STYLE_ICONV);
	    break;
	}
    }
    
    free(languages);

    if (!detected_sub_cp) {
	detected_sub_cp = fallback;
	mp_msg(MSGT_SUBREADER, MSGL_INFO, "ENCA detection failed: fallback to %s\n", fallback);
    }else{
	mp_msg(MSGT_SUBREADER, MSGL_INFO, "ENCA detected charset: %s\n", detected_sub_cp);
    }

    return detected_sub_cp;
}

#define MAX_GUESS_BUFFER_SIZE (256*1024)
const char* guess_cp(stream_t *st, const char *preferred_language, const char *fallback)
{
    size_t buflen;
    unsigned char *buffer;
    const char *detected_sub_cp = NULL;

    buffer = malloc(MAX_GUESS_BUFFER_SIZE);
    buflen = stream_read(st,buffer, MAX_GUESS_BUFFER_SIZE);

    detected_sub_cp = guess_buffer_cp(buffer, buflen, preferred_language, fallback);
    
    free(buffer);
    stream_reset(st);
    stream_seek(st,0);

    return detected_sub_cp;
}
#undef MAX_GUESS_BUFFER_SIZE
#endif

sub_data* sub_read_file (char *filename, float fps) {
    stream_t* fd;
    int n_max, n_first, i, j, sub_first, sub_orig;
    subtitle *first, *second, *sub, *return_sub;
    sub_data *subt_data;
    int uses_time = 0, sub_num = 0, sub_errs = 0;
    struct subreader sr[]=
    {
	    { sub_read_line_microdvd, NULL, "microdvd" },
	    { sub_read_line_subrip, NULL, "subrip" },
	    { sub_read_line_subviewer, NULL, "subviewer" },
	    { sub_read_line_sami, NULL, "sami" },
	    { sub_read_line_vplayer, NULL, "vplayer" },
	    { sub_read_line_rt, NULL, "rt" },
	    { sub_read_line_ssa, sub_pp_ssa, "ssa" },
	    { sub_read_line_pjs, NULL, "pjs" },
	    { sub_read_line_mpsub, NULL, "mpsub" },
	    { sub_read_line_aqt, NULL, "aqt" },
	    { sub_read_line_subviewer2, NULL, "subviewer 2.0" },
	    { sub_read_line_subrip09, NULL, "subrip 0.9" },
	    { sub_read_line_jacosub, NULL, "jacosub" },
	    { sub_read_line_mpl2, NULL, "mpl2" }
    };
    struct subreader *srp;
    
    if(filename==NULL) return NULL; //qnx segfault
    i = 0;
    fd=open_stream (filename, NULL, &i); if (!fd) return NULL;
    
    sub_format=sub_autodetect (fd, &uses_time);
    mpsub_multiplier = (uses_time ? 100.0 : 1.0);
    if (sub_format==SUB_INVALID) {mp_msg(MSGT_SUBREADER,MSGL_WARN,"SUB: Could not determine file format\n");return NULL;}
    srp=sr+sub_format;
    mp_msg(MSGT_SUBREADER,MSGL_INFO,"SUB: Detected subtitle file format: %s\n", srp->name);
    
    stream_reset(fd);
    stream_seek(fd,0);

#ifdef CONFIG_ICONV
    sub_utf8_prev=sub_utf8;
    {
	    int l,k;
	    k = -1;
	    if ((l=strlen(filename))>4){
		    char *exts[] = {".utf", ".utf8", ".utf-8" };
		    for (k=3;--k>=0;)
			if (l >= strlen(exts[k]) && !strcasecmp(filename+(l - strlen(exts[k])), exts[k])){
			    sub_utf8 = 1;
			    break;
			}
	    }
	    if (k<0) subcp_open(fd);
    }
#endif

    sub_num=0;n_max=32;
    first=malloc(n_max*sizeof(subtitle));
    if(!first){
#ifdef CONFIG_ICONV
	  subcp_close();
          sub_utf8=sub_utf8_prev;
#endif
	    return NULL;
    }
    
#ifdef CONFIG_SORTSUB
    sub = malloc(sizeof(subtitle));
    //This is to deal with those formats (AQT & Subrip) which define the end of a subtitle
    //as the beginning of the following
    previous_sub_end = 0;
#endif    
    while(1){
        if(sub_num>=n_max){
            n_max+=16;
            first=realloc(first,n_max*sizeof(subtitle));
        }
#ifndef CONFIG_SORTSUB
	sub = &first[sub_num];
#endif	
	memset(sub, '\0', sizeof(subtitle));
        sub=srp->read(fd,sub);
        if(!sub) break;   // EOF
#ifdef CONFIG_ICONV
	if ((sub!=ERR) && (sub_utf8 & 2)) sub=subcp_recode(sub);
#endif
#ifdef CONFIG_FRIBIDI
	if (sub!=ERR) sub=sub_fribidi(sub,sub_utf8);
#endif
	if ( sub == ERR )
	 {
#ifdef CONFIG_ICONV
          subcp_close();
#endif
    	  if ( first ) free(first);
	  return NULL; 
	 }
        // Apply any post processing that needs recoding first
        if ((sub!=ERR) && !sub_no_text_pp && srp->post) srp->post(sub);
#ifdef CONFIG_SORTSUB
	if(!sub_num || (first[sub_num - 1].start <= sub->start)){
	    first[sub_num].start = sub->start;
  	    first[sub_num].end   = sub->end;
	    first[sub_num].lines = sub->lines;
	    first[sub_num].alignment = sub->alignment;
  	    for(i = 0; i < sub->lines; ++i){
		first[sub_num].text[i] = sub->text[i];
  	    }
	    if (previous_sub_end){
  		first[sub_num - 1].end = previous_sub_end;
    		previous_sub_end = 0;
	    }
	} else {
	    for(j = sub_num - 1; j >= 0; --j){
    		first[j + 1].start = first[j].start;
    		first[j + 1].end   = first[j].end;
		first[j + 1].lines = first[j].lines;
		first[j + 1].alignment = first[j].alignment;
    		for(i = 0; i < first[j].lines; ++i){
      		    first[j + 1].text[i] = first[j].text[i];
		}
		if(!j || (first[j - 1].start <= sub->start)){
	    	    first[j].start = sub->start;
	    	    first[j].end   = sub->end;
	    	    first[j].lines = sub->lines;
	    	    first[j].alignment = sub->alignment;
	    	    for(i = 0; i < SUB_MAX_TEXT; ++i){
			first[j].text[i] = sub->text[i];
		    }
		    if (previous_sub_end){
			first[j].end = first[j - 1].end;
			first[j - 1].end = previous_sub_end;
			previous_sub_end = 0;
		    }
		    break;
    		}
	    }
	}
#endif	
        if(sub==ERR) ++sub_errs; else ++sub_num; // Error vs. Valid
    }
    
    free_stream(fd);

#ifdef CONFIG_ICONV
    subcp_close();
#endif

//    printf ("SUB: Subtitle format %s time.\n", uses_time?"uses":"doesn't use");
    mp_msg(MSGT_SUBREADER,MSGL_INFO,"SUB: Read %i subtitles", sub_num);
    if (sub_errs) mp_msg(MSGT_SUBREADER,MSGL_INFO,", %i bad line(s).\n", sub_errs);
    else 	  mp_msg(MSGT_SUBREADER,MSGL_INFO,".\n");

    if(sub_num<=0){
	free(first);
	return NULL;
    }

    // we do overlap if the user forced it (suboverlap_enable == 2) or
    // the user didn't forced no-overlapsub and the format is Jacosub or Ssa.
    // this is because usually overlapping subtitles are found in these formats,
    // while in others they are probably result of bad timing
if ((suboverlap_enabled == 2) ||
    ((suboverlap_enabled) && ((sub_format == SUB_JACOSUB) || (sub_format == SUB_SSA)))) {
    adjust_subs_time(first, 6.0, fps, 0, sub_num, uses_time);/*~6 secs AST*/
// here we manage overlapping subtitles
    sub_orig = sub_num;
    n_first = sub_num;
    sub_num = 0;
    second = NULL;
    // for each subtitle in first[] we deal with its 'block' of
    // bonded subtitles
    for (sub_first = 0; sub_first < n_first; ++sub_first) {
	unsigned long global_start = first[sub_first].start,
		global_end = first[sub_first].end, local_start, local_end;
	int lines_to_add = first[sub_first].lines, sub_to_add = 0,
		**placeholder = NULL, higher_line = 0, counter, start_block_sub = sub_num;
	char real_block = 1;

	// here we find the number of subtitles inside the 'block'
	// and its span interval. this works well only with sorted
	// subtitles
	while ((sub_first + sub_to_add + 1 < n_first) && (first[sub_first + sub_to_add + 1].start < global_end)) {
	    ++sub_to_add;
	    lines_to_add += first[sub_first + sub_to_add].lines;
	    if (first[sub_first + sub_to_add].start < global_start) {
		global_start = first[sub_first + sub_to_add].start;
	    }
	    if (first[sub_first + sub_to_add].end > global_end) {
		global_end = first[sub_first + sub_to_add].end;
	    }
	}

	// we need a structure to keep trace of the screen lines
	// used by the subs, a 'placeholder'
	counter = 2 * sub_to_add + 1;  // the maximum number of subs derived
	                               // from a block of sub_to_add+1 subs
	placeholder = malloc(sizeof(int *) * counter);
	for (i = 0; i < counter; ++i) {
	    placeholder[i] = malloc(sizeof(int) * lines_to_add);
	    for (j = 0; j < lines_to_add; ++j) {
		placeholder[i][j] = -1;
	    }
	}

	counter = 0;
	local_end = global_start - 1;
	do {
	    int ls;

	    // here we find the beginning and the end of a new
	    // subtitle in the block
	    local_start = local_end + 1;
	    local_end   = global_end;
	    for (j = 0; j <= sub_to_add; ++j) {
		if ((first[sub_first + j].start - 1 > local_start) && (first[sub_first + j].start - 1 < local_end)) {
		    local_end = first[sub_first + j].start - 1;
		} else if ((first[sub_first + j].end > local_start) && (first[sub_first + j].end < local_end)) {
		    local_end = first[sub_first + j].end;
		}
	    }
            // here we allocate the screen lines to subs we must
	    // display in current local_start-local_end interval.
	    // if the subs were yet presents in the previous interval
	    // they keep the same lines, otherside they get unused lines
	    for (j = 0; j <= sub_to_add; ++j) {
		if ((first[sub_first + j].start <= local_end) && (first[sub_first + j].end > local_start)) {
		    unsigned long sub_lines = first[sub_first + j].lines, fragment_length = lines_to_add + 1,
			tmp = 0;
		    char boolean = 0;
		    int fragment_position = -1;

		    // if this is not the first new sub of the block
		    // we find if this sub was present in the previous
		    // new sub
		    if (counter)
			for (i = 0; i < lines_to_add; ++i) {
			    if (placeholder[counter - 1][i] == sub_first + j) {
				placeholder[counter][i] = sub_first + j;
				boolean = 1;
			    }
			}
		    if (boolean)
			continue;

		    // we are looking for the shortest among all groups of
		    // sequential blank lines whose length is greater than or
		    // equal to sub_lines. we store in fragment_position the
		    // position of the shortest group, in fragment_length its
		    // length, and in tmp the length of the group currently
		    // examinated
		    for (i = 0; i < lines_to_add; ++i) {
			if (placeholder[counter][i] == -1) {
			    // placeholder[counter][i] is part of the current group
			    // of blank lines
			    ++tmp;
			} else {
			    if (tmp == sub_lines) {
				// current group's size fits exactly the one we
				// need, so we stop looking
				fragment_position = i - tmp;
				tmp = 0;
				break;
			    }
			    if ((tmp) && (tmp > sub_lines) && (tmp < fragment_length)) {
				// current group is the best we found till here,
				// but is still bigger than the one we are looking
				// for, so we keep on looking
				fragment_length = tmp;
				fragment_position = i - tmp;
				tmp = 0;
			    } else {
				// current group doesn't fit at all, so we forget it
				tmp = 0;
			    }
			}
		    }
		    if (tmp) {
			// last screen line is blank, a group ends with it
			if ((tmp >= sub_lines) && (tmp < fragment_length)) {
			    fragment_position = i - tmp;
			}
		    }
		    if (fragment_position == -1) {
			// it was not possible to find free screen line(s) for a subtitle,
			// usually this means a bug in the code; however we do not overlap
			mp_msg(MSGT_SUBREADER, MSGL_WARN, "SUB: we could not find a suitable position for an overlapping subtitle\n");
			higher_line = SUB_MAX_TEXT + 1;
			break;
		    } else {
			for (tmp = 0; tmp < sub_lines; ++tmp) {
			    placeholder[counter][fragment_position + tmp] = sub_first + j;
			}
		    }
		}
	    }
	    for (j = higher_line + 1; j < lines_to_add; ++j) {
		if (placeholder[counter][j] != -1)
		    higher_line = j;
		else
		    break;
	    }
	    if (higher_line >= SUB_MAX_TEXT) {
		// the 'block' has too much lines, so we don't overlap the
		// subtitles
		second = (subtitle *) realloc(second, (sub_num + sub_to_add + 1) * sizeof(subtitle));
		for (j = 0; j <= sub_to_add; ++j) {
		    int ls;
		    memset(&second[sub_num + j], '\0', sizeof(subtitle));
		    second[sub_num + j].start = first[sub_first + j].start;
		    second[sub_num + j].end   = first[sub_first + j].end;
		    second[sub_num + j].lines = first[sub_first + j].lines;
		    second[sub_num + j].alignment = first[sub_first + j].alignment;
		    for (ls = 0; ls < second[sub_num + j].lines; ls++) {
			second[sub_num + j].text[ls] = strdup(first[sub_first + j].text[ls]);
		    }
		}
		sub_num += sub_to_add + 1;
		sub_first += sub_to_add;
		real_block = 0;
		break;
	    }

	    // we read the placeholder structure and create the new
	    // subs.
	    second = (subtitle *) realloc(second, (sub_num + 1) * sizeof(subtitle));
	    memset(&second[sub_num], '\0', sizeof(subtitle));
	    second[sub_num].start = local_start;
	    second[sub_num].end   = local_end;
	    second[sub_num].alignment = first[sub_first].alignment;
	    n_max = (lines_to_add < SUB_MAX_TEXT) ? lines_to_add : SUB_MAX_TEXT;
	    for (i = 0, j = 0; j < n_max; ++j) {
		if (placeholder[counter][j] != -1) {
		    int lines = first[placeholder[counter][j]].lines;
		    for (ls = 0; ls < lines; ++ls) {
			second[sub_num].text[i++] = strdup(first[placeholder[counter][j]].text[ls]);
		    }
		    j += lines - 1;
		} else {
		    second[sub_num].text[i++] = strdup(" ");
		}
	    }
	    ++sub_num;
	    ++counter;
	} while (local_end < global_end);
	if (real_block)
	    for (i = 0; i < counter; ++i)
		second[start_block_sub + i].lines = higher_line + 1;

	counter = 2 * sub_to_add + 1;
	for (i = 0; i < counter; ++i) {
	    free(placeholder[i]);
	}
	free(placeholder);
	sub_first += sub_to_add;
    }

    for (j = sub_orig - 1; j >= 0; --j) {
	for (i = first[j].lines - 1; i >= 0; --i) {
	    free(first[j].text[i]);
	}
    }
    free(first);

    return_sub = second;
} else { //if(suboverlap_enabled)
    adjust_subs_time(first, 6.0, fps, 1, sub_num, uses_time);/*~6 secs AST*/
    return_sub = first;
}
    if (return_sub == NULL) return NULL;
    subt_data = malloc(sizeof(sub_data));
    subt_data->filename = strdup(filename);
    subt_data->sub_uses_time = uses_time;
    subt_data->sub_num = sub_num;
    subt_data->sub_errs = sub_errs;
    subt_data->subtitles = return_sub;
    return subt_data;
}

#if 0
char * strreplace( char * in,char * what,char * whereof )
{
 int i;
 char * tmp;
 
 if ( ( in == NULL )||( what == NULL )||( whereof == NULL )||( ( tmp=strstr( in,what ) ) == NULL ) ) return NULL;
 for( i=0;i<strlen( whereof );i++ ) tmp[i]=whereof[i];
 if ( strlen( what ) > strlen( whereof ) ) tmp[i]=0;
 return in;
}
#endif


static void strcpy_trim(char *d, char *s)
{
    // skip leading whitespace
    while (*s && !isalnum(*s)) {
	s++;
    }
    for (;;) {
	// copy word
	while (*s && isalnum(*s)) {
	    *d = tolower(*s);
	    s++; d++;
	}
	if (*s == 0) break;
	// trim excess whitespace
	while (*s && !isalnum(*s)) {
	    s++;
	}
	if (*s == 0) break;
	*d++ = ' ';
    }
    *d = 0;
}
 
static void strcpy_strip_ext(char *d, char *s)
{
    char *tmp = strrchr(s,'.');
    if (!tmp) {
	strcpy(d, s);
	return;
    } else {
	strncpy(d, s, tmp-s);
	d[tmp-s] = 0;
    }
    while (*d) {
	*d = tolower(*d);
	d++;
    }
}
 
static void strcpy_get_ext(char *d, char *s)
{
    char *tmp = strrchr(s,'.');
    if (!tmp) {
	strcpy(d, "");
	return;
    } else {
	strcpy(d, tmp+1);
   }
}

static int whiteonly(char *s)
{
    while (*s) {
	if (isalnum(*s)) return 0;
	s++;
  }
    return 1;
}

typedef struct subfn
{
    int priority;
    char *fname;
} subfn;

static int compare_sub_priority(const void *a, const void *b)
{
    if (((const subfn*)a)->priority > ((const subfn*)b)->priority) {
	return -1;
    } else if (((const subfn*)a)->priority < ((const subfn*)b)->priority) {
	return 1;
    } else {
	return strcoll(((const subfn*)a)->fname, ((const subfn*)b)->fname);
    }
}

char** sub_filenames(const char* path, char *fname)
{
    char *f_dir, *f_fname, *f_fname_noext, *f_fname_trim, *tmp, *tmp_sub_id;
    char *tmp_fname_noext, *tmp_fname_trim, *tmp_fname_ext, *tmpresult;
 
    int len, pos, found, i, j;
    char * sub_exts[] = {  "utf", "utf8", "utf-8", "sub", "srt", "smi", "rt", "txt", "ssa", "aqt", "jss", "js", "ass", NULL};
    subfn *result;
    char **result2;
    
    int subcnt;
 
    FILE *f;

    DIR *d;
    struct dirent *de;

    len = (strlen(fname) > 256 ? strlen(fname) : 256)
	+(strlen(path) > 256 ? strlen(path) : 256)+2;

    f_dir = malloc(len);
    f_fname = malloc(len);
    f_fname_noext = malloc(len);
    f_fname_trim = malloc(len);

    tmp_fname_noext = malloc(len);
    tmp_fname_trim = malloc(len);
    tmp_fname_ext = malloc(len);

    tmpresult = malloc(len);

    result = malloc(sizeof(subfn)*MAX_SUBTITLE_FILES);
    memset(result, 0, sizeof(subfn)*MAX_SUBTITLE_FILES);
    
    subcnt = 0;
    
    tmp = strrchr(fname,'/');
#if defined(__MINGW32__) || defined(__CYGWIN__) || defined(__OS2__)
    if(!tmp)tmp = strrchr(fname,'\\');
    if(!tmp)tmp = strrchr(fname,':');
#endif
    
    // extract filename & dirname from fname
    if (tmp) {
	strcpy(f_fname, tmp+1);
	pos = tmp - fname;
	strncpy(f_dir, fname, pos+1);
	f_dir[pos+1] = 0;
    } else {
	strcpy(f_fname, fname);
	strcpy(f_dir, "./");
    }
 
    strcpy_strip_ext(f_fname_noext, f_fname);
    strcpy_trim(f_fname_trim, f_fname_noext);

    tmp_sub_id = NULL;
    if (dvdsub_lang && !whiteonly(dvdsub_lang)) {
	tmp_sub_id = malloc(strlen(dvdsub_lang)+1);
	strcpy_trim(tmp_sub_id, dvdsub_lang);
    }

    // 0 = nothing
    // 1 = any subtitle file
    // 2 = any sub file containing movie name
    // 3 = sub file containing movie name and the lang extension
    for (j = 0; j <= 1; j++) {
	d = opendir(j == 0 ? f_dir : path);
	if (d) {
	    while ((de = readdir(d))) {
		// retrieve various parts of the filename
		strcpy_strip_ext(tmp_fname_noext, de->d_name);
		strcpy_get_ext(tmp_fname_ext, de->d_name);
		strcpy_trim(tmp_fname_trim, tmp_fname_noext);

		// does it end with a subtitle extension?
		found = 0;
#ifdef CONFIG_ICONV
#ifdef CONFIG_ENCA
		for (i = ((sub_cp && strncasecmp(sub_cp, "enca", 4) != 0) ? 3 : 0); sub_exts[i]; i++) {
#else
		for (i = (sub_cp ? 3 : 0); sub_exts[i]; i++) {
#endif
#else
		for (i = 0; sub_exts[i]; i++) {
#endif
		    if (strcasecmp(sub_exts[i], tmp_fname_ext) == 0) {
			found = 1;
			break;
		    }
		}
 
		// we have a (likely) subtitle file
		if (found) {
		    int prio = 0;
		    if (!prio && tmp_sub_id)
		    {
			sprintf(tmpresult, "%s %s", f_fname_trim, tmp_sub_id);
			mp_msg(MSGT_SUBREADER,MSGL_INFO,"dvdsublang...%s\n", tmpresult);
			if (strcmp(tmp_fname_trim, tmpresult) == 0 && sub_match_fuzziness >= 1) {
			    // matches the movie name + lang extension
			    prio = 5;
			}		    
		    }
		    if (!prio && strcmp(tmp_fname_trim, f_fname_trim) == 0) {
			// matches the movie name
			prio = 4;
		    }
		    if (!prio && (tmp = strstr(tmp_fname_trim, f_fname_trim)) && (sub_match_fuzziness >= 1)) {
			// contains the movie name
			tmp += strlen(f_fname_trim);
			if (tmp_sub_id && strstr(tmp, tmp_sub_id)) {
			    // with sub_id specified prefer localized subtitles
			    prio = 3;
			} else if ((tmp_sub_id == NULL) && whiteonly(tmp)) {
			    // without sub_id prefer "plain" name
			    prio = 3;
			} else {
			    // with no localized subs found, try any else instead
			    prio = 2;
			}
		    }
		    if (!prio) {
			// doesn't contain the movie name
			// don't try in the mplayer subtitle directory
			if ((j == 0) && (sub_match_fuzziness >= 2)) {
			    prio = 1;
			}
		    }

		    if (prio) {
			prio += prio;
#ifdef CONFIG_ICONV
			if (i<3){ // prefer UTF-8 coded
			    prio++;
			}
#endif
			sprintf(tmpresult, "%s%s", j == 0 ? f_dir : path, de->d_name);
//			fprintf(stderr, "%s priority %d\n", tmpresult, prio);
			if ((f = fopen(tmpresult, "rt"))) {
			    fclose(f);
			    result[subcnt].priority = prio;
			    result[subcnt].fname = strdup(tmpresult);
			    subcnt++;
			}
		    }

		}
		if (subcnt >= MAX_SUBTITLE_FILES) break;
	    }
	    closedir(d);
	}
 
    }

    if (tmp_sub_id) free(tmp_sub_id);
    
    free(f_dir);
    free(f_fname);
    free(f_fname_noext);
    free(f_fname_trim);

    free(tmp_fname_noext);
    free(tmp_fname_trim);
    free(tmp_fname_ext);

    free(tmpresult);

    qsort(result, subcnt, sizeof(subfn), compare_sub_priority);

    result2 = malloc(sizeof(char*)*(subcnt+1));
    memset(result2, 0, sizeof(char*)*(subcnt+1));

    for (i = 0; i < subcnt; i++) {
	result2[i] = result[i].fname;
    }
    result2[subcnt] = NULL;
    
    free(result);

    return result2;
}

void list_sub_file(sub_data* subd){
    int i,j;
    subtitle *subs = subd->subtitles;

    for(j=0; j < subd->sub_num; j++){
	subtitle* egysub=&subs[j];
        mp_msg(MSGT_SUBREADER,MSGL_INFO,"%i line%c (%li-%li)\n",
		    egysub->lines,
		    (1==egysub->lines)?' ':'s',
		    egysub->start,
		    egysub->end);
	for (i=0; i<egysub->lines; i++) {
	    mp_msg(MSGT_SUBREADER,MSGL_INFO,"\t\t%d: %s%s", i,egysub->text[i], i==egysub->lines-1?"":" \n ");
	}
	mp_msg(MSGT_SUBREADER,MSGL_INFO,"\n");
    }

    mp_msg(MSGT_SUBREADER,MSGL_INFO,"Subtitle format %s time.\n", 
                                  subd->sub_uses_time ? "uses":"doesn't use");
    mp_msg(MSGT_SUBREADER,MSGL_INFO,"Read %i subtitles, %i errors.\n", subd->sub_num, subd->sub_errs);
}

void dump_srt(sub_data* subd, float fps){
    int i,j;
    int h,m,s,ms;
    FILE * fd;
    subtitle * onesub;
    unsigned long temp;
    subtitle *subs = subd->subtitles;

    if (!subd->sub_uses_time && sub_fps == 0)
	sub_fps = fps;
    fd=fopen("dumpsub.srt","w");
    if(!fd)
    { 
	perror("dump_srt: fopen");
	return;
    }
    for(i=0; i < subd->sub_num; i++)
    {
        onesub=subs+i;    //=&subs[i];
	fprintf(fd,"%d\n",i+1);//line number

	temp=onesub->start;
	if (!subd->sub_uses_time)
	    temp = temp * 100 / sub_fps;
	temp -= sub_delay * 100;
	h=temp/360000;temp%=360000;	//h =1*100*60*60
	m=temp/6000;  temp%=6000;	//m =1*100*60
	s=temp/100;   temp%=100;	//s =1*100
	ms=temp*10;			//ms=1*10
	fprintf(fd,"%02d:%02d:%02d,%03d --> ",h,m,s,ms);

	temp=onesub->end;
	if (!subd->sub_uses_time)
	    temp = temp * 100 / sub_fps;
	temp -= sub_delay * 100;
	h=temp/360000;temp%=360000;
	m=temp/6000;  temp%=6000;
	s=temp/100;   temp%=100;
	ms=temp*10;
	fprintf(fd,"%02d:%02d:%02d,%03d\n",h,m,s,ms);
	
	for(j=0;j<onesub->lines;j++)
	    fprintf(fd,"%s\n",onesub->text[j]);

	fprintf(fd,"\n");
    }
    fclose(fd);
    mp_msg(MSGT_SUBREADER,MSGL_INFO,"SUB: Subtitles dumped in \'dumpsub.srt\'.\n");
}

void dump_mpsub(sub_data* subd, float fps){
	int i,j;
	FILE *fd;
	float a,b;
        subtitle *subs = subd->subtitles;

	mpsub_position = subd->sub_uses_time? (sub_delay*100) : (sub_delay*fps);
	if (sub_fps==0) sub_fps=fps;

	fd=fopen ("dump.mpsub", "w");
	if (!fd) {
		perror ("dump_mpsub: fopen");
		return;
	}
	

	if (subd->sub_uses_time) fprintf (fd,"FORMAT=TIME\n\n");
	else fprintf (fd, "FORMAT=%5.2f\n\n", fps);

	for(j=0; j < subd->sub_num; j++){
		subtitle* egysub=&subs[j];
		if (subd->sub_uses_time) {
			a=((egysub->start-mpsub_position)/100.0);
			b=((egysub->end-egysub->start)/100.0);
			if ( (float)((int)a) == a)
			fprintf (fd, "%.0f",a);
			else
			fprintf (fd, "%.2f",a);
			    
			if ( (float)((int)b) == b)
			fprintf (fd, " %.0f\n",b);
			else
			fprintf (fd, " %.2f\n",b);
		} else {
			fprintf (fd, "%ld %ld\n", (long)((egysub->start*(fps/sub_fps))-((mpsub_position*(fps/sub_fps)))),
					(long)(((egysub->end)-(egysub->start))*(fps/sub_fps)));
		}

		mpsub_position = egysub->end;
		for (i=0; i<egysub->lines; i++) {
			fprintf (fd, "%s\n",egysub->text[i]);
		}
		fprintf (fd, "\n");
	}
	fclose (fd);
	mp_msg(MSGT_SUBREADER,MSGL_INFO,"SUB: Subtitles dumped in \'dump.mpsub\'.\n");
}

void dump_microdvd(sub_data* subd, float fps) {
    int i, delay;
    FILE *fd;
    subtitle *subs = subd->subtitles;
    if (sub_fps == 0)
	sub_fps = fps;
    fd = fopen("dumpsub.sub", "w");
    if (!fd) {
	perror("dumpsub.sub: fopen");
	return;
    }
    delay = sub_delay * sub_fps;
    for (i = 0; i < subd->sub_num; ++i) {
	int j, start, end;
	start = subs[i].start;
	end = subs[i].end;
	if (subd->sub_uses_time) {
	    start = start * sub_fps / 100 ;
	    end = end * sub_fps / 100;
	}
	else {
	    start = start * sub_fps / fps;
	    end = end * sub_fps / fps;
	}
	start -= delay;
	end -= delay;
	fprintf(fd, "{%d}{%d}", start, end);
	for (j = 0; j < subs[i].lines; ++j) 
	    fprintf(fd, "%s%s", j ? "|" : "", subs[i].text[j]);
	fprintf(fd, "\n");
    }
    fclose(fd);
    mp_msg(MSGT_SUBREADER,MSGL_INFO,"SUB: Subtitles dumped in \'dumpsub.sub\'.\n");
}

void dump_jacosub(sub_data* subd, float fps) {
    int i,j;
    int h,m,s,cs;
    FILE * fd;
    subtitle * onesub;
    unsigned long temp;
    subtitle *subs = subd->subtitles;

    if (!subd->sub_uses_time && sub_fps == 0)
	sub_fps = fps;
    fd=fopen("dumpsub.jss","w");
    if(!fd)
    { 
	perror("dump_jacosub: fopen");
	return;
    }
    fprintf(fd, "#TIMERES %d\n", (subd->sub_uses_time) ? 100 : (int)sub_fps); 
    for(i=0; i < subd->sub_num; i++)
    {
        onesub=subs+i;    //=&subs[i];

	temp=onesub->start;
	if (!subd->sub_uses_time)
	    temp = temp * 100 / sub_fps;
	temp -= sub_delay * 100;
	h=temp/360000;temp%=360000;	//h =1*100*60*60
	m=temp/6000;  temp%=6000;	//m =1*100*60
	s=temp/100;   temp%=100;	//s =1*100
	cs=temp;			//cs=1*10
	fprintf(fd,"%02d:%02d:%02d.%02d ",h,m,s,cs);

	temp=onesub->end;
	if (!subd->sub_uses_time)
	    temp = temp * 100 / sub_fps;
	temp -= sub_delay * 100;
	h=temp/360000;temp%=360000;
	m=temp/6000;  temp%=6000;
	s=temp/100;   temp%=100;
	cs=temp;
	fprintf(fd,"%02d:%02d:%02d.%02d {~} ",h,m,s,cs);
	
	for(j=0;j<onesub->lines;j++)
	    fprintf(fd,"%s%s",j ? "\\n" : "", onesub->text[j]);

	fprintf(fd,"\n");
    }
    fclose(fd);
    mp_msg(MSGT_SUBREADER,MSGL_INFO,"SUB: Subtitles dumped in \'dumpsub.js\'.\n");
}

void dump_sami(sub_data* subd, float fps) {
    int i,j;
    FILE * fd;
    subtitle * onesub;
    unsigned long temp;
    subtitle *subs = subd->subtitles;

    if (!subd->sub_uses_time && sub_fps == 0)
	sub_fps = fps;
    fd=fopen("dumpsub.smi","w");
    if(!fd)
    { 
	perror("dump_jacosub: fopen");
	return;
    }
    fprintf(fd, "<SAMI>\n"
		"<HEAD>\n"
		"	<STYLE TYPE=\"Text/css\">\n"
		"	<!--\n"
		"	  P {margin-left: 29pt; margin-right: 29pt; font-size: 24pt; text-align: center; font-family: Tahoma; font-weight: bold; color: #FCDD03; background-color: #000000;}\n"
		"	  .SUBTTL {Name: 'Subtitles'; Lang: en-US; SAMIType: CC;}\n"
		"	-->\n"
		"	</STYLE>\n"
		"</HEAD>\n"
		"<BODY>\n");
    for(i=0; i < subd->sub_num; i++)
    {
        onesub=subs+i;    //=&subs[i];

	temp=onesub->start;
	if (!subd->sub_uses_time)
	    temp = temp * 100 / sub_fps;
	temp -= sub_delay * 100;
	fprintf(fd,"\t<SYNC Start=%lu>\n"
		    "\t  <P>", temp * 10);
	
	for(j=0;j<onesub->lines;j++)
	    fprintf(fd,"%s%s",j ? "<br>" : "", onesub->text[j]);

	fprintf(fd,"\n");

	temp=onesub->end;
	if (!subd->sub_uses_time)
	    temp = temp * 100 / sub_fps;
	temp -= sub_delay * 100;
	fprintf(fd,"\t<SYNC Start=%lu>\n"
		    "\t  <P>&nbsp;\n", temp * 10);
    }
    fprintf(fd, "</BODY>\n"
		"</SAMI>\n");
    fclose(fd);
    mp_msg(MSGT_SUBREADER,MSGL_INFO,"SUB: Subtitles dumped in \'dumpsub.smi\'.\n");
}

void sub_free( sub_data * subd )
{
 int i;
 
    if ( !subd ) return;
 
    if (subd->subtitles) {
	for (i=0; i < subd->subtitles->lines; i++) free( subd->subtitles->text[i] );
	free( subd->subtitles );
    }
    if (subd->filename) free( subd->filename );
    free( subd );
}

#define MAX_SUBLINE 512
/**
 * \brief parse text and append it to subtitle in sub
 * \param sub subtitle struct to add text to
 * \param txt text to parse
 * \param len length of text in txt
 * \param endpts pts at which this subtitle text should be removed again
 *
 * <> and {} are interpreted as comment delimiters, "\n", "\N", '\n', '\r'
 * and '\0' are interpreted as newlines, duplicate, leading and trailing
 * newlines are ignored.
 */
void sub_add_text(subtitle *sub, const char *txt, int len, double endpts) {
  int comment = 0;
  int double_newline = 1; // ignore newlines at the beginning
  int i, pos;
  char *buf;
  if (sub->lines >= SUB_MAX_TEXT) return;
  pos = 0;
  buf = malloc(MAX_SUBLINE + 1);
  sub->text[sub->lines] = buf;
  sub->endpts[sub->lines] = endpts;
  for (i = 0; i < len && pos < MAX_SUBLINE; i++) {
    char c = txt[i];
    if (c == '<') comment |= 1;
    if (c == '{') comment |= 2;
    if (comment) {
      if (c == '}') comment &= ~2;
      if (c == '>') comment &= ~1;
      continue;
    }
    if (pos == MAX_SUBLINE - 1) {
      i--;
      c = 0;
    }
    if (c == '\\' && i + 1 < len) {
      c = txt[++i];
      if (c == 'n' || c == 'N') c = 0;
    }
    if (c == '\n' || c == '\r') c = 0;
    if (c) {
      double_newline = 0;
      buf[pos++] = c;
    } else if (!double_newline) {
      if (sub->lines >= SUB_MAX_TEXT - 1) {
        mp_msg(MSGT_VO, MSGL_WARN, "Too many subtitle lines\n");
        break;
      }
      double_newline = 1;
      buf[pos] = 0;
      sub->lines++;
      pos = 0;
      buf = malloc(MAX_SUBLINE + 1);      
      sub->text[sub->lines] = buf;
      sub->endpts[sub->lines] = endpts;
    }
  }
  buf[pos] = 0;
  if (sub->lines < SUB_MAX_TEXT &&
      strlen(sub->text[sub->lines]))
    sub->lines++;
}

#define MP_NOPTS_VALUE (-1LL<<63)
/**
 * \brief remove outdated subtitle lines.
 * \param sub subtitle struct to modify
 * \param pts current pts. All lines with endpts <= this will be removed.
 *            Use MP_NOPTS_VALUE to remove all lines
 * \return 1 if sub was modified, 0 otherwise.
 */
int sub_clear_text(subtitle *sub, double pts) {
  int i = 0;
  int changed = 0;
  while (i < sub->lines) {
    double endpts = sub->endpts[i];
    if (pts == MP_NOPTS_VALUE || (endpts != MP_NOPTS_VALUE && pts >= endpts)) {
      int j;
      free(sub->text[i]);
      for (j = i + 1; j < sub->lines; j++) {
        sub->text[j - 1] = sub->text[j];
        sub->endpts[j - 1] = sub->endpts[j];
      }
      sub->lines--;
      changed = 1;
    } else
      i++;
  }
  return changed;
}
