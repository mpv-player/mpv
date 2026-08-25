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
#include "options/m_config.h"
#include "options/path.h"
#include "osdep/threads.h"
#include "osdep/timer.h"
#include "stream.h"
#include "demux/demux.h"
#include "video/csputils.h"
#include "video/out/vo.h"

#define TITLE_MENU -1
#define TITLE_LONGEST -2

#define DVD_TIMEBASE 90000
#define DVD_TIME_TO_S(x) ((x) / (double)(DVD_TIMEBASE))
#define DVD_TIME_FROM_S(x) ((int64_t)((x) * DVD_TIMEBASE))

#define DVD_SRC_W_DEFAULT 720
#define DVD_SRC_H_DEFAULT 576

struct priv {
    dvdnav_t *dvdnav;                   // handle to libdvdnav stuff
    struct mp_log *log;                 // borrowed from the stream
    struct mp_log *lib_log;             // used by libdvdnav's log callback
    bool probing;                       // open is an .iso auto-detection probe
    char *filename;                     // path
    int64_t duration;                   // in 90 kHz PTS ticks
    int title;
    bool had_initial_vts;

    int dvd_speed;

    int track;
    char *device;

    mp_mutex lock;                      // guards the nav/menu state below

    bool still_active;                  // fill_buffer() is holding a still
    uint32_t spu_clut[16];
    bool spu_clut_valid;
    bool in_menu;
    int current_button;                 // mirror of libdvdnav HL_BTNN_REG
    struct mp_dvdnav_highlight hl;      // focused button rect + palette
    uint32_t nav_change_id;
    uint32_t discontinuity_id;          // bumped on actions that may jump
    bool drain_enabled;                 // a demuxer is attached and acks drains
    bool pending_drain;                 // hold EOF until the demuxer acks
    bool at_boundary;                   // no payload delivered since last jump
    int src_w, src_h;                   // video resolution in pixels
    int auto_actioned_button;           // last auto-activated button; 0 if none
    pci_t pci;                          // Copy of the last NAV packet's PCI.
    bool pci_valid;

    // Disc-driven audio/sub/angle state.
    int audio_physical;                 // 0..7 from DVDNAV_AUDIO_STREAM_CHANGE
    int sub_physical;                   // 0..31 from DVDNAV_SPU_STREAM_CHANGE
    bool sub_visible;                   // SPU "on" flag from same event

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

static int pci_num_buttons(const pci_t *pci)
{
    return MPMIN(pci->hli.hl_gi.btn_ns, MP_ARRAY_SIZE(pci->hli.btnit));
}

static void compute_button_rect(struct priv *priv, pci_t *pci, int btn)
{
    priv->hl.rect = (struct mp_rect){0};
    if (btn <= 0 || btn > pci_num_buttons(pci))
        return;
    // btni_t is packed and full of bitfields, memcpy to ensure correct alignment.
    btni_t b;
    memcpy(&b, &pci->hli.btnit[btn - 1], sizeof(b));
    int xs = b.x_start, xe = b.x_end;
    int ys = b.y_start, ye = b.y_end;
    priv->hl.rect = (struct mp_rect){
        .x0 = xs, .y0 = ys,
        .x1 = xe > xs ? xe : xs,
        .y1 = ye > ys ? ye : ys,
    };
}

// Resolve the 4-entry "select-state" highlight palette for the focused button.
// btn_coli holds two packed words per color set ([select, action]); the select
// word encodes [Ci3 Ci2 Ci1 Ci0 A3 A2 A1 A0]: four CLUT indices and four 4-bit
// alphas.
static void compute_highlight_palette(struct priv *priv, pci_t *pci, int btn)
{
    memset(priv->hl.palette, 0, sizeof(priv->hl.palette));
    if (!priv->spu_clut_valid || btn <= 0 || btn > pci_num_buttons(pci))
        return;
    btni_t b;
    memcpy(&b, &pci->hli.btnit[btn - 1], sizeof(b));
    if (b.btn_coln == 0)
        return;
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
        priv->hl.palette[i] = (alpha << 24) | (c[0] << 16) | (c[1] << 8) | c[2];
    }
}

static const char *dvd_domain_name(dvdnav_t *dvdnav)
{
    if (dvdnav_is_domain_fp(dvdnav) == 1)
        return "FP";
    if (dvdnav_is_domain_vmgm(dvdnav) == 1)
        return "VMGM";
    if (dvdnav_is_domain_vtsm(dvdnav) == 1)
        return "VTSM";
    if (dvdnav_is_domain_vts(dvdnav) == 1)
        return "VTS";
    return "?";
}

// Map a libdvdnav physical audio stream number (0..7) to the corresponding
// MPEG-PS substream byte that demux_lavf assigns to AVStream->id.
static int dvd_physical_audio_to_substream(struct priv *priv, int physical)
{
    if (physical < 0 || physical > 7)
        return -1;
    uint16_t fmt = dvdnav_audio_stream_format(priv->dvdnav, physical);
    switch (fmt) {
    case DVD_AUDIO_FORMAT_AC3:
        return 0x80 + physical;
    case DVD_AUDIO_FORMAT_DTS:
        return 0x88 + physical;
    case DVD_AUDIO_FORMAT_LPCM:
        return 0xa0 + physical;
    case DVD_AUDIO_FORMAT_MPEG:
    case DVD_AUDIO_FORMAT_MPEG2_EXT:
        return 0x1c0 + physical;
    default:
        return -1;
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
// Runs with priv->lock held.
static void update_highlight(struct priv *priv)
{
    int prev_btn = priv->current_button;
    bool prev_menu = priv->in_menu;
    struct mp_dvdnav_highlight prev_hl = priv->hl;

    priv->in_menu = in_menu_domain(priv->dvdnav);
    pci_t *pci = priv->in_menu && priv->pci_valid ? &priv->pci : NULL;
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

    if (!highlight_live || btn <= 0 || btn > pci_num_buttons(pci)) {
        priv->current_button = 0;
        priv->hl = (struct mp_dvdnav_highlight){0};
    } else {
        priv->current_button = btn;
        compute_button_rect(priv, pci, btn);
        compute_highlight_palette(priv, pci, btn);
    }

    if (priv->in_menu != prev_menu || priv->current_button != prev_btn ||
        memcmp(&prev_hl, &priv->hl, sizeof(prev_hl)) != 0)
    {
        priv->nav_change_id++;
    }

    // When we leave the menu, clear the auto-action latch so the next entry
    // can fire again on the same button number.
    if (!priv->in_menu)
        priv->auto_actioned_button = 0;

    // A btnit entry can request immediate activation when its button gets focus.
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

// Move the highlight to the spec-defined neighbour of the current button. We
// resolve the neighbour ourselves and use dvdnav_button_select() (rather than
// dvdnav_{upper,lower,left,right}_button_select()) so that auto-action buttons
// are activated only through update_highlight(), where we can observe it.
static void select_neighbour_button(struct priv *priv, pci_t *pci,
                                    enum stream_nav_action action)
{
    int32_t cur = 0;
    dvdnav_get_current_highlight(priv->dvdnav, &cur);
    if (cur <= 0 || cur > pci_num_buttons(pci))
        return;
    btni_t b;
    memcpy(&b, &pci->hli.btnit[cur - 1], sizeof(b));
    int target = action == STREAM_NAV_UP    ? b.up    :
                 action == STREAM_NAV_DOWN  ? b.down  :
                 action == STREAM_NAV_LEFT  ? b.left  :
                 action == STREAM_NAV_RIGHT ? b.right : 0;
    if (target > 0)
        dvdnav_button_select(priv->dvdnav, pci, target);
}

static void do_nav_cmd(stream_t *stream, struct stream_nav_cmd *cmd)
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

    if (!in_menu_domain(priv->dvdnav) || !priv->pci_valid)
        return;

    pci_t *pci = &priv->pci;
    if (pci->hli.hl_gi.hli_ss == 0 || pci->hli.hl_gi.btn_ns == 0)
        return;

    switch (cmd->action) {
    case STREAM_NAV_UP:
    case STREAM_NAV_DOWN:
    case STREAM_NAV_LEFT:
    case STREAM_NAV_RIGHT:
        select_neighbour_button(priv, pci, cmd->action);
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

// Mark a source-position jump. Runs with priv->lock held.
static void bump_discontinuity(struct priv *priv)
{
    // Back-to-back jump events with no payload between them are one jump.
    if (priv->at_boundary)
        return;
    priv->at_boundary = true;
    priv->discontinuity_id++;
    // Hold an EOF at the boundary until the demuxer flushed and acked.
    // Off while no demuxer is attached (open/probing would starve).
    if (priv->drain_enabled)
        priv->pending_drain = true;
    MP_DBG(priv, "discontinuity -> %"PRIu32" (drain=%d)\n",
           priv->discontinuity_id, priv->pending_drain);
}

static void handle_nav_cmd(stream_t *stream, struct stream_nav_cmd *cmd)
{
    struct priv *priv = stream->priv;

    mp_mutex_lock(&priv->lock);
    int prev_auto = priv->auto_actioned_button;
    do_nav_cmd(stream, cmd);
    bool activated = stream_nav_action_activates(cmd->action) ||
                     priv->auto_actioned_button != prev_auto;
    if (priv->still_active && activated)
        bump_discontinuity(priv);
    mp_mutex_unlock(&priv->lock);
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

// Handle one event from dvdnav_get_next_block(). Returns the value that
// fill_buffer() should return, or -1 to continue reading..
static int process_event(stream_t *s, int event, void *buf, int len)
{
    struct priv *priv = s->priv;
    dvdnav_t *dvdnav = priv->dvdnav;

    switch (event) {
    case DVDNAV_BLOCK_OK:
        // Real data is flowing again: we are no longer holding a still, and
        // the last jump boundary (if any) has been crossed.
        priv->still_active = false;
        priv->at_boundary = false;
        return len;
    case DVDNAV_STOP:
        // End of disc: a real EOF, not a held still.
        priv->still_active = false;
        return 0;
    case DVDNAV_NAV_PACKET: {
        pci_t *pnavpci = dvdnav_get_current_nav_pci(dvdnav);
        priv->pci = *pnavpci;
        priv->pci_valid = true;
        MP_TRACE(s, "start pts = %"PRIu32"\n", pnavpci->pci_gi.vobu_s_ptm);
        // Reset selection to first button if the current selection is out of
        // range. libdvdnav holds stale button selections when the menu changes,
        // which makes the new page unusable.
        if (in_menu_domain(dvdnav) && pnavpci->hli.hl_gi.hli_ss &&
            pnavpci->hli.hl_gi.btn_ns > 0)
        {
            int32_t btn = 0;
            dvdnav_get_current_highlight(dvdnav, &btn);
            if (btn <= 0 || btn > pci_num_buttons(pnavpci)) {
                MP_VERBOSE(s, "stale button %d for a %d-button menu, "
                           "selecting button 1\n", btn,
                           pci_num_buttons(pnavpci));
                dvdnav_button_select(dvdnav, pnavpci, 1);
            }
        }
        // Each NAV packet can change the highlighted button or the
        // available button set; keep our mirrored state in sync.
        update_highlight(priv);
        break;
    }
    case DVDNAV_STILL_FRAME: {
        dvdnav_still_event_t *still = buf;
        // We only honor indefinite (0xff) stills. Finite stills (studio
        // logos / warnings shown for a few seconds before the menu) are
        // not hold on screen. This avoids complexities with correctly
        // timing the still frames, and there is little use-case for holding
        // 10+ seconds on single still frame.
        if (still->length != 0xFF) {
            MP_VERBOSE(s, "skipping finite still (%d s)\n",
                       still->length);
            dvdnav_still_skip(dvdnav);
            break;
        }
        // Indefinite still: report EOF to the demuxer so the video decoder
        // is drained and the last frame is actually pushed to screen.
        if (!priv->still_active)
            MP_VERBOSE(s, "indefinite still -> EOF, hold last frame\n");
        priv->still_active = true;
        return 0;
    }
    case DVDNAV_WAIT:
        dvdnav_wait_skip(dvdnav);
        break;
    case DVDNAV_HOP_CHANNEL:
        MP_VERBOSE(s, "hop channel (domain=%s)\n", dvd_domain_name(dvdnav));
        bump_discontinuity(priv);
        break;
    case DVDNAV_HIGHLIGHT:
        update_highlight(priv);
        break;
    case DVDNAV_VTS_CHANGE: {
        int tit = 0, part = 0;
        dvdnav_vts_change_event_t *vts_event =
            (dvdnav_vts_change_event_t *)buf;
        MP_VERBOSE(s, "switched to VTS: %d (old=%d) domain=%s\n",
                   vts_event->new_vtsN, vts_event->old_vtsN,
                   dvd_domain_name(dvdnav));
        if (!priv->had_initial_vts) {
            // dvdnav sends an initial VTS change before any data; don't
            // cause a blocking wait for the player, because the player in
            // turn can't initialize the demuxer without data.
            priv->had_initial_vts = true;
            break;
        }
        if (dvdnav_current_title_info(dvdnav, &tit, &part) == DVDNAV_STATUS_OK)
        {
            MP_VERBOSE(s, "new title %d\n", tit);
            if (priv->title > 0 && tit != priv->title)
                MP_WARN(s, "Requested title not found\n");
        }
        // Resolution can change across VTS (PAL vs. NTSC titles); refresh
        // so mouse coordinate translation stays correct.
        refresh_video_resolution(priv);
        // VTS change is a title-set boundary, flush.
        bump_discontinuity(priv);
        break;
    }
    case DVDNAV_CELL_CHANGE: {
        dvdnav_cell_change_event_t *ev =  (dvdnav_cell_change_event_t *)buf;

        if (ev->pgc_length)
            priv->duration = ev->pgc_length;

        MP_VERBOSE(s, "cell change: cell=%d pg=%d pgc_len=%.3f "
                   "cur_time=%.3f domain=%s\n",
                   ev->cellN, ev->pgN, DVD_TIME_TO_S(ev->pgc_length),
                   DVD_TIME_TO_S(dvdnav_get_current_time(dvdnav)),
                   dvd_domain_name(dvdnav));
        break;
    }
    case DVDNAV_SPU_CLUT_CHANGE: {
        memcpy(priv->spu_clut, buf, 16 * sizeof(uint32_t));
        priv->spu_clut_valid = true;
        update_highlight(priv);
        break;
    }
    case DVDNAV_AUDIO_STREAM_CHANGE: {
        dvdnav_audio_stream_change_event_t *ev = buf;
        // physical: 0..7 = active audio stream, -1 = SPU/audio off.
        MP_VERBOSE(s, "audio change phys=%d log=%d domain=%s\n",
                   ev->physical, ev->logical, dvd_domain_name(dvdnav));
        if (priv->audio_physical != ev->physical) {
            priv->audio_physical = ev->physical;
            priv->nav_change_id++;
        }
        break;
    }
    case DVDNAV_SPU_STREAM_CHANGE: {
        dvdnav_spu_stream_change_event_t *ev = buf;
        int raw = ev->physical_wide;
        bool visible = raw >= 0 && !(raw & 0x80);
        int phys = raw >= 0 ? (raw & 0x1F) : -1;
        MP_VERBOSE(s, "sub change phys_wide=0x%x lb=0x%x ps=0x%x log=%d domain=%s\n",
                   ev->physical_wide, ev->physical_letterbox,
                   ev->physical_pan_scan, ev->logical, dvd_domain_name(dvdnav));
        if (priv->sub_physical != phys || priv->sub_visible != visible) {
            priv->sub_physical = phys;
            priv->sub_visible = visible;
            priv->nav_change_id++;
        }
        break;
    }
    }
    return -1;
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
        mp_mutex_lock(&priv->lock);
        bool drain = priv->pending_drain;
        mp_mutex_unlock(&priv->lock);
        if (drain) {
            MP_DBG(s, "holding drain EOF (jump boundary)\n");
            return 0;
        }

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
            MP_TRACE(s, "event %s (%d)\n", name, event);
        }

        mp_mutex_lock(&priv->lock);
        int r = process_event(s, event, buf, len);
        mp_mutex_unlock(&priv->lock);
        if (r >= 0)
            return r;
    }
    return 0;
}

static int64_t seek_landing_ticks(struct priv *priv, double d, double margin)
{
    int64_t tm = DVD_TIME_FROM_S(d - margin);
    if (tm < 0)
        tm = 0;
    if (priv->duration > 0 && tm >= priv->duration)
        tm = priv->duration - 1;
    return tm;
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
        *ch = chapter > 0 ? DVD_TIME_TO_S(parts[chapter - 1]) : 0;
        free(parts);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_TIME_LENGTH: {
        if (priv->duration > 0) {
            *(double *)arg = DVD_TIME_TO_S(priv->duration);
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
        int64_t tm = dvdnav_get_current_time(dvdnav);
        if (tm != -1) {
            *(double *)arg = DVD_TIME_TO_S(tm);
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
        *(double *)arg = DVD_TIME_TO_S(duration);
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
        if (dvdnav_get_number_of_titles(priv->dvdnav, &num_titles) != DVDNAV_STATUS_OK)
            break;
        // demux_disc appends a synthetic "Disc Menu" edition at the end.
        dvdnav_status_t status;
        if (title == num_titles) {
            status = dvdnav_menu_call(priv->dvdnav, DVD_MENU_Root);
        } else {
            status = dvdnav_title_play(priv->dvdnav, title + 1);
        }
        if (status != DVDNAV_STATUS_OK)
            break;
        // This may run on the player thread; buffered data is dropped on the
        // demuxer thread when the discontinuity is processed.
        mp_mutex_lock(&priv->lock);
        bump_discontinuity(priv);
        mp_mutex_unlock(&priv->lock);
        return STREAM_OK;
    }
    case STREAM_CTRL_SEEK_TO_TIME: {
        double *args = arg;
        double d = args[0]; // absolute target timestamp
        int flags = args[1]; // from SEEK_* flags (demux.h)
        if (in_menu_domain(dvdnav)) {
            // The menu subpicture (buttons, highlight) is a one-shot packet
            // at its cell start, and dvdnav's time search lands mid-cell,
            // past it, so the menu would show without buttons until its next
            // loop. Map seeks to program skips, every landing is a program
            // start where the display state is established.
            double cur = DVD_TIME_TO_S(dvdnav_get_current_time(dvdnav));
            MP_VERBOSE(stream, "menu seek to %f (cur %f) -> %s program\n",
                       d, cur, d >= cur ? "next" : "prev");
            dvdnav_status_t r = d >= cur ? dvdnav_next_pg_search(dvdnav)
                                         : dvdnav_prev_pg_search(dvdnav);
            if (r != DVDNAV_STATUS_OK)
                break;
        } else {
            uint32_t pos, len;
            if (dvdnav_get_position(dvdnav, &pos, &len) != DVDNAV_STATUS_OK)
                break;
            // hr-seeks decode from the landing up to the exact target, so
            // the landing must not overshoot it. Time-map landings are
            // accurate to one map entry, the time_search fallback
            // interpolates from cell durations and can be off by several
            // seconds on VBR content.
            int64_t tm = seek_landing_ticks(priv, d, flags & SEEK_HR ? 2 : 0);
            MP_VERBOSE(stream, "seek to PTS %f (%"PRId64")\n", d, tm);
            // The disc's time maps give accurate landings. Fall back to
            // dvdnav's cell-duration interpolation for titles that lack
            // them.
            bool jumped = false;
#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(7, 0, 0)
            jumped = dvdnav_jump_to_sector_by_time(dvdnav, tm, 0) == DVDNAV_STATUS_OK;
#endif
            if (!jumped) {
                tm = seek_landing_ticks(priv, d, flags & SEEK_HR ? 10 : 0);
                if (dvdnav_time_search(dvdnav, tm) != DVDNAV_STATUS_OK)
                    break;
            }
        }
        // The seek reinitializes everything itself. Clear any held boundary
        // and coalesce the HOP_CHANNEL libdvdnav emits for the seek.
        mp_mutex_lock(&priv->lock);
        priv->pending_drain = false;
        priv->at_boundary = true;
        mp_mutex_unlock(&priv->lock);
        stream_drop_buffers(stream);
        d = DVD_TIME_TO_S(dvdnav_get_current_time(dvdnav));
        MP_VERBOSE(stream, "landed at: %f\n", d);
        uint32_t pos, len;
        if (dvdnav_get_position(dvdnav, &pos, &len) == DVDNAV_STATUS_OK)
            MP_VERBOSE(stream, "block: %" PRIu32 "\n", pos);
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
        mp_mutex_lock(&priv->lock);
        memcpy(req->palette, priv->spu_clut, sizeof(req->palette));
        mp_mutex_unlock(&priv->lock);
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
    case STREAM_CTRL_NAV_DRAIN_ENABLE: {
        mp_mutex_lock(&priv->lock);
        priv->drain_enabled = true;
        priv->pending_drain = false;
        priv->at_boundary = false;
        mp_mutex_unlock(&priv->lock);
        MP_DBG(stream, "jump boundary drain enabled\n");
        return STREAM_OK;
    }
    case STREAM_CTRL_NAV_DRAIN_ACK: {
        mp_mutex_lock(&priv->lock);
        MP_DBG(stream, "jump boundary drain acked (was %d)\n",
               priv->pending_drain);
        priv->pending_drain = false;
        mp_mutex_unlock(&priv->lock);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_NAV_STATE: {
        struct stream_nav_state *st = arg;
        uint32_t cur_angle = 0, num_angles = 0;
        dvdnav_get_angle_info(dvdnav, &cur_angle, &num_angles);
        // The current PGC provably has no audio when no physical stream maps
        // to a logical one (e.g. silent menus).
        bool no_audio = true;
        for (int n = 0; n < 8 && no_audio; n++)
            no_audio = dvdnav_get_audio_logical_stream(dvdnav, n) == -1;
        mp_mutex_lock(&priv->lock);
        if (priv->src_w <= 0 || priv->src_h <= 0)
            refresh_video_resolution(priv);
        *st = (struct stream_nav_state){
            .nav_active = true,
            .no_audio = no_audio,
            .menu_active = priv->in_menu,
            .still_active = priv->still_active,
            .src_w = priv->src_w,
            .src_h = priv->src_h,
            .hl = priv->hl,
            .change_id = priv->nav_change_id,
            .discontinuity_id = priv->discontinuity_id,
            .drain_pending = priv->pending_drain,
            .active_audio_id = dvd_physical_audio_to_substream(priv, priv->audio_physical),
            .active_sub_id = priv->sub_physical >= 0 ? 0x20 + priv->sub_physical : -1,
            .sub_visible = priv->sub_visible,
            .angle = cur_angle,
            .num_angles = num_angles,
        };
        mp_mutex_unlock(&priv->lock);
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
    mp_mutex_destroy(&priv->lock);
}

static void dvdnav_log(void *p, dvdnav_logger_level_t level,
                       const char *fmt, va_list va)
{
    struct priv *priv = p;
    int lvl;
    switch (level) {
    case DVDNAV_LOGGER_LEVEL_ERROR: lvl = MSGL_ERR;   break;
    case DVDNAV_LOGGER_LEVEL_WARN:  lvl = MSGL_WARN;  break;
    case DVDNAV_LOGGER_LEVEL_DEBUG: lvl = MSGL_DEBUG; break;
    case DVDNAV_LOGGER_LEVEL_INFO:
    default:                        lvl = MSGL_V;     break;
    }
    if (priv->probing)
        lvl = MPMAX(lvl, MSGL_V);
    if (!mp_msg_test(priv->lib_log, lvl))
        return;
    mp_msg_va(priv->lib_log, lvl, fmt, va);
    mp_msg(priv->lib_log, lvl, "\n");
}

static dvdnav_status_t nav_open(stream_t *stream, dvdnav_t **dest, const char *path)
{
    struct priv *priv = stream->priv;
    priv->lib_log = mp_log_new(stream, stream->log, "/libdvdnav");
    const dvdnav_logger_cb logger_cb = { .pf_log = dvdnav_log };
    return dvdnav_open2(dest, priv, &logger_cb, path);
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

    if (nav_open(stream, &priv->dvdnav, priv->filename) != DVDNAV_STATUS_OK)
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

    mp_mutex_init(&priv->lock);

    priv->log = stream->log;
    priv->audio_physical = -1;
    priv->sub_physical = -1;
    priv->sub_visible = false;

    p->opts = mp_get_config_group(stream, stream->global, &dvd_conf);

    if (p->device && p->device[0])
        filename = p->device;
    else if (p->opts->device && p->opts->device[0])
        filename = p->opts->device;
    else
        filename = DEFAULT_OPTICAL_DEVICE;
    if (!new_dvdnav_stream(stream, filename)) {
        if (!priv->probing)
            MP_ERR(stream, "Couldn't open DVD device: %s\n", filename);
        ret = priv->probing ? STREAM_UNSUPPORTED : STREAM_ERROR;
        goto err;
    }
    priv->probing = false;

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
            MP_FATAL(stream, "couldn't select title %d, error '%s'\n",
                   p->track, dvdnav_err_to_string(priv->dvdnav));
            ret = STREAM_UNSUPPORTED;
            goto err;
        }
    } else {
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

    // Hand the .iso to libdvdnav as the device. Opening validates the VMG
    // IFO, so it doubles as the probe (see priv->probing).
    if (bstr_case_endswith(bstr0(path), bstr0(".iso"))) {
        priv->device = talloc_strdup(priv, path);
        priv->probing = stream->autoprobed;
        int r = open_s_internal(stream);
        if (r != STREAM_OK) {
            if (priv->probing)
                goto unsupported;
            return r;
        }
        MP_INFO(stream, "DVD ISO image detected. Redirecting to dvd://\n");
        return r;
    }

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
