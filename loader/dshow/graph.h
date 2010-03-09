#ifndef MPLAYER_GRAPH_H
#define MPLAYER_GRAPH_H

#include "interfaces.h"
#include "cmediasample.h"

typedef struct FilterGraph FilterGraph;

struct FilterGraph
{
    IFilterGraph_vt* vt;
    DECLARE_IUNKNOWN();
    GUID interfaces[2];

    HRESULT STDCALL ( *AddFilter )(FilterGraph* This,
                                   /* [in] */ IBaseFilter* pFilter,
                                   /* [string][in] */ unsigned short* pName);
    HRESULT STDCALL ( *RemoveFilter )(FilterGraph* This,
                                      /* [in] */ IBaseFilter* pFilter);
    HRESULT STDCALL ( *EnumFilters )(FilterGraph* This,
                                     /* [out] */ IEnumFilters** ppEnum);
    HRESULT STDCALL ( *FindFilterByName )(FilterGraph* This,
                                          /* [string][in] */ unsigned short* pName,
                                          /* [out] */ IBaseFilter** ppFilter);
    HRESULT STDCALL ( *ConnectDirect )(FilterGraph* This,
                                       /* [in] */ IPin* ppinOut,
                                       /* [in] */ IPin* ppinIn,
                                       /* [in] */ const AM_MEDIA_TYPE* pmt);
    HRESULT STDCALL ( *Reconnect )(FilterGraph* This,
                                   /* [in] */ IPin* ppin);
    HRESULT STDCALL ( *Disconnect )(FilterGraph* This,
                                    /* [in] */ IPin* ppin);
    HRESULT STDCALL ( *SetDefaultSyncSource )(FilterGraph* This);
};

FilterGraph* FilterGraphCreate(void);

#endif /* MPLAYER_GRAPH_H */
