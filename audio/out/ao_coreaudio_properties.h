/*
 * This file is part of mpv.
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
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

#ifndef MPV_COREAUDIO_PROPERTIES_H
#define MPV_COREAUDIO_PROPERTIES_H

#include <AudioToolbox/AudioToolbox.h>

#include "internal.h"

// CoreAudio names are way too verbose
#define ca_sel    AudioObjectPropertySelector
#define ca_scope  AudioObjectPropertyScope
#define CA_GLOBAL kAudioObjectPropertyScopeGlobal
#define CA_OUTPUT kAudioDevicePropertyScopeOutput

OSStatus ca_get(AudioObjectID id, ca_scope scope, ca_sel selector,
                uint32_t size, void *data);

OSStatus ca_set(AudioObjectID id, ca_scope scope, ca_sel selector,
                uint32_t size, void *data);

#define CA_GET(id, sel, data) ca_get(id, CA_GLOBAL, sel, sizeof(*(data)), data)
#define CA_SET(id, sel, data) ca_set(id, CA_GLOBAL, sel, sizeof(*(data)), data)
#define CA_GET_O(id, sel, data) ca_get(id, CA_OUTPUT, sel, sizeof(*(data)), data)

OSStatus ca_get_ary(AudioObjectID id, ca_scope scope, ca_sel selector,
                    uint32_t element_size, void **data, size_t *elements);

#define CA_GET_ARY(id, sel, data, elements) \
    ca_get_ary(id, CA_GLOBAL, sel, sizeof(**(data)), (void **)data, elements)

#define CA_GET_ARY_O(id, sel, data, elements) \
    ca_get_ary(id, CA_OUTPUT, sel, sizeof(**(data)), (void **)data, elements)

OSStatus ca_get_str(AudioObjectID id, ca_scope scope,ca_sel selector,
                    char **data);

#define CA_GET_STR(id, sel, data) ca_get_str(id, CA_GLOBAL, sel, data)

Boolean ca_settable(AudioObjectID id, ca_scope scope, ca_sel selector,
                    Boolean *data);

#define CA_SETTABLE(id, sel, data) ca_settable(id, CA_GLOBAL, sel, data)

#endif /* MPV_COREAUDIO_PROPERTIES_H */
