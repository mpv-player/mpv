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

#define COBJMACROS 1
#define _WIN32_WINNT 0x600

#include "audio/out/ao_wasapi.h"

#include "options/m_option.h"
#include "options/m_config.h"
#include "audio/format.h"
#include "common/msg.h"
#include "misc/ring.h"
#include "ao.h"
#include "internal.h"
#include "compat/atomics.h"
#include "osdep/timer.h"

int wasapi_fill_VistaBlob(wasapi_state *state);

const char *wasapi_explain_err(const HRESULT hr);

char* wasapi_get_device_name(IMMDevice *pDevice);
char* wasapi_get_device_id(IMMDevice *pDevice);

int wasapi_find_formats(struct ao *const ao);
int wasapi_fix_format(struct wasapi_state *state);

int wasapi_enumerate_devices(struct mp_log *log);

HRESULT wasapi_find_and_load_device(struct ao *ao, IMMDevice **ppDevice,
                                    char *search);

int wasapi_validate_device(struct mp_log *log, const m_option_t *opt,
                           struct bstr name, struct bstr param);

#endif