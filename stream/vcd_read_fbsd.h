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

#ifndef MPLAYER_VCD_READ_FBSD_H
#define MPLAYER_VCD_READ_FBSD_H

#define _XOPEN_SOURCE 500

#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>
#include "libavutil/intreadwrite.h"
#include <sys/cdio.h>
#include <sys/ioctl.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#define VCD_NETBSD 1
#endif
#ifdef VCD_NETBSD
#include <sys/scsiio.h>
#define TOCADDR(te) ((te).data->addr)
#define READ_TOC CDIOREADTOCENTRYS
#else
#include <sys/cdrio.h>
#define TOCADDR(te) ((te).entry.addr)
#define READ_TOC CDIOREADTOCENTRY
#endif
#include "mp_msg.h"

//=================== VideoCD ==========================
#define	CDROM_LEADOUT	0xAA

typedef struct {
	uint8_t sync            [12];
	uint8_t header          [4];
	uint8_t subheader       [8];
	uint8_t data            [2324];
	uint8_t spare           [4];
} cdsector_t;

#ifdef VCD_NETBSD
typedef struct ioc_read_toc_entry vcd_tocentry;
#else
typedef struct ioc_read_toc_single_entry vcd_tocentry;
#endif

typedef struct mp_vcd_priv_st {
  int fd;
  vcd_tocentry entry;
#ifdef VCD_NETBSD
  struct cd_toc_entry entry_data;
#else
  cdsector_t buf;
#endif
  struct ioc_toc_header tochdr;
} mp_vcd_priv_t;

static inline void
vcd_set_msf(mp_vcd_priv_t* vcd, unsigned int sect)
{
#ifdef VCD_NETBSD
  vcd->entry.data = &vcd->entry_data;
#endif
  sect += 150;
  TOCADDR(vcd->entry).msf.frame = sect % 75;
  sect = sect / 75;
  TOCADDR(vcd->entry).msf.second = sect % 60;
  sect = sect / 60;
  TOCADDR(vcd->entry).msf.minute = sect;
}

static inline void
vcd_inc_msf(mp_vcd_priv_t* vcd)
{
#ifdef VCD_NETBSD
  vcd->entry.data = &vcd->entry_data;
#endif
  TOCADDR(vcd->entry).msf.frame++;
  if (TOCADDR(vcd->entry).msf.frame==75){
    TOCADDR(vcd->entry).msf.frame=0;
    TOCADDR(vcd->entry).msf.second++;
    if (TOCADDR(vcd->entry).msf.second==60){
      TOCADDR(vcd->entry).msf.second=0;
      TOCADDR(vcd->entry).msf.minute++;
    }
  }
}

static inline unsigned int
vcd_get_msf(mp_vcd_priv_t* vcd)
{
#ifdef VCD_NETBSD
  vcd->entry.data = &vcd->entry_data;
#endif
  return TOCADDR(vcd->entry).msf.frame +
        (TOCADDR(vcd->entry).msf.second +
         TOCADDR(vcd->entry).msf.minute * 60) * 75 - 150;
}

/**
 * \brief read a TOC entry
 * \param fd device to read from
 * \param dst buffer to read data into
 * \param nr track number to read info for
 * \return 1 on success, 0 on failure
 */
static int
read_toc_entry(mp_vcd_priv_t *vcd, int nr)
{
  vcd->entry.address_format = CD_MSF_FORMAT;
#ifdef VCD_NETBSD
  vcd->entry.data_len = sizeof(struct cd_toc_entry);
  vcd->entry.data = &vcd->entry_data;
  vcd->entry.starting_track = nr;
#else
  vcd->entry.track = nr;
#endif
  if (ioctl(vcd->fd, READ_TOC, &vcd->entry) == -1) {
    mp_msg(MSGT_OPEN,MSGL_ERR,"read CDROM toc entry: %s\n",strerror(errno));
    return 0;
  }
  return 1;
}

int
vcd_seek_to_track(mp_vcd_priv_t* vcd, int track)
{
  if (!read_toc_entry(vcd, track))
    return -1;
  return VCD_SECTOR_DATA * vcd_get_msf(vcd);
}

static int
vcd_get_track_end(mp_vcd_priv_t* vcd, int track)
{
  if (!read_toc_entry(vcd,
          track < vcd->tochdr.ending_track ? track + 1 : CDROM_LEADOUT))
    return -1;
  return VCD_SECTOR_DATA * vcd_get_msf(vcd);
}

static mp_vcd_priv_t*
vcd_read_toc(int fd)
{
  struct ioc_toc_header tochdr;
  mp_vcd_priv_t* vcd;
  int i, last_startsect;
  if (ioctl(fd, CDIOREADTOCHEADER, &tochdr) == -1) {
    mp_msg(MSGT_OPEN,MSGL_ERR,"read CDROM toc header: %s\n",strerror(errno));
    return NULL;
  }
  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_START_TRACK=%d\n", tochdr.starting_track);
  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_END_TRACK=%d\n", tochdr.ending_track);
  vcd = malloc(sizeof(mp_vcd_priv_t));
  vcd->fd = fd;
  vcd->tochdr = tochdr;
  for (i = tochdr.starting_track; i <= tochdr.ending_track + 1; i++) {
    if (!read_toc_entry(vcd,
          i <= tochdr.ending_track ? i : CDROM_LEADOUT)) {
      free(vcd);
      return NULL;
    }

    if (i <= tochdr.ending_track)
    mp_msg(MSGT_OPEN,MSGL_INFO,"track %02d:  adr=%d  ctrl=%d  format=%d  %02d:%02d:%02d\n",
#ifdef VCD_NETBSD
          (int)vcd->entry.starting_track,
          (int)vcd->entry.data->addr_type,
          (int)vcd->entry.data->control,
#else
          (int)vcd->entry.track,
          (int)vcd->entry.entry.addr_type,
          (int)vcd->entry.entry.control,
#endif
          (int)vcd->entry.address_format,
          (int)TOCADDR(vcd->entry).msf.minute,
          (int)TOCADDR(vcd->entry).msf.second,
          (int)TOCADDR(vcd->entry).msf.frame
      );

    if (mp_msg_test(MSGT_IDENTIFY, MSGL_INFO))
    {
      int startsect = vcd_get_msf(vcd);
      if (i > tochdr.starting_track)
      {
        // convert duraion to MSF
        vcd_set_msf(vcd, startsect - last_startsect);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_VCD_TRACK_%d_MSF=%02d:%02d:%02d\n",
               i - 1,
               TOCADDR(vcd->entry).msf.minute,
               TOCADDR(vcd->entry).msf.second,
               TOCADDR(vcd->entry).msf.frame);
      }
      last_startsect = startsect;
    }
  }
  return vcd;
}

static int vcd_end_track(mp_vcd_priv_t* vcd)
{
  return vcd->tochdr.ending_track;
}

static int
vcd_read(mp_vcd_priv_t* vcd, char *mem)
{
#ifdef VCD_NETBSD
  struct scsireq  sc;
  int             lba = vcd_get_msf(vcd);
  int             blocks;
  int             rc;

  blocks = 1;

  memset(&sc, 0, sizeof(sc));
  sc.cmd[0] = 0xBE;
  sc.cmd[1] = 5 << 2; // mode2/form2
  AV_WB32(&sc.cmd[2], lba);
  AV_WB24(&sc.cmd[6], blocks);
  sc.cmd[9] = 1 << 4; // user data only
  sc.cmd[10] = 0;     // no subchannel
  sc.cmdlen = 12;
  sc.databuf = (caddr_t) mem;
  sc.datalen = VCD_SECTOR_DATA;
  sc.senselen = sizeof(sc.sense);
  sc.flags = SCCMD_READ;
  sc.timeout = 10000;
  rc = ioctl(vcd->fd, SCIOCCOMMAND, &sc);
  if (rc == -1) {
    mp_msg(MSGT_STREAM,MSGL_ERR,"SCIOCCOMMAND: %s\n",strerror(errno));
    return -1;
  }
  if (sc.retsts || sc.error) {
    mp_msg(MSGT_STREAM,MSGL_ERR,"scsi command failed: status %d error %d\n",
	   sc.retsts,sc.error);
    return -1;
  }
#else
  if (pread(vcd->fd,&vcd->buf,VCD_SECTOR_SIZE,vcd_get_msf(vcd)*VCD_SECTOR_SIZE)
     != VCD_SECTOR_SIZE) return 0;  // EOF?

  memcpy(mem,vcd->buf.data,VCD_SECTOR_DATA);
#endif
  vcd_inc_msf(vcd);
  return VCD_SECTOR_DATA;
}

#endif /* MPLAYER_VCD_READ_FBSD_H */
