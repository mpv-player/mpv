#ifndef ROQAV_H
#define ROQAV_H

void *roq_decode_video_init(void);
void roq_decode_video(void *context, unsigned char *encoded, 
  int encoded_size, mp_image_t *mpi);

void *roq_decode_audio_init(void);
int roq_decode_audio(unsigned short *output, unsigned char *input,
  int encoded_size, int channels, void *context);

#endif // ROQAV_H
