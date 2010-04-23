/*
 * OS/2 video output driver
 *
 * Copyright (c) 2007-2009 by KO Myung-Hun (komh@chollian.net)
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define INCL_WIN
#define INCL_GPI
#define INCL_DOS
#include <os2.h>

#include <mmioos2.h>
#include <fourcc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include <kva.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"

#include "fastmemcpy.h"
#include "mp_fifo.h"
#include "osdep/keycodes.h"
#include "input/input.h"
#include "input/mouse.h"
#include "subopt-helper.h"
#include "sub.h"

#include "cpudetect.h"
#include "libswscale/swscale.h"
#include "libmpcodecs/vf_scale.h"

static const vo_info_t info = {
    "SNAP/WarpOverlay!/DIVE video output",
    "kva",
    "KO Myung-Hun <komh@chollian.net>",
    ""
};

const LIBVO_EXTERN(kva)

#define WC_MPLAYER  "WC_MPLAYER"

#define SRC_WIDTH   m_int.kvas.szlSrcSize.cx
#define SRC_HEIGHT  m_int.kvas.szlSrcSize.cy

#define HWNDFROMWINID(wid)    ((wid) + 0x80000000UL)

static const struct mp_keymap m_vk_map[] = {
    {VK_NEWLINE, KEY_ENTER}, {VK_TAB, KEY_TAB}, {VK_SPACE, ' '},

    // control keys
    {VK_CTRL,   KEY_CTRL},    {VK_BACKSPACE, KEY_BS},
    {VK_DELETE, KEY_DELETE},  {VK_INSERT,    KEY_INSERT},
    {VK_HOME,   KEY_HOME},    {VK_END,       KEY_END},
    {VK_PAGEUP, KEY_PAGE_UP}, {VK_PAGEDOWN,  KEY_PAGE_DOWN},
    {VK_ESC,    KEY_ESC},

    // cursor keys
    {VK_RIGHT, KEY_RIGHT}, {VK_LEFT, KEY_LEFT},
    {VK_DOWN,  KEY_DOWN},  {VK_UP,   KEY_UP},

    // function keys
    {VK_F1, KEY_F+1}, {VK_F2,  KEY_F+2},  {VK_F3,  KEY_F+3},  {VK_F4,  KEY_F+4},
    {VK_F5, KEY_F+5}, {VK_F6,  KEY_F+6},  {VK_F7,  KEY_F+7},  {VK_F8,  KEY_F+8},
    {VK_F9, KEY_F+9}, {VK_F10, KEY_F+10}, {VK_F11, KEY_F+11}, {VK_F12, KEY_F+12},

    {0, 0}
};

static const struct mp_keymap m_keypad_map[] = {
    // keypad keys
    {0x52, KEY_KP0}, {0x4F, KEY_KP1}, {0x50, KEY_KP2},   {0x51, KEY_KP3},
    {0x4B, KEY_KP4}, {0x4C, KEY_KP5}, {0x4D, KEY_KP6},   {0x47, KEY_KP7},
    {0x48, KEY_KP8}, {0x49, KEY_KP9}, {0x53, KEY_KPDEC}, {0x5A, KEY_KPENTER},

    {0, 0}
};

static const struct mp_keymap m_mouse_map[] = {
    {WM_BUTTON1DOWN,   MOUSE_BTN0},
    {WM_BUTTON3DOWN,   MOUSE_BTN1},
    {WM_BUTTON2DOWN,   MOUSE_BTN2},
    {WM_BUTTON1DBLCLK, MOUSE_BTN0_DBL},
    {WM_BUTTON3DBLCLK, MOUSE_BTN1_DBL},
    {WM_BUTTON2DBLCLK, MOUSE_BTN2_DBL},

    {0, 0}
};

struct {
    HAB         hab;
    HMQ         hmq;
    HWND        hwndFrame;
    HWND        hwndClient;
    HWND        hwndSysMenu;
    HWND        hwndTitleBar;
    HWND        hwndMinMax;
    FOURCC      fcc;
    int         iImageFormat;
    int         nChromaShift;
    KVASETUP    kvas;
    KVACAPS     kvac;
    RECTL       rclDst;
    int         bpp;
    LONG        lStride;
    PBYTE       pbImage;
    BOOL        fFixT23;
    PFNWP       pfnwpOldFrame;
    uint8_t    *planes[MP_MAX_PLANES];     // y = 0, u = 1, v = 2
    int         stride[MP_MAX_PLANES];
    BOOL        fHWAccel;
    RECTL       rclParent;
    struct SwsContext *sws;
} m_int;

static inline void setAspectRatio(ULONG ulRatio)
{
    m_int.kvas.ulRatio = ulRatio;
    kvaSetup(&m_int.kvas);
}

static int query_format_info(int format, PBOOL pfHWAccel, PFOURCC pfcc,
                             int *pbpp, int *pnChromaShift)
{
    BOOL    fHWAccel;
    FOURCC  fcc;
    INT     bpp;
    INT     nChromaShift;

    switch (format) {
    case IMGFMT_YV12:
        fHWAccel        = m_int.kvac.ulInputFormatFlags & KVAF_YV12;
        fcc             = FOURCC_YV12;
        bpp             = 1;
        nChromaShift    = 1;
        break;

    case IMGFMT_YUY2:
        fHWAccel        = m_int.kvac.ulInputFormatFlags & KVAF_YUY2;
        fcc             = FOURCC_Y422;
        bpp             = 2;
        nChromaShift    = 0;
        break;

    case IMGFMT_YVU9:
        fHWAccel        = m_int.kvac.ulInputFormatFlags & KVAF_YVU9;
        fcc             = FOURCC_YVU9;
        bpp             = 1;
        nChromaShift    = 2;
        break;

    case IMGFMT_BGR24:
        fHWAccel        = m_int.kvac.ulInputFormatFlags & KVAF_BGR24;
        fcc             = FOURCC_BGR3;
        bpp             = 3;
        nChromaShift    = 0;
        break;

    case IMGFMT_BGR16:
        fHWAccel        = m_int.kvac.ulInputFormatFlags & KVAF_BGR16;
        fcc             = FOURCC_R565;
        bpp             = 2;
        nChromaShift    = 0;
        break;

    case IMGFMT_BGR15:
        fHWAccel        = m_int.kvac.ulInputFormatFlags & KVAF_BGR15;
        fcc             = FOURCC_R555;
        bpp             = 2;
        nChromaShift    = 0;
        break;

    default:
        return 1;
    }

    if (pfHWAccel)
        *pfHWAccel = fHWAccel;

    if (pfcc)
        *pfcc = fcc;

    if (pbpp)
        *pbpp = bpp;

    if (pnChromaShift)
        *pnChromaShift = nChromaShift;

    return 0;
}

static void imgCreate(void)
{
    int size = SRC_HEIGHT * m_int.lStride;;

    switch (m_int.iImageFormat) {
    case IMGFMT_YV12:
        size += size / 2;
        break;

    case IMGFMT_YVU9:
        size += size / 8;
        break;
    }

    m_int.pbImage = malloc(size);

    memset(m_int.planes, 0, sizeof(m_int.planes));
    memset(m_int.stride, 0, sizeof(m_int.stride));
    m_int.planes[0] = m_int.pbImage;
    m_int.stride[0] = m_int.lStride;

    // YV12 or YVU9 ?
    if (m_int.nChromaShift) {
        m_int.planes[1] = m_int.planes[0] + SRC_HEIGHT * m_int.stride[0];
        m_int.stride[1] = m_int.stride[0] >> m_int.nChromaShift;

        m_int.planes[2] = m_int.planes[1] +
                          (SRC_HEIGHT >> m_int.nChromaShift) * m_int.stride[1];
        m_int.stride[2] = m_int.stride[1];
    }
}

static void imgFree(void)
{
    free(m_int.pbImage);

    m_int.pbImage = NULL;
}

static void imgDisplay(void)
{
    PVOID pBuffer;
    ULONG ulBPL;

    if (!kvaLockBuffer(&pBuffer, &ulBPL)) {
        uint8_t *dst[MP_MAX_PLANES] = {NULL};
        int      dstStride[MP_MAX_PLANES] = {0};

        // Get packed or Y
        dst[0]       = pBuffer;
        dstStride[0] = ulBPL;

        // YV12 or YVU9 ?
        if (m_int.nChromaShift) {
            // Get V
            dst[2]       = dst[0] + SRC_HEIGHT * dstStride[0];
            dstStride[2] = dstStride[0] >> m_int.nChromaShift;

            // Get U
            dst[1]       = dst[2] +
                           (SRC_HEIGHT >> m_int.nChromaShift ) * dstStride[2];
            dstStride[1] = dstStride[2];
        }

        if (m_int.fHWAccel) {
            int w, h;

            w = m_int.stride[0];
            h = SRC_HEIGHT;

            // Copy packed or Y
            mem2agpcpy_pic(dst[0], m_int.planes[0], w, h,
                           dstStride[0], m_int.stride[0]);

            // YV12 or YVU9 ?
            if (m_int.nChromaShift) {
                w >>= m_int.nChromaShift; h >>= m_int.nChromaShift;

                // Copy U
                mem2agpcpy_pic(dst[1], m_int.planes[1], w, h,
                               dstStride[1], m_int.stride[1]);

                // Copy V
                mem2agpcpy_pic(dst[2], m_int.planes[2], w, h,
                               dstStride[2], m_int.stride[2]);
            }
        } else {
            sws_scale(m_int.sws, m_int.planes, m_int.stride, 0, SRC_HEIGHT,
                      dst, dstStride);
        }

        kvaUnlockBuffer();
    }
}

// Frame window procedure to work around T23 laptop with S3 video card,
// which supports upscaling only.
static MRESULT EXPENTRY NewFrameWndProc(HWND hwnd, ULONG msg, MPARAM mp1,
                                        MPARAM mp2)
{
    switch (msg) {
    case WM_QUERYTRACKINFO:
        {
        PTRACKINFO  pti = (PTRACKINFO)mp2;
        RECTL       rcl;

        if (vo_fs)
            break;

        m_int.pfnwpOldFrame(hwnd, msg, mp1, mp2);

        rcl.xLeft   = 0;
        rcl.yBottom = 0;
        rcl.xRight  = SRC_WIDTH  + 1;
        rcl.yTop    = SRC_HEIGHT + 1;

        WinCalcFrameRect(hwnd, &rcl, FALSE);

        pti->ptlMinTrackSize.x = rcl.xRight - rcl.xLeft;
        pti->ptlMinTrackSize.y = rcl.yTop   - rcl.yBottom;

        pti->ptlMaxTrackSize.x = vo_screenwidth;
        pti->ptlMaxTrackSize.y = vo_screenheight;

        return (MRESULT)TRUE;
        }

    case WM_ADJUSTWINDOWPOS:
        {
        PSWP    pswp = (PSWP)mp1;
        RECTL   rcl;

        if (vo_fs)
            break;

        if (pswp->fl & SWP_SIZE) {
            rcl.xLeft   = pswp->x;
            rcl.yBottom = pswp->y;
            rcl.xRight  = rcl.xLeft   + pswp->cx;
            rcl.yTop    = rcl.yBottom + pswp->cy;

            WinCalcFrameRect(hwnd, &rcl, TRUE);

            if (rcl.xRight - rcl.xLeft <= SRC_WIDTH)
                rcl.xRight = rcl.xLeft + (SRC_WIDTH + 1);

            if (rcl.yTop - rcl.yBottom <= SRC_HEIGHT)
                rcl.yTop = rcl.yBottom + (SRC_HEIGHT + 1);

            WinCalcFrameRect(hwnd, &rcl, FALSE);

            if (rcl.xRight - rcl.xLeft > vo_screenwidth) {
                rcl.xLeft  = 0;
                rcl.xRight = vo_screenwidth;
            }

            if (rcl.yTop - rcl.yBottom > vo_screenheight) {
                rcl.yBottom = 0;
                rcl.yTop    = vo_screenheight;
            }

            pswp->fl |= SWP_MOVE;
            pswp->x   = rcl.xLeft;
            pswp->y   = rcl.yBottom;
            pswp->cx  = rcl.xRight - rcl.xLeft;
            pswp->cy  = rcl.yTop   - rcl.yBottom;
        }
        break;
        }
    }

    return m_int.pfnwpOldFrame(hwnd, msg, mp1, mp2);
}

static MRESULT EXPENTRY WndProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    // if slave mode, ignore mouse events and deliver them to a parent window
    if (WinID != -1 &&
        ((msg >= WM_MOUSEFIRST    && msg <= WM_MOUSELAST) ||
         (msg >= WM_EXTMOUSEFIRST && msg <= WM_EXTMOUSELAST))) {
        WinPostMsg(HWNDFROMWINID(WinID), msg, mp1, mp2);

        return (MRESULT)TRUE;
    }

    switch (msg) {
    case WM_CLOSE:
        mplayer_put_key(KEY_CLOSE_WIN);

        return 0;

    case WM_CHAR:
        {
        USHORT fsFlags = SHORT1FROMMP(mp1);
        UCHAR  uchScan =  CHAR4FROMMP(mp1);
        USHORT usCh    = SHORT1FROMMP(mp2);
        USHORT usVk    = SHORT2FROMMP(mp2);
        int    mpkey;

        if (fsFlags & KC_KEYUP)
            break;

        if (fsFlags & KC_SCANCODE) {
            mpkey = lookup_keymap_table(m_keypad_map, uchScan);
            if (mpkey) {
                // distinguish KEY_KP0 and KEY_KPINS
                if (mpkey == KEY_KP0 && usCh != '0')
                    mpkey = KEY_KPINS;

                // distinguish KEY_KPDEC and KEY_KPDEL
                if (mpkey == KEY_KPDEC && usCh != '.')
                    mpkey = KEY_KPDEL;

                mplayer_put_key(mpkey);

                return (MRESULT)TRUE;
            }
        }

        if (fsFlags & KC_VIRTUALKEY) {
            mpkey = lookup_keymap_table(m_vk_map, usVk);
            if (mpkey) {
                mplayer_put_key(mpkey);

                return (MRESULT)TRUE;
            }
        }

        if ((fsFlags & KC_CHAR) && !HIBYTE(usCh))
            mplayer_put_key(usCh);

        return (MRESULT)TRUE;
        }

    case WM_BUTTON1DOWN:
    case WM_BUTTON3DOWN:
    case WM_BUTTON2DOWN:
    case WM_BUTTON1DBLCLK:
    case WM_BUTTON3DBLCLK:
    case WM_BUTTON2DBLCLK:
        if (WinQueryFocus(HWND_DESKTOP) != hwnd)
            WinSetFocus(HWND_DESKTOP, hwnd);
        else if (!vo_nomouse_input)
            mplayer_put_key(lookup_keymap_table(m_mouse_map, msg));

        return (MRESULT)TRUE;

    case WM_PAINT:
        {
        HPS     hps;
        RECTL   rcl, rclDst;
        PRECTL  prcl = NULL;
        HRGN    hrgn, hrgnDst;
        RGNRECT rgnCtl;

        // get a current movie area
        kvaAdjustDstRect(&m_int.kvas.rclSrcRect, &rclDst);

        // get a current invalidated area
        hps = WinBeginPaint(hwnd, NULLHANDLE, &rcl);

        // create a region for an invalidated area
        hrgn    = GpiCreateRegion(hps, 1, &rcl);
        // create a region for a movie area
        hrgnDst = GpiCreateRegion(hps, 1, &rclDst);

        // exclude a movie area from an invalidated area
        GpiCombineRegion(hps, hrgn, hrgn, hrgnDst, CRGN_DIFF);

        // get rectangles from the region
        rgnCtl.ircStart     = 1;
        rgnCtl.ulDirection  = RECTDIR_LFRT_TOPBOT;
        GpiQueryRegionRects(hps, hrgn, NULL, &rgnCtl, NULL);

        if (rgnCtl.crcReturned > 0) {
            rgnCtl.crc = rgnCtl.crcReturned;
            prcl       = malloc(sizeof(RECTL) * rgnCtl.crcReturned);
        }

        // draw black bar if needed
        if (prcl && GpiQueryRegionRects(hps, hrgn, NULL, &rgnCtl, prcl)) {
            int i;

            for (i = 0; i < rgnCtl.crcReturned; i++)
                WinFillRect(hps, &prcl[i], CLR_BLACK);
        }

        free(prcl);

        GpiDestroyRegion(hps, hrgnDst);
        GpiDestroyRegion(hps, hrgn);

        WinEndPaint(hps);

        return 0;
        }
    }

    return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

// Change process type from VIO to PM to use PM APIs.
static void morphToPM(void)
{
    PPIB pib;

    DosGetInfoBlocks(NULL, &pib);

    // Change flag from VIO to PM:
    if (pib->pib_ultype == 2)
        pib->pib_ultype = 3;
}

static int preinit(const char *arg)
{
    HWND    hwndParent;
    ULONG   flFrameFlags;
    ULONG   kvaMode = 0;

    int     fUseSnap = 0;
    int     fUseWO   = 0;
    int     fUseDive = 0;
    int     fFixT23  = 0;

    const opt_t subopts[] = {
        {"snap", OPT_ARG_BOOL, &fUseSnap, NULL},
        {"wo",   OPT_ARG_BOOL, &fUseWO,   NULL},
        {"dive", OPT_ARG_BOOL, &fUseDive, NULL},
        {"t23",  OPT_ARG_BOOL, &fFixT23,  NULL},
        {NULL,              0, NULL,      NULL}
    };

    PCSZ pcszVideoModeStr[3] = {"DIVE", "WarpOverlay!", "SNAP"};

    if (subopt_parse(arg, subopts) != 0)
        return -1;

    morphToPM();

    memset(&m_int, 0, sizeof(m_int));

    m_int.hab = WinInitialize(0);
    m_int.hmq = WinCreateMsgQueue(m_int.hab, 0);

    WinRegisterClass(m_int.hab,
                     WC_MPLAYER,
                     WndProc,
                     CS_SIZEREDRAW | CS_MOVENOTIFY,
                     sizeof(PVOID));

    if (WinID == -1) {
        hwndParent   = HWND_DESKTOP;
        flFrameFlags = FCF_SYSMENU    | FCF_TITLEBAR | FCF_MINMAX |
                       FCF_SIZEBORDER | FCF_TASKLIST;
    } else {
        hwndParent   = HWNDFROMWINID(WinID);
        flFrameFlags = 0;
    }

    m_int.hwndFrame =
        WinCreateStdWindow(hwndParent,          // parent window handle
                           WS_VISIBLE,          // frame window style
                           &flFrameFlags,       // window style
                           WC_MPLAYER,          // class name
                           "",                  // window title
                           0L,                  // default client style
                           NULLHANDLE,          // resource in exe file
                           1,                   // frame window id
                           &m_int.hwndClient);  // client window handle

    if (m_int.hwndFrame == NULLHANDLE)
        return -1;

    m_int.hwndSysMenu  = WinWindowFromID(m_int.hwndFrame, FID_SYSMENU);
    m_int.hwndTitleBar = WinWindowFromID(m_int.hwndFrame, FID_TITLEBAR);
    m_int.hwndMinMax   = WinWindowFromID(m_int.hwndFrame, FID_MINMAX);

    m_int.fFixT23 = fFixT23;

    if (m_int.fFixT23)
        m_int.pfnwpOldFrame = WinSubclassWindow(m_int.hwndFrame,
                                                NewFrameWndProc);

    if (!!fUseSnap + !!fUseWO + !!fUseDive > 1)
        mp_msg(MSGT_VO, MSGL_WARN,"KVA: Multiple mode specified!!!\n");

    if (fUseSnap)
        kvaMode = KVAM_SNAP;
    else if (fUseWO)
        kvaMode = KVAM_WO;
    else if (fUseDive)
        kvaMode = KVAM_DIVE;
    else
        kvaMode = KVAM_AUTO;

    if (kvaInit(kvaMode, m_int.hwndClient, vo_colorkey)) {
        mp_msg(MSGT_VO, MSGL_ERR, "KVA: Init failed!!!\n");

        return -1;
    }

    kvaCaps(&m_int.kvac);

    mp_msg(MSGT_VO, MSGL_V, "KVA: Selected video mode = %s\n",
           pcszVideoModeStr[m_int.kvac.ulMode - 1]);

    kvaDisableScreenSaver();

    // Might cause PM DLLs to be loaded which incorrectly enable SIG_FPE,
    // so mask off all floating-point exceptions.
    _control87(MCW_EM, MCW_EM);

    return 0;
}

static void uninit(void)
{
    kvaEnableScreenSaver();

    imgFree();

    sws_freeContext(m_int.sws);

    if (m_int.hwndFrame != NULLHANDLE) {
        kvaResetAttr();
        kvaDone();

        if (m_int.fFixT23)
            WinSubclassWindow(m_int.hwndFrame, m_int.pfnwpOldFrame);

        WinDestroyWindow(m_int.hwndFrame);
    }

    WinDestroyMsgQueue(m_int.hmq);
    WinTerminate(m_int.hab);
}

static int config(uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height,
                  uint32_t flags, char *title, uint32_t format)
{
    RECTL   rcl;

    mp_msg(MSGT_VO, MSGL_V,
           "KVA: Using 0x%X (%s) image format, vo_config_count = %d\n",
           format, vo_format_name(format), vo_config_count);

    imgFree();

    if (query_format_info(format, &m_int.fHWAccel, &m_int.fcc, &m_int.bpp,
                          &m_int.nChromaShift))
        return 1;

    m_int.iImageFormat = format;

    // if there is no hw accel for given format,
    // try any format supported by hw accel
    if (!m_int.fHWAccel) {
        int dstFormat = 0;

        sws_freeContext(m_int.sws);

        if (m_int.kvac.ulInputFormatFlags & KVAF_YV12)
            dstFormat = IMGFMT_YV12;
        else if (m_int.kvac.ulInputFormatFlags & KVAF_YUY2)
            dstFormat = IMGFMT_YUY2;
        else if (m_int.kvac.ulInputFormatFlags & KVAF_YVU9)
            dstFormat = IMGFMT_YVU9;
        else if (m_int.kvac.ulInputFormatFlags & KVAF_BGR24)
            dstFormat = IMGFMT_BGR24;
        else if (m_int.kvac.ulInputFormatFlags & KVAF_BGR16)
            dstFormat = IMGFMT_BGR16;
        else if (m_int.kvac.ulInputFormatFlags & KVAF_BGR15)
            dstFormat = IMGFMT_BGR15;

        if (query_format_info(dstFormat, NULL, &m_int.fcc, NULL, NULL))
            return 1;

        m_int.sws = sws_getContextFromCmdLine(width, height, format,
                                              width, height, dstFormat);
    }

    mp_msg(MSGT_VO, MSGL_V, "KVA: Selected FOURCC = %.4s\n", (char *)&m_int.fcc);

    m_int.kvas.ulLength           = sizeof(KVASETUP);
    m_int.kvas.szlSrcSize.cx      = width;
    m_int.kvas.szlSrcSize.cy      = height;
    m_int.kvas.rclSrcRect.xLeft   = 0;
    m_int.kvas.rclSrcRect.yTop    = 0;
    m_int.kvas.rclSrcRect.xRight  = width;
    m_int.kvas.rclSrcRect.yBottom = height;
    m_int.kvas.ulRatio            = vo_keepaspect ? KVAR_FORCEANY : KVAR_NONE;
    m_int.kvas.ulAspectWidth      = d_width;
    m_int.kvas.ulAspectHeight     = d_height;
    m_int.kvas.fccSrcColor        = m_int.fcc;
    m_int.kvas.fDither            = TRUE;

    if (kvaSetup(&m_int.kvas)) {
        mp_msg(MSGT_VO, MSGL_ERR, "KVA: Setup failed!!!\n");

        return 1;
    }

    m_int.lStride = width * m_int.bpp;

    imgCreate();

    if (WinID == -1) {
        WinSetWindowText(m_int.hwndFrame, title);

        // initialize 'vo_fs' only once at first config() call
        if (vo_config_count == 0)
            vo_fs = flags & VOFLAG_FULLSCREEN;

        // workaround for T23 laptop with S3 Video by Franz Bakan
        if (!vo_fs && m_int.fFixT23) {
            d_width++;
            d_height++;
        }

        m_int.rclDst.xLeft   = ((LONG)vo_screenwidth  - (LONG)d_width)  / 2;
        m_int.rclDst.yBottom = ((LONG)vo_screenheight - (LONG)d_height) / 2;
        m_int.rclDst.xRight  = m_int.rclDst.xLeft   + d_width;
        m_int.rclDst.yTop    = m_int.rclDst.yBottom + d_height;

        if (vo_fs) {
            d_width  = vo_screenwidth;
            d_height = vo_screenheight;

            // when -fs option is used without this, title bar is not highlighted
            WinSetActiveWindow(HWND_DESKTOP, m_int.hwndFrame);

            WinSetParent(m_int.hwndSysMenu,  HWND_OBJECT, FALSE);
            WinSetParent(m_int.hwndTitleBar, HWND_OBJECT, FALSE);
            WinSetParent(m_int.hwndMinMax,   HWND_OBJECT, FALSE);

            setAspectRatio(KVAR_FORCEANY);
        }

        rcl.xLeft   = ((LONG)vo_screenwidth  - (LONG)d_width) / 2;
        rcl.yBottom = ((LONG)vo_screenheight - (LONG)d_height) /2 ;
        rcl.xRight  = rcl.xLeft              + d_width;
        rcl.yTop    = rcl.yBottom            + d_height;
    } else {
        vo_fs = 0;

        WinQueryWindowRect(HWNDFROMWINID(WinID), &m_int.rclDst);
        rcl = m_int.rclDst;
    }

    WinCalcFrameRect(m_int.hwndFrame, &rcl, FALSE);

    WinSetWindowPos(m_int.hwndFrame, HWND_TOP,
                    rcl.xLeft, rcl.yBottom,
                    rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                    SWP_SIZE | SWP_MOVE | SWP_ZORDER | SWP_SHOW |
                    (WinID == -1 ? SWP_ACTIVATE : 0));

    WinInvalidateRect(m_int.hwndFrame, NULL, TRUE);

    return 0;
}

static uint32_t get_image(mp_image_t *mpi)
{
    if (m_int.iImageFormat != mpi->imgfmt)
        return VO_FALSE;

    if (mpi->type == MP_IMGTYPE_STATIC || mpi->type == MP_IMGTYPE_TEMP) {
        if (mpi->flags & MP_IMGFLAG_PLANAR) {
            mpi->planes[1] = m_int.planes[1];
            mpi->planes[2] = m_int.planes[2];

            mpi->stride[1] = m_int.stride[1];
            mpi->stride[2] = m_int.stride[2];
        }

        mpi->planes[0] = m_int.planes[0];
        mpi->stride[0] = m_int.stride[0];
        mpi->flags    |= MP_IMGFLAG_DIRECT;

        return VO_TRUE;
    }

    return VO_FALSE;
}

static uint32_t draw_image(mp_image_t *mpi)
{
    // if -dr or -slices then do nothing:
    if (mpi->flags & (MP_IMGFLAG_DIRECT | MP_IMGFLAG_DRAW_CALLBACK))
        return VO_TRUE;

    draw_slice(mpi->planes, mpi->stride, mpi->w, mpi->h, mpi->x, mpi->y);

    return VO_TRUE;
}

static int query_format(uint32_t format)
{
    BOOL fHWAccel;
    int  res;

    if (query_format_info(format, &fHWAccel, NULL, NULL, NULL))
        return 0;

    res = VFCAP_CSP_SUPPORTED | VFCAP_OSD;
    if (fHWAccel) {
        res |= VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_HWSCALE_UP;

        if (!m_int.fFixT23)
            res |= VFCAP_HWSCALE_DOWN;
    }

    return res;
}

static int fs_toggle(void)
{
    RECTL   rcl;

    vo_fs = !vo_fs;

    if (vo_fs) {
        SWP swp;

        WinQueryWindowPos(m_int.hwndFrame, &swp);
        m_int.rclDst.xLeft   = swp.x;
        m_int.rclDst.yBottom = swp.y;
        m_int.rclDst.xRight  = m_int.rclDst.xLeft   + swp.cx;
        m_int.rclDst.yTop    = m_int.rclDst.yBottom + swp.cy;
        WinCalcFrameRect(m_int.hwndFrame, &m_int.rclDst, TRUE);

        if (WinID != -1)
            WinSetParent(m_int.hwndFrame, HWND_DESKTOP, FALSE);

        WinSetParent(m_int.hwndSysMenu,  HWND_OBJECT, FALSE);
        WinSetParent(m_int.hwndTitleBar, HWND_OBJECT, FALSE);
        WinSetParent(m_int.hwndMinMax,   HWND_OBJECT, FALSE);

        rcl.xLeft   = 0;
        rcl.yBottom = 0;
        rcl.xRight  = vo_screenwidth;
        rcl.yTop    = vo_screenheight;

        setAspectRatio(KVAR_FORCEANY);
    } else {
        if (WinID != -1)
            WinSetParent(m_int.hwndFrame, HWNDFROMWINID(WinID), TRUE);

        WinSetParent(m_int.hwndSysMenu,  m_int.hwndFrame, FALSE);
        WinSetParent(m_int.hwndTitleBar, m_int.hwndFrame, FALSE);
        WinSetParent(m_int.hwndMinMax,   m_int.hwndFrame, FALSE);

        rcl = m_int.rclDst;

        setAspectRatio(vo_keepaspect ? KVAR_FORCEANY : KVAR_NONE);
    }

    WinCalcFrameRect(m_int.hwndFrame, &rcl, FALSE);

    WinSetWindowPos(m_int.hwndFrame, HWND_TOP,
                    rcl.xLeft, rcl.yBottom,
                    rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                    SWP_SIZE | SWP_MOVE | SWP_ZORDER | SWP_SHOW |
                    (WinID == -1 ? SWP_ACTIVATE : 0));

    return VO_TRUE;
}

static int color_ctrl_set(char *what, int value)
{
    ULONG   ulAttr;
    ULONG   ulValue;

    if (!strcmp(what, "brightness"))
        ulAttr = KVAA_BRIGHTNESS;
    else if (!strcmp(what, "contrast"))
        ulAttr = KVAA_CONTRAST;
    else if (!strcmp(what, "hue"))
        ulAttr = KVAA_HUE;
    else if (!strcmp(what, "saturation"))
        ulAttr = KVAA_SATURATION;
    else
        return VO_NOTIMPL;

    ulValue = (value + 100) * 255 / 200;

    if (kvaSetAttr(ulAttr, &ulValue))
        return VO_NOTIMPL;

    return VO_TRUE;
}

static int color_ctrl_get(char *what, int *value)
{
    ULONG   ulAttr;
    ULONG   ulValue;

    if (!strcmp(what, "brightness"))
        ulAttr = KVAA_BRIGHTNESS;
    else if (!strcmp(what, "contrast"))
        ulAttr = KVAA_CONTRAST;
    else if (!strcmp(what, "hue"))
        ulAttr = KVAA_HUE;
    else if (!strcmp(what, "saturation"))
        ulAttr = KVAA_SATURATION;
    else
        return VO_NOTIMPL;

    if (kvaQueryAttr(ulAttr, &ulValue))
        return VO_NOTIMPL;

    // add 1 to adjust range
    *value = ((ulValue + 1) * 200 / 255) - 100;

    return VO_TRUE;
}

static int control(uint32_t request, void *data, ...)
{
    switch (request) {
    case VOCTRL_GET_IMAGE:
        return get_image(data);

    case VOCTRL_DRAW_IMAGE:
        return draw_image(data);

    case VOCTRL_QUERY_FORMAT:
        return query_format(*(uint32_t *)data);

    case VOCTRL_FULLSCREEN:
        return fs_toggle();

    case VOCTRL_SET_EQUALIZER:
        {
        va_list ap;
        int     value;

        va_start(ap, data);
        value = va_arg(ap, int);
        va_end(ap);

        return color_ctrl_set(data, value);
        }

    case VOCTRL_GET_EQUALIZER:
        {
        va_list ap;
        int     *value;

        va_start(ap, data);
        value = va_arg(ap, int *);
        va_end(ap);

        return color_ctrl_get(data, value);
        }

    case VOCTRL_UPDATE_SCREENINFO:
        vo_screenwidth  = m_int.kvac.cxScreen;
        vo_screenheight = m_int.kvac.cyScreen;

        aspect_save_screenres(vo_screenwidth, vo_screenheight);

        return VO_TRUE;
    }

    return VO_NOTIMPL;
}

static int draw_frame(uint8_t *src[])
{
    return VO_ERROR;
}

static int draw_slice(uint8_t *src[], int stride[], int w, int h, int x, int y)
{
    uint8_t *s;
    uint8_t *d;

    // copy packed or Y
    d = m_int.planes[0] + m_int.stride[0] * y + x;
    s = src[0];
    mem2agpcpy_pic(d, s, w * m_int.bpp, h, m_int.stride[0], stride[0]);

    // YV12 or YVU9
    if (m_int.nChromaShift) {
        w >>= m_int.nChromaShift; h >>= m_int.nChromaShift;
        x >>= m_int.nChromaShift; y >>= m_int.nChromaShift;

        // copy U
        d = m_int.planes[1] + m_int.stride[1] * y + x;
        s = src[1];
        mem2agpcpy_pic(d, s, w, h, m_int.stride[1], stride[1]);

        // copy V
        d = m_int.planes[2] + m_int.stride[2] * y + x;
        s = src[2];
        mem2agpcpy_pic(d, s, w, h, m_int.stride[2], stride[2]);
    }

    return 0;
}

#define vo_draw_alpha(imgfmt) \
    vo_draw_alpha_##imgfmt(w, h, src, srca, stride, \
                           m_int.planes[0] + m_int.stride[0] * y0 + m_int.bpp * x0, \
                           m_int.stride[0])

static void draw_alpha(int x0, int y0, int w, int h,
                       unsigned char *src, unsigned char *srca, int stride)
{
    switch (m_int.iImageFormat) {
    case IMGFMT_YV12:
    case IMGFMT_YVU9:
        vo_draw_alpha(yv12);
        break;

    case IMGFMT_YUY2:
        vo_draw_alpha(yuy2);
        break;

    case IMGFMT_BGR24:
        vo_draw_alpha(rgb24);
        break;

    case IMGFMT_BGR16:
        vo_draw_alpha(rgb16);
        break;

    case IMGFMT_BGR15:
        vo_draw_alpha(rgb15);
        break;
    }
}

static void draw_osd(void)
{
    vo_draw_text(SRC_WIDTH, SRC_HEIGHT, draw_alpha);
}

static void flip_page(void)
{
    imgDisplay();
}

static void check_events(void)
{
    QMSG    qm;

    // On slave mode, we need to change our window size according to a
    // parent window size
    if (WinID != -1) {
        RECTL rcl;

        WinQueryWindowRect(HWNDFROMWINID(WinID), &rcl);

        if (rcl.xLeft   != m_int.rclParent.xLeft   ||
            rcl.yBottom != m_int.rclParent.yBottom ||
            rcl.xRight  != m_int.rclParent.xRight  ||
            rcl.yTop    != m_int.rclParent.yTop) {
            WinSetWindowPos(m_int.hwndFrame, NULLHANDLE,
                            rcl.xLeft, rcl.yBottom,
                            rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                            SWP_SIZE | SWP_MOVE);

            m_int.rclParent = rcl;
        }
    }

    while (WinPeekMsg(m_int.hab, &qm, NULLHANDLE, 0, 0, PM_REMOVE))
        WinDispatchMsg(m_int.hab, &qm);
}
