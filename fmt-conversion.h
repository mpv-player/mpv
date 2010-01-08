#ifndef MPLAYER_FMT_CONVERSION_H
#define MPLAYER_FMT_CONVERSION_H

#include "config.h"
#include "libavutil/avutil.h"

enum PixelFormat imgfmt2pixfmt(int fmt);
int pixfmt2imgfmt(enum PixelFormat pix_fmt);

#endif /* MPLAYER_FMT_CONVERSION_H */
