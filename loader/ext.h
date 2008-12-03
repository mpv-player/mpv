/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#ifndef MPLAYER_EXT_H
#define MPLAYER_EXT_H

#include "wine/windef.h"

LPVOID FILE_dommap( int unix_handle, LPVOID start,
                    DWORD size_high, DWORD size_low,
                    DWORD offset_high, DWORD offset_low,
                    int prot, int flags );
int FILE_munmap( LPVOID start, DWORD size_high, DWORD size_low );
int wcsnicmp( const unsigned short* s1, const unsigned short* s2, int n );
int __vprintf( const char *format, ... );

#endif /* MPLAYER_EXT_H */
