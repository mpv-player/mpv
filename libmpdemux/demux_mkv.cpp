extern "C" {
#include "config.h"
}

#ifdef HAVE_MATROSKA

extern "C" {
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../mp_msg.h"
#include "../help_mp.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "../subreader.h"
#include "../libvo/sub.h"
}

#include <iostream>
#include <cassert>
#include <typeinfo>

#include "EbmlHead.h"
#include "EbmlSubHead.h"
#include "EbmlStream.h"
#include "EbmlContexts.h"
#include "FileKax.h"

#include "KaxAttachements.h"
#include "KaxBlock.h"
#include "KaxBlockData.h"
#include "KaxChapters.h"
#include "KaxCluster.h"
#include "KaxClusterData.h"
#include "KaxContexts.h"
#include "KaxCues.h"
#include "KaxCuesData.h"
#include "KaxInfo.h"
#include "KaxInfoData.h"
#include "KaxSeekHead.h"
#include "KaxSegment.h"
#include "KaxTracks.h"
#include "KaxTrackAudio.h"
#include "KaxTrackVideo.h"

#include "StdIOCallback.h"

#include "matroska.h"

using namespace LIBMATROSKA_NAMESPACE;
using namespace std;

// for e.g. "-slang ger"
extern char *dvdsub_lang;

// default values for Matroska elements
#define MKVD_TIMECODESCALE 1000000 // 1000000 = 1ms

class mpstream_io_callback: public IOCallback {
  private:
    stream_t *s;
  public:
    mpstream_io_callback(stream_t *stream);

    virtual uint32 read(void *buffer, size_t size);
    virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
    virtual size_t write(const void *buffer, size_t size);
    virtual uint64 getFilePointer();
    virtual void close();
};

mpstream_io_callback::mpstream_io_callback(stream_t *stream) {
  s = stream;
}

uint32 mpstream_io_callback::read(void *buffer, size_t size) {
  uint32_t result;

  result = stream_read(s, (char *)buffer, size);

  return result;
}

void mpstream_io_callback::setFilePointer(int64 offset, seek_mode mode) {
  int64 new_pos;
  
  if (mode == seek_beginning)
    new_pos = offset + s->start_pos;
  else if (mode == seek_end)
    new_pos = s->end_pos - offset;
  else
    new_pos = s->pos + offset;

  if (new_pos > s->end_pos) {
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] seek warning: new_pos %lld > end_pos "
           "%lld\n", new_pos, s->end_pos);
    return;
  }

  stream_seek(s, new_pos);
}

size_t mpstream_io_callback::write(const void */*buffer*/, size_t /*size*/) {
  return 0;
}

uint64 mpstream_io_callback::getFilePointer() {
  return s->pos - s->buf_len + s->buf_pos;
}

void mpstream_io_callback::close() {
}

typedef struct mkv_index_entry {
  uint64_t timecode, filepos;
  int is_key;
} mkv_index_entry_t;

typedef struct mkv_track_index {
  uint32_t tnum;
  int num_entries;
  mkv_index_entry_t *entries;
} mkv_track_index_t;

typedef struct mkv_track {
  uint32_t tnum;
  
  char *codec_id;
  int ms_compat;
  char *language;

  char type; // 'v' = video, 'a' = audio, 's' = subs
  
  char v_fourcc[5];
  uint32_t v_width, v_height, v_dwidth, v_dheight;
  float v_frate;

  uint16_t a_formattag;
  uint32_t a_channels, a_bps;
  float a_sfreq;

  int default_track;

  void *private_data;
  unsigned int private_size;

  unsigned char *headers[3];
  uint32_t header_sizes[3];

  int ok;
} mkv_track_t;

typedef struct mkv_demuxer {
  float duration, last_pts;
  uint64_t last_filepos;

  mkv_track_t **tracks;
  int num_tracks;
  mkv_track_t *video, *audio, *subs_track;

  uint64_t tc_scale, cluster_tc;

  mpstream_io_callback *in;

  uint64_t clear_subs_at;

  subtitle subs;

  EbmlStream *es;
  EbmlElement *saved_l1, *saved_l2;
  KaxSegment *segment;
  KaxCluster *cluster;

  mkv_track_index_t *index;
  int num_indexes, cues_found, cues_searched;
  int64_t *cluster_positions;
  int num_cluster_pos;

  int skip_to_keyframe;
  int64_t skip_to_timecode;
} mkv_demuxer_t;

static uint16_t get_uint16(const void *buf) {
  uint16_t      ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[1] & 0xff;
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

static uint32_t get_uint32(const void *buf) {
  uint32_t      ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[3] & 0xff;
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

static void handle_subtitles(demuxer_t *d, KaxBlock *block, int64_t duration) {
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *)d->priv;
  int len, line, state;
  char *s1, *s2, *buffer;

  if (duration == -1) {
    mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Warning: No KaxBlockDuration "
           "element for subtitle track found.\n");
    return;
  }

  DataBuffer &data = block->GetBuffer(0);
  len = data.Size();

  buffer = (char *)data.Buffer();
  s1 = buffer;

  while (((*s1 == '\n') || (*s1 == '\r')) &&
         ((unsigned int)(s1 - buffer) <= data.Size()))
    s1++;

  line = 0;
  s2 = mkv_d->subs.text[0];
  mkv_d->subs.lines = 1;
  state = 0;
  while ((unsigned int)(s1 - buffer) != data.Size()) {
    if ((*s1 == '\n') || (*s1 == '\r')) {
      if (state == 0) {       // normal char --> newline
        if (mkv_d->subs.lines == SUB_MAX_TEXT)
          break;
        *s2 = 0;
        s2 = mkv_d->subs.text[mkv_d->subs.lines];
        mkv_d->subs.lines++;
        state = 1;
      }
    } else if (*s1 == '<')    // skip HTML tags
      state = 2;
    else if (*s1 == '>')
      state = 0;
    else if (state != 2) {      // normal character
      state = 0;
      if ((s2 - mkv_d->subs.text[mkv_d->subs.lines - 1]) < 255) {
        *s2 = *s1;
        s2++;
      }
    }
    s1++;
  }

  *s2 = 0;

#ifdef USE_ICONV
  subcp_recode1(&mkv_d->subs);
#endif

  vo_sub = &mkv_d->subs;
  vo_osd_changed(OSDTYPE_SUBTITLE);

  mkv_d->clear_subs_at = block->GlobalTimecode() / 1000000 + duration;
}

static mkv_track_t *new_mkv_track(mkv_demuxer_t *d) {
  mkv_track_t *t;
  
  t = (mkv_track_t *)malloc(sizeof(mkv_track_t));
  if (t != NULL) {
    memset(t, 0, sizeof(mkv_track_t));
    d->tracks = (mkv_track_t **)realloc(d->tracks, (d->num_tracks + 1) *
                                        sizeof(mkv_track_t *));
    if (d->tracks == NULL)
      return NULL;
    d->tracks[d->num_tracks] = t;
    d->num_tracks++;

    // Set default values.
    t->default_track = 1;
    t->a_sfreq = 8000.0;
    t->a_channels = 1;
    t->language = strdup("eng");
  }
  
  return t;
}

static mkv_track_t *find_track_by_num(mkv_demuxer_t *d, uint32_t n,
                                      mkv_track_t *c) {
  int i;
  
  for (i = 0; i < d->num_tracks; i++)
    if ((d->tracks[i] != NULL) && (d->tracks[i]->tnum == n) &&
        (d->tracks[i] != c))
      return d->tracks[i];
  
  return NULL;
}

static mkv_track_t *find_track_by_language(mkv_demuxer_t *d, char *language,
                                           mkv_track_t *c, char type = 's') {
  int i;
  
  for (i = 0; i < d->num_tracks; i++)
    if ((d->tracks[i] != NULL) && (d->tracks[i] != c) &&
        (d->tracks[i]->language != NULL) &&
        !strcmp(d->tracks[i]->language, language) &&
        (d->tracks[i]->type == type))
      return d->tracks[i];
  
  return NULL;
}

static int check_track_information(mkv_demuxer_t *d) {
  int i, track_num;
  unsigned char *c;
  uint32_t u, offset, length;
  mkv_track_t *t;
  BITMAPINFOHEADER *bih;
  WAVEFORMATEX *wfe;
  
  for (track_num = 0; track_num < d->num_tracks; track_num++) {
    
    t = d->tracks[track_num];
    switch (t->type) {
      case 'v':                 // video track
        if (t->codec_id == NULL)
          continue;
        if (!strcmp(t->codec_id, MKV_V_MSCOMP)) {
          if ((t->private_data == NULL) ||
              (t->private_size < sizeof(BITMAPINFOHEADER))) {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] WARNING: CodecID for track "
                   "%u is '" MKV_V_MSCOMP "', but there was no "
                   "BITMAPINFOHEADER struct present. Therefore we don't have "
                   "a FourCC to identify the video codec used.\n", t->tnum);
            continue;
          } else {
            t->ms_compat = 1;

            bih = (BITMAPINFOHEADER *)t->private_data;

            u = get_uint32(&bih->biWidth);
            if (t->v_width != u) {
              mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] WARNING: (MS "
                     "compatibility mode, track %u) "
                     "Matrosa says video width is %u, but the "
                     "BITMAPINFOHEADER says %u.\n", t->tnum, t->v_width, u);
              if (t->v_width == 0)
                t->v_width = u;
            }

            u = get_uint32(&bih->biHeight);
            if (t->v_height != u) {
              mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] WARNING: (MS compatibility "
                     "mode, track %u) "
                     "Matrosa video height is %u, but the BITMAPINFOHEADER "
                     "says %u.\n", t->tnum, t->v_height, u);
              if (t->v_height == 0)
                t->v_height = u;
            }

            memcpy(t->v_fourcc, &bih->biCompression, 4);

            if (t->v_frate == 0.0) {
              mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] ERROR: (MS compatibility "
                     "mode, track %u) "
                     "No VideoFrameRate element was found.\n", t->tnum);
              continue;
            }
          }
        } else {
          mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Native CodecIDs for video "
                 "tracks are not supported yet (track %u).\n", t->tnum);
          continue;
        }

        if (t->v_width == 0) {
          mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] The width for track %u was "
                 "not set.\n", t->tnum);
          continue;
        }
        if (t->v_height == 0) {
          mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] The height for track %u was "
                 "not set.\n", t->tnum);
          continue;
        }

        if (t->v_dwidth == 0)
          t->v_dwidth = t->v_width;
        if (t->v_dheight == 0)
          t->v_dheight = t->v_height;

        // This track seems to be ok.
        t->ok = 1;

        break;

      case 'a':                 // audio track
        if (t->codec_id == NULL)
          continue;
        if (!strcmp(t->codec_id, MKV_A_ACM)) {
          if ((t->private_data == NULL) ||
              (t->private_size < sizeof(WAVEFORMATEX))) {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] WARNING: CodecID for track "
                   "%u is '" MKV_A_ACM "', "
                   "but there was no WAVEFORMATEX struct present. "
                   "Therefore we don't have a format ID to identify the audio "
                   "codec used.\n", t->tnum);
            continue;
          } else {
            t->ms_compat = 1;

            wfe = (WAVEFORMATEX *)t->private_data;
            u = get_uint32(&wfe->nSamplesPerSec);
            if (((uint32_t)t->a_sfreq) != u) {
              mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] WARNING: (MS compatibility "
                     "mode for track %u) "
                     "Matroska says that there are %u samples per second, "
                     "but WAVEFORMATEX says that there are %u.\n", t->tnum,
                     (uint32_t)t->a_sfreq, u);
              if (t->a_sfreq == 0.0)
                t->a_sfreq = (float)u;
            }

            u = get_uint16(&wfe->nChannels);
            if (t->a_channels != u) {
              mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] WARNING: (MS "
                     "compatibility mode for track %u) "
                     "Matroska says that there are %u channels, but the "
                     "WAVEFORMATEX says that there are %u.\n", t->tnum,
                     t->a_channels, u);
              if (t->a_channels == 0)
                t->a_channels = u;
            }

            u = get_uint16(&wfe->wBitsPerSample);
            if (t->a_channels != u) {
              mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] WARNING: (MS "
                     "compatibility mode for track %u) "
                     "Matroska says that there are %u bits per sample, "
                     "but the WAVEFORMATEX says that there are %u.\n", t->tnum,
                     t->a_bps, u);
              if (t->a_bps == 0)
                t->a_bps = u;
            }
            
            t->a_formattag = get_uint16(&wfe->wFormatTag);
          }
        } else {
          if (!strcmp(t->codec_id, MKV_A_MP3))
            t->a_formattag = 0x0055;
          else if (!strcmp(t->codec_id, MKV_A_AC3))
            t->a_formattag = 0x2000;
          else if (!strcmp(t->codec_id, MKV_A_PCM))
            t->a_formattag = 0x0001;
          else if (!strcmp(t->codec_id, MKV_A_VORBIS)) {
            if (t->private_data == NULL) {
              mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] WARNING: CodecID for "
                     "track %u is '" MKV_A_VORBIS
                     "', but there are no header packets present.", t->tnum);
              continue;
            }

            c = (unsigned char *)t->private_data;
            if (c[0] != 2) {
              mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Vorbis track does not "
                     "contain valid headers.\n");
              continue;
            }

            offset = 1;
            for (i = 0; i < 2; i++) {
              length = 0;
              while ((c[offset] == (unsigned char )255) &&
                     (length < t->private_size)) {
                length += 255;
                offset++;
              }
              if (offset >= (t->private_size - 1)) {
                mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Vorbis track does not "
                       "contain valid headers.\n");
                continue;
              }
              length += c[offset];
              offset++;
              t->header_sizes[i] = length;
            }

            t->headers[0] = &c[offset];
            t->headers[1] = &c[offset + t->header_sizes[0]];
            t->headers[2] = &c[offset + t->header_sizes[0] +
                               t->header_sizes[1]];
            t->header_sizes[2] = t->private_size - offset -
              t->header_sizes[0] - t->header_sizes[1];

            t->a_formattag = 0xFFFE;
          } else {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Unknown/unsupported audio "
                   "codec ID '%s' for track %u.\n", t->codec_id, t->tnum);
            continue;
          }
        }

        if (t->a_sfreq == 0.0) {
          mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] The sampling frequency was not "
                 "set for track %u.\n", t->tnum);
          continue;
        }

        if (t->a_channels == 0) {
          mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] The number of channels was not "
                 "set for track %u.\n", t->tnum);
          continue;
        }

        if (t->a_formattag == 0) {
          mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] The audio format tag was not "
                 "set for track %u.\n", t->tnum);
          continue;
        }

        // This track seems to be ok.
        t->ok = 1;

        break;

      case 's':                 // Text subtitles do not need any data
        t->ok = 1;              // except the CodecID.
        break;

      default:                  // unknown track type!? error in demuxer...
        mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] Error in demux_mkv.cpp: unknown "
               "demuxer type for track %u: '%c'\n", t->tnum, t->type);
        continue;
    }

    if (t->ok)
      mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Track %u seems to be ok.\n", t->tnum);
  }

  return 1;
}

static void free_mkv_demuxer(mkv_demuxer_t *d) {
  int i;
  
  if (d == NULL)
    return;

  for (i = 0; i < d->num_tracks; i++)
    if (d->tracks[i] != NULL) {
      if (d->tracks[i]->private_data != NULL)
        free(d->tracks[i]->private_data);
      if (d->tracks[i]->language != NULL)
        free(d->tracks[i]->language);
      free(d->tracks[i]);
    }

  for (i = 0; i < d->num_indexes; i++)
    free(d->index[i].entries);
  free(d->index);
  
  if (d->es != NULL)
    delete d->es;
  if (d->saved_l1 != NULL)
    delete d->saved_l1;
  if (d->in != NULL)
    delete d->in;
  if (d->segment != NULL)
    delete d->segment;

  free(d);
}

static void add_index_entry(mkv_demuxer_t *d, uint32_t tnum, uint64_t filepos,
                            uint64_t timecode, int is_key) {
  int i, found;
  mkv_index_entry_t *entry;

  for (i = 0, found = 0; i < d->num_indexes; i++)
    if (d->index[i].tnum == tnum) {
      found = 1;
      break;
    }

  if (!found) {
    d->index = (mkv_track_index_t *)realloc(d->index, (d->num_indexes + 1) *
                                            sizeof(mkv_track_index_t));
    if (d->index == NULL)
      return;
    i = d->num_indexes;
    memset(&d->index[i], 0, sizeof(mkv_track_index_t));
    d->index[i].tnum = tnum;
    d->num_indexes++;
  }

  d->index[i].entries =
    (mkv_index_entry_t *)realloc(d->index[i].entries,
                                 (d->index[i].num_entries + 1) *
                                 sizeof(mkv_index_entry_t));
  if (d->index[i].entries == NULL)
    return;
  entry = &d->index[i].entries[d->index[i].num_entries];
  entry->filepos = filepos;
  entry->timecode = timecode;
  entry->is_key = is_key;
  d->index[i].num_entries++;
}

static void add_cluster_position(mkv_demuxer_t *mkv_d, int64_t position) {
  mkv_d->cluster_positions = (int64_t *)realloc(mkv_d->cluster_positions,
                                                (mkv_d->num_cluster_pos + 1) *
                                                sizeof(int64_t));
  if (mkv_d->cluster_positions != NULL) {
    mkv_d->cluster_positions[mkv_d->num_cluster_pos] = position;
    mkv_d->num_cluster_pos++;
  } else
    mkv_d->num_cluster_pos = 0;
}

static int parse_cues(mkv_demuxer_t *mkv_d) {
  EbmlElement *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL, *l5 = NULL;
  EbmlStream *es;
  int upper_lvl_el, elements_found, i, k;
  uint64_t tc_scale, filepos = 0, timecode = 0;
  uint32_t tnum = 0;
  mkv_index_entry_t *entry;

  es = mkv_d->es;
  tc_scale = mkv_d->tc_scale;
  upper_lvl_el = 0;

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] /---- [ parsing cues ] -----------\n");

  l1 = es->FindNextElement(mkv_d->segment->Generic().Context, upper_lvl_el,
                           0xFFFFFFFFL, true, 1);
  if (l1 == NULL)
    return 0;

  if (!(EbmlId(*l1) == KaxCues::ClassInfos.GlobalId)) {
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] No KaxCues element found but but %s.\n"
           "[mkv] \\---- [ parsing cues ] -----------\n", typeid(*l1).name());
    
    return 0;
  }

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ found cues\n");

  l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                           true, 1);
  while (l2 != NULL) {
    if (upper_lvl_el != 0)
      break;

    if (EbmlId(*l2) == KaxCuePoint::ClassInfos.GlobalId) {
      mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] | + found cue point\n");

      elements_found = 0;

      l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true, 1);
      while (l3 != NULL) {
        if (upper_lvl_el != 0)
          break;

        if (EbmlId(*l3) == KaxCueTime::ClassInfos.GlobalId) {
          KaxCueTime &cue_time = *static_cast<KaxCueTime *>(l3);
          cue_time.ReadData(es->I_O());
          mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |  + found cue time: %.3fs\n",
                 ((float)uint64(cue_time)) * tc_scale / 1000000000.0);

          timecode = uint64(cue_time) * tc_scale / 1000000;
          elements_found |= 1;

        } else if (EbmlId(*l3) ==
                   KaxCueTrackPositions::ClassInfos.GlobalId) {
          mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |  + found cue track "
                 "positions\n");

          l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true, 1);
          while (l4 != NULL) {
            if (upper_lvl_el != 0)
              break;

            if (EbmlId(*l4) == KaxCueTrack::ClassInfos.GlobalId) {
              KaxCueTrack &cue_track = *static_cast<KaxCueTrack *>(l4);
              cue_track.ReadData(es->I_O());
              mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |   + found cue track: "
                     "%u\n", uint32(cue_track));

              tnum = uint32(cue_track);
              elements_found |= 2;

            } else if (EbmlId(*l4) ==
                       KaxCueClusterPosition::ClassInfos.GlobalId) {
              KaxCueClusterPosition &cue_cp =
                *static_cast<KaxCueClusterPosition *>(l4);
              cue_cp.ReadData(es->I_O());
              mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |   + found cue cluster "
                     "position: %llu\n", uint64(cue_cp));

              filepos = mkv_d->segment->GetGlobalPosition(uint64_t(cue_cp));
              elements_found |= 4;

            } else if (EbmlId(*l4) ==
                       KaxCueBlockNumber::ClassInfos.GlobalId) {
              KaxCueBlockNumber &cue_bn =
                *static_cast<KaxCueBlockNumber *>(l4);
              cue_bn.ReadData(es->I_O());
              mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |   + found cue block "
                     "number: %llu\n", uint64(cue_bn));

            } else if (EbmlId(*l4) ==
                       KaxCueCodecState::ClassInfos.GlobalId) {
              KaxCueCodecState &cue_cs =
                *static_cast<KaxCueCodecState *>(l4);
              cue_cs.ReadData(es->I_O());
              mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |   + found cue codec "
                     "state: %llu\n", uint64(cue_cs));

            } else if (EbmlId(*l4) ==
                       KaxCueReference::ClassInfos.GlobalId) {
              mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |  + found cue "
                     "reference\n");

              elements_found |= 8;

              l5 = es->FindNextElement(l4->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true, 1);
              while (l5 != NULL) {
                if (upper_lvl_el != 0)
                  break;

                if (EbmlId(*l5) == KaxCueRefTime::ClassInfos.GlobalId) {
                  KaxCueRefTime &cue_rt =
                    *static_cast<KaxCueRefTime *>(l5);
                  mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |    + found cue ref "
                         "time: %.3fs\n", ((float)uint64(cue_rt)) * tc_scale /
                         1000000000.0);

                } else if (EbmlId(*l5) ==
                           KaxCueRefCluster::ClassInfos.GlobalId) {
                  KaxCueRefCluster &cue_rc =
                    *static_cast<KaxCueRefCluster *>(l5);
                  cue_rc.ReadData(es->I_O());
                  mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |    + found cue ref "
                         "cluster: %llu\n", uint64(cue_rc));

                } else if (EbmlId(*l5) ==
                           KaxCueRefNumber::ClassInfos.GlobalId) {
                  KaxCueRefNumber &cue_rn =
                    *static_cast<KaxCueRefNumber *>(l5);
                  cue_rn.ReadData(es->I_O());
                  mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |    + found cue ref "
                         "number: %llu\n", uint64(cue_rn));

                } else if (EbmlId(*l5) ==
                           KaxCueRefCodecState::ClassInfos.GlobalId) {
                  KaxCueRefCodecState &cue_rcs =
                    *static_cast<KaxCueRefCodecState *>(l5);
                  cue_rcs.ReadData(es->I_O());
                  mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |    + found cue ref "
                         "codec state: %llu\n", uint64(cue_rcs));

                } else {
                  mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |    + unknown "
                         "element, level 5: %s\n", typeid(*l5).name());
                }

                l5->SkipData(static_cast<EbmlStream &>(*es),
                             l5->Generic().Context);
                delete l5;
                l5 = es->FindNextElement(l4->Generic().Context,
                                         upper_lvl_el, 0xFFFFFFFFL, true, 1);
              } // while (l5 != NULL)

            } else
              mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |  + unknown element, "
                     "level 4: %s\n", typeid(*l4).name());

            if (upper_lvl_el > 0) {		// we're coming from l5
              upper_lvl_el--;
              delete l4;
              l4 = l5;
              if (upper_lvl_el > 0)
                break;

            } else {
              l4->SkipData(static_cast<EbmlStream &>(*es),
                           l4->Generic().Context);
              delete l4;
              l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true, 1);
            }
          } // while (l4 != NULL)

        } else
          mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |  + unknown element, level 3: "
                 "%s\n", typeid(*l3).name());

        if (upper_lvl_el > 0) {		// we're coming from l4
          upper_lvl_el--;
          delete l3;
          l3 = l4;
          if (upper_lvl_el > 0)
            break;

        } else {
          l3->SkipData(static_cast<EbmlStream &>(*es),
                       l3->Generic().Context);
          delete l3;
          l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true, 1);
        }
      } // while (l3 != NULL)

      // Three elements must have been found in order for this to be a
      // correct entry:
      // 1: cue time (timecode)
      // 2: cue track (tnum)
      // 4: cue cluster position (filepos)
      // If 8 is also set, then there was a reference element, and the
      // current block is not an I frame. If 8 is not set then this is
      // an I frame.
      if ((elements_found & 7) == 7)
        add_index_entry(mkv_d, tnum, filepos, timecode,
                        (elements_found & 8) ? 0 : 1);

    } else
      mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |  + unknown element, level 2: "
             "%s\n", typeid(*l2).name());

    if (upper_lvl_el > 0) {		// we're coming from l3
      upper_lvl_el--;
      delete l2;
      l2 = l3;
      if (upper_lvl_el > 0)
        break;

    } else {
      l2->SkipData(static_cast<EbmlStream &>(*es),
                   l2->Generic().Context);
      delete l2;
      l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true, 1);
    }
  } // while (l2 != NULL)

  // Debug: dump the index
  for (i = 0; i < mkv_d->num_indexes; i++) {
    mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] Index for track %u contains %u "
           "entries.\n", mkv_d->index[i].tnum, mkv_d->index[i].num_entries);
    for (k = 0; k < mkv_d->index[i].num_entries; k++) {
      entry = &mkv_d->index[i].entries[k];
      mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv]   %d: timecode %llu, filepos %llu, "
             "is key: %s\n", k, entry->timecode, entry->filepos,
             entry->is_key ? "yes" : "no");
    }
  }

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] \\---- [ parsing cues ] -----------\n");

  return 1;
}

extern "C" int demux_mkv_open(demuxer_t *demuxer) {
  unsigned char signature[4];
  stream_t *s;
  demux_packet_t *dp;
  mkv_demuxer_t *mkv_d;
  int upper_lvl_el, exit_loop, i;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  EbmlStream *es;
  mkv_track_t *track;
  sh_audio_t *sh_a;
  sh_video_t *sh_v;
  uint64_t seek_pos, current_pos;
  int seek_element_is_cue;

#ifdef USE_ICONV
          subcp_open();
#endif

  s = demuxer->stream;
  stream_seek(s, s->start_pos);
  memset(signature, 0, 4);
  stream_read(s, (char *)signature, 4);
  if ((signature[0] != 0x1A) || (signature[1] != 0x45) ||
      (signature[2] != 0xDF) || (signature[3] != 0xA3))
    return 0;
  stream_seek(s, s->start_pos);
  
  try {
    // structure for storing the demuxer's private data
    mkv_d = (mkv_demuxer_t *)malloc(sizeof(mkv_demuxer_t));
    if (mkv_d == NULL)
      return 0;
    memset(mkv_d, 0, sizeof(mkv_demuxer_t));
    mkv_d->duration = -1.0;
    
    // Create the interface between MPlayer's IO system and
    // libmatroska's IO system.
    mkv_d->in = new mpstream_io_callback(demuxer->stream);
    if (mkv_d->in == NULL) {
      free_mkv_demuxer(mkv_d);
      return 0;
    }
    mpstream_io_callback &io = *static_cast<mpstream_io_callback *>(mkv_d->in);
    mkv_d->es = new EbmlStream(io);
    if (mkv_d->es == NULL) {
      free_mkv_demuxer(mkv_d);
      return 0;
    }
    es = mkv_d->es;
    
    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] no head found\n");
      free_mkv_demuxer(mkv_d);
      return 0;
    }
    // Don't verify its data for now.
    l0->SkipData(static_cast<EbmlStream &>(*es), l0->Generic().Context);
    delete l0;
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Found the head...\n");
    
    // Next element must be a segment
    l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] but no segment :(\n");
      free_mkv_demuxer(mkv_d);
      return 0;
    }
    if (!(EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)) {
      mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] but no segment :(\n");
      free_mkv_demuxer(mkv_d);
      return 0;
    }
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] + a segment...\n");
    
    mkv_d->segment = (KaxSegment *)l0;

    upper_lvl_el = 0;
    exit_loop = 0;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while (l1 != NULL) {
      if ((upper_lvl_el != 0) || exit_loop)
        break;

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ segment information...\n");
        
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if ((upper_lvl_el != 0) || exit_loop)
            break;

          if (EbmlId(*l2) == KaxTimecodeScale::ClassInfos.GlobalId) {
            KaxTimecodeScale &tc_scale = *static_cast<KaxTimecodeScale *>(l2);
            tc_scale.ReadData(es->I_O());
            mkv_d->tc_scale = uint64(tc_scale);
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + timecode scale: %llu\n",
                   mkv_d->tc_scale);

          } else if (EbmlId(*l2) == KaxDuration::ClassInfos.GlobalId) {
            KaxDuration &duration = *static_cast<KaxDuration *>(l2);
            duration.ReadData(es->I_O());
            mkv_d->duration = float(duration) * mkv_d->tc_scale / 1000000000.0;
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + duration: %.3fs\n",
                   mkv_d->duration);

          } else
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + unknown element@2: %s\n",
                   typeid(*l2).name());

          l2->SkipData(static_cast<EbmlStream &>(*es),
                       l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true, 1);
        }

        if (mkv_d->tc_scale == 0)
          mkv_d->tc_scale = MKVD_TIMECODESCALE;

      } else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) {
        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ segment tracks...\n");
        
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if ((upper_lvl_el != 0) || exit_loop)
            break;
          
          if (EbmlId(*l2) == KaxTrackEntry::ClassInfos.GlobalId) {
            // We actually found a track entry :) We're happy now.
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + a track...\n");
            
            track = new_mkv_track(mkv_d);
            if (track == NULL)
              return 0;
            
            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
                break;
              
              // Now evaluate the data belonging to this track
              if (EbmlId(*l3) == KaxTrackNumber::ClassInfos.GlobalId) {
                KaxTrackNumber &tnum = *static_cast<KaxTrackNumber *>(l3);
                tnum.ReadData(es->I_O());
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Track number: %u\n",
                       uint32(tnum));
                track->tnum = uint32(tnum);
                if (find_track_by_num(mkv_d, track->tnum, track) != NULL)
                  mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] |  + WARNING: There's "
                         "more than one track with the number %u.\n",
                         track->tnum);

              } else if (EbmlId(*l3) == KaxTrackUID::ClassInfos.GlobalId) {
                KaxTrackUID &tuid = *static_cast<KaxTrackUID *>(l3);
                tuid.ReadData(es->I_O());
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Track UID: %u\n",
                       uint32(tuid));

              } else if (EbmlId(*l3) == KaxTrackType::ClassInfos.GlobalId) {
                KaxTrackType &ttype = *static_cast<KaxTrackType *>(l3);
                ttype.ReadData(es->I_O());
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Track type: ");

                switch (uint8(ttype)) {
                  case track_audio:
                    mp_msg(MSGT_DEMUX, MSGL_V, "Audio\n");
                    track->type = 'a';
                    break;
                  case track_video:
                    mp_msg(MSGT_DEMUX, MSGL_V, "Video\n");
                    track->type = 'v';
                    break;
                  case track_subtitle:
                    mp_msg(MSGT_DEMUX, MSGL_V, "Subtitle\n");
                    track->type = 's';
                    break;
                  default:
                    mp_msg(MSGT_DEMUX, MSGL_V, "unknown\n");
                    track->type = '?';
                    break;
                }

              } else if (EbmlId(*l3) == KaxTrackAudio::ClassInfos.GlobalId) {
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Audio track\n");
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el != 0)
                    break;
                
                  if (EbmlId(*l4) ==
                      KaxAudioSamplingFreq::ClassInfos.GlobalId) {
                    KaxAudioSamplingFreq &freq =
                      *static_cast<KaxAudioSamplingFreq*>(l4);
                    freq.ReadData(es->I_O());
                    track->a_sfreq = float(freq);
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Sampling "
                           "frequency: %f\n", track->a_sfreq);

                  } else if (EbmlId(*l4) ==
                             KaxAudioChannels::ClassInfos.GlobalId) {
                    KaxAudioChannels &channels =
                      *static_cast<KaxAudioChannels*>(l4);
                    channels.ReadData(es->I_O());
                    track->a_channels = uint8(channels);
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Channels: %u\n",
                           track->a_channels);

                  } else if (EbmlId(*l4) ==
                             KaxAudioBitDepth::ClassInfos.GlobalId) {
                    KaxAudioBitDepth &bps =
                      *static_cast<KaxAudioBitDepth*>(l4);
                    bps.ReadData(es->I_O());
                    track->a_bps = uint8(bps);
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Bit depth: %u\n",
                           track->a_bps);

                  } else
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + unknown "
                           "element@4: %s\n", typeid(*l4).name());

                  l4->SkipData(static_cast<EbmlStream &>(*es),
                               l4->Generic().Context);
                  delete l4;
                  l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                           0xFFFFFFFFL, true, 1);
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxTrackVideo::ClassInfos.GlobalId) {
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Video track\n");
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el != 0)
                    break;

                  if (EbmlId(*l4) == KaxVideoPixelWidth::ClassInfos.GlobalId) {
                    KaxVideoPixelWidth &width =
                      *static_cast<KaxVideoPixelWidth *>(l4);
                    width.ReadData(es->I_O());
                    track->v_width = uint16(width);
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Pixel width: %u\n",
                           track->v_width);

                  } else if (EbmlId(*l4) ==
                             KaxVideoPixelHeight::ClassInfos.GlobalId) {
                    KaxVideoPixelHeight &height =
                      *static_cast<KaxVideoPixelHeight *>(l4);
                    height.ReadData(es->I_O());
                    track->v_height = uint16(height);
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Pixel height: "
                           "%u\n", track->v_height);

                  } else if (EbmlId(*l4) ==
                             KaxVideoDisplayWidth::ClassInfos.GlobalId) {
                    KaxVideoDisplayWidth &width =
                      *static_cast<KaxVideoDisplayWidth *>(l4);
                    width.ReadData(es->I_O());
                    track->v_dwidth = uint16(width);
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Display width: "
                           "%u\n", track->v_dwidth);

                  } else if (EbmlId(*l4) ==
                             KaxVideoDisplayHeight::ClassInfos.GlobalId) {
                    KaxVideoDisplayHeight &height =
                      *static_cast<KaxVideoDisplayHeight *>(l4);
                    height.ReadData(es->I_O());
                    track->v_dheight = uint16(height);
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Display height: "
                           "%u\n", track->v_dheight);

                  } else if (EbmlId(*l4) ==
                             KaxVideoFrameRate::ClassInfos.GlobalId) {
                    KaxVideoFrameRate &framerate =
                      *static_cast<KaxVideoFrameRate *>(l4);
                    framerate.ReadData(es->I_O());
                    track->v_frate = float(framerate);
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Frame rate: %f\n",
                           float(framerate));

                  } else
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + unknown "
                           "element@4: %s\n", typeid(*l4).name());

                  l4->SkipData(static_cast<EbmlStream &>(*es),
                               l4->Generic().Context);
                  delete l4;
                  l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                           0xFFFFFFFFL, true, 1);
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxCodecID::ClassInfos.GlobalId) {
                KaxCodecID &codec_id = *static_cast<KaxCodecID*>(l3);
                codec_id.ReadData(es->I_O());
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Codec ID: %s\n",
                       &binary(codec_id));
                track->codec_id = strdup((char *)&binary(codec_id));

              } else if (EbmlId(*l3) == KaxCodecPrivate::ClassInfos.GlobalId) {
                KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate*>(l3);
                c_priv.ReadData(es->I_O());
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + CodecPrivate, length "
                       "%llu\n", c_priv.GetSize());
                track->private_size = c_priv.GetSize();
                if (track->private_size > 0) {
                  track->private_data = malloc(track->private_size);
                  if (track->private_data == NULL)
                    return 0;
                  memcpy(track->private_data, c_priv.GetBuffer(),
                         track->private_size);
                }

              } else if (EbmlId(*l3) ==
                         KaxTrackFlagDefault::ClassInfos.GlobalId) {
                KaxTrackFlagDefault &f_default = 
                  *static_cast<KaxTrackFlagDefault *>(l3);
                f_default.ReadData(es->I_O());
                track->default_track = uint32(f_default);
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Default flag: %u\n",
                       track->default_track);

              } else if (EbmlId(*l3) ==
                         KaxTrackLanguage::ClassInfos.GlobalId) {
                KaxTrackLanguage &language =
                  *static_cast<KaxTrackLanguage *>(l3);
                language.ReadData(es->I_O());
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Language: %s\n",
                        string(language).c_str());
                if (track->language != NULL)
                  free(track->language);
                track->language = strdup(string(language).c_str());

              } else if ((!(EbmlId(*l3) ==
                            KaxTrackFlagLacing::ClassInfos.GlobalId)) &&
                         (!(EbmlId(*l3) ==
                            KaxTrackMinCache::ClassInfos.GlobalId)) &&
                         (!(EbmlId(*l3) ==
                            KaxTrackMaxCache::ClassInfos.GlobalId)))
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + unknown element@3: "
                       "%s\n", typeid(*l3).name());

              if (upper_lvl_el > 0) {	// we're coming from l4
                upper_lvl_el--;
                delete l3;
                l3 = l4;
                if (upper_lvl_el > 0)
                  break;
              } else {
                l3->SkipData(static_cast<EbmlStream &>(*es),
                             l3->Generic().Context);
                delete l3;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
              }
            } // while (l3 != NULL)

          } else
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + unknown element@2: %s\n",
                   typeid(*l2).name());
          if (upper_lvl_el > 0) {	// we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;
          } else {
            l2->SkipData(static_cast<EbmlStream &>(*es),
                         l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        } // while (l2 != NULL)

      } else if (EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId) {
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ found seek head\n");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxSeek::ClassInfos.GlobalId) {
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + seek entry\n");

            seek_pos = 0;
            seek_element_is_cue = 0;

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
                break;

              if (EbmlId(*l3) == KaxSeekID::ClassInfos.GlobalId) {
                binary *b;
                int s;
                KaxSeekID &seek_id = static_cast<KaxSeekID &>(*l3);
                seek_id.ReadData(es->I_O());
                b = seek_id.GetBuffer();
                s = seek_id.GetSize();
                EbmlId id(b, s);
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + seek ID: ");
                for (i = 0; i < s; i++)
                  mp_msg(MSGT_DEMUX, MSGL_V, "0x%02x ",
                         ((unsigned char *)b)[i]);
                mp_msg(MSGT_DEMUX, MSGL_V, "(%s)\n",
                        (id == KaxInfo::ClassInfos.GlobalId) ?
                        "KaxInfo" :
                        (id == KaxCluster::ClassInfos.GlobalId) ?
                        "KaxCluster" :
                        (id == KaxTracks::ClassInfos.GlobalId) ?
                        "KaxTracks" :
                        (id == KaxCues::ClassInfos.GlobalId) ?
                        "KaxCues" :
                        (id == KaxAttachements::ClassInfos.GlobalId) ?
                        "KaxAttachements" :
                        (id == KaxChapters::ClassInfos.GlobalId) ?
                        "KaxChapters" :
                        "unknown");

                if (id == KaxCues::ClassInfos.GlobalId)
                  seek_element_is_cue = 1;

              } else if (EbmlId(*l3) == KaxSeekPosition::ClassInfos.GlobalId) {
                KaxSeekPosition &kax_seek_pos =
                  static_cast<KaxSeekPosition &>(*l3);
                kax_seek_pos.ReadData(es->I_O());
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + seek position: %llu\n",
                        uint64(kax_seek_pos));

                seek_pos = uint64(kax_seek_pos);

              } else
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + unknown element, "
                       "level 3: %s\n", typeid(*l3).name());

              l3->SkipData(static_cast<EbmlStream &>(*es),
                           l3->Generic().Context);
              delete l3;
              l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true, 1);
            } // while (l3 != NULL)

            if (!mkv_d->cues_found && (seek_pos > 0) &&
                seek_element_is_cue && (s->end_pos != 0)) {
              current_pos = io.getFilePointer();
              io.setFilePointer(mkv_d->segment->GetGlobalPosition(seek_pos));
              mkv_d->cues_found = parse_cues(mkv_d);
              if (s->eof)
                stream_reset(s);
              io.setFilePointer(current_pos);
            }

          } else
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + unknown element, level 2: "
                   "%s\n", typeid(*l2).name());

          if (upper_lvl_el > 0) {		// we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;

          } else {
            l2->SkipData(static_cast<EbmlStream &>(*es),
                         l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        } // while (l2 != NULL)

      } else if ((EbmlId(*l1) == KaxCues::ClassInfos.GlobalId) &&
                 !mkv_d->cues_found) {
        // If the cues are up front then by all means read them now!
        current_pos = io.getFilePointer();
        io.setFilePointer(l1->GetElementPosition());
        mkv_d->cues_found = parse_cues(mkv_d);
        stream_reset(s);
        io.setFilePointer(current_pos);

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ found cluster, headers are "
               "parsed completely :)\n");
        add_cluster_position(mkv_d, l1->GetElementPosition());
        mkv_d->saved_l1 = l1;
        exit_loop = 1;

      } else
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ unknown element@1: %s\n",
               typeid(*l1).name());
      
      if (exit_loop)      // we've found the first cluster, so get out
        break;

      if (upper_lvl_el > 0) {		// we're coming from l2
        upper_lvl_el--;
        delete l1;
        l1 = l2;
        if (upper_lvl_el > 0)
          break;
      } else {
        l1->SkipData(static_cast<EbmlStream &>(*es), l1->Generic().Context);
        delete l1;
        l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
      }
    } // while (l1 != NULL)

    if (!exit_loop) {
      free_mkv_demuxer(mkv_d);
      return 0;
    }

  } catch (exception &ex) {
    mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] caught exception\n");
    return 0;
  }

  if (!check_track_information(mkv_d)) {
    free_mkv_demuxer(mkv_d);
    return 0;
  }

  track = NULL;
  if (demuxer->video->id == -1) { // Automatically select a video track.
    // Search for a video track that has the 'default' flag set.
    for (i = 0; i < mkv_d->num_tracks; i++)
      if ((mkv_d->tracks[i]->type == 'v') && mkv_d->tracks[i]->ok &&
          mkv_d->tracks[i]->default_track) {
        track = mkv_d->tracks[i];
        break;
      }

    if (track == NULL)
      // No track has the 'default' flag set - let's take the first video
      // track.
      for (i = 0; i < mkv_d->num_tracks; i++)
        if ((mkv_d->tracks[i]->type == 'v') && mkv_d->tracks[i]->ok) {
          track = mkv_d->tracks[i];
          break;
        }
  } else if (demuxer->video->id != -2) // -2 = no video at all
    track = find_track_by_num(mkv_d, demuxer->video->id, NULL);

  if (track) {
    if (track->ms_compat) {         // MS compatibility mode
      mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] Will play video track %u\n",
             track->tnum);
      sh_v = new_sh_video(demuxer, track->tnum);
      sh_v->bih = (BITMAPINFOHEADER *)calloc(1, sizeof(BITMAPINFOHEADER));
      if (sh_v->bih == NULL) {
        free_mkv_demuxer(mkv_d);
        return 0;
      }

      memcpy(sh_v->bih, track->private_data, sizeof(BITMAPINFOHEADER));
      sh_v->format = sh_v->bih->biCompression;
      sh_v->fps = track->v_frate;
      sh_v->frametime = 1 / track->v_frate;
      sh_v->disp_w = track->v_width;
      sh_v->disp_h = track->v_height;
      sh_v->aspect = (float)track->v_dwidth / (float)track->v_dheight;
      mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Aspect: %f\n", sh_v->aspect);

      demuxer->video->id = track->tnum;
      demuxer->video->sh = sh_v;
      sh_v->ds = demuxer->video;

      mkv_d->video = track;
    } else {
      mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Native CodecIDs not supported at "
             "the moment (track %u).\n", track->tnum);
      demuxer->video->id = -2;
    }
  } else {
    mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] No video track found/wanted.\n");
    demuxer->video->id = -2;
  }

  track = NULL;
  if (demuxer->audio->id == -1) { // Automatically select an audio track.
    // Search for an audio track that has the 'default' flag set.
    for (i = 0; i < mkv_d->num_tracks; i++)
      if ((mkv_d->tracks[i]->type == 'a') && mkv_d->tracks[i]->ok &&
          mkv_d->tracks[i]->default_track) {
        track = mkv_d->tracks[i];
        break;
      }

    if (track == NULL)
      // No track has the 'default' flag set - let's take the first audio
      // track.
      for (i = 0; i < mkv_d->num_tracks; i++)
        if ((mkv_d->tracks[i]->type == 'a') && mkv_d->tracks[i]->ok) {
          track = mkv_d->tracks[i];
          break;
        }
  } else if (demuxer->audio->id != -2) // -2 = no audio at all
    track = find_track_by_num(mkv_d, demuxer->audio->id, NULL);

  if (track) {
    mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] Will play audio track %u\n",
           track->tnum);
    sh_a = new_sh_audio(demuxer, track->tnum);

    demuxer->audio->id = track->tnum;
    demuxer->audio->sh = sh_a;
    sh_a->ds = demuxer->audio;

    mkv_d->audio = track;

    if (track->ms_compat) {
      sh_a->wf = (WAVEFORMATEX *)calloc(1, track->private_size);
      if (sh_a->wf == NULL) {
        free_mkv_demuxer(mkv_d);
        return 0;
      }
      memcpy(sh_a->wf, track->private_data, track->private_size);
    } else {
      sh_a->wf = (WAVEFORMATEX *)calloc(1, sizeof(WAVEFORMATEX));
      if (sh_a->wf == NULL) {
        free_mkv_demuxer(mkv_d);
        return 0;
      }
    }
    sh_a->format = sh_a->wf->wFormatTag = track->a_formattag;
    sh_a->channels = sh_a->wf->nChannels = track->a_channels;
    sh_a->samplerate = sh_a->wf->nSamplesPerSec = (uint32_t)track->a_sfreq;
    if (!strcmp(track->codec_id, MKV_A_MP3)) {
      sh_a->wf->nAvgBytesPerSec = 16000;
      sh_a->wf->nBlockAlign = 1152;
      sh_a->wf->wBitsPerSample = 0;
      sh_a->samplesize = 0;
    } else if (!strcmp(track->codec_id, MKV_A_AC3)) {
      sh_a->wf->nAvgBytesPerSec = 16000;
      sh_a->wf->nBlockAlign = 1536;
      sh_a->wf->wBitsPerSample = 0;
      sh_a->samplesize = 0;
    } else if (!strcmp(track->codec_id, MKV_A_PCM)) {
      sh_a->wf->nAvgBytesPerSec = sh_a->channels * sh_a->samplerate * 2;
      sh_a->wf->nBlockAlign = sh_a->wf->nAvgBytesPerSec;
      sh_a->wf->wBitsPerSample = track->a_bps;
      sh_a->samplesize = track->a_bps / 8;
    } else if (!strcmp(track->codec_id, MKV_A_VORBIS)) {
      for (i = 0; i < 3; i++) {
        dp = new_demux_packet(track->header_sizes[i]);
        memcpy(dp->buffer, track->headers[i], track->header_sizes[i]);
        dp->pts = 0;
        dp->flags = 0;
        ds_add_packet(demuxer->audio, dp);
      }
    }
  } else {
    mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] No audio track found/wanted.\n");
    demuxer->audio->id = -2;
  }

  // DO NOT automatically select a subtitle track and behave like DVD
  // playback: only show subtitles if the user explicitely wants them.
  track = NULL;
  if (demuxer->sub->id >= 0)
    track = find_track_by_num(mkv_d, demuxer->sub->id, NULL);
  else if (dvdsub_lang != NULL)
    track = find_track_by_language(mkv_d, dvdsub_lang, NULL);
  if (track) {
    if (strcmp(track->codec_id, MKV_S_TEXTASCII) &&
        strcmp(track->codec_id, MKV_S_TEXTUTF8))
      mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] Subtitle type '%s' is not "
             "supported. Track will not be displayed.\n", track->codec_id);
    else {
      mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] Will display subtitle track %u\n",
             track->tnum);
      mkv_d->subs_track = track;
      if (!mkv_d->subs.text[0]) {
        for (i = 0; i < SUB_MAX_TEXT; i++)
          mkv_d->subs.text[i] = (char *)malloc(256);

        if (!strcmp(track->codec_id, MKV_S_TEXTUTF8))
          sub_utf8 = 1;       // Force UTF-8 conversion.
      } else
        mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] File does not contain a "
               "subtitle track with the id %u.\n", demuxer->sub->id);
    }
  }

  if (s->end_pos == 0)
    demuxer->seekable = 0;
  else {
    demuxer->movi_start = s->start_pos;
    demuxer->movi_end = s->end_pos;
    demuxer->seekable = 1;
  }

  demuxer->priv = mkv_d;

  return 1;
}

extern "C" int demux_mkv_fill_buffer(demuxer_t *d) {
  demux_packet_t *dp;
  demux_stream_t *ds;
  mkv_demuxer_t *mkv_d;
  int upper_lvl_el, exit_loop, found_data, i, delete_element, elements_found;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL;
  EbmlStream *es;
  KaxBlock *block;
  int64_t block_duration, block_ref1, block_ref2;

  mkv_d = (mkv_demuxer_t *)d->priv;
  es = mkv_d->es;
  l0 = mkv_d->segment;

  // End of stream
  if (mkv_d->saved_l1 == NULL)
    return 0;

  exit_loop = 0;
  upper_lvl_el = 0;
  l1 = mkv_d->saved_l1;
  mkv_d->saved_l1 = NULL;
  found_data = 0;
  try {
    // The idea is not to handle a complete KaxCluster with each call to
    // demux_mkv_fill_buffer because those might be rather big.
    while (l1 != NULL)  {
      if ((upper_lvl_el != 0) || exit_loop)
        break;

      if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        mkv_d->cluster = (KaxCluster *)l1;
        if (found_data) {
          mkv_d->saved_l1 = l1;
          break;
        }

        if (mkv_d->saved_l2 != NULL) {
          l2 = mkv_d->saved_l2;
          mkv_d->saved_l2 = NULL;
        } else
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          // Handle at least one data packets in one call to
          // demux_mkv_fill_buffer - but abort if we have found that.
          if (found_data >= 1) {
            mkv_d->saved_l2 = l2;
            mkv_d->saved_l1 = l1;
            exit_loop = 1;
            break;
          }

          if (EbmlId(*l2) == KaxClusterTimecode::ClassInfos.GlobalId) {
            KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
            ctc.ReadData(es->I_O());
            mkv_d->cluster_tc = uint64(ctc);
            mkv_d->cluster->InitTimecode(mkv_d->cluster_tc);

          } else if (EbmlId(*l2) == KaxBlockGroup::ClassInfos.GlobalId) {

            block = NULL;
            block_duration = -1;
            block_ref1 = 0;
            block_ref2 = 0;
            elements_found = 0;

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              delete_element = 1;
              if (upper_lvl_el > 0)
                break;

              if (EbmlId(*l3) == KaxBlock::ClassInfos.GlobalId) {
                block = static_cast<KaxBlock *>(l3);
                block->SetParent(*mkv_d->cluster);
                block->ReadData(es->I_O());
                delete_element = 0;
                elements_found |= 1;

              } else if (EbmlId(*l3) ==
                         KaxBlockDuration::ClassInfos.GlobalId) {
                KaxBlockDuration &duration =
                  *static_cast<KaxBlockDuration *>(l3);
                duration.ReadData(es->I_O());
                block_duration = (int64_t)uint64(duration);
                elements_found |= 2;

              } else if (EbmlId(*l3) ==
                         KaxReferenceBlock::ClassInfos.GlobalId) {
                KaxReferenceBlock &ref =
                  *static_cast<KaxReferenceBlock *>(l3);
                ref.ReadData(es->I_O());
                if (block_ref1 == 0) {
                  block_ref1 = int64(ref);
                  elements_found |= 4;
                } else {
                  block_ref2 = int64(ref);
                  elements_found |= 8;
                }

              }

              l3->SkipData(static_cast<EbmlStream &>(*es),
                           l3->Generic().Context);
              if (delete_element)
                delete l3;
              l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true, 1);
            } // while (l3 != NULL)

            if (block != NULL) {
              // Clear the subtitles if they're obsolete now.
              if ((mkv_d->clear_subs_at > 0) &&
                  (mkv_d->clear_subs_at <=
                   (block->GlobalTimecode() / 1000000))) {
                mkv_d->subs.lines = 0;
                vo_sub = &mkv_d->subs;
                vo_osd_changed(OSDTYPE_SUBTITLE);
                mkv_d->clear_subs_at = 0;
              }

              ds = NULL;
              if ((mkv_d->video != NULL) &&
                  (mkv_d->video->tnum == block->TrackNum()))
                ds = d->video;
              else if ((mkv_d->audio != NULL) && 
                       (mkv_d->audio->tnum == block->TrackNum()))
                  ds = d->audio;

              if (!mkv_d->skip_to_keyframe ||     // Not skipping is ok.
                  (((elements_found & 4) == 0) && // It's a key frame.
                   (ds != NULL) &&                // Corresponding track found
                   (ds == d->video))) {           // track is our video track
                if ((ds != NULL) && ((block->GlobalTimecode() / 1000000) >=
                                     (uint64_t)mkv_d->skip_to_timecode)) {
                  for (i = 0; i < (int)block->NumberFrames(); i++) {
                    DataBuffer &data = block->GetBuffer(i);
                    dp = new_demux_packet(data.Size());
                    memcpy(dp->buffer, data.Buffer(), data.Size());
                    dp->pts = mkv_d->last_pts;
                    dp->flags = (elements_found & 4) == 0 ? 1 : 0; // keyframe
                    ds_add_packet(ds, dp);
                    found_data++;
                  }
                  mkv_d->skip_to_keyframe = 0;
                  mkv_d->skip_to_timecode = 0;
                } else if ((mkv_d->subs_track != NULL) &&
                           (mkv_d->subs_track->tnum == block->TrackNum()))
                  handle_subtitles(d, block, block_duration);

                d->filepos = mkv_d->in->getFilePointer();
                mkv_d->last_pts = (float)block->GlobalTimecode() /
                  1000000000.0;
                mkv_d->last_filepos = d->filepos;
              }

              delete block;
            } // block != NULL

          }

          if (upper_lvl_el > 0) {		// we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;
          } else {
            l2->SkipData(static_cast<EbmlStream &>(*es),
                         l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        } // while (l2 != NULL)
      } else if (EbmlId(*l1) == KaxCues::ClassInfos.GlobalId)
        return 0;
      else
         mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Unknown element@1: %s (stream "
                "position %llu)\n", typeid(*l1).name(),
                l1->GetElementPosition());

      if (exit_loop)
        break;

      if (upper_lvl_el > 0) {		// we're coming from l2
        upper_lvl_el--;
        delete l1;
        l1 = l2;
        if (upper_lvl_el > 0)
          break;
      } else {
        l1->SkipData(static_cast<EbmlStream &>(*es), l1->Generic().Context);
        delete l1;
        l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        if ((l1 != NULL) && (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId))
          add_cluster_position(mkv_d, l1->GetElementPosition());
      }
    } // while (l1 != NULL)
  } catch (exception ex) {
    mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] exception caught\n");
    return 0;
  }

  if (found_data)
    return 1;

  return 0;
}

extern "C" void resync_audio_stream(sh_audio_t *sh_audio);

extern "C" void demux_mkv_seek(demuxer_t *demuxer, float rel_seek_secs,
                               int flags) {
  int i, k, upper_lvl_el;
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *)demuxer->priv;
  int64_t target_timecode, target_filepos = 0, min_diff, diff, current_pos;
  int64_t cluster_pos;
  mkv_track_index_t *index;
  mkv_index_entry_t *entry;
  EbmlElement *l1;

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] SEEK, relss: %.3f, flags: %d\n",
         rel_seek_secs, flags);

  if (!mkv_d->cues_found && !mkv_d->cues_searched) {
    // We've not found an index so far. So let's skip over all level 1
    // elements until we either hit another segment, the end of the file
    // or - suprise - some cues.
    current_pos = mkv_d->in->getFilePointer();

    // Skip the data but do not delete the element! This is our current
    // cluster, and we need it later on in demux_mkv_fill_buffer.
    l1 = mkv_d->saved_l1;
    l1->SkipData(static_cast<EbmlStream &>(*mkv_d->es), l1->Generic().Context);
    l1 = mkv_d->es->FindNextElement(mkv_d->segment->Generic().Context,
                                    upper_lvl_el, 0xFFFFFFFFL, true, 1);
    while (l1 != NULL) {
      if (upper_lvl_el)
        break;

      if (EbmlId(*l1) == KaxCues::ClassInfos.GlobalId) {
        mkv_d->in->setFilePointer(l1->GetElementPosition());
        delete l1;
        mkv_d->cues_found = parse_cues(mkv_d);
        break;
      } else {
        if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId)
          add_cluster_position(mkv_d, l1->GetElementPosition());
        l1->SkipData(static_cast<EbmlStream &>(*mkv_d->es),
                     l1->Generic().Context);
        delete l1;
        l1 = mkv_d->es->FindNextElement(mkv_d->segment->Generic().Context,
                                        upper_lvl_el, 0xFFFFFFFFL, true, 1);
      }
    }

    if (demuxer->stream->eof)
      stream_reset(demuxer->stream);
    mkv_d->in->setFilePointer(current_pos);

    mkv_d->cues_searched = 1;
  }

  if (!(flags & 2)) {           // Time in secs
    if (flags & 1)              // Absolute seek
      target_timecode = 0;
    else                        // Relative seek
      target_timecode = (int64_t)(mkv_d->last_pts * 1000.0);
    target_timecode += (int64_t)(rel_seek_secs * 1000.0);
    if (target_timecode < 0)
      target_timecode = 0;

    min_diff = 0xFFFFFFFL;

    // Let's find the entry in the index with the smallest difference
    // to the wanted timecode.
    entry = NULL;
    for (i = 0; i < mkv_d->num_indexes; i++)
      if (mkv_d->index[i].tnum == mkv_d->video->tnum) {
        index = &mkv_d->index[i];
        for (k = 0; k < index->num_entries; k++) {
          if (!index->entries[k].is_key)
            continue;
          diff = target_timecode - (int64_t)index->entries[k].timecode;
          if (diff < 0)
            diff *= -1;
          if (diff < min_diff) {
            min_diff = diff;
            entry = & index->entries[k];
          }
        }
        break;
      }

    if (mkv_d->saved_l1 != NULL)
      delete mkv_d->saved_l1;

    if (mkv_d->saved_l2 != NULL) {
      delete mkv_d->saved_l2;
      mkv_d->saved_l2 = NULL;
    }

    if (entry != NULL) {        // We've found an entry.
      mkv_d->in->setFilePointer(entry->filepos);
      upper_lvl_el = 0;
      mkv_d->saved_l1 =
        mkv_d->es->FindNextElement(mkv_d->segment->Generic().Context,
                                   upper_lvl_el, 0xFFFFFFFFL, true, 1);
    } else {                    // We've not found an entry --> no index?
      target_filepos = (int64_t)(target_timecode * mkv_d->last_filepos /
        (mkv_d->last_pts * 1000.0));
      mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] No index entry found. Calculated "
             "filepos %lld. Old timecode %lld.\n", target_filepos,
             (int64_t)(mkv_d->last_pts * 1000.0));
      // Let's find the nearest cluster so that libebml does not have to
      // do so much work.
      cluster_pos = 0;
      min_diff = 0x0FFFFFFFL;
      for (i = 0; i < mkv_d->num_cluster_pos; i++) {
        diff = mkv_d->cluster_positions[i] - target_filepos;
        if (rel_seek_secs < 0) {
          if ((diff > 0) && (diff < min_diff)) {
            cluster_pos = mkv_d->cluster_positions[i];
            min_diff = diff;
          }
        } else if (abs(diff) < min_diff) {
          cluster_pos = mkv_d->cluster_positions[i];
          min_diff = abs(diff);
        }
      }
      if (min_diff != 0x0FFFFFFFL) {
        target_filepos = cluster_pos;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] New target_filepos because of "
               "cluster: %lld.\n", target_filepos);
      }
      if (target_filepos >= demuxer->movi_end)
        return;
      mkv_d->in->setFilePointer(target_filepos);
      upper_lvl_el = 0;
      mkv_d->saved_l1 =
        mkv_d->es->FindNextElement(mkv_d->segment->Generic().Context,
                                   upper_lvl_el, 0xFFFFFFFFL, true, 1);
      mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] seek result: target_timecode %lld, "
             "did not find an entry. Calculated target_filspos: %lld\n",
             target_timecode, target_filepos);
      mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] seek found %p (%s).\n",
             mkv_d->saved_l1, mkv_d->saved_l1 == NULL ? "null" :
             typeid(*mkv_d->saved_l1).name());
    }

    if (mkv_d->video != NULL)
      mkv_d->skip_to_keyframe = 1;
    if (rel_seek_secs > 0.0)
      mkv_d->skip_to_timecode = target_timecode;

    demux_mkv_fill_buffer(demuxer);
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] New timecode: %lld\n",
           (int64_t)(mkv_d->last_pts * 1000.0));

    mkv_d->subs.lines = 0;
    vo_sub = &mkv_d->subs;
    vo_osd_changed(OSDTYPE_SUBTITLE);
    mkv_d->clear_subs_at = 0;

    if(demuxer->audio->sh != NULL)
      resync_audio_stream((sh_audio_t *)demuxer->audio->sh); 

  } else
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] seek unsupported flags\n");

}

extern "C" void demux_close_mkv(demuxer_t *demuxer) {
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *)demuxer->priv;

  free_mkv_demuxer(mkv_d);

#ifdef USE_ICONV
  subcp_close();
#endif
}

extern "C" int demux_mkv_control(demuxer_t *demuxer, int cmd, void *arg) {
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *)demuxer->priv;
  
  switch (cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH:
      if (mkv_d->duration == -1.0)
        return DEMUXER_CTRL_DONTKNOW;

      *((unsigned long *)arg) = (unsigned long)mkv_d->duration;
      return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_GET_PERCENT_POS:
      if (mkv_d->duration == -1.0) {
        if (demuxer->movi_start == demuxer->movi_end)
          return DEMUXER_CTRL_DONTKNOW;

        *((int *)arg) =
          (int)((demuxer->filepos - demuxer->movi_start) /
                ((demuxer->movi_end - demuxer->movi_start) / 100));
        return DEMUXER_CTRL_OK;
      }

      *((int *)arg) = (int)(100 * mkv_d->last_pts / mkv_d->duration);
      return DEMUXER_CTRL_OK; 

    default:
      return DEMUXER_CTRL_NOTIMPL;
  }
}

#endif /* HAVE_MATROSKA */
