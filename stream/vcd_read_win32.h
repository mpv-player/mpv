/*
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

#ifndef MPLAYER_VCD_READ_WIN32_H
#define MPLAYER_VCD_READ_WIN32_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ntddcdrm.h>
#include "common/msg.h"

typedef struct mp_vcd_priv_st mp_vcd_priv_t;

/*
   Unlike Unices, upon reading TOC, Windows will retrieve information for
   all tracks, so we don't need to call DeviceIoControl() in functions
   like vcd_seek_to_track() and vcd_get_track_end() for each track. Instead
   we cache the information in mp_vcd_priv_st.
*/
struct mp_vcd_priv_st {
    HANDLE hd;
    CDROM_TOC toc;
    unsigned sect;
    char buf[VCD_SECTOR_SIZE];
};

static inline void vcd_set_msf(mp_vcd_priv_t* vcd, unsigned sect)
{
    vcd->sect = sect;
}

static inline unsigned vcd_get_msf(mp_vcd_priv_t* vcd, int track){
    int index = track - vcd->toc.FirstTrack;
    /* -150 to compensate the 2-second pregap */
    return vcd->toc.TrackData[index].Address[3] +
        (vcd->toc.TrackData[index].Address[2] +
         vcd->toc.TrackData[index].Address[1] * 60) * 75 - 150;
}

static int vcd_seek_to_track(mp_vcd_priv_t* vcd, int track)
{
    unsigned sect;
    if (track < vcd->toc.FirstTrack || track > vcd->toc.LastTrack)
        return -1;
    sect = vcd_get_msf(vcd, track);
    vcd_set_msf(vcd, sect);
    return VCD_SECTOR_DATA * (sect + 2);
}

static int vcd_get_track_end(mp_vcd_priv_t* vcd, int track)
{
    if (track < vcd->toc.FirstTrack || track > vcd->toc.LastTrack)
        return -1;
    return VCD_SECTOR_DATA * (vcd_get_msf(vcd, track + 1));
}

static mp_vcd_priv_t* vcd_read_toc(stream_t *stream, int fd)
{
    DWORD dwBytesReturned;
    HANDLE hd;
    int i;
    mp_vcd_priv_t* vcd = malloc(sizeof(mp_vcd_priv_t));
    if (!vcd)
        return NULL;

    hd = (HANDLE)_get_osfhandle(fd);
    if (!DeviceIoControl(hd, IOCTL_CDROM_READ_TOC, NULL, 0, &vcd->toc,
                sizeof(CDROM_TOC), &dwBytesReturned, NULL)) {
        MP_ERR(stream, "read CDROM toc header: %lu\n",
                GetLastError());
        free(vcd);
        return NULL;
    }

    for (i = vcd->toc.FirstTrack; i <= vcd->toc.LastTrack + 1; i++) {
        int index = i - vcd->toc.FirstTrack;
        if (i <= vcd->toc.LastTrack) {
            MP_INFO(stream, "track %02d:  adr=%d  ctrl=%d"
                    "  %02d:%02d:%02d\n",
                    vcd->toc.TrackData[index].TrackNumber,
                    vcd->toc.TrackData[index].Adr,
                    vcd->toc.TrackData[index].Control,
                    vcd->toc.TrackData[index].Address[1],
                    vcd->toc.TrackData[index].Address[2],
                    vcd->toc.TrackData[index].Address[3]);
        }
    }

    vcd->hd = hd;
    return vcd;
}

#define vcd_close(priv) (CloseHandle(((mp_vcd_priv_t*)priv)->hd))

static int vcd_read(mp_vcd_priv_t* vcd, char *mem)
{
    DWORD dwBytesReturned;
    RAW_READ_INFO cdrom_raw;

    /* 2048 isn't a typo: it's the Windows way. */
    cdrom_raw.DiskOffset.QuadPart = (long long)(2048 * vcd->sect);
    cdrom_raw.SectorCount = 1;
    cdrom_raw.TrackMode = XAForm2;

    if (!DeviceIoControl(vcd->hd, IOCTL_CDROM_RAW_READ, &cdrom_raw,
                sizeof(RAW_READ_INFO), vcd->buf, sizeof(vcd->buf),
                &dwBytesReturned, NULL))
        return 0;

    vcd->sect++;
    memcpy(mem, &vcd->buf[VCD_SECTOR_OFFS], VCD_SECTOR_DATA);
    return VCD_SECTOR_DATA;
}

#endif /* MPLAYER_VCD_READ_WIN32_H */
