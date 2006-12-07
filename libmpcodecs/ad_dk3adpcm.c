/*
    DK3 ADPCM Decoder for MPlayer
      by Mike Melanson

    "This format number was used by Duck Corp. but not officially 
    registered with Microsoft"

    This file is responsible for decoding audio data encoded with
    Duck Corp's DK3 ADPCM algorithm. Details about the data format
    can be found here:
      http://www.pcisys.net/~melanson/codecs/
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "libavutil/common.h"
#include "mpbswap.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"Duck Corp. DK3 ADPCM decoder",
	"dk3adpcm",
	"Nick Kurshev",
	"Mike Melanson",
	""
};

LIBAD_EXTERN(dk3adpcm)

#define DK3_ADPCM_PREAMBLE_SIZE 16

#define LE_16(x) (le2me_16(*(unsigned short *)(x)))
#define LE_32(x) (le2me_32(*(unsigned int *)(x)))

// useful macros
// clamp a number between 0 and 88
#define CLAMP_0_TO_88(x)  if (x < 0) x = 0; else if (x > 88) x = 88;
// clamp a number within a signed 16-bit range
#define CLAMP_S16(x)  if (x < -32768) x = -32768; \
  else if (x > 32767) x = 32767;
// clamp a number above 16
#define CLAMP_ABOVE_16(x)  if (x < 16) x = 16;
// sign extend a 16-bit value
#define SE_16BIT(x)  if (x & 0x8000) x -= 0x10000;
// sign extend a 4-bit value
#define SE_4BIT(x)  if (x & 0x8) x -= 0x10;

// pertinent tables
static int adpcm_step[89] =
{
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
  19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
  50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
  130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
  337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
  876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
  2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
  5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
  15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static int adpcm_index[16] =
{
  -1, -1, -1, -1, 2, 4, 6, 8,
  -1, -1, -1, -1, 2, 4, 6, 8
};

static int preinit(sh_audio_t *sh_audio)
{
  sh_audio->audio_out_minsize = sh_audio->wf->nBlockAlign * 6;
  sh_audio->ds->ss_div = 
    (sh_audio->wf->nBlockAlign - DK3_ADPCM_PREAMBLE_SIZE) * 8 / 3;
  sh_audio->audio_in_minsize=
  sh_audio->ds->ss_mul = sh_audio->wf->nBlockAlign;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
  sh_audio->channels = sh_audio->wf->nChannels;
  sh_audio->samplerate = sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps =
    (sh_audio->ds->ss_mul * sh_audio->samplerate) / sh_audio->ds->ss_div;
  sh_audio->samplesize=2;
  return 1;
}

static void uninit(sh_audio_t *sh_audio)
{
}

static int control(sh_audio_t *sh_audio,int cmd,void* arg, ...)
{
  if(cmd==ADCTRL_SKIP_FRAME){
    demux_read_data(sh_audio->ds, sh_audio->a_in_buffer,sh_audio->ds->ss_mul);
    return CONTROL_TRUE;
  }
  return CONTROL_UNKNOWN;
}

#define DK3_GET_NEXT_NIBBLE() \
    if (decode_top_nibble_next) \
    { \
      nibble = (last_byte >> 4) & 0x0F; \
      decode_top_nibble_next = 0; \
    } \
    else \
    { \
      last_byte = input[in_ptr++]; \
      nibble = last_byte & 0x0F; \
      decode_top_nibble_next = 1; \
    }

// note: This decoder assumes the format 0x62 data always comes in
// stereo flavor
static int dk3_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int block_size)
{
  int sum_pred;
  int diff_pred;
  int sum_index;
  int diff_index;
  int diff_channel;
  int in_ptr = 0x10;
  int out_ptr = 0;

  unsigned char last_byte = 0;
  unsigned char nibble;
  int decode_top_nibble_next = 0;

  // ADPCM work variables
  int sign;
  int delta;
  int step;
  int diff;

  sum_pred = LE_16(&input[10]);
  diff_pred = LE_16(&input[12]);
  SE_16BIT(sum_pred);
  SE_16BIT(diff_pred);
  diff_channel = diff_pred;
  sum_index = input[14];
  diff_index = input[15];

  while (in_ptr < block_size - !decode_top_nibble_next)
//  while (in_ptr < 2048)
  {
    // process the first predictor of the sum channel
    DK3_GET_NEXT_NIBBLE();

    step = adpcm_step[sum_index];

    sign = nibble & 8;
    delta = nibble & 7;

    diff = step >> 3;
    if (delta & 4) diff += step;
    if (delta & 2) diff += step >> 1;
    if (delta & 1) diff += step >> 2;

    if (sign)
      sum_pred -= diff;
    else
      sum_pred += diff;

    CLAMP_S16(sum_pred);

    sum_index += adpcm_index[nibble];
    CLAMP_0_TO_88(sum_index);

    // process the diff channel predictor
    DK3_GET_NEXT_NIBBLE();

    step = adpcm_step[diff_index];

    sign = nibble & 8;
    delta = nibble & 7;

    diff = step >> 3;
    if (delta & 4) diff += step;
    if (delta & 2) diff += step >> 1;
    if (delta & 1) diff += step >> 2;

    if (sign)
      diff_pred -= diff;
    else
      diff_pred += diff;

    CLAMP_S16(diff_pred);

    diff_index += adpcm_index[nibble];
    CLAMP_0_TO_88(diff_index);

    // output the first pair of stereo PCM samples
    diff_channel = (diff_channel + diff_pred) / 2;
    output[out_ptr++] = sum_pred + diff_channel;
    output[out_ptr++] = sum_pred - diff_channel;

    // process the second predictor of the sum channel
    DK3_GET_NEXT_NIBBLE();

    step = adpcm_step[sum_index];

    sign = nibble & 8;
    delta = nibble & 7;

    diff = step >> 3;
    if (delta & 4) diff += step;
    if (delta & 2) diff += step >> 1;
    if (delta & 1) diff += step >> 2;

    if (sign)
      sum_pred -= diff;
    else
      sum_pred += diff;

    CLAMP_S16(sum_pred);

    sum_index += adpcm_index[nibble];
    CLAMP_0_TO_88(sum_index);

    // output the second pair of stereo PCM samples
    output[out_ptr++] = sum_pred + diff_channel;
    output[out_ptr++] = sum_pred - diff_channel;
  }

  return out_ptr;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  if (demux_read_data(sh_audio->ds, sh_audio->a_in_buffer,
    sh_audio->ds->ss_mul) != 
    sh_audio->ds->ss_mul) 
      return -1; /* EOF */

  if (maxlen < 2 * 4 * sh_audio->wf->nBlockAlign * 2 / 3) {
    mp_msg(MSGT_DECAUDIO, MSGL_V, "dk3adpcm: maxlen too small in decode_audio\n");
    return -1;
  }
  return 2 * dk3_adpcm_decode_block(
    (unsigned short*)buf, sh_audio->a_in_buffer,
    sh_audio->ds->ss_mul);
}
