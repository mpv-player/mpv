
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"
#include "stream.h"

#include "mf.h"

int    mf_support = 0;
int    mf_w = 352;
int    mf_h = 288;
int    mf_fps = 25;
char * mf_type = "jpg";

int stream_open_mf(char * filename,stream_t * stream)
{
 glob_t        gg;
 struct stat   fs;
 int           i;
 char        * fname;
 mf_t        * mf;

 fname=malloc( strlen( filename ) + 2 );
 strcpy( fname,filename ); strcat( fname,"*" );

 if ( glob( fname,0,NULL,&gg ) )
  { free( fname ); return 0; }

 printf( "[mf] search expr: %s\n",fname );

 mf=malloc( sizeof( mf_t ) );
 mf->nr_of_files=gg.gl_pathc;
 mf->names=malloc( gg.gl_pathc * sizeof( char* ) );

 printf( "[mf] number of files: %d (%d)\n",mf->nr_of_files, gg.gl_pathc * sizeof( char* ) );

 for( i=0;i < gg.gl_pathc;i++ )
  {
   stat( gg.gl_pathv[i],&fs );
   if( S_ISDIR( fs.st_mode ) ) continue;
   mf->names[i]=strdup( gg.gl_pathv[i] );
//   printf( "[mf] added file %d.: %s\n",i,mf->names[i] );
  }
 globfree( &gg );

 free( fname );
 stream->priv=(void*)mf;

 return 1;
}

#if 0

stream_t stream;

int main( void )
{
 stream_open_mf( "tmp/a",&stream );
 return 0;
}

#endif
