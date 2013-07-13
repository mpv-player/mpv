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

/*
 * This file contains functions interacting with the CoreAudio framework
 * that are not specific to the AUHAL. These are split in a separate file for
 * the sake of readability. In the future the could be used by other AOs based
 * on CoreAudio but not the AUHAL (such as using AudioQueue services).
 */

#include "audio/out/ao_coreaudio_utils.h"
#include "audio/out/ao_coreaudio_properties.h"
#include "osdep/timer.h"

#define ca_msg(a, b ...) mp_msg(MSGT_AO, a, "AO: [coreaudio] " b)
#define CA_CFSTR_ENCODING kCFStringEncodingASCII

char *fourcc_repr(void *talloc_ctx, uint32_t code)
{
    // Extract FourCC letters from the uint32_t and finde out if it's a valid
    // code that is made of letters.
    char fcc[4] = {
        (code >> 24) & 0xFF,
        (code >> 16) & 0xFF,
        (code >> 8)  & 0xFF,
        code         & 0xFF,
    };

    bool valid_fourcc = true;
    for (int i = 0; i < 4; i++)
        if (!isprint(fcc[i]))
            valid_fourcc = false;

    char *repr;
    if (valid_fourcc)
        repr = talloc_asprintf(talloc_ctx, "'%c%c%c%c'",
                               fcc[0], fcc[1], fcc[2], fcc[3]);
    else
        repr = talloc_asprintf(NULL, "%d", code);

    return repr;
}

bool check_ca_st(int level, OSStatus code, const char *message)
{
    if (code == noErr) return true;

    char *error_string = fourcc_repr(NULL, code);
    ca_msg(level, "%s (%s)\n", message, error_string);
    talloc_free(error_string);

    return false;
}

void ca_print_asbd(const char *description,
                   const AudioStreamBasicDescription *asbd)
{
    uint32_t flags  = asbd->mFormatFlags;
    char *format    = fourcc_repr(NULL, asbd->mFormatID);

    ca_msg(MSGL_V,
       "%s %7.1fHz %" PRIu32 "bit [%s]"
       "[%" PRIu32 "][%" PRIu32 "][%" PRIu32 "]"
       "[%" PRIu32 "][%" PRIu32 "] "
       "%s %s %s%s%s%s\n",
       description, asbd->mSampleRate, asbd->mBitsPerChannel, format,
       asbd->mFormatFlags, asbd->mBytesPerPacket, asbd->mFramesPerPacket,
       asbd->mBytesPerFrame, asbd->mChannelsPerFrame,
       (flags & kAudioFormatFlagIsFloat) ? "float" : "int",
       (flags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE",
       (flags & kAudioFormatFlagIsSignedInteger) ? "S" : "U",
       (flags & kAudioFormatFlagIsPacked) ? " packed" : "",
       (flags & kAudioFormatFlagIsAlignedHigh) ? " aligned" : "",
       (flags & kAudioFormatFlagIsNonInterleaved) ? " P" : "");

    talloc_free(format);
}

bool ca_format_is_digital(AudioStreamBasicDescription asbd)
{
    switch (asbd.mFormatID)
    case 'IAC3':
    case 'iac3':
    case  kAudioFormat60958AC3:
    case  kAudioFormatAC3:
        return true;
    return false;
}

bool ca_stream_supports_digital(AudioStreamID stream)
{
    AudioStreamRangedDescription *formats = NULL;

    /* Retrieve all the stream formats supported by each output stream. */
    uint32_t size =
        GetGlobalAudioPropertyArray(stream,
                                    kAudioStreamPropertyAvailablePhysicalFormats,
                                    (void **)&formats);

    if (!size) {
        ca_msg(MSGL_WARN, "Could not get number of stream formats.\n");
        return false;
    }

    const int n_formats = size / sizeof(AudioStreamRangedDescription);
    for (int i = 0; i < n_formats; i++) {
        AudioStreamBasicDescription asbd = formats[i].mFormat;
        ca_print_asbd("supported format:", &(asbd));
        if (ca_format_is_digital(asbd)) {
            free(formats);
            return true;
        }
    }

    free(formats);
    return false;
}

bool ca_device_supports_digital(AudioDeviceID device)
{
    AudioStreamID *streams = NULL;

    /* Retrieve all the output streams. */
    uint32_t size = GetAudioPropertyArray(device,
                                          kAudioDevicePropertyStreams,
                                          kAudioDevicePropertyScopeOutput,
                                          (void **)&streams);

    if (!size) {
        ca_msg(MSGL_WARN, "could not get number of streams.\n");
        return CONTROL_FALSE;
    }

    const int n_streams = size / sizeof(AudioStreamID);
    for (int i = 0; i < n_streams; i++) {
        if (ca_stream_supports_digital(streams[i])) {
            free(streams);
            return true;
        }
    }

    free(streams);
    return false;
}

OSStatus ca_property_listener(AudioObjectPropertySelector selector,
                              AudioObjectID object, uint32_t n_addresses,
                              const AudioObjectPropertyAddress addresses[],
                              void *data)
{
    void *talloc_ctx = talloc_new(NULL);

    for (int i = 0; i < n_addresses; i++) {
        if (addresses[i].mSelector == selector) {
            ca_msg(MSGL_WARN, "event: property %s changed\n",
                              fourcc_repr(talloc_ctx, selector));
            if (data) *(volatile int *)data = 1;
            break;
        }
    }
    talloc_free(talloc_ctx);
    return noErr;
}

OSStatus ca_stream_listener(AudioObjectID object, uint32_t n_addresses,
                            const AudioObjectPropertyAddress addresses[],
                            void *data)
{
    return ca_property_listener(kAudioStreamPropertyPhysicalFormat,
                                object, n_addresses, addresses, data);
}

OSStatus ca_device_listener(AudioObjectID object, uint32_t n_addresses,
                            const AudioObjectPropertyAddress addresses[],
                            void *data)
{
    return ca_property_listener(kAudioDevicePropertyDeviceHasChanged,
                                object, n_addresses, addresses, data);
}

OSStatus ca_lock_device(AudioDeviceID device, pid_t *pid) {
    *pid = getpid();
    OSStatus err = SetAudioProperty(device, kAudioDevicePropertyHogMode,
                                    sizeof(*pid), pid);
    if (err != noErr)
        *pid = -1;

    return err;
}

OSStatus ca_unlock_device(AudioDeviceID device, pid_t *pid) {
    if (*pid == getpid()) {
        *pid = -1;
        return SetAudioProperty(device, kAudioDevicePropertyHogMode,
                                sizeof(*pid), &pid);
    }
    return noErr;
}

static OSStatus ca_change_mixing(AudioDeviceID device, uint32_t val,
                                 bool *changed) {
    *changed = false;

    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioDevicePropertySupportsMixing,
        .mScope    = kAudioObjectPropertyScopeGlobal,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    if (AudioObjectHasProperty(device, &p_addr)) {
        OSStatus err;
        Boolean writeable = 0;
        err = IsAudioPropertySettable(device, kAudioDevicePropertySupportsMixing,
                                      &writeable);

        if (!CHECK_CA_WARN("can't tell if mixing property is settable")) {
            return err;
        }

        if (!writeable)
            return noErr;

        err = SetAudioProperty(device, kAudioDevicePropertySupportsMixing,
                               sizeof(uint32_t), &val);
        if (err != noErr)
            return err;

        if (!CHECK_CA_WARN("can't set mix mode")) {
            return err;
        }

        *changed = true;
    }

    return noErr;
}

OSStatus ca_disable_mixing(AudioDeviceID device, bool *changed) {
    return ca_change_mixing(device, 0, changed);
}

OSStatus ca_enable_mixing(AudioDeviceID device, bool changed) {
    if (changed) {
        bool dont_care = false;
        return ca_change_mixing(device, 1, &dont_care);
    }

    return noErr;
}

static OSStatus ca_change_device_listening(AudioDeviceID device,
                                           void *flag, bool enabled)
{
    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioDevicePropertyDeviceHasChanged,
        .mScope    = kAudioObjectPropertyScopeGlobal,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    if (enabled) {
        return AudioObjectAddPropertyListener(
            device, &p_addr, ca_device_listener, flag);
    } else {
        return AudioObjectRemovePropertyListener(
            device, &p_addr, ca_device_listener, flag);
    }
}

OSStatus ca_enable_device_listener(AudioDeviceID device, void *flag) {
    return ca_change_device_listening(device, flag, true);
}

OSStatus ca_disable_device_listener(AudioDeviceID device, void *flag) {
    return ca_change_device_listening(device, flag, false);
}

bool ca_change_format(AudioStreamID stream,
                      AudioStreamBasicDescription change_format)
{
    OSStatus err = noErr;
    AudioObjectPropertyAddress p_addr;
    volatile int stream_format_changed = 0;

    ca_print_asbd("setting stream format:", &change_format);

    /* Install the callback. */
    p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioStreamPropertyPhysicalFormat,
        .mScope    = kAudioObjectPropertyScopeGlobal,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    err = AudioObjectAddPropertyListener(stream, &p_addr, ca_stream_listener,
                                         (void *)&stream_format_changed);
    if (!CHECK_CA_WARN("can't add property listener during format change")) {
        return false;
    }

    /* Change the format. */
    err = SetAudioProperty(stream, kAudioStreamPropertyPhysicalFormat,
                           sizeof(AudioStreamBasicDescription), &change_format);
    if (!CHECK_CA_WARN("error changing physical format")) {
        return false;
    }

    /* The AudioStreamSetProperty is not only asynchronious,
     * it is also not Atomic, in its behaviour.
     * Therefore we check 5 times before we really give up. */
    bool format_set = false;
    for (int i = 0; !format_set && i < 5; i++) {
        for (int j = 0; !stream_format_changed && j < 50; j++)
            mp_sleep_us(10000);

        if (stream_format_changed) {
            stream_format_changed = 0;
        } else {
            ca_msg(MSGL_V, "reached timeout\n");
        }

        AudioStreamBasicDescription actual_format;
        err = CA_GET(stream, kAudioStreamPropertyPhysicalFormat, &actual_format);

        ca_print_asbd("actual format in use:", &actual_format);
        if (actual_format.mSampleRate == change_format.mSampleRate &&
            actual_format.mFormatID == change_format.mFormatID &&
            actual_format.mFramesPerPacket == change_format.mFramesPerPacket) {
            format_set = true;
        }
    }

    err = AudioObjectRemovePropertyListener(stream, &p_addr, ca_stream_listener,
                                            (void *)&stream_format_changed);

    if (!CHECK_CA_WARN("can't remove property listener")) {
        return false;
    }

    return format_set;
}
