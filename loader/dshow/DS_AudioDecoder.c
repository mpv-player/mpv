/********************************************************

         DirectShow audio decoder
	 Copyright 2001 Eugene Kuznetsov  (divx@euro.ru)

*********************************************************/

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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

DS_AudioDecoder * DS_AudioDecoder_Open(char* dllname, GUID* guid, WAVEFORMATEX* wf)
//DS_AudioDecoder * DS_AudioDecoder_Create(const CodecInfo * info, const WAVEFORMATEX* wf)
{
    DS_AudioDecoder *this;
    int sz;
    WAVEFORMATEX* pWF;

    Setup_LDT_Keeper();
    Setup_FS_Segment();
        
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

//print_wave_header(this->m_sVhdr);
//print_wave_header(this->m_sVhdr2);

    /*try*/
    {
        ALLOCATOR_PROPERTIES props, props1;
        this->m_pDS_Filter = DS_FilterCreate(dllname, guid, &this->m_sOurType, &this->m_sDestType);
	if( !this->m_pDS_Filter ) {
           free(this);
           return NULL;
        }
        
        this->m_pDS_Filter->Start(this->m_pDS_Filter);

	props.cBuffers=1;
        props.cbBuffer=this->m_sOurType.lSampleSize;
	props.cbAlign=props.cbPrefix=0;
	this->m_pDS_Filter->m_pAll->vt->SetProperties(this->m_pDS_Filter->m_pAll, &props, &props1);
	this->m_pDS_Filter->m_pAll->vt->Commit(this->m_pDS_Filter->m_pAll);
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

    Setup_FS_Segment();

    in_size -= in_size%this->in_fmt.nBlockAlign;
    while (in_size>0)
    {
	unsigned int frame_size = 0;
	char* frame_pointer;
	IMediaSample* sample=0;
	char* ptr;
	int result;
	
//	this->m_pOurOutput->SetFramePointer(out_data+written);
	this->m_pDS_Filter->m_pOurOutput->SetFramePointer(this->m_pDS_Filter->m_pOurOutput,&frame_pointer);
	this->m_pDS_Filter->m_pOurOutput->SetFrameSizePointer(this->m_pDS_Filter->m_pOurOutput,(long*)&frame_size);
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
	if ((written + frame_size) > out_size)
	{
	    sample->vt->Release((IUnknown*)sample);
	    break;
	}
	memcpy((uint8_t*)out_data + written, frame_pointer, frame_size);
        sample->vt->Release((IUnknown*)sample);
	read+=this->in_fmt.nBlockAlign;
	written+=frame_size;
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
