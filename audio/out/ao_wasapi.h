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

#ifndef MP_AO_WASAPI_H_
#define MP_AO_WASAPI_H_

#define COBJMACROS 1
#define _WIN32_WINNT 0x600

#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <avrt.h>

#include "osdep/atomics.h"

typedef struct change_notify {
  IMMNotificationClient client;
  HANDLE OnDefaultDeviceChanged;
  HANDLE OnDeviceAdded;
  HANDLE OnDeviceRemoved;
  HANDLE OnDeviceStateChanged;
  HANDLE OnPropertyValueChanged;
  LPWSTR monitored; /* Monitored device */
  LPWSTR newDevice; /* whatever last plugged in new device */
  PROPERTYKEY propChanged; /* what prop changed ?*/
  ULONG count;
} change_notify;

HRESULT wasapi_change_init(struct change_notify *change, IMMDevice *monitor);
void wasapi_change_free(struct change_notify *change);
HRESULT wasapi_change_reset(struct change_notify *change, IMMDevice *monitor);

typedef struct wasapi_state {
    struct mp_log *log;
    HANDLE threadLoop;

    /* Init phase */
    int init_ret;
    HANDLE init_done;
    int share_mode;

    HANDLE hUninit;

    /* volume control */
    DWORD vol_hw_support, status;
    float audio_volume;
    float previous_volume;
    float initial_volume;

    /* Buffers */
    size_t buffer_block_size; /* Size of each block in bytes */
    REFERENCE_TIME
        minRequestedDuration; /* minimum wasapi buffer block size, in 100-nanosecond units */
    REFERENCE_TIME
        defaultRequestedDuration; /* default wasapi default block size, in 100-nanosecond units */
    UINT32 bufferFrameCount; /* wasapi buffer block size, number of frames, frame size at format.nBlockAlign */

    /* WASAPI handles, owned by other thread */
    IMMDevice *pDevice;
    IAudioClient *pAudioClient;
    IAudioRenderClient *pRenderClient;
    ISimpleAudioVolume *pAudioVolume;
    IAudioEndpointVolume *pEndpointVolume;
    IAudioSessionControl *pSessionControl;
    IMMDeviceEnumerator *pEnumerator;

    HANDLE hFeed; /* wasapi event */
    HANDLE hForceFeed; /* forces writing a buffer (e.g. before audio_resume) */
    HANDLE hFeedDone; /* set only after a hForceFeed */
    HANDLE hTask; /* AV thread */
    DWORD taskIndex; /* AV task ID */
    WAVEFORMATEXTENSIBLE format;

    /* WASAPI proxy handles, for Single-Threaded Apartment communication.
       One is needed for each object that's accessed by a different thread. */
    IAudioClient *pAudioClientProxy;
    ISimpleAudioVolume *pAudioVolumeProxy;
    IAudioEndpointVolume *pEndpointVolumeProxy;
    IAudioSessionControl *pSessionControlProxy;

    /* Streams used to marshal the proxy objects. The thread owning the actual objects
       needs to marshal proxy objects into these streams, and the thread that wants the
       proxies unmarshals them from here. */
    IStream *sAudioClient;
    IStream *sAudioVolume;
    IStream *sEndpointVolume;
    IStream *sSessionControl;

    /* WASAPI internal clock information, for estimating delay */
    IAudioClock *pAudioClock;
    UINT64 clock_frequency; /* scale for the "samples" returned by the clock */
    atomic_ullong sample_count; /* the amount of samples per channel written to a GetBuffer buffer */
    LARGE_INTEGER qpc_frequency; /* frequency of windows' high resolution timer */

    int opt_exclusive;
    int opt_list;
    char *opt_device;

    /* We still need to support XP, don't use these functions directly, blob owned by main thread */
    struct {
        HMODULE hAvrt;
        HANDLE (WINAPI *pAvSetMmThreadCharacteristicsW)(LPCWSTR, LPDWORD);
        WINBOOL (WINAPI *pAvRevertMmThreadCharacteristics)(HANDLE);
    } VistaBlob;

    change_notify changeNotify;

} wasapi_state;

#endif
