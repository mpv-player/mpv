/*
 * Only a sample!
 *
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

#include "config.h"

#include <stdio.h>
#include "common/common.h"
#include "tv.h"

static tvi_handle_t *tvi_init_dummy(struct mp_log *log, tv_param_t* tv_param);
/* information about this file */
const tvi_info_t tvi_info_dummy = {
        tvi_init_dummy,
        "NULL-TV",
        "dummy",
};

/* private data's */
typedef struct priv {
    int width;
    int height;
} priv_t;

#include "tvi_def.h"

/* handler creator - entry point ! */
static tvi_handle_t *tvi_init_dummy(struct mp_log *log, tv_param_t* tv_param)
{
    return tv_new_handle(sizeof(priv_t), log, &functions);
}

/* initialisation */
static int init(priv_t *priv)
{
    priv->width = 320;
    priv->height = 200;
    return 1;
}

/* that's the real start, we'got the format parameters (checked with control) */
static int start(priv_t *priv)
{
    return 1;
}

static int uninit(priv_t *priv)
{
    return 1;
}

static int do_control(priv_t *priv, int cmd, void *arg)
{
    switch(cmd)
    {
        case TVI_CONTROL_IS_VIDEO:
            return TVI_CONTROL_TRUE;
        case TVI_CONTROL_VID_GET_FORMAT:
            *(int *)arg = MP_FOURCC_YV12;
            return TVI_CONTROL_TRUE;
        case TVI_CONTROL_VID_SET_FORMAT:
        {
//          int req_fmt = *(int *)arg;
            int req_fmt = *(int *)arg;
            if (req_fmt != MP_FOURCC_YV12)
                return TVI_CONTROL_FALSE;
            return TVI_CONTROL_TRUE;
        }
        case TVI_CONTROL_VID_SET_WIDTH:
            priv->width = *(int *)arg;
            return TVI_CONTROL_TRUE;
        case TVI_CONTROL_VID_GET_WIDTH:
            *(int *)arg = priv->width;
            return TVI_CONTROL_TRUE;
        case TVI_CONTROL_VID_SET_HEIGHT:
            priv->height = *(int *)arg;
            return TVI_CONTROL_TRUE;
        case TVI_CONTROL_VID_GET_HEIGHT:
            *(int *)arg = priv->height;
            return TVI_CONTROL_TRUE;
        case TVI_CONTROL_VID_CHK_WIDTH:
        case TVI_CONTROL_VID_CHK_HEIGHT:
            return TVI_CONTROL_TRUE;
        case TVI_CONTROL_TUN_SET_NORM:
            return TVI_CONTROL_TRUE;
    }
    return TVI_CONTROL_UNKNOWN;
}

static double grab_video_frame(priv_t *priv, char *buffer, int len)
{
    memset(buffer, 0x42, len);
    return MP_NOPTS_VALUE;
}

static int get_video_framesize(priv_t *priv)
{
    /* YV12 */
    return priv->width * priv->height * 12 / 8;
}

static double grab_audio_frame(priv_t *priv, char *buffer, int len)
{
    memset(buffer, 0x42, len);
    return MP_NOPTS_VALUE;
}

static int get_audio_framesize(priv_t *priv)
{
    return 1;
}
