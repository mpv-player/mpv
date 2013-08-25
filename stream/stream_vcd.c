/*
 * This file is part of MPlayer.
 *
 * Original author: Albeu
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

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <windows.h>
#endif

#include "mpvcore/mp_msg.h"
#include "stream.h"
#include "mpvcore/m_option.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#if !defined(__MINGW32__) && !defined(__CYGWIN__)
#include <sys/ioctl.h>
#endif
#include <errno.h>

#include "talloc.h"

#define VCD_SECTOR_SIZE 2352
#define VCD_SECTOR_OFFS 24
#define VCD_SECTOR_DATA 2324

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include "vcd_read_fbsd.h"
#elif defined(__APPLE__)
#include "vcd_read_darwin.h"
#elif defined(__MINGW32__) || defined(__CYGWIN__)
#include "vcd_read_win32.h"
#else
#include "vcd_read.h"
#endif

#ifndef vcd_close
#define vcd_close(priv) (close(((mp_vcd_priv_t*)priv)->fd))
#endif

extern char *cdrom_device;

static int fill_buffer(stream_t *s, char* buffer, int max_len){
  if(s->pos > s->end_pos) /// don't past end of current track
    return 0;
  if (max_len < VCD_SECTOR_DATA)
    return -1;
  return vcd_read(s->priv,buffer);
}

static int seek(stream_t *s,int64_t newpos) {
  vcd_set_msf(s->priv,newpos/VCD_SECTOR_DATA);
  return 1;
}

static void close_s(stream_t *stream) {
  vcd_close(stream->priv);
  free(stream->priv);
}

static int open_s(stream_t *stream,int mode)
{
  int ret,ret2,f,sect,tmp;
  mp_vcd_priv_t* vcd;
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
  int bsize = VCD_SECTOR_SIZE;
#endif
#if defined(__MINGW32__) || defined(__CYGWIN__)
  HANDLE hd;
  char device[20] = "\\\\.\\?:";
#endif

  if(mode != STREAM_READ
#if defined(__MINGW32__) || defined(__CYGWIN__)
      || GetVersion() > 0x80000000 // Win9x
#endif
      ) {
    return STREAM_UNSUPPORTED;
  }

  char *dev = stream->url;
  if (strncmp("vcd://", dev, 6) != 0)
    return STREAM_UNSUPPORTED;
  dev += 6;
  if (!dev[0]) {
    if(cdrom_device)
      dev = cdrom_device;
    else
      dev = DEFAULT_CDROM_DEVICE;
  }

#if defined(__MINGW32__) || defined(__CYGWIN__)
  device[4] = dev ? dev[0] : 0;
  /* open() can't be used for devices so do it the complicated way */
  hd = CreateFile(device, GENERIC_READ, FILE_SHARE_READ, NULL,
	  OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  f = _open_osfhandle((intptr_t)hd, _O_RDONLY);
#else
  f=open(dev,O_RDONLY);
#endif
  if(f<0){
    mp_tmsg(MSGT_OPEN,MSGL_ERR,"CD-ROM Device '%s' not found.\n",dev);
    return STREAM_ERROR;
  }

  vcd = vcd_read_toc(f);
  if(!vcd) {
    mp_msg(MSGT_OPEN,MSGL_ERR,"Failed to get cd toc\n");
    close(f);
    return STREAM_ERROR;
  }
  ret2=vcd_get_track_end(vcd,1);
  if(ret2<0){
      mp_msg(MSGT_OPEN, MSGL_ERR, "%s (get)\n",
             mp_gtext("Error selecting VCD track."));
    close(f);
    free(vcd);
    return STREAM_ERROR;
  }
  ret=vcd_seek_to_track(vcd,1);
  if(ret<0){
      mp_msg(MSGT_OPEN, MSGL_ERR, "%s (seek)\n",
             mp_gtext("Error selecting VCD track."));
    close(f);
    free(vcd);
    return STREAM_ERROR;
  }
  /* search forward up to at most 3 seconds to skip leading margin */
  sect = ret / VCD_SECTOR_DATA;
  for (tmp = sect; tmp < sect + 3 * 75; tmp++) {
    char mem[VCD_SECTOR_DATA];
    //since MPEG packs are block-aligned we stop discarding sectors if they are non-null
    if (vcd_read(vcd, mem) != VCD_SECTOR_DATA || mem[2] || mem[3])
      break;
  }
  mp_msg(MSGT_OPEN, MSGL_DBG2, "%d leading sectors skipped\n", tmp - sect);
  vcd_set_msf(vcd, tmp);
  ret = tmp * VCD_SECTOR_DATA;

  mp_msg(MSGT_OPEN,MSGL_V,"VCD start byte position: 0x%X  end: 0x%X\n",ret,ret2);

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
  if (ioctl (f, CDRIOCSETBLOCKSIZE, &bsize) == -1) {
    mp_msg(MSGT_OPEN,MSGL_WARN,"Error in CDRIOCSETBLOCKSIZE");
  }
#endif

  stream->sector_size = VCD_SECTOR_DATA;
  stream->start_pos=ret;
  stream->end_pos=ret2;
  stream->priv = vcd;

  stream->fill_buffer = fill_buffer;
  stream->seek = seek;
  stream->close = close_s;
  stream->demuxer = "lavf"; // mpegps ( or "vcd"?)

  return STREAM_OK;
}

const stream_info_t stream_info_vcd = {
    .name = "vcd",
    .open = open_s,
    .protocols = (const char*[]){ "vcd", NULL },
};
