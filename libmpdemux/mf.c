
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
float  mf_fps = 25.0;
char * mf_type = "jpg";

int stream_open_mf(char * filename,stream_t * stream)
{
 glob_t        gg;
 struct stat   fs;
 int           i;
 char        * fname;
 mf_t        * mf;
 int           error_count = 0;
 int	       count = 0;

 mf=calloc( 1,sizeof( mf_t ) );

 if( strchr( filename,',') )
  { 
   fname=malloc( 255 ); 
   mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] filelist: %s\n",filename );
 
   while ( ( fname=strsep( &filename,"," ) ) )
    {
     if ( stat( fname,&fs ) ) 
      {
       mp_msg( MSGT_STREAM,MSGL_V,"[mf] file not found: '%s'\n",fname );
      }
      else
      {
       mf->names=realloc( mf->names,( mf->nr_of_files + 1 ) * sizeof( char* ) );
       mf->names[mf->nr_of_files]=strdup( fname );
//       mp_msg( MSGT_STREAM,MSGL_V,"[mf] added file %d.: %s\n",mf->nr_of_files,mf->names[mf->nr_of_files] );
       mf->nr_of_files++;
      }
    }
   goto exit_mf;
  } 

 fname=malloc( strlen( filename ) + 32 );

 mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] number of files: %d\n",mf->nr_of_files );
 
 if ( !strchr( filename,'%' ) )
  {
   strcpy( fname,filename ); 
   if ( !strchr( filename,'*' ) ) strcat( fname,"*" );

   mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] search expr: %s\n",fname );

   if ( glob( fname,0,NULL,&gg ) )
    { free( mf ); free( fname ); return 0; }

   mf->nr_of_files=gg.gl_pathc;
   mf->names=malloc( gg.gl_pathc * sizeof( char* ) );

   mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] number of files: %d (%d)\n",mf->nr_of_files, gg.gl_pathc * sizeof( char* ) );

   for( i=0;i < gg.gl_pathc;i++ )
    {
     stat( gg.gl_pathv[i],&fs );
     if( S_ISDIR( fs.st_mode ) ) continue;
     mf->names[i]=strdup( gg.gl_pathv[i] );
//     mp_msg( MSGT_STREAM,MSGL_DBG2,"[mf] added file %d.: %s\n",i,mf->names[i] );
    }
   globfree( &gg );
   goto exit_mf;
  }

 mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] search expr: %s\n",filename );
 
 while ( error_count < 5 )
  {
   sprintf( fname,filename,count++ );
   if ( stat( fname,&fs ) ) 
    {
     error_count++;
     mp_msg( MSGT_STREAM,MSGL_V,"[mf] file not found: '%s'\n",fname );
    }
    else
    {
     mf->names=realloc( mf->names,( mf->nr_of_files + 1 ) * sizeof( char* ) );
     mf->names[mf->nr_of_files]=strdup( fname );
//     mp_msg( MSGT_STREAM,MSGL_V,"[mf] added file %d.: %s\n",mf->nr_of_files,mf->names[mf->nr_of_files] );
     mf->nr_of_files++;
    }
  }

 mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] number of files: %d\n",mf->nr_of_files );

exit_mf:
 free( fname );
 stream->priv=(void*)mf;
 return 1;
}

