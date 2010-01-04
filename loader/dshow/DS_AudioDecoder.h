#ifndef MPLAYER_DS_AUDIODECODER_H
#define MPLAYER_DS_AUDIODECODER_H

#include "loader/com.h"
#include "loader/wine/mmreg.h"

typedef struct DS_AudioDecoder DS_AudioDecoder;

//DS_AudioDecoder * DS_AudioDecoder_Create(const CodecInfo * info, const WAVEFORMATEX* wf);
DS_AudioDecoder * DS_AudioDecoder_Open(char* dllname, GUID* guid, WAVEFORMATEX* wf);

void DS_AudioDecoder_Destroy(DS_AudioDecoder *this);

int DS_AudioDecoder_Convert(DS_AudioDecoder *this, const void* in_data, unsigned int in_size,
			     void* out_data, unsigned int out_size,
			     unsigned int* size_read, unsigned int* size_written);

int DS_AudioDecoder_GetSrcSize(DS_AudioDecoder *this, int dest_size);

#endif /* MPLAYER_DS_AUDIODECODER_H */
