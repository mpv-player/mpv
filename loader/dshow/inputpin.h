#ifndef DS_INPUTPIN_H
#define DS_INPUTPIN_H

#include "interfaces.h"

typedef struct _CBaseFilter2 CBaseFilter2;
struct _CBaseFilter2
{
    IBaseFilter_vt* vt;
    DECLARE_IUNKNOWN();
    IPin* pin;
    GUID interfaces[5];

    IPin* ( *GetPin )(CBaseFilter2* This);
};

CBaseFilter2* CBaseFilter2Create(void);


typedef struct _CBaseFilter CBaseFilter;
struct _CBaseFilter
{
    IBaseFilter_vt* vt;
    DECLARE_IUNKNOWN();  // has to match CBaseFilter2 - INHERITANCE!!
    IPin* pin;
    IPin* unused_pin;
    GUID interfaces[2];

    IPin* ( *GetPin )(CBaseFilter* This);
    IPin* ( *GetUnusedPin )(CBaseFilter* This);
};

CBaseFilter* CBaseFilterCreate(const AM_MEDIA_TYPE* vhdr, CBaseFilter2* parent);


typedef struct
{
    IPin_vt* vt;
    DECLARE_IUNKNOWN();
    CBaseFilter* parent;
    AM_MEDIA_TYPE type;
    GUID interfaces[1];
} CInputPin;

CInputPin* CInputPinCreate(CBaseFilter* parent, const AM_MEDIA_TYPE* vhdr);


typedef struct
{
    IPin_vt* vt;
    DECLARE_IUNKNOWN();
    CBaseFilter* parent;
    GUID interfaces[1];
    IPin* remote_pin;
} CRemotePin;

CRemotePin* CRemotePinCreate(CBaseFilter* pt, IPin* rpin);


typedef struct
{
    IPin_vt* vt;
    DECLARE_IUNKNOWN();
    CBaseFilter2* parent;
    GUID interfaces[1];
} CRemotePin2;

CRemotePin2* CRemotePin2Create(CBaseFilter2* parent);

#endif /* DS_INPUTPIN_H */
