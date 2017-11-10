/*
 * This file is part of mpv.
 *
 * Original author: Jonathan Yong <10walls@gmail.com>
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MP_AO_WASAPI_H_
#define MP_AO_WASAPI_H_

#include <stdlib.h>
#include <stdbool.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <endpointvolume.h>

#include "common/msg.h"
#include "osdep/atomic.h"
#include "osdep/windows_utils.h"
#include "internal.h"
#include "ao.h"

typedef struct change_notify {
    IMMNotificationClient client; // this must be first in the structure!
    IMMDeviceEnumerator *pEnumerator; // object where client is registered
    LPWSTR monitored; // Monitored device
    bool is_hotplug;
    struct ao *ao;
} change_notify;

HRESULT wasapi_change_init(struct ao* ao, bool is_hotplug);
void wasapi_change_uninit(struct ao* ao);

enum wasapi_thread_state {
    WASAPI_THREAD_FEED = 0,
    WASAPI_THREAD_RESUME,
    WASAPI_THREAD_RESET,
    WASAPI_THREAD_SHUTDOWN
};

typedef struct wasapi_state {
    struct mp_log *log;

    bool init_ok;            // status of init phase
    // Thread handles
    HANDLE hInitDone;        // set when init is complete in audio thread
    HANDLE hAudioThread;     // the audio thread itself
    HANDLE hWake;            // thread wakeup event
    atomic_int thread_state; // enum wasapi_thread_state (what to do on wakeup)
    struct mp_dispatch_queue *dispatch; // for volume/mute/session display

    // for setting the audio thread priority
    HANDLE hTask;

    // ID of the device to use
    LPWSTR deviceID;
    // WASAPI object handles owned and used by audio thread
    IMMDevice *pDevice;
    IAudioClient *pAudioClient;
    IAudioRenderClient *pRenderClient;

    // WASAPI internal clock information, for estimating delay
    IAudioClock *pAudioClock;
    atomic_ullong sample_count;  // samples per channel written by GetBuffer
    UINT64 clock_frequency;      // scale for position returned by GetPosition
    LARGE_INTEGER qpc_frequency; // frequency of Windows' high resolution timer

    // WASAPI control
    IAudioSessionControl *pSessionControl; // setting the stream title
    IAudioEndpointVolume *pEndpointVolume; // exclusive mode volume/mute
    ISimpleAudioVolume *pAudioVolume;      // shared mode volume/mute
    DWORD vol_hw_support; // is hardware volume supported for exclusive-mode?

    // ao options
    int opt_exclusive;

    // format info
    WAVEFORMATEXTENSIBLE format;
    AUDCLNT_SHAREMODE share_mode; // AUDCLNT_SHAREMODE_EXCLUSIVE / SHARED
    UINT32 bufferFrameCount;      // number of frames in buffer
    struct ao_convert_fmt convert_format;

    change_notify change;
} wasapi_state;

char *mp_PKEY_to_str_buf(char *buf, size_t buf_size, const PROPERTYKEY *pkey);
#define mp_PKEY_to_str(pkey) mp_PKEY_to_str_buf((char[42]){0}, 42, (pkey))

void wasapi_list_devs(struct ao *ao, struct ao_device_list *list);
bstr wasapi_get_specified_device_string(struct ao *ao);
LPWSTR wasapi_find_deviceID(struct ao *ao);

bool wasapi_thread_init(struct ao *ao);
void wasapi_thread_uninit(struct ao *ao);

#define EXIT_ON_ERROR(hres)  \
              do { if (FAILED(hres)) { goto exit_label; } } while(0)
#define SAFE_DESTROY(unk, release) \
              do { if ((unk) != NULL) { release; (unk) = NULL; } } while(0)

#endif
