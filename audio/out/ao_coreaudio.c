/*
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

#include "config.h"
#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "options/m_option.h"
#include "misc/ring.h"
#include "common/msg.h"
#include "audio/out/ao_coreaudio_properties.h"
#include "audio/out/ao_coreaudio_utils.h"

static void audio_pause(struct ao *ao);
static void audio_resume(struct ao *ao);
static void reset(struct ao *ao);

struct priv {
    AudioDeviceID device;   // selected device
    AudioUnit audio_unit;   // AudioUnit for lpcm output
    bool paused;
    struct mp_ring *buffer;

    // options
    int opt_device_id;
    int opt_list;
};

static int get_ring_size(struct ao *ao)
{
    return af_fmt_seconds_to_bytes(
            ao->format, 0.5, ao->channels.num, ao->samplerate);
}

static OSStatus render_cb_lpcm(void *ctx, AudioUnitRenderActionFlags *aflags,
                              const AudioTimeStamp *ts, UInt32 bus,
                              UInt32 frames, AudioBufferList *buffer_list)
{
    struct ao *ao   = ctx;
    struct priv *p  = ao->priv;

    AudioBuffer buf = buffer_list->mBuffers[0];
    int requested   = buf.mDataByteSize;

    if (mp_ring_buffered(p->buffer) < requested) {
        MP_VERBOSE(ao, "buffer underrun\n");
        audio_pause(ao);
        memset(buf.mData, 0, requested);
    } else {
        mp_ring_read(p->buffer, buf.mData, requested);
    }

    return noErr;
}

static int get_volume(struct ao *ao, struct ao_control_vol *vol) {
    struct priv *p = ao->priv;
    float auvol;
    OSStatus err =
        AudioUnitGetParameter(p->audio_unit, kHALOutputParam_Volume,
                              kAudioUnitScope_Global, 0, &auvol);

    CHECK_CA_ERROR("could not get HAL output volume");
    vol->left = vol->right = auvol * 100.0;
    return CONTROL_TRUE;
coreaudio_error:
    return CONTROL_ERROR;
}

static int set_volume(struct ao *ao, struct ao_control_vol *vol) {
    struct priv *p = ao->priv;
    float auvol = (vol->left + vol->right) / 200.0;
    OSStatus err =
        AudioUnitSetParameter(p->audio_unit, kHALOutputParam_Volume,
                              kAudioUnitScope_Global, 0, auvol, 0);
    CHECK_CA_ERROR("could not set HAL output volume");
    return CONTROL_TRUE;
coreaudio_error:
    return CONTROL_ERROR;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        return get_volume(ao, arg);
    case AOCONTROL_SET_VOLUME:
        return set_volume(ao, arg);
    } // end switch
    return CONTROL_UNKNOWN;
}

static bool init_chmap(struct ao *ao);
static bool init_audiounit(struct ao *ao, AudioStreamBasicDescription asbd);

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->opt_list) ca_print_device_list(ao);

    ao->per_application_mixer = true;
    ao->no_persistent_volume  = true;

    OSStatus err = ca_select_device(ao, p->opt_device_id, &p->device);
    CHECK_CA_ERROR("failed to select device");

    if (!init_chmap(ao))
        goto coreaudio_error;

    ao->format = af_fmt_from_planar(ao->format);

    AudioStreamBasicDescription asbd;
    ca_fill_asbd(ao, &asbd);

    if (!init_audiounit(ao, asbd))
        goto coreaudio_error;

    return CONTROL_OK;

coreaudio_error:
    return CONTROL_ERROR;
}

static bool init_chmap(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSStatus err;
    AudioChannelLayout *layouts;
    size_t n_layouts;

    err = CA_GET_ARY_O(p->device,
                       kAudioDevicePropertyPreferredChannelLayout,
                       &layouts, &n_layouts);
    CHECK_CA_ERROR("could not get audio device prefered layouts");

    struct mp_chmap_sel chmap_sel = {0};
    for (int i = 0; i < n_layouts; i++) {
        struct mp_chmap chmap = {0};
        if (ca_layout_to_mp_chmap(ao, &layouts[i], &chmap))
            mp_chmap_sel_add_map(&chmap_sel, &chmap);
    }

    talloc_free(layouts);

    if (ao->channels.num < 3) {
        struct mp_chmap chmap;
        mp_chmap_from_channels(&chmap, ao->channels.num);
        mp_chmap_sel_add_map(&chmap_sel, &chmap);
    }

    if (!ao_chmap_sel_adjust(ao, &chmap_sel, &ao->channels)) {
        MP_ERR(ao, "could not select a suitable channel map among the "
                   "hardware supported ones. Make sure to configure your "
                   "output device correctly in 'Audio MIDI Setup.app'\n");
        goto coreaudio_error;
    }

    return true;

coreaudio_error:
    return false;
}

static bool init_audiounit(struct ao *ao, AudioStreamBasicDescription asbd)
{
    OSStatus err;
    uint32_t size;
    struct priv *p = ao->priv;

    AudioComponentDescription desc = (AudioComponentDescription) {
        .componentType         = kAudioUnitType_Output,
        .componentSubType      = (p->opt_device_id < 0) ?
                                    kAudioUnitSubType_DefaultOutput :
                                    kAudioUnitSubType_HALOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags        = 0,
        .componentFlagsMask    = 0,
    };

    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (comp == NULL) {
        MP_ERR(ao, "unable to find audio component\n");
        goto coreaudio_error;
    }

    err = AudioComponentInstanceNew(comp, &(p->audio_unit));
    CHECK_CA_ERROR("unable to open audio component");

    // Initialize AudioUnit
    err = AudioUnitInitialize(p->audio_unit);
    CHECK_CA_ERROR_L(coreaudio_error_component,
                     "unable to initialize audio unit");

    size = sizeof(AudioStreamBasicDescription);
    err = AudioUnitSetProperty(p->audio_unit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &asbd, size);

    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "unable to set the input format on the audio unit");

    //Set the Current Device to the Default Output Unit.
    err = AudioUnitSetProperty(p->audio_unit,
                               kAudioOutputUnitProperty_CurrentDevice,
                               kAudioUnitScope_Global, 0, &p->device,
                               sizeof(p->device));
    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "can't link audio unit to selected device");

    p->buffer = mp_ring_new(p, get_ring_size(ao));
    AURenderCallbackStruct render_cb = (AURenderCallbackStruct) {
        .inputProc       = render_cb_lpcm,
        .inputProcRefCon = ao,
    };

    err = AudioUnitSetProperty(p->audio_unit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input, 0, &render_cb,
                               sizeof(AURenderCallbackStruct));

    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "unable to set render callback on audio unit");

    reset(ao);
    return true;

coreaudio_error_audiounit:
    AudioUnitUninitialize(p->audio_unit);
coreaudio_error_component:
    AudioComponentInstanceDispose(p->audio_unit);
coreaudio_error:
    return false;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *p       = ao->priv;
    void *output_samples = data[0];
    int num_bytes        = samples * ao->sstride;

    int wrote = mp_ring_write(p->buffer, output_samples, num_bytes);
    audio_resume(ao);

    return wrote / ao->sstride;
}

static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    audio_pause(ao);
    mp_ring_reset(p->buffer);
}

static int get_space(struct ao *ao)
{
    struct priv *p = ao->priv;
    return mp_ring_available(p->buffer) / ao->sstride;
}

static float get_delay(struct ao *ao)
{
    // FIXME: should also report the delay of coreaudio itself (hardware +
    // internal buffers)
    struct priv *p = ao->priv;
    return mp_ring_buffered(p->buffer) / (float)ao->bps;
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;
    AudioOutputUnitStop(p->audio_unit);
    AudioUnitUninitialize(p->audio_unit);
    AudioComponentInstanceDispose(p->audio_unit);
}

static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->paused)
        return;

    OSStatus err = AudioOutputUnitStop(p->audio_unit);
    CHECK_CA_WARN("can't stop audio unit");

    p->paused = true;
}

static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (!p->paused)
        return;

    OSStatus err = AudioOutputUnitStart(p->audio_unit);
    CHECK_CA_WARN("can't start audio unit");

    p->paused = false;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_coreaudio = {
    .description = "CoreAudio AudioUnit",
    .name      = "coreaudio",
    .uninit    = uninit,
    .init      = init,
    .play      = play,
    .control   = control,
    .get_space = get_space,
    .get_delay = get_delay,
    .reset     = reset,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_INT("device_id", opt_device_id, 0, OPTDEF_INT(-1)),
        OPT_FLAG("list", opt_list, 0),
        {0}
    },
};
