#include <sys/cdio.h>
#include <sys/cdrio.h>

//=================== VideoCD ==========================
#define	CDROM_LEADOUT	0xAA

typedef	struct {
	unsigned char   unused;
	unsigned char   minute;
	unsigned char   second;
	unsigned char   frame;
} cdrom_msf;

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

static char vcd_buf[VCD_SECTOR_SIZE];

static int vcd_read(int fd,char *mem){
      memcpy(vcd_buf,&vcd_entry.entry.addr.msf,sizeof(cdrom_msf));
/*      if(ioctl(fd,CDROMREADRAW,vcd_buf)==-1) return 0; */ // EOF?
/*      if(ioctl(fd,CDRIOCSETBLOCKSIZE,VCD_SECTOR_SIZE)==-1) return 0;
      if (pread(fd,vcd_buf,VCD_SECTOR_SIZE,ntohl(vcd_entry.entry.addr.lba)*VCD_SECTOR_SIZE) != VCD_SECTOR_SIZE) return 0; */ // EOF?
      vcd_entry.entry.addr.msf.frame++;
      if (vcd_entry.entry.addr.msf.frame==75){
        vcd_entry.entry.addr.msf.frame=0;
        vcd_entry.entry.addr.msf.second++;
        if (vcd_entry.entry.addr.msf.second==60){
          vcd_entry.entry.addr.msf.second=0;
          vcd_entry.entry.addr.msf.minute++;
        }
      }

      memcpy(mem,&vcd_buf[VCD_SECTOR_OFFS],VCD_SECTOR_DATA);
      return VCD_SECTOR_DATA;
}

//================== VCD CACHE =======================
#ifdef VCD_CACHE

static int vcd_cache_size=0;
static char *vcd_cache_data=NULL;
static int *vcd_cache_sectors=NULL;
static int vcd_cache_index=0; // index to first free (or oldest) cache sector
static int vcd_cache_current=-1;

void vcd_cache_init(int s){
  vcd_cache_size=s;
  vcd_cache_sectors=malloc(s*sizeof(int));
  vcd_cache_data=malloc(s*VCD_SECTOR_SIZE);
  memset(vcd_cache_sectors,255,s*sizeof(int));
}

static inline void vcd_cache_seek(int sect){
  vcd_cache_current=sect;
}

int vcd_cache_read(int fd,char* mem){
int i;
char* vcd_buf;
  for(i=0;i<vcd_cache_size;i++)
    if(vcd_cache_sectors[i]==vcd_cache_current){
      // found in the cache! :)
      vcd_buf=&vcd_cache_data[i*VCD_SECTOR_SIZE];
      ++vcd_cache_current;
      memcpy(mem,&vcd_buf[VCD_SECTOR_OFFS],VCD_SECTOR_DATA);
      return VCD_SECTOR_DATA;
    }
  // NEW cache entry:
  vcd_buf=&vcd_cache_data[vcd_cache_index*VCD_SECTOR_SIZE];
  vcd_cache_sectors[vcd_cache_index]=vcd_cache_current;
  ++vcd_cache_index;if(vcd_cache_index>=vcd_cache_size)vcd_cache_index=0;
  // read data!
  vcd_set_msf(vcd_cache_current);
  memcpy(vcd_buf,&vcd_entry.entry.addr.msf,sizeof(struct cdrom_msf));
/*  if(ioctl(fd,CDROMREADRAW,vcd_buf)==-1) return 0; */ // EOF?
  ++vcd_cache_current;
  memcpy(mem,&vcd_buf[VCD_SECTOR_OFFS],VCD_SECTOR_DATA);
  return VCD_SECTOR_DATA;
}

#endif
