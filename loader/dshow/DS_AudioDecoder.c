/********************************************************

         DirectShow audio decoder
	 Copyright 2001 Eugene Kuznetsov  (divx@euro.ru)

*********************************************************/

#include "DS_AudioDecoder.h"
#include <string.h>
#include <stdio.h>

// using namespace std;

#define __MODULE__ "DirectShow audio decoder"
const GUID FORMAT_WaveFormatEx = {
    0x05589f81, 0xc356, 0x11CE,
    { 0xBF, 0x01, 0x00, 0xAA, 0x00, 0x55, 0x59, 0x5A }
};
const GUID MEDIATYPE_Audio = {
    0x73647561, 0x0000, 0x0010,
    { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 }
};
const GUID MEDIASUBTYPE_PCM = {
    0x00000001, 0x0000, 0x0010,
    { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 }
};

typedef long STDCALL (*GETCLASS) (GUID*, GUID*, void**);

DS_AudioDecoder::DS_AudioDecoder(const CodecInfo& info, const WAVEFORMATEX* wf)
    : IAudioDecoder(info, wf), m_pDS_Filter(0), m_sVhdr(0), m_sVhdr2(0)
{
    int sz = 18 + wf->cbSize;
    m_sVhdr=new char[sz];
    memcpy(m_sVhdr, wf, sz);
    m_sVhdr2=new char[sz];
    memcpy(m_sVhdr2, m_sVhdr, sz);
    WAVEFORMATEX* pWF=(WAVEFORMATEX*)m_sVhdr2;
    pWF->wFormatTag=1;
    pWF->wBitsPerSample=16;
    pWF->nBlockAlign=2*pWF->nChannels;
    pWF->cbSize=0;
    in_fmt=*wf;

    memset(&m_sOurType, 0, sizeof(m_sOurType));
    m_sOurType.majortype=MEDIATYPE_Audio;
    m_sOurType.subtype=MEDIASUBTYPE_PCM;
    m_sOurType.subtype.f1=wf->wFormatTag;
    m_sOurType.formattype=FORMAT_WaveFormatEx;
    m_sOurType.lSampleSize=wf->nBlockAlign;
    m_sOurType.bFixedSizeSamples=true;
    m_sOurType.bTemporalCompression=false;
    m_sOurType.pUnk=0;
    m_sOurType.cbFormat=sz;
    m_sOurType.pbFormat=m_sVhdr;

    memset(&m_sDestType, 0, sizeof(m_sDestType));
    m_sDestType.majortype=MEDIATYPE_Audio;
    m_sDestType.subtype=MEDIASUBTYPE_PCM;
    m_sDestType.formattype=FORMAT_WaveFormatEx;
    m_sDestType.bFixedSizeSamples=true;
    m_sDestType.bTemporalCompression=false;
    m_sDestType.lSampleSize=2*wf->nChannels;
    m_sDestType.pUnk=0;
    m_sDestType.cbFormat=pWF->cbSize;
    m_sDestType.pbFormat=m_sVhdr2;

    try
    {
        m_pDS_Filter = new DS_Filter();
	m_pDS_Filter->Create(info.dll, &info.guid, &m_sOurType, &m_sDestType);
	m_pDS_Filter->Start();

	ALLOCATOR_PROPERTIES props, props1;
	props.cBuffers=1;
        props.cbBuffer=m_sOurType.lSampleSize;
	props.cbAlign=props.cbPrefix=0;
	m_pDS_Filter->m_pAll->vt->SetProperties(m_pDS_Filter->m_pAll, &props, &props1);
	m_pDS_Filter->m_pAll->vt->Commit(m_pDS_Filter->m_pAll);
    }
    catch (FatalError e)
    {
	e.PrintAll();
	delete[] m_sVhdr;
	delete[] m_sVhdr2;
	delete m_pDS_Filter;
	throw;
    }
}

DS_AudioDecoder::~DS_AudioDecoder()
{
    delete[] m_sVhdr;
    delete[] m_sVhdr2;
    delete m_pDS_Filter;
}

int DS_AudioDecoder::Convert(const void* in_data, uint_t in_size,
			void* out_data, uint_t out_size,
			uint_t* size_read, uint_t* size_written)
{
    if (!in_data || !out_data)
	return -1;

    uint_t written = 0;
    uint_t read = 0;
    in_size -= in_size%in_fmt.nBlockAlign;
    while (in_size>0)
    {
	uint_t frame_size=0;
	char* frame_pointer;
//	m_pOurOutput->SetFramePointer(out_data+written);
	m_pDS_Filter->m_pOurOutput->SetFramePointer(&frame_pointer);
	m_pDS_Filter->m_pOurOutput->SetFrameSizePointer((long*)&frame_size);
	IMediaSample* sample=0;
	m_pDS_Filter->m_pAll->vt->GetBuffer(m_pDS_Filter->m_pAll, &sample, 0, 0, 0);
	if(!sample)
	{
	    Debug printf("DS_AudioDecoder::Convert() Error: null sample\n");
	    break;
	}
	char* ptr;
	sample->vt->GetPointer(sample, (BYTE **)&ptr);
	memcpy(ptr, (const uint8_t*)in_data + read, in_fmt.nBlockAlign);
	sample->vt->SetActualDataLength(sample, in_fmt.nBlockAlign);
	sample->vt->SetSyncPoint(sample, true);
	sample->vt->SetPreroll(sample, 0);
	int result = m_pDS_Filter->m_pImp->vt->Receive(m_pDS_Filter->m_pImp, sample);
        if (result)
	    Debug printf("DS_AudioDecoder::Convert() Error: putting data into input pin %x\n", result);
	if ((written + frame_size) > out_size)
	{
	    sample->vt->Release((IUnknown*)sample);
	    break;
	}
	memcpy((uint8_t*)out_data + written, frame_pointer, frame_size);
        sample->vt->Release((IUnknown*)sample);
	read+=in_fmt.nBlockAlign;
	written+=frame_size;
    }
    if (size_read)
	*size_read = read;
    if (size_written)
	*size_written = written;
    return 0;
}

int DS_AudioDecoder::GetSrcSize(int dest_size)
{
    double efficiency = (double) in_fmt.nAvgBytesPerSec
	/ (in_fmt.nSamplesPerSec*in_fmt.nBlockAlign);
    int frames = int(dest_size*efficiency);
    if (frames < 1)
	frames = 1;
    return frames * in_fmt.nBlockAlign;
}
