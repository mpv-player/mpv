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

#ifndef MPV_COREAUDIO_CHMAP_H
#define MPV_COREAUDIO_CHMAP_H

#include <AudioToolbox/AudioToolbox.h>

struct mp_chmap;

int ca_label_to_mp_speaker_id(AudioChannelLabel label);

#if HAVE_COREAUDIO
bool ca_init_chmap(struct ao *ao, AudioDeviceID device);
void ca_get_active_chmap(struct ao *ao, AudioDeviceID device, int channel_count,
                         struct mp_chmap *out_map);
#endif

#endif
