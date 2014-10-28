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
#include "osdep/endian.h"
#include "audio/format.h"

static bool ca_is_output_device(struct ao *ao, AudioDeviceID dev)
{
    size_t n_buffers;
    AudioBufferList *buffers;
    const ca_scope scope = kAudioDevicePropertyStreamConfiguration;
    CA_GET_ARY_O(dev, scope, &buffers, &n_buffers);
    talloc_free(buffers);
    return n_buffers > 0;
}

void ca_get_device_list(struct ao *ao, struct ao_device_list *list)
{
    AudioDeviceID *devs;
    size_t n_devs;
    OSStatus err =
        CA_GET_ARY(kAudioObjectSystemObject, kAudioHardwarePropertyDevices,
                   &devs, &n_devs);
    CHECK_CA_ERROR("Failed to get list of output devices.");
    for (int i = 0; i < n_devs; i++) {
        if (!ca_is_output_device(ao, devs[i]))
            continue;
        char name[32];
        char *desc;
        sprintf(name, "%d", devs[i]);
        err = CA_GET_STR(devs[i], kAudioObjectPropertyName, &desc);
        if (err != noErr)
            desc = "Unknown";
        ao_device_list_add(list, ao, &(struct ao_device_desc){name, desc});
    }
    talloc_free(devs);
coreaudio_error:
    return;
}

OSStatus ca_select_device(struct ao *ao, char* name, AudioDeviceID *device)
{
    OSStatus err = noErr;
    int selection = name ? strtol(name, (char **)NULL, 10) : -1;
    if (errno == EINVAL || errno == ERANGE) {
        selection = -1;
        MP_WARN(ao, "device identifier '%s' is invalid\n", name);
    }
    *device = 0;
    if (selection < 0) {
        // device not set by user, get the default one
        err = CA_GET(kAudioObjectSystemObject,
                     kAudioHardwarePropertyDefaultOutputDevice,
                     device);
        CHECK_CA_ERROR("could not get default audio device");
    } else {
        *device = selection;
    }

    if (mp_msg_test(ao->log, MSGL_V)) {
        char *desc;
        err = CA_GET_STR(*device, kAudioObjectPropertyName, &desc);
        CHECK_CA_ERROR("could not get selected audio device name");

        MP_VERBOSE(ao, "selected audio output device: %s (%" PRIu32 ")\n",
                       desc, *device);

        talloc_free(desc);
    }

coreaudio_error:
    return err;
}

char *fourcc_repr(void *talloc_ctx, uint32_t code)
{
    // Extract FourCC letters from the uint32_t and finde out if it's a valid
    // code that is made of letters.
    unsigned char fcc[4] = {
        (code >> 24) & 0xFF,
        (code >> 16) & 0xFF,
        (code >> 8)  & 0xFF,
        code         & 0xFF,
    };

    bool valid_fourcc = true;
    for (int i = 0; i < 4; i++)
        if (fcc[i] >= 32 && fcc[i] < 128)
            valid_fourcc = false;

    char *repr;
    if (valid_fourcc)
        repr = talloc_asprintf(talloc_ctx, "'%c%c%c%c'",
                               fcc[0], fcc[1], fcc[2], fcc[3]);
    else
        repr = talloc_asprintf(NULL, "%d", code);

    return repr;
}

bool check_ca_st(struct ao *ao, int level, OSStatus code, const char *message)
{
    if (code == noErr) return true;

    char *error_string = fourcc_repr(NULL, code);
    mp_msg(ao->log, level, "%s (%s)\n", message, error_string);
    talloc_free(error_string);

    return false;
}

void ca_fill_asbd(struct ao *ao, AudioStreamBasicDescription *asbd)
{
    asbd->mSampleRate       = ao->samplerate;
    // Set "AC3" for other spdif formats too - unknown if that works.
    asbd->mFormatID         = AF_FORMAT_IS_IEC61937(ao->format) ?
                              kAudioFormat60958AC3 :
                              kAudioFormatLinearPCM;
    asbd->mChannelsPerFrame = ao->channels.num;
    asbd->mBitsPerChannel   = af_fmt2bits(ao->format);
    asbd->mFormatFlags      = kAudioFormatFlagIsPacked;

    if ((ao->format & AF_FORMAT_TYPE_MASK) == AF_FORMAT_F)
        asbd->mFormatFlags |= kAudioFormatFlagIsFloat;

    if ((ao->format & AF_FORMAT_SIGN_MASK) == AF_FORMAT_SI)
        asbd->mFormatFlags |= kAudioFormatFlagIsSignedInteger;

    if (BYTE_ORDER == BIG_ENDIAN)
        asbd->mFormatFlags |= kAudioFormatFlagIsBigEndian;

    asbd->mFramesPerPacket = 1;
    asbd->mBytesPerPacket = asbd->mBytesPerFrame =
        asbd->mFramesPerPacket * asbd->mChannelsPerFrame *
        (asbd->mBitsPerChannel / 8);
}

void ca_print_asbd(struct ao *ao, const char *description,
                   const AudioStreamBasicDescription *asbd)
{
    uint32_t flags  = asbd->mFormatFlags;
    char *format    = fourcc_repr(NULL, asbd->mFormatID);

    MP_VERBOSE(ao,
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

