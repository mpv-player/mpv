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

#ifndef MPV_COREAUDIO_UTILS_H
#define MPV_COREAUDIO_UTILS_H

#include <AudioToolbox/AudioToolbox.h>
#include <inttypes.h>
#include <stdbool.h>
#include "common/msg.h"
#include "audio/out/ao.h"
#include "internal.h"

#define CA_CFSTR_ENCODING kCFStringEncodingASCII

char *fourcc_repr(void *talloc_ctx, uint32_t code);
bool check_ca_st(struct ao *ao, int level, OSStatus code, const char *message);

#define CHECK_CA_ERROR_L(label, message) \
    do { \
        if (!check_ca_st(ao, MSGL_ERR, err, message)) { \
            goto label; \
        } \
    } while (0)

#define CHECK_CA_ERROR(message) CHECK_CA_ERROR_L(coreaudio_error, message)
#define CHECK_CA_WARN(message)  check_ca_st(ao, MSGL_WARN, err, message)

#define CHECK_CA_ERROR_SILENT_L(label) \
    do { \
        if (err != noErr) goto label; \
    } while (0)

void ca_print_device_list(struct ao *ao);
OSStatus ca_select_device(struct ao *ao, int selection, AudioDeviceID *device);

void ca_print_asbd(struct ao *ao, const char *description,
                   const AudioStreamBasicDescription *asbd);

bool ca_layout_to_mp_chmap(struct ao *ao, AudioChannelLayout *layout,
                           struct mp_chmap *chmap);

#endif /* MPV_COREAUDIO_UTILS_H */
