/*
 * Speex decoder by Reimar Döffinger <Reimar.Doeffinger@stud.uni-karlsruhe.de>
 *
 * This code may be be relicensed under the terms of the GNU LGPL when it
 * becomes part of the FFmpeg project (ffmpeg.org)
 *
 * This file is part of MPlayer.
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
#include <stdlib.h>
#include <speex/speex.h>
#include <speex/speex_stereo.h>
#include <speex/speex_header.h>
#include "ad_internal.h"

static const ad_info_t info = {
  "Speex audio decoder",
  "speex",
  "Reimar Döffinger",
  "",
  ""
};

LIBAD_EXTERN(speex)

typedef struct {
  SpeexBits bits;
  void *dec_context;
  SpeexStereoState stereo;
  SpeexHeader *hdr;
} context_t;

#define MAX_FRAMES_PER_PACKET 100

static int preinit(sh_audio_t *sh) {
  sh->audio_out_minsize = 2 * 320 * MAX_FRAMES_PER_PACKET * 2 * sizeof(short);
  return 1;
}

static int read_le32(const uint8_t **src) {
    const uint8_t *p = *src;
    *src += 4;
    return p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
}

static int init(sh_audio_t *sh) {
  context_t *ctx = calloc(1, sizeof(context_t));
  const uint8_t *hdr = (const uint8_t *)(sh->wf + 1);
  const SpeexMode *spx_mode;
  const SpeexStereoState st_st = SPEEX_STEREO_STATE_INIT; // hack
  if (!sh->wf || sh->wf->cbSize < 80) {
    mp_msg(MSGT_DECAUDIO, MSGL_FATAL, "Missing extradata!\n");
    return 0;
  }
  ctx->hdr = speex_packet_to_header((char *)&sh->wf[1], sh->wf->cbSize);
  if (!ctx->hdr && sh->wf->cbSize == 0x72 && hdr[0] == 1 && hdr[1] == 0) {
    // speex.acm format: raw SpeexHeader dump
    ctx->hdr = calloc(1, sizeof(*ctx->hdr));
    hdr += 2;
    hdr += 8; // identifier string
    hdr += 20; // version string
    ctx->hdr->speex_version_id = read_le32(&hdr);
    ctx->hdr->header_size = read_le32(&hdr);
    ctx->hdr->rate = read_le32(&hdr);
    ctx->hdr->mode = read_le32(&hdr);
    ctx->hdr->mode_bitstream_version = read_le32(&hdr);
    ctx->hdr->nb_channels = read_le32(&hdr);
    ctx->hdr->bitrate = read_le32(&hdr);
    ctx->hdr->frame_size = read_le32(&hdr);
    ctx->hdr->vbr = read_le32(&hdr);
    ctx->hdr->frames_per_packet = read_le32(&hdr);
  }
  if (!ctx->hdr) {
    mp_msg(MSGT_DECAUDIO, MSGL_FATAL, "Invalid extradata!\n");
    return 0;
  }
  if (ctx->hdr->nb_channels != 1 && ctx->hdr->nb_channels != 2) {
    mp_msg(MSGT_DECAUDIO, MSGL_WARN, "Invalid number of channels (%i), "
            "assuming mono\n", ctx->hdr->nb_channels);
    ctx->hdr->nb_channels = 1;
  }
  if (ctx->hdr->frames_per_packet > MAX_FRAMES_PER_PACKET) {
    mp_msg(MSGT_DECAUDIO, MSGL_WARN, "Invalid number of frames per packet (%i), "
            "assuming 1\n", ctx->hdr->frames_per_packet);
    ctx->hdr->frames_per_packet = 1;
  }
  switch (ctx->hdr->mode) {
    case 0:
      spx_mode = &speex_nb_mode; break;
    case 1:
      spx_mode = &speex_wb_mode; break;
    case 2:
      spx_mode = &speex_uwb_mode; break;
    default:
      mp_msg(MSGT_DECAUDIO, MSGL_WARN, "Unknown speex mode (%i)\n", ctx->hdr->mode);
      spx_mode = &speex_nb_mode;
  }
  ctx->dec_context = speex_decoder_init(spx_mode);
  speex_bits_init(&ctx->bits);
  memcpy(&ctx->stereo, &st_st, sizeof(ctx->stereo)); // hack part 2
  sh->channels = ctx->hdr->nb_channels;
  sh->samplerate = ctx->hdr->rate;
  sh->samplesize = 2;
  sh->sample_format = AF_FORMAT_S16_NE;
  sh->context = ctx;
  return 1;
}

static void uninit(sh_audio_t *sh) {
  context_t *ctx = sh->context;
  if (ctx) {
    speex_bits_destroy(&ctx->bits);
    speex_decoder_destroy(ctx->dec_context);
    if (ctx->hdr)
      free(ctx->hdr);
    free(ctx);
  }
  ctx = NULL;
}

static int decode_audio(sh_audio_t *sh, unsigned char *buf,
                        int minlen, int maxlen) {
  context_t *ctx = sh->context;
  int len, framelen, framesamples;
  char *packet;
  int i, err;
  speex_decoder_ctl(ctx->dec_context, SPEEX_GET_FRAME_SIZE, &framesamples);
  framelen = framesamples * ctx->hdr->nb_channels * sizeof(short);
  if (maxlen < ctx->hdr->frames_per_packet * framelen) {
    mp_msg(MSGT_DECAUDIO, MSGL_V, "maxlen too small in decode_audio\n");
    return -1;
  }
  len = ds_get_packet(sh->ds, (unsigned char **)&packet);
  if (len <= 0) return -1;
  speex_bits_read_from(&ctx->bits, packet, len);
  i = ctx->hdr->frames_per_packet;
  do {
    err = speex_decode_int(ctx->dec_context, &ctx->bits, (short *)buf);
    if (err == -2)
      mp_msg(MSGT_DECAUDIO, MSGL_ERR, "Error decoding file.\n");
    if (ctx->hdr->nb_channels == 2)
      speex_decode_stereo_int((short *)buf, framesamples, &ctx->stereo);
    buf = &buf[framelen];
  } while (--i > 0);
  return ctx->hdr->frames_per_packet * framelen;
}

static int control(sh_audio_t *sh, int cmd, void *arg, ...) {
  return CONTROL_UNKNOWN;
}
