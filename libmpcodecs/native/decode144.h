#ifndef DECODE144_H
#define DECODE144_H

/* internal globals */
typedef struct {
	unsigned int	 resetflag, val, oldval;
	unsigned int	 unpacked[28];		/* buffer for unpacked input */
	unsigned int	*iptr;				/* pointer to current input (from unpacked) */
	unsigned int	 gval;
	unsigned short	*gsp;
	unsigned int	 gbuf1[8];
	unsigned short	 gbuf2[120];
	signed   short	 output_buffer[40];
	unsigned int	*decptr;			/* decoder ptr */
	signed   short	*decsp;

	/* the swapped buffers */
	unsigned int	 swapb1a[10];
	unsigned int	 swapb2a[10];
	unsigned int	 swapb1b[10];
	unsigned int	 swapb2b[10];
	unsigned int	*swapbuf1;
	unsigned int	*swapbuf2;
	unsigned int	*swapbuf1alt;
	unsigned int	*swapbuf2alt;

	unsigned int buffer[5];
	unsigned short int buffer_2[148];
	unsigned short int buffer_a[40];
	unsigned short int buffer_b[40];
	unsigned short int buffer_c[40];
	unsigned short int buffer_d[40];

	unsigned short int work[50];
	unsigned short *sptr;

	int buffer1[10];
	int buffer2[10];

	signed short wavtable1[2304];
	unsigned short wavtable2[2304];
} Real_internal;

/* consts */
#define NBLOCKS		4				/* number of segments within a block */
#define BLOCKSIZE	40				/* (quarter) block size in 16-bit words (80 bytes) */
#define HALFBLOCK	20				/* BLOCKSIZE/2 */
#define BUFFERSIZE	146				/* for do_output */

/* prototypes */
static int          t_sqrt             (unsigned int x);
static void         do_voice           (int *a1, int *a2);
static void         do_output_subblock (Real_internal *glob, int x);
static void         rotate_block       (short *source, short *target, int offset);
static int          irms               (short *data, int factor);
static void         add_wav            (Real_internal *glob, int n, int f, int m1, int m2, int m3, short *s1, short *s2, short *s3, short *dest);
static void         final              (Real_internal *glob, short *i1, short *i2, void *out, int *statbuf, int len);
static void         unpack_input       (unsigned char *input, unsigned int *output);
static unsigned int rms                (int *data, int f);
static void         dec1               (Real_internal *glob, int *data, int *inp, int n, int f);
static void         dec2               (Real_internal *glob, int *data, int *inp, int n, int f, int *inp2, int l);
static int          eq                 (Real_internal *glob, short *in, int *target);

#endif /* !DECODE144_H */
