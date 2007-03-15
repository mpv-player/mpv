/********************************************************

         DirectShow audio decoder
	 Copyright 2001 Eugene Kuznetsov  (divx@euro.ru)

*********************************************************/
#include "config.h"
#ifndef NOAVIFILE_HEADERS
#include "audiodecoder.h"
#include "except.h"
#else
#include "dshow/libwin32.h"
#ifdef WIN32_LOADER
#include "ldt_keeper.h"
#endif
#endif

#include "DMO_Filter.h"
#include "DMO_AudioDecoder.h"

struct _DMO_AudioDecoder
{ 
    DMO_MEDIA_TYPE m_sOurType, m_sDestType;
    DMO_Filter* m_pDMO_Filter;
    char* m_sVhdr;
    char* m_sVhdr2;
    int m_iFlushed;
};

#include "DMO_AudioDecoder.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../mp_msg.h"

typedef long STDCALL (*GETCLASS) (GUID*, GUID*, void**);
extern void print_wave_header(WAVEFORMATEX *h, int verbose_level);

DMO_AudioDecoder * DMO_AudioDecoder_Open(char* dllname, GUID* guid, WAVEFORMATEX* wf,int out_channels)
//DMO_AudioDecoder * DMO_AudioDecoder_Create(const CodecInfo * info, const WAVEFORMATEX* wf)
{
    DMO_AudioDecoder *this;
    int sz;
    WAVEFORMATEX* pWF;

#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
    Setup_FS_Segment();
#endif
        
    this = malloc(sizeof(DMO_AudioDecoder));
    
    this->m_iFlushed=1;
    
    sz = 18 + wf->cbSize;
    this->m_sVhdr = malloc(sz);
    memcpy(this->m_sVhdr, wf, sz);
    this->m_sVhdr2 = malloc(18);
    memcpy(this->m_sVhdr2, this->m_sVhdr, 18);
    
    pWF = (WAVEFORMATEX*)this->m_sVhdr2;
    pWF->wFormatTag = 1;
    pWF->wBitsPerSample = 16;
    pWF->nChannels = out_channels;
    pWF->nBlockAlign = 2*pWF->nChannels; //pWF->nChannels * (pWF->wBitsPerSample + 7) / 8;
    pWF->nAvgBytesPerSec = pWF->nBlockAlign * pWF->nSamplesPerSec;
    pWF->cbSize = 0;
    
    memset(&this->m_sOurType, 0, sizeof(this->m_sOurType));
    this->m_sOurType.majortype=MEDIATYPE_Audio;
    this->m_sOurType.subtype=MEDIASUBTYPE_PCM;
    this->m_sOurType.subtype.f1=wf->wFormatTag;
    this->m_sOurType.formattype=FORMAT_WaveFormatEx;
    this->m_sOurType.lSampleSize=wf->nBlockAlign;
    this->m_sOurType.bFixedSizeSamples=1;
    this->m_sOurType.bTemporalCompression=0;
    this->m_sOurType.cbFormat=sz;
    this->m_sOurType.pbFormat=this->m_sVhdr;

    memset(&this->m_sDestType, 0, sizeof(this->m_sDestType));
    this->m_sDestType.majortype=MEDIATYPE_Audio;
    this->m_sDestType.subtype=MEDIASUBTYPE_PCM;
    this->m_sDestType.formattype=FORMAT_WaveFormatEx;
    this->m_sDestType.bFixedSizeSamples=1;
    this->m_sDestType.bTemporalCompression=0;
    this->m_sDestType.lSampleSize=pWF->nBlockAlign;
    this->m_sDestType.cbFormat=18; //pWF->cbSize;
    this->m_sDestType.pbFormat=this->m_sVhdr2;

print_wave_header((WAVEFORMATEX *)this->m_sVhdr,  MSGL_V);
print_wave_header((WAVEFORMATEX *)this->m_sVhdr2, MSGL_V);

        this->m_pDMO_Filter = DMO_FilterCreate(dllname, guid, &this->m_sOurType, &this->m_sDestType);
	if( !this->m_pDMO_Filter ) {
           free(this);
           return NULL;
        }
        
    return this;
}

void DMO_AudioDecoder_Destroy(DMO_AudioDecoder *this)
{
    free(this->m_sVhdr);
    free(this->m_sVhdr2);
    DMO_Filter_Destroy(this->m_pDMO_Filter);
    free(this);
}

int DMO_AudioDecoder_Convert(DMO_AudioDecoder *this, const void* in_data, unsigned int in_size,
			     void* out_data, unsigned int out_size,
			     unsigned int* size_read, unsigned int* size_written)
{
    DMO_OUTPUT_DATA_BUFFER db;
    CMediaBuffer* bufferin;
    unsigned long written = 0;
    unsigned long read = 0;
    int r = 0;

    if (!in_data || !out_data)
	return -1;

#ifdef WIN32_LOADER
    Setup_FS_Segment();
#endif
    
    //m_pDMO_Filter->m_pMedia->vt->Lock(m_pDMO_Filter->m_pMedia, 1);
    bufferin = CMediaBufferCreate(in_size, (void*)in_data, in_size, 1);
    r = this->m_pDMO_Filter->m_pMedia->vt->ProcessInput(this->m_pDMO_Filter->m_pMedia, 0,
						  (IMediaBuffer*)bufferin,
						  (this->m_iFlushed) ? DMO_INPUT_DATA_BUFFERF_SYNCPOINT : 0,
						  0, 0);
    if (r == 0){
	((IMediaBuffer*)bufferin)->vt->GetBufferAndLength((IMediaBuffer*)bufferin, 0, &read);
	this->m_iFlushed = 0;
    }

    ((IMediaBuffer*)bufferin)->vt->Release((IUnknown*)bufferin);

    //printf("RESULTA: %d 0x%x %ld    %d   %d\n", r, r, read, m_iFlushed, out_size);
    if (r == 0 || (unsigned)r == DMO_E_NOTACCEPTING){
	unsigned long status = 0;
	/* something for process */
	db.rtTimestamp = 0;
	db.rtTimelength = 0;
	db.dwStatus = 0;
	db.pBuffer = (IMediaBuffer*) CMediaBufferCreate(out_size, out_data, 0, 0);
	//printf("OUTSIZE  %d\n", out_size);
	r = this->m_pDMO_Filter->m_pMedia->vt->ProcessOutput(this->m_pDMO_Filter->m_pMedia,
						       0, 1, &db, &status);

	((IMediaBuffer*)db.pBuffer)->vt->GetBufferAndLength((IMediaBuffer*)db.pBuffer, 0, &written);
	((IMediaBuffer*)db.pBuffer)->vt->Release((IUnknown*)db.pBuffer);
 
	//printf("RESULTB: %d 0x%x %ld\n", r, r, written);
	//printf("Converted  %d  -> %d\n", in_size, out_size);
    }
    else if (in_size > 0)
	printf("ProcessInputError  r:0x%x=%d\n", r, r);

    if (size_read)
	*size_read = read;
    if (size_written)
	*size_written = written;
    return r;
}

int DMO_AudioDecoder_GetSrcSize(DMO_AudioDecoder *this, int dest_size)
{
//    unsigned long inputs, outputs;
//    Setup_FS_Segment();
//    this->m_pDMO_Filter->m_pMedia->vt->GetOutputSizeInfo(this->m_pDMO_Filter->m_pMedia, 0, &inputs, &outputs);
    return ((WAVEFORMATEX*)this->m_sVhdr)->nBlockAlign*4;
}
