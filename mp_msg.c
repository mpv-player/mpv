
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
#include "Gui/mplayer/widgets.h"
extern void gtkMessageBox( int type,char * str );
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
    setlocale(LC_ALL, ""); /* set from the environment variables */
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
    tmp[MSGSIZE_MAX-1] = 0;

#if ENABLE_GUI_CODE
    if(use_gui)
    {
	switch(x & 255)
	{
	    case MSGL_FATAL:
		gtkMessageBox(GTK_MB_FATAL|GTK_MB_SIMPLE, tmp);
		break;
	    case MSGL_ERR:
		gtkMessageBox(GTK_MB_ERROR|GTK_MB_SIMPLE, tmp);
		break;
#if 0
// WARNING! Do NOT enable this! There are too many non-critical messages with
// MSGL_WARN, for example: broken SPU packets, codec's bit error messages,
// etc etc, they should not raise up a new window every time.
	    case MSGL_WARN:
		gtkMessageBox(GTK_MB_WARNING|GTK_MB_SIMPLE, tmp);
		break;
#endif
	}
    }
#endif

#ifdef MSG_USE_COLORS
/* that's only a silly color test */
#ifdef MP_DEBUG
    { int c;
      static int flag=1;
      if(flag)
      for(c=0;c<16;c++)
          printf("\033[%d;3%dm***  COLOR TEST %d  ***\n",(c>7),c&7,c);
      flag=0;
    }
#endif    
    {	unsigned char v_colors[10]={9,9,11,14,15,7,6,5,5,5};
	int c=v_colors[(x & 255)];
	fprintf(((x & 255) <= MSGL_WARN)?stderr:stdout, "\033[%d;3%dm",(c>7),c&7);
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
