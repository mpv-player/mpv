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

#include <stdbool.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <avrt.h>

#include "osdep/atomics.h"

typedef struct change_notify {
    IMMNotificationClient client; // this must be first in the structure!
    LPWSTR monitored; // Monitored device
    bool is_hotplug;
    struct ao *ao;
} change_notify;

HRESULT wasapi_change_init(struct ao* ao, bool is_hotplug);
void wasapi_change_uninit(struct ao* ao);

#define EXIT_ON_ERROR(hres)  \
              do { if (FAILED(hres)) { goto exit_label; } } while(0)
#define SAFE_RELEASE(unk, release) \
              do { if ((unk) != NULL) { release; (unk) = NULL; } } while(0)

enum wasapi_thread_state {
    WASAPI_THREAD_FEED = 0,
    WASAPI_THREAD_RESUME,
    WASAPI_THREAD_RESET,
    WASAPI_THREAD_SHUTDOWN
};

typedef struct wasapi_state {
    struct mp_log *log;

    // Thread handles
    HRESULT init_ret;        // status of init phase
    HANDLE hInitDone;        // set when init is complete in audio thread
    HANDLE hAudioThread;     // the audio thread itself
    HANDLE hWake;            // thread wakeup event
    atomic_int thread_state; // enum wasapi_thread_state (what to do on wakeup)

    // for setting the audio thread priority
    HANDLE hTask;

    // WASAPI object handles owned and used by audio thread
    IMMDevice *pDevice;
    IAudioClient *pAudioClient;
    IAudioRenderClient *pRenderClient;
    IMMDeviceEnumerator *pEnumerator;

    // WASAPI internal clock information, for estimating delay
    IAudioClock *pAudioClock;
    atomic_ullong sample_count;  // samples per channel written by GetBuffer
    UINT64 clock_frequency;      // scale for position returned by GetPosition
    LARGE_INTEGER qpc_frequency; // frequency of Windows' high resolution timer

    // WASAPI control (handles owned by audio thread but used by main thread)
    IAudioSessionControl *pSessionControl; // setting the stream title
    IAudioEndpointVolume *pEndpointVolume; // exclusive mode volume/mute
    ISimpleAudioVolume *pAudioVolume;      // shared mode volume/mute
    DWORD vol_hw_support; // is hardware volume supported for exclusive-mode?

    // Streams used to marshal the proxy objects. The thread owning the actual
    // objects needs to marshal proxy objects into these streams, and the thread
    // that wants the proxies unmarshals them from here.
    IStream *sSessionControl;
    IStream *sEndpointVolume;
    IStream *sAudioVolume;

    // WASAPI proxy handles, for Single-Threaded Apartment communication. One is
    // needed for each audio thread object that's accessed from the main thread.
    IAudioSessionControl *pSessionControlProxy;
    IAudioEndpointVolume *pEndpointVolumeProxy;
    ISimpleAudioVolume *pAudioVolumeProxy;

    // ao options
    int opt_exclusive;
    int opt_list;
    char *opt_device;

    // format info
    WAVEFORMATEXTENSIBLE format;
    AUDCLNT_SHAREMODE share_mode; // AUDCLNT_SHAREMODE_EXCLUSIVE / SHARED
    UINT32 bufferFrameCount;      // number of frames in buffer

    change_notify change;
} wasapi_state;

#endif
