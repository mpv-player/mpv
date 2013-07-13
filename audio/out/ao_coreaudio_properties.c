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
 * Abstractions on the CoreAudio API to make property setting/getting suck les
*/

#include "audio/out/ao_coreaudio_properties.h"
#include "audio/out/ao_coreaudio_utils.h"

OSStatus ca_get(AudioObjectID id, AudioObjectPropertySelector selector,
                uint32_t size, void *data)
{
    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = selector,
        .mScope    = kAudioObjectPropertyScopeGlobal,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    return AudioObjectGetPropertyData(id, &p_addr, 0, NULL, &size, data);
}

uint32_t GetAudioPropertyArray(AudioObjectID id,
                               AudioObjectPropertySelector selector,
                               AudioObjectPropertyScope scope, void **data)
{
    OSStatus err;
    AudioObjectPropertyAddress p_addr;
    UInt32 p_size;

    p_addr.mSelector = selector;
    p_addr.mScope    = scope;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    err = AudioObjectGetPropertyDataSize(id, &p_addr, 0, NULL, &p_size);
    CHECK_CA_ERROR("Can't fetch property size");

    *data = malloc(p_size);

    err = AudioObjectGetPropertyData(id, &p_addr, 0, NULL, &p_size, *data);
    CHECK_CA_ERROR_L(coreaudio_error_free, "Can't fetch property data %s");

    return p_size;

coreaudio_error_free:
    free(*data);
coreaudio_error:
    return 0;
}

uint32_t GetGlobalAudioPropertyArray(AudioObjectID id,
                                     AudioObjectPropertySelector selector,
                                     void **data)
{
    return GetAudioPropertyArray(id, selector, kAudioObjectPropertyScopeGlobal,
                                 data);
}

OSStatus GetAudioPropertyString(AudioObjectID id,
                                AudioObjectPropertySelector selector,
                                char **data)
{
    OSStatus err;
    AudioObjectPropertyAddress p_addr;
    UInt32 p_size = sizeof(CFStringRef);
    CFStringRef string;

    p_addr.mSelector = selector;
    p_addr.mScope    = kAudioObjectPropertyScopeGlobal;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    err = AudioObjectGetPropertyData(id, &p_addr, 0, NULL, &p_size, &string);
    CHECK_CA_ERROR("Can't fetch array property");

    CFIndex size =
        CFStringGetMaximumSizeForEncoding(
            CFStringGetLength(string), CA_CFSTR_ENCODING) + 1;

    *data = malloc(size);
    CFStringGetCString(string, *data, size, CA_CFSTR_ENCODING);
    CFRelease(string);
coreaudio_error:
    return err;
}

OSStatus SetAudioProperty(AudioObjectID id,
                          AudioObjectPropertySelector selector,
                          uint32_t size, void *data)
{
    AudioObjectPropertyAddress p_addr;

    p_addr.mSelector = selector;
    p_addr.mScope    = kAudioObjectPropertyScopeGlobal;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    return AudioObjectSetPropertyData(id, &p_addr, 0, NULL,
                                      size, data);
}

Boolean IsAudioPropertySettable(AudioObjectID id,
                                AudioObjectPropertySelector selector,
                                Boolean *data)
{
    AudioObjectPropertyAddress p_addr;

    p_addr.mSelector = selector;
    p_addr.mScope    = kAudioObjectPropertyScopeGlobal;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    return AudioObjectIsPropertySettable(id, &p_addr, data);
}

