
#ifndef _CUT_H
#define _CUT_H

extern void  cutItem( char * in,char * out,char sep,int num );
extern int   cutItemToInt( char * in,char sep,int num );
extern float cutItemToFloat( char * in,char sep,int num );
extern void  cutChunk( char * in,char * s1 );

#endif
