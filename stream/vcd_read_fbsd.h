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

typedef struct mp_vcd_priv_st {
  int fd;
  struct ioc_read_toc_single_entry entry;
  cdsector_t buf;
} mp_vcd_priv_t;

static inline void vcd_set_msf(mp_vcd_priv_t* vcd, unsigned int sect){
  vcd->entry.entry.addr.msf.frame=sect%75;
  sect=sect/75;
  vcd->entry.entry.addr.msf.second=sect%60;
  sect=sect/60;
  vcd->entry.entry.addr.msf.minute=sect;
}

static inline unsigned int vcd_get_msf(mp_vcd_priv_t* vcd){
  return vcd->entry.entry.addr.msf.frame +
        (vcd->entry.entry.addr.msf.second+
         vcd->entry.entry.addr.msf.minute*60)*75;
}

int vcd_seek_to_track(mp_vcd_priv_t* vcd, int track){
  vcd->entry.address_format = CD_MSF_FORMAT;
  vcd->entry.track  = track;
  if (ioctl(vcd->fd, CDIOREADTOCENTRY, &vcd->entry)) {
    mp_msg(MSGT_STREAM,MSGL_ERR,"ioctl dif1: %s\n",strerror(errno));
    return -1;
  }
  return VCD_SECTOR_DATA*vcd_get_msf(vcd);
}

int vcd_get_track_end(mp_vcd_priv_t* vcd, int track){
  struct ioc_toc_header tochdr;
  if (ioctl(vcd->fd,CDIOREADTOCHEADER,&tochdr)==-1) {
    mp_msg(MSGT_STREAM,MSGL_ERR,"read CDROM toc header: %s\n",strerror(errno));
    return -1;
  }
  vcd->entry.address_format = CD_MSF_FORMAT;
  vcd->entry.track  = track<tochdr.ending_track?(track+1):CDROM_LEADOUT;
  if (ioctl(vcd->fd, CDIOREADTOCENTRY, &vcd->entry)) {
    mp_msg(MSGT_STREAM,MSGL_ERR,"ioctl dif2: %s\n",strerror(errno));
    return -1;
  }
  return VCD_SECTOR_DATA*vcd_get_msf(vcd);
}

mp_vcd_priv_t* vcd_read_toc(int fd){
  struct ioc_toc_header tochdr;
  mp_vcd_priv_t* vcd;
  int i, min = 0, sec = 0, frame = 0;
  if (ioctl(fd,CDIOREADTOCHEADER,&tochdr)==-1) {
    mp_msg(MSGT_OPEN,MSGL_ERR,"read CDROM toc header: %s\n",strerror(errno));
    return NULL;
  }
  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_START_TRACK=%d\n", tochdr.starting_track);
  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_END_TRACK=%d\n", tochdr.ending_track);
  for (i=tochdr.starting_track ; i<=tochdr.ending_track + 1; i++){
      struct ioc_read_toc_single_entry tocentry;

      tocentry.track  = i<=tochdr.ending_track ? i : CDROM_LEADOUT;
      tocentry.address_format = CD_MSF_FORMAT;

      if (ioctl(fd,CDIOREADTOCENTRY,&tocentry)==-1) {
	mp_msg(MSGT_OPEN,MSGL_ERR,"read CDROM toc entry: %s\n",strerror(errno));
	return NULL;
      }
        
      if (i<=tochdr.ending_track)
      mp_msg(MSGT_OPEN,MSGL_INFO,"track %02d:  adr=%d  ctrl=%d  format=%d  %02d:%02d:%02d\n",
          (int)tocentry.track,
          (int)tocentry.entry.addr_type,
          (int)tocentry.entry.control,
          (int)tocentry.address_format,
          (int)tocentry.entry.addr.msf.minute,
          (int)tocentry.entry.addr.msf.second,
          (int)tocentry.entry.addr.msf.frame
      );

      if (mp_msg_test(MSGT_IDENTIFY, MSGL_INFO))
      {
        if (i > tochdr.starting_track)
        {
          min = tocentry.entry.addr.msf.minute - min;
          sec = tocentry.entry.addr.msf.second - sec;
          frame = tocentry.entry.addr.msf.frame - frame;
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
        min = tocentry.entry.addr.msf.minute;
        sec = tocentry.entry.addr.msf.second;
        frame = tocentry.entry.addr.msf.frame;
      }
    }
  vcd = malloc(sizeof(mp_vcd_priv_t));
  vcd->fd = fd;
  return vcd;  
}

static int vcd_read(mp_vcd_priv_t* vcd,char *mem){

      if (pread(vcd->fd,&vcd->buf,VCD_SECTOR_SIZE,vcd_get_msf(vcd)*VCD_SECTOR_SIZE)
	 != VCD_SECTOR_SIZE) return 0;  // EOF?

      vcd->entry.entry.addr.msf.frame++;
      if (vcd->entry.entry.addr.msf.frame==75){
        vcd->entry.entry.addr.msf.frame=0;
        vcd->entry.entry.addr.msf.second++;
        if (vcd->entry.entry.addr.msf.second==60){
          vcd->entry.entry.addr.msf.second=0;
          vcd->entry.entry.addr.msf.minute++;
        }
      }
      memcpy(mem,vcd->buf.data,VCD_SECTOR_DATA);
      return VCD_SECTOR_DATA;
}

