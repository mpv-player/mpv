#ifndef DS_IUNK_H
#define DS_IUNK_H

#include "interfaces.h"
#include "guids.h"

#define DECLARE_IUNKNOWN(CLASSNAME) \
    int refcount; \
    static long STDCALL QueryInterface(IUnknown * This, GUID* riid, void **ppvObject); \
    static long STDCALL AddRef (IUnknown * This); \
    static long STDCALL Release (IUnknown * This); 
    
#define IMPLEMENT_IUNKNOWN(CLASSNAME) 		\
long STDCALL CLASSNAME ::QueryInterface(IUnknown * This, GUID* riid, void **ppvObject) \
{ \
    Debug printf(#CLASSNAME "::QueryInterface() called\n");\
    if (!ppvObject) return 0x80004003; 		\
    CLASSNAME * me = (CLASSNAME *)This;		\
    unsigned int i = 0;				\
    for(const GUID* r=me->interfaces; i<sizeof(CLASSNAME ::interfaces)/sizeof(CLASSNAME ::interfaces[0]); r++, i++) \
	if(!memcmp(r, riid, 16)) 		\
	{ 					\
	    This->vt->AddRef((IUnknown*)This); 	\
	    *ppvObject=This; 			\
	    return 0; 				\
	} 					\
    Debug printf("Failed\n");			\
    return E_NOINTERFACE;			\
} 						\
						\
long STDCALL CLASSNAME ::AddRef ( 		\
    IUnknown * This) 				\
{						\
    Debug printf(#CLASSNAME "::AddRef() called\n"); \
    CLASSNAME * me=( CLASSNAME *)This;		\
    return ++(me->refcount); 			\
}     						\
						\
long STDCALL CLASSNAME ::Release ( 		\
    IUnknown * This) 				\
{ 						\
    Debug printf(#CLASSNAME "::Release() called\n"); \
    CLASSNAME* me=( CLASSNAME *)This;	 	\
    if(--(me->refcount) ==0)			\
		delete ( CLASSNAME *) This; 	\
    return 0; 					\
}

#endif /* DS_IUNK_H */
