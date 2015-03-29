/*
 * This file is part of mpv.
 *
 * Original author: Jonathan Yong <10walls@gmail.com>
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

#ifndef MP_AO_WASAPI_UTILS_H_
#define MP_AO_WASAPI_UTILS_H_

#include "audio/out/ao_wasapi.h"

#include "options/m_option.h"
#include "common/msg.h"
#include "ao.h"
#include "internal.h"

char *mp_GUID_to_str_buf(char *buf, size_t buf_size, const GUID *guid);
char *mp_PKEY_to_str_buf(char *buf, size_t buf_size, const PROPERTYKEY *pkey);
char *mp_HRESULT_to_str_buf(char *buf, size_t buf_size, HRESULT hr);
#define mp_GUID_to_str(guid) mp_GUID_to_str_buf((char[40]){0}, 40, (guid))
#define mp_PKEY_to_str(pkey) mp_PKEY_to_str_buf((char[42]){0}, 42, (pkey))
#define mp_HRESULT_to_str(hr) mp_HRESULT_to_str_buf((char[60]){0}, 60, (hr))

bool wasapi_fill_VistaBlob(wasapi_state *state);

void wasapi_list_devs(struct ao *ao, struct ao_device_list *list);

void wasapi_dispatch(struct ao *ao);
HRESULT wasapi_thread_init(struct ao *ao);
void wasapi_thread_uninit(struct ao *ao);

HRESULT wasapi_setup_proxies(wasapi_state *state);
void wasapi_release_proxies(wasapi_state *state);

#endif
