/********************************************************

         DirectShow audio decoder
	 Copyright 2001 Eugene Kuznetsov  (divx@euro.ru)

*********************************************************/
#include "config.h"

#ifndef NOAVIFILE_HEADERS
#include "audiodecoder.h"
#include "except.h"
#else
#include "libwin32.h"
#endif

#include "DS_Filter.h"

struct _DS_AudioDecoder
{ 
    WAVEFORMATEX in_fmt;
    AM_MEDIA_TYPE m_sOurType, m_sDestType;
    DS_Filter* m_pDS_Filter;
    char* m_sVhdr;
    char* m_sVhdr2;
};

#include "DS_AudioDecoder.h"
#ifdef WIN32_LOADER
#include "../ldt_keeper.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef long STDCALL (*GETCLASS) (GUID*, GUID*, void**);

static SampleProcUserData sampleProcData;


DS_AudioDecoder * DS_AudioDecoder_Open(char* dllname, GUID* guid, WAVEFORMATEX* wf)
//DS_AudioDecoder * DS_AudioDecoder_Create(const CodecInfo * info, const WAVEFORMATEX* wf)
{
    DS_AudioDecoder *this;
    int sz;
    WAVEFORMATEX* pWF;

#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
    Setup_FS_Segment();
#endif
        
    this = malloc(sizeof(DS_AudioDecoder));
    
    sz = 18 + wf->cbSize;
    this->m_sVhdr = malloc(sz);
    memcpy(this->m_sVhdr, wf, sz);
    this->m_sVhdr2 = malloc(18);
    memcpy(this->m_sVhdr2, this->m_sVhdr, 18);
    
    pWF = (WAVEFORMATEX*)this->m_sVhdr2;
    pWF->wFormatTag = 1;
    pWF->wBitsPerSample = 16;
    pWF->nBlockAlign = pWF->nChannels * (pWF->wBitsPerSample + 7) / 8;
    pWF->cbSize = 0;
    pWF->nAvgBytesPerSec = pWF->nBlockAlign * pWF->nSamplesPerSec;
    
    memcpy(&this->in_fmt,wf,sizeof(WAVEFORMATEX));

    memset(&this->m_sOurType, 0, sizeof(this->m_sOurType));
    this->m_sOurType.majortype=MEDIATYPE_Audio;
    this->m_sOurType.subtype=MEDIASUBTYPE_PCM;
    this->m_sOurType.subtype.f1=wf->wFormatTag;
    this->m_sOurType.formattype=FORMAT_WaveFormatEx;
    this->m_sOurType.lSampleSize=wf->nBlockAlign;
    this->m_sOurType.bFixedSizeSamples=1;
    this->m_sOurType.bTemporalCompression=0;
    this->m_sOurType.pUnk=0;
    this->m_sOurType.cbFormat=sz;
    this->m_sOurType.pbFormat=this->m_sVhdr;

    memset(&this->m_sDestType, 0, sizeof(this->m_sDestType));
    this->m_sDestType.majortype=MEDIATYPE_Audio;
    this->m_sDestType.subtype=MEDIASUBTYPE_PCM;
//    this->m_sDestType.subtype.f1=pWF->wFormatTag;
    this->m_sDestType.formattype=FORMAT_WaveFormatEx;
    this->m_sDestType.bFixedSizeSamples=1;
    this->m_sDestType.bTemporalCompression=0;
    this->m_sDestType.lSampleSize=pWF->nBlockAlign;
    if (wf->wFormatTag == 0x130)
	// ACEL hack to prevent memory corruption
        // obviosly we are missing something here
	this->m_sDestType.lSampleSize *= 288;
    this->m_sDestType.pUnk=0;
    this->m_sDestType.cbFormat=18; //pWF->cbSize;
    this->m_sDestType.pbFormat=this->m_sVhdr2;

//print_wave_header(this->m_sVhdr,  MSGL_V);
//print_wave_header(this->m_sVhdr2, MSGL_V);

    /*try*/
    {
        ALLOCATOR_PROPERTIES props, props1;
        this->m_pDS_Filter = DS_FilterCreate(dllname, guid, &this->m_sOurType, &this->m_sDestType,&sampleProcData);
	if( !this->m_pDS_Filter ) {
           free(this);
           return NULL;
        }
        
        //Commit should be done before binary codec start
        this->m_pDS_Filter->m_pAll->vt->Commit(this->m_pDS_Filter->m_pAll);

        this->m_pDS_Filter->Start(this->m_pDS_Filter);

    }
    /*
    catch (FatalError& e)
    {
	e.PrintAll();
	delete[] m_sVhdr;
	delete[] m_sVhdr2;
	delete m_pDS_Filter;
	throw;
    }
    */
    return this;
}

void DS_AudioDecoder_Destroy(DS_AudioDecoder *this)
{
    free(this->m_sVhdr);
    free(this->m_sVhdr2);
    DS_Filter_Destroy(this->m_pDS_Filter);
    free(this);
}

int DS_AudioDecoder_Convert(DS_AudioDecoder *this, const void* in_data, unsigned int in_size,
			     void* out_data, unsigned int out_size,
			     unsigned int* size_read, unsigned int* size_written)
{
    unsigned int written = 0;
    unsigned int read = 0;
        
    if (!in_data || !out_data)
	return -1;

#ifdef WIN32_LOADER
    Setup_FS_Segment();
#endif

    in_size -= in_size%this->in_fmt.nBlockAlign;
    while (in_size>0)
    {
	IMediaSample* sample=0;
	char* ptr;
	int result;
	this->m_pDS_Filter->m_pAll->vt->GetBuffer(this->m_pDS_Filter->m_pAll, &sample, 0, 0, 0);
	if (!sample)
	{
	    Debug printf("DS_AudioDecoder::Convert() Error: null sample\n");
	    break;
	}
	sample->vt->SetActualDataLength(sample, this->in_fmt.nBlockAlign);
	sample->vt->GetPointer(sample, (BYTE **)&ptr);
	memcpy(ptr, (const uint8_t*)in_data + read, this->in_fmt.nBlockAlign);
	sample->vt->SetSyncPoint(sample, 1);
	sample->vt->SetPreroll(sample, 0);
	result = this->m_pDS_Filter->m_pImp->vt->Receive(this->m_pDS_Filter->m_pImp, sample);
        if (result)
	    Debug printf("DS_AudioDecoder::Convert() Error: putting data into input pin %x\n", result);
	if ((written + sampleProcData.frame_size) > out_size)
	{
	    sample->vt->Release((IUnknown*)sample);
	    break;
	}
	memcpy((uint8_t*)out_data + written, sampleProcData.frame_pointer, sampleProcData.frame_size);
        sample->vt->Release((IUnknown*)sample);
	read+=this->in_fmt.nBlockAlign;
	written+=sampleProcData.frame_size;
	break;
    }
    if (size_read)
	*size_read = read;
    if (size_written)
	*size_written = written;
    return 0;
}

int DS_AudioDecoder_GetSrcSize(DS_AudioDecoder *this, int dest_size)
{
    double efficiency =(double) this->in_fmt.nAvgBytesPerSec
	/ (this->in_fmt.nSamplesPerSec*this->in_fmt.nBlockAlign);
    int frames = (int)(dest_size*efficiency);;
    
    if (frames < 1)
	frames = 1;
    return frames * this->in_fmt.nBlockAlign;
}
