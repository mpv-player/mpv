#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#define DATABLOCK1	20				/* size of 14.4 input block in bytes */
#define DATABLOCK2	38				/* size of 28.8 input block in bytes */
#define DATACHUNK1	1440			/* size of 14.4 input chunk in bytes */
#define DATACHUNK2	2736			/* size of 28.8 input chunk in bytes */
#define AUDIOBLOCK	160				/* size of output block in 16-bit words (320 bytes) */
//#define AUDIOBUFFER	11520			/* size of output buffer in 16-bit words (23040 bytes) */
#define AUDIOBUFFER	12288			/* size of output buffer in 16-bit words (24576 bytes) */

typedef void Real_144;
typedef void Real_288;
typedef void Real_dnet;

/* common prototypes */
Real_144 *init_144     (void);
Real_288 *init_288     (void);
Real_dnet *init_dnet   (void);
void      free_144     (Real_144 *global);
void      free_288     (Real_288 *global);
void      free_dnet    (Real_dnet *global);
void      deinterleave (unsigned char *in, unsigned char *out, unsigned int size);
void      swapbytes    (unsigned char *in, unsigned char *out, unsigned int len);
void      decode_144   (Real_144 *global, unsigned char *source, signed short *target);
void      decode_288   (Real_288 *global, unsigned char *in, signed short *out);
int       decode_dnet  (Real_dnet *global, unsigned char *in, signed short *out, int *freq, int chans);

#endif /* !COMMON_H */
