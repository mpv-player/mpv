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
#include <mmdeviceapi.h>
#include <avrt.h>

typedef struct wasapi_state {
    struct mp_log *log;
    HANDLE threadLoop;

    /* Init phase */
    int init_ret;
    HANDLE init_done;
    HANDLE fatal_error; /* signal to indicate unrecoverable error */
    int share_mode;

    /* Events */
    HANDLE hPause;

    /* Play */
    HANDLE hPlay;
    int is_playing;

    /* Reset */
    HANDLE hReset;

    /* uninit */
    HANDLE hUninit;
    LONG immed;

    /* volume control */
    HANDLE hGetvol, hSetvol, hDoneVol;
    DWORD vol_hw_support, status;
    float audio_volume;

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
    IAudioEndpointVolume *pEndpointVolume;
    HANDLE hFeed; /* wasapi event */
    HANDLE hTask; /* AV thread */
    DWORD taskIndex; /* AV task ID */
    WAVEFORMATEXTENSIBLE format;

    /* WASAPI internal clock information, for estimating delay */
    IAudioClock *pAudioClock;
    UINT64 clock_frequency; /* scale for the "samples" returned by the clock */
    UINT64 sample_count; /* the amount of samples per channel written to a GetBuffer buffer */
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
} wasapi_state;

#endif