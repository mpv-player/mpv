//=================== VideoCD ==========================
#if	defined(linux) || defined(sun) || defined(__bsdi__)

#if	defined(linux)
#include <linux/cdrom.h>
#elif	defined(sun)
#include <sys/cdio.h>
static int sun_vcd_read(int, int*);
#elif	defined(__bsdi__)
#include <dvd.h>
#endif


static struct cdrom_tocentry vcd_entry;

static inline void vcd_set_msf(unsigned int sect){
  vcd_entry.cdte_addr.msf.frame=sect%75;
  sect=sect/75;
  vcd_entry.cdte_addr.msf.second=sect%60;
  sect=sect/60;
  vcd_entry.cdte_addr.msf.minute=sect;
}

static inline unsigned int vcd_get_msf(){
  return vcd_entry.cdte_addr.msf.frame +
        (vcd_entry.cdte_addr.msf.second+
         vcd_entry.cdte_addr.msf.minute*60)*75;
}

int vcd_seek_to_track(int fd,int track){
  vcd_entry.cdte_format = CDROM_MSF;
  vcd_entry.cdte_track  = track;
  if (ioctl(fd, CDROMREADTOCENTRY, &vcd_entry)) {
    perror("ioctl dif1");
    return -1;
  }
  return VCD_SECTOR_DATA*vcd_get_msf();
}

int vcd_get_track_end(int fd,int track){
  struct cdrom_tochdr tochdr;
  if (ioctl(fd,CDROMREADTOCHDR,&tochdr)==-1)
    { perror("read CDROM toc header: "); return -1; }
  vcd_entry.cdte_format = CDROM_MSF;
  vcd_entry.cdte_track  = track<tochdr.cdth_trk1?(track+1):CDROM_LEADOUT;
  if (ioctl(fd, CDROMREADTOCENTRY, &vcd_entry)) {
    perror("ioctl dif2");
    return -1;
  }
  return VCD_SECTOR_DATA*vcd_get_msf();
}

void vcd_read_toc(int fd){
  struct cdrom_tochdr tochdr;
  int i;
  if (ioctl(fd,CDROMREADTOCHDR,&tochdr)==-1)
    { perror("read CDROM toc header: "); return; }
  for (i=tochdr.cdth_trk0 ; i<=tochdr.cdth_trk1 ; i++){
      struct cdrom_tocentry tocentry;

      tocentry.cdte_track  = i;
      tocentry.cdte_format = CDROM_MSF;

      if (ioctl(fd,CDROMREADTOCENTRY,&tocentry)==-1)
	{ perror("read CDROM toc entry: "); return; }
        
      mp_msg(MSGT_OPEN,MSGL_INFO,"track %02d:  adr=%d  ctrl=%d  format=%d  %02d:%02d:%02d  mode: %d\n",
          (int)tocentry.cdte_track,
          (int)tocentry.cdte_adr,
          (int)tocentry.cdte_ctrl,
          (int)tocentry.cdte_format,
          (int)tocentry.cdte_addr.msf.minute,
          (int)tocentry.cdte_addr.msf.second,
          (int)tocentry.cdte_addr.msf.frame,
          (int)tocentry.cdte_datamode
      );
    }
}


static char vcd_buf[VCD_SECTOR_SIZE];

static int vcd_read(int fd,char *mem){
#if	defined(linux) || defined(__bsdi__)
  memcpy(vcd_buf,&vcd_entry.cdte_addr.msf,sizeof(struct cdrom_msf));
  if(ioctl(fd,CDROMREADRAW,vcd_buf)==-1) return 0; // EOF?
  memcpy(mem,&vcd_buf[VCD_SECTOR_OFFS],VCD_SECTOR_DATA);
#elif	defined(sun)
  {
    int offset;
    if (sun_vcd_read(fd, &offset) <= 0) return 0;
    memcpy(mem,&vcd_buf[offset],VCD_SECTOR_DATA);
  }
#endif

  vcd_entry.cdte_addr.msf.frame++;
  if (vcd_entry.cdte_addr.msf.frame==75){
    vcd_entry.cdte_addr.msf.frame=0;
    vcd_entry.cdte_addr.msf.second++;
    if (vcd_entry.cdte_addr.msf.second==60){
      vcd_entry.cdte_addr.msf.second=0;
      vcd_entry.cdte_addr.msf.minute++;
    }
  }
    
  return VCD_SECTOR_DATA;
}


#ifdef	sun
#include <sys/scsi/generic/commands.h>
#include <sys/scsi/impl/uscsi.h>

#define	SUN_XAREAD	1	/*fails on atapi drives*/
#define	SUN_MODE2READ	2	/*fails on atapi drives*/
#define	SUN_SCSIREAD	3
#define	SUN_VCDREAD	SUN_SCSIREAD

static int sun_vcd_read(int fd, int *offset)
{
#if SUN_VCDREAD == SUN_XAREAD
  struct cdrom_cdxa cdxa;
  cdxa.cdxa_addr = vcd_get_msf();
  cdxa.cdxa_length = 1;
  cdxa.cdxa_data = vcd_buf;
  cdxa.cdxa_format = CDROM_XA_SECTOR_DATA;
  
  if(ioctl(fd,CDROMCDXA,&cdxa)==-1) {
    perror("CDROMCDXA");
    return 0;
  }
  *offset = 0;
#elif SUN_VCDREAD == SUN_MODE2READ
  struct cdrom_read cdread;
  cdread.cdread_lba = 4*vcd_get_msf();
  cdread.cdread_bufaddr = vcd_buf;
  cdread.cdread_buflen = 2336;

  if(ioctl(fd,CDROMREADMODE2,&cdread)==-1) {
    perror("CDROMREADMODE2");
    return 0;
  }
  *offset = 8;
#elif SUN_VCDREAD == SUN_SCSIREAD
  struct uscsi_cmd sc;
  union scsi_cdb cdb;
  int lba = vcd_get_msf();
  int blocks = 1;
  int sector_type;
  int sync, header_code, user_data, edc_ecc, error_field;
  int sub_channel;

  /* sector_type = 3; *//* mode2 */
  sector_type = 5;	/* mode2/form2 */
  sync = 0;
  header_code = 0;
  user_data = 1;
  edc_ecc = 0;
  error_field = 0;
  sub_channel = 0;

  memset(&cdb, 0, sizeof(cdb));
  memset(&sc, 0, sizeof(sc));
  cdb.scc_cmd = 0xBE;
  cdb.cdb_opaque[1] = (sector_type) << 2;
  cdb.cdb_opaque[2] = (lba >> 24) & 0xff;
  cdb.cdb_opaque[3] = (lba >> 16) & 0xff;
  cdb.cdb_opaque[4] = (lba >>  8) & 0xff;
  cdb.cdb_opaque[5] =  lba & 0xff;
  cdb.cdb_opaque[6] = (blocks >> 16) & 0xff;
  cdb.cdb_opaque[7] = (blocks >>  8) & 0xff;
  cdb.cdb_opaque[8] =  blocks & 0xff;
  cdb.cdb_opaque[9] = (sync << 7) |
		      (header_code << 5) |
		      (user_data << 4) |
		      (edc_ecc << 3) |
		      (error_field << 1);
  cdb.cdb_opaque[10] = sub_channel;

  sc.uscsi_cdb = (caddr_t)&cdb;
  sc.uscsi_cdblen = 12;
  sc.uscsi_bufaddr = vcd_buf;
  sc.uscsi_buflen = 2336;
  sc.uscsi_flags = USCSI_ISOLATE | USCSI_READ;
  sc.uscsi_timeout = 20;
  if (ioctl(fd, USCSICMD, &sc)) {
      perror("USCSICMD: READ CD");
      return -1;
  }
  if (sc.uscsi_status) {
      fprintf(stderr, "scsi command failed with status %d\n", sc.uscsi_status);
      return -1;
  }
  *offset = 0;
  return 1;
#else
#error SUN_VCDREAD
#endif
}
#endif	/*sun*/

#else /* linux || sun || __bsdi__ */

#error vcd is not yet supported on this arch...

#endif
