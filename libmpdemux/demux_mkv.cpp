// Matroska demuxer
// written by Moritz Bunkus <moritz@bunkus.org>
// License: GPL of course ;)

// $Id$

extern "C" {
#include "config.h"
}

#ifdef HAVE_MATROSKA

#include <vector>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlContexts.h>
#include <ebml/EbmlVersion.h>
#include <ebml/StdIOCallback.h>

#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxContexts.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/FileKax.h>

extern "C" {
#include <ctype.h>
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

#include "matroska.h"

using namespace libebml;
using namespace libmatroska;
using namespace std;

#ifndef LIBEBML_VERSION
#define LIBEBML_VERSION 000000
#endif // LIBEBML_VERSION

#if LIBEBML_VERSION < 000500
#error libebml version too old - need at least 0.5.0
#endif

// for e.g. "-slang ger"
extern char *dvdsub_lang;
extern char *audio_lang;

// default values for Matroska elements
#define MKVD_TIMECODESCALE 1000000 // 1000000 = 1ms

#define MKV_SUBTYPE_TEXT                  1
#define MKV_SUBTYPE_SSA                   2
#define MKV_SUBTYPE_VOBSUB                3

#define MKV_SUBCOMPRESSION_NONE           0
#define MKV_SUBCOMPRESSION_ZLIB           1

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
  uint32_t tnum, xid;
  
  char *codec_id;
  int ms_compat;
  char *language;

  char type; // 'v' = video, 'a' = audio, 's' = subs
  
  char v_fourcc[5];
  uint32_t v_width, v_height, v_dwidth, v_dheight;
  float v_frate;

  uint32_t a_formattag;
  uint32_t a_channels, a_bps;
  float a_sfreq;

  int default_track;

  void *private_data;
  unsigned int private_size;

  // For Vorbis audio
  unsigned char *headers[3];
  uint32_t header_sizes[3];

  int ok;

  // Stuff for RealMedia
  bool realmedia;
  demux_packet_t *rm_dp;
  int rm_seqnum, rv_kf_base, rv_kf_pts;
  float rv_pts;                 // previous video timestamp
  float ra_pts;                 // previous audio timestamp

  // Stuff for QuickTime
  bool fix_i_bps;
  float qt_last_a_pts;

  // Stuff for VobSubs
  mkv_sh_sub_t sh_sub;
  int vobsub_compression;
} mkv_track_t;

typedef struct mkv_demuxer {
  float duration, last_pts;
  uint64_t last_filepos;

  mkv_track_t **tracks;
  int num_tracks;
  mkv_track_t *video, *audio, *subs_track;

  uint64_t tc_scale, cluster_tc, first_tc;

  mpstream_io_callback *in;

  uint64_t clear_subs_at[SUB_MAX_TEXT];

  subtitle subs;
  int subtitle_type;

  EbmlStream *es;
  EbmlElement *saved_l1, *saved_l2;
  KaxSegment *segment;
  KaxCluster *cluster;

  mkv_track_index_t *index;
  int num_indexes, cues_found, cues_searched;
  int64_t *cluster_positions;
  int num_cluster_pos;
  vector<uint64_t> *parsed_seekheads;
  vector<uint64_t> *parsed_cues;

  int64_t skip_to_timecode;
  bool v_skip_to_keyframe, a_skip_to_keyframe;
} mkv_demuxer_t;

typedef struct {
  uint32_t chunks;              // number of chunks
  uint32_t timestamp;           // timestamp from packet header
  uint32_t len;                 // length of actual data
  uint32_t chunktab;            // offset to chunk offset array
} dp_hdr_t;

#if __GNUC__ == 2
#pragma pack(2)
#else
#pragma pack(push,2)
#endif

typedef struct {
  uint32_t size;
  uint32_t fourcc1;
  uint32_t fourcc2;
  uint16_t width;
  uint16_t height;
  uint16_t bpp;
  uint32_t unknown1;
  uint32_t fps;
  uint32_t type1;
  uint32_t type2;
} real_video_props_t;

typedef struct {
  uint32_t fourcc1;             // '.', 'r', 'a', 0xfd
  uint16_t version1;            // 4 or 5
  uint16_t unknown1;            // 00 000
  uint32_t fourcc2;             // .ra4 or .ra5
  uint32_t unknown2;            // ???
  uint16_t version2;            // 4 or 5
  uint32_t header_size;         // == 0x4e
  uint16_t flavor;              // codec flavor id
  uint32_t coded_frame_size;    // coded frame size
  uint32_t unknown3;            // big number
  uint32_t unknown4;            // bigger number
  uint32_t unknown5;            // yet another number
  uint16_t sub_packet_h;
  uint16_t frame_size;
  uint16_t sub_packet_size;
  uint16_t unknown6;            // 00 00
  uint16_t sample_rate;
  uint16_t unknown8;            // 0
  uint16_t sample_size;
  uint16_t channels;
} real_audio_v4_props_t;

typedef struct {
  uint32_t fourcc1;             // '.', 'r', 'a', 0xfd
  uint16_t version1;            // 4 or 5
  uint16_t unknown1;            // 00 000
  uint32_t fourcc2;             // .ra4 or .ra5
  uint32_t unknown2;            // ???
  uint16_t version2;            // 4 or 5
  uint32_t header_size;         // == 0x4e
  uint16_t flavor;              // codec flavor id
  uint32_t coded_frame_size;    // coded frame size
  uint32_t unknown3;            // big number
  uint32_t unknown4;            // bigger number
  uint32_t unknown5;            // yet another number
  uint16_t sub_packet_h;
  uint16_t frame_size;
  uint16_t sub_packet_size;
  uint16_t unknown6;            // 00 00
  uint8_t unknown7[6];          // 0, srate, 0
  uint16_t sample_rate;
  uint16_t unknown8;            // 0
  uint16_t sample_size;
  uint16_t channels;
  uint32_t genr;                // "genr"
  uint32_t fourcc3;             // fourcc
} real_audio_v5_props_t;

// I have to (re)define this struct here because g++ will not compile
// components.h from the qtsdk if I include it.
typedef struct {
  uint32_t id_size;
  uint32_t codec_type;
  uint32_t reserved1;
  uint16_t reserved2;
  uint16_t data_reference_index;
  uint16_t version;
  uint16_t revision;
  uint32_t vendor;
  uint32_t temporal_quality;
  uint32_t spatial_quality;
  uint16_t width;
  uint16_t height;
  uint32_t horizontal_resolution; // 32bit fixed-point number
  uint32_t vertical_resolution; // 32bit fixed-point number
  uint32_t data_size;
  uint16_t frame_count;
  char compressor_name[32];
  uint16_t depth;
  uint16_t color_table_id;
} qt_image_description_t;

#if __GNUC__ == 2
#pragma pack()
#else
#pragma pack(pop)
#endif

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

static uint16_t get_uint16_be(const void *buf) {
  uint16_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);

  return ret;
}

static uint32_t get_uint32_be(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[3] & 0xff);

  return ret;
}

unsigned char read_char(unsigned char *p, int &pos, int size) {
  if ((pos + 1) > size)
    throw exception();
  pos++;
  return p[pos - 1];
}

unsigned short read_word(unsigned char *p, int &pos, int size) {
  unsigned short v;

  if ((pos + 2) > size)
    throw exception();
  v = p[pos];
  v = (v << 8) | (p[pos + 1] & 0xff);
  pos += 2;
  return v;
}

unsigned int read_dword(unsigned char *p, int &pos, int size) {
  unsigned int v;

  if ((pos + 4) > size)
    throw exception();
  v = p[pos];
  v = (v << 8) | (p[pos + 1] & 0xff);
  v = (v << 8) | (p[pos + 2] & 0xff);
  v = (v << 8) | (p[pos + 3] & 0xff);
  pos += 4;
  return v;
}

static void handle_subtitles(demuxer_t *d, KaxBlock *block, int64_t duration) {
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *)d->priv;
  int len, line, state, i;
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
  mkv_d->subs.lines++;
  s2 = mkv_d->subs.text[mkv_d->subs.lines - 1];
  state = 0;

  if (mkv_d->subtitle_type == MKV_SUBTYPE_SSA) {
    /* Matroska's SSA format does not have timecodes embedded into 
       the SAA line. Timescodes are encoded into the blocks timecode
       and duration. */

    /* Find text section. */
    for (i = 0; (i < 8) && (*s1 != 0); s1++)
      if (*s1 == ',')
        i++;

    if (*s1 == 0) {             // Broken line?
      mkv_d->subs.lines--;
      return;
    }

    /* Load text. */
    while ((unsigned int)(s1 - buffer) < data.Size()) {
      if (*s1 == '{')
        state = 1;
      else if ((*s1 == '}') && (state == 1))
        state = 2;

      if (state == 0) {
        *s2 = *s1;
        s2++;
        if ((s2 - mkv_d->subs.text[mkv_d->subs.lines - 1]) >= 255)
          break;
      }
      s1++;
      
      /* Newline */
      if ((*s1 == '\\') && ((unsigned int)(s1 + 1 - buffer) < data.Size()) &&
          ((*(s1 + 1) == 'N') || (*(s1 + 1) == 'n'))) {
        mkv_d->clear_subs_at[mkv_d->subs.lines - 1] = 
          block->GlobalTimecode() / 1000000 - mkv_d->first_tc + duration;
        
        mkv_d->subs.lines++;
        *s2 = 0;
        s2 = mkv_d->subs.text[mkv_d->subs.lines - 1];
        s1 += 2;
      }

      if (state == 2)
        state = 0;
    }
    *s2 = 0;

  } else {
    while ((unsigned int)(s1 - buffer) != data.Size()) {
      if ((*s1 == '\n') || (*s1 == '\r')) {
        if (state == 0) {       // normal char --> newline
          if (mkv_d->subs.lines == SUB_MAX_TEXT)
            break;
          *s2 = 0;
          mkv_d->clear_subs_at[mkv_d->subs.lines - 1]= 
            block->GlobalTimecode() / 1000000 - mkv_d->first_tc + duration;
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
  }

#ifdef USE_ICONV
  subcp_recode1(&mkv_d->subs);
#endif

  vo_sub = &mkv_d->subs;
  vo_osd_changed(OSDTYPE_SUBTITLE);

  mkv_d->clear_subs_at[mkv_d->subs.lines - 1] = 
    block->GlobalTimecode() / 1000000 - mkv_d->first_tc + duration;
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
                                      char track_type) {
  int i;

  for (i = 0; i < d->num_tracks; i++)
    if ((d->tracks[i] != NULL) && (d->tracks[i]->type == track_type) &&
        (d->tracks[i]->xid == n))
      return d->tracks[i];
  
  return NULL;
}

static mkv_track_t *find_duplicate_track_by_num(mkv_demuxer_t *d, uint32_t n,
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

static bool mkv_parse_idx(mkv_track_t *t) {
  uint32_t i, p, things_found;
  int idx;
  string line, s1, s2;
  char *src;

  if ((t->private_data == NULL) || (t->private_size < 1))
    return false;

  things_found = 0;
  i = 0;
  src = (char *)t->private_data;
  do {
    line = "";
    while ((i < t->private_size) && (src[i] != '\n') && (src[i] != '\r')) {
      if (!isspace(src[i]))
        line += src[i];
      i++;
    }
    while ((i < t->private_size) && ((src[i] == '\n') || (src[i] == '\r')))
      i++;

    if (!strncasecmp(line.c_str(), "size:", 5)) {
      s1 = line.substr(5);
      idx = s1.find('x');
      if (idx >= 0) {
        s2 = s1.substr(idx + 1);
        s1.erase(idx);
        t->sh_sub.width = strtol(s1.c_str(), NULL, 10);
        t->sh_sub.height = strtol(s2.c_str(), NULL, 10);
        things_found |= 1;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] VobSub IDX parser: size: %d x %d\n",
               t->sh_sub.width, t->sh_sub.height);
      }

    } else if (!strncasecmp(line.c_str(), "palette:", 8)) {
      s1 = line.substr(8);
      for (p = 0; p < 15; p++) {
        idx = s1.find(',');
        if (idx < 0)
          break;
        s2 = s1.substr(0, idx);
        s1.erase(0, idx + 1);
        t->sh_sub.palette[p] = (unsigned int)strtol(s2.c_str(), NULL, 16);
      }
      if (idx >= 0) {
        t->sh_sub.palette[15] = (unsigned int)strtol(s1.c_str(), NULL, 16);
        things_found |= 2;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] VobSub IDX parser: palette: 0x%06x "
               "0x%06x 0x%06x 0x%06x 0x%06x 0x%06x 0x%06x 0x%06x 0x%06x "
               "0x%06x 0x%06x 0x%06x 0x%06x 0x%06x 0x%06x 0x%06x\n",
               t->sh_sub.palette[0], t->sh_sub.palette[1],
               t->sh_sub.palette[2], t->sh_sub.palette[3],
               t->sh_sub.palette[4], t->sh_sub.palette[5],
               t->sh_sub.palette[6], t->sh_sub.palette[7],
               t->sh_sub.palette[8], t->sh_sub.palette[9],
               t->sh_sub.palette[10], t->sh_sub.palette[11],
               t->sh_sub.palette[12], t->sh_sub.palette[13],
               t->sh_sub.palette[14], t->sh_sub.palette[15]);
      }
    }

  } while ((i != t->private_size) && (things_found != 3));
  t->sh_sub.type = 'v';

  return (things_found == 3);
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
          }
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
        mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] Track ID %u: video (%s), "
               "-vid %u\n", t->tnum, t->codec_id, t->xid);

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
            if (t->a_bps != u) {
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
          else if (!strncmp(t->codec_id, MKV_A_AC3, strlen(MKV_A_AC3)))
            t->a_formattag = 0x2000;
          else if (!strcmp(t->codec_id, MKV_A_DTS))
            // uses same format tag as AC3, only supported with -hwac3
            t->a_formattag = 0x2000;
          else if (!strcmp(t->codec_id, MKV_A_PCM) ||
                   !strcmp(t->codec_id, MKV_A_PCM_BE))
            t->a_formattag = 0x0001;
          else if (!strcmp(t->codec_id, MKV_A_AAC_2MAIN) ||
                   !strcmp(t->codec_id, MKV_A_AAC_2LC) ||
                   !strcmp(t->codec_id, MKV_A_AAC_2SSR) ||
                   !strcmp(t->codec_id, MKV_A_AAC_4MAIN) ||
                   !strcmp(t->codec_id, MKV_A_AAC_4LC) ||
                   !strcmp(t->codec_id, MKV_A_AAC_4SSR) ||
                   !strcmp(t->codec_id, MKV_A_AAC_4LTP) ||
                   !strcmp(t->codec_id, MKV_A_AAC_4SBR))
            t->a_formattag = mmioFOURCC('M', 'P', '4', 'A');
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
          } else if (t->private_size >= sizeof(real_audio_v4_props_t)) {
            if (!strcmp(t->codec_id, MKV_A_REAL28))
              t->a_formattag = mmioFOURCC('2', '8', '_', '8');
            else if (!strcmp(t->codec_id, MKV_A_REALATRC))
              t->a_formattag = mmioFOURCC('a', 't', 'r', 'c');
            else if (!strcmp(t->codec_id, MKV_A_REALCOOK))
              t->a_formattag = mmioFOURCC('c', 'o', 'o', 'k');
            else if (!strcmp(t->codec_id, MKV_A_REALDNET))
              t->a_formattag = mmioFOURCC('d', 'n', 'e', 't');
            else if (!strcmp(t->codec_id, MKV_A_REALSIPR))
              t->a_formattag = mmioFOURCC('s', 'i', 'p', 'r');
          } else if (!strcmp(t->codec_id, MKV_A_QDMC) ||
                     !strcmp(t->codec_id, MKV_A_QDMC2)) {
            ;
          } else {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Unknown/unsupported audio "
                   "codec ID '%s' for track %u or missing/faulty private "
                   "codec data.\n", t->codec_id, t->tnum);
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

        // This track seems to be ok.
        t->ok = 1;
        mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] Track ID %u: audio (%s), -aid "
               "%u%s%s\n", t->tnum, t->codec_id, t->xid,
               t->language != NULL ? ", -alang " : "",
               t->language != NULL ? t->language : "");
        break;

      case 's':
        if (!strncmp(t->codec_id, MKV_S_VOBSUB, strlen(MKV_S_VOBSUB))) {
          if (mkv_parse_idx(t)) {
            t->ok = 1;
            mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] Track ID %u: subtitles (%s), "
                   "-sid %u%s%s\n", t->tnum, t->codec_id,
                   t->xid, t->language != NULL ? ", -slang " : "",
                   t->language != NULL ? t->language : "");
          }

        } else {                // Text subtitles do not need any data
          t->ok = 1;            // except the CodecID.
          mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] Track ID %u: subtitles (%s), "
                 "-sid %u%s%s\n", t->tnum, t->codec_id, t->xid,
                 t->language != NULL ? ", -slang " : "",
                 t->language != NULL ? t->language : "");
        }
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

static int find_in_vector(vector<uint64_t> &vec, uint64_t value) {
  unsigned int i;

  for (i = 0; i < vec.size(); i++)
    if (vec[i] == value)
      return 1;

  return 0;
}

#define in_parent(p) (mkv_d->in->getFilePointer() < \
                      (p->GetElementPosition() + p->ElementSize()))
#define FINDFIRST(p, c) (static_cast<c *> \
  (((EbmlMaster *)p)->FindFirstElt(c::ClassInfos, false)))
#define FINDNEXT(p, c, e) (static_cast<c *> \
  (((EbmlMaster *)p)->FindNextElt(*e, false)))

static void parse_cues(mkv_demuxer_t *mkv_d, uint64_t pos) {
  EbmlElement *l2 = NULL;
  EbmlStream *es;
  KaxCues *cues;
  KaxCuePoint *cpoint;
  KaxCueTime *ctime;
  KaxCueClusterPosition *ccpos;
  KaxCueTrack *ctrack;
  KaxCueTrackPositions *ctrackpos;
  KaxCueReference *cref;
  int upper_lvl_el, i, k;
  uint64_t tc_scale, filepos = 0, timecode = 0;
  uint32_t tnum = 0;
  mkv_index_entry_t *entry;

  if (find_in_vector(*mkv_d->parsed_cues, pos))
    return;

  mkv_d->parsed_cues->push_back(pos);

  mkv_d->in->setFilePointer(pos);

  es = mkv_d->es;
  tc_scale = mkv_d->tc_scale;
  upper_lvl_el = 0;

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] /---- [ parsing cues ] -----------\n");

  cues = (KaxCues *)es->FindNextElement(mkv_d->segment->Generic().Context,
                                        upper_lvl_el, 0xFFFFFFFFL, true, 1);
  if (cues == NULL)
    return;

  if (!(EbmlId(*cues) == KaxCues::ClassInfos.GlobalId)) {
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] No KaxCues element found but %s.\n"
           "[mkv] \\---- [ parsing cues ] -----------\n",
           cues->Generic().DebugName);
    
    return;
  }

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ found cues\n");

  cues->Read(*es, KaxCues::ClassInfos.Context, upper_lvl_el, l2, true);

  cpoint = FINDFIRST(cues, KaxCuePoint);

  while (cpoint != NULL) {
    mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] | + found cue point\n");

    ctime = FINDFIRST(cpoint, KaxCueTime);
    if (ctime == NULL) {
      cpoint = FINDNEXT(cues, KaxCuePoint, cpoint);
      continue;
    }
      
    timecode = uint64(*ctime) * tc_scale / 1000000 - mkv_d->first_tc;
    mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |  + found cue time: %.3fs\n",
           (float)timecode / 1000.0);

    ctrackpos = FINDFIRST(cpoint, KaxCueTrackPositions);

    while (ctrackpos != NULL) {
      ctrack = FINDFIRST(ctrackpos, KaxCueTrack);

      if (ctrack == NULL) {
        ctrackpos = FINDNEXT(cpoint, KaxCueTrackPositions, ctrackpos);
        continue;
      }

      tnum = uint32(*ctrack);
      mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |   + found cue track: %u\n", tnum);

      ccpos = FINDFIRST(ctrackpos, KaxCueClusterPosition);
      if (ccpos == NULL) {
        ctrackpos = FINDNEXT(cpoint, KaxCueTrackPositions, ctrackpos);
        continue;
      }

      filepos = mkv_d->segment->GetGlobalPosition(uint64_t(*ccpos));
      mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] |   + found cue cluster "
             "position: %llu\n", filepos);

      cref = FINDFIRST(ctrackpos, KaxCueReference);
      add_index_entry(mkv_d, tnum, filepos, timecode,
                      cref == NULL ? 1 : 0);
     
      ctrackpos = FINDNEXT(cpoint, KaxCueTrackPositions, ctrackpos);
    }

    cpoint = FINDNEXT(cues, KaxCuePoint, cpoint);
  }

  delete cues;

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

  mkv_d->cues_found = 1;
}

static void parse_seekhead(mkv_demuxer_t *mkv_d, uint64_t pos) {
  EbmlElement *l2 = NULL;
  EbmlStream *es;
  KaxSeekHead *kseekhead;
  KaxSeek *kseek;
  KaxSeekID *ksid;
  KaxSeekPosition *kspos;
  int upper_lvl_el, i, k, s;
  uint64_t seek_pos;
  EbmlId *id;
  EbmlElement *e;
  binary *b;

  if (find_in_vector(*mkv_d->parsed_seekheads, pos))
    return;

  mkv_d->parsed_seekheads->push_back(pos);

  es = mkv_d->es;
  upper_lvl_el = 0;

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] /---- [ parsing seek head ] ---------\n");

  mkv_d->in->setFilePointer(pos);

  kseekhead =
    (KaxSeekHead *)es->FindNextElement(mkv_d->segment->Generic().Context,
                                       upper_lvl_el, 0xFFFFFFFFL, true, 1);
  if (kseekhead == NULL)
    return;

  if (!(EbmlId(*kseekhead) == KaxSeekHead::ClassInfos.GlobalId)) {
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] No KaxSeekead element found but %s.\n"
           "[mkv] \\---- [ parsing seek head ] ---------\n",
           kseekhead->Generic().DebugName);
    
    return;
  }

  kseekhead->Read(*es, KaxSeekHead::ClassInfos.Context, upper_lvl_el, l2,
                  true);

  for (i = 0; i < (int)kseekhead->ListSize(); i++) {
    kseek = (KaxSeek *)(*kseekhead)[i];
    if (!(EbmlId(*kseek) == KaxSeek::ClassInfos.GlobalId))
      continue;

    seek_pos = 0;
    id = NULL;

    for (k = 0; k < (int)kseek->ListSize(); k++) {
      e = (*kseek)[k];

      if (EbmlId(*e) == KaxSeekID::ClassInfos.GlobalId) {
        ksid = (KaxSeekID *)e;

        b = ksid->GetBuffer();
        s = ksid->GetSize();
        if (id != NULL)
          delete id;
        id = new EbmlId(b, s);

      } else if (EbmlId(*e) == KaxSeekPosition::ClassInfos.GlobalId) {
        kspos = (KaxSeekPosition *)e;
        seek_pos = mkv_d->segment->GetGlobalPosition(uint64(*kspos));

      }
    }

    if ((seek_pos != 0) && (id != NULL)) {
      if (*id == KaxSeekHead::ClassInfos.GlobalId)
        parse_seekhead(mkv_d, seek_pos);
      else if (*id == KaxCues::ClassInfos.GlobalId)
        parse_cues(mkv_d, seek_pos);
    }

    if (id != NULL)
      delete id;
  }

  delete kseekhead;

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] \\---- [ parsing seek head ] ---------\n");
}

extern "C" void print_wave_header(WAVEFORMATEX *h);

extern "C" int demux_mkv_open(demuxer_t *demuxer) {
  unsigned char signature[4];
  stream_t *s;
  demux_packet_t *dp;
  mkv_demuxer_t *mkv_d;
  int upper_lvl_el, exit_loop, i, vid, sid, aid;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL;
  EbmlStream *es;
  mkv_track_t *track;
  sh_audio_t *sh_a;
  sh_video_t *sh_v;
  vector<uint64_t> seekheads_to_parse;
  vector<uint64_t> cues_to_parse;
  int64_t current_pos;
  qt_image_description_t *idesc;

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
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFFFFFFFFFULL);
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
    l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFULL);
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
    mkv_d->tc_scale = MKVD_TIMECODESCALE;
    mkv_d->parsed_seekheads = new vector<uint64_t>;
    mkv_d->parsed_cues = new vector<uint64_t>;

    vid = 0;
    aid = 0;
    sid = 0;

    upper_lvl_el = 0;
    exit_loop = 0;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        KaxTimecodeScale *ktc_scale;
        KaxDuration *kduration;

        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ segment information...\n");

        l1->Read(*es, KaxInfo::ClassInfos.Context, upper_lvl_el, l2, true);

        ktc_scale = FINDFIRST(l1, KaxTimecodeScale);
        if (ktc_scale != NULL) {
          mkv_d->tc_scale = uint64(*ktc_scale);
          mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + timecode scale: %llu\n",
                 mkv_d->tc_scale);
        } else
          mkv_d->tc_scale = MKVD_TIMECODESCALE;

        kduration = FINDFIRST(l1, KaxDuration);
        if (kduration != NULL) {
          mkv_d->duration = float(*kduration) * mkv_d->tc_scale / 1000000000.0;
          mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + duration: %.3fs\n",
                 mkv_d->duration);
        }

        l1->SkipData(*es, l1->Generic().Context);

      } else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) {
        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.

        KaxTrackEntry *ktentry;

        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ segment tracks...\n");

        l1->Read(*es, KaxTracks::ClassInfos.Context, upper_lvl_el, l2, true);

        ktentry = FINDFIRST(l1, KaxTrackEntry);
        while (ktentry != NULL) {
          // We actually found a track entry :) We're happy now.

          KaxTrackNumber *ktnum;
          KaxTrackDefaultDuration *kdefdur;
          KaxTrackType *kttype;
          KaxTrackAudio *ktaudio;
          KaxTrackVideo *ktvideo;
          KaxCodecID *kcodecid;
          KaxCodecPrivate *kcodecpriv;
          KaxTrackFlagDefault *ktfdefault;
          KaxTrackLanguage *ktlanguage;

          mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + a track...\n");
            
          track = new_mkv_track(mkv_d);
          if (track == NULL)
            return 0;

          ktnum = FINDFIRST(ktentry, KaxTrackNumber);
          if (ktnum != NULL) {
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Track number: %u\n",
                   uint32(*ktnum));
            track->tnum = uint32(*ktnum);
            if (find_duplicate_track_by_num(mkv_d, track->tnum, track) != NULL)
              mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] |  + WARNING: There's "
                     "more than one track with the number %u.\n",
                     track->tnum);
          }

          kdefdur = FINDFIRST(ktentry, KaxTrackDefaultDuration);
          if (kdefdur != NULL) {
            if (uint64(*kdefdur) == 0)
              mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Default duration: 0");
            else {
              track->v_frate = 1000000000.0 / (float)uint64(*kdefdur);
              mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Default duration: "
                     "%.3fms ( = %.3f fps)\n",
                     (float)uint64(*kdefdur) / 1000000.0, track->v_frate);
            }
          }

          kttype = FINDFIRST(ktentry, KaxTrackType);
          if (kttype != NULL) {
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Track type: ");

            switch (uint8(*kttype)) {
              case track_audio:
                mp_msg(MSGT_DEMUX, MSGL_V, "Audio\n");
                track->type = 'a';
                track->xid = aid;
                aid++;
                break;
              case track_video:
                mp_msg(MSGT_DEMUX, MSGL_V, "Video\n");
                track->type = 'v';
                track->xid = vid;
                vid++;
                break;
              case track_subtitle:
                mp_msg(MSGT_DEMUX, MSGL_V, "Subtitle\n");
                track->type = 's';
                track->xid = sid;
                sid++;
                break;
              default:
                mp_msg(MSGT_DEMUX, MSGL_V, "unknown\n");
                track->type = '?';
                break;
            }
          }

          ktaudio = FINDFIRST(ktentry, KaxTrackAudio);
          if (ktaudio != NULL) {
            KaxAudioSamplingFreq *ka_sfreq;
            KaxAudioChannels *ka_channels;
            KaxAudioBitDepth *ka_bitdepth;

            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Audio track\n");

            ka_sfreq = FINDFIRST(ktaudio, KaxAudioSamplingFreq);
            if (ka_sfreq != NULL) {
              track->a_sfreq = float(*ka_sfreq);
              mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Sampling "
                     "frequency: %f\n", track->a_sfreq);
            } else
              track->a_sfreq = 8000.0;

            ka_channels = FINDFIRST(ktaudio, KaxAudioChannels);
            if (ka_channels != NULL) {
              track->a_channels = uint8(*ka_channels);
              mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Channels: %u\n",
                     track->a_channels);
            } else
              track->a_channels = 1;

            ka_bitdepth = FINDFIRST(ktaudio, KaxAudioBitDepth);
            if (ka_bitdepth != NULL) {
              track->a_bps = uint8(*ka_bitdepth);
              mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Bit depth: %u\n",
                     track->a_bps);
            }

          }

          ktvideo = FINDFIRST(ktentry, KaxTrackVideo);
          if (ktvideo != NULL) {
            KaxVideoPixelWidth *kv_pwidth;
            KaxVideoPixelHeight *kv_pheight;
            KaxVideoDisplayWidth *kv_dwidth;
            KaxVideoDisplayHeight *kv_dheight;
            KaxVideoFrameRate *kv_frate;

            kv_pwidth = FINDFIRST(ktvideo, KaxVideoPixelWidth);
            if (kv_pwidth != NULL) {
              track->v_width = uint16(*kv_pwidth);
              mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Pixel width: %u\n",
                     track->v_width);
            }

            kv_pheight = FINDFIRST(ktvideo, KaxVideoPixelHeight);
            if (kv_pheight != NULL) {
              track->v_height = uint16(*kv_pheight);
              mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Pixel height: %u\n",
                     track->v_height);
            }

            kv_dwidth = FINDFIRST(ktvideo, KaxVideoDisplayWidth);
            if (kv_dwidth != NULL) {
              track->v_dwidth = uint16(*kv_dwidth);
              mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Display width: %u\n",
                     track->v_dwidth);
            }

            kv_dheight = FINDFIRST(ktvideo, KaxVideoDisplayHeight);
            if (kv_dheight != NULL) {
              track->v_dheight = uint16(*kv_dheight);
              mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Display height: %u\n",
                     track->v_dheight);
            }

            // For older files.
            kv_frate = FINDFIRST(ktvideo, KaxVideoFrameRate);
            if (kv_frate != NULL) {
              track->v_frate = float(*kv_frate);
              mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Frame rate: %f\n",
                     track->v_frate);
            }

          }

          kcodecid = FINDFIRST(ktentry, KaxCodecID);
          if (kcodecid != NULL) {
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Codec ID: %s\n",
                   string(*kcodecid).c_str());
            track->codec_id = strdup(string(*kcodecid).c_str());
          }

          kcodecpriv = FINDFIRST(ktentry, KaxCodecPrivate);
          if (kcodecpriv != NULL) {
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + CodecPrivate, length "
                   "%llu\n", kcodecpriv->GetSize());
            track->private_size = kcodecpriv->GetSize();
            if (track->private_size > 0) {
              track->private_data = malloc(track->private_size);
              if (track->private_data == NULL)
                return 0;
              memcpy(track->private_data, kcodecpriv->GetBuffer(),
                     track->private_size);
            }
          }

          ktfdefault = FINDFIRST(ktentry, KaxTrackFlagDefault);
          if (ktfdefault != NULL) {
            track->default_track = uint32(*ktfdefault);
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Default flag: %u\n",
                   track->default_track);
          }

          ktlanguage = FINDFIRST(ktentry, KaxTrackLanguage);
          if (ktlanguage != NULL) {
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Language: %s\n",
                   string(*ktlanguage).c_str());
            if (track->language != NULL)
              free(track->language);
            track->language = strdup(string(*ktlanguage).c_str());
          }

          ktentry = FINDNEXT(l1, KaxTrackEntry, ktentry);
        } // while (ktentry != NULL)

        l1->SkipData(*es, l1->Generic().Context);

      } else if (EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId) {
        if (!find_in_vector(seekheads_to_parse, l1->GetElementPosition()))
          seekheads_to_parse.push_back(l1->GetElementPosition());
        l1->SkipData(*es, l1->Generic().Context);

      } else if ((EbmlId(*l1) == KaxCues::ClassInfos.GlobalId) &&
                 !mkv_d->cues_found) {
        if (!find_in_vector(cues_to_parse, l1->GetElementPosition()))
          cues_to_parse.push_back(l1->GetElementPosition());
        l1->SkipData(*es, l1->Generic().Context);

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ found cluster, headers are "
               "parsed completely :)\n");
        add_cluster_position(mkv_d, l1->GetElementPosition());
        mkv_d->saved_l1 = l1;
        exit_loop = 1;

      } else
        l1->SkipData(*es, l1->Generic().Context);
      
      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (upper_lvl_el > 0) {
        upper_lvl_el--;
        if (upper_lvl_el > 0)
          break;
        delete l1;
        l1 = l2;
        continue;

      } else if (upper_lvl_el < 0) {
        upper_lvl_el++;
        if (upper_lvl_el < 0)
          break;

      }

      if (exit_loop)      // we've found the first cluster, so get out
        break;

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true);

    } // while (l1 != NULL)

    if (!exit_loop) {
      free_mkv_demuxer(mkv_d);
      return 0;
    }

    current_pos = io.getFilePointer();

    // Try to find the very first timecode (cluster timecode).
    l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                             0xFFFFFFFFL, true, 1);
    if ((l2 != NULL) && !upper_lvl_el &&
        (EbmlId(*l2) == KaxClusterTimecode::ClassInfos.GlobalId)) {
      KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
      ctc.ReadData(es->I_O());
      mkv_d->first_tc = uint64(ctc) * mkv_d->tc_scale / 1000000;
      delete l2;
    } else
      mkv_d->first_tc = 0;

    // Parse all cues and seek heads
    for (i = 0; i < (int)cues_to_parse.size(); i++)
      parse_cues(mkv_d, cues_to_parse[i]);
    for (i = 0; i < (int)seekheads_to_parse.size(); i++)
      parse_seekhead(mkv_d, seekheads_to_parse[i]);

    io.setFilePointer(current_pos);

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
    track = find_track_by_num(mkv_d, demuxer->video->id, 'v');

  if (track) {
    BITMAPINFOHEADER *bih;

    idesc = NULL;
    bih = (BITMAPINFOHEADER *)calloc(1, track->private_size);
    if (bih == NULL) {
      free_mkv_demuxer(mkv_d);
      return 0;
    }

    if (track->ms_compat) {         // MS compatibility mode
      BITMAPINFOHEADER *src;
      src = (BITMAPINFOHEADER *)track->private_data;
      bih->biSize = get_uint32(&src->biSize);
      bih->biWidth = get_uint32(&src->biWidth);
      bih->biHeight = get_uint32(&src->biHeight);
      bih->biPlanes = get_uint16(&src->biPlanes);
      bih->biBitCount = get_uint16(&src->biBitCount);
      bih->biCompression = get_uint32(&src->biCompression);
      bih->biSizeImage = get_uint32(&src->biSizeImage);
      bih->biXPelsPerMeter = get_uint32(&src->biXPelsPerMeter);
      bih->biYPelsPerMeter = get_uint32(&src->biYPelsPerMeter);
      bih->biClrUsed = get_uint32(&src->biClrUsed);
      bih->biClrImportant = get_uint32(&src->biClrImportant);
      memcpy((char *)bih + sizeof(BITMAPINFOHEADER),
             (char *)src + sizeof(BITMAPINFOHEADER),
             track->private_size - sizeof(BITMAPINFOHEADER));

    } else {
      bih->biSize = sizeof(BITMAPINFOHEADER);
      bih->biWidth = track->v_width;
      bih->biHeight = track->v_height;
      bih->biBitCount = 24;
      bih->biSizeImage = bih->biWidth * bih->biHeight * bih->biBitCount / 8;

      if ((track->private_size >= sizeof(real_video_props_t)) &&
          (!strcmp(track->codec_id, MKV_V_REALV10) ||
           !strcmp(track->codec_id, MKV_V_REALV20) ||
           !strcmp(track->codec_id, MKV_V_REALV30) ||
           !strcmp(track->codec_id, MKV_V_REALV40))) {
        unsigned char *dst, *src;
        real_video_props_t *rvp;
        uint32_t type2;

        rvp = (real_video_props_t *)track->private_data;
        src = (unsigned char *)(rvp + 1);

        bih = (BITMAPINFOHEADER *)realloc(bih, sizeof(BITMAPINFOHEADER) + 12);
        bih->biSize = 48;
        bih->biPlanes = 1;
        type2 = get_uint32_be(&rvp->type2);
        if ((type2 == 0x10003000) || (type2 == 0x10003001))
          bih->biCompression = mmioFOURCC('R', 'V', '1', '3');
        else
          bih->biCompression = mmioFOURCC('R', 'V', track->codec_id[9], '0');
        dst = (unsigned char *)(bih + 1);
        ((unsigned int *)dst)[0] = get_uint32_be(&rvp->type1);
        ((unsigned int *)dst)[1] = type2;

		    if ((bih->biCompression <= 0x30335652) &&
            (type2 >= 0x20200002)) {
          // read secondary WxH for the cmsg24[] (see vd_realvid.c)
          ((unsigned short *)(bih + 1))[4] = 4 * (unsigned short)src[0];
          ((unsigned short *)(bih + 1))[5] = 4 * (unsigned short)src[1];
        } else
          memset(&dst[8], 0, 4);
        track->realmedia = true;

#if defined(USE_QTX_CODECS)
      } else if ((track->private_size >= sizeof(qt_image_description_t)) &&
                 (!strcmp(track->codec_id, MKV_V_QUICKTIME))) {
        idesc = (qt_image_description_t *)track->private_data;
        idesc->id_size = get_uint32_be(&idesc->id_size);
        idesc->codec_type = get_uint32(&idesc->codec_type);
        idesc->version = get_uint16_be(&idesc->version);
        idesc->revision = get_uint16_be(&idesc->revision);
        idesc->vendor = get_uint32_be(&idesc->vendor);
        idesc->temporal_quality = get_uint32_be(&idesc->temporal_quality);
        idesc->spatial_quality = get_uint32_be(&idesc->spatial_quality);
        idesc->width = get_uint16_be(&idesc->width);
        idesc->height = get_uint16_be(&idesc->height);
        idesc->horizontal_resolution =
          get_uint32_be(&idesc->horizontal_resolution);
        idesc->vertical_resolution =
          get_uint32_be(&idesc->vertical_resolution);
        idesc->data_size = get_uint32_be(&idesc->data_size);
        idesc->frame_count = get_uint16_be(&idesc->frame_count);
        idesc->depth = get_uint16_be(&idesc->depth);
        idesc->color_table_id = get_uint16_be(&idesc->color_table_id);
        bih->biPlanes = 1;
        bih->biCompression = idesc->codec_type;
#endif // defined(USE_QTX_CODECS)

      } else {
        mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Unknown/unsupported CodecID "
               "(%s) or missing/bad CodecPrivate data (track %u).\n",
               track->codec_id, track->tnum);
        demuxer->video->id = -2;
      }
    }

    if (demuxer->video->id != -2) {
      mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] Will play video track %u\n",
             track->tnum);

      sh_v = new_sh_video(demuxer, track->tnum);
      sh_v->bih = bih;
      sh_v->format = sh_v->bih->biCompression;
      if (track->v_frate == 0.0)
        track->v_frate = 25.0;
      sh_v->fps = track->v_frate;
      sh_v->frametime = 1 / track->v_frate;
      sh_v->disp_w = track->v_width;
      sh_v->disp_h = track->v_height;
      sh_v->aspect = (float)track->v_dwidth / (float)track->v_dheight;
      if (idesc != NULL)
        sh_v->ImageDesc = idesc;
      mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Aspect: %f\n", sh_v->aspect);

      demuxer->video->id = track->tnum;
      demuxer->video->sh = sh_v;
      sh_v->ds = demuxer->video;

      mkv_d->video = track;
    } else
      free(bih);

  } else {
    mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] No video track found/wanted.\n");
    demuxer->video->id = -2;
  }

  track = NULL;
  if (demuxer->audio->id == -1) { // Automatically select an audio track.
    // check if the user specified an audio language
    if (audio_lang != NULL) {
      track = find_track_by_language(mkv_d, audio_lang, NULL, 'a');
    }
    if (track == NULL)
      // no audio language specified, or language not found
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
    track = find_track_by_num(mkv_d, demuxer->audio->id, 'a');

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
    sh_a->format = track->a_formattag;
    sh_a->wf->wFormatTag = track->a_formattag;
    sh_a->channels = track->a_channels;
    sh_a->wf->nChannels = track->a_channels;
    sh_a->samplerate = (uint32_t)track->a_sfreq;
    sh_a->wf->nSamplesPerSec = (uint32_t)track->a_sfreq;
    sh_a->samplesize = track->a_bps / 8;
    if (!strcmp(track->codec_id, MKV_A_MP3)) {
      sh_a->wf->nAvgBytesPerSec = 16000;
      sh_a->wf->nBlockAlign = 1152;
      sh_a->wf->wBitsPerSample = 0;
      sh_a->samplesize = 0;

    } else if (!strncmp(track->codec_id, MKV_A_AC3, strlen(MKV_A_AC3))) {
      sh_a->wf->nAvgBytesPerSec = 16000;
      sh_a->wf->nBlockAlign = 1536;
      sh_a->wf->wBitsPerSample = 0;
      sh_a->samplesize = 0;

    } else if (!strcmp(track->codec_id, MKV_A_PCM) ||
               !strcmp(track->codec_id, MKV_A_PCM_BE)) {
      sh_a->wf->nAvgBytesPerSec = sh_a->channels * sh_a->samplerate * 2;
      sh_a->wf->nBlockAlign = sh_a->wf->nAvgBytesPerSec;
      sh_a->wf->wBitsPerSample = track->a_bps;
      if (!strcmp(track->codec_id, MKV_A_PCM_BE))
        sh_a->format = mmioFOURCC('t', 'w', 'o', 's');

    } else if (!strcmp(track->codec_id, MKV_A_QDMC) ||
               !strcmp(track->codec_id, MKV_A_QDMC2)) {
      sh_a->wf->wBitsPerSample = track->a_bps;
      sh_a->wf->nAvgBytesPerSec = 16000;
      sh_a->wf->nBlockAlign = 1486;
      track->fix_i_bps = true;
      track->qt_last_a_pts = 0.0;
      if (track->private_data != NULL) {
        sh_a->codecdata = (unsigned char *)calloc(track->private_size, 1);
        memcpy(sh_a->codecdata, track->private_data, track->private_size);
        sh_a->codecdata_len = track->private_size;
      }
      if (!strcmp(track->codec_id, MKV_A_QDMC))
        sh_a->format = mmioFOURCC('Q', 'D', 'M', 'C');
      else
        sh_a->format = mmioFOURCC('Q', 'D', 'M', '2');

    } else if (track->a_formattag == mmioFOURCC('M', 'P', '4', 'A')) {
      int profile, srate_idx;

      sh_a->wf->nAvgBytesPerSec = 16000;
      sh_a->wf->nBlockAlign = 1024;
      sh_a->wf->wBitsPerSample = 0;
      sh_a->samplesize = 0;

      // Recreate the 'private data' which faad2 uses in its initialization.
      // A_AAC/MPEG2/MAIN
      // 0123456789012345
      if (!strcmp(&track->codec_id[12], "MAIN"))
        profile = 0;
      else if (!strcmp(&track->codec_id[12], "LC"))
        profile = 1;
      else if (!strcmp(&track->codec_id[12], "SSR"))
        profile = 2;
      else
        profile = 3;
      if (92017 <= sh_a->samplerate)
        srate_idx = 0;
      else if (75132 <= sh_a->samplerate)
        srate_idx = 1;
      else if (55426 <= sh_a->samplerate)
        srate_idx = 2;
      else if (46009 <= sh_a->samplerate)
        srate_idx = 3;
      else if (37566 <= sh_a->samplerate)
        srate_idx = 4;
      else if (27713 <= sh_a->samplerate)
        srate_idx = 5;
      else if (23004 <= sh_a->samplerate)
        srate_idx = 6;
      else if (18783 <= sh_a->samplerate)
        srate_idx = 7;
      else if (13856 <= sh_a->samplerate)
        srate_idx = 8;
      else if (11502 <= sh_a->samplerate)
        srate_idx = 9;
      else if (9391 <= sh_a->samplerate)
        srate_idx = 10;
      else
        srate_idx = 11;

      sh_a->codecdata = (unsigned char *)calloc(1, 2);
      sh_a->codecdata_len = 2;
      sh_a->codecdata[0] = ((profile + 1) << 3) | ((srate_idx & 0xe) >> 1);
      sh_a->codecdata[1] = ((srate_idx & 0x1) << 7) | (track->a_channels << 3);

    } else if (!strcmp(track->codec_id, MKV_A_VORBIS)) {
      for (i = 0; i < 3; i++) {
        dp = new_demux_packet(track->header_sizes[i]);
        memcpy(dp->buffer, track->headers[i], track->header_sizes[i]);
        dp->pts = 0;
        dp->flags = 0;
        ds_add_packet(demuxer->audio, dp);
      }

    } else if ((track->private_size >= sizeof(real_audio_v4_props_t)) &&
               !strncmp(track->codec_id, MKV_A_REALATRC, 7)) {
      // Common initialization for all RealAudio codecs
      real_audio_v4_props_t *ra4p;
      real_audio_v5_props_t *ra5p;
      unsigned char *src;
      int codecdata_length, version;

      ra4p = (real_audio_v4_props_t *)track->private_data;
      ra5p = (real_audio_v5_props_t *)track->private_data;

      sh_a->wf->wBitsPerSample = sh_a->samplesize * 8;
      sh_a->wf->nAvgBytesPerSec = 0; // FIXME !?
      sh_a->wf->nBlockAlign = get_uint16_be(&ra4p->frame_size);

      version = get_uint16_be(&ra4p->version1);

      if (version == 4) {
        src = (unsigned char *)(ra4p + 1);
        src += src[0] + 1;
        src += src[0] + 1;
      } else
        src = (unsigned char *)(ra5p + 1);

      src += 3;
      if (version == 5)
        src++;
      codecdata_length = get_uint32_be(src);
      src += 4;
      sh_a->wf->cbSize = 10 + codecdata_length;
      sh_a->wf = (WAVEFORMATEX *)realloc(sh_a->wf, sizeof(WAVEFORMATEX) +
                                         sh_a->wf->cbSize);
      ((short *)(sh_a->wf + 1))[0] = get_uint16_be(&ra4p->sub_packet_size);
      ((short *)(sh_a->wf + 1))[1] = get_uint16_be(&ra4p->sub_packet_h);
      ((short *)(sh_a->wf + 1))[2] = get_uint16_be(&ra4p->flavor);
      ((short *)(sh_a->wf + 1))[3] = get_uint32_be(&ra4p->coded_frame_size);
      ((short *)(sh_a->wf + 1))[4] = codecdata_length;
      memcpy(((char *)(sh_a->wf + 1)) + 10, src, codecdata_length);

      track->realmedia = true;
    }

  } else {
    mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] No audio track found/wanted.\n");
    demuxer->audio->id = -2;
  }

  // DO NOT automatically select a subtitle track and behave like DVD
  // playback: only show subtitles if the user explicitely wants them.
  track = NULL;
  if (demuxer->sub->id >= 0)
    track = find_track_by_num(mkv_d, demuxer->sub->id, 's');
  else if (dvdsub_lang != NULL)
    track = find_track_by_language(mkv_d, dvdsub_lang, NULL);
  if (track) {
    if (!strncmp(track->codec_id, MKV_S_VOBSUB, strlen(MKV_S_VOBSUB))) {
      mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] Will display subtitle track %u\n",
             track->tnum);
      mkv_d->subs_track = track;
      mkv_d->subtitle_type = MKV_SUBTYPE_VOBSUB;
      demuxer->sub->sh = (mkv_sh_sub_t *)malloc(sizeof(mkv_sh_sub_t));
      demuxer->sub->id = track->xid;
      memcpy(demuxer->sub->sh, &track->sh_sub, sizeof(mkv_sh_sub_t));

    } else if (strcmp(track->codec_id, MKV_S_TEXTASCII) &&
               strcmp(track->codec_id, MKV_S_TEXTUTF8) && 
               strcmp(track->codec_id, MKV_S_TEXTSSA) &&
               strcmp(track->codec_id, "S_SSA"))
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
        if (!strcmp(track->codec_id, MKV_S_TEXTSSA) ||
            !strcmp(track->codec_id, "S_SSA")) {
          mkv_d->subtitle_type = MKV_SUBTYPE_SSA;
          sub_utf8 = 1;
        } else
          mkv_d->subtitle_type = MKV_SUBTYPE_TEXT;
      } else
        mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] File does not contain a "
               "subtitle track with the id %u.\n", demuxer->sub->id);
      demuxer->sub->sh = NULL;
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

// Taken from demux_real.c. Thanks to the original developpers :)
#define SKIP_BITS(n) buffer <<= n
#define SHOW_BITS(n) ((buffer) >> (32 - (n)))

static float real_fix_timestamp(mkv_track_t *track, unsigned char *s,
                                int timestamp) {
  float v_pts;
  uint32_t buffer = (s[0] << 24) + (s[1] << 16) + (s[2] << 8) + s[3];
  int kf = timestamp;
  int pict_type;
  int orig_kf;

  if (!strcmp(track->codec_id, MKV_V_REALV30) ||
      !strcmp(track->codec_id, MKV_V_REALV40)) {

    if (!strcmp(track->codec_id, MKV_V_REALV30)) {
      SKIP_BITS(3);
      pict_type = SHOW_BITS(2);
      SKIP_BITS(2 + 7);
    }else{
      SKIP_BITS(1);
      pict_type = SHOW_BITS(2);
      SKIP_BITS(2 + 7 + 3);
    }
    kf = SHOW_BITS(13);         // kf= 2*SHOW_BITS(12);
    orig_kf = kf;
    if (pict_type <= 1) {
      // I frame, sync timestamps:
      track->rv_kf_base = timestamp - kf;
      mp_msg(MSGT_DEMUX, MSGL_V, "\nTS: base=%08X\n", track->rv_kf_base);
      kf = timestamp;
    } else {
      // P/B frame, merge timestamps:
      int tmp = timestamp - track->rv_kf_base;
      kf |= tmp & (~0x1fff);    // combine with packet timestamp
      if (kf < (tmp - 4096))    // workaround wrap-around problems
        kf += 8192;
      else if (kf > (tmp + 4096))
        kf -= 8192;
      kf += track->rv_kf_base;
    }
    if (pict_type != 3) {       // P || I  frame -> swap timestamps
      int tmp = kf;
      kf = track->rv_kf_pts;
      track->rv_kf_pts = tmp;
    }
    mp_msg(MSGT_DEMUX, MSGL_V, "\nTS: %08X -> %08X (%04X) %d %02X %02X %02X "
           "%02X %5d\n", timestamp, kf, orig_kf, pict_type, s[0], s[1], s[2],
           s[3], kf - (int)(1000.0 * track->rv_pts));
  }
  v_pts = kf * 0.001f;
  track->rv_pts = v_pts;

  return v_pts;
}

static void handle_realvideo(demuxer_t *demuxer, DataBuffer &data,
                             bool keyframe, int &found_data) {
  unsigned char *p;
  dp_hdr_t *hdr;
  int chunks, isize;
  mkv_demuxer_t *mkv_d;
  demux_stream_t *ds;
  demux_packet_t *dp;

  mkv_d = (mkv_demuxer_t *)demuxer->priv;
  ds = demuxer->video;
  p = (unsigned char *)data.Buffer();
  chunks = p[0];
  isize = data.Size() - 1 - (chunks + 1) * 8;
  dp = new_demux_packet(sizeof(dp_hdr_t) + isize + 8 * (chunks + 1));
  memcpy(&dp->buffer[sizeof(dp_hdr_t)], &p[1 + (chunks + 1) * 8], isize);
  memcpy(&dp->buffer[sizeof(dp_hdr_t) + isize], &p[1], (chunks + 1) * 8);
  hdr = (dp_hdr_t *)dp->buffer;
  hdr->len = isize;
  hdr->chunks = chunks;
  hdr->timestamp = (int)(mkv_d->last_pts * 1000);
  hdr->chunktab = sizeof(dp_hdr_t) + isize;

  dp->len = sizeof(dp_hdr_t) + isize + 8 * (chunks + 1);
  if (mkv_d->v_skip_to_keyframe) {
    dp->pts = mkv_d->last_pts;
    mkv_d->video->rv_kf_base = 0;
    mkv_d->video->rv_kf_pts = hdr->timestamp;
  } else
    dp->pts = real_fix_timestamp(mkv_d->video, &dp->buffer[sizeof(dp_hdr_t)],
                                 hdr->timestamp);
  dp->pos = demuxer->filepos;
  dp->flags = keyframe ? 0x10 : 0;

  ds_add_packet(ds, dp);

  found_data++;
}

static void handle_realaudio(demuxer_t *demuxer, DataBuffer &data,
                             bool keyframe, int &found_data) {
  mkv_demuxer_t *mkv_d;
  demux_packet_t *dp;

  mkv_d = (mkv_demuxer_t *)demuxer->priv;

  dp = new_demux_packet(data.Size());
  memcpy(dp->buffer, data.Buffer(), data.Size());
  if ((mkv_d->audio->ra_pts == mkv_d->last_pts) &&
      !mkv_d->a_skip_to_keyframe)
    dp->pts = 0;
  else
    dp->pts = mkv_d->last_pts;
  mkv_d->audio->ra_pts = mkv_d->last_pts;

  dp->pos = demuxer->filepos;
  dp->flags = keyframe ? 0x10 : 0;

  ds_add_packet(demuxer->audio, dp);

  found_data++;
}

#define mpeg_read(buf, num) _mpeg_read(srcbuf, size, srcpos, buf, num)
static uint32_t _mpeg_read(unsigned char *srcbuf, uint32_t size,
                           uint32_t &srcpos, unsigned char *dstbuf,
                           uint32_t num) {
  uint32_t real_num;

  if ((srcpos + num) >= size)
    real_num = size - srcpos;
  else
    real_num = num;
  memcpy(dstbuf, &srcbuf[srcpos], real_num);
  srcpos += real_num;

  return real_num;
}

#define mpeg_getc() _mpeg_getch(srcbuf, size, srcpos)
static int _mpeg_getch(unsigned char *srcbuf, uint32_t size,
                       uint32_t &srcpos) {
  unsigned char c;

  if (mpeg_read(&c, 1) != 1)
    return -1;
  return (int)c;
}

#define mpeg_seek(b, w) _mpeg_seek(size, srcpos, b, w)
static int _mpeg_seek(uint32_t size, uint32_t &srcpos, uint32_t num,
                      int whence) {
  uint32_t new_pos;

  if (whence == SEEK_SET)
    new_pos = num;
  else if (whence == SEEK_CUR)
    new_pos = srcpos + num;
  else
    abort();

  if (new_pos >= size) {
    srcpos = size;
    return 1;
  }

  srcpos = new_pos;
  return 0;
}

#define mpeg_tell() srcpos

// Adopted from vobsub.c
static int mpeg_run(demuxer_t *demuxer, unsigned char *srcbuf, uint32_t size) {
  uint32_t len, idx, version, srcpos, packet_size;
  int c, aid;
  float pts;
  /* Goto start of a packet, it starts with 0x000001?? */
  const unsigned char wanted[] = { 0, 0, 1 };
  unsigned char buf[5];
  demux_packet_t *dp;
  demux_stream_t *ds;
  mkv_demuxer_t *mkv_d;

  mkv_d = (mkv_demuxer_t *)demuxer->priv;
  ds = demuxer->sub;

  srcpos = 0;
  packet_size = 0;
  while (1) {
    if (mpeg_read(buf, 4) != 4)
      return -1;
    while (memcmp(buf, wanted, sizeof(wanted)) != 0) {
      c = mpeg_getc();
      if (c < 0)
        return -1;
      memmove(buf, buf + 1, 3);
      buf[3] = c;
    }
    switch (buf[3]) {
      case 0xb9:			/* System End Code */
        return 0;
        break;

      case 0xba:			/* Packet start code */
        c = mpeg_getc();
        if (c < 0)
          return -1;
        if ((c & 0xc0) == 0x40)
          version = 4;
        else if ((c & 0xf0) == 0x20)
          version = 2;
        else {
          mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub: Unsupported MPEG "
                 "version: 0x%02x\n", c);
          return -1;
        }

        if (version == 4) {
          if (mpeg_seek(9, SEEK_CUR))
            return -1;
        } else if (version == 2) {
          if (mpeg_seek(7, SEEK_CUR))
            return -1;
        } else
          abort();
        break;

      case 0xbd:			/* packet */
        if (mpeg_read(buf, 2) != 2)
          return -1;
        len = buf[0] << 8 | buf[1];
        idx = mpeg_tell();
        c = mpeg_getc();
        if (c < 0)
          return -1;
        if ((c & 0xC0) == 0x40) { /* skip STD scale & size */
          if (mpeg_getc() < 0)
            return -1;
          c = mpeg_getc();
          if (c < 0)
            return -1;
        }
        if ((c & 0xf0) == 0x20) { /* System-1 stream timestamp */
          /* Do we need this? */
          abort();
        } else if ((c & 0xf0) == 0x30) {
          /* Do we need this? */
          abort();
        } else if ((c & 0xc0) == 0x80) { /* System-2 (.VOB) stream */
          uint32_t pts_flags, hdrlen, dataidx;
          c = mpeg_getc();
          if (c < 0)
            return -1;
          pts_flags = c;
          c = mpeg_getc();
          if (c < 0)
            return -1;
          hdrlen = c;
          dataidx = mpeg_tell() + hdrlen;
          if (dataidx > idx + len) {
            mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub: Invalid header "
                   "length: %d (total length: %d, idx: %d, dataidx: %d)\n",
                   hdrlen, len, idx, dataidx);
            return -1;
          }
          if ((pts_flags & 0xc0) == 0x80) {
            if (mpeg_read(buf, 5) != 5)
              return -1;
            if (!(((buf[0] & 0xf0) == 0x20) && (buf[0] & 1) && (buf[2] & 1) &&
                  (buf[4] & 1))) {
              mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub PTS error: 0x%02x "
                     "%02x%02x %02x%02x \n",
                     buf[0], buf[1], buf[2], buf[3], buf[4]);
              pts = 0;
            } else
              pts = ((buf[0] & 0x0e) << 29 | buf[1] << 22 |
                     (buf[2] & 0xfe) << 14 | buf[3] << 7 | (buf[4] >> 1));
          } else /* if ((pts_flags & 0xc0) == 0xc0) */ {
            /* what's this? */
            /* abort(); */
          }
          mpeg_seek(dataidx, SEEK_SET);
          aid = mpeg_getc();
          if (aid < 0) {
            mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub: Bogus aid %d\n", aid);
            return -1;
          }
          packet_size = len - ((unsigned int)mpeg_tell() - idx);
          
          dp = new_demux_packet(packet_size);
          dp->flags = 1;
          dp->pts = mkv_d->last_pts;
          if (mpeg_read(dp->buffer, packet_size) != packet_size) {
            mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub: mpeg_read failure");
            packet_size = 0;
            return -1;
          }
          ds_add_packet(ds, dp);
          idx = len;
        }
        break;

      case 0xbe:			/* Padding */
        if (mpeg_read(buf, 2) != 2)
          return -1;
        len = buf[0] << 8 | buf[1];
        if ((len > 0) && mpeg_seek(len, SEEK_CUR))
          return -1;
        break;

      default:
        if ((0xc0 <= buf[3]) && (buf[3] < 0xf0)) {
          /* MPEG audio or video */
          if (mpeg_read(buf, 2) != 2)
            return -1;
          len = (buf[0] << 8) | buf[1];
          if ((len > 0) && mpeg_seek(len, SEEK_CUR))
            return -1;

        }
        else {
          mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub: unknown header "
                 "0x%02X%02X%02X%02X\n", buf[0], buf[1], buf[2], buf[3]);
          return -1;
        }
    }
  }
  return 0;
}

extern "C" int demux_mkv_fill_buffer(demuxer_t *d) {
  demux_packet_t *dp;
  demux_stream_t *ds;
  mkv_demuxer_t *mkv_d;
  int upper_lvl_el, exit_loop, found_data, i, linei, sl;
  char *texttmp;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL;
  EbmlStream *es;
  KaxBlock *block;
  int64_t block_duration, block_bref, block_fref;
  bool use_this_block, lines_cut;
  float current_pts;

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
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {

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
        while ((l2 != NULL) && (upper_lvl_el <= 0)) {

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
#if LIBEBML_VERSION >= 000404
            mkv_d->cluster->InitTimecode(mkv_d->cluster_tc, mkv_d->tc_scale);
#else
            mkv_d->cluster->InitTimecode(mkv_d->cluster_tc);
#endif // LIBEBML_VERSION

          } else if (EbmlId(*l2) == KaxBlockGroup::ClassInfos.GlobalId) {

            KaxBlockDuration *kbdur;
            KaxReferenceBlock *krefblock;
            KaxBlock *kblock;

            block = NULL;
            block_duration = -1;
            block_bref = 0;
            block_fref = 0;

            l2->Read(*es, KaxBlockGroup::ClassInfos.Context, upper_lvl_el, l3,
                     true);

            kbdur = FINDFIRST(l2, KaxBlockDuration);
            if (kbdur != NULL)
              block_duration = uint64(*kbdur);

            kblock = FINDFIRST(l2, KaxBlock);
            if (kblock != NULL)
              kblock->SetParent(*mkv_d->cluster);

            krefblock = FINDFIRST(l2, KaxReferenceBlock);
            while (krefblock != NULL) {
              if (int64(*krefblock) < 0)
                block_bref = int64(*krefblock);
              else
                block_fref = int64(*krefblock);

              krefblock = FINDNEXT(l2, KaxReferenceBlock, krefblock);
            }

            if (kblock != NULL) {
              // Clear the subtitles if they're obsolete now.
              lines_cut = false;
              for (linei = 0; linei < mkv_d->subs.lines; linei++) {
                if (mkv_d->clear_subs_at[linei] <=
                    (kblock->GlobalTimecode() / 1000000 - mkv_d->first_tc)) {
                  sl = linei; 
                  texttmp = mkv_d->subs.text[sl];
                  while (sl < mkv_d->subs.lines) {
                    mkv_d->subs.text[sl] = mkv_d->subs.text[sl + 1];
                    mkv_d->clear_subs_at[sl] = mkv_d->clear_subs_at[sl + 1];
                    sl++;
                  }
                  mkv_d->subs.text[sl] = texttmp;
                  mkv_d->subs.lines--;
                  linei--;
                  lines_cut = true;
                }
                if (lines_cut) {
                  vo_sub = &mkv_d->subs;
                  vo_osd_changed(OSDTYPE_SUBTITLE);
                }
              }

              ds = NULL;
              if ((mkv_d->video != NULL) &&
                  (mkv_d->video->tnum == kblock->TrackNum()))
                ds = d->video;
              else if ((mkv_d->audio != NULL) && 
                         (mkv_d->audio->tnum == kblock->TrackNum()))
                ds = d->audio;
              else if ((mkv_d->subs_track != NULL) && 
                       (mkv_d->subs_track->tnum == kblock->TrackNum()))
                ds = d->sub;

              use_this_block = true;

              current_pts = (float)(kblock->GlobalTimecode() / 1000000.0 -
                                    mkv_d->first_tc) / 1000.0;
              if (current_pts < 0.0)
                current_pts = 0.0;

              if (ds == d->audio) {
                if (mkv_d->a_skip_to_keyframe &&       
                    (block_bref != 0))
                  use_this_block = false;
                
                else if (mkv_d->v_skip_to_keyframe)
                  use_this_block = false;

                if (mkv_d->audio->fix_i_bps && use_this_block) {
                  uint32_t i, sum;
                  sh_audio_t *sh;

                  for (i = 0, sum = 0; i < kblock->NumberFrames(); i++) {
                    DataBuffer &data = kblock->GetBuffer(i);
                    sum += data.Size();
                  }
                  sh = (sh_audio_t *)ds->sh;
                  if (block_duration != -1) {
                    sh->i_bps = sum * 1000 / block_duration;
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Changed i_bps to %d.\n",
                           sh->i_bps);
                    mkv_d->audio->fix_i_bps = false;
                  } else if (mkv_d->audio->qt_last_a_pts == 0.0)
                    mkv_d->audio->qt_last_a_pts = current_pts;
                  else if (mkv_d->audio->qt_last_a_pts != current_pts) {
                    sh->i_bps = (int)(sum / (current_pts -
                                             mkv_d->audio->qt_last_a_pts));
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Changed i_bps to %d.\n",
                           sh->i_bps);
                    mkv_d->audio->fix_i_bps = false;
                  }
                }

              } else if ((current_pts * 1000) < mkv_d->skip_to_timecode)
                use_this_block = false;

              else if (ds == d->video) {
                if (mkv_d->v_skip_to_keyframe &&
                    (block_bref != 0))
                  use_this_block = false;
                
              } else if ((mkv_d->subs_track != NULL) &&
                         (mkv_d->subs_track->tnum == kblock->TrackNum())) {
                if (mkv_d->subtitle_type != MKV_SUBTYPE_VOBSUB) {
                  if (!mkv_d->v_skip_to_keyframe)
                    handle_subtitles(d, kblock, block_duration);
                  use_this_block = false;
                }

              } else
                use_this_block = false;

              if (use_this_block) {
                mkv_d->last_pts = current_pts;
                d->filepos = mkv_d->in->getFilePointer();
                mkv_d->last_filepos = d->filepos;

                for (i = 0; i < (int)kblock->NumberFrames(); i++) {
                  DataBuffer &data = kblock->GetBuffer(i);

                  if ((ds == d->video) && mkv_d->video->realmedia)
                    handle_realvideo(d, data, block_bref == 0,
                                     found_data);

                  else if ((ds == d->audio) && mkv_d->audio->realmedia)
                    handle_realaudio(d, data, block_bref == 0,
                                     found_data);

                  else if ((ds == d->sub) &&
                           (mkv_d->subtitle_type == MKV_SUBTYPE_VOBSUB))
                    mpeg_run(d, (unsigned char *)data.Buffer(), data.Size());

                  else {
                    dp = new_demux_packet(data.Size());
                    memcpy(dp->buffer, data.Buffer(), data.Size());
                    dp->flags = block_bref == 0 ? 1 : 0;
                    dp->pts = mkv_d->last_pts;
                    ds_add_packet(ds, dp);
                    found_data++;
                  }
                }
                if (ds == d->video) {
                  mkv_d->v_skip_to_keyframe = false;
                  mkv_d->skip_to_timecode = 0;
                } else if (ds == d->audio)
                  mkv_d->a_skip_to_keyframe = false;
              }

              delete block;
            } // kblock != NULL

          } else
            l2->SkipData(*es, l2->Generic().Context);

          if (!in_parent(l1)) {
            delete l2;
            break;
          }

          if (upper_lvl_el > 0) {
            upper_lvl_el--;
            if (upper_lvl_el > 0)
              break;
            delete l2;
            l2 = l3;
            continue;

          } else if (upper_lvl_el < 0) {
            upper_lvl_el++;
            if (upper_lvl_el < 0)
              break;

          }

          l2->SkipData(*es, l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true);

        } // while (l2 != NULL)

      } else if (EbmlId(*l1) == KaxCues::ClassInfos.GlobalId)
        return 0;
      else
        l1->SkipData(*es, l1->Generic().Context);

      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (upper_lvl_el > 0) {
        upper_lvl_el--;
        if (upper_lvl_el > 0)
          break;
        delete l1;
        l1 = l2;
        continue;

      } else if (upper_lvl_el < 0) {
        upper_lvl_el++;
        if (upper_lvl_el < 0)
          break;

      }

      if (exit_loop)
        break;

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true);

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
        parse_cues(mkv_d, l1->GetElementPosition());
        delete l1;
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
          if ((target_timecode <= (mkv_d->last_pts * 1000)) &&
              (diff >= 0) && (diff < min_diff)) {
            min_diff = diff;
            entry = &index->entries[k];
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] seek BACK, solution: last_pts: "
                   "%d, target: %d, diff: %d, entry->timecode: %d, PREV diff: "
                   "%d, k: %d\n", (int)(mkv_d->last_pts * 1000),
                   (int)target_timecode, (int)diff, (int)entry->timecode,
                   k > 0 ? (int)(index->entries[k - 1].timecode -
                                 target_timecode) : 0, k);
                   
          } else if ((target_timecode > (mkv_d->last_pts * 1000)) &&
                     (diff < 0) && (-diff < min_diff)) {
            min_diff = -diff;
            entry = &index->entries[k];
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] seek FORW, solution: last_pts: "
                   "%d, target: %d, diff: %d, entry->timecode: %d, NEXT diff: "
                   "%d, k: %d\n", (int)(mkv_d->last_pts * 1000),
                   (int)target_timecode, (int)diff, (int)entry->timecode,
                   k < index->num_entries ?
                   (int)(index->entries[k + 1].timecode - target_timecode) :
                   0, k);
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
        } else if ((diff < 0 ? -1 * diff : diff) < min_diff) {
          cluster_pos = mkv_d->cluster_positions[i];
          min_diff = diff < 0 ? -1 * diff : diff;
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
             mkv_d->saved_l1->Generic().DebugName);
    }

    if (mkv_d->video != NULL)
      mkv_d->v_skip_to_keyframe = true;
    if (rel_seek_secs > 0.0)
      mkv_d->skip_to_timecode = target_timecode;

    mkv_d->a_skip_to_keyframe = true;

    demux_mkv_fill_buffer(demuxer);
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] New timecode: %lld\n",
           (int64_t)(mkv_d->last_pts * 1000.0));

    mkv_d->subs.lines = 0;
    vo_sub = &mkv_d->subs;
    vo_osd_changed(OSDTYPE_SUBTITLE);

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
