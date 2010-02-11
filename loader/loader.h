/********************************************************

	Win32 binary loader interface
	Copyright 2000 Eugene Kuznetsov (divx@euro.ru)
	Shamelessly stolen from Wine project

*********************************************************/

/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#ifndef MPLAYER_LOADER_H
#define MPLAYER_LOADER_H

#include "wine/windef.h"
#include "wine/driver.h"
#include "wine/mmreg.h"
#include "wine/vfw.h"
#include "wine/msacm.h"

unsigned int GetPrivateProfileIntA_(const char* appname, const char* keyname, int default_value, const char* filename);
int GetPrivateProfileStringA_(const char* appname, const char* keyname,
	const char* def_val, char* dest, unsigned int len, const char* filename);
int WritePrivateProfileStringA_(const char* appname, const char* keyname,
	const char* string, const char* filename);

INT WINAPI LoadStringA( HINSTANCE instance, UINT resource_id,
                            LPSTR buffer, INT buflen );

#endif /* MPLAYER_LOADER_H */
