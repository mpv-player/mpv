//=================== VideoCD ==========================
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
        
      printf("track %02d:  adr=%d  ctrl=%d  format=%d  %02d:%02d:%02d  mode: %d\n",
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
      memcpy(vcd_buf,&vcd_entry.cdte_addr.msf,sizeof(struct cdrom_msf));
      if(ioctl(fd,CDROMREADRAW,vcd_buf)==-1) return 0; // EOF?

      vcd_entry.cdte_addr.msf.frame++;
      if (vcd_entry.cdte_addr.msf.frame==75){
        vcd_entry.cdte_addr.msf.frame=0;
        vcd_entry.cdte_addr.msf.second++;
        if (vcd_entry.cdte_addr.msf.second==60){
          vcd_entry.cdte_addr.msf.second=0;
          vcd_entry.cdte_addr.msf.minute++;
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
  memcpy(vcd_buf,&vcd_entry.cdte_addr.msf,sizeof(struct cdrom_msf));
  if(ioctl(fd,CDROMREADRAW,vcd_buf)==-1) return 0; // EOF?
  ++vcd_cache_current;
  memcpy(mem,&vcd_buf[VCD_SECTOR_OFFS],VCD_SECTOR_DATA);
  return VCD_SECTOR_DATA;
}

#endif
