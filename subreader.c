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

#include "subreader.h"

#define ERR (void *)-1


int sub_uses_time=0;
int sub_errs=0;
int sub_num=0;          // number of subtitle structs
int sub_format=-1;     // 0 for microdvd
		      // 1 for SubRip
		     // 2 for SubViewer
		    // 3 for SAMI (smi)
		   // 4 for vplayer format
		  // 5 for RT format
		 // 6 for ssa (Sub Station Alpha)
		// 7 for ... erm ... dunnowhat. tell me if you know

int eol(char p) {
    return (p=='\r' || p=='\n' || p=='\0');
}

static inline void trail_space(char *s) {
	int i;
	while (isspace(*s)) strcpy(s, s + 1);
	i = strlen(s) - 1;
	while (i > 0 && isspace(s[i])) s[i--] = '\0';
}

subtitle *sub_read_line_sami(FILE *fd, subtitle *current) {
    static char line[1001];
    static char *s = NULL;
    char text[1000], *p, *q;
    int state;

    current->lines = current->start = current->end = 0;
    state = 0;

    /* read the first line */
    if (!s)
	    if (!(s = fgets(line, 1000, fd))) return 0;

    do {
	switch (state) {

	case 0: /* find "START=" */
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
	    if (*s == '\0') { break; }
	    else if (*s == '<') { state = 4; }
	    else if (!strncasecmp (s, "&nbsp;", 6)) { *p++ = ' '; s += 6; }
	    else if (*s == '\r') { s++; }
	    else if (!strncasecmp (s, "<br>", 4) || *s == '\n') {
		*p = '\0'; p = text; trail_space (text);
		if (text[0] != '\0')
		    current->text[current->lines++] = strdup (text);
		if (*s == '\n') s++; else s += 4;
	    }
	    else *p++ = *s++;
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
	if (state != 99 && !(s = fgets (line, 1000, fd))) return 0;

    } while (state != 99);

    return current;
}


char *sub_readtext(char *source, char **dest) {
    int len=0;
    char *p=source;
    
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
    char line[1001];
    char line2[1001];
    char *p, *next;
    int i;

    bzero (current, sizeof(current));

    do {
	if (!fgets (line, 1000, fd)) return NULL;
    } while (sscanf (line, "{%ld}{%ld}%[^\r\n]", &(current->start), &(current->end),line2) <3);

    p=line2;

    next=p, i=0;
    while ((next =sub_readtext (next, &(current->text[i])))) {
        if (current->text[i]==ERR) {return ERR;}
	i++;
	if (i>=SUB_MAX_TEXT) { printf ("Too many lines in a subtitle\n");current->lines=i;return current;}
    }
    current->lines= ++i;

    return current;
}

subtitle *sub_read_line_subrip(FILE *fd, subtitle *current) {
    char line[1001];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL, *q=NULL;
    int len;
    
    bzero (current, sizeof(current));
    
    while (!current->text[0]) {
	if (!fgets (line, 1000, fd)) return NULL;
	if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4) < 8) continue;
	current->start = a1*360000+a2*6000+a3*100+a4;
	current->end   = b1*360000+b2*6000+b3*100+b4;

	if (!fgets (line, 1000, fd)) return NULL;

	p=q=line;
	for (current->lines=1; current->lines < SUB_MAX_TEXT; current->lines++) {
	    for (q=p,len=0; *p && *p!='\r' && *p!='\n' && strncmp(p,"[br]",4); p++,len++);
	    current->text[current->lines-1]=(char *)malloc (len+1);
	    if (!current->text[current->lines-1]) return ERR;
	    strncpy (current->text[current->lines-1], q, len);
	    current->text[current->lines-1][len]='\0';
	    if (!*p || *p=='\r' || *p=='\n') break;
	    while (*p++!=']');
	}
    }
    return current;
}

subtitle *sub_read_line_third(FILE *fd,subtitle *current) {
    char line[1001];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL;
    int i,len;
    
    bzero (current, sizeof(current));
    
    while (!current->text[0]) {
	if (!fgets (line, 1000, fd)) return NULL;
	if ((len=sscanf (line, "%d:%d:%d,%d --> %d:%d:%d,%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4)) < 8)
	    continue;
	current->start = a1*360000+a2*6000+a3*100+a4/10;
	current->end   = b1*360000+b2*6000+b3*100+b4/10;
	for (i=0; i<SUB_MAX_TEXT;) {
	    if (!fgets (line, 1000, fd)) break;
	    len=0;
	    for (p=line; *p!='\n' && *p!='\r' && *p; p++,len++);
	    if (len) {
		current->text[i]=(char *)malloc (len+1);
		if (!current->text[i]) return ERR;
		strncpy (current->text[i], line, len); current->text[i][len]='\0';
		i++;
	    } else {
		break;
	    }
	}
	current->lines=i;
    }
    return current;
}

subtitle *sub_read_line_vplayer(FILE *fd,subtitle *current) {
	char line[1001];
	char line2[1001];
	int a1,a2,a3,b1,b2,b3;
	char *p=NULL, *next;
	int i,len,len2,plen;

	bzero (current, sizeof(current));

	while (!current->text[0]) {
		if (!fgets (line, 1000, fd)) return NULL;
		if ((len=sscanf (line, "%d:%d:%d:%n",&a1,&a2,&a3,&plen)) < 3)
			continue;
		if (!fgets (line2, 1000, fd)) return NULL;
		if ((len2=sscanf (line2, "%d:%d:%d:",&b1,&b2,&b3)) < 3)
			continue;
		// przewiñ o linijkê do ty³u:
		fseek(fd,-strlen(line2),SEEK_CUR);

		current->start = a1*360000+a2*6000+a3*100;
		current->end   = b1*360000+b2*6000+b3*100;
		if ((current->end - current->start) > 1000) {current->end = current->start + 1000;} // not too long though. 
		// teraz czas na wkopiowanie stringu
 		p=line;	
 		// finds the body of the subtitle
 		for (i=0; i<3; i++){              
                   p=strchr(p,':')+1;
                 } 
 		i=0;

		if (*p!='|') {
			//
			next = p,i=0;
			while ((next =sub_readtext (next, &(current->text[i])))) {
				if (current->text[i]==ERR) {return ERR;}
				i++;
				if (i>=SUB_MAX_TEXT) { printf ("Too many lines in a subtitle\n");current->lines=i;return current;}
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
    char line[1001];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL,*next=NULL;
    int i,len,plen;
    
    bzero (current, sizeof(current));
    
    while (!current->text[0]) {
	if (!fgets (line, 1000, fd)) return NULL;
	//TODO: it seems that format of time is not easily determined, it may be 1:12, 1:12.0 or 0:1:12.0
	//to describe the same moment in time. Maybe there are even more formats in use.
	//if ((len=sscanf (line, "<Time Begin=\"%d:%d:%d.%d\" End=\"%d:%d:%d.%d\"",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4)) < 8)
	plen=a1=a2=a3=a4=b1=b2=b3=b4=0;
	if (
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d\" %*[Ee]nd=\"%d:%d\"%*[^<]<clear/>%n",&a2,&a3,&b2,&b3,&plen)) < 4) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d\" %*[Ee]nd=\"%d:%d.%d\"%*[^<]<clear/>%n",&a2,&a3,&b2,&b3,&b4,&plen)) < 5) &&
//	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d.%d\" %*[Ee]nd=\"%d:%d\"%*[^<]<clear/>%n",&a2,&a3,&a4,&b2,&b3,&plen)) < 5) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d.%d\" %*[Ee]nd=\"%d:%d.%d\"%*[^<]<clear/>%n",&a2,&a3,&a4,&b2,&b3,&b4,&plen)) < 6) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d:%d.%d\" %*[Ee]nd=\"%d:%d:%d.%d\"%*[^<]<clear/>%n",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4,&plen)) < 8) 
	)
	    continue;
	current->start = a1*360000+a2*6000+a3*100+a4/10;
	current->end   = b1*360000+b2*6000+b3*100+b4/10;
	p=line;	p+=plen;i=0;
	// TODO: I don't know what kind of convention is here for marking multiline subs, maybe <br/> like in xml?
	next = strstr(line,"<clear/>")+8;i=0;
	while ((next =sub_readtext (next, &(current->text[i])))) {
		if (current->text[i]==ERR) {return ERR;}
		i++;
		if (i>=SUB_MAX_TEXT) { printf ("Too many lines in a subtitle\n");current->lines=i;return current;}
	}
			current->lines=i+1;
    }
    return current;
}

subtitle *sub_read_line_ssa(FILE *fd,subtitle *current) {
	int hour1, min1, sec1, hunsec1,
	    hour2, min2, sec2, hunsec2, nothing;
	
	char line[1000],
	     line2[1000];
	do {
		if (!fgets (line, 1000, fd)) return NULL;
	} while (sscanf (line, "Dialogue: Marked=%d,%d:%d:%d.%d,%d:%d:%d.%d,"
			"*Default,%d,%d,%d,%d,,%[^\n\r]", &nothing, &hour1, &min1,
			&sec1, &hunsec1,
			&hour2, &min2, &sec2, &hunsec2, &nothing,
			&nothing, &nothing, &nothing, line2) < 14);
	current->lines=1;
	current->start = 360000*hour1 + 6000*min1 + 100*sec1 + hunsec1;
	current->end   = 360000*hour2 + 6000*min2 + 100*sec2 + hunsec2;
	current->text[0]=(char *) malloc(strlen(line2)+1);
	strcpy(current->text[0],line2);

	return current;
}

subtitle *sub_read_line_dunnowhat(FILE *fd,subtitle *current) {
    char line[1001];
    char text[1001];

    bzero (current, sizeof(current));

    if (!fgets (line, 1000, fd))
	return NULL;
    if (sscanf (line, "%ld,%ld,\"%[^\"]", &(current->start),
		&(current->end), text) <3)
	return ERR;
    current->text[0] = strdup(text);
    current->lines = 1;

    return current;
}

int sub_autodetect (FILE *fd) {
    char line[1001];
    int i,j=0;
//    char *p;
    
    while (j < 100) {
	j++;
	if (!fgets (line, 1000, fd))
	    return -1;

	if (sscanf (line, "{%d}{%d}", &i, &i)==2)
		{sub_uses_time=0;return 0;}
	if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",     &i, &i, &i, &i, &i, &i, &i, &i)==8)
		{sub_uses_time=1;return 1;}
	if (sscanf (line, "%d:%d:%d,%d --> %d:%d:%d,%d", &i, &i, &i, &i, &i, &i, &i, &i)==8)
		{sub_uses_time=1;return 2;}
	if (strstr (line, "<SAMI>"))
		{sub_uses_time=1; return 3;}
	if (sscanf (line, "%d:%d:%d:",     &i, &i, &i )==3)
		{sub_uses_time=1;return 4;}
	//TODO: just checking if first line of sub starts with "<" is WAY
	// too weak test for RT
	// Please someone who knows the format of RT... FIX IT!!!
	// It may conflict with other sub formats in the future (actually it doesn't)
	if ( *line == '<' )
		{sub_uses_time=1;return 5;}

	// I have only seen only 1 piece of .ssa file.
	// It may be not correct (tell me if it's not)
	if (!memcmp(line, "Dialogue: Marked", 16))
		{sub_uses_time=1; return 6;}
	if (sscanf (line, "%d,%d,\"%c", &i, &i, (char *) &i) == 3)
		{sub_uses_time=0;return 7;}
    }

    return -1;  // too many bad lines
}


subtitle* sub_read_file (char *filename) {
    FILE *fd;
    int n_max;
    subtitle *first;
    subtitle * (*func[])(FILE *fd,subtitle *dest)=
    {
	    sub_read_line_microdvd,
	    sub_read_line_subrip,
	    sub_read_line_third,
	    sub_read_line_sami,
	    sub_read_line_vplayer,
	    sub_read_line_rt,
	    sub_read_line_ssa,
	    sub_read_line_dunnowhat
    };

    fd=fopen (filename, "r"); if (!fd) return NULL;

    sub_format=sub_autodetect (fd);
    if (sub_format==-1) {printf ("SUB: Could not determine file format\n");return NULL;}
    printf ("SUB: Detected subtitle file format: %d\n",sub_format);
    
    rewind (fd);

    sub_num=0;n_max=32;
    first=(subtitle *)malloc(n_max*sizeof(subtitle));
    if(!first) return NULL;
    
    while(1){
        subtitle *sub;
        if(sub_num>=n_max){
            n_max+=16;
            first=realloc(first,n_max*sizeof(subtitle));
        }
        sub=func[sub_format](fd,&first[sub_num]);
        if(!sub) break;   // EOF
        if(sub==ERR) ++sub_errs; else ++sub_num; // Error vs. Valid
    }
    
    fclose(fd);

//    printf ("SUB: Subtitle format %s time.\n", sub_uses_time?"uses":"doesn't use");
    printf ("SUB: Read %i subtitles", sub_num);
    if (sub_errs) printf (", %i bad line(s).\n", sub_errs);
    else 	  printf (".\n");

    return first;
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

char * sub_filename(char* path,  char * fname )
{
 extern int sub_utf8;
 char * sub_name1;
 char * sub_name2;
 char * aviptr1, * aviptr2, * tmp;
 int    i,j;
 FILE * f;
 int pos=0;
 char * sub_exts[] = 
  { ".utf",
    ".UTF",
    ".sub",
    ".SUB",
    ".srt",
    ".SRT",
    ".smi",
    ".SMI",
    ".rt",
    ".RT",
    ".txt",
    ".TXT",
    ".ssa",
    ".SSA"};


 if ( fname == NULL ) return NULL;
 
 sub_name1=strrchr(fname,'.');
 if (!sub_name1) return NULL;
 pos=sub_name1-fname;
 
 sub_name1=malloc(strlen(fname)+8);
 strcpy(sub_name1,fname);

 sub_name2=malloc (strlen(path) + strlen(fname) + 8);
 if ((tmp=strrchr(fname,'/')))
	 sprintf (sub_name2, "%s%s", path, tmp+1);
 else
	 sprintf (sub_name2, "%s%s", path, fname);
 
 aviptr1=strrchr(sub_name1,'.');
 aviptr2=strrchr(sub_name2,'.');
 
 for(j=0;j<=1;j++){
  char* sub_name=j?sub_name1:sub_name2;
  for ( i=0;i<(sizeof(sub_exts)/sizeof(char*));i++ ) {
   strcpy(j?aviptr1:aviptr2,sub_exts[i]);
//   printf("trying: '%s'\n",sub_name);
   if((f=fopen( sub_name,"rt" ))) {
     fclose( f );
     printf( "SUB: Detected sub file: %s\n",sub_name );
     if (i<2) sub_utf8=1;
     return sub_name;
   }
  }
 }
 
 return NULL;
}

void list_sub_file(subtitle* subs){
    int i,j;

    for(j=0;j<sub_num;j++){
	subtitle* egysub=&subs[j];
        printf ("%i line%c (%li-%li) ",
		    egysub->lines,
		    (1==egysub->lines)?' ':'s',
		    egysub->start,
		    egysub->end);
	for (i=0; i<egysub->lines; i++) {
	    printf ("%s%s",egysub->text[i], i==egysub->lines-1?"":" <BREAK> ");
	}
	printf ("\n");
    }

    printf ("Subtitle format %s time.\n", sub_uses_time?"uses":"doesn't use");
    printf ("Read %i subtitles, %i errors.\n", sub_num, sub_errs);

}

#if 0
int main(int argc, char **argv) {  // for testing

    int i,j;
    subtitle *subs;
    subtitle *egysub;
    
    if(argc<2){
        printf("\nUsage: subreader filename.sub\n\n");
        exit(1);
    }
    
    subs=sub_read_file(argv[1]);
    if(!subs){
        printf("Couldn't load file... let's write a bugreport :)\n");
        exit(1);
    }
    
    list_sub_file(subs);

    return 0;
}
#endif
