
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "config.h"
#include "../version.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"
#include "bswap.h"

#include "muxer.h"

// 18 bytes reserved for block headers and STD
#define MUXER_MPEG_DATASIZE (MUXER_MPEG_BLOCKSIZE-18)

// ISO-11172 requirements
#define MPEG_MAX_PTS_DELAY 90000 /* 1s */
#define MPEG_MAX_SCR_INTERVAL 63000 /* 0.7s */

// suggestions
#define MPEG_STARTPTS 45000 /* 0.5s */
#define MPEG_MIN_PTS_DELAY 9000 /* 0.1s */
#define MPEG_STARTSCR 9 /* 0.1ms */

//static unsigned int mpeg_min_delay;
//static unsigned int mpeg_max_delay;

static muxer_stream_t* mpegfile_new_stream(muxer_t *muxer,int type){
  muxer_stream_t *s;

  if (!muxer) return NULL;
  if(muxer->avih.dwStreams>=MUXER_MAX_STREAMS){
    printf("Too many streams! increase MUXER_MAX_STREAMS !\n");
    return NULL;
  }
  switch (type) {
    case MUXER_TYPE_VIDEO:
      if (muxer->num_videos >= 15) {
	printf ("MPEG stream can't contain above of 15 video streams!\n");
	return NULL;
      }
      break;
    case MUXER_TYPE_AUDIO:
      if (muxer->avih.dwStreams - muxer->num_videos >= 31) {
	printf ("MPEG stream can't contain above of 31 audio streams!\n");
	return NULL;
      }
      break;
    default:
      printf ("Unknown stream type!\n");
      return NULL;
  }
  s=malloc(sizeof(muxer_stream_t));
  memset(s,0,sizeof(muxer_stream_t));
  if(!s) return NULL; // no mem!?
  if (!(s->b_buffer = malloc (MUXER_MPEG_BLOCKSIZE))) {
    free (s);
    return NULL; // no mem?!
  } else if (type == MUXER_TYPE_VIDEO) {
    s->ckid = be2me_32 (0x1e0 + muxer->num_videos);
    muxer->num_videos++;
    s->h.fccType=streamtypeVIDEO;
    if(!muxer->def_v) muxer->def_v=s;
//    printf ("Added video stream %d\n", muxer->num_videos);
  } else { // MUXER_TYPE_AUDIO
    s->ckid = be2me_32 (0x1c0 + s->id - muxer->num_videos);
    s->h.fccType=streamtypeAUDIO;
//    printf ("Added audio stream %d\n", s->id - muxer->num_videos + 1);
  }
  muxer->streams[muxer->avih.dwStreams]=s;
  s->type=type;
  s->id=muxer->avih.dwStreams;
  s->timer=0.0;
  s->size=0;
  s->muxer=muxer;
  muxer->avih.dwStreams++;
  return s;
}

static void write_mpeg_ts(unsigned char *b, unsigned int ts, char mod) {
  b[0] = ((ts >> 29) & 0xf) | 1 | mod;
  b[1] = (ts >> 22) & 0xff;
  b[2] = ((ts >> 14) & 0xff) | 1;
  b[3] = (ts >> 7) & 0xff;
  b[4] = ((ts << 1) & 0xff) | 1;
}

static void write_mpeg_rate(unsigned char *b, unsigned int rate) {
  if (rate)
    rate--; // for round upward
  rate /= 50;
  rate++; // round upward
  b[0] = ((rate >> 15) & 0x7f) | 0x80;
  b[1] = (rate >> 7) & 0xff;
  b[2] = ((rate << 1) & 0xff) | 1;
}

static void write_mpeg_std(unsigned char *b, unsigned int size, char mod) {
  if (size)
    size--; // for round upward
  if (size < (128 << 8))
    size >>= 7; // by 128 bytes
  else {
    size >>= 10;
    size |= 0x2000; // by 1kbyte
  }
  size++; // round upward
  b[0] = ((size >> 8) & 0x3f) | 0x40 | mod;
  b[1] = size & 0xff;
}

static int write_mpeg_block(muxer_t *muxer, muxer_stream_t *s, FILE *f, char *bl, size_t len, int isoend){
  size_t sz; // rest in block buffer
  unsigned char buff[12]; // 0x1ba header
  unsigned int mints=0;
  uint16_t l1;

  if (s->b_buffer_ptr == 0) { // 00001111 if no PTS
    s->b_buffer[0] = 0xf;
    s->b_buffer_ptr = 1;
    sz = MUXER_MPEG_DATASIZE-1;
  } else if (s->b_buffer_ptr > MUXER_MPEG_DATASIZE) {
    printf ("Unknown error in write_mpeg_block()!\n");
    return 0;
  } else {
    sz = MUXER_MPEG_DATASIZE - s->b_buffer_ptr;
    if (s->b_buffer[7] == 0xff) // PTS not set yet
      s->b_buffer[11] = 0xf; // terminate stuFFing bytes
  }
  if (len > sz)
    len = sz;
  *(uint32_t *)buff = be2me_32 (0x1ba);
  write_mpeg_ts (buff+4, muxer->file_end, 0x20); // 0010 and SCR
  write_mpeg_rate (buff+9, muxer->sysrate);
  fwrite (buff, 12, 1, f);
  fwrite (&s->ckid, 4, 1, f); // stream_id
  memset (buff, 0xff, 12); // stuFFing bytes
  sz -= len;
  // calculate padding bytes in buffer
  while (mints < s->b_buffer_ptr && s->b_buffer[mints] == 0xff) mints++;
  if (mints+sz < 12) { // cannot write padding block so write up to 12 stuFFing bytes
    l1 = be2me_16 (MUXER_MPEG_DATASIZE);
    fwrite (&l1, 2, 1, f);
    mints = 0; // so stuFFed bytes will be written all
    if (sz)
      fwrite (buff, sz, 1, f);
    sz = 0; // no padding block anyway
  } else { // use padding block
    if (sz > 6) // sufficient for PAD header so don't shorter data
      mints = 0;
    else
      sz += mints; // skip stuFFing bytes (sz>8 here)
    l1 = be2me_16 (s->b_buffer_ptr+len-mints);
    fwrite (&l1, 2, 1, f);
  }
  if (s->b_buffer_ptr)
    fwrite (s->b_buffer+mints, s->b_buffer_ptr-mints, 1, f);
  if (len)
    fwrite (bl, len, 1, f);
  if (sz > 6) { // padding block (0x1be)
    uint32_t l0;

    if (isoend)
      l0 = be2me_32 (0x1b9);
    else
      l0 = be2me_32 (0x1be);
    sz -= 6;
    l1 = be2me_16 (sz);
    fwrite (&l0, 4, 1, f);
    fwrite (&l1, 2, 1, f);
    memset (s->b_buffer, 0xff, sz); // stuFFing bytes
    fwrite (s->b_buffer, sz, 1, f);
  }
  s->b_buffer_ptr = 0;
  muxer->movi_end += MUXER_MPEG_BLOCKSIZE;
  // prepare timestamps for next pack
  mints = (MUXER_MPEG_BLOCKSIZE*90000/muxer->sysrate)+1; // min ts delta
  sz = (int)(s->timer*90000) + MPEG_STARTPTS; // new PTS
  if (sz > muxer->file_end)
    sz -= muxer->file_end; // suggested ts delta
  else
  {
    sz = 0;
    printf ("Error in stream: PTS earlier than SCR!\n");
  }
  if (sz > MPEG_MAX_PTS_DELAY) {
//    printf ("Warning: attempt to set PTS to SCR delay to %u \n", sz);
    mints = sz-MPEG_MAX_PTS_DELAY; // try to compensate
    if (mints > MPEG_MAX_SCR_INTERVAL) {
      printf ("Error in stream: SCR interval %u is too big!\n", mints);
    }
  } else if (sz > 54000) // assume 0.3...0.7s is optimal
    mints += (sz-45000)>>2; // reach 0.5s in 4 blocks ?
  else if (sz < 27000) {
    unsigned int newsysrate = 0;

    if (s->timer > 0.5) // too early to calculate???
      newsysrate = muxer->movi_end/(s->timer*0.4); // pike-factor 2.5 (8dB)
    if (sz < MPEG_MIN_PTS_DELAY)
      printf ("Error in stream: PTS to SCR delay %u is too little!\n", sz);
    if (muxer->sysrate < newsysrate)
      muxer->sysrate = newsysrate; // increase next rate to current rate
    else if (!newsysrate)
      muxer->sysrate += muxer->sysrate>>3; // increase next rate by 25%
  }
  muxer->file_end += mints; // update the system timestamp
  return len;
}

static void set_mpeg_pts(muxer_t *muxer, muxer_stream_t *s, unsigned int pts) {
  unsigned int dts, nts;

  if (s->b_buffer_ptr != 0 && s->b_buffer[7] != 0xff)
    return; // already set
  if (s->b_buffer_ptr == 0) {
    memset (s->b_buffer, 0xff, 7); // reserved for PTS or STD, stuFFing for now
    s->b_buffer_ptr = 12;
  }
  dts = (int)(s->timer*90000) + MPEG_STARTPTS; // PTS
  if (pts) {
    write_mpeg_ts (s->b_buffer+2, pts, 0x30); // 0011 and both PTS/DTS
  } else {
    write_mpeg_ts (s->b_buffer+7, dts, 0x20); // 0010 and PTS only
    return;
  }
  nts = dts - muxer->file_end;
//  if (nts < mpeg_min_delay) mpeg_min_delay = nts;
//  if (nts > mpeg_max_delay) mpeg_max_delay = nts;
  nts = 180000*s->h.dwScale/s->h.dwRate; // two frames
  if (dts-nts < muxer->file_end) {
    dts += muxer->file_end;
    dts /= 2; // calculate average time
    printf ("Warning: DTS to SCR delay is too small\n");
  }
  else
    dts -= nts/2; // one frame :)
  write_mpeg_ts (s->b_buffer+7, dts, 0x10);
}

static void mpegfile_write_chunk(muxer_stream_t *s,size_t len,unsigned int flags){
  size_t ptr=0, sz;
  unsigned int pts=0;
  muxer_t *muxer = s->muxer;
  FILE *f;

  f = muxer->file;
  if (s->type == MUXER_TYPE_VIDEO) { // try to recognize frame type...
    if (s->buffer[0] != 0 || s->buffer[1] != 0 || s->buffer[2] != 1 || len<6) {
      printf ("Unknown block type, possibly non-MPEG stream!\n");
      sz = len;
//      return;
    } else if (s->buffer[3] == 0 || s->buffer[3] == 0xb3 ||
	       s->buffer[3] == 0xb8) { // Picture or GOP
      int temp_ref;
      int pt;

      if (s->buffer[3]) { // GOP -- scan for Picture
	s->gop_start = s->h.dwLength;
	while (ptr < len-5 && (s->buffer[ptr] != 0 || s->buffer[ptr+1] != 0 ||
	       s->buffer[ptr+2] != 1 || s->buffer[ptr+3] != 0)) ptr++;
	if (s->b_buffer_ptr > MUXER_MPEG_DATASIZE-39-12) { // 39 bytes for Gop+Pic+Slice headers
	  write_mpeg_block (muxer, s, f, NULL, 0, 0);
	}
      }
      if (ptr >= len-5) {
	pt = 0; // Picture not found?!
	temp_ref = 0;
	printf ("Warning: picture not found in GOP!\n");
      } else {
	pt = (s->buffer[ptr+5]>>3) & 7;
	temp_ref = (s->buffer[ptr+4]<<2)+(s->buffer[ptr+5]>>6);
      }
      ptr = 0;
      temp_ref += s->gop_start;
      switch (pt) {
	case 2: // predictive
	  if (s->ipb[0]) {
	    sz = len + s->ipb[0];
	    if (s->ipb[0] < s->ipb[2])
	      s->ipb[0] = s->ipb[2];
	    s->ipb[2] = 0;
	  } else if (s->ipb[2]) {
	    sz = len + s->ipb[2];
	    s->ipb[0] = s->ipb[2];
	    s->ipb[2] = 0;
	  } else
	    sz = 4 * len; // no bidirectional frames yet?
	  s->ipb[1] = len;
	  // pictires may be not in frame sequence so recalculate timer
	  pts = (int)(90000*((double)temp_ref*s->h.dwScale/s->h.dwRate)) + MPEG_STARTPTS;
	  break;
	case 3: // bidirectional
	  s->ipb[2] += len;
	  sz = s->ipb[1] + s->ipb[2];
	  // pictires may be not in frame sequence so recalculate timer
	  s->timer = (double)temp_ref*s->h.dwScale/s->h.dwRate;
	  break;
	default: // intra-coded
	  // pictires may be not in frame sequence so recalculate timer
	  pts = (int)(90000*((double)temp_ref*s->h.dwScale/s->h.dwRate)) + MPEG_STARTPTS;
	  sz = len; // no extra buffer for it...
      }
    } else {
      printf ("Unknown block type, possibly non-MPEG stream!\n");
      sz = len;
//      return;
    }
    sz <<= 1;
  } else { // MUXER_TYPE_AUDIO
    if (len < 2*MUXER_MPEG_DATASIZE)
      sz = 2*MUXER_MPEG_DATASIZE; // min requirement
    else
      sz = len;
  }
  set_mpeg_pts (muxer, s, pts);
  // alter counters:
  if (s->h.dwSampleSize) {
    // CBR
    s->h.dwLength += len/s->h.dwSampleSize;
    if (len%s->h.dwSampleSize) printf("Warning! len isn't divisable by samplesize!\n");
  } else {
    // VBR
    s->h.dwLength++;
  }
  if (!muxer->sysrate) {
    muxer->sysrate = 2108000/8; // constrained stream parameter
    muxer->file_end = MUXER_MPEG_BLOCKSIZE*90000/muxer->sysrate + MPEG_STARTSCR+1;
  }
  if (sz > s->h.dwSuggestedBufferSize) { // increase and set STD
    s->h.dwSuggestedBufferSize = sz;
    if (s->b_buffer[2] != 0xff) // has both PTS and DTS
      write_mpeg_std (s->b_buffer, s->h.dwSuggestedBufferSize, 0x40); // 01
    else // has only PTS
      write_mpeg_std (s->b_buffer+5, s->h.dwSuggestedBufferSize, 0x40); // 01
  }
  s->size += len;
  // write out block(s) if it's ready
  while (s->b_buffer_ptr+len >= MUXER_MPEG_DATASIZE-12) { // reserved for std and pts
    // write out the block
    sz = write_mpeg_block (muxer, s, f, &s->buffer[ptr], len, 0);
    // recalculate the rest of chunk
    ptr += sz;
    len -= sz;
  }
  s->timer = (double)s->h.dwLength*s->h.dwScale/s->h.dwRate;
  if (len) { // save rest in buffer
    if (s->b_buffer_ptr == 0) {
      memset (s->b_buffer, 0xff, 12); // stuFFing bytes for now
      if (s->type == MUXER_TYPE_AUDIO && s->h.dwSampleSize) { // CBR audio
	sz = s->h.dwLength - len/s->h.dwSampleSize; // first sample number
	write_mpeg_ts (s->b_buffer+7,
	    (int)(90000*((double)sz*s->h.dwScale/s->h.dwRate)) + MPEG_STARTPTS,
	    0x20); // 0010 and PTS only
      }
      s->b_buffer_ptr = 12;
    }
    memcpy (s->b_buffer+s->b_buffer_ptr, s->buffer+ptr, len);
    s->b_buffer_ptr += len;
  }
}

static void mpegfile_write_header(muxer_t *muxer){
  unsigned int i;
  size_t sz = MUXER_MPEG_BLOCKSIZE-24;
  unsigned char buff[12];
  muxer_stream_t *s = muxer->streams[0];
  uint32_t l1;
  uint16_t l2;
  FILE *f = muxer->file;

  if (s == NULL)
    return; // no streams!?
  // packet header (0x1ba) -- rewrite first stream buffer
  *(uint32_t *)buff = be2me_32 (0x1ba);
  write_mpeg_ts (buff+4, MPEG_STARTSCR, 0x20); // 0010 -- pack
  write_mpeg_rate (buff+9, muxer->sysrate);
  fwrite (buff, 12, 1, f);
  // start system stream (in own block): Sys (0x1bb)
  l1 = be2me_32 (0x1bb);
  l2 = be2me_16 (6 + 3*muxer->avih.dwStreams); // header_length
  fwrite (&l1, 4, 1, f);
  fwrite (&l2, 2, 1, f);
  write_mpeg_rate (buff, muxer->sysrate); // rate_bound
  // set number of audio/video, fixed_flag=CSPS_flag=system_*_lock_flag=0
  buff[3] = (muxer->avih.dwStreams - muxer->num_videos) << 2; // audio_bound
  buff[4] = muxer->num_videos | 0x20;
  buff[5] = 0xff; // reserved_byte
  fwrite (buff, 6, 1, f);
  for (i = 0; i < muxer->avih.dwStreams; i++) {
    buff[0] = ((char *)&muxer->streams[i]->ckid)[3]; // last char in big endian
//fprintf (stderr, "... stream 0x1%02x; bufsize %u", (int)buff[0], muxer->streams[i]->h.dwSuggestedBufferSize);
    write_mpeg_std (buff+1, muxer->streams[i]->h.dwSuggestedBufferSize, 0xc0); // 11
    fwrite (buff, 3, 1, f);
    sz -= 3;
  }
  if (sz >= 6) { // padding block
    l1 = be2me_32 (0x1be);
    sz -= 6;
    l2 = be2me_16 (sz);
    fwrite (&l1, 4, 1, f);
    fwrite (&l2, 2, 1, f);
  }
  s->b_buffer[0] = 0x0f; // end of list - next bit has to be 0
  // stuFFing bytes -- rewrite first stream buffer
  if (sz > 1)
    memset (s->b_buffer+1, 0xff, sz-1);
  fwrite (s->b_buffer, sz, 1, f);
  muxer->movi_start = 0;
  muxer->movi_end = MUXER_MPEG_BLOCKSIZE;
}

static void mpegfile_write_index(muxer_t *muxer){
  unsigned int i;
  unsigned int rsr;

  if (!muxer->avih.dwStreams) return; // no streams?!
  // finish all but one video and audio streams
  rsr = muxer->sysrate; // reserve it since it's silly change it at that point
  for (i = 0; i < muxer->avih.dwStreams-1; i++)
    write_mpeg_block (muxer, muxer->streams[i], muxer->file, NULL, 0, 0);
  // end sequence: ISO-11172-End (0x1b9) and finish very last block
  write_mpeg_block (muxer, muxer->streams[i], muxer->file, NULL, 0, 1);
//fprintf (stderr, "PTS to SCR delay: min %u.%03u, max %u.%03u\n",
//	mpeg_min_delay/90000, (mpeg_min_delay/90)%1000,
//	mpeg_max_delay/90000, (mpeg_max_delay/90)%1000);
  muxer->sysrate = rsr;
}

void muxer_init_muxer_mpeg(muxer_t *muxer){
  muxer->cont_new_stream = &mpegfile_new_stream;
  muxer->cont_write_chunk = &mpegfile_write_chunk;
  muxer->cont_write_header = &mpegfile_write_header;
  muxer->cont_write_index = &mpegfile_write_index;
//  mpeg_min_delay = mpeg_max_delay = MPEG_STARTPTS-MPEG_STARTSCR;
}

