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

void ca_print_device_list(struct ao *ao)
{
    char *help = talloc_strdup(NULL, "Available output devices:\n");

    AudioDeviceID *devs;
    size_t n_devs;

    OSStatus err =
        CA_GET_ARY(kAudioObjectSystemObject, kAudioHardwarePropertyDevices,
                   &devs, &n_devs);

    CHECK_CA_ERROR("Failed to get list of output devices.");

    for (int i = 0; i < n_devs; i++) {
        char *name;
        err = CA_GET_STR(devs[i], kAudioObjectPropertyName, &name);

        if (err == noErr)
            talloc_steal(devs, name);
        else
            name = "Unknown";

        help = talloc_asprintf_append(
                help, "  * %s (id: %" PRIu32 ")\n", name, devs[i]);
    }

    talloc_free(devs);

coreaudio_error:
    MP_INFO(ao, "%s", help);
    talloc_free(help);
}

OSStatus ca_select_device(struct ao *ao, int selection, AudioDeviceID *device)
{
    OSStatus err = noErr;
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
        char *name;
        err = CA_GET_STR(*device, kAudioObjectPropertyName, &name);
        CHECK_CA_ERROR("could not get selected audio device name");

        MP_VERBOSE(ao, "selected audio output device: %s (%" PRIu32 ")\n",
                       name, *device);

        talloc_free(name);
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

static const int speaker_map[][2] = {
    { kAudioChannelLabel_Left,                 MP_SPEAKER_ID_FL   },
    { kAudioChannelLabel_Right,                MP_SPEAKER_ID_FR   },
    { kAudioChannelLabel_Center,               MP_SPEAKER_ID_FC   },
    { kAudioChannelLabel_LFEScreen,            MP_SPEAKER_ID_LFE  },
    { kAudioChannelLabel_LeftSurround,         MP_SPEAKER_ID_BL   },
    { kAudioChannelLabel_RightSurround,        MP_SPEAKER_ID_BR   },
    { kAudioChannelLabel_LeftCenter,           MP_SPEAKER_ID_FLC  },
    { kAudioChannelLabel_RightCenter,          MP_SPEAKER_ID_FRC  },
    { kAudioChannelLabel_CenterSurround,       MP_SPEAKER_ID_BC   },
    { kAudioChannelLabel_LeftSurroundDirect,   MP_SPEAKER_ID_SL   },
    { kAudioChannelLabel_RightSurroundDirect,  MP_SPEAKER_ID_SR   },
    { kAudioChannelLabel_TopCenterSurround,    MP_SPEAKER_ID_TC   },
    { kAudioChannelLabel_VerticalHeightLeft,   MP_SPEAKER_ID_TFL  },
    { kAudioChannelLabel_VerticalHeightCenter, MP_SPEAKER_ID_TFC  },
    { kAudioChannelLabel_VerticalHeightRight,  MP_SPEAKER_ID_TFR  },
    { kAudioChannelLabel_TopBackLeft,          MP_SPEAKER_ID_TBL  },
    { kAudioChannelLabel_TopBackCenter,        MP_SPEAKER_ID_TBC  },
    { kAudioChannelLabel_TopBackRight,         MP_SPEAKER_ID_TBR  },

    // unofficial extensions
    { kAudioChannelLabel_RearSurroundLeft,     MP_SPEAKER_ID_SDL  },
    { kAudioChannelLabel_RearSurroundRight,    MP_SPEAKER_ID_SDR  },
    { kAudioChannelLabel_LeftWide,             MP_SPEAKER_ID_WL   },
    { kAudioChannelLabel_RightWide,            MP_SPEAKER_ID_WR   },
    { kAudioChannelLabel_LFE2,                 MP_SPEAKER_ID_LFE2 },

    { kAudioChannelLabel_HeadphonesLeft,       MP_SPEAKER_ID_DL   },
    { kAudioChannelLabel_HeadphonesRight,      MP_SPEAKER_ID_DR   },

    { kAudioChannelLabel_Unknown,              -1 },
};

static int ca_label_to_mp_speaker_id(AudioChannelLabel label)
{
    for (int i = 0; speaker_map[i][1] >= 0; i++)
        if (speaker_map[i][0] == label)
            return speaker_map[i][1];
    return -1;
}

static void ca_log_layout(struct ao *ao, AudioChannelLayout *layout)
{
    if (!mp_msg_test(ao->log, MSGL_V))
        return;

    AudioChannelDescription *descs = layout->mChannelDescriptions;

    MP_VERBOSE(ao, "layout: tag: <%d>, bitmap: <%d>, "
                   "descriptions <%d>\n",
                   layout->mChannelLayoutTag,
                   layout->mChannelBitmap,
                   layout->mNumberChannelDescriptions);

    for (int i = 0; i < layout->mNumberChannelDescriptions; i++) {
        AudioChannelDescription d = descs[i];
        MP_VERBOSE(ao, " - description %d: label <%d, %d>, flags: <%u>, "
                       "coords: <%f, %f, %f>\n", i,
                       d.mChannelLabel,
                       ca_label_to_mp_speaker_id(d.mChannelLabel),
                       d.mChannelFlags,
                       d.mCoordinates[0],
                       d.mCoordinates[1],
                       d.mCoordinates[2]);
    }
}

bool ca_layout_to_mp_chmap(struct ao *ao, AudioChannelLayout *layout,
                           struct mp_chmap *chmap)
{
    AudioChannelLayoutTag tag  = layout->mChannelLayoutTag;
    uint32_t layout_size       = sizeof(layout);
    OSStatus err;

    if (tag == kAudioChannelLayoutTag_UseChannelBitmap) {
        err = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForBitmap,
                                     sizeof(uint32_t),
                                     &layout->mChannelBitmap,
                                     &layout_size,
                                     layout);
        CHECK_CA_ERROR("failed to convert channel bitmap to descriptions");
    } else if (tag != kAudioChannelLayoutTag_UseChannelDescriptions) {
        err = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag,
                                     sizeof(AudioChannelLayoutTag),
                                     &layout->mChannelLayoutTag,
                                     &layout_size,
                                     layout);
        CHECK_CA_ERROR("failed to convert channel tag to descriptions");
    }

    ca_log_layout(ao, layout);

    // If the channel layout uses channel descriptions, from my
    // experiments there are there three possibile cases:
    // * The description has a label kAudioChannelLabel_Unknown:
    //   Can't do anything about this (looks like non surround
    //   layouts are like this).
    // * The description uses positional information: this in
    //   theory could be used but one would have to map spatial
    //   positions to labels which is not really feasible.
    // * The description has a well known label which can be mapped
    //   to the waveextensible definition: this is the kind of
    //   descriptions we process here.

    for (int n = 0; n < layout->mNumberChannelDescriptions; n++) {
        AudioChannelLabel label = layout->mChannelDescriptions[n].mChannelLabel;
        uint8_t speaker = ca_label_to_mp_speaker_id(label);
        if (label == kAudioChannelLabel_Unknown)
            continue;
        if (speaker < 0) {
            MP_VERBOSE(ao, "channel label=%d unusable to build channel "
                           "bitmap, skipping layout\n", label);
        } else {
            chmap->speaker[n] = speaker;
            chmap->num = n + 1;
        }
    }

    return chmap->num > 0;
coreaudio_error:
    ca_log_layout(ao, layout);
    return false;
}
