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

#include <initguid.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wchar.h>
#include <stdlib.h>

#include "ao_wasapi.h"
#include "ao_wasapi_utils.h"

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_QueryInterface(
    IMMNotificationClient* This, REFIID riid, void **ppvObject)
{
    /* Compatible with IMMNotificationClient and IUnknown */
    if (IsEqualGUID(&IID_IMMNotificationClient, riid) ||
        IsEqualGUID(&IID_IUnknown, riid))
    {
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
    struct ao *ao = change->ao;

    if (change->is_hotplug) {
        MP_VERBOSE(ao, "OnDeviceStateChanged triggered: sending hotplug event\n");
        ao_hotplug_event(ao);
    } else if (pwstrDeviceId && !wcscmp(pwstrDeviceId, change->monitored)) {
        switch (dwNewState) {
        case DEVICE_STATE_DISABLED:
        case DEVICE_STATE_NOTPRESENT:
        case DEVICE_STATE_UNPLUGGED:
            MP_VERBOSE(ao, "OnDeviceStateChanged triggered on device %S: "
                       "requesting ao reload\n", pwstrDeviceId);
            ao_request_reload(ao);
            break;
        case DEVICE_STATE_ACTIVE:
            break;
        }
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnDeviceAdded(
    IMMNotificationClient *This,
    LPCWSTR pwstrDeviceId)
{
    change_notify *change = (change_notify *)This;
    struct ao *ao = change->ao;

    if (change->is_hotplug) {
        MP_VERBOSE(ao, "OnDeviceAdded triggered: sending hotplug event\n");
        ao_hotplug_event(ao);
    }

    return S_OK;
}

/* maybe MPV can go over to the prefered device once it is plugged in? */
static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnDeviceRemoved(
    IMMNotificationClient *This,
    LPCWSTR pwstrDeviceId)
{
    change_notify *change = (change_notify *)This;
    struct ao *ao = change->ao;

    if (change->is_hotplug) {
        MP_VERBOSE(ao, "OnDeviceRemoved triggered: sending hotplug event\n");
        ao_hotplug_event(ao);
    } else if (pwstrDeviceId && !wcscmp(pwstrDeviceId, change->monitored)) {
        MP_VERBOSE(ao, "OnDeviceRemoved triggered for device %S: "
                   "requesting ao reload\n", pwstrDeviceId);
        ao_request_reload(ao);
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
    struct ao *ao = change->ao;
    struct wasapi_state *state = ao->priv;

    /* don't care about "eCapture" or non-"eMultimedia" roles  */
    if (flow == eCapture || role != eMultimedia) return S_OK;

    if (change->is_hotplug) {
        MP_VERBOSE(ao, "OnDefaultDeviceChanged triggered: sending hotplug event\n");
        ao_hotplug_event(ao);
    } else {
        /* stay on the device the user specified */
        if (state->opt_device) {
            MP_VERBOSE(ao, "OnDefaultDeviceChanged triggered: "
                       "staying on specified device %s\n", state->opt_device);
            return S_OK;
        }

        /* don't reload if already on the new default */
        if (pwstrDeviceId && !wcscmp(pwstrDeviceId, change->monitored)) {
            MP_VERBOSE(ao, "OnDefaultDeviceChanged triggered: "
                       "already using default device, no reload required\n");
            return S_OK;
        }

        /* if we got here, we need to reload */
        MP_VERBOSE(ao, "OnDefaultDeviceChanged triggered: requesting ao reload\n");
        ao_request_reload(ao);
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sIMMNotificationClient_OnPropertyValueChanged(
    IMMNotificationClient *This,
    LPCWSTR pwstrDeviceId,
    const PROPERTYKEY key)
{
    change_notify *change = (change_notify *)This;
    struct ao *ao = change->ao;

    if (!change->is_hotplug && pwstrDeviceId &&
        !wcscmp(pwstrDeviceId, change->monitored))
    {
        MP_VERBOSE(ao, "OnPropertyValueChanged triggered on device %S\n",
                   pwstrDeviceId);
        if (IsEqualPropertyKey(PKEY_AudioEngine_DeviceFormat, key)) {
            MP_VERBOSE(change->ao,
                       "Changed property: PKEY_AudioEngine_DeviceFormat "
                       "- requesting ao reload\n");
            ao_request_reload(change->ao);
        } else {
            MP_VERBOSE(ao, "Changed property: %s\n", mp_PKEY_to_str(&key));
        }
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


HRESULT wasapi_change_init(struct ao *ao, bool is_hotplug)
{
    struct wasapi_state *state = ao->priv;
    struct change_notify *change = &state->change;
    HRESULT hr;
    /* COM voodoo to emulate c++ class */
    change->client.lpVtbl = &sIMMDeviceEnumeratorVtbl_vtbl;

    /* register the change notification client */
    hr = IMMDeviceEnumerator_RegisterEndpointNotificationCallback(
        state->pEnumerator, (IMMNotificationClient *)change);
    EXIT_ON_ERROR(hr);

    /* so the callbacks can access the ao */
    change->ao = ao;

    /* whether or not this is the hotplug instance */
    change->is_hotplug = is_hotplug;

    if (is_hotplug) {
        MP_DBG(ao, "Monitoring for hotplug events\n");
    } else {
        /* Get the device string to compare with the pwstrDeviceId */
        hr = IMMDevice_GetId(state->pDevice, &change->monitored);
        EXIT_ON_ERROR(hr);
        MP_VERBOSE(ao, "Monitoring changes in device %S\n", change->monitored);
    }

    return hr;
exit_label:
    MP_ERR(state, "Error setting up device change monitoring: %s\n",
           mp_HRESULT_to_str(hr));
    wasapi_change_uninit(ao);
    return hr;
}

void wasapi_change_uninit(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    struct change_notify *change = &state->change;

    if (state->pEnumerator && change->client.lpVtbl) {
        IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(
            state->pEnumerator, (IMMNotificationClient *)change);
    }

    if (change->monitored) CoTaskMemFree(change->monitored);
}
