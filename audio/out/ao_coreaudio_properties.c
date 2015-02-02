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
#include "talloc.h"

OSStatus ca_get(AudioObjectID id, ca_scope scope, ca_sel selector,
                uint32_t size, void *data)
{
    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = selector,
        .mScope    = scope,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    return AudioObjectGetPropertyData(id, &p_addr, 0, NULL, &size, data);
}

OSStatus ca_set(AudioObjectID id, ca_scope scope, ca_sel selector,
                uint32_t size, void *data)
{
    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = selector,
        .mScope    = scope,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    return AudioObjectSetPropertyData(id, &p_addr, 0, NULL, size, data);
}

OSStatus ca_get_ary(AudioObjectID id, ca_scope scope, ca_sel selector,
                    uint32_t element_size, void **data, size_t *elements)
{
    OSStatus err;
    uint32_t p_size;

    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = selector,
        .mScope    = scope,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    err = AudioObjectGetPropertyDataSize(id, &p_addr, 0, NULL, &p_size);
    CHECK_CA_ERROR_SILENT_L(coreaudio_error);

    *data = talloc_zero_size(NULL, p_size);
    *elements = p_size / element_size;

    err = ca_get(id, scope, selector, p_size, *data);
    CHECK_CA_ERROR_SILENT_L(coreaudio_error_free);

    return err;
coreaudio_error_free:
    free(*data);
coreaudio_error:
    return err;
}

OSStatus ca_get_str(AudioObjectID id, ca_scope scope, ca_sel selector,
                    char **data)
{
    CFStringRef string;
    OSStatus err =
        ca_get(id, scope, selector, sizeof(CFStringRef), (void **)&string);
    CHECK_CA_ERROR_SILENT_L(coreaudio_error);

    *data = cfstr_get_cstr(string);
    CFRelease(string);
coreaudio_error:
    return err;
}

Boolean ca_settable(AudioObjectID id, ca_scope scope, ca_sel selector,
                    Boolean *data)
{
    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = selector,
        .mScope    = kAudioObjectPropertyScopeGlobal,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    return AudioObjectIsPropertySettable(id, &p_addr, data);
}

