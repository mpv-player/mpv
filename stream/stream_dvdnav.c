/*
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

#include <libavutil/common.h>

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>

#include <dvdnav/dvdnav.h>

#include "osdep/io.h"

#include "options/options.h"
#include "common/msg.h"
#include "input/input.h"
#include "options/m_option.h"
#include "options/path.h"
#include "osdep/timer.h"
#include "stream.h"
#include "demux/demux.h"
#include "discnav.h"
#include "video/out/vo.h"
#include "stream_dvd_common.h"

#define TITLE_MENU -1
#define TITLE_LONGEST -2

struct priv {
    dvdnav_t *dvdnav;                   // handle to libdvdnav stuff
    char *filename;                     // path
    unsigned int duration;              // in milliseconds
    int mousex, mousey;
    int title;
    uint32_t spu_clut[16];
    bool spu_clut_valid;
    dvdnav_highlight_event_t hlev;
    int still_length;                   // still frame duration
    unsigned long next_event;           // bitmask of events to return to player
    bool suspended_read;
    bool nav_enabled;
    bool had_initial_vts;

    int dvd_speed;

    int track;
    char *device;
};

static const struct priv stream_priv_dflts = {
  .track = TITLE_LONGEST,
};

#define OPT_BASE_STRUCT struct priv
static const m_option_t stream_opts_fields[] = {
    OPT_CHOICE_OR_INT("title", track, 0, 0, 99,
                      ({"menu", TITLE_MENU},
                       {"longest", TITLE_LONGEST})),
    OPT_STRING("device", device, 0),
    {0}
};

#define DNE(e) [e] = # e
static const char *const mp_dvdnav_events[] = {
    DNE(DVDNAV_BLOCK_OK),
    DNE(DVDNAV_NOP),
    DNE(DVDNAV_STILL_FRAME),
    DNE(DVDNAV_SPU_STREAM_CHANGE),
    DNE(DVDNAV_AUDIO_STREAM_CHANGE),
    DNE(DVDNAV_VTS_CHANGE),
    DNE(DVDNAV_CELL_CHANGE),
    DNE(DVDNAV_NAV_PACKET),
    DNE(DVDNAV_STOP),
    DNE(DVDNAV_HIGHLIGHT),
    DNE(DVDNAV_SPU_CLUT_CHANGE),
    DNE(DVDNAV_HOP_CHANNEL),
    DNE(DVDNAV_WAIT),
};

static const char *const mp_nav_cmd_types[] = {
    DNE(MP_NAV_CMD_NONE),
    DNE(MP_NAV_CMD_ENABLE),
    DNE(MP_NAV_CMD_DRAIN_OK),
    DNE(MP_NAV_CMD_RESUME),
    DNE(MP_NAV_CMD_SKIP_STILL),
    DNE(MP_NAV_CMD_MENU),
    DNE(MP_NAV_CMD_MOUSE_POS),
};

static const char *const mp_nav_event_types[] = {
    DNE(MP_NAV_EVENT_NONE),
    DNE(MP_NAV_EVENT_RESET),
    DNE(MP_NAV_EVENT_RESET_CLUT),
    DNE(MP_NAV_EVENT_RESET_ALL),
    DNE(MP_NAV_EVENT_DRAIN),
    DNE(MP_NAV_EVENT_STILL_FRAME),
    DNE(MP_NAV_EVENT_HIGHLIGHT),
    DNE(MP_NAV_EVENT_MENU_MODE),
    DNE(MP_NAV_EVENT_EOF),
};

#define LOOKUP_NAME(array, i) \
    (((i) >= 0 && (i) < MP_ARRAY_SIZE(array)) ? array[(i)] : "?")

static void dvdnav_get_highlight(struct priv *priv, int display_mode)
{
    pci_t *pnavpci = NULL;
    dvdnav_highlight_event_t *hlev = &(priv->hlev);
    int btnum;

    if (!priv || !priv->dvdnav)
        return;

    pnavpci = dvdnav_get_current_nav_pci(priv->dvdnav);
    if (!pnavpci) {
        hlev->display = 0;
        return;
    }

    dvdnav_get_current_highlight(priv->dvdnav, &(hlev->buttonN));
    hlev->display = display_mode; /* show */

    if (hlev->buttonN > 0 && pnavpci->hli.hl_gi.btn_ns > 0 && hlev->display) {
        for (btnum = 0; btnum < pnavpci->hli.hl_gi.btn_ns; btnum++) {
            btni_t *btni = &(pnavpci->hli.btnit[btnum]);

            if (hlev->buttonN == btnum + 1) {
                hlev->sx = FFMIN(btni->x_start, btni->x_end);
                hlev->ex = FFMAX(btni->x_start, btni->x_end);
                hlev->sy = FFMIN(btni->y_start, btni->y_end);
                hlev->ey = FFMAX(btni->y_start, btni->y_end);

                hlev->palette = (btni->btn_coln == 0) ?
                    0 : pnavpci->hli.btn_colit.btn_coli[btni->btn_coln - 1][0];
                break;
            }
        }
    } else { /* hide button or no button */
        hlev->sx = hlev->ex = 0;
        hlev->sy = hlev->ey = 0;
        hlev->palette = hlev->buttonN = 0;
    }
}

static void handle_menu_input(stream_t *stream, const char *cmd)
{
    struct priv *priv = stream->priv;
    dvdnav_t *nav = priv->dvdnav;
    dvdnav_status_t status = DVDNAV_STATUS_ERR;
    pci_t *pci = dvdnav_get_current_nav_pci(nav);

    MP_VERBOSE(stream, "DVDNAV: input '%s'\n", cmd);

    if (!pci)
        return;

    if (strcmp(cmd, "up") == 0) {
        status = dvdnav_upper_button_select(nav, pci);
    } else if (strcmp(cmd, "down") == 0) {
        status = dvdnav_lower_button_select(nav, pci);
    } else if (strcmp(cmd, "left") == 0) {
        status = dvdnav_left_button_select(nav, pci);
    } else if (strcmp(cmd, "right") == 0) {
        status = dvdnav_right_button_select(nav, pci);
    } else if (strcmp(cmd, "menu") == 0) {
        status = dvdnav_menu_call(nav, DVD_MENU_Root);
    } else if (strcmp(cmd, "prev") == 0) {
        int title = 0, part = 0;
        dvdnav_current_title_info(nav, &title, &part);
        if (title)
            status = dvdnav_menu_call(nav, DVD_MENU_Part);
        if (status != DVDNAV_STATUS_OK)
            status = dvdnav_menu_call(nav, DVD_MENU_Title);
        if (status != DVDNAV_STATUS_OK)
            status = dvdnav_menu_call(nav, DVD_MENU_Root);
    } else if (strcmp(cmd, "select") == 0) {
        status = dvdnav_button_activate(nav, pci);
    } else if (strcmp(cmd, "mouse") == 0) {
        status = dvdnav_mouse_activate(nav, pci, priv->mousex, priv->mousey);
    } else {
        MP_VERBOSE(stream, "Unknown DVDNAV command: '%s'\n", cmd);
    }
}

static dvdnav_status_t handle_mouse_pos(stream_t *stream, int x, int y)
{
    struct priv *priv = stream->priv;
    dvdnav_t *nav = priv->dvdnav;
    pci_t *pci = dvdnav_get_current_nav_pci(nav);

    if (!pci)
        return DVDNAV_STATUS_ERR;

    dvdnav_status_t status = dvdnav_mouse_select(nav, pci, x, y);
    priv->mousex = x;
    priv->mousey = y;
    return status;
}

/**
 * \brief mp_dvdnav_lang_from_aid() returns the language corresponding to audio id 'aid'
 * \param stream: - stream pointer
 * \param sid: physical subtitle id
 * \return 0 on error, otherwise language id
 */
static int mp_dvdnav_lang_from_aid(stream_t *stream, int aid)
{
    uint8_t lg;
    uint16_t lang;
    struct priv *priv = stream->priv;

    if (aid < 0)
        return 0;
    lg = dvdnav_get_audio_logical_stream(priv->dvdnav, aid & 0x7);
    if (lg == 0xff)
        return 0;
    lang = dvdnav_audio_stream_to_lang(priv->dvdnav, lg);
    if (lang == 0xffff)
        return 0;
    return lang;
}

/**
 * \brief mp_dvdnav_lang_from_sid() returns the language corresponding to subtitle id 'sid'
 * \param stream: - stream pointer
 * \param sid: physical subtitle id
 * \return 0 on error, otherwise language id
 */
static int mp_dvdnav_lang_from_sid(stream_t *stream, int sid)
{
    uint8_t k;
    uint16_t lang;
    struct priv *priv = stream->priv;
    if (sid < 0)
        return 0;
    for (k = 0; k < 32; k++)
        if (dvdnav_get_spu_logical_stream(priv->dvdnav, k) == sid)
            break;
    if (k == 32)
        return 0;
    lang = dvdnav_spu_stream_to_lang(priv->dvdnav, k);
    if (lang == 0xffff)
        return 0;
    return lang;
}

/**
 * \brief mp_dvdnav_number_of_subs() returns the count of available subtitles
 * \param stream: - stream pointer
 * \return 0 on error, something meaningful otherwise
 */
static int mp_dvdnav_number_of_subs(stream_t *stream)
{
    struct priv *priv = stream->priv;
    uint8_t lg, k, n = 0;

    for (k = 0; k < 32; k++) {
        lg = dvdnav_get_spu_logical_stream(priv->dvdnav, k);
        if (lg == 0xff)
            continue;
        if (lg >= n)
            n = lg + 1;
    }
    return n;
}

static void handle_cmd(stream_t *s, struct mp_nav_cmd *ev)
{
    struct priv *priv = s->priv;
    MP_VERBOSE(s, "DVDNAV: input '%s'\n",
           LOOKUP_NAME(mp_nav_cmd_types, ev->event));
    switch (ev->event) {
    case MP_NAV_CMD_ENABLE:
        priv->nav_enabled = true;
        break;
    case MP_NAV_CMD_DRAIN_OK:
        dvdnav_wait_skip(priv->dvdnav);
        break;
    case MP_NAV_CMD_RESUME:
        priv->suspended_read = false;
        break;
    case MP_NAV_CMD_SKIP_STILL:
        dvdnav_still_skip(priv->dvdnav);
        break;
    case MP_NAV_CMD_MENU:
        handle_menu_input(s, ev->u.menu.action);
        break;
    case MP_NAV_CMD_MOUSE_POS:
        ev->mouse_on_button = handle_mouse_pos(s, ev->u.mouse_pos.x, ev->u.mouse_pos.y);
        break;
    }

}

static inline bool set_event_type(struct priv *priv, int type,
                                  struct mp_nav_event *event)
{
    if (!(priv->next_event & (1 << type)))
        return false;
    priv->next_event &= ~(1 << type);
    event->event = type;
    return true;
}

static void fill_next_event(stream_t *s, struct mp_nav_event **ret)
{
    struct priv *priv = s->priv;
    struct mp_nav_event e = {0};
    if (!set_event_type(priv, MP_NAV_EVENT_RESET_ALL, &e))
        for (int n = 0; n < 30 && !set_event_type(priv, n, &e); n++) ;
    switch (e.event) {
    case MP_NAV_EVENT_NONE:
        return;
    case MP_NAV_EVENT_HIGHLIGHT: {
        dvdnav_highlight_event_t hlev = priv->hlev;
        e.u.highlight.display = hlev.display;
        e.u.highlight.sx = hlev.sx;
        e.u.highlight.sy = hlev.sy;
        e.u.highlight.ex = hlev.ex;
        e.u.highlight.ey = hlev.ey;
        e.u.highlight.palette = hlev.palette;
        break;
    }
    case MP_NAV_EVENT_MENU_MODE:
        e.u.menu_mode.enable = !dvdnav_is_domain_vts(priv->dvdnav);
        break;
    case MP_NAV_EVENT_STILL_FRAME:
        e.u.still_frame.seconds = priv->still_length;
        break;
    }
    *ret = talloc(NULL, struct mp_nav_event);
    **ret = e;

    MP_VERBOSE(s, "DVDNAV: player event '%s'\n",
                LOOKUP_NAME(mp_nav_event_types, e.event));
}

static int fill_buffer(stream_t *s, char *buf, int max_len)
{
    struct priv *priv = s->priv;
    dvdnav_t *dvdnav = priv->dvdnav;

    if (max_len < 2048)
        return -1;

    while (1) {
        if (priv->suspended_read)
            return -1;

        int len = -1;
        int event = DVDNAV_NOP;
        if (dvdnav_get_next_block(dvdnav, buf, &event, &len) != DVDNAV_STATUS_OK)
        {
            MP_ERR(s, "Error getting next block from DVD %d (%s)\n",
                   event, dvdnav_err_to_string(dvdnav));
            return 0;
        }
        if (event != DVDNAV_BLOCK_OK) {
            const char *name = LOOKUP_NAME(mp_dvdnav_events, event);
            MP_VERBOSE(s, "DVDNAV: event %s (%d).\n", name, event);
            dvdnav_get_highlight(priv, 1);
        }
        switch (event) {
        case DVDNAV_BLOCK_OK:
            return len;
        case DVDNAV_STOP: {
            priv->next_event |= 1 << MP_NAV_EVENT_EOF;
            return 0;
        }
        case DVDNAV_NAV_PACKET: {
            pci_t *pnavpci = dvdnav_get_current_nav_pci(dvdnav);
            uint32_t start_pts = pnavpci->pci_gi.vobu_s_ptm;
            MP_TRACE(s, "start pts = %"PRIu32"\n", start_pts);
            break;
        }
        case DVDNAV_STILL_FRAME: {
            dvdnav_still_event_t *still_event = (dvdnav_still_event_t *) buf;
            priv->still_length = still_event->length;
            if (priv->still_length == 255)
                priv->still_length = -1;
            MP_VERBOSE(s, "len=%d\n", priv->still_length);
            /* set still frame duration */
            if (priv->still_length <= 1) {
                pci_t *pnavpci = dvdnav_get_current_nav_pci(dvdnav);
                priv->duration = mp_dvdtimetomsec(&pnavpci->pci_gi.e_eltm);
            }
            if (priv->nav_enabled) {
                priv->next_event |= 1 << MP_NAV_EVENT_STILL_FRAME;
            } else {
                dvdnav_still_skip(dvdnav);
            }
            return 0;
        }
        case DVDNAV_WAIT: {
            if (priv->nav_enabled) {
                priv->next_event |= 1 << MP_NAV_EVENT_DRAIN;
            } else {
                dvdnav_wait_skip(dvdnav);
            }
            return 0;
        }
        case DVDNAV_HIGHLIGHT: {
            dvdnav_get_highlight(priv, 1);
            priv->next_event |= 1 << MP_NAV_EVENT_HIGHLIGHT;
            break;
        }
        case DVDNAV_VTS_CHANGE: {
            int tit = 0, part = 0;
            dvdnav_vts_change_event_t *vts_event =
                (dvdnav_vts_change_event_t *)s->buffer;
            MP_INFO(s, "DVDNAV, switched to title: %d\n",
                   vts_event->new_vtsN);
            if (!priv->had_initial_vts) {
                // dvdnav sends an initial VTS change before any data; don't
                // cause a blocking wait for the player, because the player in
                // turn can't initialize the demuxer without data.
                priv->had_initial_vts = true;
                break;
            }
            // clear all previous events
            priv->next_event = 0;
            priv->next_event |= 1 << MP_NAV_EVENT_RESET;
            priv->next_event |= 1 << MP_NAV_EVENT_RESET_ALL;
            if (dvdnav_current_title_info(dvdnav, &tit, &part) == DVDNAV_STATUS_OK)
            {
                MP_VERBOSE(s, "DVDNAV, NEW TITLE %d\n", tit);
                dvdnav_get_highlight(priv, 0);
                if (priv->title > 0 && tit != priv->title) {
                    priv->next_event |= 1 << MP_NAV_EVENT_EOF;;
                    MP_WARN(s, "Requested title not found\n");
                }
            }
            if (priv->nav_enabled)
                priv->suspended_read = true;
            break;
        }
        case DVDNAV_CELL_CHANGE: {
            dvdnav_cell_change_event_t *ev =  (dvdnav_cell_change_event_t *)buf;

            priv->next_event |= 1 << MP_NAV_EVENT_RESET;
            priv->next_event |= 1 << MP_NAV_EVENT_MENU_MODE;
            if (ev->pgc_length)
                priv->duration = ev->pgc_length / 90;

            dvdnav_get_highlight(priv, 1);
            break;
        }
        case DVDNAV_SPU_CLUT_CHANGE: {
            memcpy(priv->spu_clut, buf, 16 * sizeof(uint32_t));
            priv->spu_clut_valid = true;
            priv->next_event |= 1 << MP_NAV_EVENT_RESET_CLUT;
            break;
        }
        }
    }
    return 0;
}

static int control(stream_t *stream, int cmd, void *arg)
{
    struct priv *priv = stream->priv;
    dvdnav_t *dvdnav = priv->dvdnav;
    int tit, part;

    switch (cmd) {
    case STREAM_CTRL_GET_NUM_CHAPTERS: {
        if (dvdnav_current_title_info(dvdnav, &tit, &part) != DVDNAV_STATUS_OK)
            break;
        if (dvdnav_get_number_of_parts(dvdnav, tit, &part) != DVDNAV_STATUS_OK)
            break;
        if (!part)
            break;
        *(unsigned int *)arg = part;
        return 1;
    }
    case STREAM_CTRL_GET_CHAPTER_TIME: {
        double *ch = arg;
        int chapter = *ch;
        if (dvdnav_current_title_info(dvdnav, &tit, &part) != DVDNAV_STATUS_OK)
            break;
        uint64_t *parts = NULL, duration = 0;
        int n = dvdnav_describe_title_chapters(dvdnav, tit, &parts, &duration);
        if (!parts)
            break;
        if (chapter < 0 || chapter + 1 > n)
            break;
        *ch = chapter > 0 ? parts[chapter - 1] / 90000.0 : 0;
        free(parts);
        return 1;
    }
    case STREAM_CTRL_GET_TIME_LENGTH: {
        if (priv->duration) {
            *(double *)arg = (double)priv->duration / 1000.0;
            return 1;
        }
        break;
    }
    case STREAM_CTRL_GET_ASPECT_RATIO: {
        uint8_t ar = dvdnav_get_video_aspect(dvdnav);
        *(double *)arg = !ar ? 4.0 / 3.0 : 16.0 / 9.0;
        return 1;
    }
    case STREAM_CTRL_GET_CURRENT_TIME: {
        double tm;
        tm = dvdnav_get_current_time(dvdnav) / 90000.0f;
        if (tm != -1) {
            *(double *)arg = tm;
            return 1;
        }
        break;
    }
    case STREAM_CTRL_GET_NUM_TITLES: {
        int32_t num_titles = 0;
        if (dvdnav_get_number_of_titles(dvdnav, &num_titles) != DVDNAV_STATUS_OK)
            break;
        *((unsigned int*)arg)= num_titles;
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_TITLE_LENGTH: {
        int t = *(double *)arg;
        int32_t num_titles = 0;
        if (dvdnav_get_number_of_titles(dvdnav, &num_titles) != DVDNAV_STATUS_OK)
            break;
        if (t < 0 || t >= num_titles)
            break;
        uint64_t duration = 0;
        uint64_t *parts = NULL;
        dvdnav_describe_title_chapters(dvdnav, t + 1, &parts, &duration);
        if (!parts)
            break;
        free(parts);
        *(double *)arg = duration / 90000.0;
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_CURRENT_TITLE: {
        if (dvdnav_current_title_info(dvdnav, &tit, &part) != DVDNAV_STATUS_OK)
            break;
        *((unsigned int *) arg) = tit - 1;
        return STREAM_OK;
    }
    case STREAM_CTRL_SET_CURRENT_TITLE: {
        int title = *((unsigned int *) arg);
        if (dvdnav_title_play(priv->dvdnav, title + 1) != DVDNAV_STATUS_OK)
            break;
        stream_drop_buffers(stream);
        return STREAM_OK;
    }
    case STREAM_CTRL_SEEK_TO_TIME: {
        double *args = arg;
        double d = args[0]; // absolute target timestamp
        double r = args[1]; // if not SEEK_ABSOLUTE, the base time for d
        int flags = args[2]; // from SEEK_* flags (demux.h)
        if (flags & SEEK_HR)
            d -= 10; // fudge offset; it's a hack, because fuck libdvd*
        int64_t tm = (int64_t)(d * 90000);
        if (tm < 0)
            tm = 0;
        if (priv->duration && tm >= (priv->duration * 90))
            tm = priv->duration * 90 - 1;
        uint32_t pos, len;
        if (dvdnav_get_position(dvdnav, &pos, &len) != DVDNAV_STATUS_OK)
            break;
        // The following is convoluted, because we have to translate between
        // dvdnav's block/CBR-based seeking bullshit, and the player's
        // timestamp-based high-level machinery.
        if (!(flags & SEEK_ABSOLUTE) && !(flags & SEEK_HR) && priv->duration > 0)
        {
            int dir = (flags & SEEK_BACKWARD) ? -1 : 1;
            // The user is making a relative seek (translated to absolute),
            // and we try not to get the user stuck on "boundaries". So try
            // to do block based seeks, which should workaround libdvdnav's
            // terrible CBR-based seeking.
            d -= r; // relative seek amount in seconds
            d = d / (priv->duration / 1000.0) * len; // d is now in blocks
            d += pos; // absolute target in blocks
            if (dir > 0)
                d = MPMAX(d, pos + 1.0);
            if (dir < 0)
                d = MPMIN(d, pos - 1.0);
            d += 0.5; // round
            uint32_t target = MPCLAMP(d, 0, len);
            MP_VERBOSE(stream, "seek from block %lu to %lu, dir=%d\n",
                       (unsigned long)pos, (unsigned long)target, dir);
            if (dvdnav_sector_search(dvdnav, target, SEEK_SET) != DVDNAV_STATUS_OK)
                break;
        } else {
            // "old" method, should be good enough for large seeks. Used for
            // hr-seeks (with fudge offset), because I fear that block-based
            // seeking might be off too far for large jumps.
            MP_VERBOSE(stream, "seek to PTS %f (%"PRId64")\n", d, tm);
            if (dvdnav_time_search(dvdnav, tm) != DVDNAV_STATUS_OK)
                break;
        }
        stream_drop_buffers(stream);
        d = dvdnav_get_current_time(dvdnav) / 90000.0f;
        MP_VERBOSE(stream, "landed at: %f\n", d);
        if (dvdnav_get_position(dvdnav, &pos, &len) == DVDNAV_STATUS_OK)
            MP_VERBOSE(stream, "block: %lu\n", (unsigned long)pos);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_NUM_ANGLES: {
        uint32_t curr, angles;
        if (dvdnav_get_angle_info(dvdnav, &curr, &angles) != DVDNAV_STATUS_OK)
            break;
        *(int *)arg = angles;
        return 1;
    }
    case STREAM_CTRL_GET_ANGLE: {
        uint32_t curr, angles;
        if (dvdnav_get_angle_info(dvdnav, &curr, &angles) != DVDNAV_STATUS_OK)
            break;
        *(int *)arg = curr;
        return 1;
    }
    case STREAM_CTRL_SET_ANGLE: {
        uint32_t curr, angles;
        int new_angle = *(int *)arg;
        if (dvdnav_get_angle_info(dvdnav, &curr, &angles) != DVDNAV_STATUS_OK)
            break;
        if (new_angle > angles || new_angle < 1)
            break;
        if (dvdnav_angle_change(dvdnav, new_angle) != DVDNAV_STATUS_OK)
            return 1;
    }
    case STREAM_CTRL_GET_LANG: {
        struct stream_lang_req *req = arg;
        int lang = 0;
        switch (req->type) {
        case STREAM_AUDIO:
            lang = mp_dvdnav_lang_from_aid(stream, req->id);
            break;
        case STREAM_SUB:
            lang = mp_dvdnav_lang_from_sid(stream, req->id);
            break;
        }
        if (!lang)
            break;
        snprintf(req->name, sizeof(req->name), "%c%c", lang >> 8, lang);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_DVD_INFO: {
        struct stream_dvd_info_req *req = arg;
        memset(req, 0, sizeof(*req));
        req->num_subs = mp_dvdnav_number_of_subs(stream);
        assert(sizeof(uint32_t) == sizeof(unsigned int));
        memcpy(req->palette, priv->spu_clut, sizeof(req->palette));
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_NAV_EVENT: {
        struct mp_nav_event **ev = arg;
        if (ev)
            fill_next_event(stream, ev);
        return STREAM_OK;
    }
    case STREAM_CTRL_NAV_CMD: {
        handle_cmd(stream, (struct mp_nav_cmd *)arg);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_DISC_NAME: {
        const char *volume = NULL;
        if (dvdnav_get_title_string(dvdnav, &volume) != DVDNAV_STATUS_OK)
            break;
        if (!volume || !volume[0])
            break;
        *(char**)arg = talloc_strdup(NULL, volume);
        return STREAM_OK;
    }
    }

    return STREAM_UNSUPPORTED;
}

static void stream_dvdnav_close(stream_t *s)
{
    struct priv *priv = s->priv;
    dvdnav_close(priv->dvdnav);
    priv->dvdnav = NULL;
    if (priv->dvd_speed)
        dvd_set_speed(s, priv->filename, -1);
    if (priv->filename)
        free(priv->filename);
}

static struct priv *new_dvdnav_stream(stream_t *stream, char *filename)
{
    struct priv *priv = stream->priv;
    const char *title_str;

    if (!filename)
        return NULL;

    if (!(priv->filename = strdup(filename)))
        return NULL;

    priv->dvd_speed = stream->opts->dvd_speed;
    dvd_set_speed(stream, priv->filename, priv->dvd_speed);

    if (dvdnav_open(&(priv->dvdnav), priv->filename) != DVDNAV_STATUS_OK) {
        free(priv->filename);
        priv->filename = NULL;
        return NULL;
    }

    if (!priv->dvdnav)
        return NULL;

    dvdnav_set_readahead_flag(priv->dvdnav, 1);
    if (dvdnav_set_PGC_positioning_flag(priv->dvdnav, 1) != DVDNAV_STATUS_OK)
        MP_ERR(stream, "stream_dvdnav, failed to set PGC positioning\n");
    /* report the title?! */
    dvdnav_get_title_string(priv->dvdnav, &title_str);

    return priv;
}

static int open_s(stream_t *stream)
{
    struct priv *priv, *p;
    priv = p = stream->priv;
    char *filename;

    if (p->device && p->device[0])
        filename = p->device;
    else if (stream->opts->dvd_device && stream->opts->dvd_device[0])
        filename = stream->opts->dvd_device;
    else
        filename = DEFAULT_DVD_DEVICE;
    if (!new_dvdnav_stream(stream, filename)) {
        MP_ERR(stream, "Couldn't open DVD device: %s\n",
                filename);
        return STREAM_UNSUPPORTED;
    }

    if (p->track == TITLE_LONGEST) { // longest
        dvdnav_t *dvdnav = priv->dvdnav;
        uint64_t best_length = 0;
        int best_title = -1;
        int32_t num_titles;
        if (dvdnav_get_number_of_titles(dvdnav, &num_titles) == DVDNAV_STATUS_OK) {
            for (int n = 1; n <= num_titles; n++) {
                uint64_t *parts = NULL, duration = 0;
                dvdnav_describe_title_chapters(dvdnav, n, &parts, &duration);
                if (parts) {
                    if (duration > best_length) {
                        best_length = duration;
                        best_title = n;
                    }
                    free(parts);
                }
            }
        }
        p->track = best_title - 1;
        MP_INFO(stream, "Selecting title %d.\n", p->track);
    }

    if (p->track >= 0) {
        priv->title = p->track;
        if (dvdnav_title_play(priv->dvdnav, p->track + 1) != DVDNAV_STATUS_OK) {
            MP_FATAL(stream, "dvdnav_stream, couldn't select title %d, error '%s'\n",
                   p->track, dvdnav_err_to_string(priv->dvdnav));
            return STREAM_UNSUPPORTED;
        }
    } else {
        if (dvdnav_menu_call(priv->dvdnav, DVD_MENU_Root) != DVDNAV_STATUS_OK)
            dvdnav_menu_call(priv->dvdnav, DVD_MENU_Title);
    }
    if (stream->opts->dvd_angle > 1)
        dvdnav_angle_change(priv->dvdnav, stream->opts->dvd_angle);

    stream->sector_size = 2048;
    stream->fill_buffer = fill_buffer;
    stream->control = control;
    stream->close = stream_dvdnav_close;
    stream->type = STREAMTYPE_DVD;
    stream->demuxer = "+disc";
    stream->lavf_type = "mpeg";
    stream->allow_caching = false;

    return STREAM_OK;
}

const stream_info_t stream_info_dvdnav = {
    .name = "dvdnav",
    .open = open_s,
    .protocols = (const char*const[]){ "dvd", "dvdnav", NULL },
    .priv_size = sizeof(struct priv),
    .priv_defaults = &stream_priv_dflts,
    .options = stream_opts_fields,
    .url_options = (const char*const[]){
        "hostname=title",
        "filename=device",
        NULL
    },
};

static bool check_ifo(const char *path)
{
    if (strcasecmp(mp_basename(path), "video_ts.ifo"))
        return false;

    return dvd_probe(path, ".ifo", "DVDVIDEO-VMG");
}

static int ifo_dvdnav_stream_open(stream_t *stream)
{
    struct priv *priv = talloc_ptrtype(stream, priv);
    stream->priv = priv;
    *priv = stream_priv_dflts;

    char *path = mp_file_get_path(priv, bstr0(stream->url));
    if (!path)
        goto unsupported;

    // We allow the path to point to a directory containing VIDEO_TS/, a
    // directory containing VIDEO_TS.IFO, or that file itself.
    if (!check_ifo(path)) {
        // On UNIX, just assume the filename is always uppercase.
        char *npath = mp_path_join(priv, bstr0(path), bstr0("VIDEO_TS.IFO"));
        if (!check_ifo(npath)) {
            npath = mp_path_join(priv, bstr0(path), bstr0("VIDEO_TS/VIDEO_TS.IFO"));
            if (!check_ifo(npath))
                goto unsupported;
        }
        path = npath;
    }

    priv->device = bstrto0(priv, mp_dirname(path));

    MP_INFO(stream, ".IFO detected. Redirecting to dvd://\n");
    return open_s(stream);

unsupported:
    talloc_free(priv);
    stream->priv = NULL;
    return STREAM_UNSUPPORTED;
}

const stream_info_t stream_info_ifo_dvdnav = {
    .name = "ifo/dvdnav",
    .open = ifo_dvdnav_stream_open,
    .protocols = (const char*const[]){ "file", "", NULL },
};
