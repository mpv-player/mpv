
#include "config.h"

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <windows.h>
#endif

#include "mp_msg.h"
#include "stream.h"
#include "help_mp.h"
#include "m_option.h"
#include "m_struct.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#if !defined(__MINGW32__) && !defined(__CYGWIN__)
#include <sys/ioctl.h>
#endif
#include <errno.h>

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include "vcd_read_fbsd.h" 
#elif defined(__APPLE__)
#include "vcd_read_darwin.h" 
#elif defined(__MINGW32__) || defined(__CYGWIN__)
#include "vcd_read_win32.h"
#else
#include "vcd_read.h"
#endif

#include "libmpdemux/demuxer.h"

extern char *cdrom_device;

static struct stream_priv_s {
  int track;
  char* device;
} stream_priv_dflts = {
  1,
  NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static const m_option_t stream_opts_fields[] = {
  { "track", ST_OFF(track), CONF_TYPE_INT, M_OPT_MIN, 1, 0, NULL },
  { "device", ST_OFF(device), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  /// For url parsing
  { "hostname", ST_OFF(track), CONF_TYPE_INT, M_OPT_MIN, 1, 0, NULL },
  { "filename", ST_OFF(device), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};
static const struct m_struct_st stream_opts = {
  "vcd",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

static int fill_buffer(stream_t *s, char* buffer, int max_len){
  if(s->pos > s->end_pos) /// don't past end of current track
    return 0;
  return vcd_read(s->priv,buffer);
}

static int seek(stream_t *s,off_t newpos) {
  s->pos = newpos;
  vcd_set_msf(s->priv,s->pos/VCD_SECTOR_DATA);
  return 1;
}

static void close_s(stream_t *stream) {
  free(stream->priv);
}

static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  struct stream_priv_s* p = (struct stream_priv_s*)opts;
  int ret,ret2,f,sect,tmp;
  mp_vcd_priv_t* vcd;
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
  int bsize = VCD_SECTOR_SIZE;
#endif
#if defined(__MINGW32__) || defined(__CYGWIN__)
  HANDLE hd;
  char device[] = "\\\\.\\?:";
#endif

  if(mode != STREAM_READ
#if defined(__MINGW32__) || defined(__CYGWIN__)
      || GetVersion() > 0x80000000 // Win9x
#endif
      ) {
    m_struct_free(&stream_opts,opts);
    return STREAM_UNSUPPORTED;
  }

  if (!p->device) {
    if(cdrom_device)
      p->device = strdup(cdrom_device);
    else
      p->device = strdup(DEFAULT_CDROM_DEVICE);
  }

#if defined(__MINGW32__) || defined(__CYGWIN__)
  device[4] = p->device[0];
  /* open() can't be used for devices so do it the complicated way */
  hd = CreateFile(device, GENERIC_READ, FILE_SHARE_READ, NULL,
	  OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  f = _open_osfhandle((long)hd, _O_RDONLY);
#else
  f=open(p->device,O_RDONLY);
#endif
  if(f<0){ 
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CdDevNotfound,p->device);
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }

  vcd = vcd_read_toc(f);
  if(!vcd) {
    mp_msg(MSGT_OPEN,MSGL_ERR,"Failed to get cd toc\n");
    close(f);
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }
  ret2=vcd_get_track_end(vcd,p->track);
  if(ret2<0){
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_ErrTrackSelect " (get)\n");
    close(f);
    free(vcd);
    m_struct_free(&stream_opts,opts);
    return STREAM_ERROR;
  }
  ret=vcd_seek_to_track(vcd,p->track);
  if(ret<0){
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_ErrTrackSelect " (seek)\n");
    close(f);
    free(vcd);
    m_struct_free(&stream_opts,opts);
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

  stream->fd = f;
  stream->type = STREAMTYPE_VCD;
  stream->sector_size = VCD_SECTOR_DATA;
  stream->start_pos=ret;
  stream->end_pos=ret2;
  stream->priv = vcd;

  stream->fill_buffer = fill_buffer;
  stream->seek = seek;
  stream->close = close_s;
  *file_format = DEMUXER_TYPE_MPEG_PS;

  m_struct_free(&stream_opts,opts);
  return STREAM_OK;
}

const stream_info_t stream_info_vcd = {
  "Video CD",
  "vcd",
  "Albeu",
  "based on the code from ???",
  open_s,
  { "vcd", NULL },
  &stream_opts,
  1 // Urls are an option string
};
