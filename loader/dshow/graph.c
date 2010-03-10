/*
 * Implemention of FilterGraph. Based on allocator.c.
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
 *
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "loader/com.h"
#include "loader/dshow/graph.h"
#include "loader/wine/winerror.h"

// How many FilterGraph objects exist.
// Used for knowing when to register and unregister the class in COM.
static int GraphKeeper = 0;

static long FilterGraph_CreateGraph(GUID* clsid, const GUID* iid, void** ppv)
{
    IUnknown* p;
    int result;
    if (!ppv)
        return -1;
    *ppv = 0;
    if (memcmp(clsid, &CLSID_FilterGraph, sizeof(*clsid)))
        return -1;

    p = (IUnknown*) FilterGraphCreate();
    result = p->vt->QueryInterface(p, iid, ppv);
    p->vt->Release(p);

    return result;
}

static void FilterGraph_Destroy(FilterGraph* This)
{
    Debug printf("FilterGraph_Destroy(%p) called  (%d, %d)\n", This, This->refcount, GraphKeeper);
#ifdef WIN32_LOADER
    if (--GraphKeeper == 0)
        UnregisterComClass(&CLSID_FilterGraph, FilterGraph_CreateGraph);
#endif
    free(This->vt);
    free(This);
}

HRESULT STDCALL FilterGraph_AddFilter(FilterGraph* This,
                                      IBaseFilter* pFilter,
                                      unsigned short* pName)
{
    Debug printf("FilterGraph_AddFilter(%p) called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL FilterGraph_RemoveFilter(FilterGraph* This, IBaseFilter* pFilter)
{
    Debug printf("FilterGraph_RemoveFilter(%p) called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL FilterGraph_EnumFilters(FilterGraph* This, IEnumFilters** ppEnum)
{
    Debug printf("FilterGraph_EnumFilters(%p) called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL FilterGraph_FindFilterByName(FilterGraph* This,
                                             unsigned short* pName,
                                             IBaseFilter** ppFilter)
{
    Debug printf("FilterGraph_FindFilterByName(%p) called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL FilterGraph_ConnectDirect(FilterGraph* This,
                                          IPin* ppinOut,
                                          IPin* ppinIn,
                                          const AM_MEDIA_TYPE* pmt)
{
    Debug printf("FilterGraph_ConnectDirect(%p) called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL FilterGraph_Reconnect(FilterGraph* This, IPin* ppin)
{
    Debug printf("FilterGraph_Reconnect(%p) called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL FilterGraph_Disconnect(FilterGraph* This, IPin* ppin)
{
    Debug printf("FilterGraph_Disconnect(%p) called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL FilterGraph_SetDefaultSyncSource(FilterGraph* This)
{
    Debug printf("FilterGraph_SetDefaultSyncSource(%p) called\n", This);
    return E_NOTIMPL;
}

IMPLEMENT_IUNKNOWN(FilterGraph)

FilterGraph* FilterGraphCreate()
{
    FilterGraph* This = calloc(1, sizeof(*This));

    if (!This)
        return NULL;

    Debug printf("FilterGraphCreate() called -> %p\n", This);

    This->refcount = 1;

    This->vt = calloc(1, sizeof(*This->vt));

    if (!This->vt) {
        free(This);
        return NULL;
    }

    This->vt->QueryInterface       = FilterGraph_QueryInterface;
    This->vt->AddRef               = FilterGraph_AddRef;
    This->vt->Release              = FilterGraph_Release;

    This->vt->AddFilter            = FilterGraph_AddFilter;
    This->vt->RemoveFilter         = FilterGraph_RemoveFilter;
    This->vt->EnumFilters          = FilterGraph_EnumFilters;
    This->vt->FindFilterByName     = FilterGraph_FindFilterByName;
    This->vt->ConnectDirect        = FilterGraph_ConnectDirect;
    This->vt->Reconnect            = FilterGraph_Reconnect;
    This->vt->Disconnect           = FilterGraph_Disconnect;
    This->vt->SetDefaultSyncSource = FilterGraph_SetDefaultSyncSource;

    This->interfaces[0] = IID_IUnknown;
    This->interfaces[1] = IID_IFilterGraph;

#ifdef WIN32_LOADER
    if (GraphKeeper++ == 0)
        RegisterComClass(&CLSID_FilterGraph, FilterGraph_CreateGraph);
#endif

    return This;
}

