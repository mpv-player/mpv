
#include <sys/types.h>
#include <inttypes.h>
#include "libavutil/intreadwrite.h"
#include <sys/cdio.h>
#include <sys/scsiio.h>

#define	CDROM_LEADOUT	0xAA
#define TOCADDR(te) ((te).data->addr)
#define READ_TOC CDIOREADTOCENTRYS

typedef struct mp_vcd_priv_st {
  int fd;
  struct ioc_read_toc_entry entry;
  struct cd_toc_entry entry_data;
} mp_vcd_priv_t;

static inline void
vcd_set_msf(mp_vcd_priv_t* vcd, unsigned int sect)
{
  vcd->entry.data = &vcd->entry_data;
  TOCADDR(vcd->entry).msf.frame = sect % 75;
  sect = sect / 75;
  TOCADDR(vcd->entry).msf.second = sect % 60;
  sect = sect / 60;
  TOCADDR(vcd->entry).msf.minute = sect;
}

static inline void
vcd_inc_msf(mp_vcd_priv_t* vcd)
{
  vcd->entry.data = &vcd->entry_data;
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
  vcd->entry.data = &vcd->entry_data;
  return TOCADDR(vcd->entry).msf.frame +
        (TOCADDR(vcd->entry).msf.second +
         TOCADDR(vcd->entry).msf.minute * 60) * 75;
}

int
vcd_seek_to_track(mp_vcd_priv_t* vcd, int track)
{
  vcd->entry.address_format = CD_MSF_FORMAT;
  vcd->entry.starting_track = track;
  vcd->entry.data_len = sizeof(struct cd_toc_entry);
  vcd->entry.data = &vcd->entry_data;
  if (ioctl(vcd->fd, READ_TOC, &vcd->entry)) {
    mp_msg(MSGT_STREAM,MSGL_ERR,"ioctl dif1: %s\n",strerror(errno));
    return -1;
  }
  return VCD_SECTOR_DATA * vcd_get_msf(vcd);
}

int
vcd_get_track_end(mp_vcd_priv_t* vcd, int track)
{
  struct ioc_toc_header tochdr;
  if (ioctl(vcd->fd, CDIOREADTOCHEADER, &tochdr) == -1) {
    mp_msg(MSGT_STREAM,MSGL_ERR,"read CDROM toc header: %s\n",strerror(errno));
    return -1;
  }
  vcd->entry.address_format = CD_MSF_FORMAT;
  vcd->entry.starting_track = track < tochdr.ending_track ? (track + 1) : CDROM_LEADOUT;
  vcd->entry.data_len = sizeof(struct cd_toc_entry);
  vcd->entry.data = &vcd->entry_data;
  if (ioctl(vcd->fd, READ_TOC, &vcd->entry)) {
    mp_msg(MSGT_STREAM,MSGL_ERR,"ioctl dif2: %s\n",strerror(errno));
    return -1;
  }
  return VCD_SECTOR_DATA * vcd_get_msf(vcd);
}

mp_vcd_priv_t*
vcd_read_toc(int fd)
{
  struct ioc_toc_header tochdr;
  mp_vcd_priv_t* vcd;
  int i, min = 0, sec = 0, frame = 0;
  if (ioctl(fd, CDIOREADTOCHEADER, &tochdr) == -1) {
    mp_msg(MSGT_OPEN,MSGL_ERR,"read CDROM toc header: %s\n",strerror(errno));
    return NULL;
  }
  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_START_TRACK=%d\n", tochdr.starting_track);
  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_END_TRACK=%d\n", tochdr.ending_track);
  for (i = tochdr.starting_track; i <= tochdr.ending_track + 1; i++) {
    struct ioc_read_toc_entry tocentry;
    struct cd_toc_entry tocentry_data;

    tocentry.starting_track = i<=tochdr.ending_track ? i : CDROM_LEADOUT;
    tocentry.address_format = CD_MSF_FORMAT;
    tocentry.data_len = sizeof(struct cd_toc_entry);
    tocentry.data = &tocentry_data;

    if (ioctl(fd, READ_TOC, &tocentry) == -1) {
      mp_msg(MSGT_OPEN,MSGL_ERR,"read CDROM toc entry: %s\n",strerror(errno));
      return NULL;
    }

    if (i <= tochdr.ending_track)
    mp_msg(MSGT_OPEN,MSGL_INFO,"track %02d:  adr=%d  ctrl=%d  format=%d  %02d:%02d:%02d\n",
          (int)tocentry.starting_track,
          (int)tocentry.data->addr_type,
          (int)tocentry.data->control,
          (int)tocentry.address_format,
          (int)TOCADDR(tocentry).msf.minute,
          (int)TOCADDR(tocentry).msf.second,
          (int)TOCADDR(tocentry).msf.frame
      );

    if (mp_msg_test(MSGT_IDENTIFY, MSGL_INFO))
    {
      if (i > tochdr.starting_track)
      {
        min = TOCADDR(tocentry).msf.minute - min;
        sec = TOCADDR(tocentry).msf.second - sec;
        frame = TOCADDR(tocentry).msf.frame - frame;
        if ( frame < 0 )
        {
          frame += 75;
          sec --;
        }
        if ( sec < 0 )
        {
          sec += 60;
          min --;
        }
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_TRACK_%d_MSF=%02d:%02d:%02d\n", i - 1, min, sec, frame);
      }
      min = TOCADDR(tocentry).msf.minute;
      sec = TOCADDR(tocentry).msf.second;
      frame = TOCADDR(tocentry).msf.frame;
    }
  }
  vcd = malloc(sizeof(mp_vcd_priv_t));
  vcd->fd = fd;
  return vcd;
}

static int
vcd_read(mp_vcd_priv_t* vcd, char *mem)
{
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
  sc.datalen = 2328;
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
  vcd_inc_msf(vcd);
  return VCD_SECTOR_DATA;
}

