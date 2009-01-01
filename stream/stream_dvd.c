

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

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

#include <libgen.h>
#include <errno.h>

#define FIRST_AC3_AID 128
#define FIRST_DTS_AID 136
#define FIRST_MPG_AID 0
#define FIRST_PCM_AID 160

#include "stream.h"
#include "m_option.h"
#include "m_struct.h"

#include "stream_dvd.h"
#include "stream_dvd_common.h"
#include "libmpdemux/demuxer.h"
#include "libavutil/intreadwrite.h"

extern char* dvd_device;
static char* dvd_device_current;
int dvd_angle=1;
int dvd_speed=0; /* 0 => don't touch speed */

static void dvd_set_speed(char *device, unsigned speed)
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

#define	LIBDVDREAD_VERSION(maj,min,micro)	((maj)*10000 + (min)*100 + (micro))
/*
 * Try to autodetect the libdvd-0.9.0 library
 * (0.9.0 removed the <dvdread/dvd_udf.h> header, and moved the two defines
 * DVD_VIDEO_LB_LEN and MAX_UDF_FILE_NAME_LEN from it to
 * <dvdread/dvd_reader.h>)
 */
#ifndef DVDREAD_VERSION
#if defined(DVD_VIDEO_LB_LEN) && defined(MAX_UDF_FILE_NAME_LEN)
#define	DVDREAD_VERSION	LIBDVDREAD_VERSION(0,9,0)
#else
#define	DVDREAD_VERSION	LIBDVDREAD_VERSION(0,8,0)
#endif
#endif

const char * const dvd_audio_stream_types[8] = { "ac3","unknown","mpeg1","mpeg2ext","lpcm","unknown","dts" };
const char * const dvd_audio_stream_channels[6] = { "mono", "stereo", "unknown", "unknown", "5.1/6.1", "5.1" };


static struct stream_priv_s {
  int title;
  char* device;
} stream_priv_dflts = {
  1,
  NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static const m_option_t stream_opts_fields[] = {
  { "hostname", ST_OFF(title),  CONF_TYPE_INT, M_OPT_RANGE, 1, 99, NULL},
  { "filename", ST_OFF(device), CONF_TYPE_STRING, 0, 0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};
static const struct m_struct_st stream_opts = {
  "dvd",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

int dvd_parse_chapter_range(const m_option_t *conf, const char *range) {
  const char *s;
  char *t;
  if (!range)
    return M_OPT_MISSING_PARAM;
  s = range;
  dvd_chapter = 1;
  dvd_last_chapter = 0;
  if(*range && isdigit(*range)) {
    dvd_chapter = strtol(range, &s, 10);
    if(range == s) {
      mp_msg(MSGT_OPEN, MSGL_ERR, MSGTR_DVDinvalidChapterRange, range);
      return M_OPT_INVALID;
    }
  }
  if(*s == 0)
    return 0;
  else if(*s != '-') {
    mp_msg(MSGT_OPEN, MSGL_ERR, MSGTR_DVDinvalidChapterRange, range);
    return M_OPT_INVALID;
  }
  ++s;
  if(*s == 0)
      return 0;
  if(! isdigit(*s)) {
    mp_msg(MSGT_OPEN, MSGL_ERR, MSGTR_DVDinvalidChapterRange, range);
    return M_OPT_INVALID;
  }
  dvd_last_chapter = strtol(s, &t, 10);
  if (s == t || *t) {
    mp_msg(MSGT_OPEN, MSGL_ERR, MSGTR_DVDinvalidChapterRange, range);
    return M_OPT_INVALID;
  }
  return 0;
}

int dvd_chapter_from_cell(dvd_priv_t* dvd,int title,int cell)
{
  pgc_t * cur_pgc;
  ptt_info_t* ptt;
  int chapter = cell;
  int pgc_id,pgn;
  if(title < 0 || cell < 0){
    return 0;
  }
  /* for most DVD's chapter == cell */
  /* but there are more complecated cases... */
  if(chapter >= dvd->vmg_file->tt_srpt->title[title].nr_of_ptts) {
    chapter = dvd->vmg_file->tt_srpt->title[title].nr_of_ptts-1;
  }
  title = dvd->tt_srpt->title[title].vts_ttn-1;
  ptt = dvd->vts_file->vts_ptt_srpt->title[title].ptt;
  while(chapter >= 0) {
    pgc_id = ptt[chapter].pgcn;
    pgn = ptt[chapter].pgn;
    cur_pgc = dvd->vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
    if(cell >= cur_pgc->program_map[pgn-1]-1) {
      return chapter;
    }
    --chapter;
  }
  /* didn't find a chapter ??? */
  return chapter;
}

int dvd_lang_from_aid(stream_t *stream, int id) {
  dvd_priv_t *d;
  int i;
  if (!stream) return 0;
  d = stream->priv;
  if (!d) return 0;
  for(i=0;i<d->nr_of_channels;i++) {
    if(d->audio_streams[i].id==id)
      return d->audio_streams[i].language;
  }
  return 0;
}

int dvd_aid_from_lang(stream_t *stream, unsigned char* lang) {
  dvd_priv_t *d=stream->priv;
  int code,i;
  if(lang) {
    while(strlen(lang)>=2) {
      code=lang[1]|(lang[0]<<8);
      for(i=0;i<d->nr_of_channels;i++) {
        if(d->audio_streams[i].language==code) {
          mp_msg(MSGT_OPEN,MSGL_INFO,MSGTR_DVDaudioChannel,
          d->audio_streams[i].id, lang[0],lang[1]);
          return d->audio_streams[i].id;
        }
        //printf("%X != %X  (%c%c)\n",code,d->audio_streams[i].language,lang[0],lang[1]);
      }
      lang+=2; while (lang[0]==',' || lang[0]==' ') ++lang;
    }
    mp_msg(MSGT_OPEN,MSGL_WARN,MSGTR_DVDnoMatchingAudio);
  }
  return -1;
}

int dvd_number_of_subs(stream_t *stream) {
  int i;
  int maxid = -1;
  dvd_priv_t *d;
  if (!stream) return -1;
  d = stream->priv;
  if (!d) return -1;
  for (i = 0; i < d->nr_of_subtitles; i++)
    if (d->subtitles[i].id > maxid) maxid = d->subtitles[i].id;
  return maxid + 1;
}

int dvd_lang_from_sid(stream_t *stream, int id) {
  int i;
  dvd_priv_t *d;
  if (!stream) return 0;
  d = stream->priv;
  if (!d) return 0;
  for (i = 0; i < d->nr_of_subtitles; i++)
    if (d->subtitles[i].id == id && d->subtitles[i].language) return d->subtitles[i].language;
  return 0;
}

int dvd_sid_from_lang(stream_t *stream, unsigned char* lang) {
  dvd_priv_t *d=stream->priv;
  int code,i;
  while(lang && strlen(lang)>=2) {
    code=lang[1]|(lang[0]<<8);
    for(i=0;i<d->nr_of_subtitles;i++) {
      if(d->subtitles[i].language==code) {
        mp_msg(MSGT_OPEN,MSGL_INFO,MSGTR_DVDsubtitleChannel, i, lang[0],lang[1]);
        return d->subtitles[i].id;
      }
    }
    lang+=2; 
    while (lang[0]==',' || lang[0]==' ') ++lang;
  }
  mp_msg(MSGT_OPEN,MSGL_WARN,MSGTR_DVDnoMatchingSubtitle);
  return -1;
}

static int dvd_next_cell(dvd_priv_t *d) {
  int next_cell=d->cur_cell;

  mp_msg(MSGT_DVD,MSGL_DBG2, "dvd_next_cell: next1=0x%X  \n",next_cell);
  if( d->cur_pgc->cell_playback[ next_cell ].block_type == BLOCK_TYPE_ANGLE_BLOCK ) {
    while(next_cell<d->last_cell) {
      if( d->cur_pgc->cell_playback[next_cell].block_mode == BLOCK_MODE_LAST_CELL )
        break;
      ++next_cell;
    }
  }
  mp_msg(MSGT_DVD,MSGL_DBG2, "dvd_next_cell: next2=0x%X  \n",next_cell);

  ++next_cell;
  if(next_cell>=d->last_cell) 
    return -1; // EOF
  if(d->cur_pgc->cell_playback[next_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK ) {
    next_cell+=dvd_angle;
    if(next_cell>=d->last_cell) 
      return -1; // EOF
  }
  mp_msg(MSGT_DVD,MSGL_DBG2, "dvd_next_cell: next3=0x%X  \n",next_cell);
  return next_cell;
}

int dvd_read_sector(dvd_priv_t *d,unsigned char* data) {
  int len;

  if(d->packs_left==0) {
    /**
     * If we're not at the end of this cell, we can determine the next
     * VOBU to display using the VOBU_SRI information section of the
     * DSI.  Using this value correctly follows the current angle,
     * avoiding the doubled scenes in The Matrix, and makes our life
     * really happy.
     *
     * Otherwise, we set our next address past the end of this cell to
     * force the code above to go to the next cell in the program.
     */
    if(d->dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL) {
       d->cur_pack= d->dsi_pack.dsi_gi.nv_pck_lbn + ( d->dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
       mp_msg(MSGT_DVD,MSGL_DBG2, "Navi  new pos=0x%X  \n",d->cur_pack);
    } else {
      // end of cell! find next cell!
      mp_msg(MSGT_DVD,MSGL_V, "--- END OF CELL !!! ---\n");
      d->cur_pack=d->cell_last_pack+1;
    }
  }

read_next:
  if(d->cur_pack>d->cell_last_pack) {
    // end of cell!
    int next=dvd_next_cell(d);
    if(next>=0) {
      d->cur_cell=next;
      // if( d->cur_pgc->cell_playback[d->cur_cell].block_type 
      // == BLOCK_TYPE_ANGLE_BLOCK ) d->cur_cell+=dvd_angle;
      d->cur_pack = d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
      d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
      mp_msg(MSGT_DVD,MSGL_V, "DVD next cell: %d  pack: 0x%X-0x%X  \n",d->cur_cell,d->cur_pack,d->cell_last_pack);
    } else 
        return -1; // EOF
  }

  len = DVDReadBlocks(d->title, d->cur_pack, 1, data);
  // only == 0 should indicate an error, but some dvdread version are buggy when used with dvdcss
  if(len <= 0) return -1; //error

  if(data[38]==0 && data[39]==0 && data[40]==1 && data[41]==0xBF &&
    data[1024]==0 && data[1025]==0 && data[1026]==1 && data[1027]==0xBF) {
       // found a Navi packet!!!
#if DVDREAD_VERSION >= LIBDVDREAD_VERSION(0,9,0)
    navRead_DSI(&d->dsi_pack, &(data[ DSI_START_BYTE ]));
#else
    navRead_DSI(&d->dsi_pack, &(data[ DSI_START_BYTE ]), sizeof(dsi_t));
#endif
    if(d->cur_pack != d->dsi_pack.dsi_gi.nv_pck_lbn ) {
      mp_msg(MSGT_DVD,MSGL_V, "Invalid NAVI packet! lba=0x%X  navi=0x%X  \n",
        d->cur_pack,d->dsi_pack.dsi_gi.nv_pck_lbn);
    } else {
      // process!
      d->packs_left = d->dsi_pack.dsi_gi.vobu_ea;
      mp_msg(MSGT_DVD,MSGL_DBG2, "Found NAVI packet! lba=0x%X  len=%d  \n",d->cur_pack,d->packs_left);
      //navPrint_DSI(&d->dsi_pack);
      mp_msg(MSGT_DVD,MSGL_DBG3,"\r### CELL %d: Navi: %d/%d  IFO: %d/%d   \n",d->cur_cell,
        d->dsi_pack.dsi_gi.vobu_c_idn,d->dsi_pack.dsi_gi.vobu_vob_idn,
        d->cur_pgc->cell_position[d->cur_cell].cell_nr,
        d->cur_pgc->cell_position[d->cur_cell].vob_id_nr);

      if(d->angle_seek) {
        int i,skip=0;
#if defined(__GNUC__) && ( defined(__sparc__) || defined(hpux) )
        // workaround for a bug in the sparc/hpux version of gcc 2.95.X ... 3.2,
        // it generates incorrect code for unaligned access to a packed
        // structure member, resulting in an mplayer crash with a SIGBUS
        // signal.
        //
        // See also gcc problem report PR c/7847:
        // http://gcc.gnu.org/cgi-bin/gnatsweb.pl?database=gcc&cmd=view+audit-trail&pr=7847
        for(i=0;i<9;i++) {	// check if all values zero:
          __typeof__(d->dsi_pack.sml_agli.data[i].address) tmp_addr;
          memcpy(&tmp_addr,&d->dsi_pack.sml_agli.data[i].address,sizeof(tmp_addr));
          if((skip=tmp_addr)!=0) break;
        }
#else
        for(i=0;i<9;i++)	// check if all values zero:
          if((skip=d->dsi_pack.sml_agli.data[i].address)!=0) break;
#endif
        if(skip && skip!=0x7fffffff) {
          // sml_agli table has valid data (at least one non-zero):
         d->cur_pack=d->dsi_pack.dsi_gi.nv_pck_lbn+
         d->dsi_pack.sml_agli.data[dvd_angle].address;
         d->angle_seek=0;
         d->cur_pack--;
         mp_msg(MSGT_DVD,MSGL_V, "Angle-seek synced using sml_agli map!  new_lba=0x%X  \n",d->cur_pack);
        } else {
          // check if we're in the right cell, jump otherwise:
          if( (d->dsi_pack.dsi_gi.vobu_c_idn==d->cur_pgc->cell_position[d->cur_cell].cell_nr) &&
            (d->dsi_pack.dsi_gi.vobu_vob_idn==d->cur_pgc->cell_position[d->cur_cell].vob_id_nr) ){
            d->angle_seek=0;
            mp_msg(MSGT_DVD,MSGL_V, "Angle-seek synced by cell/vob IDN search!  \n");
          } else {
            // wrong angle, skip this vobu:
            d->cur_pack=d->dsi_pack.dsi_gi.nv_pck_lbn+
            d->dsi_pack.dsi_gi.vobu_ea;
            d->angle_seek=2; // DEBUG
          }
        }
      }
    }
    ++d->cur_pack;
    goto read_next;
  }

  ++d->cur_pack;
  if(d->packs_left>=0) --d->packs_left;

  if(d->angle_seek) {
    if(d->angle_seek==2) mp_msg(MSGT_DVD,MSGL_V, "!!! warning! reading packet while angle_seek !!!\n");
    goto read_next; // searching for Navi packet
  }

  return d->cur_pack-1;
}

void dvd_seek(dvd_priv_t *d,int pos) {
  d->packs_left=-1;
  d->cur_pack=pos;

  // check if we stay in current cell (speedup things, and avoid angle skip)
  if(d->cur_pack>d->cell_last_pack ||
     d->cur_pack<d->cur_pgc->cell_playback[ d->cur_cell ].first_sector) {

    // ok, cell change, find the right cell!
    d->cur_cell=0;
    if(d->cur_pgc->cell_playback[d->cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK )
      d->cur_cell+=dvd_angle;

    while(1) {
      int next;
      d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
      if(d->cur_pack<d->cur_pgc->cell_playback[ d->cur_cell ].first_sector) {
        d->cur_pack=d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
        break;
      }
      if(d->cur_pack<=d->cell_last_pack) break; // ok, we find it! :)
      next=dvd_next_cell(d);
      if(next<0) {
        //d->cur_pack=d->cell_last_pack+1;
        break; // we're after the last cell
      }
      d->cur_cell=next;
    }
  }

  mp_msg(MSGT_DVD,MSGL_V, "DVD Seek! lba=0x%X  cell=%d  packs: 0x%X-0x%X  \n",
    d->cur_pack,d->cur_cell,d->cur_pgc->cell_playback[ d->cur_cell ].first_sector,d->cell_last_pack);

  // if we're in interleaved multi-angle cell, find the right angle chain!
  // (read Navi block, and use the seamless angle jump table)
  d->angle_seek=1;
}

void dvd_close(dvd_priv_t *d) {
  ifoClose(d->vts_file);
  ifoClose(d->vmg_file);
  DVDCloseFile(d->title);
  DVDClose(d->dvd);
  dvd_chapter = 1;
  dvd_last_chapter = 0;
  dvd_set_speed(dvd_device_current, -1); /* -1 => restore default */
}

static int fill_buffer(stream_t *s, char *but, int len)
{
  if(s->type == STREAMTYPE_DVD) {
    off_t pos=dvd_read_sector(s->priv,s->buffer);
    if(pos>=0) {
      len=2048; // full sector
      s->pos=2048*pos-len;
    } else len=-1; // error
  }
  return len;
}

static int seek(stream_t *s, off_t newpos) {
  s->pos=newpos; // real seek
  dvd_seek(s->priv,s->pos/2048);
  return 1;
}

static void stream_dvd_close(stream_t *s) {
  dvd_close(s->priv);
}

static int mp_get_titleset_length(ifo_handle_t *vts_file, tt_srpt_t *tt_srpt, int title_no)
{
    int vts_ttn;  ///< title number within video title set
    int pgc_no;   ///< program chain number
    int msec;     ///< time length in milliseconds

    msec=0;
    if(!vts_file || !tt_srpt)
        return 0;

    if(vts_file->vtsi_mat && vts_file->vts_pgcit)
    {
            vts_ttn = tt_srpt->title[title_no].vts_ttn - 1;
            pgc_no = vts_file->vts_ptt_srpt->title[vts_ttn].ptt[0].pgcn - 1;
            msec = mp_dvdtimetomsec(&vts_file->vts_pgcit->pgci_srp[pgc_no].pgc->playback_time);
    }
    return msec;
}


static int mp_describe_titleset(dvd_reader_t *dvd, tt_srpt_t *tt_srpt, int vts_no)
{
    ifo_handle_t *vts_file;
    int title_no, msec=0;

    vts_file = ifoOpen(dvd, vts_no);
    if(!vts_file)
        return 0;

    if(!vts_file->vtsi_mat || !vts_file->vts_pgcit)
    {
        ifoClose(vts_file);
        return 0;
    }

    for(title_no = 0; title_no < tt_srpt->nr_of_srpts; title_no++)
    {
        if (tt_srpt->title[title_no].title_set_nr != vts_no)
            continue;
        msec = mp_get_titleset_length(vts_file, tt_srpt, title_no);
        mp_msg(MSGT_IDENTIFY, MSGL_V, "ID_DVD_TITLE_%d_LENGTH=%d.%03d\n", title_no + 1, msec / 1000, msec % 1000);
    }
    ifoClose(vts_file);
    return 1;
}

static int seek_to_chapter(stream_t *stream, ifo_handle_t *vts_file, tt_srpt_t *tt_srpt, int title_no, int chapter)
{
    int cell;
    ptt_info_t ptt;
    pgc_t *pgc;
    off_t pos;

    if(!vts_file || !tt_srpt)
       return 0;

    if(title_no < 0 || title_no >= tt_srpt->nr_of_srpts)
       return 0;

    // map global title to vts title
    title_no = tt_srpt->title[title_no].vts_ttn - 1;

    if(title_no < 0 || title_no >= vts_file->vts_ptt_srpt->nr_of_srpts)
       return 0;

    if(chapter < 0 || chapter > vts_file->vts_ptt_srpt->title[title_no].nr_of_ptts-1) //no such chapter
       return 0;

    ptt = vts_file->vts_ptt_srpt->title[title_no].ptt[chapter];
    pgc = vts_file->vts_pgcit->pgci_srp[ptt.pgcn-1].pgc;

    cell = pgc->program_map[ptt.pgn - 1] - 1;
    pos = (off_t) pgc->cell_playback[cell].first_sector * 2048;
    mp_msg(MSGT_OPEN,MSGL_V,"\r\nSTREAM_DVD, seeked to chapter: %d, cell: %u, pos: %"PRIu64"\n",
        chapter, pgc->cell_playback[cell].first_sector, pos);
    stream_seek(stream, pos);

    return chapter;
}

static void list_chapters(pgc_t *pgc)
{
    unsigned int i, cell;
    unsigned int t=0, t2=0;

    if(pgc->nr_of_programs < 2)
       return;

    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "CHAPTERS: ");
    for(i=0; i<pgc->nr_of_programs; i++)
    {
        cell = pgc->program_map[i]; //here the cell is 1-based
        t2 = t/1000;
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "%02d:%02d:%02d,", t2/3600, (t2/60)%60, t2%60);
        while(i+1<pgc->nr_of_programs && cell < pgc->program_map[i+1]) {
            if(!(pgc->cell_playback[cell-1].block_type == BLOCK_TYPE_ANGLE_BLOCK &&
                 pgc->cell_playback[cell-1].block_mode != BLOCK_MODE_FIRST_CELL)
            )
                t += mp_dvdtimetomsec(&pgc->cell_playback[cell-1].playback_time);
            cell++;
        }
    }
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "\n");
}

static double dvd_get_current_time(stream_t *stream, int cell)
{
    int i, tm;
    dvd_priv_t *d = stream->priv;

    tm=0;
    if(!cell) cell=d->cur_cell;
    for(i=0; i<d->cur_cell; i++) {
        if(d->cur_pgc->cell_playback[i].block_type == BLOCK_TYPE_ANGLE_BLOCK &&
           d->cur_pgc->cell_playback[i].block_mode != BLOCK_MODE_FIRST_CELL
        )
          continue;
        tm += d->cell_times_table[i];
    }
    tm += mp_dvdtimetomsec(&d->dsi_pack.dsi_gi.c_eltm);

    return (double)tm/1000.0;
}

static int dvd_seek_to_time(stream_t *stream, ifo_handle_t *vts_file, double sec)
{
    unsigned int i, j, k, timeunit, ac_time, tmap_sector=0, cell_sector=0, vobu_sector=0;
    int t=0;
    double tm, duration;
    off_t pos = -1;
    dvd_priv_t *d = stream->priv;
    vts_tmapt_t *vts_tmapt = vts_file->vts_tmapt;

    if(!vts_file->vts_tmapt || sec < 0)
        return 0;

    duration = (double) mp_get_titleset_length(d->vts_file, d->tt_srpt, d->cur_title-1) / 1000.0f;
    if(sec > duration)
      return 0;

    i=d->cur_pgc_idx;
    timeunit = vts_tmapt->tmap[i].tmu;
    for(j = 0; j < vts_tmapt->tmap[i].nr_of_entries; j++) {
      ac_time = timeunit * (j + 1);
      if(ac_time >= sec)
        break;
      tmap_sector = vts_tmapt->tmap[i].map_ent[j] & 0x7fffffff;
    }
    //search enclosing cell
    for(i=0; i<d->cur_pgc->nr_of_cells; i++) {
      if(tmap_sector >= d->cur_pgc->cell_playback[i].first_sector && tmap_sector <= d->cur_pgc->cell_playback[i].last_sector) {
        cell_sector = d->cur_pgc->cell_playback[i].first_sector;
        break;
      }
    }

    pos = ((off_t)cell_sector)<<11;
    stream_seek(stream, pos);
    do {
      stream_skip(stream, 2048);
      t = mp_dvdtimetomsec(&d->dsi_pack.dsi_gi.c_eltm);
    } while(!t);
    tm = dvd_get_current_time(stream, 0);

    pos = ((off_t)tmap_sector)<<11;
    stream_seek(stream, pos);
    //now get current time in terms of the cell+cell time offset
    memset(&d->dsi_pack.dsi_gi.c_eltm, 0, sizeof(dvd_time_t));
    while(tm <= sec) {
        if(!stream_skip(stream, 2048))
          break;
        tm = dvd_get_current_time(stream, 0);
    };
    tmap_sector = stream->pos >> 11;

    //search closest VOBU sector
    k=(vts_file->vts_vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE)/4; //entries in the vobu admap
    for(i=1; i<k; i++) {
      if(vts_file->vts_vobu_admap->vobu_start_sectors[i] > tmap_sector)
        break;
    }
    vobu_sector = vts_file->vts_vobu_admap->vobu_start_sectors[i-1];
    pos = ((off_t)vobu_sector) << 11;
    stream_seek(stream, pos);

    return 1;
}

static int control(stream_t *stream,int cmd,void* arg) 
{
    dvd_priv_t *d = stream->priv;
    switch(cmd) 
    {
        case STREAM_CTRL_GET_TIME_LENGTH:
        {
            *((double *)arg) = (double) mp_get_titleset_length(d->vts_file, d->tt_srpt, d->cur_title-1)/1000.0;
            return 1;
        }
        case STREAM_CTRL_GET_NUM_CHAPTERS:
        {
            if(! d->cur_pgc->nr_of_programs) return STREAM_UNSUPPORTED;
            *((unsigned int *)arg) = d->cur_pgc->nr_of_programs; 
            return 1;
        }
        case STREAM_CTRL_SEEK_TO_CHAPTER:
        {
            int r;
            r = seek_to_chapter(stream, d->vts_file, d->tt_srpt, d->cur_title-1, *((unsigned int *)arg));
            if(! r) return STREAM_UNSUPPORTED;

            return 1;
        }
        case STREAM_CTRL_GET_CURRENT_CHAPTER:
        {
            *((unsigned int *)arg) = dvd_chapter_from_cell(d, d->cur_title-1, d->cur_cell);
            return 1;
        }
        case STREAM_CTRL_GET_CURRENT_TIME:
        {
            double tm;
            tm = dvd_get_current_time(stream, 0);
            if(tm != -1) {
              *((double *)arg) = tm;
              return 1;
            }
            break;
        }
        case STREAM_CTRL_SEEK_TO_TIME:
        {
            if(dvd_seek_to_time(stream, d->vts_file, *((double*)arg)))
              return 1;
            break;
        }
        case STREAM_CTRL_GET_ASPECT_RATIO:
        {
            *((double *)arg) = !d->vts_file->vtsi_mat->vts_video_attr.display_aspect_ratio ? 4.0/3.0 : 16.0/9.0;
            return 1;
        }
        case STREAM_CTRL_GET_NUM_ANGLES:
        {
            *((int *)arg) = d->vmg_file->tt_srpt->title[dvd_title].nr_of_angles;
            return 1;
        }
        case STREAM_CTRL_GET_ANGLE:
        {
            *((int *)arg) = dvd_angle+1;
            return 1;
        }
        case STREAM_CTRL_SET_ANGLE:
        {
            int ang = *((int *)arg);
            if(ang>d->vmg_file->tt_srpt->title[dvd_title].nr_of_angles || ang<=0)
                break;
            dvd_angle = ang - 1;
            d->angle_seek = 1;
            return 1;
        }
    }
    return STREAM_UNSUPPORTED;
}


static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  struct stream_priv_s* p = (struct stream_priv_s*)opts;
  int k;

  mp_msg(MSGT_OPEN,MSGL_V,"URL: %s\n", stream->url);
  dvd_title = p->title;
  if(1){
    //int ret,ret2;
    dvd_priv_t *d;
    int ttn,pgc_id,pgn;
    dvd_reader_t *dvd;
    dvd_file_t *title;
    ifo_handle_t *vmg_file;
    tt_srpt_t *tt_srpt;
    ifo_handle_t *vts_file;
    pgc_t *pgc;
    /**
     * Open the disc.
     */
    if(p->device)
      dvd_device_current = p->device;
    else if(dvd_device)
      dvd_device_current = dvd_device;
    else
      dvd_device_current = DEFAULT_DVD_DEVICE;
    dvd_set_speed(dvd_device_current, dvd_speed);
#if defined(__APPLE__) || defined(__DARWIN__)
    /* Dynamic DVD drive selection on Darwin */
    if(!strcmp(dvd_device_current, "/dev/rdiskN")) {
      int i;
      size_t len = strlen(dvd_device_current)+1;
      char *temp_device = malloc(len);

      for (i = 1; i < 10; i++) {
        snprintf(temp_device, len, "/dev/rdisk%d", i);
        dvd = DVDOpen(temp_device);
        if(!dvd) {
          mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CantOpenDVD,temp_device, strerror(errno));
        } else {
#if DVDREAD_VERSION <= LIBDVDREAD_VERSION(0,9,4)
          dvd_file_t *dvdfile = DVDOpenFile(dvd,dvd_title,DVD_READ_INFO_FILE);
          if(!dvdfile) {
            mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CantOpenDVD,temp_device, strerror(errno));
            DVDClose(dvd);
            continue;
          }
          DVDCloseFile(dvdfile);
#endif
          break;
        }
      }
      free(temp_device);

      if(!dvd) {
        m_struct_free(&stream_opts,opts);
        return STREAM_UNSUPPORTED;
      }
    } else
#endif /* defined(__APPLE__) || defined(__DARWIN__) */
    {
        dvd = DVDOpen(dvd_device_current);
        if(!dvd) {
          mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CantOpenDVD,dvd_device_current, strerror(errno));
          m_struct_free(&stream_opts,opts);
          return STREAM_UNSUPPORTED;
        }
    }

    mp_msg(MSGT_OPEN,MSGL_V,"Reading disc structure, please wait...\n");

    /**
     * Load the video manager to find out the information about the titles on
     * this disc.
     */
    vmg_file = ifoOpen(dvd, 0);
    if(!vmg_file) {
      mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_DVDnoVMG);
      DVDClose( dvd );
      m_struct_free(&stream_opts,opts);
      return STREAM_UNSUPPORTED;
    }
    tt_srpt = vmg_file->tt_srpt;
    if (mp_msg_test(MSGT_IDENTIFY, MSGL_INFO))
    {
      int title_no; ///< title number
      mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_DVD_TITLES=%d\n", tt_srpt->nr_of_srpts);
      for (title_no = 0; title_no < tt_srpt->nr_of_srpts; title_no++)
      {
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_DVD_TITLE_%d_CHAPTERS=%d\n", title_no + 1, tt_srpt->title[title_no].nr_of_ptts);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_DVD_TITLE_%d_ANGLES=%d\n", title_no + 1, tt_srpt->title[title_no].nr_of_angles);
      }
    }
    if (mp_msg_test(MSGT_IDENTIFY, MSGL_V))
    {
      char volid[32];
      unsigned char discid [16]; ///< disk ID, a 128 bit MD5 sum
      int vts_no;   ///< video title set number
      for (vts_no = 1; vts_no <= vmg_file->vts_atrt->nr_of_vtss; vts_no++)
        mp_describe_titleset(dvd, tt_srpt, vts_no);
      if (DVDDiscID(dvd, discid) >= 0)
      {
        int i;
        mp_msg(MSGT_IDENTIFY, MSGL_V, "ID_DVD_DISC_ID=");
        for (i = 0; i < 16; i ++)
          mp_msg(MSGT_IDENTIFY, MSGL_V, "%02X", discid[i]);
        mp_msg(MSGT_IDENTIFY, MSGL_V, "\n");
      }
      if (DVDUDFVolumeInfo(dvd, volid, sizeof(volid), NULL, 0) >= 0 || DVDISOVolumeInfo(dvd, volid, sizeof(volid), NULL, 0) >= 0)
        mp_msg(MSGT_IDENTIFY, MSGL_V, "ID_DVD_VOLUME_ID=%s\n", volid);
    }
    /**
     * Make sure our title number is valid.
     */
    mp_msg(MSGT_OPEN,MSGL_STATUS, MSGTR_DVDnumTitles, tt_srpt->nr_of_srpts );
    if(dvd_title < 1 || dvd_title > tt_srpt->nr_of_srpts) {
      mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_DVDinvalidTitle, dvd_title);
      ifoClose( vmg_file );
      DVDClose( dvd );
      m_struct_free(&stream_opts,opts);
      return STREAM_UNSUPPORTED;
    }
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_DVD_CURRENT_TITLE=%d\n", dvd_title);
    --dvd_title; // remap 1.. -> 0..
    /**
     * Make sure the angle number is valid for this title.
     */
    mp_msg(MSGT_OPEN,MSGL_STATUS, MSGTR_DVDnumAngles, tt_srpt->title[dvd_title].nr_of_angles);
    if(dvd_angle<1 || dvd_angle>tt_srpt->title[dvd_title].nr_of_angles) {
      mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_DVDinvalidAngle, dvd_angle);
      goto fail;
    }
    --dvd_angle; // remap 1.. -> 0..

    ttn = tt_srpt->title[dvd_title].vts_ttn - 1;
    /**
     * Load the VTS information for the title set our title is in.
     */
    vts_file = ifoOpen( dvd, tt_srpt->title[dvd_title].title_set_nr );
    if(!vts_file) {
      mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_DVDnoIFO, tt_srpt->title[dvd_title].title_set_nr );
      goto fail;
    }
    /**
     * We've got enough info, time to open the title set data.
     */
    title = DVDOpenFile(dvd, tt_srpt->title[dvd_title].title_set_nr, DVD_READ_TITLE_VOBS);
    if(!title) {
      mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_DVDnoVOBs, tt_srpt->title[dvd_title].title_set_nr);
      ifoClose( vts_file );
      goto fail;
    }

    mp_msg(MSGT_OPEN,MSGL_V, "DVD successfully opened.\n");
    // store data
    d=malloc(sizeof(dvd_priv_t)); memset(d,0,sizeof(dvd_priv_t));
    d->dvd=dvd;
    d->title=title;
    d->vmg_file=vmg_file;
    d->tt_srpt=tt_srpt;
    d->vts_file=vts_file;
    d->cur_title = dvd_title+1;

    pgc = vts_file->vts_pgcit ? vts_file->vts_pgcit->pgci_srp[ttn].pgc : NULL;
    /**
     * Check number of audio channels and types
     */
    {
      d->nr_of_channels=0;
      if(vts_file->vts_pgcit) {
        int i;
        for(i=0;i<8;i++)
          if(pgc->audio_control[i] & 0x8000) {
            audio_attr_t * audio = &vts_file->vtsi_mat->vts_audio_attr[i];
            int language = 0;
            char tmp[] = "unknown";
            stream_language_t *audio_stream = &d->audio_streams[d->nr_of_channels];

            if(audio->lang_type == 1) {
              language=audio->lang_code;
              tmp[0]=language>>8;
              tmp[1]=language&0xff;
              tmp[2]=0;
            }

            audio_stream->language=language;
            audio_stream->id=pgc->audio_control[i] >> 8 & 7;
            switch(audio->audio_format) {
              case 0: // ac3
                audio_stream->id+=FIRST_AC3_AID;
                break;
              case 6: // dts
                audio_stream->id+=FIRST_DTS_AID;
                break;
              case 2: // mpeg layer 1/2/3
              case 3: // mpeg2 ext
                audio_stream->id+=FIRST_MPG_AID;
                break;
              case 4: // lpcm
                audio_stream->id+=FIRST_PCM_AID;
                break;
           }

           audio_stream->type=audio->audio_format;
           // Pontscho: to my mind, tha channels:
           //  1 - stereo
           //  5 - 5.1
           audio_stream->channels=audio->channels;
           mp_msg(MSGT_OPEN,MSGL_STATUS,MSGTR_DVDaudioStreamInfo,
             d->nr_of_channels,
             dvd_audio_stream_types[ audio->audio_format ],
             dvd_audio_stream_channels[ audio->channels ],
             tmp,
             audio_stream->id
           );
           mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_ID=%d\n", audio_stream->id);
           if(language && tmp[0])
             mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AID_%d_LANG=%s\n", audio_stream->id, tmp);

           d->nr_of_channels++;
         }
      }
      mp_msg(MSGT_OPEN,MSGL_STATUS,MSGTR_DVDnumAudioChannels,d->nr_of_channels );
    }

    /**
     * Check number of subtitles and language
     */
    {
      int i;

      d->nr_of_subtitles=0;
      for(i=0;i<32;i++)
      if(pgc->subp_control[i] & 0x80000000) {
        subp_attr_t * subtitle = &vts_file->vtsi_mat->vts_subp_attr[i];
        video_attr_t *video = &vts_file->vtsi_mat->vts_video_attr;
        int language = 0;
        char tmp[] = "unknown";
        stream_language_t *sub_stream = &d->subtitles[d->nr_of_subtitles];

        if(subtitle->type == 1) {
          language=subtitle->lang_code;
          tmp[0]=language>>8;
          tmp[1]=language&0xff;
          tmp[2]=0;
        }

        sub_stream->language=language;
        sub_stream->id=d->nr_of_subtitles;
        if(video->display_aspect_ratio == 0) /* 4:3 */
          sub_stream->id = pgc->subp_control[i] >> 24 & 31;
        else if(video->display_aspect_ratio == 3) /* 16:9 */
          sub_stream->id = pgc->subp_control[i] >> 8 & 31;

        mp_msg(MSGT_OPEN,MSGL_STATUS,MSGTR_DVDsubtitleLanguage, sub_stream->id, tmp);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SUBTITLE_ID=%d\n", sub_stream->id);
        if(language && tmp[0])
          mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SID_%d_LANG=%s\n", sub_stream->id, tmp);
        d->nr_of_subtitles++;
      }
      mp_msg(MSGT_OPEN,MSGL_STATUS,MSGTR_DVDnumSubtitles,d->nr_of_subtitles);
    }

    /**
     * Determine which program chain we want to watch.  This is based on the
     * chapter number.
     */
    pgc_id = vts_file->vts_ptt_srpt->title[ttn].ptt[0].pgcn; // local
    pgn  = vts_file->vts_ptt_srpt->title[ttn].ptt[0].pgn;  // local
    d->cur_pgc_idx = pgc_id-1;
    d->cur_pgc = vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
    d->cur_cell = d->cur_pgc->program_map[pgn-1] - 1; // start playback here
    d->packs_left=-1;      // for Navi stuff
    d->angle_seek=0;
      d->last_cell=d->cur_pgc->nr_of_cells;

    if(d->cur_pgc->cell_playback[d->cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK ) 
      d->cur_cell+=dvd_angle;
    d->cur_pack = d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
    d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
    mp_msg(MSGT_DVD,MSGL_V, "DVD start cell: %d  pack: 0x%X-0x%X  \n",d->cur_cell,d->cur_pack,d->cell_last_pack);

    //assign cell_times_table
    d->cell_times_table = malloc(sizeof(unsigned int) * d->cur_pgc->nr_of_cells);
    if(d->cell_times_table == NULL)
      return STREAM_UNSUPPORTED;
    for(k=0; k<d->cur_pgc->nr_of_cells; k++)
      d->cell_times_table[k] = mp_dvdtimetomsec(&d->cur_pgc->cell_playback[k].playback_time);
    list_chapters(d->cur_pgc);

    // ... (unimplemented)
    //    return NULL;
    stream->type = STREAMTYPE_DVD;
    stream->sector_size = 2048;
    stream->flags = STREAM_READ | STREAM_SEEK;
    stream->fill_buffer = fill_buffer;
    stream->seek = seek;
    stream->control = control;
    stream->close = stream_dvd_close;
    stream->start_pos = (off_t)d->cur_pack*2048;
    stream->end_pos = (off_t)(d->cur_pgc->cell_playback[d->last_cell-1].last_sector)*2048;
    *file_format = DEMUXER_TYPE_MPEG_PS;
    mp_msg(MSGT_DVD,MSGL_V,"DVD start=%d end=%d  \n",d->cur_pack,d->cur_pgc->cell_playback[d->last_cell-1].last_sector);
    stream->priv = (void*)d;
    return STREAM_OK;

fail:
      ifoClose(vmg_file);
      DVDClose(dvd);
      m_struct_free(&stream_opts, opts);
      return STREAM_UNSUPPORTED;
  }
  mp_msg(MSGT_DVD,MSGL_ERR,MSGTR_NoDVDSupport);
  m_struct_free(&stream_opts,opts);
  return STREAM_UNSUPPORTED;
}

static int ifo_stream_open (stream_t *stream, int mode, void *opts, int *file_format)
{
    char *ext;
    char* filename;
    struct stream_priv_s *spriv;

    ext = strrchr (stream->url, '.');
    if (!ext || strcasecmp (ext + 1, "ifo"))
        return STREAM_UNSUPPORTED;

    mp_msg(MSGT_DVD, MSGL_INFO, ".IFO detected. Redirecting to dvd://\n");

    filename = strdup(basename(stream->url));

    spriv=calloc(1, sizeof(struct stream_priv_s));
    spriv->device = strdup(dirname(stream->url));
    if(!strncasecmp(filename,"vts_",4))
    {
        if(sscanf(filename+3, "_%02d_", &spriv->title)!=1)
            spriv->title=1;
    }else
        spriv->title=1;

    free(filename);
    free(stream->url);
    stream->url=strdup("dvd://");

    return open_s(stream, mode, spriv, file_format);
}

const stream_info_t stream_info_dvd = {
  "DVD stream",
  "null",
  "",
  "",
  open_s,
  { "dvd", NULL },
  &stream_opts,
  1 // Urls are an option string
};

const stream_info_t stream_info_ifo = {
  "DVD IFO input",
  "ifo",
  "Benjamin Zores",
  "Mostly used to play DVDs on disk through OSD Menu",
  ifo_stream_open,
  { "file", "", NULL },
  NULL,
  0
};
