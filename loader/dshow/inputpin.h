#ifndef DS_INPUTPIN_H
#define DS_INPUTPIN_H

#include "interfaces.h"

typedef struct _CBaseFilter2 CBaseFilter2;
struct _CBaseFilter2
{
    IBaseFilter_vt* vt;
    IPin* pin;
    GUID interfaces[5];
    DECLARE_IUNKNOWN();

    IPin* ( *GetPin )(CBaseFilter2* This);
};

CBaseFilter2* CBaseFilter2Create();


typedef struct _CBaseFilter CBaseFilter;
struct _CBaseFilter
{
    IBaseFilter_vt* vt;
    IPin* pin;
    IPin* unused_pin;
    GUID interfaces[2];
    DECLARE_IUNKNOWN();

    IPin* ( *GetPin )(CBaseFilter* This);
    IPin* ( *GetUnusedPin )(CBaseFilter* This);
};

CBaseFilter* CBaseFilterCreate(const AM_MEDIA_TYPE* vhdr, CBaseFilter2* parent);


typedef struct _CInputPin CInputPin;
struct _CInputPin
{
    IPin_vt* vt;
    AM_MEDIA_TYPE type;
    CBaseFilter* parent;
    GUID interfaces[1];
    DECLARE_IUNKNOWN();
};

CInputPin* CInputPinCreate(CBaseFilter* parent, const AM_MEDIA_TYPE* vhdr);


typedef struct CRemotePin
{
    IPin_vt* vt;
    CBaseFilter* parent;
    IPin* remote_pin;
    GUID interfaces[1];
    DECLARE_IUNKNOWN();
} CRemotePin;

CRemotePin* CRemotePinCreate(CBaseFilter* pt, IPin* rpin);


typedef struct CRemotePin2
{
    IPin_vt* vt;
    CBaseFilter2* parent;
    GUID interfaces[1];
    DECLARE_IUNKNOWN();
} CRemotePin2;

CRemotePin2* CRemotePin2Create(CBaseFilter2* parent);

#endif /* DS_INPUTPIN_H */
