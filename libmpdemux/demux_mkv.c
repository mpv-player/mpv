/*
 * native Matroska demuxer
 * Written by Aurelien Jacobs <aurel@gnuage.org>
 * Based on the one written by Ronald Bultje for gstreamer
 *   and on demux_mkv.cpp from Moritz Bunkus.
 * Licence: GPL
 */

#include "config.h"
#ifdef HAVE_MATROSKA

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "ebml.h"
#include "matroska.h"
#include "bswap.h"

#include "../subreader.h"
#include "../libvo/sub.h"

#ifdef USE_QTX_CODECS
#include "qtx/qtxsdk/components.h"
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef USE_LIBLZO
#include <lzo1x.h>
#else
#include "../libmpcodecs/native/minilzo.h"
#endif


typedef struct
{
  uint32_t order, type, scope;
  uint32_t comp_algo;
  uint8_t *comp_settings;
  int comp_settings_len;
} mkv_content_encoding_t;

typedef struct mkv_track
{
  int tnum;
  
  char *codec_id;
  int ms_compat;
  char *language;

  int type;
  
  uint32_t v_width, v_height, v_dwidth, v_dheight;
  float v_frate;

  uint32_t a_formattag;
  uint32_t a_channels, a_bps;
  float a_sfreq;

  float default_duration;

  int default_track;

  void *private_data;
  unsigned int private_size;

  /* for vorbis audio */
  unsigned char *headers[3];
  uint32_t header_sizes[3];

  /* stuff for realmedia */
  int realmedia;
  int rv_kf_base, rv_kf_pts;
  float rv_pts;  /* previous video timestamp */
  float ra_pts;  /* previous audio timestamp */

  /* stuff for quicktime */
  int fix_i_bps;
  float qt_last_a_pts;

  int subtitle_type;

  /* generic content encoding support */
  mkv_content_encoding_t *encodings;
  int num_encodings;
} mkv_track_t;

typedef struct mkv_index
{
  int tnum;
  uint64_t timecode, filepos;
} mkv_index_t;

typedef struct mkv_chapter
{
  uint64_t start, end;
} mkv_chapter_t;

typedef struct mkv_demuxer
{
  off_t segment_start;

  float duration, last_pts;
  uint64_t last_filepos;

  mkv_track_t **tracks;
  int num_tracks;

  uint64_t tc_scale, cluster_tc, first_tc;
  int has_first_tc;

  uint64_t clear_subs_at[SUB_MAX_TEXT];
  subtitle subs;

  uint64_t cluster_size;
  uint64_t blockgroup_size;

  mkv_index_t *indexes;
  int num_indexes;

  off_t *parsed_cues;
  int parsed_cues_num;
  off_t *parsed_seekhead;
  int parsed_seekhead_num;

  uint64_t *cluster_positions;
  int num_cluster_pos;

  int64_t skip_to_timecode;
  int v_skip_to_keyframe, a_skip_to_keyframe;

  mkv_chapter_t *chapters;
  int num_chapters;
  int64_t stop_timecode;
} mkv_demuxer_t;


typedef struct
{
  uint32_t chunks;              /* number of chunks */
  uint32_t timestamp;           /* timestamp from packet header */
  uint32_t len;                 /* length of actual data */
  uint32_t chunktab;            /* offset to chunk offset array */
} dp_hdr_t;

typedef struct __attribute__((__packed__))
{
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

typedef struct __attribute__((__packed__))
{
  uint32_t fourcc1;             /* '.', 'r', 'a', 0xfd */
  uint16_t version1;            /* 4 or 5 */
  uint16_t unknown1;            /* 00 000 */
  uint32_t fourcc2;             /* .ra4 or .ra5 */
  uint32_t unknown2;            /* ??? */
  uint16_t version2;            /* 4 or 5 */
  uint32_t header_size;         /* == 0x4e */
  uint16_t flavor;              /* codec flavor id */
  uint32_t coded_frame_size;    /* coded frame size */
  uint32_t unknown3;            /* big number */
  uint32_t unknown4;            /* bigger number */
  uint32_t unknown5;            /* yet another number */
  uint16_t sub_packet_h;
  uint16_t frame_size;
  uint16_t sub_packet_size;
  uint16_t unknown6;            /* 00 00 */
  uint16_t sample_rate;
  uint16_t unknown8;            /* 0 */
  uint16_t sample_size;
  uint16_t channels;
} real_audio_v4_props_t;

typedef struct __attribute__((__packed__))
{
  uint32_t fourcc1;             /* '.', 'r', 'a', 0xfd */
  uint16_t version1;            /* 4 or 5 */
  uint16_t unknown1;            /* 00 000 */
  uint32_t fourcc2;             /* .ra4 or .ra5 */
  uint32_t unknown2;            /* ??? */
  uint16_t version2;            /* 4 or 5 */
  uint32_t header_size;         /* == 0x4e */
  uint16_t flavor;              /* codec flavor id */
  uint32_t coded_frame_size;    /* coded frame size */
  uint32_t unknown3;            /* big number */
  uint32_t unknown4;            /* bigger number */
  uint32_t unknown5;            /* yet another number */
  uint16_t sub_packet_h;
  uint16_t frame_size;
  uint16_t sub_packet_size;
  uint16_t unknown6;            /* 00 00 */
  uint8_t unknown7[6];          /* 0, srate, 0 */
  uint16_t sample_rate;
  uint16_t unknown8;            /* 0 */
  uint16_t sample_size;
  uint16_t channels;
  uint32_t genr;                /* "genr" */
  uint32_t fourcc3;             /* fourcc */
} real_audio_v5_props_t;


/* for e.g. "-slang ger" */
extern char *dvdsub_lang;
extern char *audio_lang;


static mkv_track_t *
demux_mkv_find_track_by_num (mkv_demuxer_t *d, int n, int type)
{
  int i, id;

  for (i=0, id=0; i < d->num_tracks; i++)
    if (d->tracks[i] != NULL && d->tracks[i]->type == type)
      if (id++ == n)
        return d->tracks[i];
  
  return NULL;
}

static mkv_track_t *
demux_mkv_find_track_by_language (mkv_demuxer_t *d, char *language, int type)
{
  int i, len;
  
  language += strspn(language,",");
  while((len = strcspn(language,",")) > 0)
    {
      for (i=0; i < d->num_tracks; i++)
        if (d->tracks[i] != NULL && d->tracks[i]->language != NULL &&
            d->tracks[i]->type == type &&
            !strncmp(d->tracks[i]->language, language, len))
          return d->tracks[i];
      language += len;
      language += strspn(language,",");
    }
  
  return NULL;
}

static void
add_cluster_position (mkv_demuxer_t *mkv_d, uint64_t position)
{
  int i = mkv_d->num_cluster_pos;

  while (i--)
    if (mkv_d->cluster_positions[i] == position)
      return;

  if (!mkv_d->cluster_positions)
    mkv_d->cluster_positions = (uint64_t *) malloc (32 * sizeof (uint64_t));
  else if (!(mkv_d->num_cluster_pos % 32))
    mkv_d->cluster_positions = (uint64_t *) realloc(mkv_d->cluster_positions,
                                                    (mkv_d->num_cluster_pos+32)
                                                    * sizeof (uint64_t));
  mkv_d->cluster_positions[mkv_d->num_cluster_pos++] = position;
}


#define AAC_SYNC_EXTENSION_TYPE 0x02b7
static int
aac_get_sample_rate_index (uint32_t sample_rate)
{
  if (92017 <= sample_rate)
    return 0;
  else if (75132 <= sample_rate)
    return 1;
  else if (55426 <= sample_rate)
    return 2;
  else if (46009 <= sample_rate)
    return 3;
  else if (37566 <= sample_rate)
    return 4;
  else if (27713 <= sample_rate)
    return 5;
  else if (23004 <= sample_rate)
    return 6;
  else if (18783 <= sample_rate)
    return 7;
  else if (13856 <= sample_rate)
    return 8;
  else if (11502 <= sample_rate)
    return 9;
  else if (9391 <= sample_rate)
    return 10;
  else
    return 11;
}


static int
demux_mkv_decode (mkv_track_t *track, uint8_t *src, uint8_t **dest,
                  uint32_t *size, uint32_t type)
{
  int i, result;
  int modified = 0;

  *dest = src;
  if (track->num_encodings <= 0)
    return 0;

  for (i=0; i<track->num_encodings; i++)
    {
      if (!(track->encodings[i].scope & type))
        continue;

#ifdef HAVE_ZLIB
      if (track->encodings[i].comp_algo == 0)
        {
          /* zlib encoded track */
          z_stream zstream;

          zstream.zalloc = (alloc_func) 0;
          zstream.zfree = (free_func) 0;
          zstream.opaque = (voidpf) 0;
          if (inflateInit (&zstream) != Z_OK)
            {
              mp_msg (MSGT_DEMUX, MSGL_WARN,
                      "[mkv] zlib initialization failed.\n");
              return modified;
            }
          zstream.next_in = (Bytef *) src;
          zstream.avail_in = *size;

          modified = 1;
          *dest = (uint8_t *) malloc (*size);
          zstream.avail_out = *size;
          do {
            *size += 4000;
            *dest = (uint8_t *) realloc (*dest, *size);
            zstream.next_out = (Bytef *) (*dest + zstream.total_out);
            result = inflate (&zstream, Z_NO_FLUSH);
            if (result != Z_OK && result != Z_STREAM_END)
              {
                mp_msg (MSGT_DEMUX, MSGL_WARN,
                        "[mkv] zlib decompression failed.\n");
                free(*dest);
                *dest = NULL;
                inflateEnd (&zstream);
                return modified;
              }
            zstream.avail_out += 4000;
          } while (zstream.avail_out == 4000 &&
                   zstream.avail_in != 0 && result != Z_STREAM_END);

          *size = zstream.total_out;
          inflateEnd (&zstream);
        }
#endif
      if (track->encodings[i].comp_algo == 2)
        {
          /* lzo encoded track */
          int dstlen = *size * 3;

          if (lzo_init () != LZO_E_OK)
            {
              mp_msg (MSGT_DEMUX, MSGL_WARN,
                      "[mkv] lzo initialization failed.\n");
              return modified;
            }

          *dest = (uint8_t *) malloc (dstlen);
          while (1)
            {
              result = lzo1x_decompress_safe (src, *size, *dest, &dstlen,
                                              NULL);
              if (result == LZO_E_OK)
                break;
              if (result != LZO_E_OUTPUT_OVERRUN)
                {
                  mp_msg (MSGT_DEMUX, MSGL_WARN,
                          "[mkv] lzo decompression failed.\n");
                  return modified;
                }
              mp_msg (MSGT_DEMUX, MSGL_DBG2,
                      "[mkv] lzo decompression buffer too small.\n");
              dstlen *= 2;
              *dest = (uint8_t *) realloc (*dest, dstlen);
            }
          *size = dstlen;
        }
    }

  return modified;
}


static int
demux_mkv_read_info (demuxer_t *demuxer)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  stream_t *s = demuxer->stream;
  uint64_t length, l;
  int il;

  length = ebml_read_length (s, NULL);
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_TIMECODESCALE:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 1;
            mkv_d->tc_scale = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] | + timecode scale: %llu\n",
                    mkv_d->tc_scale);
            break;
          }

        case MATROSKA_ID_DURATION:
          {
            long double num = ebml_read_float (s, &l);
            if (num == EBML_FLOAT_INVALID)
              return 1;
            mkv_d->duration = num * mkv_d->tc_scale / 1000000000.0;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] | + duration: %.3fs\n",
                    mkv_d->duration);
            break;
          }

        default:
          ebml_read_skip (s, &l); 
          break;
        }
      length -= l + il;
    }
  return 0;
}

static int
demux_mkv_read_trackencodings (demuxer_t *demuxer, mkv_track_t *track)
{
  stream_t *s = demuxer->stream;
  mkv_content_encoding_t *ce, e;
  uint64_t len, length, l;
  int il, n;

  ce = (mkv_content_encoding_t *) malloc (sizeof (*ce));
  n = 0;

  len = length = ebml_read_length (s, &il);
  len += il;
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_CONTENTENCODING:
          {
            uint64_t len;
            int i;

            memset (&e, 0, sizeof (e));
            e.scope = 1;

            len = ebml_read_length (s, &i);
            l = len + i;

            while (len > 0)
              {
                uint64_t num, l;
                int il;

                switch (ebml_read_id (s, &il))
                  {
                  case MATROSKA_ID_CONTENTENCODINGORDER:
                    num = ebml_read_uint (s, &l);
                    if (num == EBML_UINT_INVALID)
                      return 0;
                    e.order = num;
                    break;

                  case MATROSKA_ID_CONTENTENCODINGSCOPE:
                    num = ebml_read_uint (s, &l);
                    if (num == EBML_UINT_INVALID)
                      return 0;
                    e.scope = num;
                    break;

                  case MATROSKA_ID_CONTENTENCODINGTYPE:
                    num = ebml_read_uint (s, &l);
                    if (num == EBML_UINT_INVALID)
                      return 0;
                    e.type = num;
                    break;

                  case MATROSKA_ID_CONTENTCOMPRESSION:
                    {
                      uint64_t le;

                      le = ebml_read_length (s, &i);
                      l = le + i;

                      while (le > 0)
                        {
                          uint64_t l;
                          int il;

                          switch (ebml_read_id (s, &il))
                            {
                            case MATROSKA_ID_CONTENTCOMPALGO:
                              num = ebml_read_uint (s, &l);
                              if (num == EBML_UINT_INVALID)
                                return 0;
                              e.comp_algo = num;
                              break;

                            case MATROSKA_ID_CONTENTCOMPSETTINGS:
                              l = ebml_read_length (s, &i);
                              e.comp_settings = (uint8_t *) malloc (l);
                              stream_read (s, e.comp_settings, l);
                              e.comp_settings_len = l;
                              l += i;
                              break;

                            default:
                              ebml_read_skip (s, &l);
                              break;
                            }
                          le -= l + il;
                        }

                      if (e.type == 1)
                        {
                          mp_msg(MSGT_DEMUX, MSGL_WARN,
                                 "[mkv] Track number %u has been encrypted "
                                 "and decryption has not yet been implemented."
                                 " Skipping track.\n", track->tnum);
                        }
                      else if (e.type != 0)
                        {
                          mp_msg(MSGT_DEMUX, MSGL_WARN,
                                 "[mkv] Unknown content encoding type for "
                                 "track %u. Skipping track.\n", track->tnum);
                        }

                      if (e.comp_algo != 0 && e.comp_algo != 2)
                        {
                          mp_msg (MSGT_DEMUX, MSGL_WARN,
                                  "[mkv] Track %u has been compressed with an "
                                  "unknown/unsupported compression algorithm "
                                  "(%u). Skipping track.\n",
                                  track->tnum, e.comp_algo);
                        }
#ifndef HAVE_ZLIB
                      else if (e.comp_algo == 0)
                        {
                          mp_msg (MSGT_DEMUX, MSGL_WARN,
                                  "Track %u was compressed with zlib but "
                                  "mplayer has not been compiled with support "
                                  "for zlib compression. Skipping track.\n",
                                  track->tnum);
                        }
#endif

                      break;
                    }

                  default:
                    ebml_read_skip (s, &l);
                    break;
                  }
                len -= l + il;
              }
            for (i=0; i<n; i++)
              if (e.order <= ce[i].order)
                break;
            ce = (mkv_content_encoding_t *) realloc (ce, (n+1) *sizeof (*ce));
            memmove (ce+i+1, ce+i, (n-i) * sizeof (*ce));
            memcpy (ce+i, &e, sizeof (e));
            n++;
            break;
          }

        default:
          ebml_read_skip (s, &l);
          break;
        }

      length -= l + il;
    }

  track->encodings = ce;
  track->num_encodings = n;
  return len;
}

static int
demux_mkv_read_trackaudio (demuxer_t *demuxer, mkv_track_t *track)
{
  stream_t *s = demuxer->stream;
  uint64_t len, length, l;
  int il;

  track->a_sfreq = 8000.0;
  track->a_channels = 1;

  len = length = ebml_read_length (s, &il);
  len += il;
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_AUDIOSAMPLINGFREQ:
          {
            long double num = ebml_read_float (s, &l);
            if (num == EBML_FLOAT_INVALID)
              return 0;
            track->a_sfreq = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Sampling frequency: %f\n",
                    track->a_sfreq);
            break;
          }

        case MATROSKA_ID_AUDIOBITDEPTH:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->a_bps = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Bit depth: %u\n",
                    track->a_bps);
            break;
          }

        case MATROSKA_ID_AUDIOCHANNELS:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->a_channels = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Channels: %u\n",
                    track->a_channels);
            break;
          }

        default:
            ebml_read_skip (s, &l);
            break;
        }
      length -= l + il;
    }
  return len;
}

static int
demux_mkv_read_trackvideo (demuxer_t *demuxer, mkv_track_t *track)
{
  stream_t *s = demuxer->stream;
  uint64_t len, length, l;
  int il;

  len = length = ebml_read_length (s, &il);
  len += il;
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_VIDEOFRAMERATE:
          {
            long double num = ebml_read_float (s, &l);
            if (num == EBML_FLOAT_INVALID)
              return 0;
            track->v_frate = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Frame rate: %f\n",
                    track->v_frate);
            break;
          }

        case MATROSKA_ID_VIDEODISPLAYWIDTH:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->v_dwidth = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Display width: %u\n",
                    track->v_dwidth);
            break;
          }

        case MATROSKA_ID_VIDEODISPLAYHEIGHT:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->v_dheight = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Display height: %u\n",
                    track->v_dheight);
            break;
          }

        case MATROSKA_ID_VIDEOPIXELWIDTH:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->v_width = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Pixel width: %u\n",
                    track->v_width);
            break;
          }

        case MATROSKA_ID_VIDEOPIXELHEIGHT:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->v_height = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Pixel height: %u\n",
                    track->v_height);
            break;
          }

        default:
            ebml_read_skip (s, &l);
            break;
        }
      length -= l + il;
    }
  return len;
}

static int
demux_mkv_read_trackentry (demuxer_t *demuxer)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  stream_t *s = demuxer->stream;
  mkv_track_t *track;
  uint64_t len, length, l;
  int il;

  track = (mkv_track_t *) malloc (sizeof (*track));
  memset(track, 0, sizeof(*track));
  /* set default values */
  track->default_track = 1;
  track->language = strdup("eng");

  len = length = ebml_read_length (s, &il);
  len += il;
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_TRACKNUMBER:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->tnum = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Track number: %u\n",
                    track->tnum);
            break;
          }

        case MATROSKA_ID_TRACKTYPE:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->type = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Track type: ");
            switch (track->type)
              {
              case MATROSKA_TRACK_AUDIO:
                mp_msg (MSGT_DEMUX, MSGL_V, "Audio\n");
                break;
              case MATROSKA_TRACK_VIDEO:
                mp_msg (MSGT_DEMUX, MSGL_V, "Video\n");
                break;
              case MATROSKA_TRACK_SUBTITLE:
                mp_msg (MSGT_DEMUX, MSGL_V, "Subtitle\n");
                break;
              default:
                mp_msg (MSGT_DEMUX, MSGL_V, "unknown\n");
                break;
            }
            break;
          }

        case MATROSKA_ID_TRACKAUDIO:
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Audio track\n");
          l = demux_mkv_read_trackaudio (demuxer, track);
          if (l == 0)
            return 0;
          break;

        case MATROSKA_ID_TRACKVIDEO:
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Video track\n");
          l = demux_mkv_read_trackvideo (demuxer, track);
          if (l == 0)
            return 0;
          break;

        case MATROSKA_ID_CODECID:
          track->codec_id = ebml_read_ascii (s, &l);
          if (track->codec_id == NULL)
            return 0;
          if (!strcmp (track->codec_id, MKV_V_MSCOMP) ||
              !strcmp (track->codec_id, MKV_A_ACM))
            track->ms_compat = 1;
          else if (!strcmp (track->codec_id, MKV_S_VOBSUB))
            track->subtitle_type = MATROSKA_SUBTYPE_VOBSUB;
          else if (!strcmp (track->codec_id, MKV_S_TEXTSSA)
                   || !strcmp (track->codec_id, MKV_S_TEXTASS)
                   || !strcmp (track->codec_id, MKV_S_SSA)
                   || !strcmp (track->codec_id, MKV_S_ASS))
            {
              track->subtitle_type = MATROSKA_SUBTYPE_SSA;
              sub_utf8 = 1;
            }
          else if (!strcmp (track->codec_id, MKV_S_TEXTASCII))
            track->subtitle_type = MATROSKA_SUBTYPE_TEXT;
          if (!strcmp (track->codec_id, MKV_S_TEXTUTF8))
            {
              track->subtitle_type = MATROSKA_SUBTYPE_TEXT;
              sub_utf8 = 1;
            }
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Codec ID: %s\n",
                  track->codec_id);
          break;

        case MATROSKA_ID_CODECPRIVATE:
          {
            int x;
            uint64_t num = ebml_read_length (s, &x);
            l = x + num;
            track->private_data = malloc (num);
            if (stream_read(s, track->private_data, num) != (int) num)
              return 0;
            track->private_size = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + CodecPrivate, length "
                    "%u\n", track->private_size);
            break;
          }

        case MATROSKA_ID_TRACKLANGUAGE:
          track->language = ebml_read_utf8 (s, &l);
          if (track->language == NULL)
            return 0;
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Language: %s\n",
                  track->language);
          break;

        case MATROSKA_ID_TRACKFLAGDEFAULT:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->default_track = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Default flag: %u\n",
                    track->default_track);
            break;
          }

        case MATROSKA_ID_TRACKDEFAULTDURATION:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            if (num == 0)
              mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Default duration: 0");
            else
              {
                track->v_frate = 1000000000.0 / num;
                mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Default duration: "
                        "%.3fms ( = %.3f fps)\n",num/1000000.0,track->v_frate);
              }
            break;
          }

        case MATROSKA_ID_TRACKENCODINGS:
          l = demux_mkv_read_trackencodings (demuxer, track);
          if (l == 0)
            return 0;
          break;

        default:
          ebml_read_skip (s, &l);
          break;
        }
      length -= l + il;
    }

  mkv_d->tracks[mkv_d->num_tracks++] = track;
  return len;
}

static int
demux_mkv_read_tracks (demuxer_t *demuxer)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  stream_t *s = demuxer->stream;
  uint64_t length, l;
  int il;

  mkv_d->tracks = (mkv_track_t **) malloc (sizeof (*mkv_d->tracks));
  mkv_d->num_tracks = 0;

  length = ebml_read_length (s, NULL);
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_TRACKENTRY:
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] | + a track...\n");
          mkv_d->tracks = (mkv_track_t **) realloc (mkv_d->tracks,
                                                    (mkv_d->num_tracks+1)
                                                    *sizeof (*mkv_d->tracks));
          l = demux_mkv_read_trackentry (demuxer);
          if (l == 0)
            return 1;
          break;

        default:
            ebml_read_skip (s, &l);
            break;
        }
      length -= l + il;
    }
  return 0;
}

static int
demux_mkv_read_cues (demuxer_t *demuxer)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  stream_t *s = demuxer->stream;
  uint64_t length, l, time, track, pos;
  off_t off;
  int i, il;

  off = stream_tell (s);
  for (i=0; i<mkv_d->parsed_cues_num; i++)
    if (mkv_d->parsed_cues[i] == off)
      {
        ebml_read_skip (s, NULL);
        return 0;
      }
  mkv_d->parsed_cues = (off_t *) realloc (mkv_d->parsed_cues, 
                                          (mkv_d->parsed_cues_num+1)
                                          * sizeof (off_t));
  mkv_d->parsed_cues[mkv_d->parsed_cues_num++] = off;

  mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] /---- [ parsing cues ] -----------\n");
  length = ebml_read_length (s, NULL);

  while (length > 0)
    {
      time = track = pos = EBML_UINT_INVALID;

      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_POINTENTRY:
          {
            uint64_t len;

            len = ebml_read_length (s, &i);
            l = len + i;

            while (len > 0)
              {
                uint64_t l;
                int il;

                switch (ebml_read_id (s, &il))
                  {
                  case MATROSKA_ID_CUETIME:
                    time = ebml_read_uint (s, &l);
                    break;

                  case MATROSKA_ID_CUETRACKPOSITION:
                    {
                      uint64_t le;

                      le = ebml_read_length (s, &i);
                      l = le + i;

                      while (le > 0)
                        {
                          uint64_t l;
                          int il;

                          switch (ebml_read_id (s, &il))
                            {
                            case MATROSKA_ID_CUETRACK:
                              track = ebml_read_uint (s, &l);
                              break;

                            case MATROSKA_ID_CUECLUSTERPOSITION:
                              pos = ebml_read_uint (s, &l);
                              break;

                            default:
                              ebml_read_skip (s, &l);
                              break;
                            }
                          le -= l + il;
                        }
                      break;
                    }

                  default:
                    ebml_read_skip (s, &l);
                    break;
                  }
                len -= l + il;
              }
            break;
          }

        default:
          ebml_read_skip (s, &l);
          break;
        }

      length -= l + il;

      if (time != EBML_UINT_INVALID && track != EBML_UINT_INVALID
          && pos != EBML_UINT_INVALID)
        {
          if (mkv_d->indexes == NULL)
            mkv_d->indexes = (mkv_index_t *) malloc (32*sizeof (mkv_index_t));
          else if (mkv_d->num_indexes % 32 == 0)
            mkv_d->indexes = (mkv_index_t *) realloc (mkv_d->indexes,
                                                      (mkv_d->num_indexes+32)
                                                      *sizeof (mkv_index_t));
          mkv_d->indexes[mkv_d->num_indexes].tnum = track;
          mkv_d->indexes[mkv_d->num_indexes].timecode = time;
          mkv_d->indexes[mkv_d->num_indexes].filepos =mkv_d->segment_start+pos;
          mp_msg (MSGT_DEMUX, MSGL_DBG2, "[mkv] |+ found cue point "
                  "for track %llu: timecode %llu, filepos: %llu\n", 
                  track, time, mkv_d->segment_start + pos);
          mkv_d->num_indexes++;
        }
    }

  mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] \\---- [ parsing cues ] -----------\n");
  return 0;
}

static int
demux_mkv_read_chapters (demuxer_t *demuxer)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  stream_t *s = demuxer->stream;
  uint64_t length, l;
  int il;

  if (mkv_d->chapters)
    {
      ebml_read_skip (s, NULL);
      return 0;
    }

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] /---- [ parsing chapters ] ---------\n");
  length = ebml_read_length (s, NULL);

  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_EDITIONENTRY:
          {
            uint64_t len;
            int i;

            len = ebml_read_length (s, &i);
            l = len + i;

            while (len > 0)
              {
                uint64_t l;
                int il;

                switch (ebml_read_id (s, &il))
                  {
                  case MATROSKA_ID_CHAPTERATOM:
                    {
                      uint64_t len, start=0, end=0;
                      int i;

                      len = ebml_read_length (s, &i);
                      l = len + i;

                      if (mkv_d->chapters == NULL)
                        mkv_d->chapters = malloc (32*sizeof(*mkv_d->chapters));
                      else if (!(mkv_d->num_chapters % 32))
                        mkv_d->chapters = realloc (mkv_d->chapters,
                                                   (mkv_d->num_chapters + 32)
                                                   * sizeof(*mkv_d->chapters));

                      while (len > 0)
                        {
                          uint64_t l;
                          int il;

                          switch (ebml_read_id (s, &il))
                            {
                            case MATROSKA_ID_CHAPTERTIMESTART:
                              start = ebml_read_uint (s, &l) / 1000000;
                              break;

                            case MATROSKA_ID_CHAPTERTIMEEND:
                              end = ebml_read_uint (s, &l) / 1000000;
                              break;

                            default:
                              ebml_read_skip (s, &l);
                              break;
                            }
                          len -= l + il;
                        }

                      mkv_d->chapters[mkv_d->num_chapters].start = start;
                      mkv_d->chapters[mkv_d->num_chapters].end = end;
                      mp_msg(MSGT_DEMUX, MSGL_V,
                             "[mkv] Chapter %u from %02d:%02d:%02d."
                             "%03d to %02d:%02d:%02d.%03d\n",
                             ++mkv_d->num_chapters,
                             (int) (start / 60 / 60 / 1000),
                             (int) ((start / 60 / 1000) % 60),
                             (int) ((start / 1000) % 60),
                             (int) (start % 1000),
                             (int) (end / 60 / 60 / 1000),
                             (int) ((end / 60 / 1000) % 60),
                             (int) ((end / 1000) % 60),
                             (int) (end % 1000));
                      break;
                    }

                  default:
                    ebml_read_skip (s, &l);
                    break;
                  }
                len -= l + il;
              }
            break;
          }

        default:
          ebml_read_skip (s, &l);
          break;
        }

      length -= l + il;
    }

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] \\---- [ parsing chapters ] ---------\n");
  return 0;
}

static int
demux_mkv_read_tags (demuxer_t *demuxer)
{
  ebml_read_skip (demuxer->stream, NULL);
  return 0;
}

static int
demux_mkv_read_seekhead (demuxer_t *demuxer)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  stream_t *s = demuxer->stream;
  uint64_t length, l, seek_pos, saved_pos, num;
  uint32_t seek_id;
  int i, il, res = 0;
  off_t off;

  off = stream_tell (s);
  for (i=0; i<mkv_d->parsed_seekhead_num; i++)
    if (mkv_d->parsed_seekhead[i] == off)
      {
        ebml_read_skip (s, NULL);
        return 0;
      }
  mkv_d->parsed_seekhead = (off_t *) realloc (mkv_d->parsed_seekhead, 
                                              (mkv_d->parsed_seekhead_num+1)
                                              * sizeof (off_t));
  mkv_d->parsed_seekhead[mkv_d->parsed_seekhead_num++] = off;

  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] /---- [ parsing seek head ] ---------\n");
  length = ebml_read_length (s, NULL);
  while (length > 0 && !res)
    {

      seek_id = 0;
      seek_pos = EBML_UINT_INVALID;

      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_SEEKENTRY:
          {
            uint64_t len;

            len = ebml_read_length (s, &i);
            l = len + i;

            while (len > 0)
              {
                uint64_t l;
                int il;

                switch (ebml_read_id (s, &il))
                  {
                  case MATROSKA_ID_SEEKID:
                    num = ebml_read_uint (s, &l);
                    if (num != EBML_UINT_INVALID)
                      seek_id = num;
                    break;

                  case MATROSKA_ID_SEEKPOSITION:
                    seek_pos = ebml_read_uint (s, &l);
                    break;

                  default:
                    ebml_read_skip (s, &l);
                    break;
                  }
                len -= l + il;
              }

            break;
          }

        default:
            ebml_read_skip (s, &l);
            break;
        }
      length -= l + il;

      if (seek_id == 0 || seek_id == MATROSKA_ID_CLUSTER
          || seek_pos == EBML_UINT_INVALID ||
          ((mkv_d->segment_start + seek_pos) >= (uint64_t)demuxer->movi_end))
        continue;

      saved_pos = stream_tell (s);
      if (!stream_seek (s, mkv_d->segment_start + seek_pos))
        res = 1;
      else
        {
          if (ebml_read_id (s, &il) != seek_id)
            res = 1;
          else
            switch (seek_id)
              {
              case MATROSKA_ID_CUES:
                if (demux_mkv_read_cues (demuxer))
                  res = 1;
                break;

              case MATROSKA_ID_TAGS:
                if (demux_mkv_read_tags (demuxer))
                  res = 1;
                break;

              case MATROSKA_ID_SEEKHEAD:
                if (demux_mkv_read_seekhead (demuxer))
                  res = 1;
                break;

              case MATROSKA_ID_CHAPTERS:
                if (demux_mkv_read_chapters (demuxer))
                  res = 1;
                break;
              }
        }

      stream_seek (s, saved_pos);
    }
  if (length > 0)
     stream_seek (s, stream_tell (s) + length);
  mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] \\---- [ parsing seek head ] ---------\n");
  return res;
}

static void
display_tracks (mkv_demuxer_t *mkv_d)
{
  int i, vid=0, aid=0, sid=0;

  for (i=0; i<mkv_d->num_tracks; i++)
    {
      char *type = "unknown", str[32];
      *str = '\0';
      switch (mkv_d->tracks[i]->type)
        {
        case MATROSKA_TRACK_VIDEO:
          type = "video";
          sprintf (str, "-vid %u", vid++);
          break;
        case MATROSKA_TRACK_AUDIO:
          type = "audio";
          sprintf (str, "-aid %u, -alang %.5s",aid++,mkv_d->tracks[i]->language);
          break;
        case MATROSKA_TRACK_SUBTITLE:
          type = "sutitles";
          sprintf (str, "-sid %u, -slang %.5s",sid++,mkv_d->tracks[i]->language);
          break;
        }
      mp_msg(MSGT_DEMUX, MSGL_INFO, "[mkv] Track ID %u: %s (%s), %s\n",
             mkv_d->tracks[i]->tnum, type, mkv_d->tracks[i]->codec_id, str);
    }
}

static int
demux_mkv_open_video (demuxer_t *demuxer, mkv_track_t *track)
{
  BITMAPINFOHEADER *bih;
  void *ImageDesc = NULL;
  sh_video_t *sh_v;

  if (track->ms_compat)  /* MS compatibility mode */
    {
      BITMAPINFOHEADER *src;

      if (track->private_data == NULL
          || track->private_size < sizeof (BITMAPINFOHEADER))
        return 1;

      src = (BITMAPINFOHEADER *) track->private_data;
      bih = (BITMAPINFOHEADER *) malloc (track->private_size);
      memset (bih, 0, track->private_size);
      bih->biSize = le2me_32 (src->biSize);
      bih->biWidth = le2me_32 (src->biWidth);
      bih->biHeight = le2me_32 (src->biHeight);
      bih->biPlanes = le2me_16 (src->biPlanes);
      bih->biBitCount = le2me_16 (src->biBitCount);
      bih->biCompression = le2me_32 (src->biCompression);
      bih->biSizeImage = le2me_32 (src->biSizeImage);
      bih->biXPelsPerMeter = le2me_32 (src->biXPelsPerMeter);
      bih->biYPelsPerMeter = le2me_32 (src->biYPelsPerMeter);
      bih->biClrUsed = le2me_32 (src->biClrUsed);
      bih->biClrImportant = le2me_32 (src->biClrImportant);
      memcpy((char *) bih + sizeof (BITMAPINFOHEADER),
             (char *) src + sizeof (BITMAPINFOHEADER),
             track->private_size - sizeof (BITMAPINFOHEADER));

      if (track->v_width == 0)
        track->v_width = bih->biWidth;
      if (track->v_height == 0)
        track->v_height = bih->biHeight;
    }
  else
    {
      bih = (BITMAPINFOHEADER *) malloc (sizeof (BITMAPINFOHEADER));
      memset (bih, 0, sizeof (BITMAPINFOHEADER));
      bih->biSize = sizeof (BITMAPINFOHEADER);
      bih->biWidth = track->v_width;
      bih->biHeight = track->v_height;
      bih->biBitCount = 24;
      bih->biSizeImage = bih->biWidth * bih->biHeight * bih->biBitCount/8;

      if (track->private_size >= sizeof (real_video_props_t)
          && (!strcmp (track->codec_id, MKV_V_REALV10)
              || !strcmp (track->codec_id, MKV_V_REALV20)
              || !strcmp (track->codec_id, MKV_V_REALV30)
              || !strcmp (track->codec_id, MKV_V_REALV40)))
        {
          unsigned char *dst, *src;
          real_video_props_t *rvp;
          uint32_t type2;

          rvp = (real_video_props_t *) track->private_data;
          src = (unsigned char *) (rvp + 1);

          bih = (BITMAPINFOHEADER *) realloc(bih,
                                             sizeof (BITMAPINFOHEADER)+12);
          bih->biSize = 48;
          bih->biPlanes = 1;
          type2 = be2me_32 (rvp->type2);
          if (type2 == 0x10003000 || type2 == 0x10003001)
            bih->biCompression=mmioFOURCC('R','V','1','3');
          else
            bih->biCompression=mmioFOURCC('R','V',track->codec_id[9],'0');
          dst = (unsigned char *) (bih + 1);
          ((unsigned int *) dst)[0] = be2me_32 (rvp->type1);
          ((unsigned int *) dst)[1] = type2;

          if (bih->biCompression <= 0x30335652 && type2 >= 0x20200002)
            {
              /* read secondary WxH for the cmsg24[] (see vd_realvid.c) */
              ((unsigned short *)(bih+1))[4] = 4 * (unsigned short) src[0];
              ((unsigned short *)(bih+1))[5] = 4 * (unsigned short) src[1];
            }
          else
            memset(&dst[8], 0, 4);
          track->realmedia = 1;

#ifdef USE_QTX_CODECS
        }
      else if (track->private_size >= sizeof (ImageDescription)
               && !strcmp(track->codec_id, MKV_V_QUICKTIME))
        {
          ImageDescriptionPtr idesc;

          idesc = (ImageDescriptionPtr) track->private_data;
          idesc->idSize = be2me_32 (idesc->idSize);
          idesc->cType = be2me_32 (idesc->cType);
          idesc->version = be2me_16 (idesc->version);
          idesc->revisionLevel = be2me_16 (idesc->revisionLevel);
          idesc->vendor = be2me_32 (idesc->vendor);
          idesc->temporalQuality = be2me_32 (idesc->temporalQuality);
          idesc->spatialQuality = be2me_32 (idesc->spatialQuality);
          idesc->width = be2me_16 (idesc->width);
          idesc->height = be2me_16 (idesc->height);
          idesc->hRes = be2me_32 (idesc->hRes);
          idesc->vRes = be2me_32 (idesc->vRes);
          idesc->dataSize = be2me_32 (idesc->dataSize);
          idesc->frameCount = be2me_16 (idesc->frameCount);
          idesc->depth = be2me_16 (idesc->depth);
          idesc->clutID = be2me_16 (idesc->clutID);
          bih->biPlanes = 1;
          bih->biCompression = idesc->cType;
          ImageDesc = idesc;
#endif /* USE_QTX_CODECS */

        }
      else
        {
          mp_msg (MSGT_DEMUX,MSGL_WARN,"[mkv] Unknown/unsupported CodecID "
                  "(%s) or missing/bad CodecPrivate data (track %u).\n",
                  track->codec_id, track->tnum);
          free(bih);
          return 1;
        }
    }

  sh_v = new_sh_video (demuxer, track->tnum);
  sh_v->bih = bih;
  sh_v->format = sh_v->bih->biCompression;
  if (track->v_frate == 0.0)
    track->v_frate = 25.0;
  sh_v->fps = track->v_frate;
  sh_v->frametime = 1 / track->v_frate;
  if (!track->realmedia)
    {
      sh_v->disp_w = track->v_width;
      sh_v->disp_h = track->v_height;
      sh_v->aspect = (float)track->v_dwidth / (float)track->v_dheight;
    }
  else
    {
      // vd_realvid.c will set aspect to disp_w/disp_h and rederive
      // disp_w and disp_h from the RealVideo stream contents returned
      // by the Real DLLs. If DisplayWidth/DisplayHeight was not set in
      // the Matroska file then it has already been set to PixelWidth/Height
      // by check_track_information.
      sh_v->disp_w = track->v_dwidth;
      sh_v->disp_h = track->v_dheight;
    }
  sh_v->ImageDesc = ImageDesc;
  mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] Aspect: %f\n", sh_v->aspect);

  sh_v->ds = demuxer->video;
  return 0;
}

static int
demux_mkv_open_audio (demuxer_t *demuxer, mkv_track_t *track)
{
  sh_audio_t *sh_a = new_sh_audio(demuxer, track->tnum);
  demux_packet_t *dp;
  int i;

  sh_a->ds = demuxer->audio;
  sh_a->wf = (WAVEFORMATEX *) malloc (sizeof (WAVEFORMATEX));
  if (track->ms_compat && (track->private_size >= sizeof(WAVEFORMATEX)))
    {
      WAVEFORMATEX *wf = (WAVEFORMATEX *)track->private_data;
      sh_a->wf = (WAVEFORMATEX *) realloc(sh_a->wf, track->private_size);
      sh_a->wf->wFormatTag = le2me_16 (wf->wFormatTag);
      sh_a->wf->nChannels = le2me_16 (wf->nChannels);
      sh_a->wf->nSamplesPerSec = le2me_32 (wf->nSamplesPerSec);
      sh_a->wf->nAvgBytesPerSec = le2me_32 (wf->nAvgBytesPerSec);
      sh_a->wf->nBlockAlign = le2me_16 (wf->nBlockAlign);
      sh_a->wf->wBitsPerSample = le2me_16 (wf->wBitsPerSample);
      sh_a->wf->cbSize = track->private_size - sizeof(WAVEFORMATEX);
      memcpy(sh_a->wf + 1, wf + 1, track->private_size - sizeof(WAVEFORMATEX));
      if (track->a_sfreq == 0.0)
        track->a_sfreq = sh_a->wf->nSamplesPerSec;
      if (track->a_channels == 0)
        track->a_channels = sh_a->wf->nChannels;
      if (track->a_bps == 0)
        track->a_bps = sh_a->wf->wBitsPerSample;
      track->a_formattag = sh_a->wf->wFormatTag;
    }
  else
    {
      memset(sh_a->wf, 0, sizeof (WAVEFORMATEX));
      if (!strcmp(track->codec_id, MKV_A_MP3) ||
          !strcmp(track->codec_id, MKV_A_MP2))
        track->a_formattag = 0x0055;
      else if (!strncmp(track->codec_id, MKV_A_AC3, strlen(MKV_A_AC3)) ||
               !strcmp(track->codec_id, MKV_A_DTS))
        track->a_formattag = 0x2000;
      else if (!strcmp(track->codec_id, MKV_A_PCM) ||
               !strcmp(track->codec_id, MKV_A_PCM_BE))
        track->a_formattag = 0x0001;
      else if (!strcmp(track->codec_id, MKV_A_AAC_2MAIN) ||
               !strncmp(track->codec_id, MKV_A_AAC_2LC,
                        strlen(MKV_A_AAC_2LC)) ||
               !strcmp(track->codec_id, MKV_A_AAC_2SSR) ||
               !strcmp(track->codec_id, MKV_A_AAC_4MAIN) ||
               !strncmp(track->codec_id, MKV_A_AAC_4LC,
                        strlen(MKV_A_AAC_4LC)) ||
               !strcmp(track->codec_id, MKV_A_AAC_4SSR) ||
               !strcmp(track->codec_id, MKV_A_AAC_4LTP))
        track->a_formattag = mmioFOURCC('M', 'P', '4', 'A');
      else if (!strcmp(track->codec_id, MKV_A_VORBIS))
        {
          unsigned char *c;
          uint32_t offset, length;

          if (track->private_data == NULL)
            return 1;

          c = (unsigned char *) track->private_data;
          if (*c != 2)
            {
              mp_msg (MSGT_DEMUX, MSGL_WARN, "[mkv] Vorbis track does not "
                      "contain valid headers.\n");
              return 1;
            }

          offset = 1;
          for (i=0; i < 2; i++)
            {
              length = 0;
              while (c[offset] == (unsigned char) 0xFF
                     && length < track->private_size)
                {
                  length += 255;
                  offset++;
                }
              if (offset >= (track->private_size - 1))
                {
                  mp_msg (MSGT_DEMUX, MSGL_WARN, "[mkv] Vorbis track "
                          "does not contain valid headers.\n");
                  return 1;
                }
              length += c[offset];
              offset++;
              track->header_sizes[i] = length;
            }

          track->headers[0] = &c[offset];
          track->headers[1] = &c[offset + track->header_sizes[0]];
          track->headers[2] = &c[offset + track->header_sizes[0] +
                                 track->header_sizes[1]];
          track->header_sizes[2] = track->private_size - offset
            - track->header_sizes[0] - track->header_sizes[1];

          track->a_formattag = 0xFFFE;
        }
      else if (!strcmp(track->codec_id, MKV_A_QDMC))
        track->a_formattag = mmioFOURCC('Q', 'D', 'M', 'C');
      else if (!strcmp(track->codec_id, MKV_A_QDMC2))
        track->a_formattag = mmioFOURCC('Q', 'D', 'M', '2');
      else if (!strcmp(track->codec_id, MKV_A_FLAC))
        {
          if (track->private_data == NULL || track->private_size == 0)
            {
              mp_msg (MSGT_DEMUX, MSGL_WARN, "[mkv] FLAC track does not "
                      "contain valid headers.\n");
              return 1;
            }
          track->a_formattag = mmioFOURCC ('f', 'L', 'a', 'C');
        }
      else if (track->private_size >= sizeof (real_audio_v4_props_t))
        {
          if (!strcmp(track->codec_id, MKV_A_REAL28))
            track->a_formattag = mmioFOURCC('2', '8', '_', '8');
          else if (!strcmp(track->codec_id, MKV_A_REALATRC))
            track->a_formattag = mmioFOURCC('a', 't', 'r', 'c');
          else if (!strcmp(track->codec_id, MKV_A_REALCOOK))
            track->a_formattag = mmioFOURCC('c', 'o', 'o', 'k');
          else if (!strcmp(track->codec_id, MKV_A_REALDNET))
            track->a_formattag = mmioFOURCC('d', 'n', 'e', 't');
          else if (!strcmp(track->codec_id, MKV_A_REALSIPR))
            track->a_formattag = mmioFOURCC('s', 'i', 'p', 'r');
        }
      else
        {
          mp_msg (MSGT_DEMUX, MSGL_WARN, "[mkv] Unknown/unsupported audio "
                  "codec ID '%s' for track %u or missing/faulty private "
                  "codec data.\n", track->codec_id, track->tnum);
          free_sh_audio (sh_a);
          return 1;
        }
    }

  sh_a->format = track->a_formattag;
  sh_a->wf->wFormatTag = track->a_formattag;
  sh_a->channels = track->a_channels;
  sh_a->wf->nChannels = track->a_channels;
  sh_a->samplerate = (uint32_t) track->a_sfreq;
  sh_a->wf->nSamplesPerSec = (uint32_t) track->a_sfreq;
  sh_a->samplesize = track->a_bps / 8;
  if (track->a_formattag == 0x0055)  /* MP3 || MP2 */
    {
      sh_a->wf->nAvgBytesPerSec = 16000;
      sh_a->wf->nBlockAlign = 1152;
      sh_a->wf->wBitsPerSample = 0;
      sh_a->samplesize = 0;
    }
  else if (!strncmp(track->codec_id, MKV_A_AC3, strlen(MKV_A_AC3)))
    {
      sh_a->wf->nAvgBytesPerSec = 16000;
      sh_a->wf->nBlockAlign = 1536;
      sh_a->wf->wBitsPerSample = 0;
      sh_a->samplesize = 0;
    }
  else if (track->a_formattag == 0x0001)  /* PCM || PCM_BE */
    {
      sh_a->wf->nAvgBytesPerSec = sh_a->channels * sh_a->samplerate*2;
      sh_a->wf->nBlockAlign = sh_a->wf->nAvgBytesPerSec;
      sh_a->wf->wBitsPerSample = track->a_bps;
      if (!strcmp(track->codec_id, MKV_A_PCM_BE))
        sh_a->format = mmioFOURCC('t', 'w', 'o', 's');
    }
  else if (!strcmp(track->codec_id, MKV_A_QDMC) ||
           !strcmp(track->codec_id, MKV_A_QDMC2))
    {
      sh_a->wf->wBitsPerSample = track->a_bps;
      sh_a->wf->nAvgBytesPerSec = 16000;
      sh_a->wf->nBlockAlign = 1486;
      track->fix_i_bps = 1;
      track->qt_last_a_pts = 0.0;
      if (track->private_data != NULL)
        {
          sh_a->codecdata=(unsigned char *)malloc(track->private_size);
          memcpy (sh_a->codecdata, track->private_data,
                  track->private_size);
          sh_a->codecdata_len = track->private_size;
        }
    }
  else if (track->a_formattag == mmioFOURCC('M', 'P', '4', 'A'))
    {
      int profile, srate_idx;

      sh_a->wf->nAvgBytesPerSec = 16000;
      sh_a->wf->nBlockAlign = 1024;
      sh_a->wf->wBitsPerSample = 0;
      sh_a->samplesize = 0;

      /* Recreate the 'private data' */
      /* which faad2 uses in its initialization */
      srate_idx = aac_get_sample_rate_index (sh_a->samplerate);
      if (!strncmp (&track->codec_id[12], "MAIN", 4))
        profile = 0;
      else if (!strncmp (&track->codec_id[12], "LC", 2))
        profile = 1;
      else if (!strncmp (&track->codec_id[12], "SSR", 3))
        profile = 2;
      else
        profile = 3;
      sh_a->codecdata = (unsigned char *) malloc (5);
      sh_a->codecdata[0] = ((profile+1) << 3) | ((srate_idx&0xE) >> 1);
      sh_a->codecdata[1] = ((srate_idx&0x1)<<7)|(track->a_channels<<3);

      if (strstr(track->codec_id, "SBR") != NULL)
        {
          /* HE-AAC (aka SBR AAC) */
          sh_a->codecdata_len = 5;

          sh_a->samplerate *= 2;
          sh_a->wf->nSamplesPerSec *= 2;
          srate_idx = aac_get_sample_rate_index(sh_a->samplerate);
          sh_a->codecdata[2] = AAC_SYNC_EXTENSION_TYPE >> 3;
          sh_a->codecdata[3] = ((AAC_SYNC_EXTENSION_TYPE&0x07)<<5) | 5;
          sh_a->codecdata[4] = (1 << 7) | (srate_idx << 3);
          track->default_duration = 1024.0 / (sh_a->samplerate / 2);
        }
      else
        {
          sh_a->codecdata_len = 2;
          track->default_duration = 1024.0 / (float)sh_a->samplerate;
        }
    }
  else if (track->a_formattag == 0xFFFE)  /* VORBIS */
    {
      for (i=0; i < 3; i++)
        {
          dp = new_demux_packet (track->header_sizes[i]);
          memcpy (dp->buffer,track->headers[i],track->header_sizes[i]);
          dp->pts = 0;
          dp->flags = 0;
          ds_add_packet (demuxer->audio, dp);
        }
    }
  else if (track->private_size >= sizeof(real_audio_v4_props_t)
           && !strncmp (track->codec_id, MKV_A_REALATRC, 7))
    {
      /* Common initialization for all RealAudio codecs */
      real_audio_v4_props_t *ra4p;
      real_audio_v5_props_t *ra5p;
      unsigned char *src;
      int codecdata_length, version;

      ra4p = (real_audio_v4_props_t *) track->private_data;
      ra5p = (real_audio_v5_props_t *) track->private_data;

      sh_a->wf->wBitsPerSample = sh_a->samplesize * 8;
      sh_a->wf->nAvgBytesPerSec = 0;  /* FIXME !? */
      sh_a->wf->nBlockAlign = be2me_16 (ra4p->frame_size);

      version = be2me_16 (ra4p->version1);

      if (version == 4)
        {
          src = (unsigned char *) (ra4p + 1);
          src += src[0] + 1;
          src += src[0] + 1;
        }
      else
        src = (unsigned char *) (ra5p + 1);

      src += 3;
      if (version == 5)
        src++;
      codecdata_length = be2me_32 (*(uint32_t *)src);
      src += 4;
      sh_a->wf->cbSize = 10 + codecdata_length;
      sh_a->wf = (WAVEFORMATEX *) realloc (sh_a->wf,
                                           sizeof (WAVEFORMATEX) +
                                           sh_a->wf->cbSize);
      ((short *)(sh_a->wf + 1))[0] = be2me_16 (ra4p->sub_packet_size);
      ((short *)(sh_a->wf + 1))[1] = be2me_16 (ra4p->sub_packet_h);
      ((short *)(sh_a->wf + 1))[2] = be2me_16 (ra4p->flavor);
      ((short *)(sh_a->wf + 1))[3] = be2me_32 (ra4p->coded_frame_size);
      ((short *)(sh_a->wf + 1))[4] = codecdata_length;
      memcpy(((char *)(sh_a->wf + 1)) + 10, src, codecdata_length);

      track->realmedia = 1;
    }
  else if (!strcmp(track->codec_id, MKV_A_FLAC) ||
           (track->a_formattag == 0xf1ac))
    {
      unsigned char *ptr;
      int size;
      free(sh_a->wf);
      sh_a->wf = NULL;

      if (track->a_formattag == mmioFOURCC('f', 'L', 'a', 'C'))
        {
          ptr = (unsigned char *)track->private_data;
          size = track->private_size;
        }
      else
        {
          sh_a->format = mmioFOURCC('f', 'L', 'a', 'C');
          ptr = (unsigned char *) track->private_data
            + sizeof (WAVEFORMATEX);
          size = track->private_size - sizeof (WAVEFORMATEX);
        }
      if (size < 4 || ptr[0] != 'f' || ptr[1] != 'L' ||
          ptr[2] != 'a' || ptr[3] != 'C')
        {
          dp = new_demux_packet (4);
          memcpy (dp->buffer, "fLaC", 4);
        }
      else
        {
          dp = new_demux_packet (size);
          memcpy (dp->buffer, ptr, size);
        }
      dp->pts = 0;
      dp->flags = 0;
      ds_add_packet (demuxer->audio, dp);
    }
  else if (!track->ms_compat || (track->private_size < sizeof(WAVEFORMATEX)))
    {
      free_sh_audio (sh_a);
      return 1;
    }

  return 0;
}

static int
demux_mkv_open_sub (demuxer_t *demuxer, mkv_track_t *track)
{
  if (track->subtitle_type != MATROSKA_SUBTYPE_UNKNOWN)
    {
      if (track->subtitle_type == MATROSKA_SUBTYPE_VOBSUB)
        {
          int m, size = track->private_size;
          uint8_t *buffer;
          m = demux_mkv_decode (track,track->private_data,&buffer,&size,2);
          if (buffer && m)
            {
              free (track->private_data);
              track->private_data = buffer;
            }
        }
    }
  else
    {
      mp_msg (MSGT_DEMUX, MSGL_ERR, "[mkv] Subtitle type '%s' is not "
              "supported.\n", track->codec_id);
      return 1;
    }

  return 0;
}

void demux_mkv_seek (demuxer_t *demuxer, float rel_seek_secs, int flags);

int
demux_mkv_open (demuxer_t *demuxer)
{
  stream_t *s = demuxer->stream;
  mkv_demuxer_t *mkv_d;
  mkv_track_t *track;
  int i, version, cont = 0;
  char *str;

#ifdef USE_ICONV
  subcp_open();
#endif

  stream_seek(s, s->start_pos);
  str = ebml_read_header (s, &version);
  if (str == NULL || strcmp (str, "matroska") || version > 1)
    {
      mp_msg (MSGT_DEMUX, MSGL_DBG2, "[mkv] no head found\n");
      return 0;
    }
  free (str);

  mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] Found the head...\n");

  if (ebml_read_id (s, NULL) != MATROSKA_ID_SEGMENT)
    {
      mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] but no segment :(\n");
      return 0;
    }
  ebml_read_length (s, NULL);  /* return bytes number until EOF */

  mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] + a segment...\n");

  mkv_d = (mkv_demuxer_t *) malloc (sizeof (mkv_demuxer_t));
  memset (mkv_d, 0, sizeof(mkv_demuxer_t));
  demuxer->priv = mkv_d;
  mkv_d->tc_scale = 1000000;
  mkv_d->segment_start = stream_tell (s);
  mkv_d->parsed_cues = (off_t *) malloc (sizeof (off_t));
  mkv_d->parsed_seekhead = (off_t *) malloc (sizeof (off_t));

  for (i=0; i < SUB_MAX_TEXT; i++)
    mkv_d->subs.text[i] = (char *) malloc (256);

  while (!cont)
    {
      switch (ebml_read_id (s, NULL))
        {
        case MATROSKA_ID_INFO:
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |+ segment information...\n");
          cont = demux_mkv_read_info (demuxer);
          break;

        case MATROSKA_ID_TRACKS:
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |+ segment tracks...\n");
          cont = demux_mkv_read_tracks (demuxer);
          break;

        case MATROSKA_ID_CUES:
          cont = demux_mkv_read_cues (demuxer);
          break;

        case MATROSKA_ID_TAGS:
          cont = demux_mkv_read_tags (demuxer);
          break;

        case MATROSKA_ID_SEEKHEAD:
          cont = demux_mkv_read_seekhead (demuxer);
          break;

        case MATROSKA_ID_CHAPTERS:
          cont = demux_mkv_read_chapters (demuxer);
          break;

        case MATROSKA_ID_CLUSTER:
          {
            int p, l;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |+ found cluster, headers are "
                    "parsed completely :)\n");
            /* get the first cluster timecode */
            p = stream_tell(s);
            l = ebml_read_length (s, NULL);
            while (ebml_read_id (s, NULL) != MATROSKA_ID_CLUSTERTIMECODE)
              {
                ebml_read_skip (s, NULL);
                if (stream_tell (s) >= p + l)
                  break;
              }
            if (stream_tell (s) < p + l)
              {
                uint64_t num = ebml_read_uint (s, NULL);
                if (num == EBML_UINT_INVALID)
                  return 0;
                mkv_d->first_tc = num * mkv_d->tc_scale / 1000000.0;
                mkv_d->has_first_tc = 1;
              }
            stream_seek (s, p - 4);
            cont = 1;
            break;
          }

        default:
          cont = 1;
        case MATROSKA_ID_ATTACHMENTS:
        case EBML_ID_VOID:
          ebml_read_skip (s, NULL);
          break;
        }
    }

  display_tracks (mkv_d);

  /* select video track */
  track = NULL;
  if (demuxer->video->id == -1)  /* automatically select a video track */
    {
      /* search for a video track that has the 'default' flag set */
      for (i=0; i<mkv_d->num_tracks; i++)
        if (mkv_d->tracks[i]->type == MATROSKA_TRACK_VIDEO
            && mkv_d->tracks[i]->default_track)
          {
            track = mkv_d->tracks[i];
            break;
          }

      if (track == NULL)
        /* no track has the 'default' flag set */
        /* let's take the first video track */
        for (i=0; i<mkv_d->num_tracks; i++)
          if (mkv_d->tracks[i]->type == MATROSKA_TRACK_VIDEO)
            {
              track = mkv_d->tracks[i];
              break;
            }
    }
  else if (demuxer->video->id != -2)  /* -2 = no video at all */
    track = demux_mkv_find_track_by_num (mkv_d, demuxer->video->id,
                                         MATROSKA_TRACK_VIDEO);

  if (track && !demux_mkv_open_video (demuxer, track))
              {
                mp_msg (MSGT_DEMUX, MSGL_INFO,
                        "[mkv] Will play video track %u\n", track->tnum);
                demuxer->video->id = track->tnum;
                demuxer->video->sh = demuxer->v_streams[track->tnum];
              }
  else
    {
      mp_msg (MSGT_DEMUX, MSGL_INFO, "[mkv] No video track found/wanted.\n");
      demuxer->video->id = -2;
    }

  /* select audio track */
  track = NULL;
  if (demuxer->audio->id == -1)  /* automatically select an audio track */
    {
      /* check if the user specified an audio language */
      if (audio_lang != NULL)
        track = demux_mkv_find_track_by_language(mkv_d, audio_lang,
                                                 MATROSKA_TRACK_AUDIO);
      if (track == NULL)
        /* no audio language specified, or language not found */
        /* search for an audio track that has the 'default' flag set */
        for (i=0; i < mkv_d->num_tracks; i++)
          if (mkv_d->tracks[i]->type == MATROSKA_TRACK_AUDIO
              && mkv_d->tracks[i]->default_track)
            {
              track = mkv_d->tracks[i];
              break;
            }

      if (track == NULL)
        /* no track has the 'default' flag set */
        /* let's take the first audio track */
        for (i=0; i < mkv_d->num_tracks; i++)
          if (mkv_d->tracks[i]->type == MATROSKA_TRACK_AUDIO)
            {
              track = mkv_d->tracks[i];
              break;
            }
    }
  else if (demuxer->audio->id != -2)  /* -2 = no audio at all */
    track = demux_mkv_find_track_by_num (mkv_d, demuxer->audio->id,
                                         MATROSKA_TRACK_AUDIO);

  if (track && !demux_mkv_open_audio (demuxer, track))
              {
                mp_msg (MSGT_DEMUX, MSGL_INFO,
                        "[mkv] Will play audio track %u\n", track->tnum);
                demuxer->audio->id = track->tnum;
                demuxer->audio->sh = demuxer->a_streams[track->tnum];
              }
  else
    {
      mp_msg (MSGT_DEMUX, MSGL_INFO, "[mkv] No audio track found/wanted.\n");
      demuxer->audio->id = -2;
    }

  /* DO NOT automatically select a subtitle track and behave like DVD */
  /* playback: only show subtitles if the user explicitely wants them. */
  track = NULL;
  if (demuxer->sub->id >= 0)
    track = demux_mkv_find_track_by_num (mkv_d, demuxer->sub->id,
                                         MATROSKA_TRACK_SUBTITLE);
  else if (dvdsub_lang != NULL)
    track = demux_mkv_find_track_by_language (mkv_d, dvdsub_lang,
                                              MATROSKA_TRACK_SUBTITLE);

  if (track && !demux_mkv_open_sub (demuxer, track))
          {
            mp_msg (MSGT_DEMUX, MSGL_INFO,
                    "[mkv] Will display subtitle track %u\n", track->tnum);
            demuxer->sub->id = track->tnum;
          }
  else
    demuxer->sub->id = -2;

  if (mkv_d->chapters)
    {
      for (i=0; i < (int)mkv_d->num_chapters; i++)
        {
          mkv_d->chapters[i].start -= mkv_d->first_tc;
          mkv_d->chapters[i].end -= mkv_d->first_tc;
        }
      if (dvd_last_chapter > 0 && dvd_last_chapter <= mkv_d->num_chapters)
        {
          if (mkv_d->chapters[dvd_last_chapter-1].end != 0)
            mkv_d->stop_timecode = mkv_d->chapters[dvd_last_chapter-1].end;
          else if (dvd_last_chapter + 1 <= mkv_d->num_chapters)
            mkv_d->stop_timecode = mkv_d->chapters[dvd_last_chapter].start;
        }
    }

  if (s->end_pos == 0 || (mkv_d->indexes == NULL && index_mode < 0))
    demuxer->seekable = 0;
  else
    {
      demuxer->movi_start = s->start_pos;
      demuxer->movi_end = s->end_pos;
      demuxer->seekable = 1;
      if (mkv_d->chapters && dvd_chapter>1 && dvd_chapter<=mkv_d->num_chapters)
        {
          if (!mkv_d->has_first_tc)
            {
              mkv_d->first_tc = 0;
              mkv_d->has_first_tc = 1;
            }
          demux_mkv_seek (demuxer,
                          mkv_d->chapters[dvd_chapter-1].start/1000.0, 1);
        }
    }

  return 1;
}

void
demux_close_mkv (demuxer_t *demuxer)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;

  if (mkv_d)
    {
      int i;
      if (mkv_d->tracks)
        {
          for (i=0; i<mkv_d->num_tracks; i++)
            {
              if (mkv_d->tracks[i]->codec_id)
                free (mkv_d->tracks[i]->codec_id);
              if (mkv_d->tracks[i]->language)
                free (mkv_d->tracks[i]->language);
              if (mkv_d->tracks[i]->private_data)
                free (mkv_d->tracks[i]->private_data);
            }
          for (i=0; i < SUB_MAX_TEXT; i++)
            if (mkv_d->subs.text[i])
              free (mkv_d->subs.text[i]);
          free (mkv_d->tracks);
        }
      if (mkv_d->indexes)
        free (mkv_d->indexes);
      if (mkv_d->cluster_positions)
        free (mkv_d->cluster_positions);
      if (mkv_d->chapters)
        free (mkv_d->chapters);
      if (mkv_d->parsed_cues)
        free (mkv_d->parsed_cues);
      if (mkv_d->parsed_seekhead)
        free (mkv_d->parsed_seekhead);
      free (mkv_d);
    }
}

static int
demux_mkv_read_block_lacing (uint8_t *buffer, uint64_t *size,
                             uint8_t *laces, uint32_t **all_lace_sizes)
{
  uint32_t total = 0, *lace_size;
  uint8_t flags;
  int i;

  *all_lace_sizes = NULL;
  /* lacing flags */
  flags = *buffer++;
  (*size)--;

  switch ((flags & 0x06) >> 1)
    {
    case 0:  /* no lacing */
      *laces = 1;
      lace_size = (uint32_t *)calloc(*laces, sizeof(uint32_t));
      lace_size[0] = *size;
      break;

    case 1:  /* xiph lacing */
    case 2:  /* fixed-size lacing */
    case 3:  /* EBML lacing */
      *laces = *buffer++;
      (*size)--;
      (*laces)++;
      lace_size = (uint32_t *)calloc(*laces, sizeof(uint32_t));

      switch ((flags & 0x06) >> 1)
        {
        case 1:  /* xiph lacing */
          for (i=0; i < *laces-1; i++)
            {
              lace_size[i] = 0; 
              do
                {
                  lace_size[i] += *buffer;
                  (*size)--;
                } while (*buffer++ == 0xFF);
              total += lace_size[i];
            }
          lace_size[i] = *size - total;
          break;

        case 2:  /* fixed-size lacing */
          for (i=0; i < *laces; i++)
            lace_size[i] = *size / *laces;
          break;

        case 3:  /* EBML lacing */
          {
            int l;
            uint64_t num = ebml_read_vlen_uint (buffer, &l);
            if (num == EBML_UINT_INVALID) {
              free(lace_size);
              return 1;
            }
            buffer += l;
            *size -= l;

            total = lace_size[0] = num;
            for (i=1; i < *laces-1; i++)
              {
                int64_t snum;
                snum = ebml_read_vlen_int (buffer, &l);
                if (snum == EBML_INT_INVALID) {
                  free(lace_size);
                  return 1;
                }
                buffer += l;
                *size -= l;
                lace_size[i] = lace_size[i-1] + snum;
                total += lace_size[i];
              }
            lace_size[i] = *size - total;
            break;
          }
        }
      break;
    }
  *all_lace_sizes = lace_size;
  return 0;
}

static void
handle_subtitles(demuxer_t *demuxer, mkv_track_t *track, char *block,
                 int64_t size, uint64_t block_duration, uint64_t timecode)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  char *ptr1, *ptr2;
  int state, i;

  if (block_duration == 0)
    {
      mp_msg (MSGT_DEMUX, MSGL_WARN, "[mkv] Warning: No BlockDuration "
              "for subtitle track found.\n");
      return;
    }

  ptr1 = block;
  while (ptr1 - block <= size && (*ptr1 == '\n' || *ptr1 == '\r'))
    ptr1++;
  ptr2 = block + size - 1;
  while (ptr2 >= block && (*ptr2 == '\n' || *ptr2 == '\r'))
    {
      *ptr2 = 0;
      ptr2--;
    }

  if (mkv_d->subs.lines > SUB_MAX_TEXT - 2)
    {
      mp_msg (MSGT_DEMUX, MSGL_WARN, "[mkv] Warning: too many sublines "
              "to render, skipping\n");
      return;
    }
  ptr2 = mkv_d->subs.text[mkv_d->subs.lines];
  state = 0;

  if (track->subtitle_type == MATROSKA_SUBTYPE_SSA)
    {
      /* Find text section. */
      for (i=0; i < 8 && *ptr1 != '\0'; ptr1++)
        if (*ptr1 == ',')
          i++;
      if (*ptr1 == '\0')  /* Broken line? */
        return;

      /* Load text. */
      while (ptr1 - block < size)
        {
          if (*ptr1 == '{')
            state = 1;
          else if (*ptr1 == '}' && state == 1)
            state = 2;

          if (state == 0)
            {
              *ptr2++ = *ptr1;
              if (ptr2 - mkv_d->subs.text[mkv_d->subs.lines] >= 255)
                break;
            }
          ptr1++;
      
          /* Newline */
          if (*ptr1 == '\\' && ptr1+1-block < size && (*(ptr1+1)|0x20) == 'n')
            {
              mkv_d->clear_subs_at[mkv_d->subs.lines++]
                = timecode + block_duration;
              *ptr2 = '\0';
              if (mkv_d->subs.lines >= SUB_MAX_TEXT)
                {
                  mp_msg (MSGT_DEMUX, MSGL_WARN, "[mkv] Warning: too many "
                          "sublines to render, skipping\n");
                  return;
                }
              ptr2 = mkv_d->subs.text[mkv_d->subs.lines];
              ptr1 += 2;
            }

          if (state == 2)
            state = 0;
        }
      *ptr2 = '\0';
    }
  else
    {
      while (ptr1 - block != size)
        {
          if (*ptr1 == '\n' || *ptr1 == '\r')
            {
              if (state == 0)  /* normal char --> newline */
                {
                  *ptr2 = '\0';
                  mkv_d->clear_subs_at[mkv_d->subs.lines++]
                    = timecode + block_duration;
                  if (mkv_d->subs.lines >= SUB_MAX_TEXT)
                    {
                      mp_msg (MSGT_DEMUX, MSGL_WARN, "[mkv] Warning: too many "
                              "sublines to render, skipping\n");
                      return;
                    }
                  ptr2 = mkv_d->subs.text[mkv_d->subs.lines];
                  state = 1;
                }
            }
          else if (*ptr1 == '<')  /* skip HTML tags */
            state = 2;
          else if (*ptr1 == '>')
            state = 0;
          else if (state != 2)  /* normal character */
            {
              state = 0;
              if ((ptr2 - mkv_d->subs.text[mkv_d->subs.lines]) < 255)
                *ptr2++ = *ptr1;
            }
          ptr1++;
        }
      *ptr2 = '\0';
    }
  mkv_d->clear_subs_at[mkv_d->subs.lines++] = timecode + block_duration;

#ifdef USE_ICONV
  subcp_recode1 (&mkv_d->subs);
#endif
  vo_sub = &mkv_d->subs;
  vo_osd_changed (OSDTYPE_SUBTITLE);
}

static void
clear_subtitles(demuxer_t *demuxer, uint64_t timecode, int clear_all)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  int i, lines_cut = 0;
  char *tmp;

  /* Clear all? */
  if (clear_all)
    {
      lines_cut = mkv_d->subs.lines;
      mkv_d->subs.lines = 0;
      if (lines_cut)
        {
          vo_sub = &mkv_d->subs;
          vo_osd_changed (OSDTYPE_SUBTITLE);
        }
      return;
    }

  /* Clear the subtitles if they're obsolete now. */
  for (i=0; i < mkv_d->subs.lines; i++)
    {
      if (mkv_d->clear_subs_at[i] <= timecode)
        {
          tmp = mkv_d->subs.text[i];
          memmove (mkv_d->subs.text+i, mkv_d->subs.text+i+1,
                   (mkv_d->subs.lines-i-1) * sizeof (*mkv_d->subs.text));
          memmove (mkv_d->clear_subs_at+i, mkv_d->clear_subs_at+i+1,
                   (mkv_d->subs.lines-i-1) * sizeof (*mkv_d->clear_subs_at));
          mkv_d->subs.text[--mkv_d->subs.lines] = tmp;
          i--;
          lines_cut = 1;
        }
    }
  if (lines_cut)
    {
      vo_sub = &mkv_d->subs;
      vo_osd_changed (OSDTYPE_SUBTITLE);
    }
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

static void
handle_realvideo (demuxer_t *demuxer, mkv_track_t *track, uint8_t *buffer,
                  uint32_t size, int block_bref)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  demux_packet_t *dp;
  dp_hdr_t *hdr;
  uint8_t chunks;
  int isize;

  chunks = *buffer++;
  isize = --size - (chunks+1)*8;
  dp = new_demux_packet (sizeof (*hdr) + size);
  memcpy (dp->buffer + sizeof(*hdr), buffer + (chunks+1)*8, isize);
  memcpy (dp->buffer + sizeof(*hdr) + isize, buffer, (chunks+1)*8);

  hdr = (dp_hdr_t *) dp->buffer;
  hdr->len = isize;
  hdr->chunks = chunks;
  hdr->timestamp = mkv_d->last_pts * 1000;
  hdr->chunktab = sizeof(*hdr) + isize;

  dp->len = sizeof(*hdr) + size;
  if (mkv_d->v_skip_to_keyframe)
    {
      dp->pts = mkv_d->last_pts;
      track->rv_kf_base = 0;
      track->rv_kf_pts = hdr->timestamp;
    }
  else
    dp->pts = real_fix_timestamp (track, dp->buffer + sizeof(*hdr),
                                  hdr->timestamp);
  dp->pos = demuxer->filepos;
  dp->flags = block_bref ? 0 : 0x10;

  ds_add_packet(demuxer->video, dp);
}

static void
handle_realaudio (demuxer_t *demuxer, mkv_track_t *track, uint8_t *buffer,
                  uint32_t size, int block_bref)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  demux_packet_t *dp = new_demux_packet (size);

  memcpy (dp->buffer, buffer, size);
  if (track->ra_pts == mkv_d->last_pts && !mkv_d->a_skip_to_keyframe)
    dp->pts = 0;
  else
    dp->pts = mkv_d->last_pts;
  track->ra_pts = mkv_d->last_pts;

  dp->pos = demuxer->filepos;
  dp->flags = block_bref ? 0 : 0x10;
  ds_add_packet (demuxer->audio, dp);
}

static int
handle_block (demuxer_t *demuxer, uint8_t *block, uint64_t length,
              uint64_t block_duration, int64_t block_bref)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  mkv_track_t *track = NULL;
  demux_stream_t *ds = NULL;
  uint64_t old_length;
  int64_t tc;
  uint32_t *lace_size;
  uint8_t laces;
  int i, num, tmp, use_this_block = 1;
  float current_pts;
  int16_t time;

  /* first byte(s): track num */
  num = ebml_read_vlen_uint (block, &tmp);
  block += tmp;
  /* time (relative to cluster time) */
  time = be2me_16 (* (int16_t *) block);
  block += 2;
  length -= tmp + 2;
  old_length = length;
  if (demux_mkv_read_block_lacing (block, &length, &laces, &lace_size))
    return 0;
  block += old_length - length;

  tc = ((time*mkv_d->tc_scale+mkv_d->cluster_tc) /1000000.0 - mkv_d->first_tc);
  if (tc < 0)
    tc = 0;
  if (mkv_d->stop_timecode > 0 && tc > mkv_d->stop_timecode) {
    free(lace_size);
    return -1;
  }
  current_pts = tc / 1000.0;

  clear_subtitles(demuxer, tc, 0);

  for (i=0; i<mkv_d->num_tracks; i++)
    if (mkv_d->tracks[i]->tnum == num)
      track = mkv_d->tracks[i];
  if (num == demuxer->audio->id)
    {
      ds = demuxer->audio;

      if (mkv_d->a_skip_to_keyframe && block_bref != 0)
        use_this_block = 0;
      else if (mkv_d->v_skip_to_keyframe)
        use_this_block = 0;

      if (track->fix_i_bps && use_this_block)
        {
          sh_audio_t *sh = (sh_audio_t *) ds->sh;

          if (block_duration != 0)
            {
              sh->i_bps = length * 1000 / block_duration;
              track->fix_i_bps = 0;
            }
          else if (track->qt_last_a_pts == 0.0)
            track->qt_last_a_pts = current_pts;
          else if(track->qt_last_a_pts != current_pts)
            {
              sh->i_bps = length / (current_pts - track->qt_last_a_pts);
              track->fix_i_bps = 0;
            }
        }
    }
  else if (tc < mkv_d->skip_to_timecode)
    use_this_block = 0;
  else if (num == demuxer->video->id)
    {
      ds = demuxer->video;
      if (mkv_d->v_skip_to_keyframe && block_bref != 0)
        use_this_block = 0;
    }
  else if (num == demuxer->sub->id)
    {
      ds = demuxer->sub;
      if (track->subtitle_type != MATROSKA_SUBTYPE_VOBSUB)
        {
          if (!mkv_d->v_skip_to_keyframe)
            handle_subtitles (demuxer, track, block, length,
                              block_duration, tc);
          use_this_block = 0;
        }
    }
  else
    use_this_block = 0;

  if (use_this_block)
    {
      mkv_d->last_pts = ds->pts = current_pts;
      mkv_d->last_filepos = demuxer->filepos;

      for (i=0; i < laces; i++)
        {
          if (ds == demuxer->video && track->realmedia)
            handle_realvideo (demuxer, track, block, lace_size[i], block_bref);
          else if (ds == demuxer->audio && track->realmedia)
            handle_realaudio (demuxer, track, block, lace_size[i], block_bref);
          else
            {
              int modified, size = lace_size[i];
              demux_packet_t *dp;
              uint8_t *buffer;
              modified = demux_mkv_decode (track, block, &buffer, &size, 1);
              if (buffer)
                {
                  dp = new_demux_packet (size);
                  memcpy (dp->buffer, buffer, size);
                  if (modified)
                    free (buffer);
                  dp->flags = block_bref == 0 ? 1 : 0;
                  dp->pts = mkv_d->last_pts + i * track->default_duration;
                  ds_add_packet (ds, dp);
                }
            }
          block += lace_size[i];
        }

      if (ds == demuxer->video)
        {
          mkv_d->v_skip_to_keyframe = 0;
          mkv_d->skip_to_timecode = 0;
        }
      else if (ds == demuxer->audio)
        mkv_d->a_skip_to_keyframe = 0;

      free(lace_size);
      return 1;
    }

  free(lace_size);
  return 0;
}

int
demux_mkv_fill_buffer (demuxer_t *demuxer)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  stream_t *s = demuxer->stream;
  uint64_t l;
  int il, tmp;

  while (1)
    {
      while (mkv_d->cluster_size > 0)
        {
          uint64_t block_duration = 0,  block_length = 0;
          int64_t block_bref = 0;
          uint8_t *block = NULL;

          while (mkv_d->blockgroup_size > 0)
            {
              switch (ebml_read_id (s, &il))
                {
                case MATROSKA_ID_BLOCKDURATION:
                  {
                    block_duration = ebml_read_uint (s, &l);
                    if (block_duration == EBML_UINT_INVALID)
                      return 0;
                    break;
                  }

                case MATROSKA_ID_BLOCK:
                  block_length = ebml_read_length (s, &tmp);
                  block = (uint8_t *) malloc (block_length);
                  demuxer->filepos = stream_tell (s);
                  if (stream_read (s,block,block_length) != (int) block_length)
                    return 0;
                  l = tmp + block_length;
                  break;

                case MATROSKA_ID_REFERENCEBLOCK:
                  {
                    int64_t num = ebml_read_int (s, &l);
                    if (num == EBML_INT_INVALID)
                      return 0;
                    if (num < 0)
                      block_bref = num;
                    break;
                  }

                case EBML_ID_INVALID:
                  return 0;

                default:
                  ebml_read_skip (s, &l);
                  break;
                }
              mkv_d->blockgroup_size -= l + il;
              mkv_d->cluster_size -= l + il;
            }

          if (block)
            {
              int res = handle_block (demuxer, block, block_length,
                                      block_duration, block_bref);
              free (block);
              if (res < 0)
                return 0;
              if (res)
                return 1;
            }

          if (mkv_d->cluster_size > 0)
            {
              switch (ebml_read_id (s, &il))
                {
                case MATROSKA_ID_CLUSTERTIMECODE:
                  {
                    uint64_t num = ebml_read_uint (s, &l);
                    if (num == EBML_UINT_INVALID)
                      return 0;
                    if (!mkv_d->has_first_tc)
                      {
                        mkv_d->first_tc = num * mkv_d->tc_scale / 1000000.0;
                        mkv_d->has_first_tc = 1;
                      }
                    mkv_d->cluster_tc = num * mkv_d->tc_scale;
                    break;
                  }

                case MATROSKA_ID_BLOCKGROUP:
                  mkv_d->blockgroup_size = ebml_read_length (s, &tmp);
                  l = tmp;
                  break;

                case EBML_ID_INVALID:
                  return 0;

                default:
                  ebml_read_skip (s, &l);
                  break;
                }
              mkv_d->cluster_size -= l + il;
            }
        }

      if (ebml_read_id (s, &il) != MATROSKA_ID_CLUSTER)
        return 0;
      add_cluster_position(mkv_d, stream_tell(s)-il);
      mkv_d->cluster_size = ebml_read_length (s, NULL);
    }

  return 0;
}

void
demux_mkv_seek (demuxer_t *demuxer, float rel_seek_secs, int flags)
{
  if (!(flags & 2))  /* time in secs */
    {
      void resync_audio_stream(sh_audio_t *sh_audio);
      mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
      stream_t *s = demuxer->stream;
      int64_t target_timecode = 0, diff, min_diff=0xFFFFFFFL;
      int i;

      if (!(flags & 1))  /* relative seek */
        target_timecode = (int64_t) (mkv_d->last_pts * 1000.0);
      target_timecode += (int64_t)(rel_seek_secs * 1000.0);
      if (target_timecode < 0)
        target_timecode = 0;

      if (mkv_d->indexes == NULL)  /* no index was found */
        {
          uint64_t target_filepos, cluster_pos, max_pos;

          target_filepos = (uint64_t) (target_timecode * mkv_d->last_filepos
                                       / (mkv_d->last_pts * 1000.0));

          max_pos = mkv_d->cluster_positions[mkv_d->num_cluster_pos-1];
          if (target_filepos > max_pos)
            {
              if ((off_t) max_pos > stream_tell (s))
                stream_seek (s, max_pos);
              else
                stream_seek (s, stream_tell (s) + mkv_d->cluster_size);
              /* parse all the clusters upto target_filepos */
              while (!s->eof && stream_tell(s) < (off_t) target_filepos)
                {
                  switch (ebml_read_id (s, &i))
                    {
                    case MATROSKA_ID_CLUSTER:
                      add_cluster_position(mkv_d, (uint64_t) stream_tell(s)-i);
                      break;

                    case MATROSKA_ID_CUES:
                      demux_mkv_read_cues (demuxer);
                      break;
                    }
                  ebml_read_skip (s, NULL);
                }
              if (s->eof)
                stream_reset(s);
            }

          if (mkv_d->indexes == NULL)
            {
              cluster_pos = mkv_d->cluster_positions[0];
              /* Let's find the nearest cluster */
              for (i=0; i < mkv_d->num_cluster_pos; i++)
                {
                  diff = mkv_d->cluster_positions[i] - target_filepos;
                  if (rel_seek_secs < 0 && diff < 0 && -diff < min_diff)
                    {
                      cluster_pos = mkv_d->cluster_positions[i];
                      min_diff = -diff;
                    }
                  else if (rel_seek_secs > 0
                           && (diff < 0 ? -1 * diff : diff) < min_diff)
                    {
                      cluster_pos = mkv_d->cluster_positions[i];
                      min_diff = diff < 0 ? -1 * diff : diff;
                    }
                }
              mkv_d->cluster_size = mkv_d->blockgroup_size = 0;
              stream_seek (s, cluster_pos);
            }
        }
      else
        {
          mkv_index_t *index = NULL;

          /* let's find the entry in the indexes with the smallest */
          /* difference to the wanted timecode. */
          for (i=0; i < mkv_d->num_indexes; i++)
            if (mkv_d->indexes[i].tnum == demuxer->video->id)
              {
                diff = target_timecode - (int64_t) mkv_d->indexes[i].timecode;

                if ((flags & 1 || target_timecode <= mkv_d->last_pts*1000)
                    && diff >= 0 && diff < min_diff)
                  {
                    min_diff = diff;
                    index = mkv_d->indexes + i;
                  }
                else if (target_timecode > mkv_d->last_pts*1000
                         && diff < 0 && -diff < min_diff)
                  {
                    min_diff = -diff;
                    index = mkv_d->indexes + i;
                  }
              }

          if (index)  /* We've found an entry. */
            {
              mkv_d->cluster_size = mkv_d->blockgroup_size = 0;
              stream_seek (s, index->filepos);
            }
        }

      if (demuxer->video->id >= 0)
        mkv_d->v_skip_to_keyframe = 1;
      if (rel_seek_secs > 0.0)
        mkv_d->skip_to_timecode = target_timecode;
      mkv_d->a_skip_to_keyframe = 1;

      /* Clear subtitles. */
      if (target_timecode <= mkv_d->last_pts * 1000)
        clear_subtitles(demuxer, 0, 1);

      demux_mkv_fill_buffer(demuxer);

      if(demuxer->audio->sh != NULL)
        resync_audio_stream((sh_audio_t *) demuxer->audio->sh); 
    }
  else if ((demuxer->movi_end <= 0) || !(flags & 1))
    mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] seek unsupported flags\n");
  else
    {
      void resync_audio_stream(sh_audio_t *sh_audio);
      mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
      stream_t *s = demuxer->stream;
      uint64_t target_filepos;
      mkv_index_t *index = NULL;
      int i;

      if (mkv_d->indexes == NULL)  /* no index was found */
        {                       /* I'm lazy... */
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] seek unsupported flags\n");
          return;
        }

      target_filepos = (uint64_t)(demuxer->movi_end * rel_seek_secs);
      for (i=0; i < mkv_d->num_indexes; i++)
        if (mkv_d->indexes[i].tnum == demuxer->video->id)
          if ((index == NULL) ||
              ((mkv_d->indexes[i].filepos >= target_filepos) &&
               ((index->filepos < target_filepos) ||
                (mkv_d->indexes[i].filepos < index->filepos))))
            index = &mkv_d->indexes[i];

      if (!index)
        return;

      mkv_d->cluster_size = mkv_d->blockgroup_size = 0;
      stream_seek (s, index->filepos);

      if (demuxer->video->id >= 0)
        mkv_d->v_skip_to_keyframe = 1;
      mkv_d->skip_to_timecode = index->timecode;
      mkv_d->a_skip_to_keyframe = 1;

      /* Clear subtitles. */
      if (index->timecode <= mkv_d->last_pts * 1000)
        clear_subtitles(demuxer, 0, 1);

      demux_mkv_fill_buffer(demuxer);

      if(demuxer->audio->sh != NULL)
        resync_audio_stream((sh_audio_t *) demuxer->audio->sh); 
    }
}

int
demux_mkv_control (demuxer_t *demuxer, int cmd, void *arg)
{
  mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
  
  switch (cmd)
    {
    case DEMUXER_CTRL_GET_TIME_LENGTH:
      if (mkv_d->duration == 0)
        return DEMUXER_CTRL_DONTKNOW;

      *((unsigned long *)arg) = (unsigned long)mkv_d->duration;
      return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_GET_PERCENT_POS:
      if (mkv_d->duration == 0)
        {
          if (demuxer->movi_start == demuxer->movi_end)
            return DEMUXER_CTRL_DONTKNOW;

          *((int *)arg) = (int)((demuxer->filepos - demuxer->movi_start) /
                                ((demuxer->movi_end-demuxer->movi_start)/100));
          return DEMUXER_CTRL_OK;
        }

      *((int *) arg) = (int) (100 * mkv_d->last_pts / mkv_d->duration);
      return DEMUXER_CTRL_OK; 

    default:
      return DEMUXER_CTRL_NOTIMPL;
    }
}

#endif /* HAVE_MATROSKA */
