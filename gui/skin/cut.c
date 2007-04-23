
#include <string.h>
#include <stdlib.h>

void cutItem( char * in,char * out,char sep,int num )
{
 int i,n,c;
 for ( c=0,n=0,i=0;i<strlen( in );i++ )
  {
   if ( in[i] == sep ) n++;
   if ( n >= num && in[i] != sep ) out[c++]=in[i];
   if ( n >= num && in[i+1] == sep ) { out[c]=0; return; }
  }
 out[c]=0;
}

int cutItemToInt( char * in,char sep,int num )
{
 char tmp[512];
 cutItem( in,tmp,sep,num ); 
 return atoi( tmp );
}

float cutItemToFloat( char * in,char sep,int num )
{
 char tmp[512];
 cutItem( in,tmp,sep,num ); 
 return atof( tmp );
}

void cutChunk( char * in,char * s1 )
{
 cutItem( in,s1,'=',0 );
 memmove( in,strchr( in,'=' )+1,strlen( in ) - strlen( s1 ) );
}

