#ifndef ADPCM_H
#define ADPCM_H

#define IMA_ADPCM_BLOCK_SIZE 0x22
#define IMA_ADPCM_SAMPLES_PER_BLOCK 0x40

#define MS_ADPCM_BLOCK_SIZE 256
#define MS_ADPCM_SAMPLES_PER_BLOCK ((256 - 7) * 2)

int ima_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels);
int ms_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels);

#endif
