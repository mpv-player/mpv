#ifndef loader_win32_h
#define loader_win32_h

#include <time.h>

#include <wine/windef.h>
#include <wine/winbase.h>
#include <com.h>

extern void my_garbagecollection(void);

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


extern void* LookupExternal(const char* library, int ordinal);
extern void* LookupExternalByName(const char* library, const char* name);

extern void* my_mreq(int size, int to_zero);
extern int my_release(void* memory);


#endif
