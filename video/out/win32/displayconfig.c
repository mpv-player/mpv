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

#include <windows.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "displayconfig.h"

#include "mpv_talloc.h"

// Some DisplayConfig definitions are broken in mingw-w64 (as of 2015-3-13.) To
// get the correct struct alignment, it's necessary to define them properly.
#include <pshpack1.h>

typedef enum {
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER                 = -1,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HD15                  = 0,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_SVIDEO                = 1,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_COMPOSITE_VIDEO       = 2,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_COMPONENT_VIDEO       = 3,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DVI                   = 4,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI                  = 5,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_LVDS                  = 6,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_D_JPN                 = 8,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_SDI                   = 9,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EXTERNAL  = 10,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED  = 11,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EXTERNAL          = 12,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED          = 13,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_SDTVDONGLE            = 14,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_MIRACAST              = 15,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL              = -2147483647 - 1,
    MP_DISPLAYCONFIG_OUTPUT_TECHNOLOGY_FORCE_UINT32          = 0x7FFFFFFF
} MP_DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY;

typedef struct MP_DISPLAYCONFIG_PATH_TARGET_INFO {
    LUID adapterId;
    UINT32 id;
    UINT32 modeInfoIdx;
    MP_DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY outputTechnology;
    DISPLAYCONFIG_ROTATION rotation;
    DISPLAYCONFIG_SCALING scaling;
    DISPLAYCONFIG_RATIONAL refreshRate;
    DISPLAYCONFIG_SCANLINE_ORDERING scanLineOrdering;
    WINBOOL targetAvailable;
    UINT32 statusFlags;
} MP_DISPLAYCONFIG_PATH_TARGET_INFO;
#define DISPLAYCONFIG_PATH_TARGET_INFO MP_DISPLAYCONFIG_PATH_TARGET_INFO

typedef struct MP_DISPLAYCONFIG_PATH_INFO {
    DISPLAYCONFIG_PATH_SOURCE_INFO sourceInfo;
    MP_DISPLAYCONFIG_PATH_TARGET_INFO targetInfo;
    UINT32 flags;
} MP_DISPLAYCONFIG_PATH_INFO;
#define DISPLAYCONFIG_PATH_INFO MP_DISPLAYCONFIG_PATH_INFO

typedef struct MP_DISPLAYCONFIG_TARGET_DEVICE_NAME {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    DISPLAYCONFIG_TARGET_DEVICE_NAME_FLAGS flags;
    MP_DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY outputTechnology;
    UINT16 edidManufactureId;
    UINT16 edidProductCodeId;
    UINT32 connectorInstance;
    WCHAR monitorFriendlyDeviceName[64];
    WCHAR monitorDevicePath[128];
} MP_DISPLAYCONFIG_TARGET_DEVICE_NAME;
#define DISPLAYCONFIG_TARGET_DEVICE_NAME MP_DISPLAYCONFIG_TARGET_DEVICE_NAME

#include <poppack.h>

static pthread_once_t displayconfig_load_ran = PTHREAD_ONCE_INIT;
static bool displayconfig_loaded = false;

static LONG (WINAPI *pDisplayConfigGetDeviceInfo)(
        DISPLAYCONFIG_DEVICE_INFO_HEADER*);
static LONG (WINAPI *pGetDisplayConfigBufferSizes)(UINT32, UINT32*, UINT32*);
static LONG (WINAPI *pQueryDisplayConfig)(UINT32, UINT32*,
        DISPLAYCONFIG_PATH_INFO*, UINT32*, DISPLAYCONFIG_MODE_INFO*,
        DISPLAYCONFIG_TOPOLOGY_ID*);

static bool is_valid_refresh_rate(DISPLAYCONFIG_RATIONAL rr)
{
    // DisplayConfig sometimes reports a rate of 1 when the rate is not known
    return rr.Denominator != 0 && rr.Numerator / rr.Denominator > 1;
}

static void displayconfig_load(void)
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32)
        return;

    pDisplayConfigGetDeviceInfo =
        (LONG (WINAPI*)(DISPLAYCONFIG_DEVICE_INFO_HEADER*))
        GetProcAddress(user32, "DisplayConfigGetDeviceInfo");
    if (!pDisplayConfigGetDeviceInfo)
        return;
    pGetDisplayConfigBufferSizes =
        (LONG (WINAPI*)(UINT32, UINT32*, UINT32*))
        GetProcAddress(user32, "GetDisplayConfigBufferSizes");
    if (!pGetDisplayConfigBufferSizes)
        return;
    pQueryDisplayConfig =
        (LONG (WINAPI*)(UINT32, UINT32*, DISPLAYCONFIG_PATH_INFO*, UINT32*,
                        DISPLAYCONFIG_MODE_INFO*, DISPLAYCONFIG_TOPOLOGY_ID*))
        GetProcAddress(user32, "QueryDisplayConfig");
    if (!pQueryDisplayConfig)
        return;

    displayconfig_loaded = true;
}

static int get_config(void *ctx,
                      UINT32 *num_paths, DISPLAYCONFIG_PATH_INFO** paths,
                      UINT32 *num_modes, DISPLAYCONFIG_MODE_INFO** modes)
{
    LONG res;
    *paths = NULL;
    *modes = NULL;

    // The display configuration could change between the call to
    // GetDisplayConfigBufferSizes and the call to QueryDisplayConfig, so call
    // them in a loop until the correct buffer size is chosen
    do {
        res = pGetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, num_paths,
                                           num_modes);
        if (res != ERROR_SUCCESS)
            goto fail;

        // Free old buffers if they exist and allocate new ones
        talloc_free(*paths);
        talloc_free(*modes);
        *paths = talloc_array(ctx, DISPLAYCONFIG_PATH_INFO, *num_paths);
        *modes = talloc_array(ctx, DISPLAYCONFIG_MODE_INFO, *num_modes);

        res = pQueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, num_paths, *paths,
                                  num_modes, *modes, NULL);
    } while (res == ERROR_INSUFFICIENT_BUFFER);
    if (res != ERROR_SUCCESS)
        goto fail;

    return 0;
fail:
    talloc_free(*paths);
    talloc_free(*modes);
    return -1;
}

static DISPLAYCONFIG_PATH_INFO *get_path(UINT32 num_paths,
                                         DISPLAYCONFIG_PATH_INFO* paths,
                                         const wchar_t *device)
{
    // Search for a path with a matching device name
    for (UINT32 i = 0; i < num_paths; i++) {
        // Send a GET_SOURCE_NAME request
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source = {
            .header = {
                .size = sizeof source,
                .type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,
                .adapterId = paths[i].sourceInfo.adapterId,
                .id = paths[i].sourceInfo.id,
            }
        };
        if (pDisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS)
            return NULL;

        // Check if the device name matches
        if (!wcscmp(device, source.viewGdiDeviceName))
            return &paths[i];
    }

    return NULL;
}

static double get_refresh_rate_from_mode(DISPLAYCONFIG_MODE_INFO *mode)
{
    if (mode->infoType != DISPLAYCONFIG_MODE_INFO_TYPE_TARGET)
        return 0.0;

    DISPLAYCONFIG_VIDEO_SIGNAL_INFO *info =
        &mode->targetMode.targetVideoSignalInfo;
    if (!is_valid_refresh_rate(info->vSyncFreq))
        return 0.0;

    return ((double)info->vSyncFreq.Numerator) /
           ((double)info->vSyncFreq.Denominator);
}

double mp_w32_displayconfig_get_refresh_rate(const wchar_t *device)
{
    // Load Windows 7 DisplayConfig API
    pthread_once(&displayconfig_load_ran, displayconfig_load);
    if (!displayconfig_loaded)
        return 0.0;

    void *ctx = talloc_new(NULL);
    double freq = 0.0;

    // Get the current display configuration
    UINT32 num_paths;
    DISPLAYCONFIG_PATH_INFO* paths;
    UINT32 num_modes;
    DISPLAYCONFIG_MODE_INFO* modes;
    if (get_config(ctx, &num_paths, &paths, &num_modes, &modes))
        goto end;

    // Get the path for the specified monitor
    DISPLAYCONFIG_PATH_INFO* path;
    if (!(path = get_path(num_paths, paths, device)))
        goto end;

    // Try getting the refresh rate from the mode first. The value in the mode
    // overrides the value in the path.
    if (path->targetInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID)
        freq = get_refresh_rate_from_mode(&modes[path->targetInfo.modeInfoIdx]);

    // If the mode didn't contain a valid refresh rate, try the path
    if (freq == 0.0 && is_valid_refresh_rate(path->targetInfo.refreshRate)) {
        freq = ((double)path->targetInfo.refreshRate.Numerator) /
               ((double)path->targetInfo.refreshRate.Denominator);
    }

end:
    talloc_free(ctx);
    return freq;
}
