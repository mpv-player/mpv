#ifndef ROQAV_H
#define ROQAV_H

void *roq_decode_video_init(void);
void roq_decode_video(unsigned char *encoded, int encoded_size,
  unsigned char *decoded, int width, int height, void *context);

void *roq_decode_audio_init(void);
int roq_decode_audio(unsigned short *output, unsigned char *input,
  int channels, void *context);

#endif // ROQAV_H
