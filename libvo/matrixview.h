#ifndef MPLAYER_MATRIXVIEW_H
#define MPLAYER_MATRIXVIEW_H

#include <stdint.h>
void matrixview_init (int w, int h);
void matrixview_reshape (int w, int h);
void matrixview_draw (int w, int h, double currentTime, float frameTime, uint8_t *data);
void matrixview_matrix_resize(int w, int h);
void matrixview_contrast_set(float contrast);
void matrixview_brightness_set(float brightness);

#endif /* MPLAYER_MATRIXVIEW_H */
