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

#ifndef MPLAYER_VCD_READ_DARWIN_H
#define MPLAYER_VCD_READ_DARWIN_H

#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <CoreFoundation/CFBase.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include "compat/mpbswap.h"
#include "common/msg.h"
#include "stream.h"

//=================== VideoCD ==========================
#define CDROM_LEADOUT   0xAA

typedef struct
{
        uint8_t sync            [12];
        uint8_t header          [4];
        uint8_t subheader       [8];
        uint8_t data            [2324];
        uint8_t spare           [4];
} cdsector_t;

typedef struct mp_vcd_priv_st
{
        stream_t *stream;
        int fd;
        cdsector_t buf;
        dk_cd_read_track_info_t entry;
        struct CDDiscInfo hdr;
        CDMSF msf;
} mp_vcd_priv_t;

static inline void vcd_set_msf(mp_vcd_priv_t* vcd, unsigned int sect)
{
  vcd->msf = CDConvertLBAToMSF(sect);
}

static inline unsigned int vcd_get_msf(mp_vcd_priv_t* vcd)
{
  return CDConvertMSFToLBA(vcd->msf);
}

static int vcd_seek_to_track(mp_vcd_priv_t* vcd, int track)
{
        struct CDTrackInfo entry;

        memset( &vcd->entry, 0, sizeof(vcd->entry));
        vcd->entry.addressType = kCDTrackInfoAddressTypeTrackNumber;
        vcd->entry.address = track;
        vcd->entry.bufferLength = sizeof(entry);
        vcd->entry.buffer = &entry;

        if (ioctl(vcd->fd, DKIOCCDREADTRACKINFO, &vcd->entry))
        {
                MP_ERR(vcd->stream, "ioctl dif1: %s\n",strerror(errno));
                return -1;
        }
        vcd->msf = CDConvertLBAToMSF(be2me_32(entry.trackStartAddress));
        return VCD_SECTOR_DATA*vcd_get_msf(vcd);
}

static int vcd_get_track_end(mp_vcd_priv_t* vcd, int track)
{
        struct CDTrackInfo entry;

        if (track > vcd->hdr.lastTrackNumberInLastSessionLSB) {
                MP_ERR(vcd->stream, "track number %d greater than last track number %d\n",
                       track, vcd->hdr.lastTrackNumberInLastSessionLSB);
                return -1;
        }

        //read track info
        memset( &vcd->entry, 0, sizeof(vcd->entry));
        vcd->entry.addressType = kCDTrackInfoAddressTypeTrackNumber;
        vcd->entry.address = track<vcd->hdr.lastTrackNumberInLastSessionLSB?track+1:vcd->hdr.lastTrackNumberInLastSessionLSB;
        vcd->entry.bufferLength = sizeof(entry);
        vcd->entry.buffer = &entry;

        if (ioctl(vcd->fd, DKIOCCDREADTRACKINFO, &vcd->entry))
        {
                MP_ERR(vcd->stream, "ioctl dif2: %s\n",strerror(errno));
                return -1;
        }
        if (track == vcd->hdr.lastTrackNumberInLastSessionLSB)
                vcd->msf = CDConvertLBAToMSF(be2me_32(entry.trackStartAddress) +
                                             be2me_32(entry.trackSize));
        else
        vcd->msf = CDConvertLBAToMSF(be2me_32(entry.trackStartAddress));
        return VCD_SECTOR_DATA*vcd_get_msf(vcd);
}

static mp_vcd_priv_t* vcd_read_toc(stream_t *stream, int fd)
{
        dk_cd_read_disc_info_t tochdr;
        struct CDDiscInfo hdr;

        dk_cd_read_track_info_t tocentry;
        struct CDTrackInfo entry;
        CDMSF trackMSF;

        mp_vcd_priv_t* vcd;
        int i;

        //read toc header
    memset(&tochdr, 0, sizeof(tochdr));
    tochdr.buffer = &hdr;
    tochdr.bufferLength = sizeof(hdr);

    if (ioctl(fd, DKIOCCDREADDISCINFO, &tochdr) < 0)
        {
                MP_ERR(stream, "read CDROM toc header: %s\n",strerror(errno));
                return NULL;
    }

        //print all track info
        for (i=hdr.firstTrackNumberInLastSessionLSB ; i<=hdr.lastTrackNumberInLastSessionLSB + 1; i++)
        {
                if (i <= hdr.lastTrackNumberInLastSessionLSB) {
                memset( &tocentry, 0, sizeof(tocentry));
                tocentry.addressType = kCDTrackInfoAddressTypeTrackNumber;
                tocentry.address = i;
                tocentry.bufferLength = sizeof(entry);
                tocentry.buffer = &entry;

                if (ioctl(fd,DKIOCCDREADTRACKINFO,&tocentry)==-1)
                {
                        MP_ERR(stream, "read CDROM toc entry: %s\n",strerror(errno));
                        return NULL;
                }

                trackMSF = CDConvertLBAToMSF(be2me_32(entry.trackStartAddress));
                }
                else
                        trackMSF = CDConvertLBAToMSF(be2me_32(entry.trackStartAddress)
                                                     + be2me_32(entry.trackSize));

                //MP_INFO(vcd->stream, "track %02d:  adr=%d  ctrl=%d  format=%d  %02d:%02d:%02d\n",
                if (i<=hdr.lastTrackNumberInLastSessionLSB)
                MP_INFO(stream, "track %02d: format=%d  %02d:%02d:%02d\n",
          (int)tocentry.address,
          //(int)tocentry.entry.addr_type,
          //(int)tocentry.entry.control,
          (int)tocentry.addressType,
          (int)trackMSF.minute,
          (int)trackMSF.second,
          (int)trackMSF.frame
                );

        }

        vcd = malloc(sizeof(mp_vcd_priv_t));
        vcd->stream = stream;
        vcd->fd = fd;
        vcd->hdr = hdr;
        vcd->msf = trackMSF;
        return vcd;
}

static int vcd_read(mp_vcd_priv_t* vcd,char *mem)
{
        if (pread(vcd->fd,&vcd->buf,VCD_SECTOR_SIZE,vcd_get_msf(vcd)*VCD_SECTOR_SIZE) != VCD_SECTOR_SIZE)
                return 0;  // EOF?

        vcd->msf.frame++;
        if (vcd->msf.frame==75)
        {
                vcd->msf.frame=0;
                vcd->msf.second++;

                if (vcd->msf.second==60)
                {
                        vcd->msf.second=0;
                        vcd->msf.minute++;
        }
      }

      memcpy(mem,vcd->buf.data,VCD_SECTOR_DATA);
      return VCD_SECTOR_DATA;
}

#endif /* MPLAYER_VCD_READ_DARWIN_H */
