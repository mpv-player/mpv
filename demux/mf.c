/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>

#include "osdep/io.h"

#include "config.h"

#ifdef HAVE_GLOB
#include <glob.h>
#else
#include "osdep/glob.h"
#endif

#include "core/mp_msg.h"
#include "stream/stream.h"
#include "core/path.h"

#include "mf.h"

double mf_fps = 25.0;
char * mf_type = NULL; //"jpg";

mf_t* open_mf_pattern(char * filename)
{
#if defined(HAVE_GLOB) || defined(__MINGW32__)
 glob_t        gg;
 int           i;
 char        * fname = NULL;
 mf_t        * mf;
 int           error_count = 0;
 int	       count = 0;

 mf=calloc( 1,sizeof( mf_t ) );

 if( filename[0] == '@' )
  {
   FILE *lst_f=fopen(filename + 1,"r");
   if ( lst_f )
    {
     fname=malloc(MP_PATH_MAX);
     while ( fgets( fname,MP_PATH_MAX,lst_f ) )
      {
       /* remove spaces from end of fname */
       char *t=fname + strlen( fname ) - 1;
       while ( t > fname && isspace( *t ) ) *(t--)=0;
       if ( !mp_path_exists( fname ) )
        {
         mp_msg( MSGT_STREAM,MSGL_V,"[mf] file not found: '%s'\n",fname );
        }
        else
        {
         mf->names=realloc( mf->names,( mf->nr_of_files + 1 ) * sizeof( char* ) );
         mf->names[mf->nr_of_files]=strdup( fname );
         mf->nr_of_files++;
        }
      }
      fclose( lst_f );

      mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] number of files: %d\n",mf->nr_of_files );
      goto exit_mf;
    }
    mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] %s is not indirect filelist\n",filename+1 );
  }

 if( strchr( filename,',') )
  {
   mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] filelist: %s\n",filename );
   bstr bfilename = bstr0(filename);

   while (bfilename.len)
    {
     bstr bfname;
     bstr_split_tok(bfilename, ",", &bfname, &bfilename);
     char *fname = bstrdup0(NULL, bfname);

     if ( !mp_path_exists( fname ) )
      {
       mp_msg( MSGT_STREAM,MSGL_V,"[mf] file not found: '%s'\n",fname );
      }
      else
      {
       mf->names=realloc( mf->names,( mf->nr_of_files + 1 ) * sizeof( char* ) );
       mf->names[mf->nr_of_files] = strdup(fname);
//       mp_msg( MSGT_STREAM,MSGL_V,"[mf] added file %d.: %s\n",mf->nr_of_files,mf->names[mf->nr_of_files] );
       mf->nr_of_files++;
      }
      talloc_free(fname);
    }
   mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] number of files: %d\n",mf->nr_of_files );

   goto exit_mf;
  }

 fname=malloc( strlen( filename ) + 32 );

 if ( !strchr( filename,'%' ) )
  {
   strcpy( fname,filename );
   if ( !strchr( filename,'*' ) ) strcat( fname,"*" );

   mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] search expr: %s\n",fname );

   if ( glob( fname,0,NULL,&gg ) )
    { free( mf ); free( fname ); return NULL; }

   mf->nr_of_files=0;
   mf->names=calloc( gg.gl_pathc, sizeof( char* ) );

   for( i=0;i < gg.gl_pathc;i++ )
    {
     if (mp_path_isdir(gg.gl_pathv[i]))
       continue;
     mf->names[mf->nr_of_files]=strdup( gg.gl_pathv[i] );
     mf->nr_of_files++;
//     mp_msg( MSGT_STREAM,MSGL_DBG2,"[mf] added file %d.: %s\n",i,mf->names[i] );
    }
   mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] number of files: %d\n",mf->nr_of_files);
   globfree( &gg );
   goto exit_mf;
  }

 mp_msg( MSGT_STREAM,MSGL_INFO,"[mf] search expr: %s\n",filename );

 while ( error_count < 5 )
  {
   sprintf( fname,filename,count++ );
   if ( !mp_path_exists( fname ) )
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
 return mf;
#else
 mp_msg(MSGT_STREAM,MSGL_FATAL,"[mf] mf support is disabled on your os\n");
 return 0;
#endif
}

mf_t* open_mf_single(char * filename)
{
    mf_t *mf = calloc(1, sizeof(mf_t));
    mf->nr_of_files = 1;
    mf->names = calloc(1, sizeof(char *));
    mf->names[0] = strdup(filename);
    return mf;
}
