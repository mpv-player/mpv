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
#include "aspect.h"
#include "../postproc/swscale.h"
#include "font_load.h"
#include "sub.h"

#include "linux/keycodes.h"
#include <aalib.h>
#include "cfgparser.h"
#include "mp_msg.h"


#define MESSAGE_DURATION 3
#define MESSAGE_SIZE 512
#define MESSAGE_DEKO " +++ %s +++ "

LIBVO_EXTERN(aa)

	static vo_info_t vo_info = {
	    "AAlib",
	    "aa",
	    "Alban Bedel <albeu@free.fr> and Folke Ashberg <folke@ashberg.de>",
	    ""
	};

/* aa's main context we use */
aa_context *c;
aa_renderparams *p;
static int fast =0;
/* used for the sws */
static uint8_t * image[3];
static int image_stride[3];

/* image infos */
static int image_format;
static int image_width;
static int image_height;
static int image_x, image_y;
static int screen_x, screen_y;
static int screen_w, screen_h;
static int src_width;
static int src_height;

/* osd stuff */
time_t stoposd = 0;
static int showosdmessage = 0;
char osdmessagetext[MESSAGE_SIZE];
char posbar[MESSAGE_SIZE];
static int osdx, osdy;
static int osd_text_length = 0;
int aaconfigmode=1;
#ifdef USE_OSD
font_desc_t* vo_font_save = NULL;
#endif
static SwsContext *sws=NULL;
extern m_config_t *mconfig;

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

    aa_resize(c);

    aspect_save_screenres(aa_imgwidth(c),aa_imgheight(c));
    image_height =  aa_imgheight(c); //src_height;
    image_width = aa_imgwidth(c); //src_width;

    aspect(&image_width,&image_height,A_ZOOM);

    image_x = (aa_imgwidth(c) - image_width) / 2;
    image_y = (aa_imgheight(c) - image_height) / 2;
    screen_w = image_width * aa_scrwidth(c) / aa_imgwidth(c);
    screen_h = image_height * aa_scrheight(c) / aa_imgheight(c);
    screen_x = (aa_scrwidth(c) - screen_w) / 2;
    screen_y = (aa_scrheight(c) - screen_h) / 2;
    
    if(sws) freeSwsContext(sws);
    sws = getSwsContextFromCmdLine(src_width,src_height,image_format,
				   image_width,image_height,IMGFMT_Y8);

    image[0] = aa_image(c) + image_y * aa_imgwidth(c) + image_x;
    image[1] = NULL;
    image[2] = NULL;

    image_stride[0] = aa_imgwidth(c);
    image_stride[1] = 0; 
    image_stride[2] = 0;

    showosdmessage=0;

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
    unsigned int old_len = strlen(osdmessagetext);

    va_start(ar, fmt);
    vsprintf(m, fmt, ar);
    va_end(ar);
    if (deko==1) sprintf(osdmessagetext, MESSAGE_DEKO , m);
    else strcpy(osdmessagetext, m);

    if(old_len > strlen(osdmessagetext)) {
      memset(c->textbuffer + osdy * aa_scrwidth(c) + osdx,' ',old_len);
      memset(c->attrbuffer + osdy * aa_scrwidth(c) + osdx,0,old_len);
    }
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
    int i;

    
    step=(float)aa_scrwidth(c) /(float)(max-min);
    where=(val-min)*step;
    osdmessage(duration,deko,"%s: %i%s",desc, val, unit);
    posbar[0]='|';
    posbar[aa_scrwidth(c)-1]='|';
    for (i=0;i<aa_scrwidth(c);i++){
	if (i==where) posbar[i]='#';
	else posbar[i]='-';
    }
    if (where!=0) posbar[0]='|';
    if (where!=(aa_scrwidth(c)-1) ) posbar[aa_scrwidth(c)-1]='|';

    posbar[aa_scrwidth(c)]='\0';
 
}

void
printosdtext()
{
  if(osd_text_length > 0 && !vo_osd_text) {
    memset(c->textbuffer,' ',osd_text_length);
    memset(c->attrbuffer,0,osd_text_length);
    osd_text_length = 0;
  }
    /* 
     * places the mplayer status osd
     */
  if (vo_osd_text && vo_osd_text[0] != 0) {
    int len;
    if(vo_osd_text[0] < 32) {
      len = strlen(__sub_osd_names_short[vo_osd_text[0]]) + strlen(vo_osd_text+1) + 2;
      aa_printf(c, 0, 0 , aaopt_osdcolor, "%s %s ", __sub_osd_names_short[vo_osd_text[0]], vo_osd_text+1);
    } else {
      len = strlen(vo_osd_text) + 1;
      aa_printf(c, 0, 0 , aaopt_osdcolor, "%s ",vo_osd_text);
    }

    if(len < osd_text_length) {
      memset(c->textbuffer + len,' ',osd_text_length - len);
      memset(c->attrbuffer + len,0,osd_text_length - len);
    }
    osd_text_length = len;
    
  }
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

    int i;

    aspect_save_orig(width,height);
    aspect_save_prescale(d_width,d_height);

    src_height = height;
    src_width = width;
    image_format = format;

    /* nothing will change its size, be we need some values initialized */
    resize();

#ifdef USE_OSD
    /* now init out own 'font' (to use vo_draw_text_sub without edit them) */
    if(!vo_font_save) vo_font_save = vo_font;
    if(vo_font == vo_font_save) {
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
      for (i=0; i<255; i++){
	vo_font->width[i]=1;
	vo_font->font[i]=0;
	vo_font->start[i]=i;
	vo_font->pic_a[0]->bmp[i]=i;
	vo_font->pic_b[0]->bmp[i]=i;
      }
    }
#endif
    /* say hello */
    osdmessage(5, 1, "Welcome to ASCII ARTS MPlayer");  

    mp_msg(MSGT_VO,MSGL_V,"VO: [aa] screendriver:   %s\n", c->driver->name);
    mp_msg(MSGT_VO,MSGL_V,"VO: [aa] keyboarddriver: %s\n", c->kbddriver->name);

    mp_msg(MSGT_VO,MSGL_INFO,
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
     * All input format supported by the sws
     */
    switch(format){
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_IYU2:
	case IMGFMT_BGR32:
	case IMGFMT_BGR24:
	case IMGFMT_BGR16:
	case IMGFMT_BGR15:
	case IMGFMT_RGB32:
	case IMGFMT_RGB24:
	case IMGFMT_Y8:
	case IMGFMT_Y800:
	    return VFCAP_CSP_SUPPORTED|VFCAP_SWSCALE
#ifdef USE_OSD
	    | VFCAP_OSD
#endif
	    ;
    }
    return 0;
}

static const vo_info_t* 
get_info(void) {
    /* who i am? */
    return (&vo_info);
}

static uint32_t 
draw_frame(uint8_t *src[]) {
  int stride[3] = { 0 , 0 , 0 };

  switch(image_format) {
  case IMGFMT_BGR15:
  case IMGFMT_BGR16:
    stride[0] = src_width*2;
    break;
  case IMGFMT_IYU2:
  case IMGFMT_BGR24:
    stride[0] = src_width*3;
    break;
  case IMGFMT_BGR32:
    stride[0] = src_width*4;
    break;
  }

  sws->swScale(sws,src,stride,0,src_height,image,image_stride);

   /* Now 'ASCIInate' the image */ 
  if (fast)
    aa_fastrender(c, screen_x, screen_y, screen_w + screen_x, screen_h + screen_y );
  else
    aa_render(c, p,screen_x, screen_y, screen_w + screen_x, screen_h + screen_y );

  return 0;
}

static uint32_t 
draw_slice(uint8_t *src[], int stride[], 
	    int w, int h, int x, int y) {

  int dx1 = screen_x + (x * screen_w / src_width);
  int dy1 = screen_y + (y * screen_h / src_height);
  int dx2 = screen_x + ((x+w) * screen_w / src_width);
  int dy2 = screen_y + ((y+h) * screen_h / src_height);

  sws->swScale(sws,src,stride,y,h,image,image_stride);

  /* Now 'ASCIInate' the image */ 
  if (fast)
    aa_fastrender(c, dx1, dy1, dx2, dy2 );
  else
    aa_render(c, p,dx1, dy1, dx2, dy2 );

  
  return 0;
}

static void 
flip_page(void) {

   /* do we have to put *our* (messages, progbar) osd to aa's txtbuf ? */
    if (showosdmessage)
      {
	if (time(NULL)>=stoposd ) {
	  showosdmessage=0;
	  if(osdmessagetext) {
	    memset(c->textbuffer + osdy * aa_scrwidth(c) + osdx,' ',strlen(osdmessagetext));
	    memset(c->attrbuffer + osdy * aa_scrwidth(c) + osdx ,0,strlen(osdmessagetext));
	    osdmessagetext[0] = '\0';
	  }
	  if(posbar) {
	    memset(c->textbuffer + (osdy+1) * aa_scrwidth(c),' ',strlen(posbar));
	    memset(c->attrbuffer + (osdy+1) * aa_scrwidth(c),0,strlen(posbar));
	  }
	} else {
	  /* update osd */
	  aa_puts(c, osdx, osdy, AA_SPECIAL, osdmessagetext);
	  /* posbar? */
	  if (posbar[0]!='\0')
	    aa_puts(c, 0, osdy + 1, AA_SPECIAL, posbar);
	}
      }
    /* OSD time & playmode , subtitles */
#ifdef USE_OSD
    printosdtext();
#endif


    /* print out */
    aa_flush(c);
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
		    aaconfigmode==1 ? "on. use keys 5-7" : "off");
	}
	if (aaconfigmode==1) {
	    switch (key) {
		/* AA image controls */
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
	m_config_set_flag(mconfig,"quiet",0); /* enable mplayer outputs */
    }
#ifdef USE_OSD
    if(vo_font_save) {
      free(vo_font->pic_a[0]->bmp);
      free(vo_font->pic_a[0]);
      free(vo_font->pic_b[0]->bmp);
      free(vo_font->pic_b[0]);
      free(vo_font);
      vo_font = vo_font_save;
      vo_font_save = NULL;
    }
#endif
    aa_close(c);
}

#ifdef USE_OSD
static void draw_alpha(int x,int y, int w,int h, unsigned char* src, unsigned char *srca, int stride){
 
    c->textbuffer[x + y*aa_scrwidth(c)] = src[0];
    c->attrbuffer[x + y*aa_scrwidth(c)] = aaopt_subcolor;

}

static void clear_alpha(int x0,int y0, int w,int h) {
  int l;

  for(l = 0 ; l < h ; l++) {
    memset(c->textbuffer + (y0 + l) * aa_scrwidth(c) + x0,' ',w);
    memset(c->attrbuffer + (y0 + l) * aa_scrwidth(c) + x0,0,w);
  }
}


#endif

static void
draw_osd(void){
#ifdef USE_OSD
    char * vo_osd_text_save;
    int vo_osd_progbar_type_save;

    printosdprogbar();
    /* let vo_draw_text only write subtitle */
    vo_osd_text_save=vo_osd_text; /* we have to save the osd_text */
    vo_osd_text=NULL;
    vo_osd_progbar_type_save=vo_osd_progbar_type;
    vo_osd_progbar_type=-1;
    vo_remove_text(aa_scrwidth(c), aa_scrheight(c),clear_alpha);
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
  if (!strcasecmp(param, "aaosdcolor"))
    aaopt_osdcolor= AA_SPECIAL;
  else if (!strcasecmp(param, "aasubcolor"))
    aaopt_subcolor= AA_SPECIAL;
}

static uint32_t preinit(const char *arg)
{
    char * hidis = NULL;
    struct stat sbuf;
    int fd, vt, major, minor;
    FILE * fp;
    char fname[12];
    extern aa_linkedlist *aa_displayrecommended;

    if(arg) 
    {
	mp_msg(MSGT_VO,MSGL_ERR,"vo_aa: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }

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
	sprintf (fname, "/dev/vcsa%2.2i", vt);
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

    if (c == NULL) {
	mp_msg(MSGT_VO,MSGL_ERR,"Can not intialize aalib\n");
	return VO_ERROR;
    }   
    if (!aa_autoinitkbd(c,0)) {
	mp_msg(MSGT_VO,MSGL_ERR,"Can not intialize keyboard\n");
	aa_close(c);
	return VO_ERROR;
    }

    aa_resizehandler(c, (void *)resize);
    aa_hidecursor(c);
    p = aa_getrenderparams();

    if ((strstr(c->driver->name,"Curses")) || (strstr(c->driver->name,"Linux"))){
	freopen("/dev/null", "w", stderr);
	m_config_set_flag(mconfig,"quiet",0); /* disable mplayer outputs */
	/* disable console blanking */
	printf("\033[9;0]");
    }

    memset(image,0,3*sizeof(uint8_t));
    osdmessagetext[0] = '\0';
    osdx = osdy = 0;

    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_SET_EQUALIZER: {
    va_list ap;
    int val;

    va_start(ap, data);
    val = va_arg(ap, int);
    va_end(ap);

    if(strcmp((char*)data,"contrast") == 0)
      p->contrast = ( val + 100 ) * 64 / 100;
    else if(strcmp((char*)data,"brightness") == 0)
      p->bright = ( val + 100) * 128 / 100;
    return VO_TRUE;
  }
  case VOCTRL_GET_EQUALIZER: {
    va_list ap;
    int* val;

    va_start(ap, data);
    val = va_arg(ap, int*);
    va_end(ap);

    if(strcmp((char*)data,"contrast") == 0)
      *val = (p->contrast - 64) * 100 / 64;
    else if(strcmp((char*)data,"brightness") == 0)
      *val = (p->bright - 128) * 100 / 128;

    return VO_TRUE;
  }
  }
  return VO_NOTIMPL;
}
