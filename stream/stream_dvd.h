
#ifdef USE_DVDREAD

#ifdef USE_DVDREAD_INTERNAL
#include "dvdread/dvd_reader.h"
#include "dvdread/ifo_types.h"
#include "dvdread/ifo_read.h"
#include "dvdread/nav_read.h"
#elif defined(USE_DVDNAV)
#include <dvd_reader.h>
#include <ifo_types.h>
#include <ifo_read.h>
#include <nav_read.h>
#else
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>
#endif

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
} dvd_priv_t;

int dvd_number_of_subs(stream_t *stream);
int dvd_lang_from_aid(stream_t *stream, int id);
int dvd_lang_from_sid(stream_t *stream, int id);
int dvd_aid_from_lang(stream_t *stream, unsigned char* lang);
int dvd_sid_from_lang(stream_t *stream, unsigned char* lang);
int dvd_chapter_from_cell(dvd_priv_t *dvd,int title,int cell);

#endif
