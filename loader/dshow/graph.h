#ifndef MPLAYER_GRAPH_H
#define MPLAYER_GRAPH_H

/*
 * Copyright 2010 Steinar H. Gunderson
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "interfaces.h"
#include "cmediasample.h"

typedef struct FilterGraph FilterGraph;

struct FilterGraph {
    IFilterGraph_vt* vt;
    DECLARE_IUNKNOWN();
    GUID interfaces[2];

    HRESULT STDCALL (*AddFilter)(FilterGraph* This,
                                 /* [in] */ IBaseFilter* pFilter,
                                 /* [string][in] */ unsigned short* pName);
    HRESULT STDCALL (*RemoveFilter)(FilterGraph* This,
                                    /* [in] */ IBaseFilter* pFilter);
    HRESULT STDCALL (*EnumFilters)(FilterGraph* This,
                                   /* [out] */ IEnumFilters** ppEnum);
    HRESULT STDCALL (*FindFilterByName)(FilterGraph* This,
                                        /* [string][in] */ unsigned short* pName,
                                        /* [out] */ IBaseFilter** ppFilter);
    HRESULT STDCALL (*ConnectDirect)(FilterGraph* This,
                                     /* [in] */ IPin* ppinOut,
                                     /* [in] */ IPin* ppinIn,
                                     /* [in] */ const AM_MEDIA_TYPE* pmt);
    HRESULT STDCALL (*Reconnect)(FilterGraph* This,
                                 /* [in] */ IPin* ppin);
    HRESULT STDCALL (*Disconnect)(FilterGraph* This,
                                  /* [in] */ IPin* ppin);
    HRESULT STDCALL (*SetDefaultSyncSource)(FilterGraph* This);
};


HRESULT STDCALL FilterGraph_AddFilter(FilterGraph* This,
                                      IBaseFilter* pFilter,
                                      unsigned short* pName);
HRESULT STDCALL FilterGraph_RemoveFilter(FilterGraph* This,
                                         IBaseFilter* pFilter);
HRESULT STDCALL FilterGraph_EnumFilters(FilterGraph* This,
                                        IEnumFilters** ppEnum);
HRESULT STDCALL FilterGraph_FindFilterByName(FilterGraph* This,
                                             unsigned short* pName,
                                             IBaseFilter** ppFilter);
HRESULT STDCALL FilterGraph_ConnectDirect(FilterGraph* This,
                                          IPin* ppinOut,
                                          IPin* ppinIn,
                                          const AM_MEDIA_TYPE* pmt);
HRESULT STDCALL FilterGraph_Reconnect(FilterGraph* This, IPin* ppin);
HRESULT STDCALL FilterGraph_Disconnect(FilterGraph* This, IPin* ppin);
HRESULT STDCALL FilterGraph_SetDefaultSyncSource(FilterGraph* This);

FilterGraph* FilterGraphCreate(void);

#endif /* MPLAYER_GRAPH_H */
