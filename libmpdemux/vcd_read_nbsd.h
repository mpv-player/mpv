
#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/cdio.h>
#include <sys/scsiio.h>

#define	CDROM_LEADOUT	0xAA

static struct ioc_read_toc_entry vcd_entry;
static struct cd_toc_entry vcd_entry_data;
static char     vcd_buf[VCD_SECTOR_SIZE];

static inline void 
vcd_set_msf(unsigned int sect)
{
  unsigned int    s = sect;
  vcd_entry_data.addr.msf.frame = sect % 75;
  sect = sect / 75;
  vcd_entry_data.addr.msf.second = sect % 60;
  sect = sect / 60;
  vcd_entry_data.addr.msf.minute = sect;
}

static inline unsigned int 
vcd_get_msf()
{
  return vcd_entry_data.addr.msf.frame +
  (vcd_entry_data.addr.msf.second +
   vcd_entry_data.addr.msf.minute * 60) * 75;
}

int 
vcd_seek_to_track(int fd, int track)
{
  vcd_entry.address_format = CD_MSF_FORMAT;
  vcd_entry.starting_track = track;
  vcd_entry.data_len = sizeof(struct cd_toc_entry);
  vcd_entry.data = &vcd_entry_data;
  if (ioctl(fd, CDIOREADTOCENTRIES, &vcd_entry)) {
    perror("ioctl dif1");
    return -1;
  }
  return VCD_SECTOR_DATA * vcd_get_msf();
}

int 
vcd_get_track_end(int fd, int track)
{
  struct ioc_toc_header tochdr;
  if (ioctl(fd, CDIOREADTOCHEADER, &tochdr) == -1) {
    perror("read CDROM toc header: ");
    return -1;
  }
  vcd_entry.address_format = CD_MSF_FORMAT;
  vcd_entry.starting_track = track < tochdr.ending_track ? (track + 1) : CDROM_LEADOUT;
  vcd_entry.data_len = sizeof(struct cd_toc_entry);
  vcd_entry.data = &vcd_entry_data;
  if (ioctl(fd, CDIOREADTOCENTRYS, &vcd_entry)) {
    perror("ioctl dif2");
    return -1;
  }
  return VCD_SECTOR_DATA * vcd_get_msf();
}

void 
vcd_read_toc(int fd)
{
  struct ioc_toc_header tochdr;
  int             i;
  if (ioctl(fd, CDIOREADTOCHEADER, &tochdr) == -1) {
    perror("read CDROM toc header: ");
    return;
  }
  for (i = tochdr.starting_track; i <= tochdr.ending_track; i++) {
    struct ioc_read_toc_entry tocentry;
    struct cd_toc_entry tocentry_data;

    tocentry.starting_track = i;
    tocentry.address_format = CD_MSF_FORMAT;
    tocentry.data_len = sizeof(struct cd_toc_entry);
    tocentry.data = &tocentry_data;

    if (ioctl(fd, CDIOREADTOCENTRYS, &tocentry) == -1) {
      perror("read CDROM toc entry: ");
      return;
    }
    printf("track %02d:  adr=%d  ctrl=%d  format=%d  %02d:%02d:%02d\n",
	   (int) tocentry.starting_track,
	   (int) tocentry.data->addr_type,
	   (int) tocentry.data->control,
	   (int) tocentry.address_format,
	   (int) tocentry.data->addr.msf.minute,
	   (int) tocentry.data->addr.msf.second,
	   (int) tocentry.data->addr.msf.frame
      );
  }
}

static int 
vcd_read(int fd, char *mem)
{
  struct scsireq  sc;
  int             lba = vcd_get_msf();
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
  rc = ioctl(fd, SCIOCCOMMAND, &sc);
  if (rc == -1) {
    perror("SCIOCCOMMAND");
    return -1;
  }
  if (sc.retsts || sc.error) {
    fprintf(stderr, "scsi command failed: status %d error %d\n", sc.retsts,
	    sc.error);
    return -1;
  }
  return VCD_SECTOR_DATA;
}

#ifdef VCD_CACHE

static int      vcd_cache_size = 0;
static char    *vcd_cache_data = NULL;
static int     *vcd_cache_sectors = NULL;
static int      vcd_cache_index = 0;
static int      vcd_cache_current = -1;

void 
vcd_cache_init(int s)
{
  vcd_cache_size = s;
  vcd_cache_sectors = malloc(s * sizeof(int));
  vcd_cache_data = malloc(s * VCD_SECTOR_SIZE);
  memset(vcd_cache_sectors, 255, s * sizeof(int));
}

static inline void 
vcd_cache_seek(int sect)
{
  vcd_cache_current = sect;
}

int 
vcd_cache_read(int fd, char *mem)
{
  int             i;
  char           *vcd_buf;
  for (i = 0; i < vcd_cache_size; i++)
    if (vcd_cache_sectors[i] == vcd_cache_current) {
      vcd_buf = &vcd_cache_data[i * VCD_SECTOR_SIZE];
      ++vcd_cache_current;
      memcpy(mem, &vcd_buf[VCD_SECTOR_OFFS], VCD_SECTOR_DATA);
      return VCD_SECTOR_DATA;
    }
  vcd_buf = &vcd_cache_data[vcd_cache_index * VCD_SECTOR_SIZE];
  vcd_cache_sectors[vcd_cache_index] = vcd_cache_current;
  ++vcd_cache_index;
  if (vcd_cache_index >= vcd_cache_size)
    vcd_cache_index = 0;
  vcd_set_msf(vcd_cache_current);
  memcpy(vcd_buf, &vcd_entry_data.addr.msf, sizeof(vcd_entry_data.addr.msf));
  ++vcd_cache_current;
  memcpy(mem, &vcd_buf[VCD_SECTOR_OFFS], VCD_SECTOR_DATA);
  return VCD_SECTOR_DATA;
}
#endif

