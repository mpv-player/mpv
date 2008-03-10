#ifndef MPLAYER_VCD_READ_WIN32_H
#define MPLAYER_VCD_READ_WIN32_H

#include <ddk/ntddcdrm.h>
#include "mp_msg.h"

typedef struct mp_vcd_priv_st mp_vcd_priv_t;

/*
   Unlike Unices, upon reading TOC, Windows will retrieve information for
   all tracks, so we don't need to call DeviceIoControl() in functions
   like vcd_seek_to_track() and vcd_get_track_end() for each track. Instead
   we cache the information in mp_vcd_priv_st.
*/
struct mp_vcd_priv_st {
    HANDLE hd;
    CDROM_TOC toc;
    unsigned sect;
    char buf[VCD_SECTOR_SIZE];
};

static inline void vcd_set_msf(mp_vcd_priv_t* vcd, unsigned sect)
{
    vcd->sect = sect;
}

static inline unsigned vcd_get_msf(mp_vcd_priv_t* vcd, int track){
    int index = track - vcd->toc.FirstTrack;
    /* -150 to compensate the 2-second pregap */
    return vcd->toc.TrackData[index].Address[3] +
	(vcd->toc.TrackData[index].Address[2] +
	 vcd->toc.TrackData[index].Address[1] * 60) * 75 - 150;
}

int vcd_seek_to_track(mp_vcd_priv_t* vcd, int track)
{
    unsigned sect;
    if (track < vcd->toc.FirstTrack || track > vcd->toc.LastTrack)
	return -1;
    sect = vcd_get_msf(vcd, track);
    vcd_set_msf(vcd, sect);
    return VCD_SECTOR_DATA * (sect + 2);
}

int vcd_get_track_end(mp_vcd_priv_t* vcd, int track)
{
    if (track < vcd->toc.FirstTrack || track > vcd->toc.LastTrack)
	return -1;
    return VCD_SECTOR_DATA * (vcd_get_msf(vcd, track + 1));
}

mp_vcd_priv_t* vcd_read_toc(int fd)
{
    DWORD dwBytesReturned;
    HANDLE hd;
    int i, min = 0, sec = 0, frame = 0;
    mp_vcd_priv_t* vcd = malloc(sizeof(mp_vcd_priv_t));
    if (!vcd)
	return NULL;

    hd = (HANDLE)_get_osfhandle(fd);
    if (!DeviceIoControl(hd, IOCTL_CDROM_READ_TOC, NULL, 0, &vcd->toc,
		sizeof(CDROM_TOC), &dwBytesReturned, NULL)) {
	mp_msg(MSGT_OPEN, MSGL_ERR, "read CDROM toc header: %lu\n",
		GetLastError());
	free(vcd);
	return NULL;
    }

    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_START_TRACK=%d\n",
	    vcd->toc.FirstTrack);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_END_TRACK=%d\n",
	    vcd->toc.LastTrack);

    for (i = vcd->toc.FirstTrack; i <= vcd->toc.LastTrack + 1; i++) {
	int index = i - vcd->toc.FirstTrack;
	if (i <= vcd->toc.LastTrack) {
	    mp_msg(MSGT_OPEN, MSGL_INFO, "track %02d:  adr=%d  ctrl=%d"
		    "  %02d:%02d:%02d\n",
		    vcd->toc.TrackData[index].TrackNumber,
		    vcd->toc.TrackData[index].Adr,
		    vcd->toc.TrackData[index].Control,
		    vcd->toc.TrackData[index].Address[1],
		    vcd->toc.TrackData[index].Address[2],
		    vcd->toc.TrackData[index].Address[3]);
	}

	if (mp_msg_test(MSGT_IDENTIFY, MSGL_INFO)) {
	    if (i > vcd->toc.FirstTrack) {
		min = vcd->toc.TrackData[index].Address[1] - min;
		sec = vcd->toc.TrackData[index].Address[2] - sec;
		frame = vcd->toc.TrackData[index].Address[3] - frame;
		if (frame < 0) {
		    frame += 75;
		    sec--;
		}
		if (sec < 0) {
		    sec += 60;
		    min--;
		}
		mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VCD_TRACK_%d_MSF="
			"%02d:%02d:%02d\n", i - 1, min, sec, frame);
		min = vcd->toc.TrackData[index].Address[1];
		sec = vcd->toc.TrackData[index].Address[2];
		frame = vcd->toc.TrackData[index].Address[3];
	    }
	}
    }

    vcd->hd = hd;
    return vcd;
}

static int vcd_read(mp_vcd_priv_t* vcd, char *mem)
{
    DWORD dwBytesReturned;
    RAW_READ_INFO cdrom_raw;

    /* 2048 isn't a typo: it's the Windows way. */
    cdrom_raw.DiskOffset.QuadPart = (long long)(2048 * vcd->sect);
    cdrom_raw.SectorCount = 1;
    cdrom_raw.TrackMode = XAForm2;

    if (!DeviceIoControl(vcd->hd, IOCTL_CDROM_RAW_READ, &cdrom_raw,
		sizeof(RAW_READ_INFO), vcd->buf, sizeof(vcd->buf),
		&dwBytesReturned, NULL))
	return 0;

    vcd->sect++;
    memcpy(mem, &vcd->buf[VCD_SECTOR_OFFS], VCD_SECTOR_DATA);
    return VCD_SECTOR_DATA;
}

#endif /* MPLAYER_VCD_READ_WIN32_H */

/*
vim:noet:sw=4:cino=\:0,g0
*/
