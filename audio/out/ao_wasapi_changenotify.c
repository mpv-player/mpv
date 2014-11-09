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

static int GUID_compare (const GUID *l, const GUID *r){
  unsigned int i;
  if(l->Data1 != r->Data1) return 1;
  if(l->Data2 != r->Data2) return 1;
  if(l->Data3 != r->Data3) return 1;
  for (i = 0; i < 8; i++) {
    if(l->Data4[i] != r->Data4[i]) return 1;
  }
  return 0;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_QueryInterface
(IMMNotificationClient* This,
   REFIID riid,
   void **ppvObject){
  /* Compatible with IMMNotificationClient and IUnknown */
  if(!GUID_compare(&IID_IMMNotificationClient, riid) ||
     !GUID_compare(&IID_IUnknown, riid)) {
    *ppvObject = (IMMNotificationClient *) This;
    return S_OK;
  } else {
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
}
static ULONG STDMETHODCALLTYPE sIMMNotificationClient_AddRef(IMMNotificationClient* This){
  change_notify *_this = (change_notify *) This;
  return ++_this->count;
}

/* MSDN says it should free itself, but we're static */
static ULONG STDMETHODCALLTYPE sIMMNotificationClient_Release(IMMNotificationClient* This){
  change_notify *_this = (change_notify *) This;
  return --_this->count;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnDeviceStateChanged(
        IMMNotificationClient* This,
        LPCWSTR pwstrDeviceId,
        DWORD dwNewState){
  change_notify *_this = (change_notify *) This;
  if(!wcscmp(_this->monitored, pwstrDeviceId)){
    switch(dwNewState) {
      case DEVICE_STATE_DISABLED:
      case DEVICE_STATE_NOTPRESENT:
      case DEVICE_STATE_UNPLUGGED:
        SetEvent(_this->OnDeviceStateChanged);
      case DEVICE_STATE_ACTIVE:
      default:
      return S_OK;
      /* what? */
    }
  } /* do we care about other devices? */
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnDeviceAdded(
        IMMNotificationClient* This,
        LPCWSTR pwstrDeviceId){
  change_notify *_this = (change_notify *) This;
  size_t len = wcslen(pwstrDeviceId);
  if(_this->newDevice){ CoTaskMemFree(_this->newDevice); }
  _this->newDevice = CoTaskMemAlloc(len + 1);
  if(!_this->newDevice){ return E_OUTOFMEMORY; }
  wcsncpy(_this->newDevice, pwstrDeviceId, len);
  _this->newDevice[len] = L'\0';
  SetEvent(_this->OnDeviceAdded);
  return S_OK;
}

/* maybe MPV can go over to the prefered device once it is plugged in? */
static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnDeviceRemoved(
        IMMNotificationClient* This,
        LPCWSTR pwstrDeviceId){
  change_notify *_this = (change_notify *) This;
  if(!wcscmp(_this->monitored, pwstrDeviceId)){
    SetEvent(_this->OnDeviceRemoved);
  }
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnDefaultDeviceChanged(
        IMMNotificationClient* This,
        EDataFlow flow,
        ERole role,
        LPCWSTR pwstrDeviceId){
  change_notify *_this = (change_notify *) This;
  if(!wcscmp(_this->monitored, pwstrDeviceId)){
    SetEvent(_this->OnDefaultDeviceChanged);
  } /* do we care about other devices? */
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnPropertyValueChanged(
        IMMNotificationClient* This,
        LPCWSTR pwstrDeviceId,
        const PROPERTYKEY key){
  change_notify *_this = (change_notify *) This;
  if(!wcscmp(_this->monitored, pwstrDeviceId)){
    _this->propChanged = key;
    SetEvent(_this->OnPropertyValueChanged);
  } /* do we care about other devices? */
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

/* Change notification */
HRESULT wasapi_change_init(struct change_notify *change, IMMDevice *monitor) {
  HRESULT ret = E_OUTOFMEMORY;
  change->count = 0U;
  change->OnDefaultDeviceChanged = change->OnDeviceAdded = change->OnDeviceRemoved = NULL;
  change->OnDeviceStateChanged = change->OnPropertyValueChanged = NULL;
  change->newDevice = change->monitored = NULL;

  change->OnDefaultDeviceChanged = CreateEventW(NULL, FALSE, FALSE, NULL);
  change->OnDeviceAdded = CreateEventW(NULL, FALSE, FALSE, NULL);
  change->OnDeviceRemoved = CreateEventW(NULL, FALSE, FALSE, NULL);
  change->OnDeviceStateChanged = CreateEventW(NULL, FALSE, FALSE, NULL);
  change->OnPropertyValueChanged = CreateEventW(NULL, FALSE, FALSE, NULL);

  if(!change->OnDefaultDeviceChanged || !change->OnDeviceAdded ||
     !change->OnDeviceRemoved || !change->OnDeviceStateChanged ||
     !change->OnPropertyValueChanged ||
     ((ret = IMMDevice_GetId(monitor, &change->monitored)) != S_OK)) {
     wasapi_change_free(change);
     return ret;
  }
  ret = S_OK;

  change->client.lpVtbl = &sIMMDeviceEnumeratorVtbl_vtbl;
  return ret;
}

#define tryfreehandles(xx) if(xx) CloseHandle(xx)
void wasapi_change_free(struct change_notify *change){
  tryfreehandles(change->OnDefaultDeviceChanged);
  tryfreehandles(change->OnDeviceAdded);
  tryfreehandles(change->OnDeviceRemoved);
  tryfreehandles(change->OnDeviceStateChanged);
  tryfreehandles(change->OnPropertyValueChanged);
  if(change->monitored){ CoTaskMemFree(change->monitored); }
  if(change->newDevice){ CoTaskMemFree(change->newDevice); }
}

HRESULT wasapi_change_reset(struct change_notify *change, IMMDevice *monitor){
  if(change->monitored){ CoTaskMemFree(change->monitored); }
  return IMMDevice_GetId(monitor, &change->monitored);
}

