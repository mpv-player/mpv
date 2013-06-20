/*
 * CoreAudio audio output driver for Mac OS X
 *
 * original copyright (C) Timothy J. Wood - Aug 2000
 * ported to MPlayer libao2 by Dan Christiansen
 *
 * The S/PDIF part of the code is based on the auhal audio output
 * module from VideoLAN:
 * Copyright (c) 2006 Derk-Jan Hartman <hartman at videolan dot org>
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
 * along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * The MacOS X CoreAudio framework doesn't mesh as simply as some
 * simpler frameworks do.  This is due to the fact that CoreAudio pulls
 * audio samples rather than having them pushed at it (which is nice
 * when you are wanting to do good buffering of audio).
 */

#include "config.h"

#include "audio/out/ao_coreaudio_common.c"

#include "ao.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "core/subopt-helper.h"
#include "core/mp_ring.h"

static void audio_pause(struct ao *ao);
static void audio_resume(struct ao *ao);
static void reset(struct ao *ao);

static void print_buffer(struct mp_ring *buffer)
{
    void *tctx = talloc_new(NULL);
    ca_msg(MSGL_V, "%s\n", mp_ring_repr(buffer, tctx));
    talloc_free(tctx);
}

struct priv
{
    AudioDeviceID i_selected_dev;           /* Keeps DeviceID of the selected device. */
    int b_supports_digital;                 /* Does the currently selected device support digital mode? */
    int b_digital;                          /* Are we running in digital mode? */
    int b_muted;                            /* Are we muted in digital mode? */

    AudioDeviceIOProcID renderCallback;     /* Render callback used for SPDIF */

    /* AudioUnit */
    AudioUnit theOutputUnit;

    /* CoreAudio SPDIF mode specific */
    pid_t i_hog_pid;                        /* Keeps the pid of our hog status. */
    AudioStreamID i_stream_id;              /* The StreamID that has a cac3 streamformat */
    int i_stream_index;                     /* The index of i_stream_id in an AudioBufferList */
    AudioStreamBasicDescription stream_format; /* The format we changed the stream to */
    AudioStreamBasicDescription sfmt_revert; /* The original format of the stream */
    int b_revert;                           /* Whether we need to revert the stream format */
    int b_changed_mixing;                   /* Whether we need to set the mixing mode back */
    int b_stream_format_changed;            /* Flag for main thread to reset stream's format to digital and reset buffer */

    /* Original common part */
    int packetSize;
    int paused;

    struct mp_ring *buffer;
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
    int requested   = frames * p->packetSize;
    AudioBuffer buf = buffer_list->mBuffers[0];

    buf.mDataByteSize = mp_ring_read(p->buffer, buf.mData, requested);

    return noErr;
}

static OSStatus render_cb_digital(
        AudioDeviceID device, const AudioTimeStamp *ts,
        const void *in_data, const AudioTimeStamp *in_ts,
        AudioBufferList *out_data, const AudioTimeStamp *out_ts, void *ctx)
{
    struct ao *ao   = ctx;
    struct priv *p  = ao->priv;
    AudioBuffer buf = out_data->mBuffers[p->i_stream_index];
    int requested   = buf.mDataByteSize;

    if (p->b_muted)
        mp_ring_drain(p->buffer, requested);
    else
        mp_ring_read(p->buffer, buf.mData, requested);

    return noErr;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;
    ao_control_vol_t *control_vol;
    OSStatus err;
    Float32 vol;
    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        control_vol = (ao_control_vol_t *)arg;
        if (p->b_digital) {
            // Digital output has no volume adjust.
            int vol = p->b_muted ? 0 : 100;
            *control_vol = (ao_control_vol_t) {
                .left = vol, .right = vol,
            };
            return CONTROL_TRUE;
        }

        err = AudioUnitGetParameter(p->theOutputUnit, kHALOutputParam_Volume,
                                    kAudioUnitScope_Global, 0, &vol);

        CHECK_CA_ERROR("could not get HAL output volume");
        control_vol->left = control_vol->right = vol * 100.0 / 4.0;
        return CONTROL_TRUE;

    case AOCONTROL_SET_VOLUME:
        control_vol = (ao_control_vol_t *)arg;

        if (p->b_digital) {
            // Digital output can not set volume. Here we have to return true
            // to make mixer forget it. Else mixer will add a soft filter,
            // that's not we expected and the filter not support ac3 stream
            // will cause mplayer die.

            // Although not support set volume, but at least we support mute.
            // MPlayer set mute by set volume to zero, we handle it.
            if (control_vol->left == 0 && control_vol->right == 0)
                p->b_muted = 1;
            else
                p->b_muted = 0;
            return CONTROL_TRUE;
        }

        vol = (control_vol->left + control_vol->right) * 4.0 / 200.0;
        err = AudioUnitSetParameter(p->theOutputUnit, kHALOutputParam_Volume,
                                    kAudioUnitScope_Global, 0, vol, 0);

        CHECK_CA_ERROR("could not set HAL output volume");
        return CONTROL_TRUE;

    } // end switch
    return CONTROL_UNKNOWN;

coreaudio_error:
    return CONTROL_ERROR;
}

static int OpenSPDIF(struct ao *ao);
static int AudioStreamChangeFormat(AudioStreamID i_stream_id,
                                   AudioStreamBasicDescription change_format);

static void print_help(void)
{
    OSStatus err;
    UInt32 i_param_size;
    int num_devices;
    AudioDeviceID *devids;
    char *device_name;

    ca_msg(MSGL_FATAL,
           "\n-ao coreaudio commandline help:\n"
           "Example: mpv -ao coreaudio:device_id=266\n"
           "    open Core Audio with output device ID 266.\n"
           "\nOptions:\n"
           "    device_id\n"
           "        ID of output device to use (0 = default device)\n"
           "    help\n"
           "        This help including list of available devices.\n"
           "\n"
           "Available output devices:\n");

    i_param_size = GetGlobalAudioPropertyArray(kAudioObjectSystemObject,
                                               kAudioHardwarePropertyDevices,
                                               (void **)&devids);

    if (!i_param_size) {
        ca_msg(MSGL_FATAL, "Failed to get list of output devices.\n");
        return;
    }

    num_devices = i_param_size / sizeof(AudioDeviceID);

    for (int i = 0; i < num_devices; ++i) {
        err = GetAudioPropertyString(devids[i], kAudioObjectPropertyName,
                                     &device_name);

        if (err == noErr) {
            ca_msg(MSGL_FATAL, "%s (id: %" PRIu32 ")\n", device_name, devids[i]);
            free(device_name);
        } else
            ca_msg(MSGL_FATAL, "Unknown (id: %" PRIu32 ")\n", devids[i]);
    }

    free(devids);
}

static int init(struct ao *ao, char *params)
{
    // int rate, int channels, int format, int flags)
    struct priv *p = talloc_zero(ao, struct priv);
    ao->priv = p;

    AudioStreamBasicDescription inDesc;
    AudioComponentDescription desc;
    AudioComponent comp;
    AURenderCallbackStruct renderCallback;
    OSStatus err;
    UInt32 size, maxFrames, b_alive;
    char *psz_name;
    AudioDeviceID devid_def = 0;
    int device_id, display_help = 0;

    const opt_t subopts[] = {
        {"device_id", OPT_ARG_INT, &device_id, NULL},
        {"help", OPT_ARG_BOOL, &display_help, NULL},
        {NULL}
    };

    // set defaults
    device_id = 0;

    if (subopt_parse(ao_subdevice, subopts) != 0 || display_help) {
        print_help();
        if (!display_help)
            return 0;
    }

    ca_msg(MSGL_V, "init([%dHz][%dch][%s][%d])\n",
        ao->samplerate, ao->channels.num, af_fmt2str_short(ao->format), 0);

    p->i_selected_dev = 0;
    p->b_supports_digital = 0;
    p->b_digital = 0;
    p->b_muted = 0;
    p->b_stream_format_changed = 0;
    p->i_hog_pid = -1;
    p->i_stream_id = 0;
    p->i_stream_index = -1;
    p->b_revert = 0;
    p->b_changed_mixing = 0;

    ao->per_application_mixer = true;
    ao->no_persistent_volume  = true;

    if (device_id == 0) {
        /* Find the ID of the default Device. */
        err = GetAudioProperty(kAudioObjectSystemObject,
                               kAudioHardwarePropertyDefaultOutputDevice,
                               sizeof(UInt32), &devid_def);
        if (err != noErr) {
            ca_msg(MSGL_WARN,
                   "could not get default audio device: [%4.4s]\n",
                   (char *)&err);
            goto err_out;
        }
    } else {
        devid_def = device_id;
    }

    /* Retrieve the name of the device. */
    err = GetAudioPropertyString(devid_def,
                                 kAudioObjectPropertyName,
                                 &psz_name);
    if (err != noErr) {
        ca_msg(MSGL_WARN,
               "could not get default audio device name: [%4.4s]\n",
               (char *)&err);
        goto err_out;
    }

    ca_msg(MSGL_V,
           "got audio output device ID: %" PRIu32 " Name: %s\n", devid_def,
           psz_name);

    /* Probe whether device support S/PDIF stream output if input is AC3 stream. */
    if (AF_FORMAT_IS_AC3(ao->format)) {
        if (AudioDeviceSupportsDigital(devid_def))
            p->b_supports_digital = 1;
        ca_msg(MSGL_V,
               "probe default audio output device about support for digital s/pdif output: %d\n",
               p->b_supports_digital);
    }

    free(psz_name);

    // Save selected device id
    p->i_selected_dev = devid_def;

    struct mp_chmap_sel chmap_sel = {0};
    mp_chmap_sel_add_waveext(&chmap_sel);
    if (!ao_chmap_sel_adjust(ao, &chmap_sel, &ao->channels))
        goto err_out;

    // Build Description for the input format
    inDesc.mSampleRate = ao->samplerate;
    inDesc.mFormatID =
        p->b_supports_digital ? kAudioFormat60958AC3 : kAudioFormatLinearPCM;
    inDesc.mChannelsPerFrame = ao->channels.num;
    inDesc.mBitsPerChannel = af_fmt2bits(ao->format);

    if ((ao->format & AF_FORMAT_POINT_MASK) == AF_FORMAT_F) {
        // float
        inDesc.mFormatFlags = kAudioFormatFlagIsFloat |
                              kAudioFormatFlagIsPacked;
    } else if ((ao->format & AF_FORMAT_SIGN_MASK) == AF_FORMAT_SI) {
        // signed int
        inDesc.mFormatFlags = kAudioFormatFlagIsSignedInteger |
                              kAudioFormatFlagIsPacked;
    } else {
        // unsigned int
        inDesc.mFormatFlags = kAudioFormatFlagIsPacked;
    }
    if ((ao->format & AF_FORMAT_END_MASK) == AF_FORMAT_BE)
        inDesc.mFormatFlags |= kAudioFormatFlagIsBigEndian;

    inDesc.mFramesPerPacket = 1;
    p->packetSize = inDesc.mBytesPerPacket = inDesc.mBytesPerFrame =
                                                  inDesc.mFramesPerPacket *
                                                  ao->channels.num *
                                                  (inDesc.mBitsPerChannel / 8);
    ca_print_asbd("source format:", &inDesc);

    if (p->b_supports_digital) {
        b_alive = 1;
        err = GetAudioProperty(p->i_selected_dev,
                               kAudioDevicePropertyDeviceIsAlive,
                               sizeof(UInt32), &b_alive);
        if (err != noErr)
            ca_msg(MSGL_WARN,
                   "could not check whether device is alive: [%4.4s]\n",
                   (char *)&err);
        if (!b_alive)
            ca_msg(MSGL_WARN, "device is not alive\n");

        /* S/PDIF output need device in HogMode. */
        err = GetAudioProperty(p->i_selected_dev,
                               kAudioDevicePropertyHogMode,
                               sizeof(pid_t), &p->i_hog_pid);
        if (err != noErr) {
            /* This is not a fatal error. Some drivers simply don't support this property. */
            ca_msg(MSGL_WARN,
                   "could not check whether device is hogged: [%4.4s]\n",
                   (char *)&err);
            p->i_hog_pid = -1;
        }

        if (p->i_hog_pid != -1 && p->i_hog_pid != getpid()) {
            ca_msg(MSGL_WARN,
                   "Selected audio device is exclusively in use by another program.\n");
            goto err_out;
        }
        p->stream_format = inDesc;
        return OpenSPDIF(ao);
    }

    /* original analog output code */
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType =
        (device_id ==
         0) ? kAudioUnitSubType_DefaultOutput : kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    comp = AudioComponentFindNext(NULL, &desc);      //Finds an component that meets the desc spec's
    if (comp == NULL) {
        ca_msg(MSGL_WARN, "Unable to find Output Unit component\n");
        goto err_out;
    }

    err = AudioComponentInstanceNew(comp, &(p->theOutputUnit));      //gains access to the services provided by the component
    if (err) {
        ca_msg(MSGL_WARN,
               "Unable to open Output Unit component: [%4.4s]\n", (char *)&err);
        goto err_out;
    }

    // Initialize AudioUnit
    err = AudioUnitInitialize(p->theOutputUnit);
    if (err) {
        ca_msg(MSGL_WARN,
               "Unable to initialize Output Unit component: [%4.4s]\n",
               (char *)&err);
        goto err_out1;
    }

    size =  sizeof(AudioStreamBasicDescription);
    err = AudioUnitSetProperty(p->theOutputUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &inDesc, size);

    if (err) {
        ca_msg(MSGL_WARN, "Unable to set the input format: [%4.4s]\n",
               (char *)&err);
        goto err_out2;
    }

    size = sizeof(UInt32);
    err = AudioUnitGetProperty(p->theOutputUnit,
                               kAudioDevicePropertyBufferSize,
                               kAudioUnitScope_Input, 0, &maxFrames, &size);

    if (err) {
        ca_msg(MSGL_WARN,
               "AudioUnitGetProperty returned [%4.4s] when getting kAudioDevicePropertyBufferSize\n",
               (char *)&err);
        goto err_out2;
    }

    //Set the Current Device to the Default Output Unit.
    err = AudioUnitSetProperty(p->theOutputUnit,
                               kAudioOutputUnitProperty_CurrentDevice,
                               kAudioUnitScope_Global, 0, &p->i_selected_dev,
                               sizeof(p->i_selected_dev));

    ao->samplerate = inDesc.mSampleRate;

    if (!ao_chmap_sel_get_def(ao, &chmap_sel, &ao->channels,
                              inDesc.mChannelsPerFrame))
        goto err_out2;

    ao->bps        = ao->samplerate * inDesc.mBytesPerFrame;
    p->buffer      = mp_ring_new(p, get_ring_size(ao));

    print_buffer(p->buffer);

    renderCallback.inputProc = render_cb_lpcm;
    renderCallback.inputProcRefCon = ao;
    err = AudioUnitSetProperty(p->theOutputUnit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input, 0, &renderCallback,
                               sizeof(AURenderCallbackStruct));
    if (err) {
        ca_msg(MSGL_WARN,
               "Unable to set the render callback: [%4.4s]\n", (char *)&err);
        goto err_out2;
    }

    reset(ao);

    return CONTROL_OK;

err_out2:
    AudioUnitUninitialize(p->theOutputUnit);
err_out1:
    AudioComponentInstanceDispose(p->theOutputUnit);
err_out:
    return CONTROL_FALSE;
}

/*****************************************************************************
* Setup a encoded digital stream (SPDIF)
*****************************************************************************/
static int OpenSPDIF(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSStatus err = noErr;
    UInt32 i_param_size, b_mix = 0;
    Boolean b_writeable = 0;
    AudioStreamID *p_streams = NULL;
    int i, i_streams = 0;
    AudioObjectPropertyAddress p_addr;

    /* Start doing the SPDIF setup process. */
    p->b_digital = 1;

    /* Hog the device. */
    p->i_hog_pid = getpid();

    err = SetAudioProperty(p->i_selected_dev,
                           kAudioDevicePropertyHogMode,
                           sizeof(p->i_hog_pid), &p->i_hog_pid);
    if (err != noErr) {
        ca_msg(MSGL_WARN, "failed to set hogmode: [%4.4s]\n",
               (char *)&err);
        p->i_hog_pid = -1;
        goto err_out;
    }

    p_addr.mSelector = kAudioDevicePropertySupportsMixing;
    p_addr.mScope    = kAudioObjectPropertyScopeGlobal;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    /* Set mixable to false if we are allowed to. */
    if (AudioObjectHasProperty(p->i_selected_dev, &p_addr)) {
        /* Set mixable to false if we are allowed to. */
        err = IsAudioPropertySettable(p->i_selected_dev,
                                      kAudioDevicePropertySupportsMixing,
                                      &b_writeable);
        err = GetAudioProperty(p->i_selected_dev,
                               kAudioDevicePropertySupportsMixing,
                               sizeof(UInt32), &b_mix);
        if (err == noErr && b_writeable) {
            b_mix = 0;
            err = SetAudioProperty(p->i_selected_dev,
                                   kAudioDevicePropertySupportsMixing,
                                   sizeof(UInt32), &b_mix);
            p->b_changed_mixing = 1;
        }
        if (err != noErr) {
            ca_msg(MSGL_WARN, "failed to set mixmode: [%4.4s]\n",
                   (char *)&err);
            goto err_out;
        }
    }

    /* Get a list of all the streams on this device. */
    i_param_size = GetAudioPropertyArray(p->i_selected_dev,
                                         kAudioDevicePropertyStreams,
                                         kAudioDevicePropertyScopeOutput,
                                         (void **)&p_streams);

    if (!i_param_size) {
        ca_msg(MSGL_WARN, "could not get number of streams.\n");
        goto err_out;
    }

    i_streams = i_param_size / sizeof(AudioStreamID);

    ca_msg(MSGL_V, "current device stream number: %d\n", i_streams);

    for (i = 0; i < i_streams && p->i_stream_index < 0; ++i) {
        /* Find a stream with a cac3 stream. */
        AudioStreamRangedDescription *p_format_list = NULL;
        int i_formats = 0, j = 0, b_digital = 0;

        i_param_size = GetGlobalAudioPropertyArray(p_streams[i],
                                                   kAudioStreamPropertyAvailablePhysicalFormats,
                                                   (void **)&p_format_list);

        if (!i_param_size) {
            ca_msg(MSGL_WARN,
                   "Could not get number of stream formats.\n");
            continue;
        }

        i_formats = i_param_size / sizeof(AudioStreamRangedDescription);

        /* Check if one of the supported formats is a digital format. */
        for (j = 0; j < i_formats; ++j) {
            if (p_format_list[j].mFormat.mFormatID == 'IAC3' ||
                p_format_list[j].mFormat.mFormatID == 'iac3' ||
                p_format_list[j].mFormat.mFormatID == kAudioFormat60958AC3 ||
                p_format_list[j].mFormat.mFormatID == kAudioFormatAC3) {
                b_digital = 1;
                break;
            }
        }

        if (b_digital) {
            /* If this stream supports a digital (cac3) format, then set it. */
            int i_requested_rate_format = -1;
            int i_current_rate_format = -1;
            int i_backup_rate_format = -1;

            p->i_stream_id = p_streams[i];
            p->i_stream_index = i;

            if (p->b_revert == 0) {
                /* Retrieve the original format of this stream first if not done so already. */
                err = GetAudioProperty(p->i_stream_id,
                                       kAudioStreamPropertyPhysicalFormat,
                                       sizeof(p->sfmt_revert),
                                       &p->sfmt_revert);
                if (err != noErr) {
                    ca_msg(MSGL_WARN,
                           "Could not retrieve the original stream format: [%4.4s]\n",
                           (char *)&err);
                    free(p_format_list);
                    continue;
                }
                p->b_revert = 1;
            }

            for (j = 0; j < i_formats; ++j)
                if (p_format_list[j].mFormat.mFormatID == 'IAC3' ||
                    p_format_list[j].mFormat.mFormatID == 'iac3' ||
                    p_format_list[j].mFormat.mFormatID ==
                    kAudioFormat60958AC3 ||
                    p_format_list[j].mFormat.mFormatID == kAudioFormatAC3) {
                    if (p_format_list[j].mFormat.mSampleRate ==
                        p->stream_format.mSampleRate) {
                        i_requested_rate_format = j;
                        break;
                    }
                    if (p_format_list[j].mFormat.mSampleRate ==
                        p->sfmt_revert.mSampleRate)
                        i_current_rate_format = j;
                    else if (i_backup_rate_format < 0 ||
                             p_format_list[j].mFormat.mSampleRate >
                             p_format_list[i_backup_rate_format].mFormat.
                             mSampleRate)
                        i_backup_rate_format = j;
                }

            if (i_requested_rate_format >= 0) /* We prefer to output at the samplerate of the original audio. */
                p->stream_format =
                    p_format_list[i_requested_rate_format].mFormat;
            else if (i_current_rate_format >= 0) /* If not possible, we will try to use the current samplerate of the device. */
                p->stream_format =
                    p_format_list[i_current_rate_format].mFormat;
            else
                p->stream_format = p_format_list[i_backup_rate_format].mFormat;
            /* And if we have to, any digital format will be just fine (highest rate possible). */
        }
        free(p_format_list);
    }
    free(p_streams);

    if (p->i_stream_index < 0) {
        ca_msg(MSGL_WARN,
               "Cannot find any digital output stream format when OpenSPDIF().\n");
        goto err_out;
    }

    ca_print_asbd("original stream format:", &p->sfmt_revert);

    if (!AudioStreamChangeFormat(p->i_stream_id, p->stream_format))
        goto err_out;

    p_addr.mSelector = kAudioDevicePropertyDeviceHasChanged;
    p_addr.mScope    = kAudioObjectPropertyScopeGlobal;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    const int *stream_format_changed = &(p->b_stream_format_changed);
    err = AudioObjectAddPropertyListener(p->i_selected_dev,
                                         &p_addr,
                                         ca_device_listener,
                                         (void *)stream_format_changed);
    if (err != noErr)
        ca_msg(MSGL_WARN,
               "AudioDeviceAddPropertyListener for kAudioDevicePropertyDeviceHasChanged failed: [%4.4s]\n",
               (char *)&err);


    /* FIXME: If output stream is not native byte-order, we need change endian somewhere. */
    /*        Although there's no such case reported.                                     */
#if BYTE_ORDER == BIG_ENDIAN
    if (!(p->stream_format.mFormatFlags & kAudioFormatFlagIsBigEndian))
#else
    /* tell mplayer that we need a byteswap on AC3 streams, */
    if (p->stream_format.mFormatID & kAudioFormat60958AC3)
        ao->format = AF_FORMAT_AC3_LE;

    if (p->stream_format.mFormatFlags & kAudioFormatFlagIsBigEndian)
#endif
        ca_msg(MSGL_WARN,
               "Output stream has non-native byte order, digital output may fail.\n");


    ao->samplerate = p->stream_format.mSampleRate;
    mp_chmap_from_channels(&ao->channels, p->stream_format.mChannelsPerFrame);
    ao->bps = ao->samplerate *
                  (p->stream_format.mBytesPerPacket /
                   p->stream_format.mFramesPerPacket);

    p->buffer      = mp_ring_new(p, get_ring_size(ao));

    print_buffer(p->buffer);

    /* Create IOProc callback. */
    err = AudioDeviceCreateIOProcID(p->i_selected_dev,
                                    (AudioDeviceIOProc)render_cb_digital,
                                    (void *)ao,
                                    &p->renderCallback);

    if (err != noErr || p->renderCallback == NULL) {
        ca_msg(MSGL_WARN, "AudioDeviceAddIOProc failed: [%4.4s]\n",
               (char *)&err);
        goto err_out1;
    }

    reset(ao);

    return CONTROL_TRUE;

err_out1:
    if (p->b_revert)
        AudioStreamChangeFormat(p->i_stream_id, p->sfmt_revert);
err_out:
    if (p->b_changed_mixing && p->sfmt_revert.mFormatID !=
        kAudioFormat60958AC3) {
        int b_mix = 1;
        err = SetAudioProperty(p->i_selected_dev,
                               kAudioDevicePropertySupportsMixing,
                               sizeof(int), &b_mix);
        if (err != noErr)
            ca_msg(MSGL_WARN, "failed to set mixmode: [%4.4s]\n",
                   (char *)&err);
    }
    if (p->i_hog_pid == getpid()) {
        p->i_hog_pid = -1;
        err = SetAudioProperty(p->i_selected_dev,
                               kAudioDevicePropertyHogMode,
                               sizeof(p->i_hog_pid), &p->i_hog_pid);
        if (err != noErr)
            ca_msg(MSGL_WARN, "Could not release hogmode: [%4.4s]\n",
                   (char *)&err);
    }
    return CONTROL_FALSE;
}

/*****************************************************************************
* AudioStreamChangeFormat: Change i_stream_id to change_format
*****************************************************************************/
static int AudioStreamChangeFormat(AudioStreamID i_stream_id,
                                   AudioStreamBasicDescription change_format)
{
    OSStatus err = noErr;
    int i;
    AudioObjectPropertyAddress p_addr;

    static volatile int stream_format_changed;
    stream_format_changed = 0;

    ca_print_asbd("setting stream format:", &change_format);

    /* Install the callback. */
    p_addr.mSelector = kAudioStreamPropertyPhysicalFormat;
    p_addr.mScope    = kAudioObjectPropertyScopeGlobal;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    err = AudioObjectAddPropertyListener(i_stream_id,
                                         &p_addr,
                                         ca_stream_listener,
                                         (void *)&stream_format_changed);
    if (err != noErr) {
        ca_msg(MSGL_WARN,
               "AudioStreamAddPropertyListener failed: [%4.4s]\n",
               (char *)&err);
        return CONTROL_FALSE;
    }

    /* Change the format. */
    err = SetAudioProperty(i_stream_id,
                           kAudioStreamPropertyPhysicalFormat,
                           sizeof(AudioStreamBasicDescription), &change_format);
    if (err != noErr) {
        ca_msg(MSGL_WARN, "could not set the stream format: [%4.4s]\n",
               (char *)&err);
        return CONTROL_FALSE;
    }

    /* The AudioStreamSetProperty is not only asynchronious,
     * it is also not Atomic, in its behaviour.
     * Therefore we check 5 times before we really give up.
     * FIXME: failing isn't actually implemented yet. */
    for (i = 0; i < 5; ++i) {
        AudioStreamBasicDescription actual_format;
        int j;
        for (j = 0; !stream_format_changed && j < 50; ++j)
            mp_sleep_us(10000);
        if (stream_format_changed)
            stream_format_changed = 0;
        else
            ca_msg(MSGL_V, "reached timeout\n");

        err = GetAudioProperty(i_stream_id,
                               kAudioStreamPropertyPhysicalFormat,
                               sizeof(AudioStreamBasicDescription),
                               &actual_format);

        ca_print_asbd("actual format in use:", &actual_format);
        if (actual_format.mSampleRate == change_format.mSampleRate &&
            actual_format.mFormatID == change_format.mFormatID &&
            actual_format.mFramesPerPacket == change_format.mFramesPerPacket) {
            /* The right format is now active. */
            break;
        }
        /* We need to check again. */
    }

    /* Removing the property listener. */
    err = AudioObjectRemovePropertyListener(i_stream_id,
                                            &p_addr,
                                            ca_stream_listener,
                                            (void *)&stream_format_changed);
    if (err != noErr) {
        ca_msg(MSGL_WARN,
               "AudioStreamRemovePropertyListener failed: [%4.4s]\n",
               (char *)&err);
        return CONTROL_FALSE;
    }

    return CONTROL_TRUE;
}

static int play(struct ao *ao, void *output_samples, int num_bytes, int flags)
{
    struct priv *p = ao->priv;
    int wrote, b_digital;

    // Check whether we need to reset the digital output stream.
    if (p->b_digital && p->b_stream_format_changed) {
        p->b_stream_format_changed = 0;
        b_digital = AudioStreamSupportsDigital(p->i_stream_id);
        if (b_digital) {
            /* Current stream supports digital format output, let's set it. */
            ca_msg(MSGL_V,
                   "Detected current stream supports digital, try to restore digital output...\n");

            if (!AudioStreamChangeFormat(p->i_stream_id, p->stream_format))
                ca_msg(MSGL_WARN,
                       "Restoring digital output failed.\n");
            else {
                ca_msg(MSGL_WARN,
                       "Restoring digital output succeeded.\n");
                reset(ao);
            }
        } else
            ca_msg(MSGL_V,
                   "Detected current stream does not support digital.\n");
    }

    wrote = mp_ring_write(p->buffer, output_samples, num_bytes);
    audio_resume(ao);

    return wrote;
}

/* set variables and buffer to initial state */
static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    audio_pause(ao);
    mp_ring_reset(p->buffer);
}


/* return available space */
static int get_space(struct ao *ao)
{
    struct priv *p = ao->priv;
    return mp_ring_available(p->buffer);
}


/* return delay until audio is played */
static float get_delay(struct ao *ao)
{
    // inaccurate, should also contain the data buffered e.g. by the OS
    struct priv *p = ao->priv;
    return mp_ring_buffered(p->buffer) / (float)ao->bps;
}

static void uninit(struct ao *ao, bool immed)
{
    struct priv *p = ao->priv;
    OSStatus err = noErr;

    if (!immed) {
        long long timeleft =
            (1000000LL * mp_ring_buffered(p->buffer)) / ao->bps;
        ca_msg(MSGL_DBG2, "%d bytes left @%d bps (%d usec)\n",
                mp_ring_buffered(p->buffer), ao->bps, (int)timeleft);
        mp_sleep_us((int)timeleft);
    }

    if (!p->b_digital) {
        AudioOutputUnitStop(p->theOutputUnit);
        AudioUnitUninitialize(p->theOutputUnit);
        AudioComponentInstanceDispose(p->theOutputUnit);
    } else {
        /* Stop device. */
        err = AudioDeviceStop(p->i_selected_dev, p->renderCallback);
        if (err != noErr)
            ca_msg(MSGL_WARN, "AudioDeviceStop failed: [%4.4s]\n",
                   (char *)&err);

        /* Remove IOProc callback. */
        err =
            AudioDeviceDestroyIOProcID(p->i_selected_dev, p->renderCallback);
        if (err != noErr)
            ca_msg(MSGL_WARN,
                   "AudioDeviceRemoveIOProc failed: [%4.4s]\n", (char *)&err);

        if (p->b_revert)
            AudioStreamChangeFormat(p->i_stream_id, p->sfmt_revert);

        if (p->b_changed_mixing && p->sfmt_revert.mFormatID !=
            kAudioFormat60958AC3) {
            UInt32 b_mix;
            Boolean b_writeable = 0;
            /* Revert mixable to true if we are allowed to. */
            err = IsAudioPropertySettable(p->i_selected_dev,
                                          kAudioDevicePropertySupportsMixing,
                                          &b_writeable);
            err = GetAudioProperty(p->i_selected_dev,
                                   kAudioDevicePropertySupportsMixing,
                                   sizeof(UInt32), &b_mix);
            if (err == noErr && b_writeable) {
                b_mix = 1;
                err = SetAudioProperty(p->i_selected_dev,
                                       kAudioDevicePropertySupportsMixing,
                                       sizeof(UInt32), &b_mix);
            }
            if (err != noErr)
                ca_msg(MSGL_WARN, "failed to set mixmode: [%4.4s]\n",
                       (char *)&err);
        }
        if (p->i_hog_pid == getpid()) {
            p->i_hog_pid = -1;
            err = SetAudioProperty(p->i_selected_dev,
                                   kAudioDevicePropertyHogMode,
                                   sizeof(p->i_hog_pid), &p->i_hog_pid);
            if (err != noErr)
                ca_msg(MSGL_WARN,
                       "Could not release hogmode: [%4.4s]\n", (char *)&err);
        }
    }
}

/* stop playing, keep buffers (for pause) */
static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSErr err = noErr;

    /* Stop callback. */
    if (!p->b_digital) {
        err = AudioOutputUnitStop(p->theOutputUnit);
        if (err != noErr)
            ca_msg(MSGL_WARN, "AudioOutputUnitStop returned [%4.4s]\n",
                   (char *)&err);
    } else {
        err = AudioDeviceStop(p->i_selected_dev, p->renderCallback);
        if (err != noErr)
            ca_msg(MSGL_WARN, "AudioDeviceStop failed: [%4.4s]\n",
                   (char *)&err);
    }
    p->paused = 1;
}


/* resume playing, after audio_pause() */
static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSErr err = noErr;

    if (!p->paused)
        return;

    /* Start callback. */
    if (!p->b_digital) {
        err = AudioOutputUnitStart(p->theOutputUnit);
        if (err != noErr)
            ca_msg(MSGL_WARN,
                   "AudioOutputUnitStart returned [%4.4s]\n", (char *)&err);
    } else {
        err = AudioDeviceStart(p->i_selected_dev, p->renderCallback);
        if (err != noErr)
            ca_msg(MSGL_WARN, "AudioDeviceStart failed: [%4.4s]\n",
                   (char *)&err);
    }
    p->paused = 0;
}

const struct ao_driver audio_out_coreaudio = {
    .info = &(const struct ao_info) {
        "CoreAudio (Native OS X Audio Output)",
        "coreaudio",
        "Timothy J. Wood, Dan Christiansen, Chris Roccati & Stefano Pigozzi",
        "",
    },
    .uninit    = uninit,
    .init      = init,
    .play      = play,
    .control   = control,
    .get_space = get_space,
    .get_delay = get_delay,
    .reset     = reset,
    .pause     = audio_pause,
    .resume    = audio_resume,
};
