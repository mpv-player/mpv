/*
 IMA4:1 audio codec from QuickTime4Linux library. (http://www.heroinewarrior.com/)
*/

#include "ima4.h"

static int quicktime_ima4_step[89] = 
{
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static int quicktime_ima4_index[16] = 
{
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

/* ================================== private for ima4 */


void ima4_decode_sample(int *predictor, int *nibble, int *index, int *step)
{
	int difference, sign;

/* Get new index value */
	*index += quicktime_ima4_index[*nibble];

	if(*index < 0) *index = 0; 
	else 
	if(*index > 88) *index = 88;

/* Get sign and magnitude from *nibble */
	sign = *nibble & 8;
	*nibble = *nibble & 7;

/* Get difference */
	difference = *step >> 3;
	if(*nibble & 4) difference += *step;
	if(*nibble & 2) difference += *step >> 1;
	if(*nibble & 1) difference += *step >> 2;

/* Predict value */
	if(sign) 
	*predictor -= difference;
	else 
	*predictor += difference;

	if(*predictor > 32767) *predictor = 32767;
	else
	if(*predictor < -32768) *predictor = -32768;

/* Update the step value */
	*step = quicktime_ima4_step[*index];
}

int ima4_decode_block(unsigned short *output, unsigned char *input, int maxlen)
{
	int predictor;
	int index;
	int step;
	int i, nibble, nibble_count, block_size;
	int olen = 0;
	unsigned char *block_ptr;
	unsigned char *input_end = input + IMA4_BLOCK_SIZE;
//	quicktime_ima4_codec_t *codec = ((quicktime_codec_t*)atrack->codec)->priv;

/* Get the chunk header */
	predictor = *input++ << 8;
	predictor |= *input++;

	index = predictor & 0x7f;
	if(index > 88) index = 88;

	predictor &= 0xff80;
	if(predictor & 0x8000) predictor -= 0x10000;
	step = quicktime_ima4_step[index];

/* Read the input buffer sequentially, one nibble at a time */
	nibble_count = 0;
	while(input < input_end)
	{
		nibble = nibble_count ? (*input++  >> 4) & 0x0f : *input & 0x0f;

		ima4_decode_sample(&predictor, &nibble, &index, &step);
		if (olen+1 > maxlen)
		    break;
		*output++ = predictor;
		olen++;

		nibble_count ^= 1;
	}
	return(olen);
}

#if 0
void ima4_encode_sample(int *last_sample, int *last_index, int *nibble, int next_sample)
{
	int difference, new_difference, mask, step;

	difference = next_sample - *last_sample;
	*nibble = 0;
	step = quicktime_ima4_step[*last_index];
	new_difference = step >> 3;

	if(difference < 0)
	{
		*nibble = 8;
		difference = -difference;
	}

	mask = 4;
	while(mask)
	{
		if(difference >= step)
		{
			*nibble |= mask;
			difference -= step;
			new_difference += step;
		}

		step >>= 1;
		mask >>= 1;
	}

	if(*nibble & 8)
		*last_sample -= new_difference;
	else
		*last_sample += new_difference;

	if(*last_sample > 32767) *last_sample = 32767;
	else
	if(*last_sample < -32767) *last_sample = -32767;

	*last_index += quicktime_ima4_index[*nibble];

	if(*last_index < 0) *last_index = 0;
	else
	if(*last_index > 88) *last_index= 88;
}

#if 0
void ima4_encode_block(quicktime_audio_map_t *atrack, unsigned char *output, int16_t *input, int step, int channel)
{
	quicktime_ima4_codec_t *codec = ((quicktime_codec_t*)atrack->codec)->priv;
	int i, nibble_count = 0, nibble, header;

/* Get a fake starting sample */
	header = codec->last_samples[channel];
/* Force rounding. */
	if(header < 0x7fc0) header += 0x40;
	if(header < 0) header += 0x10000;
	header &= 0xff80;
	*output++ = (header & 0xff00) >> 8;
	*output++ = (header & 0x80) + (codec->last_indexes[channel] & 0x7f);

	for(i = 0; i < SAMPLES_PER_BLOCK; i++)
	{
		ima4_encode_sample(&(codec->last_samples[channel]), 
							&(codec->last_indexes[channel]), 
							&nibble, 
							*input);

		if(nibble_count)
			*output++ |= (nibble << 4);
		else
			*output = nibble;

		input += step;
		nibble_count ^= 1;
	}
}
#endif
/* Convert the number of samples in a chunk into the number of bytes in that */
/* chunk.  The number of samples in a chunk should end on a block boundary. */
long ima4_samples_to_bytes(long samples, int channels)
{
	long bytes = samples / SAMPLES_PER_BLOCK * BLOCK_SIZE * channels;
	return bytes;
}

/* Decode the chunk into the work buffer */
int ima4_decode_chunk(quicktime_t *file, int track, long chunk, int channel)
{
	int result = 0;
	int i, j;
	long chunk_samples, chunk_bytes;
	unsigned char *chunk_ptr, *block_ptr;
	quicktime_trak_t *trak = file->atracks[track].track;
	quicktime_ima4_codec_t *codec = ((quicktime_codec_t*)file->atracks[track].codec)->priv;

/* Get the byte count to read. */
	chunk_samples = quicktime_chunk_samples(trak, chunk);
	chunk_bytes = ima4_samples_to_bytes(chunk_samples, file->atracks[track].channels);

/* Get the buffer to read into. */
	if(codec->work_buffer && codec->work_size < chunk_samples)
	{
		free(codec->work_buffer);
		codec->work_buffer = 0;
	}

	if(!codec->work_buffer)
	{
		codec->work_size = chunk_samples;
		codec->work_buffer = malloc(sizeof(int16_t) * codec->work_size);
	}

	if(codec->read_buffer && codec->read_size < chunk_bytes)
	{
		free(codec->read_buffer);
		codec->read_buffer = 0;
	}

	if(!codec->read_buffer)
	{
		codec->read_size = chunk_bytes;
		codec->read_buffer = malloc(codec->read_size);
	}

/* codec->work_size now holds the number of samples in the last chunk */
/* codec->read_size now holds number of bytes in the last read buffer */

/* Read the entire chunk regardless of where the desired sample range starts. */
	result = quicktime_read_chunk(file, codec->read_buffer, track, chunk, 0, chunk_bytes);

/* Now decode the chunk, one block at a time, until the total samples in the chunk */
/* is reached. */

	if(!result)
	{
		block_ptr = codec->read_buffer;
		for(i = 0; i < chunk_samples; i += SAMPLES_PER_BLOCK)
		{
			for(j = 0; j < file->atracks[track].channels; j++)
			{
				if(j == channel)
					ima4_decode_block(&(file->atracks[track]), &(codec->work_buffer[i]), block_ptr);

				block_ptr += BLOCK_SIZE;
			}
		}
	}
	codec->buffer_channel = channel;
	codec->chunk = chunk;

	return result;
}


/* =================================== public for ima4 */

static int quicktime_delete_codec_ima4(quicktime_audio_map_t *atrack)
{
	quicktime_ima4_codec_t *codec = ((quicktime_codec_t*)atrack->codec)->priv;

	if(codec->work_buffer) free(codec->work_buffer);
	if(codec->read_buffer) free(codec->read_buffer);
	if(codec->last_samples) free(codec->last_samples);
	if(codec->last_indexes) free(codec->last_indexes);
	codec->last_samples = 0;
	codec->last_indexes = 0;
	codec->read_buffer = 0;
	codec->work_buffer = 0;
	codec->chunk = 0;
	codec->buffer_channel = 0; /* Channel of work buffer */
	codec->work_size = 0;          /* Size of work buffer */
	codec->read_size = 0;
	free(codec);
	return 0;
}

static int quicktime_decode_ima4(quicktime_t *file, 
					int16_t *output_i, 
					float *output_f,
					long samples, 
					int track, 
					int channel)
{
	int result = 0;
	longest chunk, chunk_sample, chunk_bytes, chunk_samples;
	longest i, chunk_start, chunk_end;
	quicktime_trak_t *trak = file->atracks[track].track;
	quicktime_ima4_codec_t *codec = ((quicktime_codec_t*)file->atracks[track].codec)->priv;

/* Get the first chunk with this routine and then increase the chunk number. */
	quicktime_chunk_of_sample(&chunk_sample, &chunk, trak, file->atracks[track].current_position);

/* Read chunks and extract ranges of samples until the output is full. */
	for(i = 0; i < samples && !result; )
	{
/* Get chunk we're on. */
		chunk_samples = quicktime_chunk_samples(trak, chunk);

		if(!codec->work_buffer ||
			codec->chunk != chunk ||
			codec->buffer_channel != channel)
		{
/* read a new chunk if necessary */
			result = ima4_decode_chunk(file, track, chunk, channel);
		}

/* Get boundaries from the chunk */
		chunk_start = 0;
		if(chunk_sample < file->atracks[track].current_position)
			chunk_start = file->atracks[track].current_position - chunk_sample;

		chunk_end = chunk_samples;
		if(chunk_sample + chunk_end > file->atracks[track].current_position + samples)
			chunk_end = file->atracks[track].current_position + samples - chunk_sample;

/* Read from the chunk */
		if(output_i)
		{
/*printf("decode_ima4 1 chunk %ld %ld-%ld output %ld\n", chunk, chunk_start + chunk_sample, chunk_end + chunk_sample, i); */
			while(chunk_start < chunk_end)
			{
				output_i[i++] = codec->work_buffer[chunk_start++];
			}
/*printf("decode_ima4 2\n"); */
		}
		else
		if(output_f)
		{
			while(chunk_start < chunk_end)
			{
				output_f[i++] = (float)codec->work_buffer[chunk_start++] / 32767;
			}
		}

		chunk++;
		chunk_sample += chunk_samples;
	}

	return result;
}

static int quicktime_encode_ima4(quicktime_t *file, 
						int16_t **input_i, 
						float **input_f, 
						int track, 
						long samples)
{
	int result = 0;
	longest i, j, step;
	longest chunk_bytes;
	longest overflow_start;
	longest offset;
	longest chunk_samples; /* Samples in the current chunk to be written */
	quicktime_audio_map_t *track_map = &(file->atracks[track]);
	quicktime_ima4_codec_t *codec = ((quicktime_codec_t*)track_map->codec)->priv;
	int16_t *input_ptr;
	unsigned char *output_ptr;

/* Get buffer sizes */
	if(codec->work_buffer && codec->work_size < (samples + codec->work_overflow + 1) * track_map->channels)
	{
/* Create new buffer */
		longest new_size = (samples + codec->work_overflow + 1) * track_map->channels;
		int16_t *new_buffer = malloc(sizeof(int16_t) * new_size);

/* Copy overflow */
		for(i = 0; i < codec->work_overflow * track_map->channels; i++)
			new_buffer[i] = codec->work_buffer[i];

/* Swap pointers. */
		free(codec->work_buffer);
		codec->work_buffer = new_buffer;
		codec->work_size = new_size;
	}
	else
	if(!codec->work_buffer)
	{
/* No buffer in the first place. */
		codec->work_size = (samples + codec->work_overflow) * track_map->channels;
/* Make the allocation enough for at least the flush routine. */
		if(codec->work_size < SAMPLES_PER_BLOCK * track_map->channels)
			codec->work_size = SAMPLES_PER_BLOCK * track_map->channels;
		codec->work_buffer = malloc(sizeof(int16_t) * codec->work_size);
	}

/* Get output size */
	chunk_bytes = ima4_samples_to_bytes(samples + codec->work_overflow, track_map->channels);
	if(codec->read_buffer && codec->read_size < chunk_bytes)
	{
		free(codec->read_buffer);
		codec->read_buffer = 0;
	}

	if(!codec->read_buffer)
	{
		codec->read_buffer = malloc(chunk_bytes);
		codec->read_size = chunk_bytes;
	}

	if(!codec->last_samples)
	{
		codec->last_samples = malloc(sizeof(int) * track_map->channels);
		for(i = 0; i < track_map->channels; i++)
		{
			codec->last_samples[i] = 0;
		}
	}

	if(!codec->last_indexes)
	{
		codec->last_indexes = malloc(sizeof(int) * track_map->channels);
		for(i = 0; i < track_map->channels; i++)
		{
			codec->last_indexes[i] = 0;
		}
	}

/* Arm the input buffer after the last overflow */
	step = track_map->channels;
	for(j = 0; j < track_map->channels; j++)
	{
		input_ptr = codec->work_buffer + codec->work_overflow * track_map->channels + j;

		if(input_i)
		{
			for(i = 0; i < samples; i++)
			{
				*input_ptr = input_i[j][i];
				input_ptr += step;
			}
		}
		else
		if(input_f)
		{
			for(i = 0; i < samples; i++)
			{
				*input_ptr = (int16_t)(input_f[j][i] * 32767);
				input_ptr += step;
			}
		}
	}

/* Encode from the input buffer to the read_buffer up to a multiple of  */
/* blocks. */
	input_ptr = codec->work_buffer;
	output_ptr = codec->read_buffer;

	for(i = 0; 
		i + SAMPLES_PER_BLOCK <= samples + codec->work_overflow; 
		i += SAMPLES_PER_BLOCK)
	{
		for(j = 0; j < track_map->channels; j++)
		{
			ima4_encode_block(track_map, output_ptr, input_ptr + j, track_map->channels, j);

			output_ptr += BLOCK_SIZE;
		}
		input_ptr += SAMPLES_PER_BLOCK * track_map->channels;
	}

/* Write to disk */
	chunk_samples = (longest)((samples + codec->work_overflow) / SAMPLES_PER_BLOCK) * SAMPLES_PER_BLOCK;

/*printf("quicktime_encode_ima4 1 %ld\n", chunk_samples); */
/* The block division may result in 0 samples getting encoded. */
/* Don't write 0 samples. */
	if(chunk_samples)
	{
		offset = quicktime_position(file);
		result = quicktime_write_data(file, codec->read_buffer, chunk_bytes);
		if(result) result = 0; else result = 1; /* defeat fwrite's return */
		quicktime_update_tables(file,
							track_map->track, 
							offset, 
							track_map->current_chunk, 
							track_map->current_position, 
							chunk_samples, 
							0);
		file->atracks[track].current_chunk++;
	}

/* Move the last overflow to the front */
	overflow_start = i;
	input_ptr = codec->work_buffer;
	for(i = overflow_start * track_map->channels ; 
		i < (samples + codec->work_overflow) * track_map->channels; 
		i++)
	{
		*input_ptr++ = codec->work_buffer[i];
	}
	codec->work_overflow = samples + codec->work_overflow - overflow_start;

	return result;
}

int quicktime_flush_ima4(quicktime_t *file, int track)
{
	quicktime_audio_map_t *track_map = &(file->atracks[track]);
	quicktime_ima4_codec_t *codec = ((quicktime_codec_t*)track_map->codec)->priv;
	int result = 0;
	int i;

/*printf("quicktime_flush_ima4 %ld\n", codec->work_overflow); */
	if(codec->work_overflow)
	{
/* Zero out enough to get a block */
		i = codec->work_overflow * track_map->channels;
		while(i < SAMPLES_PER_BLOCK * track_map->channels)
		{
			codec->work_buffer[i++] = 0;
		}
		codec->work_overflow = i / track_map->channels + 1;
/* Write the work_overflow only. */
		result = quicktime_encode_ima4(file, 0, 0, track, 0);
	}
	return result;
}

void quicktime_init_codec_ima4(quicktime_audio_map_t *atrack)
{
	quicktime_ima4_codec_t *codec;

/* Init public items */
	((quicktime_codec_t*)atrack->codec)->priv = calloc(1, sizeof(quicktime_ima4_codec_t));
	((quicktime_codec_t*)atrack->codec)->delete_acodec = quicktime_delete_codec_ima4;
	((quicktime_codec_t*)atrack->codec)->decode_video = 0;
	((quicktime_codec_t*)atrack->codec)->encode_video = 0;
	((quicktime_codec_t*)atrack->codec)->decode_audio = quicktime_decode_ima4;
	((quicktime_codec_t*)atrack->codec)->encode_audio = quicktime_encode_ima4;

/* Init private items */
	codec = ((quicktime_codec_t*)atrack->codec)->priv;
	codec->work_buffer = 0;
	codec->read_buffer = 0;
	codec->chunk = 0;
	codec->buffer_channel = 0;
	codec->work_overflow = 0;
	codec->work_size = 0;
	codec->read_size = 0;
	codec->last_samples = 0;
	codec->last_indexes = 0;
}
#endif
