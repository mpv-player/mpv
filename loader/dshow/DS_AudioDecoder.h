#ifndef AVIFILE_DSHOW_H
#define AVIFILE_DSHOW_H

#include "libwin32.h"
#include "DS_Filter.h"

class DS_AudioDecoder : public IAudioDecoder
{
public:
    DS_AudioDecoder(const CodecInfo& info, const WAVEFORMATEX*);
    virtual ~DS_AudioDecoder();
    virtual int Convert(const void*, size_t, void*, size_t, size_t*, size_t*);
    virtual int GetSrcSize(int);
protected:
    AM_MEDIA_TYPE m_sOurType, m_sDestType;
    DS_Filter* m_pDS_Filter;
    char* m_sVhdr;
    char* m_sVhdr2;
};

#endif
