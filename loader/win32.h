#ifndef loader_win32_h
#define loader_win32_h

#include <time.h>

#include <wine/windef.h>
#include <wine/winbase.h>
#include <com.h>

extern void* my_mreq(int size, int to_zero);
extern int my_release(void* memory);
extern int my_size(void* memory);
extern void* my_realloc(void *memory,int size);
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

extern int WINAPI ext_unknown(void);

extern int WINAPI expIsBadWritePtr(void* ptr, unsigned int count);
extern int WINAPI expIsBadReadPtr(void* ptr, unsigned int count);
extern int WINAPI expDisableThreadLibraryCalls(int module);
extern HMODULE WINAPI expGetDriverModuleHandle(DRVR* pdrv);
extern HMODULE WINAPI expGetModuleHandleA(const char* name);
extern void* WINAPI expCreateThread(void* pSecAttr, long dwStackSize, void* lpStartAddress,
				    void* lpParameter, long dwFlags, long* dwThreadId);
extern void* WINAPI expCreateEventA(void* pSecAttr, char bManualReset,
				    char bInitialState, const char* name);
extern void* WINAPI expSetEvent(void* event);
extern void* WINAPI expResetEvent(void* event);
extern void* WINAPI expWaitForSingleObject(void* object, int duration);
extern WIN_BOOL WINAPI expIsProcessorFeaturePresent(DWORD v);
extern void WINAPI expGetSystemInfo(SYSTEM_INFO* si);
extern long WINAPI expGetVersion(void);
long WINAPI expGetVersionExA(OSVERSIONINFOA* c);
extern HANDLE WINAPI expHeapCreate(long flags, long init_size, long max_size);
extern void* WINAPI expHeapAlloc(HANDLE heap, int flags, int size);
extern long WINAPI expHeapDestroy(void* heap);
extern long WINAPI expHeapFree(int arg1, int arg2, void* ptr);
extern void* WINAPI expHeapReAlloc(HANDLE heap,int flags,void* lpMem,int size);
extern long WINAPI expHeapSize(int heap, int flags, void* pointer);
extern long WINAPI expGetProcessHeap(void);
extern void* WINAPI expVirtualAlloc(void* v1, long v2, long v3, long v4);
extern int WINAPI expVirtualFree(void* v1, int v2, int v3);
extern void WINAPI expInitializeCriticalSection(CRITICAL_SECTION* c);
extern void WINAPI expEnterCriticalSection(CRITICAL_SECTION* c);
extern void WINAPI expLeaveCriticalSection(CRITICAL_SECTION* c);
extern void WINAPI expDeleteCriticalSection(CRITICAL_SECTION *c);
extern int WINAPI expGetCurrentThreadId(void);
extern int WINAPI expGetCurrentProcess(void);
extern void* WINAPI expTlsAlloc(void);
extern int WINAPI expTlsSetValue(tls_t* index, void* value);
extern void* WINAPI expTlsGetValue(tls_t* index);
extern int WINAPI expTlsFree(tls_t* index);
extern void* WINAPI expLocalAlloc(int flags, int size);
extern void* WINAPI expLocalReAlloc(int handle,int size,int flags);
extern void* WINAPI expLocalLock(void* z);
extern void* WINAPI expGlobalAlloc(int flags, int size);
extern void* WINAPI expGlobalLock(void* z);
extern int WINAPI expLoadStringA(long instance, long  id, void* buf, long size);
extern long WINAPI expMultiByteToWideChar(long v1, long v2, char* s1, long siz1, short* s2, int siz2);
extern long WINAPI expWideCharToMultiByte(long v1, long v2, short* s1, long siz1, char* s2, int siz2, char* c3, int* siz3);
extern long WINAPI expGetVersionExA(OSVERSIONINFOA* c);
extern HANDLE WINAPI expCreateSemaphoreA(char* v1, long init_count, long max_count, char* name);
extern long WINAPI expReleaseSemaphore(long hsem, long increment, long* prev_count);
extern long WINAPI expRegOpenKeyExA(long key, const char* subkey, long reserved, long access, int* newkey);
extern long WINAPI expRegCloseKey(long key);

extern long WINAPI expRegQueryValueExA(long key, const char* value, int* reserved, int* type, int* data, int* count);
extern long WINAPI expRegCreateKeyExA(long key, const char* name, long reserved,
				      void* classs, long options, long security,
				      void* sec_attr, int* newkey, int* status);
extern long WINAPI expRegSetValueExA(long key, const char* name, long v1, long v2, void* data, long size);
extern long WINAPI expRegOpenKeyA (long hKey, LPCSTR lpSubKey, int* phkResult);
extern long WINAPI expQueryPerformanceCounter(long long* z);
extern long WINAPI expQueryPerformanceFrequency(long long* z);
extern long WINAPI exptimeGetTime(void);
extern void* WINAPI expLocalHandle(void* v);
extern void* WINAPI expGlobalHandle(void* v);
extern int WINAPI expGlobalUnlock(void* v);
extern void* WINAPI expGlobalFree(void* v);
extern void* WINAPI expGlobalReAlloc(void* v, int size, int flags);
extern int WINAPI expLocalUnlock(void* v);
extern void* WINAPI expLocalFree(void* v);
extern HRSRC WINAPI expFindResourceA(HMODULE module, char* name, char* type);
extern HGLOBAL WINAPI expLoadResource(HMODULE module, HRSRC res);
extern void* WINAPI expLockResource(long res);
extern int WINAPI expFreeResource(long res);
extern int WINAPI expCloseHandle(long v1);
extern const char* WINAPI expGetCommandLineA(void);
extern LPWSTR WINAPI expGetEnvironmentStringsW(void);
extern void * WINAPI expRtlZeroMemory(void *p, size_t len);
extern void * WINAPI expRtlMoveMemory(void *dst, void *src, size_t len);
extern void * WINAPI expRtlFillMemory(void *p, int ch, size_t len);
extern int WINAPI expFreeEnvironmentStringsW(short* strings);
extern int WINAPI expFreeEnvironmentStringsA(char* strings);
extern LPWSTR WINAPI expGetEnvironmentStringsW(void);
LPCSTR WINAPI expGetEnvironmentStrings(void);
extern int WINAPI expGetStartupInfoA(STARTUPINFOA *s);
extern int WINAPI expGetStdHandle(int z);
extern int WINAPI expGetFileType(int handle);
extern int WINAPI expSetHandleCount(int count);
extern int WINAPI expGetACP(void);
extern int WINAPI expGetModuleFileNameA(int module, char* s, int len);
extern int WINAPI expSetUnhandledExceptionFilter(void* filter);
extern int WINAPI expLoadLibraryA(char* name);
extern int WINAPI expFreeLibrary(int module);
extern void* WINAPI expGetProcAddress(HMODULE mod, char* name);
extern long WINAPI expCreateFileMappingA(int hFile, void* lpAttr,
					 long flProtect, long dwMaxHigh, long dwMaxLow, const char* name);
extern long WINAPI expOpenFileMappingA(long hFile, long hz, const char* name);
extern void* WINAPI expMapViewOfFile(HANDLE file, DWORD mode, DWORD offHigh, DWORD offLow, DWORD size);
extern void* WINAPI expUnmapViewOfFile(void* view);
extern void* WINAPI expSleep(int time);
extern void* WINAPI expCreateCompatibleDC(int hdc);
extern int WINAPI expGetDeviceCaps(int hdc, int unk);
extern WIN_BOOL WINAPI expDeleteDC(int hdc);
extern int WINAPI expGetPrivateProfileIntA(const char* appname, const char* keyname, int default_value, const char* filename);
extern int WINAPI expGetProfileIntA(const char* appname, const char* keyname, int default_value);
extern int WINAPI expGetPrivateProfileStringA(const char* appname, const char* keyname,
					      const char* def_val, char* dest, unsigned int len, const char* filename);
extern int WINAPI expWritePrivateProfileStringA(const char* appname, const char* keyname,
						const char* string, const char* filename);
extern int WINAPI expDefDriverProc(int _private, int id, int msg, int arg1, int arg2);
extern int WINAPI expSizeofResource(int v1, int v2);
extern int WINAPI expGetLastError(void);
extern void WINAPI expSetLastError(int error);
extern long WINAPI exptimeGetTime(void);
extern int WINAPI expStringFromGUID2(GUID* guid, char* str, int cbMax);
extern int WINAPI expGetFileVersionInfoSizeA(const char* name, int* lpHandle);
extern int WINAPI expIsBadStringPtrW(const short* string, int nchars);
extern int WINAPI expIsBadStringPtrA(const char* string, int nchars);
extern long WINAPI expInterlockedIncrement( long* dest );
extern long WINAPI expInterlockedDecrement( long* dest );
extern void WINAPI expOutputDebugStringA( const char* string );
extern int WINAPI expGetDC(int hwnd);
extern int WINAPI expGetDesktopWindow(void);
extern int WINAPI expReleaseDC(int hwnd, int hdc);
extern int WINAPI expLoadCursorA(int handle,LPCSTR name);
extern int WINAPI expSetCursor(void *cursor);
extern int WINAPI expGetCursorPos(void *cursor);
extern int WINAPI expRegisterWindowMessageA(char *message);
extern int WINAPI expGetProcessVersion(int pid);
extern int WINAPI expGetCurrentThread(void);
extern int WINAPI expGetOEMCP(void);
extern int WINAPI expGetCPInfo(int cp,void *info);
extern int WINAPI expGetSysColor(int pid);
extern int WINAPI expGetSysColorBrush(int pid);
extern int WINAPI expGetSystemMetrics(int index);
extern int WINAPI expGetSystemPaletteEntries(int hdc, int iStartIndex, int nEntries, void* lppe);
extern int WINAPI expGetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation);
extern void WINAPI expGetLocalTime(SYSTEMTIME* systime);
extern int WINAPI expGetSystemTime(SYSTEMTIME* systime);
extern int WINAPI expGetEnvironmentVariableA(const char* name, char* field, int size);
extern void* WINAPI expCoTaskMemAlloc(ULONG cb);
extern void WINAPI expCoTaskMemFree(void* cb);
extern long WINAPI expCoCreateInstance(GUID* rclsid, struct IUnknown* pUnkOuter,
				       long dwClsContext, GUID* riid, void** ppv);
extern int WINAPI expIsRectEmpty(CONST RECT *lprc);
extern unsigned int WINAPI expGetTempPathA(unsigned int len, char* path);
extern HANDLE WINAPI expFindFirstFileA(LPCSTR s, LPWIN32_FIND_DATAA lpfd);
extern WIN_BOOL WINAPI expFindNextFileA(HANDLE h,LPWIN32_FIND_DATAA p);
extern WIN_BOOL WINAPI expFindClose(HANDLE h);
extern UINT WINAPI expSetErrorMode(UINT i);
extern UINT      WINAPI expGetWindowsDirectoryA(LPSTR s,UINT c);
extern WIN_BOOL  WINAPI expDeleteFileA(LPCSTR s);
extern WIN_BOOL  WINAPI expFileTimeToLocalFileTime(const FILETIME* cpf, LPFILETIME pf);
extern UINT WINAPI expGetTempFileNameA(LPCSTR cs1,LPCSTR cs2,UINT i,LPSTR ps);
extern HANDLE WINAPI expCreateFileA(LPCSTR cs1,DWORD i1,DWORD i2,
				    LPSECURITY_ATTRIBUTES p1, DWORD i3,DWORD i4,HANDLE i5);
extern LPCSTR WINAPI expGetSystemDirectoryA(void);
extern WIN_BOOL WINAPI expReadFile(HANDLE h,LPVOID pv,DWORD size,LPDWORD rd,LPOVERLAPPED unused);
extern WIN_BOOL WINAPI expWriteFile(HANDLE h,LPCVOID pv,DWORD size,LPDWORD wr,LPOVERLAPPED unused);
extern DWORD  WINAPI expSetFilePointer(HANDLE h, LONG val, LPLONG ext, DWORD whence);
extern HDRVR WINAPI expOpenDriverA(LPCSTR szDriverName, LPCSTR szSectionName,
				   LPARAM lParam2);
HDRVR WINAPI expOpenDriver(LPCSTR szDriverName, LPCSTR szSectionName,
			   LPARAM lParam2) ;
extern WIN_BOOL WINAPI expGetProcessAffinityMask(HANDLE hProcess,
						 LPDWORD lpProcessAffinityMask,
						 LPDWORD lpSystemAffinityMask);
extern  DWORD WINAPI expRegEnumValueA( HKEY hkey, DWORD index, LPSTR value, LPDWORD val_count,
				       LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD count );
extern INT WINAPI expMulDiv(int nNumber,int nNumerator,int nDenominator);
extern LONG WINAPI explstrcmpiA(const char* str1, const char* str2);
extern LONG WINAPI explstrlenA(const char* str1);
extern LONG WINAPI explstrcpyA(char* str1, const char* str2);
extern LONG WINAPI explstrcpynA(char* str1, const char* str2,int len);
extern LONG WINAPI explstrcatA(char* str1, const char* str2);
extern LONG WINAPI expInterlockedExchange(long *dest, long l);


extern void* CDECL expmalloc(int size);
extern void CDECL expfree(void* mem);
extern void* CDECL expnew(int size);
extern int CDECL expdelete(void* memory);
extern int CDECL exp_initterm(int v1, int v2);

extern int expwsprintfA(char* string, char* format, ...);
extern char* expstrrchr(char* string, int value);
extern char* expstrchr(char* string, int value);
extern int expstrlen(char* str);
extern int expstrcpy(char* str1, const char* str2);
extern int expstrcmp(const char* str1, const char* str2);
extern int expstrcat(char* str1, const char* str2);
extern int expisalnum(int c);
extern int expmemmove(void* dest, void* src, int n);
extern int expmemcmp(void* dest, void* src, int n);
extern void *expmemcpy(void* dest, void* src, int n) ;
extern time_t exptime(time_t* t);
extern int expsprintf(char* str, const char* format, ...);
extern int expsscanf(const char* str, const char* format, ...);
extern void* expfopen(const char* path, const char* mode);


extern void* LookupExternal(const char* library, int ordinal);
extern void* LookupExternalByName(const char* library, const char* name);

extern int exprand();
extern int exp_ftol(float f);
extern void WINAPI expInitCommonControls();

#endif
