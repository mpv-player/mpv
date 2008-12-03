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

#ifdef AVIFILE
#ifdef __GNUC__
#include "avm_output.h"
#ifndef __cplusplus
#define printf(a, ...)  avm_printf("Win32 plugin", a, ## __VA_ARGS__)
#endif
#endif
#endif

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
