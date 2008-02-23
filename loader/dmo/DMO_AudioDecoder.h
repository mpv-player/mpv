#ifndef MPLAYER_DMO_AUDIODECODER_H
#define MPLAYER_DMO_AUDIODECODER_H

typedef struct DMO_AudioDecoder DMO_AudioDecoder;

//DMO_AudioDecoder * DMO_AudioDecoder_Create(const CodecInfo * info, const WAVEFORMATEX* wf);
DMO_AudioDecoder * DMO_AudioDecoder_Open(char* dllname, GUID* guid, WAVEFORMATEX* wf,int out_channels);

void DMO_AudioDecoder_Destroy(DMO_AudioDecoder *this);

int DMO_AudioDecoder_Convert(DMO_AudioDecoder *this, const void* in_data, unsigned int in_size,
			     void* out_data, unsigned int out_size,
			     unsigned int* size_read, unsigned int* size_written);

int DMO_AudioDecoder_GetSrcSize(DMO_AudioDecoder *this, int dest_size);

#endif /* MPLAYER_DMO_AUDIODECODER_H */
