/**
 * convert D-Cinema Audio (probably SMPTE 302M) to a
 * wav file that MPlayer can play.
 * Usage: 302m_convert <infile> <outfile>
 */
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <byteswap.h>
#define le2me_32(x) (x)
#define le2me_16(x) (x)
#define be2me_16(x) bswap_16(x)

// From MPlayer libao/ao_pcm.c
#define WAV_ID_RIFF 0x46464952 /* "RIFF" */
#define WAV_ID_WAVE 0x45564157 /* "WAVE" */
#define WAV_ID_FMT  0x20746d66 /* "fmt " */
#define WAV_ID_DATA 0x61746164 /* "data" */
#define WAV_ID_PCM  0x0001

struct WaveHeader {
  uint32_t riff;
  uint32_t file_length;
  uint32_t wave;
  uint32_t fmt;
  uint32_t fmt_length;
  uint16_t fmt_tag;
  uint16_t channels;
  uint32_t sample_rate;
  uint32_t bytes_per_second;
  uint16_t block_align;
  uint16_t bits;
  uint32_t data;
  uint32_t data_length;
};

static struct WaveHeader wavhdr = {
  le2me_32(WAV_ID_RIFF),
  le2me_32(0x7fffffff),
  le2me_32(WAV_ID_WAVE),
  le2me_32(WAV_ID_FMT),
  le2me_32(16),
  le2me_16(WAV_ID_PCM),
  le2me_16(6),
  le2me_32(96000),
  le2me_32(1728000),
  le2me_16(18),
  le2me_16(24),
  le2me_32(WAV_ID_DATA),
  le2me_32(0x7fffffff),
};

// this format is completely braindead, and this bitorder
// is the result of pure guesswork (counting how often
// the bits flip), so it might be wrong.
void fixup(unsigned char *in_, unsigned char *out) {
  int i;
  unsigned char in[3] = {in_[0], in_[1], in_[2]};
  unsigned char sync = in[2] & 0x0f; // sync flags
  in[2] >>= 4;
  out[2] = 0;
  for (i = 0; i < 4; i++) {
    out[2] <<= 1;
    out[2] |= in[2] & 1;
    in[2] >>= 1;
  }
  for (i = 0; i < 4; i++) {
    out[2] <<= 1;
    out[2] |= in[1] & 1;
    in[1] >>= 1;
  }
  out[1] = 0;
  for (i = 0; i < 4; i++) {
    out[1] <<= 1;
    out[1] |= in[1] & 1;
    in[1] >>= 1;
  }
  for (i = 0; i < 4; i++) {
    out[1] <<= 1;
    out[1] |= in[0] & 1;
    in[0] >>= 1;
  }
  out[0] = 0;
  for (i = 0; i < 4; i++) {
    out[0] <<= 1;
    out[0] |= in[0] & 1;
    in[0] >>= 1;
  }
  out[0] <<= 4;
  out[0] |= sync; // sync flags go into lowest bits
  // it seems those might also contain audio data,
  // don't know if this is the right order then
  // these might be also useful to detect the number
  // of channels in case there are files with != 6 channels
}

int main(int argc, char *argv[]) {
  FILE *in = fopen(argv[1], "r");
  FILE *out = fopen(argv[2], "w");
  int i;
  uint16_t blocklen, unknown;
  unsigned char *block;
  if (!in) {
    printf("Could not open %s for reading\n", argv[1]);
    return EXIT_FAILURE;
  }
  if (!out) {
    printf("Could not open %s for writing\n", argv[2]);
    return EXIT_FAILURE;
  }
  fwrite(&wavhdr, 1, sizeof(wavhdr), out);
  do {
    fread(&blocklen, 2, 1, in);
    blocklen = be2me_16(blocklen);
    fread(&unknown, 2, 1, in);
    block = malloc(blocklen);
    blocklen = fread(block, 1, blocklen, in);
    for (i = 0; i < blocklen; i += 3)
      fixup(&block[i], &block[i]);
    fwrite(block, 1, blocklen, out);
    free(block);
  } while (!feof(in));
  return EXIT_SUCCESS;
}

