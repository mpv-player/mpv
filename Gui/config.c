
#include <stdlib.h>
#include <stdio.h>

unsigned char * cfgAppName = "movieplayer";
unsigned char * cfgSkin = NULL;

void cfgDefaults( void )
{
 if ( ( cfgSkin=(char *)calloc( 1,256 ) ) == NULL )
  {
   fprintf( stderr,"[config] Not enough memory.\n" );
   exit( 1 );
  }
 strcpy( cfgSkin,"default" );
// strcpy( cfgSkin,"blueHeart" );
}

int cfgRead( void )
{
 return 0;
}
