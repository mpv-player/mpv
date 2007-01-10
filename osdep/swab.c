#include "config.h"

/* system has no swab.  emulate via bswap */
#include "mpbswap.h"
#include <unistd.h>

void swab(const void *from, void *to, ssize_t n) {
  const int16_t *in = (int16_t*)from;
  int16_t *out = (int16_t*)to;
  int i;
  n /= 2;
  for (i = 0 ; i < n; i++) {
    out[i] = bswap_16(in[i]);
  }
}
