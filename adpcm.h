#ifndef ADPCM_H
#define ADPCM_H

#define IMA_ADPCM_PREAMBLE_SIZE 2
#define IMA_ADPCM_BLOCK_SIZE 0x22
#define IMA_ADPCM_SAMPLES_PER_BLOCK \
  ((IMA_ADPCM_BLOCK_SIZE - IMA_ADPCM_PREAMBLE_SIZE) * 2)

#define MS_ADPCM_PREAMBLE_SIZE 7
#define MS_ADPCM_SAMPLES_PER_BLOCK \
  ((sh_audio->wf->nBlockAlign - MS_ADPCM_PREAMBLE_SIZE) * 2)

// pretend there's such a thing as mono for this format
#define FOX62_ADPCM_PREAMBLE_SIZE 8
#define FOX62_ADPCM_BLOCK_SIZE 0x400
#define FOX62_ADPCM_SAMPLES_PER_BLOCK \
  ((FOX62_ADPCM_BLOCK_SIZE - FOX62_ADPCM_PREAMBLE_SIZE) * 2)

int ima_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels);
int ms_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels, int block_size);
int fox62_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels);

#endif
