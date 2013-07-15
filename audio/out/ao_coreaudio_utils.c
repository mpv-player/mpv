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
    size_t n_formats;

    OSStatus err =
        CA_GET_ARY(stream, kAudioStreamPropertyAvailablePhysicalFormats,
                   &formats, &n_formats);

    CHECK_CA_ERROR("Could not get number of stream formats.");

    for (int i = 0; i < n_formats; i++) {
        AudioStreamBasicDescription asbd = formats[i].mFormat;
        ca_print_asbd("supported format:", &(asbd));
        if (ca_format_is_digital(asbd)) {
            talloc_free(formats);
            return true;
        }
    }

    talloc_free(formats);
coreaudio_error:
    return false;
}

bool ca_device_supports_digital(AudioDeviceID device)
{
    AudioStreamID *streams = NULL;
    size_t n_streams;

    /* Retrieve all the output streams. */
    OSStatus err =
        CA_GET_ARY_O(device, kAudioDevicePropertyStreams, &streams, &n_streams);

    CHECK_CA_ERROR("could not get number of streams.");

    for (int i = 0; i < n_streams; i++) {
        if (ca_stream_supports_digital(streams[i])) {
            talloc_free(streams);
            return true;
        }
    }

    talloc_free(streams);

coreaudio_error:
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
    OSStatus err = CA_SET(device, kAudioDevicePropertyHogMode, pid);
    if (err != noErr)
        *pid = -1;

    return err;
}

OSStatus ca_unlock_device(AudioDeviceID device, pid_t *pid) {
    if (*pid == getpid()) {
        *pid = -1;
        return CA_SET(device, kAudioDevicePropertyHogMode, &pid);
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
        err = CA_SETTABLE(device, kAudioDevicePropertySupportsMixing,
                          &writeable);

        if (!CHECK_CA_WARN("can't tell if mixing property is settable")) {
            return err;
        }

        if (!writeable)
            return noErr;

        err = CA_SET(device, kAudioDevicePropertySupportsMixing, &val);
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
    err = CA_SET(stream, kAudioStreamPropertyPhysicalFormat, &change_format);
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

void ca_bitmaps_from_layouts(AudioChannelLayout *layouts, size_t n_layouts,
                             uint32_t **bitmaps, size_t *n_bitmaps)
{
    *n_bitmaps = 0;
    *bitmaps = talloc_array_size(NULL, sizeof(uint32_t), n_layouts);

    for (int i=0; i < n_layouts; i++) {
        uint32_t bitmap = 0;

        switch (layouts[i].mChannelLayoutTag) {
        case kAudioChannelLayoutTag_UseChannelBitmap:
            (*bitmaps)[(*n_bitmaps)++] = layouts[i].mChannelBitmap;
            break;

        case kAudioChannelLayoutTag_UseChannelDescriptions:
            if (ca_bitmap_from_ch_desc(&layouts[i], &bitmap))
                (*bitmaps)[(*n_bitmaps)++] = bitmap;
            break;

        default:
            if (ca_bitmap_from_ch_tag(&layouts[i], &bitmap))
                (*bitmaps)[(*n_bitmaps)++] = bitmap;
        }
    }
}

bool ca_bitmap_from_ch_desc(AudioChannelLayout *layout, uint32_t *bitmap)
{
    // If the channel layout uses channel descriptions, from my
    // exepriments there are there three possibile cases:
    // * The description has a label kAudioChannelLabel_Unknown:
    //   Can't do anything about this (looks like non surround
    //   layouts are like this).
    // * The description uses positional information: this in
    //   theory could be used but one would have to map spatial
    //   positions to labels which is not really feasible.
    // * The description has a well known label which can be mapped
    //   to the waveextensible definition: this is the kind of
    //   descriptions we process here.
    size_t ch_num = layout->mNumberChannelDescriptions;
    bool all_channels_valid = true;

    for (int j=0; j < ch_num && all_channels_valid; j++) {
        AudioChannelLabel label = layout->mChannelDescriptions[j].mChannelLabel;
        if (label == kAudioChannelLabel_UseCoordinates ||
            label == kAudioChannelLabel_Unknown ||
            label > kAudioChannelLabel_TopBackRight) {
            ca_msg(MSGL_V,
                    "channel label=%d unusable to build channel "
                    "bitmap, skipping layout\n", label);
            all_channels_valid = false;
        } else {
            *bitmap |= 1ULL << (label - 1);
        }
    }

    return all_channels_valid;
}

bool ca_bitmap_from_ch_tag(AudioChannelLayout *layout, uint32_t *bitmap)
{
    // This layout is defined exclusively by it's tag. Use the Audio
    // Format Services API to try and convert it to a bitmap that
    // mpv can use.
    uint32_t bitmap_size = sizeof(uint32_t);

    AudioChannelLayoutTag tag = layout->mChannelLayoutTag;
    OSStatus err = AudioFormatGetProperty(
        kAudioFormatProperty_BitmapForLayoutTag,
        sizeof(AudioChannelLayoutTag), &tag,
        &bitmap_size, bitmap);
    if (err != noErr) {
        ca_msg(MSGL_V,
                "channel layout tag=%d unusable to build channel "
                "bitmap, skipping layout\n", tag);
        return false;
    } else {
        return true;
    }
}
