#ifndef AVIFILE_DS_AUDIODECODER_H
#define AVIFILE_DS_AUDIODECODER_H

#ifndef NOAVIFILE_HEADERS
#include "audiodecoder.h"
#include "except.h"
#else
#include "libwin32.h"
#endif
#include "DS_Filter.h"

typedef struct _DS_AudioDecoder
{ 
    WAVEFORMATEX in_fmt;
    AM_MEDIA_TYPE m_sOurType, m_sDestType;
    DS_Filter* m_pDS_Filter;
    char* m_sVhdr;
    char* m_sVhdr2;
}DS_AudioDecoder;

#ifndef uint_t
#define uint_t int
#endif

//DS_AudioDecoder * DS_AudioDecoder_Create(const CodecInfo * info, const WAVEFORMATEX* wf);
DS_AudioDecoder * DS_AudioDecoder_Open(char* dllname, GUID* guid, WAVEFORMATEX* wf);

void DS_AudioDecoder_Destroy(DS_AudioDecoder *this);

int DS_AudioDecoder_Convert(DS_AudioDecoder *this, const void* in_data, uint_t in_size,
			     void* out_data, uint_t out_size,
			     uint_t* size_read, uint_t* size_written);

int DS_AudioDecoder_GetSrcSize(DS_AudioDecoder *this, int dest_size);


#endif // AVIFILE_DS_AUDIODECODER_H
