#include <sys/cdio.h>
#include <sys/cdrio.h>

//=================== VideoCD ==========================
#define	CDROM_LEADOUT	0xAA

typedef struct {
	uint8_t sync            [12];
	uint8_t header          [4];
	uint8_t subheader       [8];
	uint8_t data            [2324];
	uint8_t spare           [4];
} cdsector_t;

static cdsector_t vcd_buf;
static struct ioc_read_toc_single_entry vcd_entry;

static inline void vcd_set_msf(unsigned int sect){
  vcd_entry.entry.addr.msf.frame=sect%75;
  sect=sect/75;
  vcd_entry.entry.addr.msf.second=sect%60;
  sect=sect/60;
  vcd_entry.entry.addr.msf.minute=sect;
}

static inline unsigned int vcd_get_msf(){
  return vcd_entry.entry.addr.msf.frame +
        (vcd_entry.entry.addr.msf.second+
         vcd_entry.entry.addr.msf.minute*60)*75;
}

int vcd_seek_to_track(int fd,int track){
  vcd_entry.address_format = CD_MSF_FORMAT;
  vcd_entry.track  = track;
  if (ioctl(fd, CDIOREADTOCENTRY, &vcd_entry)) {
    perror("ioctl dif1");
    return -1;
  }
  return VCD_SECTOR_DATA*vcd_get_msf();
}

int vcd_get_track_end(int fd,int track){
  struct ioc_toc_header tochdr;
  if (ioctl(fd,CDIOREADTOCHEADER,&tochdr)==-1)
    { perror("read CDROM toc header: "); return -1; }
  vcd_entry.address_format = CD_MSF_FORMAT;
  vcd_entry.track  = track<tochdr.ending_track?(track+1):CDROM_LEADOUT;
  if (ioctl(fd, CDIOREADTOCENTRY, &vcd_entry)) {
    perror("ioctl dif2");
    return -1;
  }
  return VCD_SECTOR_DATA*vcd_get_msf();
}

void vcd_read_toc(int fd){
  struct ioc_toc_header tochdr;
  int i;
  if (ioctl(fd,CDIOREADTOCHEADER,&tochdr)==-1)
    { perror("read CDROM toc header: "); return; }
  for (i=tochdr.starting_track ; i<=tochdr.ending_track ; i++){
      struct ioc_read_toc_single_entry tocentry;

      tocentry.track  = i;
      tocentry.address_format = CD_MSF_FORMAT;

      if (ioctl(fd,CDIOREADTOCENTRY,&tocentry)==-1)
	{ perror("read CDROM toc entry: "); return; }
        
      printf("track %02d:  adr=%d  ctrl=%d  format=%d  %02d:%02d:%02d\n",
          (int)tocentry.track,
          (int)tocentry.entry.addr_type,
          (int)tocentry.entry.control,
          (int)tocentry.address_format,
          (int)tocentry.entry.addr.msf.minute,
          (int)tocentry.entry.addr.msf.second,
          (int)tocentry.entry.addr.msf.frame
      );
    }
}

static int vcd_read(int fd,char *mem){

      if (pread(fd,&vcd_buf,VCD_SECTOR_SIZE,vcd_get_msf()*VCD_SECTOR_SIZE)
	 != VCD_SECTOR_SIZE) return 0;  // EOF?

      vcd_entry.entry.addr.msf.frame++;
      if (vcd_entry.entry.addr.msf.frame==75){
        vcd_entry.entry.addr.msf.frame=0;
        vcd_entry.entry.addr.msf.second++;
        if (vcd_entry.entry.addr.msf.second==60){
          vcd_entry.entry.addr.msf.second=0;
          vcd_entry.entry.addr.msf.minute++;
        }
      }
      memcpy(mem,vcd_buf.data,VCD_SECTOR_DATA);
      return VCD_SECTOR_DATA;
}

