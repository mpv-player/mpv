#ifndef ADPCM_H
#define ADPCM_H

#define IMA_ADPCM_PREAMBLE_SIZE 2
#define IMA_ADPCM_BLOCK_SIZE 0x22
#define IMA_ADPCM_SAMPLES_PER_BLOCK \
  ((IMA_ADPCM_BLOCK_SIZE - IMA_ADPCM_PREAMBLE_SIZE) * 2)

#define MS_ADPCM_PREAMBLE_SIZE 7
#define MS_ADPCM_SAMPLES_PER_BLOCK \
  ((sh_audio->wf->nBlockAlign - MS_ADPCM_PREAMBLE_SIZE) * 2)

#define DK4_ADPCM_PREAMBLE_SIZE 4
#define DK4_ADPCM_SAMPLES_PER_BLOCK \
  (((sh_audio->wf->nBlockAlign - DK4_ADPCM_PREAMBLE_SIZE) * 2) + 1)

// pretend there's such a thing as mono for this format
#define DK3_ADPCM_PREAMBLE_SIZE 8
#define DK3_ADPCM_BLOCK_SIZE 0x400
// this isn't exact
#define DK3_ADPCM_SAMPLES_PER_BLOCK 6000

int qt_ima_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels);
int ms_ima_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels, int block_size);
int ms_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels, int block_size);
int dk4_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels, int block_size);
int dk3_adpcm_decode_block(unsigned short *output, unsigned char *input);

#endif
