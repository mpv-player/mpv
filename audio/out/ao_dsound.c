/*
 * Windows DirectSound interface
 *
 * Copyright (c) 2004 Gabor Szecsi <deje@miki.hu>
 *
 * This file is part of mpv.
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

/**
\todo verify/extend multichannel support
*/


#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#define DIRECTSOUND_VERSION 0x0600
#include <dsound.h>
#include <math.h>

#include <libavutil/avutil.h>
#include <libavutil/common.h>

#include "config.h"
#include "audio/format.h"
#include "ao.h"
#include "internal.h"
#include "common/msg.h"
#include "osdep/timer.h"
#include "osdep/io.h"
#include "options/m_option.h"

/**
\todo use the definitions from the win32 api headers when they define these
*/
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

static const GUID KSDATAFORMAT_SUBTYPE_PCM = {
    0x1, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

#if 0
#define DSSPEAKER_HEADPHONE         0x00000001
#define DSSPEAKER_MONO              0x00000002
#define DSSPEAKER_QUAD              0x00000003
#define DSSPEAKER_STEREO            0x00000004
#define DSSPEAKER_SURROUND          0x00000005
#define DSSPEAKER_5POINT1           0x00000006
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
    WAVEFORMATEX Format;
    union {
        WORD wValidBitsPerSample;       /* bits of precision  */
        WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
        WORD wReserved;                 /* If neither applies, set to zero. */
    } Samples;
    DWORD dwChannelMask;                /* which channels are */
                                        /* present in stream  */
    GUID SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

struct priv {
    HINSTANCE hdsound_dll;          ///handle to the dll
    LPDIRECTSOUND hds;              ///direct sound object
    LPDIRECTSOUNDBUFFER hdspribuf;  ///primary direct sound buffer
    LPDIRECTSOUNDBUFFER hdsbuf;     ///secondary direct sound buffer (stream buffer)
    int buffer_size;                ///size in bytes of the direct sound buffer
    int write_offset;               ///offset of the write cursor in the direct sound buffer
    int min_free_space;             ///if the free space is below this value get_space() will return 0
                                    ///there will always be at least this amout of free space to prevent
                                    ///get_space() from returning wrong values when buffer is 100% full.
                                    ///will be replaced with nBlockAlign in init()
    int underrun_check;             ///0 or last reported free space (underrun detection)
    int device_num;                 ///wanted device number
    GUID device;                    ///guid of the device
    int audio_volume;

    int device_index;

    int outburst;                   ///play in multiple of chunks of this size

    int cfg_device;
    int cfg_buffersize;

    struct ao_device_list *listing; ///temporary during list_devs()
};

/***************************************************************************************/

/**
\brief output error message
\param err error code
\return string with the error message
*/
static char * dserr2str(int err)
{
    switch (err) {
    case DS_OK:                         return "DS_OK";
    case DS_NO_VIRTUALIZATION:          return "DS_NO_VIRTUALIZATION";
    case DSERR_ALLOCATED:               return "DS_NO_VIRTUALIZATION";
    case DSERR_CONTROLUNAVAIL:          return "DSERR_CONTROLUNAVAIL";
    case DSERR_INVALIDPARAM:            return "DSERR_INVALIDPARAM";
    case DSERR_INVALIDCALL:             return "DSERR_INVALIDCALL";
    case DSERR_GENERIC:                 return "DSERR_GENERIC";
    case DSERR_PRIOLEVELNEEDED:         return "DSERR_PRIOLEVELNEEDED";
    case DSERR_OUTOFMEMORY:             return "DSERR_OUTOFMEMORY";
    case DSERR_BADFORMAT:               return "DSERR_BADFORMAT";
    case DSERR_UNSUPPORTED:             return "DSERR_UNSUPPORTED";
    case DSERR_NODRIVER:                return "DSERR_NODRIVER";
    case DSERR_ALREADYINITIALIZED:      return "DSERR_ALREADYINITIALIZED";
    case DSERR_NOAGGREGATION:           return "DSERR_NOAGGREGATION";
    case DSERR_BUFFERLOST:              return "DSERR_BUFFERLOST";
    case DSERR_OTHERAPPHASPRIO:         return "DSERR_OTHERAPPHASPRIO";
    case DSERR_UNINITIALIZED:           return "DSERR_UNINITIALIZED";
    case DSERR_NOINTERFACE:             return "DSERR_NOINTERFACE";
    case DSERR_ACCESSDENIED:            return "DSERR_ACCESSDENIED";
    }
    return "unknown";
}

/**
\brief uninitialize direct sound
*/
static void UninitDirectSound(struct ao *ao)
{
    struct priv *p = ao->priv;

    // finally release the DirectSound object
    if (p->hds) {
        IDirectSound_Release(p->hds);
        p->hds = NULL;
    }
    // free DSOUND.DLL
    if (p->hdsound_dll) {
        FreeLibrary(p->hdsound_dll);
        p->hdsound_dll = NULL;
    }
    MP_VERBOSE(ao, "DirectSound uninitialized\n");
}

/**
\brief enumerate direct sound devices
\return TRUE to continue with the enumeration
*/
static BOOL CALLBACK DirectSoundEnum(LPGUID guid, LPCSTR desc, LPCSTR module,
                                     LPVOID context)
{
    struct ao *ao = context;
    struct priv *p = ao->priv;

    MP_VERBOSE(ao, "%i %s ", p->device_index, desc);
    if (p->device_num == p->device_index) {
        MP_VERBOSE(ao, "<--");
        if (guid)
            memcpy(&p->device, guid, sizeof(GUID));
    }
    char *guidstr = talloc_strdup(NULL, "");
    if (guid) {
        wchar_t guidwstr[80] = {0};
        StringFromGUID2(guid, guidwstr, MP_ARRAY_SIZE(guidwstr));
        char *nstr = mp_to_utf8(NULL, guidwstr);
        if (nstr) {
            talloc_free(guidstr);
            guidstr = nstr;
        }
    }
    if (p->device_num < 0 && ao->device) {
        if (strcmp(ao->device, guidstr) == 0) {
            MP_VERBOSE(ao, "<--");
            p->device_num = p->device_index;
            if (guid)
                memcpy(&p->device, guid, sizeof(GUID));
        }
    }
    if (p->listing) {
        struct ao_device_desc e = {guidstr, desc};
        ao_device_list_add(p->listing, ao, &e);
    }
    talloc_free(guidstr);

    MP_VERBOSE(ao, "\n");
    p->device_index++;
    return TRUE;
}

static void EnumDevs(struct ao *ao)
{
    struct priv *p = ao->priv;

    p->device_index = 0;
    p->device_num = p->cfg_device;

    HRESULT (WINAPI *OurDirectSoundEnumerate)(LPDSENUMCALLBACKA, LPVOID);
    OurDirectSoundEnumerate = (void *)GetProcAddress(p->hdsound_dll,
                                                     "DirectSoundEnumerateA");

    if (OurDirectSoundEnumerate == NULL) {
        MP_ERR(ao, "GetProcAddress FAILED\n");
        return;
    }

    // Enumerate all directsound p->devices
    MP_VERBOSE(ao, "Output Devices:\n");
    OurDirectSoundEnumerate(DirectSoundEnum, ao);
}

static int LoadDirectSound(struct ao *ao)
{
    struct priv *p = ao->priv;

    // initialize directsound
    p->hdsound_dll = LoadLibrary(L"DSOUND.DLL");
    if (p->hdsound_dll == NULL) {
        MP_ERR(ao, "cannot load DSOUND.DLL\n");
        return 0;
    }

    return 1;
}

static void list_devs(struct ao *ao, struct ao_device_list *list)
{
    struct priv *p = ao->priv;
    bool need_init = !p->hdsound_dll;
    if (need_init && !LoadDirectSound(ao))
        return;

    p->listing = list;
    EnumDevs(ao);
    p->listing = NULL;

    if (need_init)
        UninitDirectSound(ao);
}

/**
\brief initilize direct sound
\return 0 if error, 1 if ok
*/
static int InitDirectSound(struct ao *ao)
{
    struct priv *p = ao->priv;

    DSCAPS dscaps;

    if (!LoadDirectSound(ao))
        return 0;

    HRESULT (WINAPI *OurDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
    OurDirectSoundCreate =
        (void *)GetProcAddress(p->hdsound_dll, "DirectSoundCreate");

    if (OurDirectSoundCreate == NULL) {
        MP_ERR(ao, "GetProcAddress FAILED\n");
        FreeLibrary(p->hdsound_dll);
        return 0;
    }

    EnumDevs(ao);

    // Create the direct sound object
    if (FAILED(OurDirectSoundCreate((p->device_num > 0) ? &p->device : NULL,
                                    &p->hds, NULL)))
    {
        MP_ERR(ao, "cannot create a DirectSound device\n");
        FreeLibrary(p->hdsound_dll);
        return 0;
    }

    /* Set DirectSound Cooperative level, ie what control we want over Windows
     * sound device. In our case, DSSCL_EXCLUSIVE means that we can modify the
     * settings of the primary buffer, but also that only the sound of our
     * application will be hearable when it will have the focus.
     * !!! (this is not really working as intended yet because to set the
     * cooperative level you need the window handle of your application, and
     * I don't know of any easy way to get it. Especially since we might play
     * sound without any video, and so what window handle should we use ???
     * The hack for now is to use the Desktop window handle - it seems to be
     * working */
    if (IDirectSound_SetCooperativeLevel(p->hds, GetDesktopWindow(),
                                         DSSCL_EXCLUSIVE))
    {
        MP_ERR(ao, "cannot set direct sound cooperative level\n");
        IDirectSound_Release(p->hds);
        FreeLibrary(p->hdsound_dll);
        return 0;
    }
    MP_VERBOSE(ao, "DirectSound initialized\n");

    memset(&dscaps, 0, sizeof(DSCAPS));
    dscaps.dwSize = sizeof(DSCAPS);
    if (DS_OK == IDirectSound_GetCaps(p->hds, &dscaps)) {
        if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
            MP_VERBOSE(ao, "DirectSound is emulated\n");
    } else {
        MP_VERBOSE(ao, "cannot get device capabilities\n");
    }

    return 1;
}

/**
\brief destroy the direct sound buffer
*/
static void DestroyBuffer(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->hdsbuf) {
        IDirectSoundBuffer_Release(p->hdsbuf);
        p->hdsbuf = NULL;
    }
    if (p->hdspribuf) {
        IDirectSoundBuffer_Release(p->hdspribuf);
        p->hdspribuf = NULL;
    }
}

/**
\brief fill sound buffer
\param data pointer to the sound data to copy
\param len length of the data to copy in bytes
\return number of copyed bytes
*/
static int write_buffer(struct ao *ao, unsigned char *data, int len)
{
    struct priv *p = ao->priv;
    HRESULT res;
    LPVOID lpvPtr1;
    DWORD dwBytes1;
    LPVOID lpvPtr2;
    DWORD dwBytes2;

    p->underrun_check = 0;

    // Lock the buffer
    res = IDirectSoundBuffer_Lock(p->hdsbuf, p->write_offset, len, &lpvPtr1,
                                  &dwBytes1, &lpvPtr2, &dwBytes2, 0);
    // If the buffer was lost, restore and retry lock.
    if (DSERR_BUFFERLOST == res) {
        IDirectSoundBuffer_Restore(p->hdsbuf);
        res = IDirectSoundBuffer_Lock(p->hdsbuf, p->write_offset, len, &lpvPtr1,
                                      &dwBytes1, &lpvPtr2, &dwBytes2, 0);
    }


    if (SUCCEEDED(res)) {
        memcpy(lpvPtr1, data, dwBytes1);
        if (NULL != lpvPtr2)
            memcpy(lpvPtr2, data + dwBytes1, dwBytes2);
        p->write_offset += dwBytes1 + dwBytes2;
        if (p->write_offset >= p->buffer_size)
            p->write_offset = dwBytes2;

        // Release the data back to DirectSound.
        res = IDirectSoundBuffer_Unlock(p->hdsbuf, lpvPtr1, dwBytes1, lpvPtr2,
                                        dwBytes2);
        if (SUCCEEDED(res)) {
            // Success.
            DWORD status;
            IDirectSoundBuffer_GetStatus(p->hdsbuf, &status);
            if (!(status & DSBSTATUS_PLAYING))
                res = IDirectSoundBuffer_Play(p->hdsbuf, 0, 0, DSBPLAY_LOOPING);
            return dwBytes1 + dwBytes2;
        }
    }
    // Lock, Unlock, or Restore failed.
    return 0;
}

/***************************************************************************************/

/**
\brief handle control commands
\param cmd command
\param arg argument
\return CONTROL_OK or CONTROL_UNKNOWN in case the command is not supported
*/
static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;
    DWORD volume;
    switch (cmd) {
    case AOCONTROL_GET_VOLUME: {
        ao_control_vol_t *vol = (ao_control_vol_t *)arg;
        vol->left = vol->right = p->audio_volume;
        return CONTROL_OK;
    }
    case AOCONTROL_SET_VOLUME: {
        ao_control_vol_t *vol = (ao_control_vol_t *)arg;
        volume = p->audio_volume = vol->right;
        if (volume < 1)
            volume = 1;
        volume = (DWORD)(log10(volume) * 5000.0) - 10000;
        IDirectSoundBuffer_SetVolume(p->hdsbuf, volume);
        return CONTROL_OK;
    }
    case AOCONTROL_HAS_SOFT_VOLUME:
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

/**
\brief setup sound device
\param rate samplerate
\param channels number of channels
\param format format
\param flags unused
\return 0=success -1=fail
*/
static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    int res;

    if (!InitDirectSound(ao))
        return -1;

    p->audio_volume = 100;

    // ok, now create the buffers
    WAVEFORMATEXTENSIBLE wformat;
    DSBUFFERDESC dsbpridesc;
    DSBUFFERDESC dsbdesc;
    int format = af_fmt_from_planar(ao->format);
    int rate = ao->samplerate;

    if (!AF_FORMAT_IS_IEC61937(format)) {
        struct mp_chmap_sel sel = {0};
        mp_chmap_sel_add_waveext(&sel);
        if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
            return -1;
    }
    switch (format) {
    case AF_FORMAT_S24:
    case AF_FORMAT_S16:
    case AF_FORMAT_U8:
        break;
    default:
        if (AF_FORMAT_IS_IEC61937(format))
            break;
        MP_VERBOSE(ao, "format %s not supported defaulting to Signed 16-bit Little-Endian\n",
                   af_fmt_to_str(format));
        format = AF_FORMAT_S16;
    }
    //set our audio parameters
    ao->samplerate = rate;
    ao->format = format;
    ao->bps = ao->channels.num * rate * af_fmt2bps(format);
    int buffersize = ao->bps * p->cfg_buffersize / 1000;
    MP_VERBOSE(ao, "Samplerate:%iHz Channels:%i Format:%s\n", rate,
               ao->channels.num, af_fmt_to_str(format));
    MP_VERBOSE(ao, "Buffersize:%d bytes (%f msec)\n",
               buffersize, buffersize * 1000.0 / ao->bps);

    //fill waveformatex
    ZeroMemory(&wformat, sizeof(WAVEFORMATEXTENSIBLE));
    wformat.Format.cbSize = (ao->channels.num > 2)
                    ? sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) : 0;
    wformat.Format.nChannels = ao->channels.num;
    wformat.Format.nSamplesPerSec = rate;
    if (AF_FORMAT_IS_IEC61937(format)) {
        // Whether it also works with e.g. DTS is unknown, but probably does.
        wformat.Format.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        wformat.Format.wBitsPerSample = 16;
        wformat.Format.nBlockAlign = 4;
    } else {
        wformat.Format.wFormatTag = (ao->channels.num > 2)
                                    ? WAVE_FORMAT_EXTENSIBLE : WAVE_FORMAT_PCM;
        int bps = af_fmt2bps(format);
        wformat.Format.wBitsPerSample = bps * 8;
        wformat.Format.nBlockAlign = wformat.Format.nChannels * bps;
    }

    // fill in primary sound buffer descriptor
    memset(&dsbpridesc, 0, sizeof(DSBUFFERDESC));
    dsbpridesc.dwSize = sizeof(DSBUFFERDESC);
    dsbpridesc.dwFlags       = DSBCAPS_PRIMARYBUFFER;
    dsbpridesc.dwBufferBytes = 0;
    dsbpridesc.lpwfxFormat   = NULL;

    // fill in the secondary sound buffer (=stream buffer) descriptor
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 /** Better position accuracy */
                      | DSBCAPS_GLOBALFOCUS       /** Allows background playing */
                      | DSBCAPS_CTRLVOLUME;       /** volume control enabled */

    if (ao->channels.num > 2) {
        wformat.dwChannelMask = mp_chmap_to_waveext(&ao->channels);
        wformat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        wformat.Samples.wValidBitsPerSample = wformat.Format.wBitsPerSample;
        // Needed for 5.1 on emu101k - shit soundblaster
        dsbdesc.dwFlags |= DSBCAPS_LOCHARDWARE;
    }
    wformat.Format.nAvgBytesPerSec = wformat.Format.nSamplesPerSec *
                                     wformat.Format.nBlockAlign;

    dsbdesc.dwBufferBytes = buffersize;
    dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&wformat;
    p->buffer_size = dsbdesc.dwBufferBytes;
    p->write_offset = 0;
    p->min_free_space = wformat.Format.nBlockAlign;
    p->outburst = wformat.Format.nBlockAlign * 512;

    // create primary buffer and set its format

    res = IDirectSound_CreateSoundBuffer(p->hds, &dsbpridesc, &p->hdspribuf, NULL);
    if (res != DS_OK) {
        UninitDirectSound(ao);
        MP_ERR(ao, "cannot create primary buffer (%s)\n", dserr2str(res));
        return -1;
    }
    res = IDirectSoundBuffer_SetFormat(p->hdspribuf, (WAVEFORMATEX *)&wformat);
    if (res != DS_OK) {
        MP_WARN(ao, "cannot set primary buffer format (%s), using "
                "standard setting (bad quality)", dserr2str(res));
    }

    MP_VERBOSE(ao, "primary buffer created\n");

    // now create the stream buffer

    res = IDirectSound_CreateSoundBuffer(p->hds, &dsbdesc, &p->hdsbuf, NULL);
    if (res != DS_OK) {
        if (dsbdesc.dwFlags & DSBCAPS_LOCHARDWARE) {
            // Try without DSBCAPS_LOCHARDWARE
            dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
            res = IDirectSound_CreateSoundBuffer(p->hds, &dsbdesc, &p->hdsbuf, NULL);
        }
        if (res != DS_OK) {
            UninitDirectSound(ao);
            MP_ERR(ao, "cannot create secondary (stream)buffer (%s)\n",
                   dserr2str(res));
            return -1;
        }
    }
    MP_VERBOSE(ao, "secondary (stream)buffer created\n");
    return 0;
}



/**
\brief stop playing and empty buffers (for seeking/pause)
*/
static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;

    IDirectSoundBuffer_Stop(p->hdsbuf);
    // reset directsound buffer
    IDirectSoundBuffer_SetCurrentPosition(p->hdsbuf, 0);
    p->write_offset = 0;
    p->underrun_check = 0;
}

/**
\brief stop playing, keep buffers (for pause)
*/
static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;

    IDirectSoundBuffer_Stop(p->hdsbuf);
}

/**
\brief resume playing, after audio_pause()
*/
static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;

    IDirectSoundBuffer_Play(p->hdsbuf, 0, 0, DSBPLAY_LOOPING);
}

/**
\brief close audio device
\param immed stop playback immediately
*/
static void uninit(struct ao *ao)
{
    reset(ao);

    DestroyBuffer(ao);
    UninitDirectSound(ao);
}

// return exact number of free (safe to write) bytes
static int check_free_buffer_size(struct ao *ao)
{
    struct priv *p = ao->priv;
    int space;
    DWORD play_offset;
    IDirectSoundBuffer_GetCurrentPosition(p->hdsbuf, &play_offset, NULL);
    space = p->buffer_size - (p->write_offset - play_offset);
    // |              | <-- const --> |                |                 |
    // buffer start   play_cursor     write_cursor     p->write_offset   buffer end
    // play_cursor is the actual postion of the play cursor
    // write_cursor is the position after which it is assumed to be save to write data
    // p->write_offset is the postion where we actually write the data to
    if (space > p->buffer_size)
        space -= p->buffer_size;                        // p->write_offset < play_offset
    // Check for buffer underruns. An underrun happens if DirectSound
    // started to play old data beyond the current p->write_offset. Detect this
    // by checking whether the free space shrinks, even though no data was
    // written (i.e. no write_buffer). Doesn't always work, but the only
    // reason we need this is to deal with the situation when playback ends,
    // and the buffer is only half-filled.
    if (space < p->underrun_check) {
        // there's no useful data in the buffers
        space = p->buffer_size;
        reset(ao);
    }
    p->underrun_check = space;
    return space;
}

/**
   \brief find out how many bytes can be written into the audio buffer without
   \return free space in bytes, has to return 0 if the buffer is almost full
 */
static int get_space(struct ao *ao)
{
    struct priv *p = ao->priv;

    int space = check_free_buffer_size(ao);
    if (space < p->min_free_space)
        return 0;
    return (space - p->min_free_space) / p->outburst * p->outburst / ao->sstride;
}

/**
\brief play 'len' bytes of 'data'
\param data pointer to the data to play
\param len size in bytes of the data buffer, gets rounded down to outburst*n
\param flags currently unused
\return number of played bytes
*/
static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *p = ao->priv;
    int len = samples * ao->sstride;

    int space = check_free_buffer_size(ao);
    if (space < len)
        len = space;

    if (!(flags & AOPLAY_FINAL_CHUNK))
        len = (len / p->outburst) * p->outburst;
    return write_buffer(ao, data[0], len) / ao->sstride;
}

/**
\brief get the delay between the first and last sample in the buffer
\return delay in seconds
*/
static double get_delay(struct ao *ao)
{
    struct priv *p = ao->priv;

    int space = check_free_buffer_size(ao);
    return (p->buffer_size - space) / (double)ao->bps;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_dsound = {
    .description = "Windows DirectSound audio output",
    .name      = "dsound",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .reset     = reset,
    .list_devs = list_devs,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_INT("device", cfg_device, 0, OPTDEF_INT(-1)),
        OPT_INTRANGE("buffersize", cfg_buffersize, 0, 1, 10000, OPTDEF_INT(200)),
        {0}
    },
};
