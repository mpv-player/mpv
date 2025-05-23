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

#include "displayconfig.h"

#include "mpv_talloc.h"

static bool is_valid_refresh_rate(DISPLAYCONFIG_RATIONAL rr)
{
    // DisplayConfig sometimes reports a rate of 1 when the rate is not known
    return rr.Denominator != 0 && rr.Numerator / rr.Denominator > 1;
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
        res = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, num_paths,
                                          num_modes);
        if (res != ERROR_SUCCESS)
            goto fail;

        // Free old buffers if they exist and allocate new ones
        talloc_free(*paths);
        talloc_free(*modes);
        *paths = talloc_array(ctx, DISPLAYCONFIG_PATH_INFO, *num_paths);
        *modes = talloc_array(ctx, DISPLAYCONFIG_MODE_INFO, *num_modes);

        res = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, num_paths, *paths,
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
        if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS)
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
