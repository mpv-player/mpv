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

#include "config.h"

#if !HAVE_GPL
#error GPL only
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#ifdef __linux__
#include <linux/cdrom.h>
#include <scsi/sg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#endif

#include <dvdnav/dvdnav.h>
#include <libavutil/common.h>
#include <libavutil/intreadwrite.h>

#include "osdep/io.h"

#include "options/options.h"
#include "common/msg.h"
#include "input/input.h"
#include "misc/thread_tools.h"
#include "options/m_config.h"
#include "options/path.h"
#include "osdep/timer.h"
#include "stream.h"
#include "demux/demux.h"
#include "video/csputils.h"
#include "video/out/vo.h"

#define TITLE_MENU -1
#define TITLE_LONGEST -2

// Default source dimensions if dvdnav_get_video_resolution() fails.
#define DVD_SRC_W_DEFAULT 720
#define DVD_SRC_H_DEFAULT 576

struct priv {
    dvdnav_t *dvdnav;                   // handle to libdvdnav stuff
    char *filename;                     // path
    unsigned int duration;              // in milliseconds
    int title;
    uint32_t spu_clut[16];
    bool spu_clut_valid;
    bool had_initial_vts;

    int dvd_speed;

    int track;
    char *device;

    bool in_menu;
    int current_button;                 // mirror of libdvdnav HL_BTNN_REG
    int btn_rect[4];                    // x, y, w, h in source coords
    uint32_t hl_palette[4];             // 0xAARRGGBB for SPU pixel values 0..3
    uint32_t nav_change_id;
    uint32_t discontinuity_id;          // bumped on actions that may jump
    int src_w, src_h;                   // video resolution in pixels
    int auto_actioned_button;           // last auto-activated button; 0 if none

    struct dvd_opts *opts;
};

struct dvd_opts {
    int angle;
    int speed;
    char *device;
};

#define OPT_BASE_STRUCT struct dvd_opts

const struct m_sub_options dvd_conf = {
    .opts = (const struct m_option[]){
        {"device", OPT_STRING(device), .flags = M_OPT_FILE},
        {"speed", OPT_INT(speed)},
        {"angle", OPT_INT(angle), M_RANGE(1, 99)},
        {0}
    },
    .size = sizeof(struct dvd_opts),
    .defaults = &(const struct dvd_opts){
        .angle = 1,
    },
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

#define LOOKUP_NAME(array, i) \
    (((i) >= 0 && (i) < MP_ARRAY_SIZE(array)) ? array[(i)] : "?")

static void dvd_set_speed(stream_t *stream, char *device, unsigned speed)
{
#if defined(__linux__) && defined(SG_IO) && defined(GPCMD_SET_STREAMING)
    int fd;
    unsigned char buffer[28];
    unsigned char cmd[12];
    struct sg_io_hdr sghdr;
    struct stat st;

    memset(&st, 0, sizeof(st));

    if (stat(device, &st) == -1) return;

    if (!S_ISBLK(st.st_mode)) return; /* not a block device */

    switch (speed) {
    case 0: /* don't touch speed setting */
        return;
    case -1: /* restore default value */
        MP_INFO(stream, "Restoring DVD speed... ");
        break;
    default: /* limit to <speed> KB/s */
        // speed < 100 is multiple of DVD single speed (1350KB/s)
        if (speed < 100)
            speed *= 1350;
        MP_INFO(stream, "Limiting DVD speed to %dKB/s... ", speed);
        break;
    }

    memset(&sghdr, 0, sizeof(sghdr));
    sghdr.interface_id = 'S';
    sghdr.timeout = 5000;
    sghdr.dxfer_direction = SG_DXFER_TO_DEV;
    sghdr.dxfer_len = sizeof(buffer);
    sghdr.dxferp = buffer;
    sghdr.cmd_len = sizeof(cmd);
    sghdr.cmdp = cmd;

    memset(cmd, 0, sizeof(cmd));
    cmd[0] = GPCMD_SET_STREAMING;
    cmd[10] = sizeof(buffer);

    memset(buffer, 0, sizeof(buffer));
    /* first sector 0, last sector 0xffffffff */
    AV_WB32(buffer + 8, 0xffffffff);
    if (speed == -1)
        buffer[0] = 4; /* restore default */
    else {
        /* <speed> kilobyte */
        AV_WB32(buffer + 12, speed);
        AV_WB32(buffer + 20, speed);
    }
    /* 1 second */
    AV_WB16(buffer + 18, 1000);
    AV_WB16(buffer + 26, 1000);

    fd = open(device, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd == -1) {
        MP_INFO(stream, "Couldn't open DVD device for writing, changing DVD speed needs write access.\n");
        return;
    }

    if (ioctl(fd, SG_IO, &sghdr) < 0)
        MP_INFO(stream, "failed\n");
    else
        MP_INFO(stream, "successful\n");

    close(fd);
#endif
}

// Check if this is likely to be an .ifo or similar file.
static int dvd_probe(const char *path, const char *ext, const char *sig)
{
    if (!bstr_case_endswith(bstr0(path), bstr0(ext)))
        return false;

    FILE *temp = fopen(path, "rb");
    if (!temp)
        return false;

    bool r = false;

    char data[50];

    mp_assert(strlen(sig) <= sizeof(data));

    if (fread(data, 50, 1, temp) == 1) {
        if (memcmp(data, sig, strlen(sig)) == 0)
            r = true;
    }

    fclose(temp);
    return r;
}

static bool in_menu_domain(dvdnav_t *dvdnav)
{
    return dvdnav_is_domain_vmgm(dvdnav) == 1 ||
           dvdnav_is_domain_vtsm(dvdnav) == 1 ||
           dvdnav_is_domain_fp(dvdnav) == 1;
}

static void compute_button_rect(struct priv *priv, pci_t *pci, int btn)
{
    priv->btn_rect[0] = priv->btn_rect[1] = 0;
    priv->btn_rect[2] = priv->btn_rect[3] = 0;
    if (btn <= 0 || btn > pci->hli.hl_gi.btn_ns)
        return;
    // btni_t is packed and full of bitfields, memcpy to ensure correct alignment.
    btni_t b;
    memcpy(&b, &pci->hli.btnit[btn - 1], sizeof(b));
    int xs = b.x_start, xe = b.x_end;
    int ys = b.y_start, ye = b.y_end;
    priv->btn_rect[0] = xs;
    priv->btn_rect[1] = ys;
    priv->btn_rect[2] = xe > xs ? xe - xs : 0;
    priv->btn_rect[3] = ye > ys ? ye - ys : 0;
}

// Resolve the 4-entry "select-state" highlight palette for the focused button.
// btn_coli holds two packed words per color set ([select, action]); the select
// word encodes [Ci3 Ci2 Ci1 Ci0 A3 A2 A1 A0]: four CLUT indices and four 4-bit
// alphas.
static void compute_highlight_palette(struct priv *priv, pci_t *pci, int btn)
{
    memset(priv->hl_palette, 0, sizeof(priv->hl_palette));
    if (!priv->spu_clut_valid || btn <= 0 || btn > pci->hli.hl_gi.btn_ns)
        return;
    btni_t b;
    memcpy(&b, &pci->hli.btnit[btn - 1], sizeof(b));
    if (b.btn_coln == 0)
        return; // spec: "no color" - button is invisible
    int coln = b.btn_coln - 1;
    if (coln > 2)
        coln = 2;
    uint32_t coli;
    memcpy(&coli, &pci->hli.btn_colit.btn_coli[coln][0], sizeof(coli)); // [0] = select state

    struct mp_csp_params csp = MP_CSP_PARAMS_DEFAULTS;
    struct pl_transform3x3 cmatrix;
    mp_get_csp_matrix(&csp, &cmatrix);

    for (int i = 0; i < 4; i++) {
        uint8_t ci = (coli >> (16 + i * 4)) & 0xF;
        uint8_t a  = (coli >> (i * 4)) & 0xF;
        uint32_t entry = priv->spu_clut[ci];
        // CLUT entry is 0x00YYCrCb. mp_get_csp_matrix returns a YCbCr→RGB matrix
        // expecting {Y, Cb, Cr}, reorder to match this.
        int y[3] = {(entry >> 16) & 0xff, entry & 0xff, (entry >> 8) & 0xff};
        int c[3];
        mp_map_fixp_color(&cmatrix, 8, y, 8, c);
        uint32_t alpha = (a << 4) | a;
        priv->hl_palette[i] = (alpha << 24) | (c[0] << 16) | (c[1] << 8) | c[2];
    }
}

static void refresh_video_resolution(struct priv *priv)
{
    uint32_t w = 0, h = 0;
    if (dvdnav_get_video_resolution(priv->dvdnav, &w, &h) == DVDNAV_STATUS_OK &&
        w > 0 && h > 0)
    {
        priv->src_w = (int)w;
        priv->src_h = (int)h;
    } else {
        priv->src_w = DVD_SRC_W_DEFAULT;
        priv->src_h = DVD_SRC_H_DEFAULT;
    }
}

// Pull the current selection back from libdvdnav and refresh our overlay
// state. Called on NAV_PACKET/HIGHLIGHT events and after every nav command.
static void update_highlight(struct priv *priv)
{
    int prev_btn = priv->current_button;
    int prev_x = priv->btn_rect[0], prev_y = priv->btn_rect[1];
    int prev_w = priv->btn_rect[2], prev_h = priv->btn_rect[3];
    bool prev_menu = priv->in_menu;
    uint32_t prev_palette[4];
    memcpy(prev_palette, priv->hl_palette, sizeof(prev_palette));

    priv->in_menu = in_menu_domain(priv->dvdnav);
    pci_t *pci = priv->in_menu ? dvdnav_get_current_nav_pci(priv->dvdnav) : NULL;
    bool has_buttons = pci && pci->hli.hl_gi.hli_ss != 0 && pci->hli.hl_gi.btn_ns > 0;

    // Suppress the visible highlight while we're inside the menu's intro.
    // The PCI gives us both the current VOBU's start PTS and the highlight
    // valid window; once vobu_s_ptm catches up to hli_s_ptm (and we're still
    // inside hli_e_ptm), it's live.
    bool highlight_live = false;
    if (has_buttons) {
        uint32_t now = pci->pci_gi.vobu_s_ptm;
        uint32_t hs  = pci->hli.hl_gi.hli_s_ptm;
        uint32_t he  = pci->hli.hl_gi.hli_e_ptm;
        highlight_live = now >= hs && (he == 0 || now < he);
    }

    int32_t btn = 0;
    if (highlight_live)
        dvdnav_get_current_highlight(priv->dvdnav, &btn);

    if (!highlight_live || btn <= 0 || btn > pci->hli.hl_gi.btn_ns) {
        priv->current_button = 0;
        priv->btn_rect[0] = priv->btn_rect[1] = 0;
        priv->btn_rect[2] = priv->btn_rect[3] = 0;
        memset(priv->hl_palette, 0, sizeof(priv->hl_palette));
    } else {
        priv->current_button = btn;
        compute_button_rect(priv, pci, btn);
        compute_highlight_palette(priv, pci, btn);
    }

    if (priv->in_menu != prev_menu || priv->current_button != prev_btn ||
        priv->btn_rect[0] != prev_x || priv->btn_rect[1] != prev_y ||
        priv->btn_rect[2] != prev_w || priv->btn_rect[3] != prev_h ||
        memcmp(prev_palette, priv->hl_palette, sizeof(prev_palette)) != 0)
    {
        priv->nav_change_id++;
    }

    // When we leave the menu, clear the auto-action latch so the next entry
    // can fire again on the same button number.
    if (!priv->in_menu)
        priv->auto_actioned_button = 0;

    // Auto-action buttons: a btnit entry can request immediate activation
    // when its button gets focus (auto_action_mode == 1).
    // Fire it once per menu entry / button transition.
    if (highlight_live && priv->current_button > 0 &&
        priv->current_button != priv->auto_actioned_button)
    {
        btni_t b;
        memcpy(&b, &pci->hli.btnit[priv->current_button - 1], sizeof(b));
        if (b.auto_action_mode == 1) {
            priv->auto_actioned_button = priv->current_button;
            dvdnav_button_activate(priv->dvdnav, pci);
        }
    }
}

static void handle_nav_cmd(stream_t *stream, struct stream_nav_cmd *cmd)
{
    struct priv *priv = stream->priv;

    switch (cmd->action) {
    case STREAM_NAV_MENU_ROOT:
        dvdnav_menu_call(priv->dvdnav, DVD_MENU_Root);
        update_highlight(priv);
        return;
    case STREAM_NAV_MENU_TITLE:
        dvdnav_menu_call(priv->dvdnav, DVD_MENU_Title);
        update_highlight(priv);
        return;
    case STREAM_NAV_MENU_POPUP:
        dvdnav_menu_call(priv->dvdnav, DVD_MENU_Part);
        update_highlight(priv);
        return;
    case STREAM_NAV_PREV_MENU:
        dvdnav_menu_call(priv->dvdnav, DVD_MENU_Escape);
        update_highlight(priv);
        return;
    default:
        break;
    }

    if (!in_menu_domain(priv->dvdnav))
        return;

    pci_t *pci = dvdnav_get_current_nav_pci(priv->dvdnav);
    if (!pci || pci->hli.hl_gi.hli_ss == 0 || pci->hli.hl_gi.btn_ns == 0)
        return;

    switch (cmd->action) {
    case STREAM_NAV_UP:
        dvdnav_upper_button_select(priv->dvdnav, pci);
        break;
    case STREAM_NAV_DOWN:
        dvdnav_lower_button_select(priv->dvdnav, pci);
        break;
    case STREAM_NAV_LEFT:
        dvdnav_left_button_select(priv->dvdnav, pci);
        break;
    case STREAM_NAV_RIGHT:
        dvdnav_right_button_select(priv->dvdnav, pci);
        break;
    case STREAM_NAV_MOUSE_MOVE:
        dvdnav_mouse_select(priv->dvdnav, pci, cmd->x, cmd->y);
        break;
    case STREAM_NAV_MOUSE_CLICK:
        dvdnav_mouse_activate(priv->dvdnav, pci, cmd->x, cmd->y);
        break;
    case STREAM_NAV_SELECT:
        dvdnav_button_activate(priv->dvdnav, pci);
        break;
    default:
        break;
    }

    update_highlight(priv);
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

static int fill_buffer(stream_t *s, void *buf, int max_len)
{
    struct priv *priv = s->priv;
    dvdnav_t *dvdnav = priv->dvdnav;

    if (max_len < 2048) {
        MP_FATAL(s, "Short read size. Data corruption will follow. Please "
                    "provide a patch.\n");
        return -1;
    }

    while (1) {
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
            MP_TRACE(s, "DVDNAV: event %s (%d).\n", name, event);
        }
        switch (event) {
        case DVDNAV_BLOCK_OK:
            return len;
        case DVDNAV_STOP:
            return 0;
        case DVDNAV_NAV_PACKET: {
            pci_t *pnavpci = dvdnav_get_current_nav_pci(dvdnav);
            uint32_t start_pts = pnavpci->pci_gi.vobu_s_ptm;
            MP_TRACE(s, "start pts = %"PRIu32"\n", start_pts);
            // Each NAV packet can change the highlighted button or the
            // available button set; keep our mirrored state in sync.
            update_highlight(priv);
            break;
        }
        case DVDNAV_STILL_FRAME:
            dvdnav_still_skip(dvdnav);
            break;
        case DVDNAV_WAIT:
            dvdnav_wait_skip(dvdnav);
            break;
        case DVDNAV_HOP_CHANNEL:
            // Bump discontinuity_id so the playloop flushes the cache.
            priv->discontinuity_id++;
            break;
        case DVDNAV_HIGHLIGHT:
            update_highlight(priv);
            break;
        case DVDNAV_VTS_CHANGE: {
            int tit = 0, part = 0;
            dvdnav_vts_change_event_t *vts_event =
                (dvdnav_vts_change_event_t *)buf;
            MP_VERBOSE(s, "DVDNAV, switched to VTS: %d\n",
                       vts_event->new_vtsN);
            if (!priv->had_initial_vts) {
                // dvdnav sends an initial VTS change before any data; don't
                // cause a blocking wait for the player, because the player in
                // turn can't initialize the demuxer without data.
                priv->had_initial_vts = true;
                break;
            }
            if (dvdnav_current_title_info(dvdnav, &tit, &part) == DVDNAV_STATUS_OK)
            {
                MP_VERBOSE(s, "DVDNAV, NEW TITLE %d\n", tit);
                if (priv->title > 0 && tit != priv->title)
                    MP_WARN(s, "Requested title not found\n");
            }
            // Resolution can change across VTS (PAL vs. NTSC titles); refresh
            // so mouse coordinate translation stays correct.
            refresh_video_resolution(priv);
            // VTS change is a title-set boundary, flush.
            priv->discontinuity_id++;
            break;
        }
        case DVDNAV_CELL_CHANGE: {
            dvdnav_cell_change_event_t *ev =  (dvdnav_cell_change_event_t *)buf;

            if (ev->pgc_length)
                priv->duration = ev->pgc_length / 90;

            break;
        }
        case DVDNAV_SPU_CLUT_CHANGE: {
            memcpy(priv->spu_clut, buf, 16 * sizeof(uint32_t));
            priv->spu_clut_valid = true;
            update_highlight(priv);
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
        return STREAM_OK;
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
        if (chapter < 0 || chapter + 1 > n) {
            free(parts);
            break;
        }
        *ch = chapter > 0 ? parts[chapter - 1] / 90000.0 : 0;
        free(parts);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_TIME_LENGTH: {
        if (priv->duration) {
            *(double *)arg = (double)priv->duration / 1000.0;
            return STREAM_OK;
        }
        break;
    }
    case STREAM_CTRL_GET_ASPECT_RATIO: {
        uint8_t ar = dvdnav_get_video_aspect(dvdnav);
        *(double *)arg = !ar ? 4.0 / 3.0 : 16.0 / 9.0;
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_CURRENT_TIME: {
        double tm;
        tm = dvdnav_get_current_time(dvdnav) / 90000.0f;
        if (tm != -1) {
            *(double *)arg = tm;
            return STREAM_OK;
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
        int32_t num_titles = 0;
        dvdnav_get_number_of_titles(priv->dvdnav, &num_titles);
        // demux_disc appends a synthetic "Disc Menu" edition at the end.
        if (title == num_titles) {
            if (dvdnav_menu_call(priv->dvdnav, DVD_MENU_Root)
                != DVDNAV_STATUS_OK)
                break;
            priv->discontinuity_id++;
            stream_drop_buffers(stream);
            return STREAM_OK;
        }
        if (dvdnav_title_play(priv->dvdnav, title + 1) != DVDNAV_STATUS_OK)
            break;
        priv->discontinuity_id++;
        stream_drop_buffers(stream);
        return STREAM_OK;
    }
    case STREAM_CTRL_SEEK_TO_TIME: {
        double *args = arg;
        double d = args[0]; // absolute target timestamp
        int flags = args[1]; // from SEEK_* flags (demux.h)
        if (flags & SEEK_HR)
            d -= 10; // fudge offset; it's a hack, because fuck libdvd*
        int64_t tm = (int64_t)(d * 90000);
        if (tm < 0)
            tm = 0;
        if (priv->duration && tm >= (int64_t)priv->duration * 90)
            tm = (int64_t)priv->duration * 90 - 1;
        uint32_t pos, len;
        if (dvdnav_get_position(dvdnav, &pos, &len) != DVDNAV_STATUS_OK)
            break;
        MP_VERBOSE(stream, "seek to PTS %f (%"PRId64")\n", d, tm);
        if (dvdnav_time_search(dvdnav, tm) != DVDNAV_STATUS_OK)
            break;
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
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_ANGLE: {
        uint32_t curr, angles;
        if (dvdnav_get_angle_info(dvdnav, &curr, &angles) != DVDNAV_STATUS_OK)
            break;
        *(int *)arg = curr;
        return STREAM_OK;
    }
    case STREAM_CTRL_SET_ANGLE: {
        uint32_t curr, angles;
        int new_angle = *(int *)arg;
        if (dvdnav_get_angle_info(dvdnav, &curr, &angles) != DVDNAV_STATUS_OK)
            break;
        if (new_angle > angles || new_angle < 1)
            break;
        if (dvdnav_angle_change(dvdnav, new_angle) != DVDNAV_STATUS_OK)
            break;
        return STREAM_OK;
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
        static_assert(sizeof(uint32_t) == sizeof(unsigned int), "");
        memcpy(req->palette, priv->spu_clut, sizeof(req->palette));
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
    case STREAM_CTRL_NAV_CMD: {
        handle_nav_cmd(stream, arg);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_NAV_STATE: {
        struct stream_nav_state *st = arg;
        if (priv->src_w <= 0 || priv->src_h <= 0)
            refresh_video_resolution(priv);
        *st = (struct stream_nav_state){
            .menu_active = priv->in_menu,
            .has_popup = false,
            .src_w = priv->src_w,
            .src_h = priv->src_h,
            .hl_x = priv->btn_rect[0],
            .hl_y = priv->btn_rect[1],
            .hl_w = priv->btn_rect[2],
            .hl_h = priv->btn_rect[3],
            .change_id = priv->nav_change_id,
            .discontinuity_id = priv->discontinuity_id,
        };
        memcpy(st->hl_palette, priv->hl_palette, sizeof(st->hl_palette));
        return STREAM_OK;
    }
    }

    return STREAM_UNSUPPORTED;
}

static void stream_dvdnav_close(stream_t *s)
{
    struct priv *priv = s->priv;
    if (priv->dvdnav)
        dvdnav_close(priv->dvdnav);
    priv->dvdnav = NULL;
    if (priv->dvd_speed)
        dvd_set_speed(s, priv->filename, -1);
}

static struct priv *new_dvdnav_stream(stream_t *stream, char *filename)
{
    struct priv *priv = stream->priv;
    const char *title_str;

    if (!filename)
        return NULL;

    if (!(priv->filename = mp_get_user_path(priv, stream->global, filename)))
        return NULL;

    priv->dvd_speed = priv->opts->speed;
    dvd_set_speed(stream, priv->filename, priv->dvd_speed);

    if (dvdnav_open(&(priv->dvdnav), priv->filename) != DVDNAV_STATUS_OK)
        return NULL;

    if (!priv->dvdnav)
        return NULL;

    dvdnav_set_readahead_flag(priv->dvdnav, 1);
    if (dvdnav_set_PGC_positioning_flag(priv->dvdnav, 1) != DVDNAV_STATUS_OK)
        MP_ERR(stream, "stream_dvdnav, failed to set PGC positioning\n");
    /* report the title?! */
    dvdnav_get_title_string(priv->dvdnav, &title_str);

    return priv;
}

static int open_s_internal(stream_t *stream)
{
    struct priv *priv, *p;
    priv = p = stream->priv;
    char *filename;
    int ret = 0;

    p->opts = mp_get_config_group(stream, stream->global, &dvd_conf);

    if (p->device && p->device[0])
        filename = p->device;
    else if (p->opts->device && p->opts->device[0])
        filename = p->opts->device;
    else
        filename = DEFAULT_OPTICAL_DEVICE;
    if (!new_dvdnav_stream(stream, filename)) {
        MP_ERR(stream, "Couldn't open DVD device: %s\n",
                filename);
        ret = STREAM_ERROR;
        goto err;
    }

    int32_t num_titles = 0;
    dvdnav_get_number_of_titles(priv->dvdnav, &num_titles);

    if (p->track == TITLE_LONGEST) { // longest
        dvdnav_t *dvdnav = priv->dvdnav;
        uint64_t best_length = 0;
        int best_title = -1;
        MP_VERBOSE(stream, "List of available titles:\n");
        for (int n = 1; n <= num_titles; n++) {
            uint64_t *parts = NULL, duration = 0;
            dvdnav_describe_title_chapters(dvdnav, n, &parts, &duration);
            if (parts) {
                if (duration > best_length) {
                    best_length = duration;
                    best_title = n;
                }
                if (duration > 90000) { // arbitrarily ignore <1s titles
                    char *time = mp_format_time(duration / 90000, false);
                    MP_VERBOSE(stream, "title: %3d duration: %s\n",
                               n - 1, time);
                    talloc_free(time);
                }
                free(parts);
            }
        }
        p->track = best_title - 1;
        MP_INFO(stream, "Selecting title %d.\n", p->track);
    }

    // demux_disc.c appends a synthetic "Disc Menu" edition at index num_titles.
    if (p->track >= num_titles)
        p->track = TITLE_MENU;

    if (p->track >= 0) {
        priv->title = p->track;
        if (dvdnav_title_play(priv->dvdnav, p->track + 1) != DVDNAV_STATUS_OK) {
            MP_FATAL(stream, "dvdnav_stream, couldn't select title %d, error '%s'\n",
                   p->track, dvdnav_err_to_string(priv->dvdnav));
            ret = STREAM_UNSUPPORTED;
            goto err;
        }
    } else {
        // Menu mode: don't pre-select any title; let dvdnav start with the
        // disc's first-play / VMGM menu and drive everything via NAV events.
        priv->title = 0;
        dvdnav_menu_call(priv->dvdnav, DVD_MENU_Root);
    }
    if (p->opts->angle > 1)
        dvdnav_angle_change(priv->dvdnav, p->opts->angle);

    stream->fill_buffer = fill_buffer;
    stream->control = control;
    stream->close = stream_dvdnav_close;
    stream->demuxer = "+disc";
    stream->lavf_type = "mpeg";

    return STREAM_OK;

err:
    stream_dvdnav_close(stream);
    return ret;
}

static int open_s(stream_t *stream)
{
    struct priv *priv = talloc_zero(stream, struct priv);
    stream->priv = priv;

    bstr title, bdevice;
    bstr_split_tok(bstr0(stream->path), "/", &title, &bdevice);

    struct MPOpts *opts = mp_get_config_group(stream, stream->global, &mp_opt_root);
    int edition_id = opts->edition_id;
    bool disc_menu = opts->disc_menu;
    talloc_free(opts);

    priv->track = disc_menu ? TITLE_MENU : TITLE_LONGEST;

    if (edition_id >= 0) {
        priv->track = edition_id;
    } else if (bstr_equals0(title, "longest") || bstr_equals0(title, "first")) {
        priv->track = TITLE_LONGEST;
    } else if (bstr_equals0(title, "menu")) {
        priv->track = TITLE_MENU;
    } else if (title.len) {
        bstr rest;
        priv->track = bstrtoll(title, &rest, 10);
        if (rest.len) {
            MP_ERR(stream, "number expected: '%.*s'\n", BSTR_P(rest));
            return STREAM_ERROR;
        }
    }

    priv->device = bstrto0(priv, bdevice);

    return open_s_internal(stream);
}

const stream_info_t stream_info_dvdnav = {
    .name = "dvdnav",
    .open = open_s,
    .protocols = (const char*const[]){ "dvd", "dvdnav", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};

static bool check_ifo(const char *path)
{
    if (strcasecmp(mp_basename(path), "video_ts.ifo"))
        return false;

    return dvd_probe(path, ".ifo", "DVDVIDEO-VMG");
}

static int ifo_dvdnav_stream_open(stream_t *stream)
{
    struct priv *priv = talloc_zero(stream, struct priv);
    stream->priv = priv;

    if (!stream->access_references)
        goto unsupported;

    struct MPOpts *opts = mp_get_config_group(NULL, stream->global, &mp_opt_root);
    priv->track = opts->edition_id >= 0 ? opts->edition_id :
                  (opts->disc_menu ? TITLE_MENU : TITLE_LONGEST);
    talloc_free(opts);

    char *path = mp_file_get_path(priv, bstr0(stream->url));
    if (!path)
        goto unsupported;

    // We allow the path to point to a directory containing VIDEO_TS/, a
    // directory containing VIDEO_TS.IFO, or that file itself.
    if (!check_ifo(path)) {
        // On UNIX, just assume the filename is always uppercase.
        char *npath = mp_path_join(priv, path, "VIDEO_TS.IFO");
        if (!check_ifo(npath)) {
            npath = mp_path_join(priv, path, "VIDEO_TS/VIDEO_TS.IFO");
            if (!check_ifo(npath))
                goto unsupported;
        }
        path = npath;
    }

    priv->device = bstrto0(priv, mp_dirname(path));

    MP_INFO(stream, ".IFO detected. Redirecting to dvd://\n");
    return open_s_internal(stream);

unsupported:
    talloc_free(priv);
    stream->priv = NULL;
    return STREAM_UNSUPPORTED;
}

const stream_info_t stream_info_ifo_dvdnav = {
    .name = "ifo_dvdnav",
    .open = ifo_dvdnav_stream_open,
    .protocols = (const char*const[]){ "file", "", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};
