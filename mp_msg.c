
//#define MSG_USE_COLORS

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "config.h"

#if	defined(FOR_MENCODER) || defined(CODECS2HTML)
#undef	ENABLE_GUI_CODE
#elif	defined(HAVE_NEW_GUI)
#define	ENABLE_GUI_CODE	HAVE_NEW_GUI
#else
#undef	ENABLE_GUI_CODE
#endif

#if ENABLE_GUI_CODE
#include "Gui/interface.h"
extern int use_gui;
#endif
#include "mp_msg.h"

/* maximum message length of mp_msg */
#define MSGSIZE_MAX 3072

static int mp_msg_levels[MSGT_MAX]; // verbose level of this module

#if 1

void mp_msg_init(){
#ifdef USE_I18N
#ifdef MP_DEBUG
    fprintf(stdout, "Using GNU internationalization\n");
    fprintf(stdout, "Original domain: %s\n", textdomain(NULL));
    fprintf(stdout, "Original dirname: %s\n", bindtextdomain(textdomain(NULL),NULL));
#endif
    bindtextdomain("mplayer", PREFIX"/share/locale");
    textdomain("mplayer");
#ifdef MP_DEBUG
    fprintf(stdout, "Current domain: %s\n", textdomain(NULL));
    fprintf(stdout, "Current dirname: %s\n\n", bindtextdomain(textdomain(NULL),NULL));
#endif
#endif
    mp_msg_set_level(MSGL_STATUS);
}

void mp_msg_set_level(int verbose){
    int i;
    for(i=0;i<MSGT_MAX;i++){
	mp_msg_levels[i]=verbose;
    }
}

int mp_msg_test(int mod, int lev)
{
    return lev <= mp_msg_levels[mod];
}

void mp_msg_c( int x, const char *format, ... ){
#if 1
    va_list va;
    char tmp[MSGSIZE_MAX];
    
    if((x&255)>mp_msg_levels[x>>8]) return; // do not display
    va_start(va, format);
    vsnprintf(tmp, MSGSIZE_MAX, mp_gettext(format), va);
    va_end(va);
    tmp[MSGSIZE_MAX-2] = '\n';
    tmp[MSGSIZE_MAX-1] = 0;

#if ENABLE_GUI_CODE
    if(use_gui)
        guiMessageBox(x&255, tmp);
#endif

#ifdef MSG_USE_COLORS
/* that's only a silly color test */
#ifdef MP_ANNOY_ME
    { int c;
      static int flag=1;
      if(flag)
      for(c=0;c<24;c++)
          printf("\033[%d;3%dm***  COLOR TEST %d  ***\n",(c>7),c&7,c);
      flag=0;
    }
#endif    
    {	unsigned char v_colors[10]={9,1,3,15,7,2,2,8,8,8};
        static const char *lev_text[]= {
                                "FATAL",
                                "ERROR",
                                "WARN",
                                "HINT",
                                "INFO",
                                "STATUS",
                                "V",
                                "DGB2",
                                "DGB3",
                                "DGB4"};
        static const char *mod_text[]= {
                                "GLOBAL",
                                "CPLAYER",
                                "GPLAYER",
                                "VIDEOOUT",
                                "AUDIOOUT",
                                "DEMUXER",
                                "DS",
                                "DEMUX",
                                "HEADER",
                                "AVSYNC",
                                "AUTOQ",
                                "CFGPARSER",
                                "DECAUDIO",
                                "DECVIDEO",
                                "SEEK",
                                "WIN32",
                                "OPEN",
                                "DVD",
                                "PARSEES",
                                "LIRC",
                                "STREAM",
                                "CACHE",
                                "MENCODER",
                                "XACODEC",
                                "TV",
                                "OSDEP",
                                "SPUDEC",
                                "PLAYTREE",
                                "INPUT",
                                "VFILTER",
                                "OSD",
                                "NETWORK",
                                "CPUDETECT",
                                "CODECCFG",
                                "SWS",
                                "VOBSUB",
                                "SUBREADER",
                                "AFILTER",
                                "NETST",
                                "MUXER"};

	int c=v_colors[(x & 255)];
        int c2=((x>>8)+1)%15+1;
        static int header=1;
        FILE *stream= (x & 255) <= MSGL_WARN ? stderr : stdout;
        if(header){
            fprintf(stream, "\033[%d;3%dm%9s\033[0;37m: ",c2>>3,c2&7, mod_text[x>>8]);
        }
        fprintf(stream, "\033[%d;3%dm",c>>3,c&7);
        header=    tmp[strlen(tmp)-1] == '\n'
                 /*||tmp[strlen(tmp)-1] == '\r'*/;
    }
#endif
    if ((x & 255) <= MSGL_WARN){
	fprintf(stderr, "%s", tmp);fflush(stderr);
    } else {
	printf("%s", tmp);fflush(stdout);
    }

#else
    va_list va;
    if((x&255)>mp_msg_levels[x>>8]) return; // do not display
    va_start(va, format);
#if ENABLE_GUI_CODE
    if(use_gui){
      char tmp[16*80];
      vsnprintf( tmp,8*80,format,va ); tmp[8*80-1]=0;
      switch( x&255 ) {
       case MSGL_FATAL: 
              fprintf( stderr,"%s",tmp );
	      fflush(stderr);
              gtkMessageBox( GTK_MB_FATAL|GTK_MB_SIMPLE,tmp );
       	   break;
       case MSGL_ERR:
              fprintf( stderr,"%s",tmp );
	      fflush(stderr);
              gtkMessageBox( GTK_MB_ERROR|GTK_MB_SIMPLE,tmp );
       	   break;
       case MSGL_WARN:
              fprintf( stderr, "%s",tmp );
	      fflush(stdout);
              gtkMessageBox( GTK_MB_WARNING|GTK_MB_SIMPLE,tmp );
       	   break;
       default:
              fprintf(stderr, "%s",tmp );
	      fflush(stdout);
      }
    } else
#endif
    if((x&255)<=MSGL_ERR){
//    fprintf(stderr,"%%%%%% ");
      vfprintf(stderr,format, va);
      fflush(stderr);
    } else {
//	printf("%%%%%% ");
      vfprintf(stderr,format, va);
      fflush(stdout);
    }
    va_end(va);
#endif
}

#else

FILE *mp_msg_file[MSGT_MAX]; // print message to this file (can be stdout/err)
static FILE* mp_msg_last_file=NULL;

// how to handle errors->stderr messages->stdout  ?
void mp_msg( int x, const char *format, ... ){
    if((x&255)>mp_msg_levels[x>>8] || !mp_msg_file[x>>8]) return; // do not display
    va_list va;
    va_start(va, format);
    vfprintf(mp_msg_file[x>>8],format, va);
    if(mp_msg_last_file!=mp_msg_file[x>>8]){
	fflush(mp_msg_file[x>>8]);
	mp_msg_last_file=mp_msg_file[x>>8];
    }
    va_end(va);
}

#endif
