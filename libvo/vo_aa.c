/*
 * MPlayer
 * 
 * Video driver for AAlib - 1.0
 * 
 * by Folke Ashberg <folke@ashberg.de>
 * 
 * Code started: Sun Aug 12 2001
 * Version 1.0 : Thu Aug 16 2001
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <unistd.h>

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "../postproc/rgb2rgb.h"
#include "font_load.h"
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
static int showosdmessage = 0;
char osdmessagetext[MESSAGE_SIZE];
char posbar[MESSAGE_SIZE];
static int osdx, osdy;
int aaconfigmode=1;
/* for resizing/scaling */
static int *stx;
static int *sty;
double accum;
#ifdef USE_OSD
char * osdbuffer=NULL;
#endif

/* our version of the playmodes :) */

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

    showosdmessage=0;
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
#ifdef USE_OSD
    if (osdbuffer!=NULL) free(osdbuffer);
    osdbuffer=malloc(aa_scrwidth(c) * aa_scrheight(c));
#endif
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
    if (deko==1) sprintf(osdmessagetext, MESSAGE_DEKO , m);
    else strcpy(osdmessagetext, m);
    showosdmessage=1;
    stoposd = time(NULL) + duration;
    osdx=(aa_scrwidth(c) / 2) - (strlen(osdmessagetext) / 2 ) ;
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
    if (deko==1) sprintf(osdmessagetext, MESSAGE_DEKO , m);
    else strcpy(osdmessagetext, m);
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
    showosdmessage=1;
    stoposd = time(NULL) + duration;
    osdx=(aa_scrwidth(c) / 2) - (strlen(osdmessagetext) / 2 ) ;
}

void
printosdtext()
{
    /* 
     * places the mplayer status osd
     */
    if (vo_osd_text)
	aa_printf(c, 0, 0 , aaopt_osdcolor, "%s %s ", __sub_osd_names_short[vo_osd_text[0]], vo_osd_text+1);
}

void
printosdprogbar(){
    /* print mplayer osd-progbar */
    if (vo_osd_progbar_type!=-1){
	osdpercent(1,1,0,255,vo_osd_progbar_value, __sub_osd_names[vo_osd_progbar_type], "");	
    }
}
static uint32_t
config(uint32_t width, uint32_t height, uint32_t d_width,
	    uint32_t d_height, uint32_t fullscreen, char *title, 
	    uint32_t format,const vo_tune_info_t *info) {
    /*
     * main init
     * called by mplayer
     */
    FILE * fp;
    char fname[12];
    int fd, vt, major, minor;
    struct stat sbuf;
    char * hidis = NULL;
    int i;
    extern aa_linkedlist *aa_displayrecommended;

    switch(format) {
	case IMGFMT_BGR24:
	    bpp = 24;
	    break;     
	case IMGFMT_RGB24:
	    bpp = 24;
	    break;     
	case IMGFMT_BGR32:
	    bpp = 32;
	    break;     
	case IMGFMT_RGB32:
	    bpp = 32;
	    break;     
	case IMGFMT_IYUV:
	case IMGFMT_I420:
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
    
    hidis=aa_getfirst(&aa_displayrecommended); 
    if ( hidis==NULL ){
	/* check /dev/vcsa<vt> */
	/* check only, if no driver is explicit set */
	fd = dup (fileno (stderr));
	fstat (fd, &sbuf);
	major = sbuf.st_rdev >> 8;
	vt = minor = sbuf.st_rdev & 0xff;
	close (fd);
	sprintf (fname, "/dev/vcsa%i", vt);
	fp = fopen (fname, "w+");
	if (fp==NULL){
	    fprintf(stderr,"VO: [aa] cannot open %s for writing,"
			"so we'll not use linux driver\n", fname);
    	    aa_recommendlowdisplay("linux");
    	    aa_recommendhidisplay("curses");
    	    aa_recommendhidisplay("X11");
	}else fclose(fp);
    } else aa_recommendhidisplay(hidis);
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
	/* disable console blanking */
	printf("\033[9;0]");
    }
    
    image_height = height;
    image_width = width;
    image_format = format;

    /* needed by prepare_image */
    stx = (int *) malloc(sizeof(int) * image_width);
    sty = (int *) malloc(sizeof(int) * image_height);

    /* nothing will change its size, be we need some values initialized */
    resize();

#ifdef USE_OSD
    /* now init out own 'font' (to use vo_draw_text_sub without edit them) */
    vo_font=malloc(sizeof(font_desc_t));//if(!desc) return NULL;
    memset(vo_font,0,sizeof(font_desc_t));
    vo_font->pic_a[0]=malloc(sizeof(raw_file));
    vo_font->pic_b[0]=malloc(sizeof(raw_file));

    vo_font->spacewidth=1;
    vo_font->charspace=0;
    vo_font->height=1;
    vo_font->pic_a[0]->bmp=malloc(255);
    vo_font->pic_b[0]->bmp=malloc(255);
    vo_font->pic_a[0]->w=1;
    vo_font->pic_a[0]->h=1;
    for (i=1; i<256; i++){
	vo_font->width[i]=1;
	vo_font->font[i]=0;
	vo_font->start[i]=i;
	vo_font->pic_a[0]->bmp[i]=i;
	vo_font->pic_b[0]->bmp[i]=i;
    };
#endif
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
		"AA-MPlayer Keys\n"
		"\t1 : contrast -\n"
		"\t2 : contrast +\n"
		"\t3 : brightness -\n"
		"\t4 : brightness +\n"
		"\t5 : fast rendering\n"
		"\t6 : dithering\n"
		"\t7 : invert image\n"
	        "\ta : toggles between aa and mplayer control\n"

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
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
//	case IMGFMT_RGB32:
//	case IMGFMT_BGR32:
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
show_image(uint8_t * src){
    /*
     * every frame (flip_page/draw_frame) we will be called
     */
#ifdef USE_OSD
    int i;
#endif

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

    /* do we have to put *our* (messages, progbar) osd to aa's txtbuf ? */
    if (showosdmessage)
      {
	if (time(NULL)>=stoposd ) showosdmessage=0;
	/* update osd */
	aa_puts(c, osdx, osdy, AA_SPECIAL, osdmessagetext);
	/* posbar? */
	if (posbar[0]!='\0')
	  aa_puts(c, 0, osdy + 1, AA_SPECIAL, posbar);
      }
    /* OSD time & playmode , subtitles */
#ifdef USE_OSD
    printosdtext();
    /* now write the subtitle osd buffer */
    for (i=0;i<aa_scrwidth(c)*aa_scrheight(c);i++){
	if (osdbuffer[i]){
	    c->textbuffer[i]=osdbuffer[i];
	    c->attrbuffer[i]=aaopt_subcolor;
	}
    }
#endif

    /* print out */
    aa_flush(c);
}

static uint32_t 
draw_frame(uint8_t *src[]) {
    /*
     * RGB-Video's Only
     * src[0] is handled by prepare_image
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
    if ((image_format == IMGFMT_IYUV) || (image_format == IMGFMT_I420))
    {
	uint8_t *src_i420[3];
	
	src_i420[0] = src[0];
	src_i420[1] = src[2];
	src_i420[2] = src[1];
	src = src_i420;
    }

    yuv2rgb(dst,src[0],src[1],src[2],w,h,image_width*3,stride[0],stride[1]);

    return 0;
}

static void 
flip_page(void) {
    /*
     * wow! another ready Image, so draw it !
     */
    if(image_format == IMGFMT_YV12 || image_format == IMGFMT_IYUV || image_format == IMGFMT_I420)
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
	if (key=='a' || key=='A'){
	    aaconfigmode=!aaconfigmode;
	    osdmessage(MESSAGE_DURATION, 1, "aa config mode is now %s",
		    aaconfigmode==1 ? "on. use keys 1-7" : "off");
	}
	if (aaconfigmode==1) {
	    switch (key) {
		/* AA image controls */
		case '1':		/* contrast */
		    DO_DEC(p->contrast,0,1);
		    osdpercent(MESSAGE_DURATION, 1, 0, 255, p->contrast, "AA-Contrast", "");
		    break;
		case '2':		/* contrast */
		    DO_INC(p->contrast,255,1);
		    osdpercent(MESSAGE_DURATION, 1, 0, 255, p->contrast, "AA-Contrast", "");
		    break;
		case '3':		/* brightness */
		    DO_DEC(p->bright,0,1);
		    osdpercent(MESSAGE_DURATION, 1, 0, 255, p->bright, "AA-Brightnes", "");
		    break;
		case '4':		/* brightness */
		    DO_INC(p->bright,255,1);
		    osdpercent(MESSAGE_DURATION, 1, 0, 255, p->bright, "AA-Brightnes", "");
		    break;
		case '5':
		    fast=!fast;
		    osdmessage(MESSAGE_DURATION, 1, "Fast mode is now %s", fast==1 ? "on" : "off");
		    break;
		case '6':
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
		case '7':
		    p->inversion=!p->inversion;
		    osdmessage(MESSAGE_DURATION, 1, "Invert mode is now %s",
				p->inversion==1 ? "on" : "off");
		    break;

		default :
		    /* nothing if we're interested in?
		     * the mplayer should handle it!
		     */
		    mplayer_put_key(key);
		    break;
	    }
	}// aaconfigmode
	else mplayer_put_key(key);
    }
}

static void 
uninit(void) {
    /*
     * THE END
     */ 
    if (strstr(c->driver->name,"Curses") || strstr(c->driver->name,"Linux")){
	freopen("/dev/tty", "w", stderr);
	quiet=0; /* enable mplayer outputs */
    }
#ifdef USE_OSD
    if (osdbuffer!=NULL) free(osdbuffer);
#endif
    aa_close(c);
    free(stx);
    free(sty);
    if (convertbuf!=NULL) free(convertbuf);
}

#ifdef USE_OSD
static void draw_alpha(int x,int y, int w,int h, unsigned char* src, unsigned char *srca, int stride){
    /* alpha, hm, grr, only the char into our osdbuffer */
    int pos;
    pos=(x)+(y)*(aa_scrwidth(c));
    osdbuffer[pos]=src[0];
}



#endif

static void
draw_osd(void){
#ifdef USE_OSD
    /* 
     * the subtiles are written into a own osdbuffer
     * because draw_osd is called after show_image/flip_page
     * the osdbuffer is written the next show_image/flip_page
     * into aatextbuf
     */
    char * vo_osd_text_save;
    int vo_osd_progbar_type_save;

    memset(osdbuffer,0,aa_scrwidth(c)*aa_scrheight(c));
    printosdprogbar();
    /* let vo_draw_text only write subtitle */
    vo_osd_text_save=vo_osd_text; /* we have to save the osd_text */
    vo_osd_text=NULL;
    vo_osd_progbar_type_save=vo_osd_progbar_type;
    vo_osd_progbar_type=-1;
    vo_draw_text(aa_scrwidth(c), aa_scrheight(c), draw_alpha);
    vo_osd_text=vo_osd_text_save;
    vo_osd_progbar_type=vo_osd_progbar_type_save;
#endif
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
    char *pseudoargv[4];
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
	while ((x=strtok(NULL, "-"))){
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
	    fprintf(stderr," Parameter -%s accepted\n", opt);
	    return 0; /* param could be the filename */
	}
	fprintf(stderr," Parameter -%s %s accepted\n", opt, ((param==NULL) ? "" : param) );
	return 1; /* all opt & params accepted */

    }
    return ERR_NOT_AN_OPTION;
		
}

void
vo_aa_revertoption(config_t* opt,char* param) {
  if (!strcasecmp(opt, "aaosdcolor"))
    aaopt_osdcolor= AA_SPECIAL;
  else if (!strcasecmp(opt, "aasubcolor"))
    aaopt_subcolor= AA_SPECIAL;
}

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	printf("vo_aa: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }
    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
