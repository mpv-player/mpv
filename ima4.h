#ifndef QUICKTIME_IMA4_H
#define QUICKTIME_IMA4_H

//#include "quicktime.h"
//#include "inttypes.h"

/* Known by divine revelation */

#define IMA4_BLOCK_SIZE 0x22
#define IMA4_SAMPLES_PER_BLOCK 0x40

// in:  out buffer, in buffer (IMA4_BLOCK_SIZE bytes), outbuf max size
// return: number of samples decoded
int ima4_decode_block(unsigned short *output, unsigned char *input, int maxlen);

#if 0

typedef struct
{
/* During decoding the work_buffer contains the most recently read chunk. */
/* During encoding the work_buffer contains interlaced overflow samples  */
/* from the last chunk written. */
	int16_t *work_buffer;
	unsigned char *read_buffer;    /* Temporary buffer for drive reads. */

/* Starting information for all channels during encoding. */
	int *last_samples, *last_indexes;
	long chunk; /* Number of chunk in work buffer */
	int buffer_channel; /* Channel of work buffer */

/* Number of samples in largest chunk read. */
/* Number of samples plus overflow in largest chunk write, interlaced. */
	long work_size;     
	long work_overflow; /* Number of overflow samples from the last chunk written. */
	long read_size;     /* Size of read buffer. */
} quicktime_ima4_codec_t;

#endif

#endif
