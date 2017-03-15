/*
 * Original author: Benjamin Zores
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <libgen.h>
#include <errno.h>
#include <stdint.h>

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>

#include "osdep/io.h"

#include "config.h"
#include "mpv_talloc.h"
#include "common/common.h"
#include "common/msg.h"

#define FIRST_AC3_AID 128
#define FIRST_DTS_AID 136
#define FIRST_MPG_AID 0
#define FIRST_PCM_AID 160

#include "stream.h"
#include "options/m_config.h"
#include "options/options.h"
#include "options/path.h"

#include "stream_dvd_common.h"

#define LIBDVDREAD_VERSION(maj,min,micro)       ((maj)*10000 + (min)*100 + (micro))
/*
 * Try to autodetect the libdvd-0.9.0 library
 * (0.9.0 removed the <dvdread/dvd_udf.h> header, and moved the two defines
 * DVD_VIDEO_LB_LEN and MAX_UDF_FILE_NAME_LEN from it to
 * <dvdread/dvd_reader.h>)
 */
#ifndef DVDREAD_VERSION
#if defined(DVD_VIDEO_LB_LEN) && defined(MAX_UDF_FILE_NAME_LEN)
#define DVDREAD_VERSION LIBDVDREAD_VERSION(0,9,0)
#else
#define DVDREAD_VERSION LIBDVDREAD_VERSION(0,8,0)
#endif
#endif

typedef struct {
    int id; // 0 - 31 mpeg; 128 - 159 ac3; 160 - 191 pcm
    int language;
    int type;
    int channels;
} stream_language_t;

typedef struct {
  dvd_reader_t *dvd;
  dvd_file_t *title;
  ifo_handle_t *vmg_file;
  tt_srpt_t *tt_srpt;
  ifo_handle_t *vts_file;
  vts_ptt_srpt_t *vts_ptt_srpt;
  pgc_t *cur_pgc;
//
  int cur_title;
  int cur_cell;
  int last_cell;
  int cur_pack;
  int cell_last_pack;
  int cur_pgc_idx;
// Navi:
  int packs_left;
  dsi_t dsi_pack;
  int angle_seek;
  unsigned int *cell_times_table;
// audio datas
  int nr_of_channels;
  stream_language_t audio_streams[32];
// subtitles
  int nr_of_subtitles;
  stream_language_t subtitles[32];

  int dvd_angle;
  char *dvd_device_current;
  int dvd_speed;
  int dvd_title;

  int cfg_title;
  char *cfg_device;
} dvd_priv_t;

static int dvd_lang_from_aid(stream_t *stream, int id) {
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

static int dvd_number_of_subs(stream_t *stream) {
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

static int dvd_lang_from_sid(stream_t *stream, int id) {
  int i;
  dvd_priv_t *d;
  if (!stream) return 0;
  d = stream->priv;
  if (!d) return 0;
  for (i = 0; i < d->nr_of_subtitles; i++)
    if (d->subtitles[i].id == id && d->subtitles[i].language) return d->subtitles[i].language;
  return 0;
}

static int dvd_next_cell(stream_t *stream, dvd_priv_t *d) {
  int next_cell=d->cur_cell;

  MP_DBG(stream, "dvd_next_cell: next1=0x%X  \n",next_cell);
  if( d->cur_pgc->cell_playback[ next_cell ].block_type == BLOCK_TYPE_ANGLE_BLOCK ) {
    while(next_cell<d->last_cell) {
      if( d->cur_pgc->cell_playback[next_cell].block_mode == BLOCK_MODE_LAST_CELL )
        break;
      ++next_cell;
    }
  }
  MP_DBG(stream, "dvd_next_cell: next2=0x%X  \n",next_cell);

  ++next_cell;
  if(next_cell>=d->last_cell)
    return -1; // EOF
  if(d->cur_pgc->cell_playback[next_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK ) {
    next_cell+=d->dvd_angle-1;
    if(next_cell>=d->last_cell)
      return -1; // EOF
  }
  MP_DBG(stream, "dvd_next_cell: next3=0x%X  \n",next_cell);
  return next_cell;
}

static int dvd_read_sector(stream_t *stream, dvd_priv_t *d, unsigned char *data)
{
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
       MP_DBG(stream, "Navi  new pos=0x%X  \n",d->cur_pack);
    } else {
      // end of cell! find next cell!
      MP_VERBOSE(stream, "--- END OF CELL !!! ---\n");
      d->cur_pack=d->cell_last_pack+1;
    }
  }

read_next:
  if(d->cur_pack>d->cell_last_pack) {
    // end of cell!
    int next=dvd_next_cell(stream, d);
    if(next>=0) {
      d->cur_cell=next;
      // if( d->cur_pgc->cell_playback[d->cur_cell].block_type
      // == BLOCK_TYPE_ANGLE_BLOCK ) d->cur_cell+=dvd_angle-1;
      d->cur_pack = d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
      d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
      MP_VERBOSE(stream, "DVD next cell: %d  pack: 0x%X-0x%X  \n",d->cur_cell,d->cur_pack,d->cell_last_pack);
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
      MP_VERBOSE(stream, "Invalid NAVI packet! lba=0x%X  navi=0x%X  \n",
        d->cur_pack,d->dsi_pack.dsi_gi.nv_pck_lbn);
    } else {
      // process!
      d->packs_left = d->dsi_pack.dsi_gi.vobu_ea;
      MP_DBG(stream, "Found NAVI packet! lba=0x%X  len=%d  \n",d->cur_pack,d->packs_left);
      //navPrint_DSI(&d->dsi_pack);
      MP_TRACE(stream, "\r### CELL %d: Navi: %d/%d  IFO: %d/%d   \n",d->cur_cell,
        d->dsi_pack.dsi_gi.vobu_c_idn,d->dsi_pack.dsi_gi.vobu_vob_idn,
        d->cur_pgc->cell_position[d->cur_cell].cell_nr,
        d->cur_pgc->cell_position[d->cur_cell].vob_id_nr);

      if(d->angle_seek) {
        int i,skip=0;
        for(i=0;i<9;i++)        // check if all values zero:
          if((skip=d->dsi_pack.sml_agli.data[i].address)!=0) break;
        if(skip && skip!=0x7fffffff) {
          // sml_agli table has valid data (at least one non-zero):
         d->cur_pack=d->dsi_pack.dsi_gi.nv_pck_lbn+
         d->dsi_pack.sml_agli.data[d->dvd_angle-1].address;
         d->angle_seek=0;
         d->cur_pack--;
         MP_VERBOSE(stream, "Angle-seek synced using sml_agli map!  new_lba=0x%X  \n",d->cur_pack);
        } else {
          // check if we're in the right cell, jump otherwise:
          if( (d->dsi_pack.dsi_gi.vobu_c_idn==d->cur_pgc->cell_position[d->cur_cell].cell_nr) &&
            (d->dsi_pack.dsi_gi.vobu_vob_idn==d->cur_pgc->cell_position[d->cur_cell].vob_id_nr) ){
            d->angle_seek=0;
            MP_VERBOSE(stream, "Angle-seek synced by cell/vob IDN search!  \n");
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
    if(d->angle_seek==2) MP_VERBOSE(stream, "!!! warning! reading packet while angle_seek !!!\n");
    goto read_next; // searching for Navi packet
  }

  return d->cur_pack-1;
}

static int fill_buffer(stream_t *s, char *buf, int len)
{
  int64_t pos;
  if (len < 2048)
    return -1;
  pos = dvd_read_sector(s, s->priv, buf);
  if (pos < 0)
    return -1;
  return 2048; // full sector
}

static void stream_dvd_close(stream_t *s) {
  dvd_priv_t *d = s->priv;
  ifoClose(d->vts_file);
  ifoClose(d->vmg_file);
  DVDCloseFile(d->title);
  DVDClose(d->dvd);
  if (d->dvd_speed)
    dvd_set_speed(s,d->dvd_device_current, -1); /* -1 => restore default */
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


static int get_num_chapter(ifo_handle_t *vts_file, tt_srpt_t *tt_srpt, int title_no)
{
    if(!vts_file || !tt_srpt)
       return 0;

    if(title_no < 0 || title_no >= tt_srpt->nr_of_srpts)
       return 0;

    // map global title to vts title
    title_no = tt_srpt->title[title_no].vts_ttn - 1;

    if(title_no < 0 || title_no >= vts_file->vts_ptt_srpt->nr_of_srpts)
       return 0;

    return vts_file->vts_ptt_srpt->title[title_no].nr_of_ptts;
}

// p: in=chapter number, out=PTS
static int get_chapter_time(ifo_handle_t *vts_file, tt_srpt_t *tt_srpt, int title_no, double *p)
{
    unsigned int i, cell, last_cell;
    unsigned int t=0;
    ptt_info_t *ptt;
    pgc_t *pgc;

    title_no = tt_srpt->title[title_no].vts_ttn - 1;
    if(vts_file->vts_ptt_srpt->title[title_no].nr_of_ptts < 2)
       return 0;
    ptt = vts_file->vts_ptt_srpt->title[title_no].ptt;

    int cur = 0;
    for(i=0; i<vts_file->vts_ptt_srpt->title[title_no].nr_of_ptts; i++)
    {
        pgc = vts_file->vts_pgcit->pgci_srp[ptt[i].pgcn-1].pgc;
        cell = pgc->program_map[ptt[i].pgn-1]; //here the cell is 1-based
        if(ptt[i].pgn<pgc->nr_of_programs)
            last_cell = pgc->program_map[ptt[i].pgn];
        else
            last_cell = 0;
        while (cell < last_cell) {
            if(!(pgc->cell_playback[cell-1].block_type == BLOCK_TYPE_ANGLE_BLOCK &&
                 pgc->cell_playback[cell-1].block_mode != BLOCK_MODE_FIRST_CELL)
            ) {
                if (cur == *p) {
                    *p = t / 1000.0;
                    return 1;
                }
                t += mp_dvdtimetomsec(&pgc->cell_playback[cell-1].playback_time);
                cur++;
            }
            cell++;
        }
    }
    return 0;
}

static void list_chapters(stream_t *stream, ifo_handle_t *vts_file, tt_srpt_t *tt_srpt, int title_no)
{
    MP_INFO(stream, "CHAPTERS: ");
    for (int n = 0; ; n++) {
        double p = n;
        int r;
        r = get_chapter_time(vts_file, tt_srpt, title_no, &p);
        if (!r)
            break;
        int t = p * 1000;
        MP_INFO(stream, "%02d:%02d:%02d.%03d,", t/3600000, (t/60000)%60, (t/1000)%60, t%1000);
    }
    MP_INFO(stream, "\n");
}

static double dvd_get_current_time(stream_t *stream, int cell)
{
    int i, tm;
    dvd_priv_t *d = stream->priv;

    tm=0;
    if(cell < 0) cell=d->cur_cell;
    for(i=0; i<cell; i++) {
        if(d->cur_pgc->cell_playback[i].block_type == BLOCK_TYPE_ANGLE_BLOCK &&
           d->cur_pgc->cell_playback[i].block_mode != BLOCK_MODE_FIRST_CELL
        )
          continue;
        tm += d->cell_times_table[i];
    }
    tm += mp_dvdtimetomsec(&d->dsi_pack.dsi_gi.c_eltm);

    return (double)tm/1000.0;
}

static void dvd_seek(stream_t *stream, dvd_priv_t *d, int pos)
{
  d->packs_left=-1;
  d->cur_pack=pos;

  // check if we stay in current cell (speedup things, and avoid angle skip)
  if(d->cur_pack>d->cell_last_pack ||
     d->cur_pack<d->cur_pgc->cell_playback[ d->cur_cell ].first_sector) {

    // ok, cell change, find the right cell!
    cell_playback_t *cell;
    for(d->cur_cell=0; d->cur_cell < d->cur_pgc->nr_of_cells; d->cur_cell++) {
      cell = &(d->cur_pgc->cell_playback[d->cur_cell]);
      if(cell->block_type == BLOCK_TYPE_ANGLE_BLOCK && cell->block_mode != BLOCK_MODE_FIRST_CELL)
        continue;
      d->cell_last_pack=cell->last_sector;
      if(d->cur_pack<cell->first_sector) {
        d->cur_pack=cell->first_sector;
        break;
      }
      if(d->cur_pack<=d->cell_last_pack) break; // ok, we find it! :)
    }
  }

  MP_VERBOSE(stream, "DVD Seek! lba=0x%X  cell=%d  packs: 0x%X-0x%X  \n",
    d->cur_pack,d->cur_cell,d->cur_pgc->cell_playback[ d->cur_cell ].first_sector,d->cell_last_pack);

  // if we're in interleaved multi-angle cell, find the right angle chain!
  // (read Navi block, and use the seamless angle jump table)
  d->angle_seek=1;
}

static int do_seek(stream_t *s, int64_t newpos) {
  stream_drop_buffers(s);
  dvd_seek(s, s->priv,newpos/2048);
  return 1;
}

static int dvd_seek_to_time(stream_t *stream, ifo_handle_t *vts_file, double sec)
{
    unsigned int i, j, k, timeunit, ac_time, tmap_sector=0, cell_sector=0, vobu_sector=0;
    int t=0;
    double tm, duration;
    int64_t pos = -1;
    dvd_priv_t *d = stream->priv;
    vts_tmapt_t *vts_tmapt = vts_file->vts_tmapt;

    if(!vts_file->vts_tmapt || sec < 0)
        return 0;

    duration = (double) mp_get_titleset_length(d->vts_file, d->tt_srpt, d->cur_title) / 1000.0f;
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

    pos = ((int64_t)cell_sector)<<11;
    do_seek(stream, pos);
    do {
      char buf[2048];
      if (dvd_read_sector(stream, stream->priv, buf) < 0) // skip
          break;
      t = mp_dvdtimetomsec(&d->dsi_pack.dsi_gi.c_eltm);
    } while(!t);
    tm = dvd_get_current_time(stream, -1);

    pos = ((int64_t)tmap_sector)<<11;
    do_seek(stream, pos);
    //now get current time in terms of the cell+cell time offset
    memset(&d->dsi_pack.dsi_gi.c_eltm, 0, sizeof(dvd_time_t));
    while(tm <= sec) {
        char buf[2048];
        if (dvd_read_sector(stream, stream->priv, buf) < 0) // skip
            break;
        pos += 2048;
        tm = dvd_get_current_time(stream, -1);
    };
    tmap_sector = pos >> 11;

    //search closest VOBU sector
    k=(vts_file->vts_vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE)/4; //entries in the vobu admap
    for(i=1; i<k; i++) {
      if(vts_file->vts_vobu_admap->vobu_start_sectors[i] > tmap_sector)
        break;
    }
    vobu_sector = vts_file->vts_vobu_admap->vobu_start_sectors[i-1];
    pos = ((int64_t)vobu_sector) << 11;
    do_seek(stream, pos);

    return 1;
}

static int control(stream_t *stream,int cmd,void* arg)
{
    dvd_priv_t *d = stream->priv;
    switch(cmd)
    {
        case STREAM_CTRL_GET_TIME_LENGTH:
        {
            *((double *)arg) = (double) mp_get_titleset_length(d->vts_file, d->tt_srpt, d->cur_title)/1000.0;
            return 1;
        }
        case STREAM_CTRL_GET_NUM_TITLES:
        {
            *((unsigned int *)arg) = d->vmg_file->tt_srpt->nr_of_srpts;
            return 1;
        }
        case STREAM_CTRL_GET_TITLE_LENGTH:
        {
            int t = *(double *)arg;
            if (t < 0 || t >= d->vmg_file->tt_srpt->nr_of_srpts)
                break;
            if (d->tt_srpt->title[t].title_set_nr !=
                d->tt_srpt->title[d->dvd_title].title_set_nr)
                break;
            *(double *)arg =
                mp_get_titleset_length(d->vts_file, d->tt_srpt, t) / 1000.0;
            return 1;
        }
        case STREAM_CTRL_GET_NUM_CHAPTERS:
        {
            int r;
            r = get_num_chapter(d->vts_file, d->tt_srpt, d->cur_title);
            if(! r) return STREAM_UNSUPPORTED;
            *((unsigned int *)arg) = r;
            return 1;
        }
        case STREAM_CTRL_GET_CHAPTER_TIME:
        {
            int r;
            r = get_chapter_time(d->vts_file, d->tt_srpt, d->cur_title, (double *)arg);
            if(! r) return STREAM_UNSUPPORTED;
            return 1;
        }
        case STREAM_CTRL_GET_CURRENT_TITLE:
        {
            *((unsigned int *)arg) = d->cur_title;
            return 1;
        }
        case STREAM_CTRL_GET_CURRENT_TIME:
        {
            double tm;
            tm = dvd_get_current_time(stream, -1);
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
            *((int *)arg) = d->vmg_file->tt_srpt->title[d->dvd_title].nr_of_angles;
            return 1;
        }
        case STREAM_CTRL_GET_ANGLE:
        {
            *((int *)arg) = d->dvd_angle;
            return 1;
        }
        case STREAM_CTRL_SET_ANGLE:
        {
            int ang = *((int *)arg);
            if(ang>d->vmg_file->tt_srpt->title[d->dvd_title].nr_of_angles || ang<=0)
                break;
            d->dvd_angle = ang;
            d->angle_seek = 1;
            return 1;
        }
        case STREAM_CTRL_GET_LANG:
        {
            struct stream_lang_req *req = arg;
            int lang = 0;
            switch(req->type) {
            case STREAM_AUDIO:
                lang = dvd_lang_from_aid(stream, req->id);
                break;
            case STREAM_SUB:
                lang = dvd_lang_from_sid(stream, req->id);
                break;
            }
            if (!lang)
                break;
            snprintf(req->name, sizeof(req->name), "%c%c", lang >> 8, lang);
            return STREAM_OK;
        }
        case STREAM_CTRL_GET_DVD_INFO:
        {
            struct stream_dvd_info_req *req = arg;
            memset(req, 0, sizeof(*req));
            req->num_subs = dvd_number_of_subs(stream);
            memcpy(req->palette, d->cur_pgc->palette, sizeof(req->palette));
            return STREAM_OK;
        }
        case STREAM_CTRL_GET_DISC_NAME:
        {
            char buffer[128];
            if (DVDUDFVolumeInfo(d->dvd, buffer, sizeof(buffer), NULL, 0) < 0 &&
                DVDISOVolumeInfo(d->dvd, buffer, sizeof(buffer), NULL, 0) < 0)
                break;
            if (!buffer[0])
                break;
            *(char**)arg = talloc_strdup(NULL, buffer);
            return STREAM_OK;
        }
        case STREAM_CTRL_GET_SIZE:
            *(int64_t *)arg =
                (d->cur_pgc->cell_playback[d->last_cell-1].last_sector)*2048LL;
            return STREAM_OK;
    }
    return STREAM_UNSUPPORTED;
}


static int open_s_internal(stream_t *stream)
{
  int k;
  dvd_priv_t *d = stream->priv;

  struct dvd_opts *opts =
    mp_get_config_group(stream, stream->global, &dvd_conf);

  d->dvd_angle = opts->angle;

  MP_VERBOSE(stream, "URL: %s\n", stream->url);
  d->dvd_title = d->cfg_title + 1;
  if(1){
    //int ret,ret2;
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
    if(d->cfg_device && d->cfg_device[0])
      d->dvd_device_current = d->cfg_device;
    else if(opts->device && opts->device[0])
      d->dvd_device_current = talloc_strdup(stream, opts->device);
    else
      d->dvd_device_current = DEFAULT_DVD_DEVICE;
    d->dvd_speed = opts->speed;
    dvd_set_speed(stream,d->dvd_device_current, d->dvd_speed);
#if defined(__APPLE__) || defined(__DARWIN__)
    /* Dynamic DVD drive selection on Darwin */
    if(!strcmp(d->dvd_device_current, "/dev/rdiskN")) {
      int i;
      size_t len = strlen(d->dvd_device_current)+1;
      char *temp_device = malloc(len);

      for (i = 1; i < 10; i++) {
        snprintf(temp_device, len, "/dev/rdisk%d", i);
        dvd = DVDOpen(temp_device);
        if(!dvd) {
          MP_ERR(stream, "Couldn't open DVD device: %s (%s)\n",temp_device,
                 mp_strerror(errno));
        } else {
#if DVDREAD_VERSION <= LIBDVDREAD_VERSION(0,9,4)
          dvd_file_t *dvdfile = DVDOpenFile(dvd,d->dvd_title,DVD_READ_INFO_FILE);
          if(!dvdfile) {
            MP_ERR(stream, "Couldn't open DVD device: %s (%s)\n",temp_device,
                   mp_strerror(errno));
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
        return STREAM_UNSUPPORTED;
      }
    } else
#endif /* defined(__APPLE__) || defined(__DARWIN__) */
    {
        dvd = DVDOpen(d->dvd_device_current);
        if(!dvd) {
          MP_ERR(stream, "Couldn't open DVD device: %s (%s)\n",
                 d->dvd_device_current, mp_strerror(errno));
          return STREAM_UNSUPPORTED;
        }
    }

    MP_VERBOSE(stream, "Reading disc structure, please wait...\n");

    /**
     * Load the video manager to find out the information about the titles on
     * this disc.
     */
    vmg_file = ifoOpen(dvd, 0);
    if(!vmg_file) {
      MP_ERR(stream, "Can't open VMG info!\n");
      DVDClose( dvd );
      return STREAM_UNSUPPORTED;
    }
    tt_srpt = vmg_file->tt_srpt;
    /**
     * Make sure our title number is valid.
     */
    MP_INFO(stream, "There are %d titles on this DVD.\n", tt_srpt->nr_of_srpts );
    if(d->dvd_title < 1 || d->dvd_title > tt_srpt->nr_of_srpts) {
      MP_ERR(stream, "Invalid DVD title number: %d\n", d->dvd_title);
      ifoClose( vmg_file );
      DVDClose( dvd );
      return STREAM_UNSUPPORTED;
    }
    --(d->dvd_title); // remap 1.. -> 0..
    /**
     * Make sure the angle number is valid for this title.
     */
    MP_INFO(stream, "There are %d angles in this DVD title.\n", tt_srpt->title[d->dvd_title].nr_of_angles);
    if(d->dvd_angle<1 || d->dvd_angle>tt_srpt->title[d->dvd_title].nr_of_angles) {
      MP_ERR(stream, "Invalid DVD angle number: %d\n", d->dvd_angle);
      goto fail;
    }

    ttn = tt_srpt->title[d->dvd_title].vts_ttn - 1;
    /**
     * Load the VTS information for the title set our title is in.
     */
    vts_file = ifoOpen( dvd, tt_srpt->title[d->dvd_title].title_set_nr );
    if(!vts_file) {
      MP_ERR(stream, "Cannot open the IFO file for DVD title %d.\n", tt_srpt->title[d->dvd_title].title_set_nr );
      goto fail;
    }
    /**
     * We've got enough info, time to open the title set data.
     */
    title = DVDOpenFile(dvd, tt_srpt->title[d->dvd_title].title_set_nr, DVD_READ_TITLE_VOBS);
    if(!title) {
      MP_ERR(stream, "Cannot open title VOBS (VTS_%02d_1.VOB).\n", tt_srpt->title[d->dvd_title].title_set_nr);
      ifoClose( vts_file );
      goto fail;
    }

    MP_VERBOSE(stream, "DVD successfully opened.\n");
    // store data
    d->dvd=dvd;
    d->title=title;
    d->vmg_file=vmg_file;
    d->tt_srpt=tt_srpt;
    d->vts_file=vts_file;
    d->cur_title = d->dvd_title;

    pgc_id = vts_file->vts_ptt_srpt->title[ttn].ptt[0].pgcn; // local
    pgn  = vts_file->vts_ptt_srpt->title[ttn].ptt[0].pgn;  // local
    pgc = vts_file->vts_pgcit ? vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc : NULL;
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
           MP_INFO(stream, "audio stream: %d format: %s (%s) language: %s aid: %d.\n",
             d->nr_of_channels,
             dvd_audio_stream_types[ audio->audio_format ],
             dvd_audio_stream_channels[ audio->channels ],
             tmp,
             audio_stream->id
           );

           d->nr_of_channels++;
         }
      }
      MP_INFO(stream, "number of audio channels on disk: %d.\n",d->nr_of_channels );
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

        MP_INFO(stream, "subtitle ( sid ): %d language: %s\n", sub_stream->id, tmp);
        d->nr_of_subtitles++;
      }
      MP_INFO(stream, "number of subtitles on disk: %d\n",d->nr_of_subtitles);
    }

    /**
     * Determine which program chain we want to watch.  This is based on the
     * chapter number.
     */
    d->cur_pgc_idx = pgc_id-1;
    d->cur_pgc = vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
    d->cur_cell = d->cur_pgc->program_map[pgn-1] - 1; // start playback here
    d->packs_left=-1;      // for Navi stuff
    d->angle_seek=0;
      d->last_cell=d->cur_pgc->nr_of_cells;

    if(d->cur_pgc->cell_playback[d->cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK )
      d->cur_cell+=d->dvd_angle-1;
    d->cur_pack = d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
    d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
    MP_VERBOSE(stream, "DVD start cell: %d  pack: 0x%X-0x%X  \n",d->cur_cell,d->cur_pack,d->cell_last_pack);

    //assign cell_times_table
    d->cell_times_table = malloc(sizeof(unsigned int) * d->cur_pgc->nr_of_cells);
    if(d->cell_times_table == NULL)
      return STREAM_UNSUPPORTED;
    for(k=0; k<d->cur_pgc->nr_of_cells; k++)
      d->cell_times_table[k] = mp_dvdtimetomsec(&d->cur_pgc->cell_playback[k].playback_time);
    list_chapters(stream, vts_file,tt_srpt,d->dvd_title);

    // ... (unimplemented)
    //    return NULL;
    stream->demuxer = "+disc";
    stream->lavf_type = "mpeg";
    stream->sector_size = 2048;
    stream->fill_buffer = fill_buffer;
    stream->control = control;
    stream->close = stream_dvd_close;
    MP_VERBOSE(stream, "DVD start=%d end=%d  \n",d->cur_pack,d->cur_pgc->cell_playback[d->last_cell-1].last_sector);
    stream->priv = (void*)d;
    return STREAM_OK;

fail:
      ifoClose(vmg_file);
      DVDClose(dvd);
      return STREAM_UNSUPPORTED;
  }
  MP_ERR(stream, "mpv was compiled without DVD support, exiting.\n");
  return STREAM_UNSUPPORTED;
}

static int open_s(stream_t *stream)
{
    dvd_priv_t *d = talloc_zero(stream, dvd_priv_t);
    stream->priv = d;

    bstr title, bdevice;
    bstr_split_tok(bstr0(stream->path), "/", &title, &bdevice);

    if (title.len) {
        bstr rest;
        d->cfg_title = bstrtoll(title, &rest, 10);
        if (rest.len) {
            MP_ERR(stream, "number expected: '%.*s'\n", BSTR_P(rest));
            return STREAM_ERROR;
        }
    }

    d->cfg_device = bstrto0(d, bdevice);

    return open_s_internal(stream);
}

static int ifo_stream_open(stream_t *stream)
{
    dvd_priv_t *priv = talloc_zero(stream, dvd_priv_t);
    stream->priv = priv;

    if (!stream->access_references)
        goto unsupported;

    char *path = mp_file_get_path(priv, bstr0(stream->url));
    if (!path)
        goto unsupported;

    if (!dvd_probe(path, ".ifo", "DVDVIDEO-VTS"))
        goto unsupported;

    char *base = mp_basename(path);

    // Only accept individual titles - use dvdnav for video_ts.ifo
    if (strncasecmp(base, "vts_", 4))
        goto unsupported;

    if (sscanf(base + 3, "_%02d_", &priv->cfg_title) != 1)
        goto unsupported;

    priv->cfg_device = bstrto0(priv, mp_dirname(path));

    MP_INFO(stream, ".IFO detected. Redirecting to dvdread://\n");
    return open_s_internal(stream);

unsupported:
    talloc_free(priv);
    stream->priv = NULL;
    return STREAM_UNSUPPORTED;
}

const stream_info_t stream_info_dvd = {
  .name = "dvd",
  .open = open_s,
  .protocols = (const char*const[]){ "dvdread", NULL },
};

const stream_info_t stream_info_ifo = {
    .name = "ifo",
    .open = ifo_stream_open,
    .protocols = (const char*const[]){ "file", "", NULL },
};
