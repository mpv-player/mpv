/**
 * Internal functions and structures for COM emulation code.
 */

#ifndef COM_H
#define COM_H

#ifdef __cplusplus
extern "C" {
#endif

void* CoTaskMemAlloc(unsigned long cb);
void CoTaskMemFree(void* cb);

typedef struct
{
    long f1;
    short f2;
    short f3;
    char f4[8];
} GUID;

extern GUID IID_IUnknown;
extern GUID IID_IClassFactory;

typedef long (*GETCLASSOBJECT) (GUID* clsid, GUID* iid, void** ppv);
int RegisterComClass(GUID* clsid, GETCLASSOBJECT gcs);

#ifndef STDCALL
#define STDCALL __attribute__((__stdcall__))	
#endif

struct IUnknown;
struct IClassFactory;
struct IUnknown_vt
{
    long STDCALL (*QueryInterface)(struct IUnknown* _this, GUID* iid, void** ppv);
    long STDCALL (*AddRef)(struct IUnknown* _this) ;
    long STDCALL (*Release)(struct IUnknown* _this) ;
} ;
struct IUnknown
{
    struct IUnknown_vt* vt;
};

struct IClassFactory_vt
{
    long STDCALL (*QueryInterface)(struct IUnknown* _this, GUID* iid, void** ppv);
    long STDCALL (*AddRef)(struct IUnknown* _this) ;
    long STDCALL (*Release)(struct IUnknown* _this) ;
    long STDCALL (*CreateInstance)(struct IClassFactory* _this, struct IUnknown* pUnkOuter, GUID* riid, void** ppvObject);
};

struct IClassFactory
{
    struct IClassFactory_vt* vt;
};

long CoCreateInstance(GUID* rclsid, struct IUnknown* pUnkOuter,
                    long dwClsContext, GUID* riid, void** ppv);

#ifdef __cplusplus
};
#endif

#endif

