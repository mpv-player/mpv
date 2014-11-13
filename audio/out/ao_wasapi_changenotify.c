/*
 * This file is part of mpv.
 *
 * Original author: Jonathan Yong <10walls@gmail.com>
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#define COBJMACROS 1
#define _WIN32_WINNT 0x600

#include <initguid.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wchar.h>
#include <stdlib.h>

#include "ao_wasapi.h"
#include "ao_wasapi_utils.h"

static int GUID_compare(const GUID *l, const GUID *r)
{
    unsigned int i;
    if (l->Data1 != r->Data1) return 1;
    if (l->Data2 != r->Data2) return 1;
    if (l->Data3 != r->Data3) return 1;
    for (i = 0; i < 8; i++) {
        if (l->Data4[i] != r->Data4[i]) return 1;
    }
    return 0;
}

static int PKEY_compare(const PROPERTYKEY *l, const PROPERTYKEY *r)
{
    if (GUID_compare(&l->fmtid, &r->fmtid)) return 1;
    if (l->pid != r->pid) return 1;
    return 0;
}

static char *GUID_to_str_buf(char *buf, size_t buf_size, const GUID *guid)
{
    snprintf(buf, buf_size,
             "{%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x}",
             guid->Data1, guid->Data2, guid->Data3,
             guid->Data4[0], guid->Data4[1],
             guid->Data4[2], guid->Data4[3],
             guid->Data4[4], guid->Data4[5],
             guid->Data4[6], guid->Data4[7]);
    return buf;
}

static char *PKEY_to_str_buf(char *buf, size_t buf_size, const PROPERTYKEY *pkey)
{
    buf = GUID_to_str_buf(buf, buf_size, &pkey->fmtid);
    size_t guid_len = strnlen(buf, buf_size);
    snprintf(buf + guid_len, buf_size - guid_len, ",%"PRIu32, (uint32_t)pkey->pid );
    return buf;
}

#define PKEY_to_str(pkey) PKEY_to_str_buf((char[42]){0}, 42, (pkey))

static char* ERole_to_str(ERole role)
{
    switch(role){
    case eConsole:        return "console";
    case eMultimedia:     return "multimedia";
    case eCommunications: return "communications";
    default:              return "<Unknown>";
    }
}

static char* EDataFlow_to_str(EDataFlow flow)
{
    switch(flow){
    case eRender:  return "render";
    case eCapture: return "capture";
    case eAll:     return "all";
    default:       return "<Unknown>";
    }
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_QueryInterface(
    IMMNotificationClient* This, REFIID riid, void **ppvObject)
{
    /* Compatible with IMMNotificationClient and IUnknown */
    if (!GUID_compare(&IID_IMMNotificationClient, riid) ||
        !GUID_compare(&IID_IUnknown, riid)) {
        *ppvObject = (void *)This;
        return S_OK;
    } else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
}

/* these are required, but not actually used */
static ULONG STDMETHODCALLTYPE sIMMNotificationClient_AddRef(
    IMMNotificationClient *This)
{
    return 1;
}

/* MSDN says it should free itself, but we're static */
static ULONG STDMETHODCALLTYPE sIMMNotificationClient_Release(
    IMMNotificationClient *This)
{
    return 1;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnDeviceStateChanged(
    IMMNotificationClient *This,
    LPCWSTR pwstrDeviceId,
    DWORD dwNewState)
{
    change_notify *change = (change_notify *)This;

    if (!wcscmp(change->monitored, pwstrDeviceId)){
        switch (dwNewState) {
        case DEVICE_STATE_DISABLED:
        case DEVICE_STATE_NOTPRESENT:
        case DEVICE_STATE_UNPLUGGED:

        case DEVICE_STATE_ACTIVE:
        default:
            return S_OK;
        }
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnDeviceAdded(
    IMMNotificationClient *This,
    LPCWSTR pwstrDeviceId)
{
    change_notify *change = (change_notify *)This;

    return S_OK;
}

/* maybe MPV can go over to the prefered device once it is plugged in? */
static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnDeviceRemoved(
    IMMNotificationClient *This,
    LPCWSTR pwstrDeviceId)
{
    change_notify *change = (change_notify *)This;

    if (!wcscmp(change->monitored, pwstrDeviceId)) {

    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnDefaultDeviceChanged(
    IMMNotificationClient *This,
    EDataFlow flow,
    ERole role,
    LPCWSTR pwstrDeviceId)
{
    change_notify *change = (change_notify *)This;

    if (!wcscmp(change->monitored, pwstrDeviceId)) {

    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnPropertyValueChanged(
    IMMNotificationClient *This,
    LPCWSTR pwstrDeviceId,
    const PROPERTYKEY key)
{
    change_notify *change = (change_notify *)This;

    if (!wcscmp(change->monitored, pwstrDeviceId)) {

    }
    return S_OK;
}

static CONST_VTBL IMMNotificationClientVtbl sIMMDeviceEnumeratorVtbl_vtbl = {
    .QueryInterface = sIMMNotificationClient_QueryInterface,
    .AddRef = sIMMNotificationClient_AddRef,
    .Release = sIMMNotificationClient_Release,
    .OnDeviceStateChanged = sIMMNotificationClient_OnDeviceStateChanged,
    .OnDeviceAdded = sIMMNotificationClient_OnDeviceAdded,
    .OnDeviceRemoved = sIMMNotificationClient_OnDeviceRemoved,
    .OnDefaultDeviceChanged = sIMMNotificationClient_OnDefaultDeviceChanged,
    .OnPropertyValueChanged = sIMMNotificationClient_OnPropertyValueChanged,
};

HRESULT wasapi_change_init(struct change_notify *change, IMMDevice *monitor)
{
    HRESULT ret;

    if ( ((ret = IMMDevice_GetId(monitor, &change->monitored)) != S_OK) ) {
        wasapi_change_free(change);
        return ret;
    }

    ret = S_OK;
    change->client.lpVtbl = &sIMMDeviceEnumeratorVtbl_vtbl;
    return ret;
}

void wasapi_change_free(struct change_notify *change)
{
    if (change->monitored) CoTaskMemFree(change->monitored);
}
