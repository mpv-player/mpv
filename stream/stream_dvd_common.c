/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dvdread/ifo_types.h>
#ifdef __FreeBSD__
#include <sys/cdrio.h>
#endif

#ifdef __linux__
#include <linux/cdrom.h>
#include <scsi/sg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#endif

#include "mp_msg.h"
#include "help_mp.h"
#include "stream_dvd_common.h"
#include "libavutil/intreadwrite.h"

const char * const dvd_audio_stream_types[8] = { "ac3","unknown","mpeg1","mpeg2ext","lpcm","unknown","dts" };
const char * const dvd_audio_stream_channels[6] = { "mono", "stereo", "unknown", "unknown", "5.1/6.1", "5.1" };

int dvd_speed=0; /* 0 => don't touch speed */

void dvd_set_speed(char *device, unsigned speed)
{
#if defined(__linux__) && defined(SG_IO) && defined(GPCMD_SET_STREAMING)
  int fd;
  unsigned char buffer[28];
  unsigned char cmd[12];
  struct sg_io_hdr sghdr;
  struct stat st;

  memset(&st, 0, sizeof(st));

  if (stat(device, &st) == -1) return;

  if (!S_ISBLK(st.st_mode)) return; /* not a block device */

  switch (speed) {
  case 0: /* don't touch speed setting */
    return;
  case -1: /* restore default value */
    if (dvd_speed == 0) return; /* we haven't touched the speed setting */
    mp_msg(MSGT_OPEN, MSGL_INFO, MSGTR_DVDrestoreSpeed);
    break;
  default: /* limit to <speed> KB/s */
    // speed < 100 is multiple of DVD single speed (1350KB/s)
    if (speed < 100)
      speed *= 1350;
    mp_msg(MSGT_OPEN, MSGL_INFO, MSGTR_DVDlimitSpeed, speed);
    break;
  }

  memset(&sghdr, 0, sizeof(sghdr));
  sghdr.interface_id = 'S';
  sghdr.timeout = 5000;
  sghdr.dxfer_direction = SG_DXFER_TO_DEV;
  sghdr.dxfer_len = sizeof(buffer);
  sghdr.dxferp = buffer;
  sghdr.cmd_len = sizeof(cmd);
  sghdr.cmdp = cmd;

  memset(cmd, 0, sizeof(cmd));
  cmd[0] = GPCMD_SET_STREAMING;
  cmd[10] = sizeof(buffer);

  memset(buffer, 0, sizeof(buffer));
  /* first sector 0, last sector 0xffffffff */
  AV_WB32(buffer + 8, 0xffffffff);
  if (speed == -1)
    buffer[0] = 4; /* restore default */
  else {
    /* <speed> kilobyte */
    AV_WB32(buffer + 12, speed);
    AV_WB32(buffer + 20, speed);
  }
  /* 1 second */
  AV_WB16(buffer + 18, 1000);
  AV_WB16(buffer + 26, 1000);

  fd = open(device, O_RDWR | O_NONBLOCK);
  if (fd == -1) {
    mp_msg(MSGT_OPEN, MSGL_INFO, MSGTR_DVDspeedCantOpen);
    return;
  }

  if (ioctl(fd, SG_IO, &sghdr) < 0)
    mp_msg(MSGT_OPEN, MSGL_INFO, MSGTR_DVDlimitFail);
  else
    mp_msg(MSGT_OPEN, MSGL_INFO, MSGTR_DVDlimitOk);

  close(fd);
#endif
}

/**
\brief Converts DVD time structure to milliseconds.
\param *dev the DVD time structure to convert
\return returns the time in milliseconds
*/
int mp_dvdtimetomsec(dvd_time_t *dt)
{
  static int framerates[4] = {0, 2500, 0, 2997};
  int framerate = framerates[(dt->frame_u & 0xc0) >> 6];
  int msec = (((dt->hour & 0xf0) >> 3) * 5 + (dt->hour & 0x0f)) * 3600000;
  msec += (((dt->minute & 0xf0) >> 3) * 5 + (dt->minute & 0x0f)) * 60000;
  msec += (((dt->second & 0xf0) >> 3) * 5 + (dt->second & 0x0f)) * 1000;
  if(framerate > 0)
    msec += (((dt->frame_u & 0x30) >> 3) * 5 + (dt->frame_u & 0x0f)) * 100000 / framerate;
  return msec;
}
