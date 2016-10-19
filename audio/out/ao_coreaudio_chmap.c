/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/common.h"

#include "ao_coreaudio_utils.h"

#include "ao_coreaudio_chmap.h"

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

    { kAudioChannelLabel_Unknown,              MP_SPEAKER_ID_NA   },

    { 0,                                       -1                 },
};

int ca_label_to_mp_speaker_id(AudioChannelLabel label)
{
    for (int i = 0; speaker_map[i][1] >= 0; i++)
        if (speaker_map[i][0] == label)
            return speaker_map[i][1];
    return -1;
}

#if HAVE_COREAUDIO
static void ca_log_layout(struct ao *ao, int l, AudioChannelLayout *layout)
{
    if (!mp_msg_test(ao->log, l))
        return;

    AudioChannelDescription *descs = layout->mChannelDescriptions;

    mp_msg(ao->log, l, "layout: tag: <%u>, bitmap: <%u>, "
                       "descriptions <%u>\n",
                       (unsigned) layout->mChannelLayoutTag,
                       (unsigned) layout->mChannelBitmap,
                       (unsigned) layout->mNumberChannelDescriptions);

    for (int i = 0; i < layout->mNumberChannelDescriptions; i++) {
        AudioChannelDescription d = descs[i];
        mp_msg(ao->log, l, " - description %d: label <%u, %u>, "
            " flags: <%u>, coords: <%f, %f, %f>\n", i,
            (unsigned) d.mChannelLabel,
            (unsigned) ca_label_to_mp_speaker_id(d.mChannelLabel),
            (unsigned) d.mChannelFlags,
            d.mCoordinates[0],
            d.mCoordinates[1],
            d.mCoordinates[2]);
    }
}

static AudioChannelLayout *ca_layout_to_custom_layout(struct ao *ao,
                                                      void *talloc_ctx,
                                                      AudioChannelLayout *l)
{
    AudioChannelLayoutTag tag = l->mChannelLayoutTag;
    AudioChannelLayout *r;
    OSStatus err;

    if (tag == kAudioChannelLayoutTag_UseChannelDescriptions)
        return l;

    if (tag == kAudioChannelLayoutTag_UseChannelBitmap) {
        uint32_t psize;
        err = AudioFormatGetPropertyInfo(
            kAudioFormatProperty_ChannelLayoutForBitmap,
            sizeof(uint32_t), &l->mChannelBitmap, &psize);
        CHECK_CA_ERROR("failed to convert channel bitmap to descriptions (info)");
        r = talloc_size(NULL, psize);
        err = AudioFormatGetProperty(
            kAudioFormatProperty_ChannelLayoutForBitmap,
            sizeof(uint32_t), &l->mChannelBitmap, &psize, r);
        CHECK_CA_ERROR("failed to convert channel bitmap to descriptions (get)");
    } else {
        uint32_t psize;
        err = AudioFormatGetPropertyInfo(
            kAudioFormatProperty_ChannelLayoutForTag,
            sizeof(AudioChannelLayoutTag), &l->mChannelLayoutTag, &psize);
        r = talloc_size(NULL, psize);
        CHECK_CA_ERROR("failed to convert channel tag to descriptions (info)");
        err = AudioFormatGetProperty(
            kAudioFormatProperty_ChannelLayoutForTag,
            sizeof(AudioChannelLayoutTag), &l->mChannelLayoutTag, &psize, r);
        CHECK_CA_ERROR("failed to convert channel tag to descriptions (get)");
    }

    MP_VERBOSE(ao, "converted input channel layout:\n");
    ca_log_layout(ao, MSGL_V, l);

    return r;
coreaudio_error:
    return NULL;
}


#define CHMAP(n, ...) &(struct mp_chmap) MP_CONCAT(MP_CHMAP, n) (__VA_ARGS__)

// Replace each channel in a with b (a->num == b->num)
static void replace_submap(struct mp_chmap *dst, struct mp_chmap *a,
                           struct mp_chmap *b)
{
    struct mp_chmap t = *dst;
    if (!mp_chmap_is_valid(&t) || mp_chmap_diffn(a, &t) != 0)
        return;
    assert(a->num == b->num);
    for (int n = 0; n < t.num; n++) {
        for (int i = 0; i < a->num; i++) {
            if (t.speaker[n] == a->speaker[i]) {
                t.speaker[n] = b->speaker[i];
                break;
            }
        }
    }
    if (mp_chmap_is_valid(&t))
        *dst = t;
}

static bool ca_layout_to_mp_chmap(struct ao *ao, AudioChannelLayout *layout,
                                  struct mp_chmap *chmap)
{
    void *talloc_ctx = talloc_new(NULL);

    MP_VERBOSE(ao, "input channel layout:\n");
    ca_log_layout(ao, MSGL_V, layout);

    AudioChannelLayout *l = ca_layout_to_custom_layout(ao, talloc_ctx, layout);
    if (!l)
        goto coreaudio_error;

    if (l->mNumberChannelDescriptions > MP_NUM_CHANNELS) {
        MP_VERBOSE(ao, "layout has too many descriptions (%u, max: %d)\n",
                   (unsigned) l->mNumberChannelDescriptions, MP_NUM_CHANNELS);
        return false;
    }

    chmap->num = l->mNumberChannelDescriptions;
    for (int n = 0; n < l->mNumberChannelDescriptions; n++) {
        AudioChannelLabel label = l->mChannelDescriptions[n].mChannelLabel;
        int speaker = ca_label_to_mp_speaker_id(label);
        if (speaker < 0) {
            MP_VERBOSE(ao, "channel label=%u unusable to build channel "
                           "bitmap, skipping layout\n", (unsigned) label);
            goto coreaudio_error;
        }
        chmap->speaker[n] = speaker;
    }

    // Remap weird 7.1(rear) layouts correctly.
    replace_submap(chmap, CHMAP(6, FL, FR, BL, BR, SDL, SDR),
                          CHMAP(6, FL, FR, SL, SR, BL,  BR));

    talloc_free(talloc_ctx);
    MP_VERBOSE(ao, "mp chmap: %s\n", mp_chmap_to_str(chmap));
    return mp_chmap_is_valid(chmap) && !mp_chmap_is_unknown(chmap);
coreaudio_error:
    MP_VERBOSE(ao, "converted input channel layout (failed):\n");
    ca_log_layout(ao, MSGL_V, layout);
    talloc_free(talloc_ctx);
    return false;
}

static AudioChannelLayout* ca_query_layout(struct ao *ao,
                                           AudioDeviceID device,
                                           void *talloc_ctx)
{
    OSStatus err;
    uint32_t psize;
    AudioChannelLayout *r = NULL;

    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioDevicePropertyPreferredChannelLayout,
        .mScope    = kAudioDevicePropertyScopeOutput,
        .mElement  = kAudioObjectPropertyElementWildcard,
    };

    err = AudioObjectGetPropertyDataSize(device, &p_addr, 0, NULL, &psize);
    CHECK_CA_ERROR("could not get device preferred layout (size)");

    r = talloc_size(talloc_ctx, psize);

    err = AudioObjectGetPropertyData(device, &p_addr, 0, NULL, &psize, r);
    CHECK_CA_ERROR("could not get device preferred layout (get)");

coreaudio_error:
    return r;
}

static AudioChannelLayout* ca_query_stereo_layout(struct ao *ao,
                                                  AudioDeviceID device,
                                                  void *talloc_ctx)
{
    OSStatus err;
    const int nch = 2;
    uint32_t channels[nch];
    AudioChannelLayout *r = NULL;

    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioDevicePropertyPreferredChannelsForStereo,
        .mScope    = kAudioDevicePropertyScopeOutput,
        .mElement  = kAudioObjectPropertyElementWildcard,
    };

    uint32_t psize = sizeof(channels);
    err = AudioObjectGetPropertyData(device, &p_addr, 0, NULL, &psize, channels);
    CHECK_CA_ERROR("could not get device preferred stereo layout");

    psize = sizeof(AudioChannelLayout) + nch * sizeof(AudioChannelDescription);
    r = talloc_zero_size(talloc_ctx, psize);
    r->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    r->mNumberChannelDescriptions = nch;

    AudioChannelDescription desc = {0};
    desc.mChannelFlags = kAudioChannelFlags_AllOff;

    for(int i = 0; i < nch; i++) {
        desc.mChannelLabel = channels[i];
        r->mChannelDescriptions[i] = desc;
    }

coreaudio_error:
    return r;
}

static void ca_retrieve_layouts(struct ao *ao, struct mp_chmap_sel *s,
                                AudioDeviceID device)
{
    void *ta_ctx = talloc_new(NULL);
    struct mp_chmap chmap;

    AudioChannelLayout *ml = ca_query_layout(ao, device, ta_ctx);
    if (ml && ca_layout_to_mp_chmap(ao, ml, &chmap))
        mp_chmap_sel_add_map(s, &chmap);

    AudioChannelLayout *sl = ca_query_stereo_layout(ao, device, ta_ctx);
    if (sl && ca_layout_to_mp_chmap(ao, sl, &chmap))
        mp_chmap_sel_add_map(s, &chmap);

    talloc_free(ta_ctx);
}

bool ca_init_chmap(struct ao *ao, AudioDeviceID device)
{
    struct mp_chmap_sel chmap_sel = {0};
    ca_retrieve_layouts(ao, &chmap_sel, device);

    if (!chmap_sel.num_chmaps)
        mp_chmap_sel_add_map(&chmap_sel, &(struct mp_chmap)MP_CHMAP_INIT_STEREO);

    mp_chmap_sel_add_map(&chmap_sel, &(struct mp_chmap)MP_CHMAP_INIT_MONO);

    if (!ao_chmap_sel_adjust(ao, &chmap_sel, &ao->channels)) {
        MP_ERR(ao, "could not select a suitable channel map among the "
                   "hardware supported ones. Make sure to configure your "
                   "output device correctly in 'Audio MIDI Setup.app'\n");
        return false;
    }
    return true;
}

void ca_get_active_chmap(struct ao *ao, AudioDeviceID device, int channel_count,
                         struct mp_chmap *out_map)
{
    // Apparently, we have to guess by looking back at the supported layouts,
    // and I haven't found a property that retrieves the actual currently
    // active channel layout.

    struct mp_chmap_sel chmap_sel = {0};
    ca_retrieve_layouts(ao, &chmap_sel, device);

    // Use any exact match.
    for (int n = 0; n < chmap_sel.num_chmaps; n++) {
        if (chmap_sel.chmaps[n].num == channel_count) {
            MP_VERBOSE(ao, "mismatching channels - fallback #%d\n", n);
            *out_map = chmap_sel.chmaps[n];
            return;
        }
    }

    // Fall back to stereo or mono, and fill the rest with silence. (We don't
    // know what the device expects. We could use a larger default layout here,
    // but let's not.)
    mp_chmap_from_channels(out_map, MPMIN(2, channel_count));
    out_map->num = channel_count;
    for (int n = 2; n < out_map->num; n++)
        out_map->speaker[n] = MP_SPEAKER_ID_NA;
    MP_WARN(ao, "mismatching channels - falling back to %s\n",
            mp_chmap_to_str(out_map));
}
#endif
