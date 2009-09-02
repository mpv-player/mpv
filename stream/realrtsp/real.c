/*
 * This file was ported to MPlayer from xine CVS real.c,v 1.8 2003/03/30 17:11:50
 */

/*
 * Copyright (C) 2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * special functions for real streams.
 * adopted from joschkas real tools.
 *
 */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "libavutil/common.h"
#include "real.h"
#include "asmrp.h"
#include "sdpplin.h"
#include "xbuffer.h"
#include "libavutil/md5.h"
#include "libavutil/intreadwrite.h"
#include "stream/http.h"
#include "mp_msg.h"

/*
#define LOG
*/

#define XOR_TABLE_SIZE 37

static const unsigned char xor_table[XOR_TABLE_SIZE] = {
    0x05, 0x18, 0x74, 0xd0, 0x0d, 0x09, 0x02, 0x53,
    0xc0, 0x01, 0x05, 0x05, 0x67, 0x03, 0x19, 0x70,
    0x08, 0x27, 0x66, 0x10, 0x10, 0x72, 0x08, 0x09,
    0x63, 0x11, 0x03, 0x71, 0x08, 0x08, 0x70, 0x02,
    0x10, 0x57, 0x05, 0x18, 0x54 };


#define BUF_SIZE 4096

#ifdef LOG
static void hexdump (const char *buf, int length) {

  int i;

  printf (" hexdump> ");
  for (i = 0; i < length; i++) {
    unsigned char c = buf[i];

    printf ("%02x", c);

    if ((i % 16) == 15)
      printf ("\n         ");

    if ((i % 2) == 1)
      printf (" ");

  }
  printf ("\n");
}
#endif


static void real_calc_response_and_checksum (char *response, char *chksum, char *challenge) {

  int   ch_len;
  int   i;
  unsigned char zres[16], buf[64];

  /* initialize buffer */
  AV_WB32(buf, 0xa1e9149d);
  AV_WB32(buf+4, 0x0e6b3b59);

  /* some (length) checks */
  if (challenge != NULL)
  {
    ch_len = strlen (challenge);

    if (ch_len == 40) /* what a hack... */
      ch_len=32;
    if ( ch_len > 56 ) ch_len=56;

    /* copy challenge to buf */
    memcpy(buf+8, challenge, ch_len);
    memset(buf+8+ch_len, 0, 56-ch_len);
  }

    /* xor challenge bytewise with xor_table */
    for (i=0; i<XOR_TABLE_SIZE; i++)
      buf[8+i] ^= xor_table[i];

  av_md5_sum(zres, buf, 64);

  /* convert zres to ascii string */
  for (i=0; i<16; i++ )
    sprintf(response+i*2, "%02x", zres[i]);

  /* add tail */
  strcpy (&response[32], "01d0a8e3");

  /* calculate checksum */
  for (i=0; i<8; i++)
    chksum[i] = response[i*4];
  chksum[8] = 0;
}


/*
 * takes a MLTI-Chunk and a rule number got from match_asm_rule,
 * returns a pointer to selected data and number of bytes in that.
 */

static int select_mlti_data(const char *mlti_chunk, int mlti_size, int selection, char **out) {

  int numrules, codec, size;
  int i;

  /* MLTI chunk should begin with MLTI */

  if ((mlti_chunk[0] != 'M')
      ||(mlti_chunk[1] != 'L')
      ||(mlti_chunk[2] != 'T')
      ||(mlti_chunk[3] != 'I'))
  {
#ifdef LOG
    printf("libreal: MLTI tag not detected, copying data\n");
#endif
    *out = xbuffer_copyin(*out, 0, mlti_chunk, mlti_size);
    return mlti_size;
  }

  mlti_chunk+=4;

  /* next 16 bits are the number of rules */
  numrules=AV_RB16(mlti_chunk);
  if (selection >= numrules) return 0;

  /* now <numrules> indices of codecs follows */
  /* we skip to selection                     */
  mlti_chunk+=(selection+1)*2;

  /* get our index */
  codec=AV_RB16(mlti_chunk);

  /* skip to number of codecs */
  mlti_chunk+=(numrules-selection)*2;

  /* get number of codecs */
  numrules=AV_RB16(mlti_chunk);

  if (codec >= numrules) {
    mp_msg(MSGT_STREAM, MSGL_WARN, "realrtsp: codec index >= number of codecs. %i %i\n",
      codec, numrules);
    return 0;
  }

  mlti_chunk+=2;

  /* now seek to selected codec */
  for (i=0; i<codec; i++) {
    size=AV_RB32(mlti_chunk);
    mlti_chunk+=size+4;
  }

  size=AV_RB32(mlti_chunk);

#ifdef LOG
  hexdump(mlti_chunk+4, size);
#endif
  *out = xbuffer_copyin(*out, 0, mlti_chunk+4, size);
  return size;
}

/*
 * looking at stream description.
 */

static rmff_header_t *real_parse_sdp(char *data, char **stream_rules, uint32_t bandwidth) {

  sdpplin_t *desc;
  rmff_header_t *header;
  char *buf;
  int len, i;
  int max_bit_rate=0;
  int avg_bit_rate=0;
  int max_packet_size=0;
  int avg_packet_size=0;
  int duration=0;


  if (!data) return NULL;

  desc=sdpplin_parse(data);

  if (!desc) return NULL;

  buf = xbuffer_init(2048);
  header=calloc(1,sizeof(rmff_header_t));

  header->fileheader=rmff_new_fileheader(4+desc->stream_count);
  header->cont=rmff_new_cont(
      desc->title,
      desc->author,
      desc->copyright,
      desc->abstract);
  header->data=rmff_new_dataheader(0,0);
  header->streams=calloc(1,sizeof(rmff_mdpr_t*)*(desc->stream_count+1));
#ifdef LOG
    printf("number of streams: %u\n", desc->stream_count);
#endif

  for (i=0; i<desc->stream_count; i++) {

    int j=0;
    int n;
    char b[64];
    int rulematches[MAX_RULEMATCHES];

    if (!desc->stream[i])
      continue;
#ifdef LOG
    printf("calling asmrp_match with:\n%s\n%u\n", desc->stream[i]->asm_rule_book, bandwidth);
#endif
    n=asmrp_match(desc->stream[i]->asm_rule_book, bandwidth, rulematches);
    for (j=0; j<n; j++) {
#ifdef LOG
      printf("asmrp rule match: %u for stream %u\n", rulematches[j], desc->stream[i]->stream_id);
#endif
      sprintf(b,"stream=%u;rule=%u,", desc->stream[i]->stream_id, rulematches[j]);
      *stream_rules = xbuffer_strcat(*stream_rules, b);
    }

    if (!desc->stream[i]->mlti_data) {
	len = 0;
	buf = xbuffer_free(buf);
    } else
    len=select_mlti_data(desc->stream[i]->mlti_data, desc->stream[i]->mlti_data_size, rulematches[0], &buf);

    header->streams[i]=rmff_new_mdpr(
	desc->stream[i]->stream_id,
        desc->stream[i]->max_bit_rate,
        desc->stream[i]->avg_bit_rate,
        desc->stream[i]->max_packet_size,
        desc->stream[i]->avg_packet_size,
        desc->stream[i]->start_time,
        desc->stream[i]->preroll,
        desc->stream[i]->duration,
        desc->stream[i]->stream_name,
        desc->stream[i]->mime_type,
	len,
	buf);

    duration=FFMAX(duration,desc->stream[i]->duration);
    max_bit_rate+=desc->stream[i]->max_bit_rate;
    avg_bit_rate+=desc->stream[i]->avg_bit_rate;
    max_packet_size=FFMAX(max_packet_size, desc->stream[i]->max_packet_size);
    if (avg_packet_size)
      avg_packet_size=(avg_packet_size + desc->stream[i]->avg_packet_size) / 2;
    else
      avg_packet_size=desc->stream[i]->avg_packet_size;
  }

  if (*stream_rules && strlen(*stream_rules) && (*stream_rules)[strlen(*stream_rules)-1] == ',')
    (*stream_rules)[strlen(*stream_rules)-1]=0; /* delete last ',' in stream_rules */

  header->prop=rmff_new_prop(
      max_bit_rate,
      avg_bit_rate,
      max_packet_size,
      avg_packet_size,
      0,
      duration,
      0,
      0,
      0,
      desc->stream_count,
      desc->flags);

  rmff_fix_header(header);
  buf = xbuffer_free(buf);
  sdpplin_free(desc);

  return header;
}

int real_get_rdt_chunk(rtsp_t *rtsp_session, char **buffer, int rdt_rawdata) {

  int n=1;
  uint8_t header[8];
  rmff_pheader_t ph;
  int size;
  int flags1, flags2;
  int unknown1;
  uint32_t ts;
  static uint32_t prev_ts = -1;
  static int prev_stream_number = -1;

  n=rtsp_read_data(rtsp_session, header, 8);
  if (n<8) return 0;
  if (header[0] != 0x24)
  {
    mp_msg(MSGT_STREAM, MSGL_WARN, "realrtsp: rdt chunk not recognized: got 0x%02x\n",
      header[0]);
    return 0;
  }
  /* header[1] is channel, normally 0, ignored */
  size=(header[2]<<8)+header[3];
  flags1=header[4];
  if ((flags1 & 0xc0) != 0x40)
  {
#ifdef LOG
    printf("got flags1: 0x%02x\n",flags1);
#endif
    if(header[6] == 0x06) { // eof packet
      rtsp_read_data(rtsp_session, header, 7); // Skip the rest of the eof packet
      /* Some files have short auxiliary streams, we must ignore eof packets
       * for these streams to avoid premature eof.
       * Now the code declares eof only if the stream with id == 0 gets eof
       * (old code was: eof on the first eof packet received).
       */
      if(flags1 & 0x7c) // ignore eof for streams with id != 0
        return 0;
      mp_msg(MSGT_STREAM, MSGL_INFO, "realrtsp: Stream EOF detected\n");
      return -1;
    }
    header[0]=header[5];
    header[1]=header[6];
    header[2]=header[7];
    n=rtsp_read_data(rtsp_session, header+3, 5);
    if (n<5) return 0;
#ifdef LOG
    printf("ignoring bytes:\n");
    hexdump(header, 8);
#endif
    n=rtsp_read_data(rtsp_session, header+4, 4);
    if (n<4) return 0;
    flags1=header[4];
    size-=9;
  }
  flags2=header[7];
  // header[5..6] == frame number in stream
  unknown1=(header[5]<<16)+(header[6]<<8)+(header[7]);
  n=rtsp_read_data(rtsp_session, header, 6);
  if (n<6) return 0;
  ts=AV_RB32(header);

#ifdef LOG
  printf("ts: %u, size: %u, flags: 0x%02x, unknown values: 0x%06x 0x%02x 0x%02x\n",
          ts, size, flags1, unknown1, header[4], header[5]);
#endif
  size+=2;

  ph.object_version=0;
  ph.length=size;
  ph.stream_number=(flags1>>1)&0x1f;
  ph.timestamp=ts;
  ph.reserved=0;
  if ((flags2&1) == 0 && (prev_ts != ts || prev_stream_number != ph.stream_number))
  {
    prev_ts = ts;
    prev_stream_number = ph.stream_number;
    ph.flags=2;
  }
  else
    ph.flags=0;
  *buffer = xbuffer_ensure_size(*buffer, 12+size);
  if(rdt_rawdata) {
      if (size < 12)
          return 0;
    n=rtsp_read_data(rtsp_session, *buffer, size-12);
    return (n <= 0) ? 0 : n;
  }
  rmff_dump_pheader(&ph, *buffer);
  if (size < 12)
      return 0;
  size-=12;
  n=rtsp_read_data(rtsp_session, (*buffer)+12, size);

  return (n <= 0) ? 0 : n+12;
}

static int convert_timestamp(char *str, int *sec, int *msec) {
  int hh, mm, ss, ms = 0;

  // Timestamp may be optionally quoted with ", skip it
  // Since the url is escaped when we get here, we skip the string "%22"
  if (!strncmp(str, "%22", 3))
    str += 3;
  if (sscanf(str, "%d:%d:%d.%d", &hh, &mm, &ss, &ms) < 3) {
    hh = 0;
    if (sscanf(str, "%d:%d.%d", &mm, &ss, &ms) < 2) {
      mm = 0;
      if (sscanf(str, "%d.%d", &ss, &ms) < 1) {
	ss = 0;
	ms = 0;
      }
    }
  }
  if (sec)
    *sec = hh * 3600 + mm * 60 + ss;
  if (msec)
    *msec = ms;
  return 1;
}

//! maximum size of the rtsp description, must be < INT_MAX
#define MAX_DESC_BUF (20 * 1024 * 1024)
rmff_header_t *real_setup_and_get_header(rtsp_t *rtsp_session, uint32_t bandwidth,
  char *username, char *password) {

  char *description=NULL;
  char *session_id=NULL;
  rmff_header_t *h = NULL;
  char *challenge1 = NULL;
  char challenge2[41];
  char checksum[9];
  char *subscribe = NULL;
  char *buf = xbuffer_init(256);
  char *mrl=rtsp_get_mrl(rtsp_session);
  unsigned int size;
  int status;
  uint32_t maxbandwidth = bandwidth;
  char* authfield = NULL;
  int i;

  /* get challenge */
  challenge1=rtsp_search_answers(rtsp_session,"RealChallenge1");
  if (!challenge1)
      goto out;
  challenge1=strdup(challenge1);
#ifdef LOG
  printf("real: Challenge1: %s\n", challenge1);
#endif

  /* set a reasonable default to get the best stream, unless bandwidth given */
  if (!bandwidth)
      bandwidth = 10485800;

  /* request stream description */
rtsp_send_describe:
  rtsp_schedule_field(rtsp_session, "Accept: application/sdp");
  sprintf(buf, "Bandwidth: %u", bandwidth);
  rtsp_schedule_field(rtsp_session, buf);
  rtsp_schedule_field(rtsp_session, "GUID: 00000000-0000-0000-0000-000000000000");
  rtsp_schedule_field(rtsp_session, "RegionData: 0");
  rtsp_schedule_field(rtsp_session, "ClientID: Linux_2.4_6.0.9.1235_play32_RN01_EN_586");
  rtsp_schedule_field(rtsp_session, "SupportsMaximumASMBandwidth: 1");
  rtsp_schedule_field(rtsp_session, "Language: en-US");
  rtsp_schedule_field(rtsp_session, "Require: com.real.retain-entity-for-setup");
  if(authfield)
    rtsp_schedule_field(rtsp_session, authfield);
  status=rtsp_request_describe(rtsp_session,NULL);

  if (status == 401) {
    int authlen, b64_authlen;
    char *authreq;
    char* authstr = NULL;

    if (authfield) {
      mp_msg(MSGT_STREAM, MSGL_ERR, "realrtsp: authorization failed, check your credentials\n");
      goto autherr;
    }
    if (!(authreq = rtsp_search_answers(rtsp_session,"WWW-Authenticate"))) {
      mp_msg(MSGT_STREAM, MSGL_ERR, "realrtsp: 401 but no auth request, aborting\n");
      goto autherr;
    }
    if (!username) {
      mp_msg(MSGT_STREAM, MSGL_ERR, "realrtsp: auth required but no username supplied\n");
      goto autherr;
    }
    if (!strstr(authreq, "Basic")) {
      mp_msg(MSGT_STREAM, MSGL_ERR, "realrtsp: authenticator not supported (%s)\n", authreq);
      goto autherr;
    }
    authlen = strlen(username) + (password ? strlen(password) : 0) + 2;
    authstr = malloc(authlen);
    sprintf(authstr, "%s:%s", username, password ? password : "");
    authfield = malloc(authlen*2+22);
    strcpy(authfield, "Authorization: Basic ");
    b64_authlen = base64_encode(authstr, authlen, authfield+21, authlen*2);
    free(authstr);
    if (b64_authlen < 0) {
      mp_msg(MSGT_STREAM, MSGL_ERR, "realrtsp: base64 output overflow, this should never happen\n");
      goto autherr;
    }
    authfield[b64_authlen+21] = 0;
    goto rtsp_send_describe;
  }
autherr:

  if (authfield)
     free(authfield);

  if ( status<200 || status>299 )
  {
    char *alert=rtsp_search_answers(rtsp_session,"Alert");
    if (alert) {
      mp_msg(MSGT_STREAM, MSGL_WARN, "realrtsp: got message from server:\n%s\n",
        alert);
    }
    rtsp_send_ok(rtsp_session);
    goto out;
  }

  /* receive description */
  size=0;
  if (!rtsp_search_answers(rtsp_session,"Content-length"))
    mp_msg(MSGT_STREAM, MSGL_WARN, "real: got no Content-length!\n");
  else
    size=atoi(rtsp_search_answers(rtsp_session,"Content-length"));

  // as size is unsigned this also catches the case (size < 0)
  if (size > MAX_DESC_BUF) {
    mp_msg(MSGT_STREAM, MSGL_ERR, "realrtsp: Content-length for description too big (> %uMB)!\n",
            MAX_DESC_BUF/(1024*1024) );
    goto out;
  }

  if (!rtsp_search_answers(rtsp_session,"ETag"))
    mp_msg(MSGT_STREAM, MSGL_WARN, "realrtsp: got no ETag!\n");
  else
    session_id=strdup(rtsp_search_answers(rtsp_session,"ETag"));

#ifdef LOG
  printf("real: Stream description size: %u\n", size);
#endif

  description=malloc(size+1);

  if( rtsp_read_data(rtsp_session, description, size) <= 0) {
    goto out;
  }
  description[size]=0;

  /* parse sdp (sdpplin) and create a header and a subscribe string */
  subscribe = xbuffer_init(256);
  strcpy(subscribe, "Subscribe: ");
  h=real_parse_sdp(description, &subscribe, bandwidth);
  if (!h) {
    goto out;
  }
  rmff_fix_header(h);

#ifdef LOG
  printf("Title: %s\nCopyright: %s\nAuthor: %s\nStreams: %i\n",
    h->cont->title, h->cont->copyright, h->cont->author, h->prop->num_streams);
#endif

  /* setup our streams */
  real_calc_response_and_checksum (challenge2, checksum, challenge1);
  buf = xbuffer_ensure_size(buf, strlen(challenge2) + strlen(checksum) + 32);
  sprintf(buf, "RealChallenge2: %s, sd=%s", challenge2, checksum);
  rtsp_schedule_field(rtsp_session, buf);
  buf = xbuffer_ensure_size(buf, strlen(session_id) + 32);
  sprintf(buf, "If-Match: %s", session_id);
  rtsp_schedule_field(rtsp_session, buf);
  rtsp_schedule_field(rtsp_session, "Transport: x-pn-tng/tcp;mode=play,rtp/avp/tcp;unicast;mode=play");
  buf = xbuffer_ensure_size(buf, strlen(mrl) + 32);
  sprintf(buf, "%s/streamid=0", mrl);
  rtsp_request_setup(rtsp_session,buf,NULL);

  /* Do setup for all the other streams we subscribed to */
  for (i = 1; i < h->prop->num_streams; i++) {
    rtsp_schedule_field(rtsp_session, "Transport: x-pn-tng/tcp;mode=play,rtp/avp/tcp;unicast;mode=play");
    buf = xbuffer_ensure_size(buf, strlen(session_id) + 32);
    sprintf(buf, "If-Match: %s", session_id);
    rtsp_schedule_field(rtsp_session, buf);

    buf = xbuffer_ensure_size(buf, strlen(mrl) + 32);
    sprintf(buf, "%s/streamid=%d", mrl, i);
    rtsp_request_setup(rtsp_session,buf,NULL);
  }
  /* set stream parameter (bandwidth) with our subscribe string */
  rtsp_schedule_field(rtsp_session, subscribe);
  rtsp_request_setparameter(rtsp_session,NULL);

  /* set delivery bandwidth */
  if (maxbandwidth) {
      sprintf(buf, "SetDeliveryBandwidth: Bandwidth=%u;BackOff=0", maxbandwidth);
      rtsp_schedule_field(rtsp_session, buf);
      rtsp_request_setparameter(rtsp_session,NULL);
  }

  {
    int s_ss = 0, s_ms = 0, e_ss = 0, e_ms = 0;
    char *str;
    if ((str = rtsp_get_param(rtsp_session, "start"))) {
      convert_timestamp(str, &s_ss, &s_ms);
      free(str);
    }
    if ((str = rtsp_get_param(rtsp_session, "end"))) {
      convert_timestamp(str, &e_ss, &e_ms);
      free(str);
    }
    str = buf + sprintf(buf, s_ms ? "%s%d.%d-" : "%s%d-", "Range: npt=", s_ss, s_ms);
    if (e_ss || e_ms)
      sprintf(str, e_ms ? "%d.%d" : "%d", e_ss, e_ms);
  }
  rtsp_schedule_field(rtsp_session, buf);
  /* and finally send a play request */
  rtsp_request_play(rtsp_session,NULL);

out:
  subscribe = xbuffer_free(subscribe);
  buf = xbuffer_free(buf);
  free(description);
  free(session_id);
  free(challenge1);
  return h;
}

struct real_rtsp_session_t *
init_real_rtsp_session (void)
{
  struct real_rtsp_session_t *real_rtsp_session = NULL;

  real_rtsp_session = malloc (sizeof (struct real_rtsp_session_t));
  real_rtsp_session->recv = xbuffer_init (BUF_SIZE);
  real_rtsp_session->rdteof = 0;
  real_rtsp_session->rdt_rawdata = 0;

  return real_rtsp_session;
}

void
free_real_rtsp_session (struct real_rtsp_session_t* real_session)
{
  if (!real_session)
    return;

  xbuffer_free (real_session->recv);
  free (real_session);
}
