#ifndef DS_IUNK_H
#define DS_IUNK_H

#include "guids.h"

#define INHERIT_IUNKNOWN() \
    long STDCALL ( *QueryInterface )(IUnknown * This, const GUID* riid, void **ppvObject); \
    long STDCALL ( *AddRef )(IUnknown * This); \
    long STDCALL ( *Release )(IUnknown * This);

#define DECLARE_IUNKNOWN() \
    int refcount;

#define IMPLEMENT_IUNKNOWN(CLASSNAME) 		\
static long STDCALL CLASSNAME ## _QueryInterface(IUnknown * This, \
					  const GUID* riid, void **ppvObject) \
{ \
    CLASSNAME * me = (CLASSNAME *)This;		\
    GUID* r; unsigned int i = 0;		\
    Debug printf(#CLASSNAME "_QueryInterface(%p) called\n", This);\
    if (!ppvObject) return E_POINTER; 		\
    for(r=me->interfaces; i<sizeof(me->interfaces)/sizeof(me->interfaces[0]); r++, i++) \
	if(!memcmp(r, riid, sizeof(*r)))	\
	{ 					\
	    me->vt->AddRef((IUnknown*)This); 	\
	    *ppvObject=This; 			\
	    return 0; 				\
	} 					\
    Debug printf("Query failed! (GUID: 0x%x)\n", *(unsigned int*)riid); \
    return E_NOINTERFACE;			\
} 						\
						\
static long STDCALL CLASSNAME ## _AddRef(IUnknown * This) \
{						\
    CLASSNAME * me=( CLASSNAME *)This;		\
    Debug printf(#CLASSNAME "_AddRef(%p) called (ref:%d)\n", This, me->refcount); \
    return ++(me->refcount); 			\
}     						\
						\
static long STDCALL CLASSNAME ## _Release(IUnknown * This) \
{ 						\
    CLASSNAME* me=( CLASSNAME *)This;	 	\
    Debug printf(#CLASSNAME "_Release(%p) called (new ref:%d)\n", This, me->refcount - 1); \
    if(--(me->refcount) == 0)			\
		CLASSNAME ## _Destroy(me); 	\
    return 0; 					\
}

#endif /* DS_IUNK_H */
