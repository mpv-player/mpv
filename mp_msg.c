
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "config.h"

#ifdef HAVE_NEW_GUI
#include "Gui/mplayer/widgets.h"
extern void gtkMessageBox( int type,char * str );
extern int use_gui;
#endif

#include "mp_msg.h"

static int mp_msg_levels[MSGT_MAX]; // verbose level of this module

#if 1

void mp_msg_init(int verbose){
    int i;
    for(i=0;i<MSGT_MAX;i++){
	mp_msg_levels[i]=verbose;
    }
}

void mp_msg_c( int x, const char *format, ... ){
    va_list va;
    char tmp[128];
    if((x&255)>mp_msg_levels[x>>8]) return; // do not display
    va_start(va, format);
    vsnprintf( tmp,128,format,va );
/*    
    if((x&255)<=MSGL_ERR){
//	fprintf(stderr,"%%%%%% ");
	vfprintf(stderr,format, va);
    } else {
//	printf("%%%%%% ");
	vprintf(format, va);
    }
*/    
    va_end(va);
    switch( x&255 )
     {
       case MSGL_FATAL: 
              fprintf( stderr,"%s",tmp );
              #ifdef HAVE_NEW_GUI
               if ( use_gui ) gtkMessageBox( GTK_MB_FATAL|GTK_MB_SIMPLE,tmp );
              #endif
       	   break;
       case MSGL_ERR:
              fprintf( stderr,"%s",tmp );
              #ifdef HAVE_NEW_GUI 
               if ( use_gui ) gtkMessageBox( GTK_MB_ERROR|GTK_MB_SIMPLE,tmp );
              #endif
       	   break;
       case MSGL_WARN:
              printf( "%s",tmp );
              #ifdef HAVE_NEW_GUI 
               if ( use_gui ) gtkMessageBox( GTK_MB_WARNING|GTK_MB_SIMPLE,tmp );
              #endif
       	   break;
       default:
              printf( "%s",tmp );
       	   break;
     }
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
