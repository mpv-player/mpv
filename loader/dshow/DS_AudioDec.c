/********************************************************

	DirectShow Audio decoder implementation
	Copyright 2000 Eugene Kuznetsov  (divx@euro.ru)
        Converted  C++ --> C  :) by A'rpi/ESP-team

*********************************************************/

//#include <config.h>

//#include "DS_AudioDecoder.h"
//#include <string.h>
using namespace std;
#include <stdlib.h>
#include <except.h>
#define __MODULE__ "DirectShow_AudioDecoder"

#include <errno.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
//#include <loader.h>
//#include <wine/winbase.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <strstream>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <registry.h>
#include <wine/winreg.h>

#include "guids.h"
#include "interfaces.h"
#include "DS_Filter.h"

#include "BitmapInfo.h"

#include <string>
#include <default.h>

#include "DS_AudioDec.h"

const GUID FORMAT_WaveFormatEx={
0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a};
const GUID MEDIATYPE_Audio={
0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
const GUID MEDIASUBTYPE_PCM={
0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};


extern "C" char* def_path;

    static DS_Filter* dsf=0;

    static AM_MEDIA_TYPE m_sOurType, m_sDestType;
//    static void* m_pCust;
    static char* m_sVhdr;
    static char* m_sVhdr2;

    static WAVEFORMATEX in_fmt;

//    int m_iState=0;

extern "C" int DS_AudioDecoder_Open(char* dllname, GUID* guid, WAVEFORMATEX* wf)
{

    m_sVhdr=new char[18+wf->cbSize];
    memcpy(m_sVhdr, wf, 18+wf->cbSize);
    m_sVhdr2=new char[18];
    memcpy(m_sVhdr2, m_sVhdr, 18);
    WAVEFORMATEX* pWF=(WAVEFORMATEX*)m_sVhdr2;
    pWF->wFormatTag=1;
    pWF->wBitsPerSample=16;
    pWF->nBlockAlign=2*pWF->nChannels;
    pWF->cbSize=0;
    in_fmt=*wf;

    memset(&m_sOurType, 0, sizeof m_sOurType);
    m_sOurType.majortype=MEDIATYPE_Audio;
    m_sOurType.subtype=MEDIASUBTYPE_PCM;
    m_sOurType.subtype.f1=wf->wFormatTag;
    m_sOurType.formattype=FORMAT_WaveFormatEx;
    m_sOurType.lSampleSize=wf->nBlockAlign;
    m_sOurType.bFixedSizeSamples=true;
    m_sOurType.bTemporalCompression=false;
    m_sOurType.pUnk=0;
    m_sOurType.cbFormat=18+wf->cbSize;
    m_sOurType.pbFormat=m_sVhdr;

    memset(&m_sDestType, 0, sizeof m_sDestType);
    m_sDestType.majortype=MEDIATYPE_Audio;
    m_sDestType.subtype=MEDIASUBTYPE_PCM;
    m_sDestType.formattype=FORMAT_WaveFormatEx;
    m_sDestType.bFixedSizeSamples=true;
    m_sDestType.bTemporalCompression=false;
    m_sDestType.lSampleSize=2*wf->nChannels;
    m_sDestType.pUnk=0;
    m_sDestType.cbFormat=18;
    m_sDestType.pbFormat=m_sVhdr2;

    try
    {

        dsf=new DS_Filter();
	dsf->Create(dllname, guid, &m_sOurType, &m_sDestType);
        dsf->Start();

	ALLOCATOR_PROPERTIES props, props1;
	props.cBuffers=1;
        props.cbBuffer=m_sOurType.lSampleSize;
	props.cbAlign=props.cbPrefix=0;
	dsf->m_pAll->vt->SetProperties(dsf->m_pAll, &props, &props1);
	dsf->m_pAll->vt->Commit(dsf->m_pAll);
    }
    catch(FatalError e)
    {
	e.PrintAll();
	delete[] m_sVhdr;
	delete[] m_sVhdr2;
        return 1;
    }

    return 0;
}

extern "C" void DS_AudioDecoder_Close(){
    delete[] m_sVhdr;
    delete[] m_sVhdr2;
}

extern "C" int DS_AudioDecoder_GetSrcSize(int dest_size)
{
    double efficiency=in_fmt.nAvgBytesPerSec/double(in_fmt.nSamplesPerSec*in_fmt.nBlockAlign);
    int frames=(int)(dest_size*efficiency);
    if(frames<1)frames=1;
    return frames*in_fmt.nBlockAlign;
}


extern "C" int DS_AudioDecoder_Convert(unsigned char* in_data, unsigned in_size,
	     unsigned char* out_data, unsigned out_size,
	    unsigned* size_read, unsigned* size_written)
{
    if(in_data==0)return -1;
    if(out_data==0)return -1;
    int written=0;
    int read=0;
    in_size-=in_size%in_fmt.nBlockAlign;
    while(in_size>0)
    {
	long frame_size=0;
	char* frame_pointer;
//	m_pOurOutput->SetFramePointer(out_data+written);
	dsf->m_pOurOutput->SetFramePointer(&frame_pointer);
	dsf->m_pOurOutput->SetFrameSizePointer(&frame_size);
	IMediaSample* sample=0;
	dsf->m_pAll->vt->GetBuffer(dsf->m_pAll, &sample, 0, 0, 0);
	if(!sample)
	{
	    cerr<<"ERROR: null sample"<<endl;
	    break;
	}
	char* ptr;
	sample->vt->GetPointer(sample, (BYTE **)&ptr);
	memcpy(ptr, in_data+read, in_fmt.nBlockAlign);
	sample->vt->SetActualDataLength(sample, in_fmt.nBlockAlign);
	sample->vt->SetSyncPoint(sample, true);
	sample->vt->SetPreroll(sample, 0);
        int result=dsf->m_pImp->vt->Receive(dsf->m_pImp, sample);
        if(result) printf("Error putting data into input pin %x\n", result);
	if(written+frame_size>out_size)
	{
	    sample->vt->Release((IUnknown*)sample);
	    break;
	}
	memcpy(out_data+written, frame_pointer, frame_size);
        sample->vt->Release((IUnknown*)sample);
	read+=in_fmt.nBlockAlign;
	written+=frame_size;
    }
    if(size_read) *size_read=read;
    if(size_written) *size_written=written;
    return 0;
}

