#ifndef AVIFILE_COM_H
#define AVIFILE_COM_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif

/**
 * Internal functions and structures for COM emulation code.
 */

#ifndef WIN32

#ifdef __cplusplus
extern "C" {
#endif

void* CoTaskMemAlloc(unsigned long cb);
void CoTaskMemFree(void* cb);

#ifndef GUID_TYPE
#define GUID_TYPE
typedef struct
{
    uint32_t f1;
    uint16_t f2;
    uint16_t f3;
    uint8_t  f4[8];
} GUID;
#endif

extern const GUID IID_IUnknown;
extern const GUID IID_IClassFactory;

typedef long (*GETCLASSOBJECT) (GUID* clsid, const GUID* iid, void** ppv);
int RegisterComClass(const GUID* clsid, GETCLASSOBJECT gcs);
int UnregisterComClass(const GUID* clsid, GETCLASSOBJECT gcs);

#ifndef STDCALL
#define STDCALL __attribute__((__stdcall__))
#endif

struct IUnknown;
struct IClassFactory;
struct IUnknown_vt
{
    long STDCALL (*QueryInterface)(struct IUnknown* _this, const GUID* iid, void** ppv);
    long STDCALL (*AddRef)(struct IUnknown* _this) ;
    long STDCALL (*Release)(struct IUnknown* _this) ;
} ;

typedef struct IUnknown
{
    struct IUnknown_vt* vt;
} IUnknown;

struct IClassFactory_vt
{
    long STDCALL (*QueryInterface)(struct IUnknown* _this, const GUID* iid, void** ppv);
    long STDCALL (*AddRef)(struct IUnknown* _this) ;
    long STDCALL (*Release)(struct IUnknown* _this) ;
    long STDCALL (*CreateInstance)(struct IClassFactory* _this, struct IUnknown* pUnkOuter, const GUID* riid, void** ppvObject);
};

struct IClassFactory
{
    struct IClassFactory_vt* vt;
};

long CoCreateInstance(GUID* rclsid, struct IUnknown* pUnkOuter,
		      long dwClsContext, const GUID* riid, void** ppv);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* WIN32 */

#endif /* AVIFILE_COM_H */
