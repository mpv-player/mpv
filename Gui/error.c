
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "error.h"

int    debug_level = 6;
FILE * debug_file;
int    debug_stderr = 0;

void defaultErrorHandler( int critical,const char * format, ... )
{
 char    * p;
 va_list   ap;

 if ( (p=(char *)malloc( 512 ) ) == NULL ) return;
 va_start( ap,format );
 vsnprintf( p,512,format,ap );
 va_end( ap );
 fprintf( stderr,"%s",p );
 free( p );
 if ( critical ) exit( 1 );
}

void defaultDebugHandler( int critical,const char * format, ... )
{
 char    * p;
 va_list   ap;

 if ( critical >= debug_level ) return;
 if ( (p=(char *)malloc( 512 ) ) == NULL ) return;
 va_start( ap,format );
 vsnprintf( p,512,format,ap );
 va_end( ap );
 fprintf( debug_file,"%s",p );
 free( p );
}

errorTHandler message = defaultErrorHandler;
errorTHandler dbprintf = defaultDebugHandler;

void initDebug( char * name )
{
 if ( name )
  {
   if ( ( debug_file=fopen( name,"wt+" ) ) != NULL )
    {
     debug_stderr=0;
     return;
    }
  }
 debug_file=stderr;
 debug_stderr=1;
}
void doneDebug( void )
{
 if ( !debug_stderr ) fclose( debug_file );
 debug_file=stderr;
 debug_stderr=1;
}
