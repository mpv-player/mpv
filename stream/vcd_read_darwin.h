#ifndef MPLAYER_VCD_READ_DARWIN_H
#define MPLAYER_VCD_READ_DARWIN_H

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
#include "mpbswap.h"
#include "mp_msg.h"
#include "stream.h"

//=================== VideoCD ==========================
#define	CDROM_LEADOUT	0xAA

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

int vcd_seek_to_track(mp_vcd_priv_t* vcd, int track)
{
	struct CDTrackInfo entry;

	memset( &vcd->entry, 0, sizeof(vcd->entry));
	vcd->entry.addressType = kCDTrackInfoAddressTypeTrackNumber;
	vcd->entry.address = track;
	vcd->entry.bufferLength = sizeof(entry);
	vcd->entry.buffer = &entry;
  
	if (ioctl(vcd->fd, DKIOCCDREADTRACKINFO, &vcd->entry))
	{
		mp_msg(MSGT_STREAM,MSGL_ERR,"ioctl dif1: %s\n",strerror(errno));
		return -1;
	}
	vcd->msf = CDConvertLBAToMSF(be2me_32(entry.trackStartAddress));
	return VCD_SECTOR_DATA*vcd_get_msf(vcd);
}

int vcd_get_track_end(mp_vcd_priv_t* vcd, int track)
{
	struct CDTrackInfo entry;
	
	if (track > vcd->hdr.lastTrackNumberInLastSessionLSB) {
		mp_msg(MSGT_OPEN, MSGL_ERR,
		       "track number %d greater than last track number %d\n",
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
		mp_msg(MSGT_STREAM,MSGL_ERR,"ioctl dif2: %s\n",strerror(errno));
		return -1;
	}
	if (track == vcd->hdr.lastTrackNumberInLastSessionLSB)
		vcd->msf = CDConvertLBAToMSF(be2me_32(entry.trackStartAddress) +
		                             be2me_32(entry.trackSize));
	else
	vcd->msf = CDConvertLBAToMSF(be2me_32(entry.trackStartAddress));
	return VCD_SECTOR_DATA*vcd_get_msf(vcd);
}

mp_vcd_priv_t* vcd_read_toc(int fd)
{
	dk_cd_read_disc_info_t tochdr;
	struct CDDiscInfo hdr;
	
	dk_cd_read_track_info_t tocentry;
	struct CDTrackInfo entry;
	CDMSF trackMSF;
	
	mp_vcd_priv_t* vcd;
	int i, min = 0, sec = 0, frame = 0;
  
	//read toc header
    memset(&tochdr, 0, sizeof(tochdr));
    tochdr.buffer = &hdr;
    tochdr.bufferLength = sizeof(hdr);
  
    if (ioctl(fd, DKIOCCDREADDISCINFO, &tochdr) < 0)
	{
		mp_msg(MSGT_OPEN,MSGL_ERR,"read CDROM toc header: %s\n",strerror(errno));
		return NULL;
    }
	
	//print all track info
	mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_START_TRACK=%d\n", hdr.firstTrackNumberInLastSessionLSB);
	mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_END_TRACK=%d\n", hdr.lastTrackNumberInLastSessionLSB);
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
			mp_msg(MSGT_OPEN,MSGL_ERR,"read CDROM toc entry: %s\n",strerror(errno));
			return NULL;
		}
		
		trackMSF = CDConvertLBAToMSF(be2me_32(entry.trackStartAddress));
		}
		else
			trackMSF = CDConvertLBAToMSF(be2me_32(entry.trackStartAddress)
			                             + be2me_32(entry.trackSize));
        
		//mp_msg(MSGT_OPEN,MSGL_INFO,"track %02d:  adr=%d  ctrl=%d  format=%d  %02d:%02d:%02d\n",
		if (i<=hdr.lastTrackNumberInLastSessionLSB)
		mp_msg(MSGT_OPEN,MSGL_INFO,"track %02d: format=%d  %02d:%02d:%02d\n",
          (int)tocentry.address,
          //(int)tocentry.entry.addr_type,
          //(int)tocentry.entry.control,
          (int)tocentry.addressType,
          (int)trackMSF.minute,
          (int)trackMSF.second,
          (int)trackMSF.frame
		);

		if (mp_msg_test(MSGT_IDENTIFY, MSGL_INFO))
		{
		  if (i > hdr.firstTrackNumberInLastSessionLSB)
		  {
		    min = trackMSF.minute - min;
		    sec = trackMSF.second - sec;
		    frame = trackMSF.frame - frame;
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
		  min = trackMSF.minute;
		  sec = trackMSF.second;
		  frame = trackMSF.frame;
		}
	}
 
	vcd = malloc(sizeof(mp_vcd_priv_t));
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
