#ifndef DECODE288_H
#define DECODE288_H

/* internal globals */
typedef struct {
	float	history[8];
	float	output[40];
	float	pr1[36];
	float	pr2[10];
	int		phase, phasep;

	float st1a[111],st1b[37],st1[37];
	float st2a[38],st2b[11],st2[11];
	float sb[41];
	float lhist[10];
} Real_internal;

/* prototypes */
static void unpack  (unsigned short *tgt, unsigned char *src, int len);
static void decode  (Real_internal *internal, unsigned int input);
static void update  (Real_internal *internal);
static void colmult (float *tgt, float *m1, const float *m2, int n);
static int  pred    (float *in, float *tgt, int n);
static void co      (int n, int i, int j, float *in, float *out, float *st1, float *st2, const float *table);
static void prodsum (float *tgt, float *src, int len, int n);

#endif /* !DECODE288_H */
