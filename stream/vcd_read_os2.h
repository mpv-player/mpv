/*
 * implementation of VCD IO for OS/2
 *
 * Copyright (c) 2009 KO Myung-Hun (komh@chollian.net)
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

#ifndef MPLAYER_VCD_READ_OS2_H
#define MPLAYER_VCD_READ_OS2_H

#include "mp_msg.h"

struct __attribute__((packed)) msf {
    BYTE bFrame;
    BYTE bSecond;
    BYTE bMinute;
    BYTE bReserved;
};

typedef struct {
    HFILE      hcd;
    struct msf msfCurrent;
    int        iFirstTrack;
    int        iLastTrack;
    struct msf msfLeadOut;
    BYTE       abVCDSector[VCD_SECTOR_SIZE];
} mp_vcd_priv_t;

static inline void vcd_set_msf(mp_vcd_priv_t *vcd, unsigned sect)
{
    sect += 150;
    vcd->msfCurrent.bFrame = sect % 75;
    sect = sect / 75;
    vcd->msfCurrent.bSecond = sect % 60;
    sect = sect / 60;
    vcd->msfCurrent.bMinute = sect;
}

static inline unsigned vcd_get_msf(mp_vcd_priv_t *vcd)
{
  return vcd->msfCurrent.bFrame  +
        (vcd->msfCurrent.bSecond + vcd->msfCurrent.bMinute * 60) * 75 - 150;
}

int vcd_seek_to_track(mp_vcd_priv_t *vcd, int track)
{
    struct {
        UCHAR auchSign[4];
        BYTE  bTrack;
    } __attribute__((packed)) sParam = {{'C', 'D', '0', '1'},};

    struct {
        struct msf msfStart;
        BYTE       bControlInfo;
    } __attribute__((packed)) sData;

    ULONG ulParamLen;
    ULONG ulDataLen;
    ULONG rc;

    sParam.bTrack = track;
    rc = DosDevIOCtl(vcd->hcd, IOCTL_CDROMAUDIO, CDROMAUDIO_GETAUDIOTRACK,
                     &sParam, sizeof(sParam), &ulParamLen,
                     &sData, sizeof(sData), &ulDataLen);
    if (rc) {
        mp_msg(MSGT_STREAM, MSGL_ERR, "DosDevIOCtl(GETAUDIOTRACK) = 0x%lx\n", rc);
        return -1;
    }

    vcd->msfCurrent = sData.msfStart;

    return VCD_SECTOR_DATA * vcd_get_msf(vcd);
}

static int vcd_get_track_end(mp_vcd_priv_t *vcd, int track)
{
    if (track < vcd->iLastTrack)
        return vcd_seek_to_track(vcd, track + 1);

    vcd->msfCurrent = vcd->msfLeadOut;

    return VCD_SECTOR_DATA * vcd_get_msf(vcd);
}

static mp_vcd_priv_t *vcd_read_toc(int fd)
{
    mp_vcd_priv_t *vcd;

    UCHAR auchParamDisk[4] = {'C', 'D', '0', '1'};

    struct {
        BYTE       bFirstTrack;
        BYTE       bLastTrack;
        struct msf msfLeadOut;
    } __attribute__((packed)) sDataDisk;

    struct {
        UCHAR auchSign[4];
        BYTE  bTrack;
    } __attribute__((packed)) sParamTrack = {{'C', 'D', '0', '1'},};

    struct {
        struct msf msfStart;
        BYTE       bControlInfo;
    } __attribute__((packed)) sDataTrack;

    ULONG ulParamLen;
    ULONG ulDataLen;
    ULONG rc;
    int   i, iMinute = 0, iSecond = 0, iFrame = 0;

    rc = DosDevIOCtl(fd, IOCTL_CDROMAUDIO, CDROMAUDIO_GETAUDIODISK,
                     auchParamDisk, sizeof(auchParamDisk), &ulParamLen,
                     &sDataDisk, sizeof(sDataDisk), &ulDataLen);
    if (rc) {
        mp_msg(MSGT_OPEN, MSGL_ERR, "DosDevIOCtl(GETAUDIODISK) = 0x%lx\n", rc);
        return NULL;
    }

    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_START_TRACK=%d\n", sDataDisk.bFirstTrack);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_END_TRACK=%d\n", sDataDisk.bLastTrack);

    for (i = sDataDisk.bFirstTrack; i <= sDataDisk.bLastTrack + 1; i++) {
        if (i <= sDataDisk.bLastTrack) {
            sParamTrack.bTrack = i;
            rc = DosDevIOCtl(fd, IOCTL_CDROMAUDIO, CDROMAUDIO_GETAUDIOTRACK,
                             &sParamTrack, sizeof(sParamTrack), &ulParamLen,
                             &sDataTrack, sizeof(sDataTrack), &ulDataLen);
            if (rc) {
                mp_msg(MSGT_OPEN, MSGL_ERR, "DosDevIOCtl(GETAUDIOTRACK) = 0x%lx\n", rc);
                return NULL;
            }

            mp_msg(MSGT_OPEN, MSGL_INFO, "track %02d:  adr=%d  ctrl=%d  %02d:%02d:%02d\n",
                   i,
                   sDataTrack.bControlInfo & 0x0F,
                   sDataTrack.bControlInfo >> 4,
                   sDataTrack.msfStart.bMinute,
                   sDataTrack.msfStart.bSecond,
                   sDataTrack.msfStart.bFrame);
        } else
            sDataTrack.msfStart = sDataDisk.msfLeadOut;

        if (mp_msg_test(MSGT_IDENTIFY, MSGL_INFO)) {
            if (i > sDataDisk.bFirstTrack) {
                iMinute = sDataTrack.msfStart.bMinute - iMinute;
                iSecond = sDataTrack.msfStart.bSecond - iSecond;
                iFrame  = sDataTrack.msfStart.bFrame  - iFrame;
                if (iFrame < 0) {
                    iFrame += 75;
                    iSecond--;
                }
                if (iSecond < 0) {
                    iSecond += 60;
                    iMinute--;
                }
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_TRACK_%d_MSF=%02d:%02d:%02d\n",
                       i - 1, iMinute, iSecond, iFrame);
            }

            iMinute = sDataTrack.msfStart.bMinute;
            iSecond = sDataTrack.msfStart.bSecond;
            iFrame  = sDataTrack.msfStart.bFrame;
        }
    }

    vcd                 = calloc(1, sizeof(mp_vcd_priv_t));
    vcd->hcd            = fd;
    vcd->iFirstTrack    = sDataDisk.bFirstTrack;
    vcd->iLastTrack     = sDataDisk.bLastTrack;
    vcd->msfLeadOut     = sDataDisk.msfLeadOut;

    return vcd;
}

static int vcd_end_track(mp_vcd_priv_t* vcd)
{
    return vcd->iLastTrack;
}

static int vcd_read(mp_vcd_priv_t *vcd, char *mem)
{
    struct {
        UCHAR      auchSign[4];
        BYTE       bAddrMode;
        USHORT     usSectors;
        struct msf msfStart;
        BYTE       bReserved;
        BYTE       bInterleavedSize;
    } __attribute__((packed)) sParam = {{'C', 'D', '0', '1'}, 1, 1,};

    ULONG ulParamLen;
    ULONG ulDataLen;
    ULONG rc;

    /* lead-out ? */
    if (vcd->msfCurrent.bMinute == vcd->msfLeadOut.bMinute &&
        vcd->msfCurrent.bSecond == vcd->msfLeadOut.bSecond &&
        vcd->msfCurrent.bFrame  == vcd->msfLeadOut.bFrame)
        return 0;

    sParam.msfStart = vcd->msfCurrent;
    rc = DosDevIOCtl(vcd->hcd, IOCTL_CDROMDISK, CDROMDISK_READLONG,
                     &sParam, sizeof(sParam), &ulParamLen,
                     vcd->abVCDSector, sizeof(vcd->abVCDSector), &ulDataLen);
    if (rc) {
        mp_msg(MSGT_STREAM, MSGL_ERR, "DosDevIOCtl(READLONG) = 0x%lx\n", rc);
        return 0;
    }

    memcpy(mem, &vcd->abVCDSector[VCD_SECTOR_OFFS], VCD_SECTOR_DATA);

    vcd->msfCurrent.bFrame++;
    if (vcd->msfCurrent.bFrame == 75) {
        vcd->msfCurrent.bFrame = 0;
        vcd->msfCurrent.bSecond++;
        if (vcd->msfCurrent.bSecond == 60) {
            vcd->msfCurrent.bSecond = 0;
            vcd->msfCurrent.bMinute++;
        }
    }

    return VCD_SECTOR_DATA;
}

#endif /* MPLAYER_VCD_READ_OS2_H */

