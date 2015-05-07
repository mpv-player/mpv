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

#include <CoreAudio/HostTime.h>

#include "audio/out/ao_coreaudio_utils.h"
#include "audio/out/ao_coreaudio_properties.h"
#include "osdep/timer.h"
#include "osdep/endian.h"
#include "audio/format.h"

CFStringRef cfstr_from_cstr(char *str)
{
    return CFStringCreateWithCString(NULL, str, CA_CFSTR_ENCODING);
}

char *cfstr_get_cstr(CFStringRef cfstr)
{
    CFIndex size =
        CFStringGetMaximumSizeForEncoding(
            CFStringGetLength(cfstr), CA_CFSTR_ENCODING) + 1;
    char *buffer = talloc_zero_size(NULL, size);
    CFStringGetCString(cfstr, buffer, size, CA_CFSTR_ENCODING);
    return buffer;
}

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
        void *ta_ctx = talloc_new(NULL);
        char *name;
        char *desc;
        err = CA_GET_STR(devs[i], kAudioDevicePropertyDeviceUID, &name);
        talloc_steal(ta_ctx, name);
        err = CA_GET_STR(devs[i], kAudioObjectPropertyName, &desc);
        talloc_steal(ta_ctx, desc);
        if (err != noErr)
            desc = "Unknown";
        ao_device_list_add(list, ao, &(struct ao_device_desc){name, desc});
        talloc_free(ta_ctx);
    }
    talloc_free(devs);
coreaudio_error:
    return;
}

OSStatus ca_select_device(struct ao *ao, char* name, AudioDeviceID *device)
{
    OSStatus err = noErr;
    *device = kAudioObjectUnknown;

    if (name && name[0]) {
        CFStringRef uid = cfstr_from_cstr(name);
        AudioValueTranslation v = (AudioValueTranslation) {
            .mInputData = &uid,
            .mInputDataSize = sizeof(CFStringRef),
            .mOutputData = device,
            .mOutputDataSize = sizeof(*device),
        };
        uint32_t size = sizeof(AudioValueTranslation);
        AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
            .mSelector = kAudioHardwarePropertyDeviceForUID,
            .mScope    = kAudioObjectPropertyScopeGlobal,
            .mElement  = kAudioObjectPropertyElementMaster,
        };
        err = AudioObjectGetPropertyData(
            kAudioObjectSystemObject, &p_addr, 0, 0, &size, &v);
        CFRelease(uid);
        CHECK_CA_ERROR("unable to query for device UID");
    } else {
        // device not set by user, get the default one
        err = CA_GET(kAudioObjectSystemObject,
                     kAudioHardwarePropertyDefaultOutputDevice,
                     device);
        CHECK_CA_ERROR("could not get default audio device");
    }

    if (mp_msg_test(ao->log, MSGL_V)) {
        char *desc;
        OSStatus err2 = CA_GET_STR(*device, kAudioObjectPropertyName, &desc);
        if (err2 == noErr) {
            MP_VERBOSE(ao, "selected audio output device: %s (%" PRIu32 ")\n",
                           desc, *device);
            talloc_free(desc);
        }
    }

coreaudio_error:
    return err;
}

char *fourcc_repr_buf(char *buf, size_t buf_size, uint32_t code)
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
    for (int i = 0; i < 4; i++) {
        if (fcc[i] < 32 || fcc[i] >= 128)
            valid_fourcc = false;
    }

    if (valid_fourcc)
        snprintf(buf, buf_size, "'%c%c%c%c'", fcc[0], fcc[1], fcc[2], fcc[3]);
    else
        snprintf(buf, buf_size, "%u", (unsigned int)code);

    return buf;
}

bool check_ca_st(struct ao *ao, int level, OSStatus code, const char *message)
{
    if (code == noErr) return true;

    mp_msg(ao->log, level, "%s (%s)\n", message, fourcc_repr(code));

    return false;
}

static void ca_fill_asbd_raw(AudioStreamBasicDescription *asbd, int mp_format,
                             int samplerate, int num_channels)
{
    asbd->mSampleRate       = samplerate;
    // Set "AC3" for other spdif formats too - unknown if that works.
    asbd->mFormatID         = AF_FORMAT_IS_IEC61937(mp_format) ?
                              kAudioFormat60958AC3 :
                              kAudioFormatLinearPCM;
    asbd->mChannelsPerFrame = num_channels;
    asbd->mBitsPerChannel   = af_fmt2bits(mp_format);
    asbd->mFormatFlags      = kAudioFormatFlagIsPacked;

    if ((mp_format & AF_FORMAT_TYPE_MASK) == AF_FORMAT_F) {
        asbd->mFormatFlags |= kAudioFormatFlagIsFloat;
    } else if ((mp_format & AF_FORMAT_SIGN_MASK) == AF_FORMAT_SI) {
        asbd->mFormatFlags |= kAudioFormatFlagIsSignedInteger;
    }

    if (BYTE_ORDER == BIG_ENDIAN)
        asbd->mFormatFlags |= kAudioFormatFlagIsBigEndian;

    asbd->mFramesPerPacket = 1;
    asbd->mBytesPerPacket = asbd->mBytesPerFrame =
        asbd->mFramesPerPacket * asbd->mChannelsPerFrame *
        (asbd->mBitsPerChannel / 8);
}

void ca_fill_asbd(struct ao *ao, AudioStreamBasicDescription *asbd)
{
    ca_fill_asbd_raw(asbd, ao->format, ao->samplerate, ao->channels.num);
}

static bool ca_formatid_is_digital(uint32_t formatid)
{
    switch (formatid)
    case 'IAC3':
    case 'iac3':
    case  kAudioFormat60958AC3:
    case  kAudioFormatAC3:
        return true;
    return false;
}

// This might be wrong, but for now it's sufficient for us.
static uint32_t ca_normalize_formatid(uint32_t formatID)
{
    return ca_formatid_is_digital(formatID) ? kAudioFormat60958AC3 : formatID;
}

bool ca_asbd_equals(const AudioStreamBasicDescription *a,
                    const AudioStreamBasicDescription *b)
{
    int flags = kAudioFormatFlagIsPacked | kAudioFormatFlagIsFloat |
            kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian;

    return (a->mFormatFlags & flags) == (b->mFormatFlags & flags) &&
           a->mBitsPerChannel == b->mBitsPerChannel &&
           ca_normalize_formatid(a->mFormatID) ==
                ca_normalize_formatid(b->mFormatID) &&
           a->mBytesPerPacket == b->mBytesPerPacket;
}

// Return the AF_FORMAT_* (AF_FORMAT_S16 etc.) corresponding to the asbd.
int ca_asbd_to_mp_format(const AudioStreamBasicDescription *asbd)
{
    for (int n = 0; af_fmtstr_table[n].format; n++) {
        int mp_format = af_fmtstr_table[n].format;
        AudioStreamBasicDescription mp_asbd = {0};
        ca_fill_asbd_raw(&mp_asbd, mp_format, 0, asbd->mChannelsPerFrame);
        if (ca_asbd_equals(&mp_asbd, asbd))
            return mp_format;
    }
    return 0;
}

void ca_print_asbd(struct ao *ao, const char *description,
                   const AudioStreamBasicDescription *asbd)
{
    uint32_t flags  = asbd->mFormatFlags;
    char *format    = fourcc_repr(asbd->mFormatID);
    int mpfmt       = ca_asbd_to_mp_format(asbd);

    MP_VERBOSE(ao,
       "%s %7.1fHz %" PRIu32 "bit %s "
       "[%" PRIu32 "][%" PRIu32 "bpp][%" PRIu32 "fbp]"
       "[%" PRIu32 "bpf][%" PRIu32 "ch] "
       "%s %s %s%s%s%s (%s)\n",
       description, asbd->mSampleRate, asbd->mBitsPerChannel, format,
       asbd->mFormatFlags, asbd->mBytesPerPacket, asbd->mFramesPerPacket,
       asbd->mBytesPerFrame, asbd->mChannelsPerFrame,
       (flags & kAudioFormatFlagIsFloat) ? "float" : "int",
       (flags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE",
       (flags & kAudioFormatFlagIsSignedInteger) ? "S" : "U",
       (flags & kAudioFormatFlagIsPacked) ? " packed" : "",
       (flags & kAudioFormatFlagIsAlignedHigh) ? " aligned" : "",
       (flags & kAudioFormatFlagIsNonInterleaved) ? " P" : "",
       mpfmt ? af_fmt_to_str(mpfmt) : "-");
}

// Return whether new is an improvement over old. Assume a higher value means
// better quality, and we always prefer the value closest to the requested one,
// which is still larger than the requested one.
// Equal values prefer the new one (so ca_asbd_is_better() checks other params).
static bool value_is_better(double req, double old, double new)
{
    if (new >= req) {
        return old < req || new <= old;
    } else {
        return old < req && new >= old;
    }
}

// Return whether new is an improvement over old (req is the requested format).
bool ca_asbd_is_better(AudioStreamBasicDescription *req,
                       AudioStreamBasicDescription *old,
                       AudioStreamBasicDescription *new)
{
    if (new->mChannelsPerFrame > MP_NUM_CHANNELS)
        return false;
    if (old->mChannelsPerFrame > MP_NUM_CHANNELS)
        return true;
    if (req->mFormatID != new->mFormatID)
        return false;
    if (req->mFormatID != old->mFormatID)
        return true;

    if (!value_is_better(req->mBitsPerChannel, old->mBitsPerChannel,
                         new->mBitsPerChannel))
        return false;

    if (!value_is_better(req->mSampleRate, old->mSampleRate, new->mSampleRate))
        return false;

    if (!value_is_better(req->mChannelsPerFrame, old->mChannelsPerFrame,
                         new->mChannelsPerFrame))
        return false;

    return true;
}

int64_t ca_frames_to_us(struct ao *ao, uint32_t frames)
{
    return frames / (float) ao->samplerate * 1e6;
}

int64_t ca_get_latency(const AudioTimeStamp *ts)
{
    uint64_t out = AudioConvertHostTimeToNanos(ts->mHostTime);
    uint64_t now = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());

    if (now > out)
        return 0;

    return (out - now) * 1e-3;
}
