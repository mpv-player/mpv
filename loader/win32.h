/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#ifndef MPLAYER_WIN32_H
#define MPLAYER_WIN32_H

#include <time.h>

#include "wine/windef.h"
#include "wine/winbase.h"
#include "com.h"

void my_garbagecollection(void);

typedef struct {
    UINT             uDriverSignature;
    HINSTANCE        hDriverModule;
    DRIVERPROC       DriverProc;
    DWORD            dwDriverID;
} DRVR;

typedef DRVR  *PDRVR;
typedef DRVR  *NPDRVR;
typedef DRVR  *LPDRVR;

typedef struct tls_s tls_t;


void* LookupExternal(const char* library, int ordinal);
void* LookupExternalByName(const char* library, const char* name);

#endif /* MPLAYER_WIN32_H */
