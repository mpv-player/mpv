/*
 * Subtitle reader with format autodetection
 *
 * Written by laaz
 * Some code cleanup & realloc() by A'rpi/ESP-team
 * dunnowhat sub format by szabi
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

#define ERR ((void *) -1)

#ifdef USE_ICONV
#include <iconv.h>
char *sub_cp=NULL;
#endif
#ifdef USE_FRIBIDI
#include <fribidi/fribidi.h>
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
#ifdef USE_SORTSUB
/* 
   Some subtitling formats, namely AQT and Subrip09, define the end of a
   subtitle as the beginning of the following. Since currently we read one
   subtitle at time, for these format we keep two global *subtitle,
   previous_aqt_sub and previous_subrip09_sub, pointing to previous subtitle,
   so we can change its end when we read current subtitle starting time.
   When USE_SORTSUB is defined, we use a single global unsigned long, 
   previous_sub_end, for both (and even future) formats, to store the end of
   the previous sub: it is initialized to 0 in sub_read_file and eventually
   modified by sub_read_aqt_line or sub_read_subrip09_line.
 */
unsigned long previous_sub_end;
#endif

static int eol(char p) {
    return (p=='\r' || p=='\n' || p=='\0');
}

/* Remove leading and trailing space */
static void trail_space(char *s) {
	int i = 0;
	while (isspace(s[i])) ++i;
	if (i) strcpy(s, s + i);
	i = strlen(s) - 1;
	while (i > 0 && isspace(s[i])) s[i--] = '\0';
}


subtitle *sub_read_line_sami(FILE *fd, subtitle *current) {
    static char line[LINE_LEN+1];
    static char *s = NULL, *slacktime_s;
    char text[LINE_LEN+1], *p=NULL, *q;
    int state;

    current->lines = current->start = current->end = 0;
    state = 0;

    /* read the first line */
    if (!s)
	    if (!(s = fgets(line, LINE_LEN, fd))) return 0;

    do {
	switch (state) {

	case 0: /* find "START=" or "Slacktime:" */
	    slacktime_s = strstr (s, "Slacktime:");
	    if (slacktime_s) 
                sub_slacktime = strtol (slacktime_s+10, NULL, 0) / 10;

	    s = strstr (s, "Start=");
	    if (s) {
		current->start = strtol (s + 6, &s, 0) / 10;
		state = 1; continue;
	    }
	    break;
 
	case 1: /* find "<P" */
	    if ((s = strstr (s, "<P"))) { s += 2; state = 2; continue; }
	    break;
 
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
	    else if (*s == '<') { state = 4; }
	    else if (!strncasecmp (s, "&nbsp;", 6)) { *p++ = ' '; s += 6; }
	    else if (*s == '\t') { *p++ = ' '; s++; }
	    else if (*s == '\r' || *s == '\n') { s++; }
	    else *p++ = *s++;

	    /* skip duplicated space */
	    if (p > text + 2) if (*(p-1) == ' ' && *(p-2) == ' ') p--;
	    
	    continue;

	case 4: /* get current->end or skip <TAG> */
	    q = strstr (s, "Start=");
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
	}

	/* read next line */
	if (state != 99 && !(s = fgets (line, LINE_LEN, fd))) {
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


char *sub_readtext(char *source, char **dest) {
    int len=0;
    char *p=source;
    
//    printf("src=%p  dest=%p  \n",source,dest);

    while ( !eol(*p) && *p!= '|' ) {
	p++,len++;
    }
    
    *dest= (char *)malloc (len+1);
    if (!dest) {return ERR;}
    
    strncpy(*dest, source, len);
    (*dest)[len]=0;
    
    while (*p=='\r' || *p=='\n' || *p=='|') p++;
    
    if (*p) return p;  // not-last text field
    else return NULL;  // last text field
}

subtitle *sub_read_line_microdvd(FILE *fd,subtitle *current) {
    char line[LINE_LEN+1];
    char line2[LINE_LEN+1];
    char *p, *next;
    int i;

    do {
	if (!fgets (line, LINE_LEN, fd)) return NULL;
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

subtitle *sub_read_line_subrip(FILE *fd, subtitle *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL, *q=NULL;
    int len;
    
    while (1) {
	if (!fgets (line, LINE_LEN, fd)) return NULL;
	if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4) < 8) continue;
	current->start = a1*360000+a2*6000+a3*100+a4;
	current->end   = b1*360000+b2*6000+b3*100+b4;

	if (!fgets (line, LINE_LEN, fd)) return NULL;

	p=q=line;
	for (current->lines=1; current->lines < SUB_MAX_TEXT; current->lines++) {
	    for (q=p,len=0; *p && *p!='\r' && *p!='\n' && *p!='|' && strncmp(p,"[br]",4); p++,len++);
	    current->text[current->lines-1]=(char *)malloc (len+1);
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

subtitle *sub_read_line_subviewer(FILE *fd,subtitle *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL;
    int i,len;
    
    while (!current->text[0]) {
	if (!fgets (line, LINE_LEN, fd)) return NULL;
	if ((len=sscanf (line, "%d:%d:%d%[,.:]%d --> %d:%d:%d%[,.:]%d",&a1,&a2,&a3,(char *)&i,&a4,&b1,&b2,&b3,(char *)&i,&b4)) < 10)
	    continue;
	current->start = a1*360000+a2*6000+a3*100+a4/10;
	current->end   = b1*360000+b2*6000+b3*100+b4/10;
	for (i=0; i<SUB_MAX_TEXT;) {
	    if (!fgets (line, LINE_LEN, fd)) break;
	    len=0;
	    for (p=line; *p!='\n' && *p!='\r' && *p; p++,len++);
	    if (len) {
                int j=0,skip=0;
		char *curptr=current->text[i]=(char *)malloc (len+1);
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

subtitle *sub_read_line_subviewer2(FILE *fd,subtitle *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3,a4;
    char *p=NULL;
    int i,len;
   
    while (!current->text[0]) {
        if (!fgets (line, LINE_LEN, fd)) return NULL;
	if (line[0]!='{')
	    continue;
        if ((len=sscanf (line, "{T %d:%d:%d:%d",&a1,&a2,&a3,&a4)) < 4)
            continue;
        current->start = a1*360000+a2*6000+a3*100+a4/10;
        for (i=0; i<SUB_MAX_TEXT;) {
            if (!fgets (line, LINE_LEN, fd)) break;
            if (line[0]=='}') break;
            len=0;
            for (p=line; *p!='\n' && *p!='\r' && *p; ++p,++len);
            if (len) {
                current->text[i]=(char *)malloc (len+1);
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


subtitle *sub_read_line_vplayer(FILE *fd,subtitle *current) {
	char line[LINE_LEN+1];
	int a1,a2,a3;
	char *p=NULL, *next,separator;
	int i,len,plen;

	while (!current->text[0]) {
		if (!fgets (line, LINE_LEN, fd)) return NULL;
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

subtitle *sub_read_line_rt(FILE *fd,subtitle *current) {
	//TODO: This format uses quite rich (sub/super)set of xhtml 
	// I couldn't check it since DTD is not included.
	// WARNING: full XML parses can be required for proper parsing 
    char line[LINE_LEN+1];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL,*next=NULL;
    int i,len,plen;
    
    while (!current->text[0]) {
	if (!fgets (line, LINE_LEN, fd)) return NULL;
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

subtitle *sub_read_line_ssa(FILE *fd,subtitle *current) {
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
		if (!fgets (line, LINE_LEN, fd)) return NULL;
	} while (sscanf (line, "Dialogue: Marked=%d,%d:%d:%d.%d,%d:%d:%d.%d,"
			"%[^\n\r]", &nothing,
			&hour1, &min1, &sec1, &hunsec1, 
			&hour2, &min2, &sec2, &hunsec2,
			line3) < 9);

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
		current->text[num]=(char *)malloc(tmp-line2+1);
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

void sub_pp_ssa(subtitle *sub) {
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

subtitle *sub_read_line_dunnowhat(FILE *fd,subtitle *current) {
    char line[LINE_LEN+1];
    char text[LINE_LEN+1];

    if (!fgets (line, LINE_LEN, fd))
	return NULL;
    if (sscanf (line, "%ld,%ld,\"%[^\"]", &(current->start),
		&(current->end), text) <3)
	return ERR;
    current->text[0] = strdup(text);
    current->lines = 1;

    return current;
}

subtitle *sub_read_line_mpsub(FILE *fd, subtitle *current) {
	char line[LINE_LEN+1];
	float a,b;
	int num=0;
	char *p, *q;

	do
	{
		if (!fgets(line, LINE_LEN, fd)) return NULL;
	} while (sscanf (line, "%f %f", &a, &b) !=2);

	mpsub_position += a*mpsub_multiplier; 
	current->start=(int) mpsub_position;
	mpsub_position += b*mpsub_multiplier; 
	current->end=(int) mpsub_position;

	while (num < SUB_MAX_TEXT) {
		if (!fgets (line, LINE_LEN, fd)) {
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

#ifndef USE_SORTSUB
//we don't need this if we use previous_sub_end
subtitle *previous_aqt_sub = NULL;
#endif

subtitle *sub_read_line_aqt(FILE *fd,subtitle *current) {
    char line[LINE_LEN+1];
    char *next;
    int i;

    while (1) {
    // try to locate next subtitle
        if (!fgets (line, LINE_LEN, fd))
		return NULL;
        if (!(sscanf (line, "-->> %ld", &(current->start)) <1))
		break;
    }
    
#ifdef USE_SORTSUB
    previous_sub_end = (current->start) ? current->start - 1 : 0; 
#else
    if (previous_aqt_sub != NULL) 
	previous_aqt_sub->end = current->start-1;
    
    previous_aqt_sub = current;
#endif

    if (!fgets (line, LINE_LEN, fd))
	return NULL;

    sub_readtext((char *) &line,&current->text[0]);
    current->lines = 1;
    current->end = current->start; // will be corrected by next subtitle

    if (!fgets (line, LINE_LEN, fd))
	return current;

    next = line,i=1;
    while ((next =sub_readtext (next, &(current->text[i])))) {
	if (current->text[i]==ERR) {return ERR;}
	i++;
	if (i>=SUB_MAX_TEXT) { mp_msg(MSGT_SUBREADER,MSGL_WARN,"Too many lines in a subtitle\n");current->lines=i;return current;}
	}
    current->lines=i+1;

    if ((current->text[0]=="") && (current->text[1]=="")) {
#ifdef USE_SORTSUB
	previous_sub_end = 0;
#else
	// void subtitle -> end of previous marked and exit
	previous_aqt_sub = NULL;
#endif	
	return NULL;
	}

    return current;
}

#ifndef USE_SORTSUB
subtitle *previous_subrip09_sub = NULL;
#endif

subtitle *sub_read_line_subrip09(FILE *fd,subtitle *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3;
    char * next=NULL;
    int i,len;
   
    while (1) {
    // try to locate next subtitle
        if (!fgets (line, LINE_LEN, fd))
		return NULL;
        if (!((len=sscanf (line, "[%d:%d:%d]",&a1,&a2,&a3)) < 3))
		break;
    }

    current->start = a1*360000+a2*6000+a3*100;
    
#ifdef USE_SORTSUB
    previous_sub_end = (current->start) ? current->start - 1 : 0; 
#else
    if (previous_subrip09_sub != NULL) 
	previous_subrip09_sub->end = current->start-1;
    
    previous_subrip09_sub = current;
#endif

    if (!fgets (line, LINE_LEN, fd))
	return NULL;

    next = line,i=0;
    
    current->text[0]=""; // just to be sure that string is clear
    
    while ((next =sub_readtext (next, &(current->text[i])))) {
	if (current->text[i]==ERR) {return ERR;}
	i++;
	if (i>=SUB_MAX_TEXT) { mp_msg(MSGT_SUBREADER,MSGL_WARN,"Too many lines in a subtitle\n");current->lines=i;return current;}
	}
    current->lines=i+1;

    if ((current->text[0]=="") && (i==0)) {
#ifdef USE_SORTSUB
	previous_sub_end = 0;
#else
	// void subtitle -> end of previous marked and exit
	previous_subrip09_sub = NULL;
#endif	
	return NULL;
	}

    return current;
}

subtitle *sub_read_line_jacosub(FILE * fd, subtitle * current)
{
    char line1[LINE_LEN], line2[LINE_LEN], directive[LINE_LEN], *p, *q;
    unsigned a1, a2, a3, a4, b1, b2, b3, b4, comment = 0;
    static unsigned jacoTimeres = 30;
    static int jacoShift = 0;

    bzero(current, sizeof(subtitle));
    bzero(line1, LINE_LEN);
    bzero(line2, LINE_LEN);
    bzero(directive, LINE_LEN);
    while (!current->text[0]) {
	if (!fgets(line1, LINE_LEN, fd)) {
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
	    strcpy(line2, line1);
	    p = line2;
	}
	for (q = line1; (!eol(*p)) && (current->lines + 1 < SUB_MAX_TEXT);
	     ++p) {
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
		    if (!fgets(directive, LINE_LEN, fd))
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

int sub_autodetect (FILE *fd, int *uses_time) {
    char line[LINE_LEN+1];
    int i,j=0;
    char p;
    
    while (j < 100) {
	j++;
	if (!fgets (line, LINE_LEN, fd))
	    return SUB_INVALID;

	if (sscanf (line, "{%d}{%d}", &i, &i)==2)
		{*uses_time=0;return SUB_MICRODVD;}
	if (sscanf (line, "{%d}{}", &i)==1)
		{*uses_time=0;return SUB_MICRODVD;}
	if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",     &i, &i, &i, &i, &i, &i, &i, &i)==8)
		{*uses_time=1;return SUB_SUBRIP;}
	if (sscanf (line, "%d:%d:%d%[,.:]%d --> %d:%d:%d%[,.:]%d", &i, &i, &i, (char *)&i, &i, &i, &i, &i, (char *)&i, &i)==10)
		{*uses_time=1;return SUB_SUBVIEWER;}
	if (sscanf (line, "{T %d:%d:%d:%d",&i, &i, &i, &i))
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
	if (sscanf (line, "%d,%d,\"%c", &i, &i, (char *) &i) == 3)
		{*uses_time=0;return SUB_DUNNOWHAT;}
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

#ifdef DUMPSUBS
int sub_utf8=0;
#else
extern int sub_utf8;
int sub_utf8_prev=0;
#endif

extern float sub_delay;
extern float sub_fps;

#ifdef USE_ICONV
static iconv_t icdsc;

void	subcp_open (void)
{
	char *tocp = "UTF-8";
	icdsc = (iconv_t)(-1);

	if (sub_cp){
		if ((icdsc = iconv_open (tocp, sub_cp)) != (iconv_t)(-1)){
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

#define ICBUFFSIZE 512
static char icbuffer[ICBUFFSIZE];

subtitle* subcp_recode (subtitle *sub)
{
	int l=sub->lines;
	size_t ileft, oleft;
	char *op, *ip, *ot;

	while (l){
		op = icbuffer;
		ip = sub->text[--l];
		ileft = strlen(ip);
		oleft = ICBUFFSIZE - 1;

		if (iconv(icdsc, &ip, &ileft,
			  &op, &oleft) == (size_t)(-1)) {
			mp_msg(MSGT_SUBREADER,MSGL_WARN,"SUB: error recoding line (1).\n");
			l++;
			break;
		}
		if (!(ot = (char *)malloc(op - icbuffer + 1))){
			mp_msg(MSGT_SUBREADER,MSGL_WARN,"SUB: error allocating mem.\n");
			l++;
		   	break;
		}
		*op='\0' ;
		strcpy (ot, icbuffer);
		free (sub->text[l]);
		sub->text[l] = ot;
	}
	if (l){
		for (l = sub->lines; l;)
			free (sub->text[--l]);
		return ERR;
	}
	return sub;
}

// for demux_ogg.c:
subtitle* subcp_recode1 (subtitle *sub)
{
  int l=sub->lines;
  size_t ileft, oleft;
  
  if(icdsc == (iconv_t)(-1)) return sub;

  while (l){
     char *ip = icbuffer;
     char *op = sub->text[--l];
     strcpy(ip, op);
     ileft = strlen(ip);
     oleft = ICBUFFSIZE - 1;
		
     if (iconv(icdsc, &ip, &ileft,
	      &op, &oleft) == (size_t)(-1)) {
	mp_msg(MSGT_SUBREADER,MSGL_V,"SUB: error recoding line (2).\n");
	return sub;
     }
     *op='\0' ;
  }
  return sub;
}
#endif

#ifdef USE_FRIBIDI
#define ALLOCATE(tp,ln) ((tp *) malloc (sizeof (tp) * (ln)))
#define max(a,b)  (((a)>(b))?(a):(b))
subtitle* sub_fribidi (subtitle *sub, int sub_utf8)
{
  FriBidiChar logical[LINE_LEN+1], visual[LINE_LEN+1]; // Hopefully these two won't smash the stack
  char        *ip      = NULL, *op     = NULL;
  FriBidiCharType base;
  size_t len,orig_len;
  int l=sub->lines;
  int char_set_num;
  fribidi_boolean log2vis;
  fribidi_set_mirroring (FRIBIDI_TRUE);
  fribidi_set_reorder_nsm (FRIBIDI_FALSE);
   
  if( sub_utf8 == 0 ) {
    char_set_num = fribidi_parse_charset ("ISO8859-8");//We might want to make this a config option
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
    base = FRIBIDI_TYPE_ON;
    log2vis = fribidi_log2vis (logical, len, &base,
			       /* output */
			       visual, NULL, NULL, NULL);
    if(log2vis) {
      len = fribidi_remove_bidi_marks (visual, len, NULL, NULL,
				       NULL);
      if((op = ALLOCATE(char,(max(2*orig_len,2*len) + 1))) == NULL) {
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
    subtitle * (*read)(FILE *fd,subtitle *dest);
    void       (*post)(subtitle *dest);
    const char *name;
};

sub_data* sub_read_file (char *filename, float fps) {
        //filename is assumed to be malloc'ed,  free() is used in sub_free()
    FILE *fd;
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
	    { sub_read_line_dunnowhat, NULL, "dunnowhat" },
	    { sub_read_line_mpsub, NULL, "mpsub" },
	    { sub_read_line_aqt, NULL, "aqt" },
	    { sub_read_line_subviewer2, NULL, "subviewer 2.0" },
	    { sub_read_line_subrip09, NULL, "subrip 0.9" },
	    { sub_read_line_jacosub, NULL, "jacosub" }
    };
    struct subreader *srp;
    
    if(filename==NULL) return NULL; //qnx segfault
    fd=fopen (filename, "r"); if (!fd) return NULL;

    sub_format=sub_autodetect (fd, &uses_time);
    mpsub_multiplier = (uses_time ? 100.0 : 1.0);
    if (sub_format==SUB_INVALID) {mp_msg(MSGT_SUBREADER,MSGL_WARN,"SUB: Could not determine file format\n");return NULL;}
    srp=sr+sub_format;
    mp_msg(MSGT_SUBREADER,MSGL_INFO,"SUB: Detected subtitle file format: %s\n", srp->name);
    
    rewind (fd);

#ifdef USE_ICONV
    sub_utf8_prev=sub_utf8;
    subcp_open();
#endif

    sub_num=0;n_max=32;
    first=(subtitle *)malloc(n_max*sizeof(subtitle));
    if(!first) return NULL;
    
#ifdef USE_SORTSUB
    sub = (subtitle *)malloc(sizeof(subtitle));
    //This is to deal with those formats (AQT & Subrip) which define the end of a subtitle
    //as the beginning of the following
    previous_sub_end = 0;
#endif    
    while(1){
        if(sub_num>=n_max){
            n_max+=16;
            first=realloc(first,n_max*sizeof(subtitle));
        }
#ifndef USE_SORTSUB
	sub = &first[sub_num];
#endif	
	memset(sub, '\0', sizeof(subtitle));
        sub=srp->read(fd,sub);
        if(!sub) break;   // EOF
#ifdef USE_ICONV
	if ((sub!=ERR) && (sub_utf8 & 2)) sub=subcp_recode(sub);
#endif
#ifdef USE_FRIBIDI
	if (sub!=ERR) sub=sub_fribidi(sub,sub_utf8);
#endif
	if ( sub == ERR )
	 {
#ifdef USE_ICONV
          subcp_close();
          sub_utf8=sub_utf8_prev;
#endif
    	  if ( first ) free(first);
	  return NULL; 
	 }
        // Apply any post processing that needs recoding first
        if ((sub!=ERR) && !sub_no_text_pp && srp->post) srp->post(sub);
#ifdef USE_SORTSUB
	if(!sub_num || (first[sub_num - 1].start <= sub->start)){
	    first[sub_num].start = sub->start;
  	    first[sub_num].end   = sub->end;
	    first[sub_num].lines = sub->lines;
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
    		for(i = 0; i < first[j].lines; ++i){
      		    first[j + 1].text[i] = first[j].text[i];
		}
		if(!j || (first[j - 1].start <= sub->start)){
	    	    first[j].start = sub->start;
	    	    first[j].end   = sub->end;
	    	    first[j].lines = sub->lines;
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
    
    fclose(fd);

#ifdef USE_ICONV
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
	placeholder = (int **) malloc(sizeof(int *) * counter);
	for (i = 0; i < counter; ++i) {
	    placeholder[i] = (int *) malloc(sizeof(int) * lines_to_add);
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
    subt_data = (sub_data *)malloc(sizeof(sub_data));
    subt_data->filename = filename;
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

typedef struct _subfn 
{
    int priority;
    char *fname;
} subfn;

static int compare_sub_priority(const void *a, const void *b)
{
    if (((subfn*)a)->priority > ((subfn*)b)->priority) {
	return -1;
    } else if (((subfn*)a)->priority < ((subfn*)b)->priority) {
	return 1;
    } else {
	return strcoll(((subfn*)a)->fname, ((subfn*)b)->fname);
    }
}

char** sub_filenames(char* path, char *fname)
{
    char *f_dir, *f_fname, *f_fname_noext, *f_fname_trim, *tmp, *tmp_sub_id;
    char *tmp_fname_noext, *tmp_fname_trim, *tmp_fname_ext, *tmpresult;
 
    int len, pos, found, i, j;
    char * sub_exts[] = { "utf", "sub", "srt", "smi", "rt", "txt", "ssa", "aqt", "jss", NULL};
    subfn *result;
    char **result2;
    
    int subcnt;
 
    FILE *f;

    DIR *d;
    struct dirent *de;

    len = (strlen(fname) > 256 ? strlen(fname) : 256)
	+(strlen(path) > 256 ? strlen(path) : 256)+2;

    f_dir = (char*)malloc(len);
    f_fname = (char*)malloc(len);
    f_fname_noext = (char*)malloc(len);
    f_fname_trim = (char*)malloc(len);

    tmp_fname_noext = (char*)malloc(len);
    tmp_fname_trim = (char*)malloc(len);
    tmp_fname_ext = (char*)malloc(len);

    tmpresult = (char*)malloc(len);

    result = (subfn*)malloc(sizeof(subfn)*MAX_SUBTITLE_FILES);
    memset(result, 0, sizeof(subfn)*MAX_SUBTITLE_FILES);
    
    subcnt = 0;
    
    tmp = strrchr(fname,'/');
    
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
	tmp_sub_id = (char*)malloc(strlen(dvdsub_lang)+1);
	strcpy_trim(tmp_sub_id, dvdsub_lang);
    }

    // 0 = nothing
    // 1 = any subtitle file
    // 2 = any sub file containing movie name
    // 3 = sub file containing movie name and the lang extension
    for (j = 0; j <= 1; j++) {
	d = opendir(j == 0 ? f_dir : path);
	if (d) {
	    while (de = readdir(d)) {
		// retrieve various parts of the filename
		strcpy_strip_ext(tmp_fname_noext, de->d_name);
		strcpy_get_ext(tmp_fname_ext, de->d_name);
		strcpy_trim(tmp_fname_trim, tmp_fname_noext);

		// does it end with a subtitle extension?
		found = 0;
#ifdef USE_ICONV
		for (i = (sub_cp ? 1 : 0); sub_exts[i]; i++) {
#else
		for (i = 0; sub_exts[i]; i++) {
#endif
		    if (strcmp(sub_exts[i], tmp_fname_ext) == 0) {
			found = 1;
			break;
		    }
		}
 
		// we have a (likely) subtitle file
		if (found) {
		    int prio = 0;
		    sprintf(tmpresult, "%s %s", f_fname_trim, tmp_sub_id);
		    if (strcmp(tmp_fname_trim, tmpresult) == 0 && sub_match_fuzziness >= 1) {
			// matches the movie name + lang extension
			prio = 5;
		    } else if (strcmp(tmp_fname_trim, f_fname_trim) == 0) {
			// matches the movie name
			prio = 4;
		    } else if ((tmp = strstr(tmp_fname_trim, f_fname_trim)) && (sub_match_fuzziness >= 1)) {
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
		    } else {
			// doesn't contain the movie name
			// don't try in the mplayer subtitle directory
			if ((j == 0) && (sub_match_fuzziness >= 2)) {
			    prio = 1;
			}
		    }

		    if (prio) {
			sprintf(tmpresult, "%s%s", f_dir, de->d_name);
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

    result2 = (char**)malloc(sizeof(char*)*(subcnt+1));
    memset(result2, 0, sizeof(char*)*(subcnt+1));

    for (i = 0; i < subcnt; i++) {
	result2[i] = result[i].fname;
    }
    result2[subcnt] = NULL;

    return result2;
}

void list_sub_file(sub_data* subd){
    int i,j;
    subtitle *subs = subd->subtitles;

    for(j=0; j < subd->sub_num; j++){
	subtitle* egysub=&subs[j];
        printf ("%i line%c (%li-%li)\n",
		    egysub->lines,
		    (1==egysub->lines)?' ':'s',
		    egysub->start,
		    egysub->end);
	for (i=0; i<egysub->lines; i++) {
	    printf ("\t\t%d: %s%s", i,egysub->text[i], i==egysub->lines-1?"":" \n ");
	}
	printf ("\n");
    }

    printf ("Subtitle format %s time.\n", 
                                  subd->sub_uses_time ? "uses":"doesn't use");
    printf ("Read %i subtitles, %i errors.\n", subd->sub_num, subd->sub_errs);
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
    fd = fopen("dumpsub.txt", "w");
    if (!fd) {
	perror("dumpsub.txt: fopen");
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
    mp_msg(MSGT_SUBREADER,MSGL_INFO,"SUB: Subtitles dumped in \'dumpsub.txt\'.\n");
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

#ifdef DUMPSUBS
int main(int argc, char **argv) {  // for testing
    sub_data *subd;
    
    if(argc<2){
        printf("\nUsage: subreader filename.sub\n\n");
        exit(1);
    }
    sub_cp = argv[2]; 
    subd = sub_read_file(argv[1]);
    if(!subd){
        printf("Couldn't load file.\n");
        exit(1);
    }
    
    list_sub_file(subd);

    return 0;
}
#endif
