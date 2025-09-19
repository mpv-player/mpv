/*
 * Linux ambient light sensor utilities
 *
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "misc/bstr.h"
#include "misc/mp_assert.h"
#include "misc/path_utils.h"
#include "osdep/als.h"
#include "osdep/io.h"
#include "player/core.h"
#include "ta/ta_talloc.h"

// For the sysfs API documentation, refer to
//     Documentation/ABI/testing/sysfs-bus-iio
// In Kernel tree.
#define SYSFS_BASEDIR "/sys/bus/iio/devices"
#define INP_RAW "in_illuminance_raw"
#define INP_OFF "in_illuminance_offset"
#define INP_SCL "in_illuminance_scale"

struct mp_als {
    DIR *device_dir;
    struct mp_log *log;

    // Flag we set to true if we've looked for IIO devices and found none. This
    // way, we know that we shouldn't look again next time.
    bool found_no_devices;
};

static void mp_als_destroy(void *p)
{
    struct mp_als *state = p;

    if (state->device_dir) {
        closedir(state->device_dir);
    }
}

struct mp_als *mp_als_create(void *parent, MPContext *mpctx)
{
    struct mp_als *state =
        talloc_zero(parent, struct mp_als);

    state->log = mp_log_new(state, mpctx->global->log, "linux-als");

    talloc_set_destructor(state, mp_als_destroy);

    return state;
}

// Assumes that state->device_dir is NULL
static int acquire_sensor(struct mp_als *state)
{
    mp_assert(!state->device_dir);
    DIR *devices_dir = opendir(SYSFS_BASEDIR);
    if (!devices_dir) {
        MP_WARN(state,
                "No IIO device directory in sysfs: %s\n",
                mp_strerror(errno));
        return -1;
    }

    void *tmp = talloc_new(NULL);

    struct dirent *ent;
    while ((ent = readdir(devices_dir))) {
        // The directory also contains files named `trigger*` which we don't
        // care about; only include ones for devices.
        if (!bstr_startswith0(bstr0(ent->d_name), "iio:device"))
            continue;

        // Check if in_illuminance_raw exists so we know we have an ambient
        // light sensor.
        char *inp_raw_path = mp_path_join(tmp, ent->d_name, INP_RAW);

        if (faccessat(dirfd(devices_dir), inp_raw_path, R_OK, 0) != 0) {
            // Couldn't stat, continue
            continue;
        }

        // We found an ALS

        char *dev_path = mp_path_join(tmp, SYSFS_BASEDIR, ent->d_name);

        state->device_dir = opendir(dev_path);
        if (state->device_dir) {
            MP_VERBOSE(state, "Chose ALS device %s\n", dev_path);
            break;
        }

        MP_WARN(state, "Failed to open ambient light sensor %s: %s\n",
                ent->d_name,
                mp_strerror(errno));
    }

    talloc_free(tmp);
    closedir(devices_dir);

    return state->device_dir ? 0 : -1;
}

static int scan_from_subfile(struct mp_als *state,
                             const char *subpath,
                             const char *format,
                             void *out)
{
    mp_assert(state->device_dir);
    int fd = openat(dirfd(state->device_dir), subpath, O_RDONLY);
    if (fd < 0)
        return -1;

    char readbuf[256];
    ssize_t nread = read(fd, readbuf, sizeof(readbuf) - 1);
    close(fd);

    if (nread < 0)
        return -1;

    readbuf[nread] = 0;
    return sscanf(readbuf, format, out) == 1 ? 0 : -1;
}

static int read_als_assume_have_sensor(struct mp_als *state,
                                       double *lux)
{
    int illu_raw;
    if (scan_from_subfile(state, INP_RAW, "%d", &illu_raw) < 0) {
        MP_ERR(state,
               "Failed to read " INP_RAW ": %s\n",
               mp_strerror(errno));
        return MP_ALS_STATUS_READFAILED;
    }

    double illu_off;
    if (scan_from_subfile(state, INP_OFF, "%lf", &illu_off) < 0) {
        MP_ERR(state,
               "Failed to read " INP_OFF ": %s\n",
               mp_strerror(errno));
        return MP_ALS_STATUS_READFAILED;
    }

    double illu_scl;
    if (scan_from_subfile(state, INP_SCL, "%lf", &illu_scl) < 0) {
        MP_ERR(state,
               "Failed to read " INP_SCL ": %s\n",
               mp_strerror(errno));
        return MP_ALS_STATUS_READFAILED;
    }

    *lux = (illu_raw + illu_off) * illu_scl;
    return MP_ALS_STATUS_OK;
}

enum mp_als_status mp_als_get_lux(struct mp_als *state, double *lux)
{
    if (state->found_no_devices)
        return MP_ALS_STATUS_NODEVICE;

    if (!state->device_dir && acquire_sensor(state) < 0) {
        state->found_no_devices = true;
        return MP_ALS_STATUS_NODEVICE;
    }

    return read_als_assume_have_sensor(state, lux);
}
