/*
 * MPlayer
 * 
 * Video driver for AAlib - alpha version
 * 
 * by Folke Ashberg <folke@ashberg.de>
 * 
 * Code started: Sun Aug 12 2001
 *
 */

#include <stdio.h>
#include <stdlib.h>


#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "yuv2rgb.h"
#include "sub.h"

#include "linux/keycodes.h"
#include <aalib.h>
#include "cfgparser.h"

#define RGB 0
#define BGR 1

#define DO_INC(val,max,step) if (val + step <=max) val+=step; else val=max;  
#define DO_DEC(val,min,step) if (val - step >=min) val-=step; else val=min;  

#define MESSAGE_DURATION 3
#define MESSAGE_SIZE 512
#define MESSAGE_DEKO " +++ %s +++ "



LIBVO_EXTERN(aa)

	static vo_info_t vo_info = {
	    "AAlib",
	    "aa",
	    "Folke Ashberg <folke@ashberg.de>",
	    ""
	};

/* aa's main context we use */
aa_context *c;
aa_renderparams *p;
static int fast =0;
/* used for YV12 streams for the converted RGB image */
uint8_t * convertbuf=NULL;

/* image infos */
static int image_format, bpp=24;
static int image_width;
static int image_height;
static int bppmul;

/* osd stuff */
time_t stoposd = 0;
static int showosd = 0;
char osdtext[MESSAGE_SIZE];
char posbar[MESSAGE_SIZE];
static int osdx, osdy;

/* for resizing/scaling */
static int *stx;
static int *sty;
double accum;

/* our version of the playmodes :) */
static char * osdmodes[] ={ "|>", "||", "[]", "<<" , ">>" };

extern void mplayer_put_key(int code);

/* to disable stdout outputs when curses/linux mode */
extern int quiet;

/* configuration */
int aaopt_osdcolor = AA_SPECIAL;
int aaopt_subcolor = AA_SPECIAL;

extern struct aa_hardware_params aa_defparams;
extern struct aa_renderparams aa_defrenderparams;

void
resize(void){
    /* 
     * this function is called by aa lib if windows resizes
     * further during init, because here we have to calculate
     * a little bit
     */

    int i;
    aa_resize(c);

    showosd=0;
    osdy=aa_scrheight(c) - ( aa_scrheight(c)/10 );

    /* now calculating the needed values for resizing */

    /* We only need to use floating point to determine the correct
       stretch vector for one line's worth. */
    stx = (int *) malloc(sizeof(int) * image_width);
    sty = (int *) malloc(sizeof(int) * image_height);
    accum = 0;
    for (i=0; (i < image_width); i++) {
	int got;
	accum += (double)aa_imgwidth(c)/(double)image_width;
	got = (int) floor(accum);
	stx[i] = got;
	accum -= got;
    }
    accum = 0;
    for (i=0; (i < image_height); i++) {
	int got;
	accum += (double)aa_imgheight(c)/(double)image_height;
	got = (int) floor(accum);
	sty[i] = got;
	accum -= got;
    }
}

void
osdmessage(int duration, int deko, char *fmt, ...)
{
    /*
     * for outputting a centered string at the bottom
     * of our window for a while
     */
    va_list ar;
    char m[MESSAGE_SIZE];
    va_start(ar, fmt);
    vsprintf(m, fmt, ar);
    va_end(ar);
    if (deko==1) sprintf(osdtext, MESSAGE_DEKO , m);
    else strcpy(osdtext, m);
    showosd=1;
    stoposd = time(NULL) + duration;
    osdx=(aa_scrwidth(c) / 2) - (strlen(osdtext) / 2 ) ;
    posbar[0]='\0';
}

void
osdpercent(int duration, int deko, int min, int max, int val, char * desc, char * unit)
{
    /*
     * prints a bar for setting values
     */
    float step;
    int where;
    char m[MESSAGE_SIZE];
    int i;
    step=(float)aa_scrwidth(c) /(float)(max-min);
    where=(val-min)*step;
    sprintf(m,"%s: %i%s",desc, val, unit);
    if (deko==1) sprintf(osdtext, MESSAGE_DEKO , m);
    else strcpy(osdtext, m);
    posbar[0]='|';
    posbar[aa_scrwidth(c)-1]='|';
    for (i=0;i<aa_scrwidth(c);i++){
	if (i==where) posbar[i]='#';
	else posbar[i]='-';
    }
    if (where!=0) posbar[0]='|';
    if (where!=(aa_scrwidth(c)-1) ) posbar[aa_scrwidth(c)-1]='|';
    /* snipp */
    posbar[aa_scrwidth(c)]='\0';
    showosd=1;
    stoposd = time(NULL) + duration;
    osdx=(aa_scrwidth(c) / 2) - (strlen(osdtext) / 2 ) ;
}

static uint32_t
init(uint32_t width, uint32_t height, uint32_t d_width,
	    uint32_t d_height, uint32_t fullscreen, char *title, 
	    uint32_t format) {
    /*
     * main init
     * called by mplayer
     */

    switch(format) {
	case IMGFMT_BGR24:
	    bpp = 24;
	    break;     
	case IMGFMT_RGB24:
	    bpp = 24;
	    break;     
	case IMGFMT_YV12:
	    bpp = 24;
	    /* YUV ? then initialize what we will need */
	    convertbuf=malloc(width*height*3);
	    yuv2rgb_init(24,MODE_BGR);
	    break;
	default:
	    return 1;     
    }
    bppmul=bpp/8;
    

    /* initializing of aalib */
    /*
    TODO check /dev/vcsa
    aa_recommendhidisplay("curses");
    aa_recommendhidisplay("X11");
    aa_recommendlowdisplay("linux");
    */
    
    c = aa_autoinit(&aa_defparams);
    aa_resizehandler(c, (void *)resize);

    if (c == NULL) {
	printf("Can not intialize aalib\n");
	return 0;
    }   
    if (!aa_autoinitkbd(c,0)) {
	printf("Can not intialize keyboard\n");
	aa_close(c);
	return 0;
    }
    /*
    if (!aa_autoinitmouse(c,0)) {
	printf("Can not intialize mouse\n");
	aa_close(c);
	return 0;
    }
    */
    aa_hidecursor(c);
    p = aa_getrenderparams();

    if ((strstr(c->driver->name,"Curses")) || (strstr(c->driver->name,"Linux"))){
	freopen("/dev/null", "w", stderr);
	quiet=1; /* disable mplayer outputs */
    }
    
    image_height = height;
    image_width = width;
    image_format = format;

    /* needed by prepare_image */
    stx = (int *) malloc(sizeof(int) * image_width);
    sty = (int *) malloc(sizeof(int) * image_height);

    /* nothing will change its size, be we need some values initialized */
    resize();

    /* say hello */
    osdmessage(5, 1, "Welcome to ASCII ARTS MPlayer");  

    printf("VO: [aa] screendriver:   %s\n", c->driver->name);
    printf("VO: [aa] keyboarddriver: %s\n", c->kbddriver->name);
    //printf("VO: mousedriver:    %s\n", c->mousedriver->name);

    printf(
		"\n"
		"Important Options\n"
		"\t-aaextended  use use all 256 characters\n"
		"\t-aaeight     use eight bit ascii\n"
		"\t-aadriver    set recommended aalib driver (X11,curses,linux)\n"
		"\t-aahelp      to see all options provided by aalib\n"
		"\n"
		"AA-MPlayer Keys:\n"
		"\t1 : fast rendering\n"
		"\t2 : dithering\n"
		"\t3 : invert image\n"
		"\t4 : contrast -\n"
		"\t5 : contrast +\n"
		"\t6 : brightness -\n"
		"\t7 : brightness +\n"
		"\n"
		"All other keys are MPlayer defaults.\n"


	  );

    return 0;
}

static uint32_t 
query_format(uint32_t format) {
    /*
     * ...are we able to... ?
     * called by mplayer
     */
    switch(format){
	case IMGFMT_YV12:
	case IMGFMT_RGB|24:
	case IMGFMT_BGR|24:
	    return 1;
    }
    return 0;
}

static const vo_info_t* 
get_info(void) {
    /* who i am? */
    return (&vo_info);
}

int
prepare_image(uint8_t *data, int inx, int iny, int outx, int outy){
    /*
     * copies an RGB-Image to the aalib imagebuffer
     * also scaling an grayscaling is done here
     * show_image calls us
     */

    int value;
    int x, y;
    int tox, toy;
    int ydest;
    int i;
    int pos;

    toy = 0;
    for (y=0; (y < (0 + iny)); y++) {
	for (ydest=0; (ydest < sty[y-0]); ydest++) {
	    tox = 0;
	    for (x=0; (x < (0 + inx)); x++) {
		if (!stx[x - 0]) {
		    continue;
		}
		pos=3*(inx*y)+(3*x);
		value=(data[pos]+data[pos+1]+data[pos+2])/3;
		for (i=0; (i < stx[x - 0]); i++) {
		    //printf("ToX: %i, ToY %i, i=%i, stx=%i, x=%i\n", tox, toy, i, stx[x], x);
		    c->imagebuffer[(toy*outx) +tox]=value;
		    tox++;
		}
	    }
	    toy++;
	}
    }
    return 0;
}

void
printosd()
{
    /* 
     * places the mplayer status osd
     */
    if (vo_osd_text){
	if (vo_osd_text[0]-1<=5)
	  aa_puts(c, 0,0, aaopt_osdcolor, osdmodes[vo_osd_text[0]-1]);
	else aa_puts(c, 0,0, aaopt_osdcolor, "?");
	aa_puts(c,2,0, aaopt_osdcolor, vo_osd_text+1);
	aa_puts(c,strlen(vo_osd_text)+1,0, aaopt_osdcolor, " ");
    }
}

void 
show_image(uint8_t * src){
    /*
     * every frame (flip_page/draw_frame) we will be called
     */

    /* events? */
    check_events();

    /* RGB->gray , scaling/resizing, stores data in aalib imgbuf */ 
    prepare_image( src, image_width, image_height,
		aa_imgwidth(c), aa_imgheight(c) );

    /* Now 'ASCIInate' the image */ 
    if (fast)
      aa_fastrender(c, 0, 0, aa_scrwidth(c), aa_scrheight(c) );
    else
      aa_render(c, p, 0, 0, aa_scrwidth(c),  aa_scrheight(c));

    /* do we have to put our osd to aa's txtbuf ? */
    if (showosd)
      {
	if (time(NULL)>=stoposd ) showosd=0;
	/* update osd */
	aa_puts(c, osdx, osdy, AA_SPECIAL, osdtext);
	/* posbar? */
	if (posbar[0]!='\0')
	  aa_puts(c, 0, osdy + 1, AA_SPECIAL, posbar);
      }
    /* and the real OSD, but only the time & playmode */
    printosd();

    /* print out */
    aa_flush(c);
}

static uint32_t 
draw_frame(uint8_t *src[]) {
    /*
     * RGB-Video's Only
     * src[0] is handled bu prepare_image
     */
    show_image(src[0]);
    return 0;
}

static uint32_t 
draw_slice(uint8_t *src[], int stride[], 
	    int w, int h, int x, int y) {
    /*
     * for MPGEGS YV12
     * draw a rectangle converted to RGB to a
     * temporary RGB Buffer
     */
    uint8_t *dst;

    dst = convertbuf+(image_width * y + x) * 3;
    yuv2rgb(dst,src[0],src[1],src[2],w,h,image_width*3,stride[0],stride[1]);

    return 0;
}

static void 
flip_page(void) {
    /*
     * wow! another ready Image, so draw it !
     */
    if(image_format == IMGFMT_YV12)
      show_image(convertbuf);
}

static void 
check_events(void) {
    /* 
     * any events?
     * called by show_image and mplayer
     */
    int key;
    while ((key=aa_getevent(c,0))!=AA_NONE ){
	if (key>255){
	    /* some conversations */
	    switch (key) {
		case AA_UP:
		    mplayer_put_key(KEY_UP);
		    break;
		case AA_DOWN:
		    mplayer_put_key(KEY_DOWN);
		    break;
		case AA_LEFT:
		    mplayer_put_key(KEY_LEFT);
		    break;
		case AA_RIGHT:
		    mplayer_put_key(KEY_RIGHT);
		    break;
		case AA_ESC:
		    mplayer_put_key(KEY_ESC);
		    break;
		case 65765:
		    mplayer_put_key(KEY_PAGE_UP);
		    break;
		case 65766:
		    mplayer_put_key(KEY_PAGE_DOWN);
		    break;
		default:
		    continue; /* aa lib special key */
		    break;
	    }
	}
	switch (key) {
	    /* AA image controls */
	    case '1':
		fast=!fast;
		osdmessage(MESSAGE_DURATION, 1, "Fast mode is now %s", fast==1 ? "on" : "off");
		break;
	    case '2':
		if (p->dither==AA_FLOYD_S){
		    p->dither=AA_NONE;
		    osdmessage(MESSAGE_DURATION, 1, "Dithering: Off");
		}else if (p->dither==AA_NONE){
		    p->dither=AA_ERRORDISTRIB;
		    osdmessage(MESSAGE_DURATION, 1, "Dithering: Error Distribution");
		}else if (p->dither==AA_ERRORDISTRIB){
		    p->dither=AA_FLOYD_S;
		    osdmessage(MESSAGE_DURATION, 1, "Dithering: Floyd Steinberg");
		}
		break;
	    case '3':
		p->inversion=!p->inversion;
		osdmessage(MESSAGE_DURATION, 1, "Invert mode is now %s",
			    p->inversion==1 ? "on" : "off");
		break;

	    case '4':		/* contrast */
		DO_DEC(p->contrast,0,1);
		osdpercent(MESSAGE_DURATION, 1, 0, 255, p->contrast, "AA-Contrast", "");

		break;
	    case '5':		/* contrast */
		DO_INC(p->contrast,255,1);
		osdpercent(MESSAGE_DURATION, 1, 0, 255, p->contrast, "AA-Contrast", "");
		break;
	    case '6':		/* brightness */
		DO_DEC(p->bright,0,1);
		osdpercent(MESSAGE_DURATION, 1, 0, 255, p->bright, "AA-Brightnes", "");
		break;
	    case '7':		/* brightness */
		DO_INC(p->bright,255,1);
		osdpercent(MESSAGE_DURATION, 1, 0, 255, p->bright, "AA-Brightnes", "");
		break;

	    default :
		/* nothing if we're interested in?
		 * the mplayer should handle it!
		 */
		mplayer_put_key(key);
		break;
	}
    }
}

static void 
uninit(void) {
    /*
     * THE END
     */ 
    aa_close(c);
    free(stx);
    free(sty);
    if (convertbuf!=NULL) free(convertbuf);
    if (strstr(c->driver->name,"Curses") || strstr(c->driver->name,"Linux")){
	freopen("/dev/tty", "w", stderr);
	quiet=0; /* enable mplayer outputs */
    }
}

static void
draw_osd(void){
}

int
getcolor(char * s){
    int i;
    char * rest;
    if  (s==NULL) return -1;
    i=strtol(s, &rest, 10);
    if ((rest==NULL || strlen(rest)==0) && i>=0 && i<=5) return i;
    if (!strcasecmp(s, "normal")) return AA_NORMAL;
    else if (!strcasecmp(s, "dim")) return AA_DIM;
    else if (!strcasecmp(s, "bold")) return AA_BOLD;
    else if (!strcasecmp(s, "boldfont")) return AA_BOLDFONT;
    else if (!strcasecmp(s, "special")) return AA_SPECIAL;
    else return -1;
}

int
vo_aa_parseoption(struct config * conf, char *opt, char *param){
    /* got an option starting with aa */
    char *pseudoargv[3];
    int pseudoargc;
    char * x, *help;
    int i;
    /* do WE need it ? */
    if (!strcasecmp(opt, "aaosdcolor")){
	if (param==NULL) return ERR_MISSING_PARAM;
	if ((i=getcolor(param))==-1) return ERR_OUT_OF_RANGE;
	aaopt_osdcolor=i;
	return 1;
    }else if (!strcasecmp(opt, "aasubcolor")){
	if ((i=getcolor(param))==-1) return ERR_OUT_OF_RANGE;
	aaopt_subcolor=i;
	return 1;
    }else if (!strcasecmp(opt, "aahelp")){
	printf("Here are the aalib options:\n");
	help=strdup(aa_help); /* aa_help is const :( */
	x=strtok(help,"-");
	printf(x);
	while (x=strtok(NULL, "-")){
	    if (*(x-2)==' ') printf("-aa");
	      else printf("-");
	    printf("%s", x);
	}
	printf(
		    "\n"
		    "\n"
		    "Additional options vo_aa provides:\n"
		    "  -aaosdcolor    set osd color\n"
		    "  -aasubcolor    set subtitle color\n"
		    "        the color params are:\n"
		    "           0 : normal\n"
		    "           1 : dark\n"
		    "           2 : bold\n"
		    "           3 : boldfont\n"
		    "           4 : reverse\n"
		    "           6 : special\n"
		    "\n\n"
		    "           dT8  8Tb\n"
                    "          dT 8  8 Tb\n"
                    "         dT  8  8  Tb\n"
                    "      <PROJECT><PROJECT>\n"
                    "       dT    8  8    Tb\n"
                    "      dT     8  8     Tb\n"
		    "\n"

	      );
	exit(0);
		
    }else{
	/* parse param to aalib */
	pseudoargv[1]=malloc(strlen(opt));
	pseudoargv[3]=NULL;
	sprintf(pseudoargv[1], "-%s", opt+2);
	if (param!=NULL){
	    pseudoargv[2]=param;
	    pseudoargc=3;
	}else{
	    pseudoargv[2]=NULL;
	    pseudoargc=2;
	}
	fprintf(stderr,"VO: [aa] ");
	i=aa_parseoptions(&aa_defparams, &aa_defrenderparams, &pseudoargc, pseudoargv);
	if (i!=1){
	    return ERR_MISSING_PARAM;
	}
	if (pseudoargv[1]!=NULL){
	    /* aalib has given param back */
	    fprintf(stderr," Parameter -%s accepted\n", opt, param);
	    return 0; /* param could be the filename */
	}
	fprintf(stderr," Parameter -%s %s accepted\n", opt, param==NULL ? "" : param);
	return 1; /* all opt & params accepted */

    }
    return ERR_NOT_AN_OPTION;
		
}
