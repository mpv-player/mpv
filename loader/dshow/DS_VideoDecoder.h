#ifndef AVIFILE_DS_VIDEODECODER_H
#define AVIFILE_DS_VIDEODECODER_H

#include <libwin32.h>
#include <DS_Filter.h>

class DS_VideoDecoder: public IVideoDecoder, public IRtConfig
{
public:
    DS_VideoDecoder(const CodecInfo& info, const BITMAPINFOHEADER& format, int flip);
    ~DS_VideoDecoder();
    int SetDestFmt(int bits = 24, fourcc_t csp = 0);
    CAPS GetCapabilities() const {return m_Caps;}
    int DecodeInternal(void* src, size_t size, int is_keyframe, CImage* pImage);
    void StartInternal();
    void StopInternal();
    //void Restart();
    int SetDirection(int d)
    {
	m_obh.biHeight = d ? m_bh->biHeight : -m_bh->biHeight;
	m_sVhdr2->bmiHeader.biHeight = m_obh.biHeight;
	return 0;
    }
    // IRtConfig interface
    virtual HRESULT GetValue(const char*, int&);
    virtual HRESULT SetValue(const char*, int);
protected:
    DS_Filter* m_pDS_Filter;
    AM_MEDIA_TYPE m_sOurType, m_sDestType;
    VIDEOINFOHEADER* m_sVhdr;
    VIDEOINFOHEADER* m_sVhdr2;
    CAPS m_Caps;                // capabilities of DirectShow decoder
    int m_iLastQuality;         // remember last quality as integer
    bool m_bIsDivX;             // for speed
};

#endif /* AVIFILE_DS_VIDEODECODER_H */
