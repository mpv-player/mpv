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

OSStatus ca_get(AudioObjectID id, AudioObjectPropertySelector selector,
                uint32_t size, void *data);

#define CA_GET(id, selector, data) ca_get(id, selector, sizeof(*(data)), data)

uint32_t GetAudioPropertyArray(AudioObjectID id,
                               AudioObjectPropertySelector selector,
                               AudioObjectPropertyScope scope, void **data);

uint32_t GetGlobalAudioPropertyArray(AudioObjectID id,
                                     AudioObjectPropertySelector selector,
                                     void **data);

OSStatus GetAudioPropertyString(AudioObjectID id,
                                AudioObjectPropertySelector selector,
                                char **data);

OSStatus SetAudioProperty(AudioObjectID id,
                          AudioObjectPropertySelector selector,
                          uint32_t size, void *data);

Boolean IsAudioPropertySettable(AudioObjectID id,
                                AudioObjectPropertySelector selector,
                                Boolean *outData);

#endif /* MPV_COREAUDIO_PROPERTIES_H */
