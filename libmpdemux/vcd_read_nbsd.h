
#include <sys/types.h>
#ifdef __NetBSD__
#include <sys/inttypes.h>
#endif
#include <sys/cdio.h>
#include <sys/scsiio.h>

#define	CDROM_LEADOUT	0xAA

typedef struct mp_vcd_priv_st {
  int fd;
  struct ioc_read_toc_entry entry;
  struct cd_toc_entry entry_data;
} mp_vcd_priv_t;

static inline void 
vcd_set_msf(mp_vcd_priv_t* vcd, unsigned int sect)
{
  vcd->entry_data.addr.msf.frame = sect % 75;
  sect = sect / 75;
  vcd->entry_data.addr.msf.second = sect % 60;
  sect = sect / 60;
  vcd->entry_data.addr.msf.minute = sect;
}

static inline void
vcd_inc_msf(mp_vcd_priv_t* vcd)
{
  vcd->entry_data.addr.msf.frame++;
  if (vcd->entry_data.addr.msf.frame==75){
    vcd->entry_data.addr.msf.frame=0;
    vcd->entry_data.addr.msf.second++;
    if (vcd->entry_data.addr.msf.second==60){
      vcd->entry_data.addr.msf.second=0;
      vcd->entry_data.addr.msf.minute++;
    }
  }
}

static inline unsigned int 
vcd_get_msf(mp_vcd_priv_t* vcd)
{
  return vcd->entry_data.addr.msf.frame +
  (vcd->entry_data.addr.msf.second +
   vcd->entry_data.addr.msf.minute * 60) * 75;
}

int 
vcd_seek_to_track(mp_vcd_priv_t* vcd, int track)
{
  vcd->entry.address_format = CD_MSF_FORMAT;
  vcd->entry.starting_track = track;
  vcd->entry.data_len = sizeof(struct cd_toc_entry);
  vcd->entry.data = &vcd->entry_data;
  if (ioctl(vcd->fd, CDIOREADTOCENTRIES, &vcd->entry)) {
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
  if (ioctl(vcd->fd, CDIOREADTOCENTRYS, &vcd->entry)) {
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
    return;
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

    if (ioctl(fd, CDIOREADTOCENTRYS, &tocentry) == -1) {
      mp_msg(MSGT_OPEN,MSGL_ERR,"read CDROM toc entry: %s\n",strerror(errno));
      return NULL;
    }
    if (i <= tochdr.ending_track)
    mp_msg(MSGT_OPEN,MSGL_INFO,"track %02d:  adr=%d  ctrl=%d  format=%d  %02d:%02d:%02d\n",
	   (int) tocentry.starting_track,
	   (int) tocentry.data->addr_type,
	   (int) tocentry.data->control,
	   (int) tocentry.address_format,
	   (int) tocentry.data->addr.msf.minute,
	   (int) tocentry.data->addr.msf.second,
	   (int) tocentry.data->addr.msf.frame
      );

    if (mp_msg_test(MSGT_IDENTIFY, MSGL_INFO))
    {
      if (i > tochdr.starting_track)
      {
        min = tocentry.data->addr.msf.minute - min;
        sec = tocentry.data->addr.msf.second - sec;
        frame = tocentry.data->addr.msf.frame - frame;
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
      min = tocentry.data->addr.msf.minute;
      sec = tocentry.data->addr.msf.second;
      frame = tocentry.data->addr.msf.frame;
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
  int             sector_type;
  int             sync, header_code, user_data, edc_ecc, error_field;
  int             sub_channel;
  int             rc;

  blocks = 1;
  sector_type = 5;		/* mode2/form2 */
  sync = 0;
  header_code = 0;
  user_data = 1;
  edc_ecc = 0;
  error_field = 0;
  sub_channel = 0;

  memset(&sc, 0, sizeof(sc));
  sc.cmd[0] = 0xBE;
  sc.cmd[1] = (sector_type) << 2;
  sc.cmd[2] = (lba >> 24) & 0xff;
  sc.cmd[3] = (lba >> 16) & 0xff;
  sc.cmd[4] = (lba >> 8) & 0xff;
  sc.cmd[5] = lba & 0xff;
  sc.cmd[6] = (blocks >> 16) & 0xff;
  sc.cmd[7] = (blocks >> 8) & 0xff;
  sc.cmd[8] = blocks & 0xff;
  sc.cmd[9] = (sync << 7) | (header_code << 5) | (user_data << 4) |
    (edc_ecc << 3) | (error_field << 1);
  sc.cmd[10] = sub_channel;
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

