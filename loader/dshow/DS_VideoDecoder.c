/********************************************************

	DirectShow Video decoder implementation
	Copyright 2000 Eugene Kuznetsov  (divx@euro.ru)

*********************************************************/

#include "guids.h"
#include "interfaces.h"

#include "DS_VideoDecoder.h"
#include <wine/winerror.h>
#include <libwin32.h>
//#include <cpuinfo.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <cstdio>
#include <iostream>
#include <strstream>

#define __MODULE__ "DirectShow_VideoDecoder"

extern "C" char* def_path;
extern "C" void setup_FS_Segment();

using namespace std;

DS_VideoDecoder::DS_VideoDecoder(const CodecInfo& info, const BITMAPINFOHEADER& format, int flip)
    :IVideoDecoder(info, format)
{
    m_sVhdr2 = 0;
    m_iLastQuality = -1;
    //memset(&m_obh, 0, sizeof(m_obh));
    //m_obh.biSize = sizeof(m_obh);
    try
    {
	m_pDS_Filter = new DS_Filter();

	unsigned bihs = (format.biSize < (int) sizeof(BITMAPINFOHEADER)) ?
	    sizeof(BITMAPINFOHEADER) : format.biSize;
        bihs = sizeof(VIDEOINFOHEADER) - sizeof(BITMAPINFOHEADER) + bihs;

	m_sVhdr = (VIDEOINFOHEADER*) new char[bihs];
	memset(m_sVhdr, 0, bihs);
	memcpy(&m_sVhdr->bmiHeader, m_bh, m_bh->biSize);
	m_sVhdr->rcSource.left = m_sVhdr->rcSource.top = 0;
	m_sVhdr->rcSource.right = m_sVhdr->bmiHeader.biWidth;
        m_sVhdr->rcSource.bottom = m_sVhdr->bmiHeader.biHeight;
	m_sVhdr->rcTarget = m_sVhdr->rcSource;

	m_sOurType.majortype = MEDIATYPE_Video;
	m_sOurType.subtype = MEDIATYPE_Video;
        m_sOurType.subtype.f1 = m_sVhdr->bmiHeader.biCompression;
	m_sOurType.formattype = FORMAT_VideoInfo;
        m_sOurType.bFixedSizeSamples = false;
	m_sOurType.bTemporalCompression = true;
	m_sOurType.pUnk = 0;
        m_sOurType.cbFormat = bihs;
        m_sOurType.pbFormat = (char*)m_sVhdr;

	m_sVhdr2 = (VIDEOINFOHEADER*)(new char[sizeof(VIDEOINFOHEADER)+12]);
	memcpy(m_sVhdr2, m_sVhdr, sizeof(VIDEOINFOHEADER));
	memset((char*)m_sVhdr2 + sizeof(VIDEOINFOHEADER), 0, 12);
	m_sVhdr2->bmiHeader.biCompression = 0;
	m_sVhdr2->bmiHeader.biBitCount = 24;

	memset(&m_sDestType, 0, sizeof(m_sDestType));
	m_sDestType.majortype = MEDIATYPE_Video;
	m_sDestType.subtype = MEDIASUBTYPE_RGB24;
	m_sDestType.formattype = FORMAT_VideoInfo;
	m_sDestType.bFixedSizeSamples = true;
	m_sDestType.bTemporalCompression = false;
	m_sDestType.lSampleSize = abs(m_sVhdr2->bmiHeader.biWidth*m_sVhdr2->bmiHeader.biHeight
				      * ((m_sVhdr2->bmiHeader.biBitCount + 7) / 8));
	m_sVhdr2->bmiHeader.biSizeImage = m_sDestType.lSampleSize;
	m_sDestType.pUnk = 0;
	m_sDestType.cbFormat = sizeof(VIDEOINFOHEADER);
        m_sDestType.pbFormat = (char*)m_sVhdr2;
	memset(&m_obh, 0, sizeof(m_obh));
	memcpy(&m_obh, m_bh, sizeof(m_obh) < m_bh->biSize ? sizeof(m_obh) : m_bh->biSize);
	m_obh.SetBits(24);

	HRESULT result;

	m_pDS_Filter->Create(info.dll.c_str(), &info.guid, &m_sOurType, &m_sDestType);

	if (!flip)
	{
	    m_sVhdr2->bmiHeader.biHeight *= -1;
	    m_obh.biHeight *= -1;
	    result = m_pDS_Filter->m_pOutputPin->vt->QueryAccept(m_pDS_Filter->m_pOutputPin, &m_sDestType);
	    if (result)
	    {
		cerr << "Decoder does not support upside-down frames" << endl;
		m_sVhdr2->bmiHeader.biHeight *= -1;
		m_obh.biHeight *= -1;
	    }
	}

	m_decoder = m_obh;

	switch (m_bh->biCompression)
	{
	case fccDIV3:
	case fccDIV4:
	case fccDIV5:
	case fccMP42:
	case fccWMV2:
	    //YV12 seems to be broken for DivX :-) codec
	case fccIV50:
	    //produces incorrect picture
	    //m_Caps = (CAPS) (m_Caps & ~CAP_YV12);
	    m_Caps = CAP_YUY2; // | CAP_I420;
	    break;
	default:
	    struct ct {
		unsigned int bits;
		fourcc_t fcc;
		GUID subtype;
		CAPS cap;
	    } check[] = {
		{16, fccYUY2, MEDIASUBTYPE_YUY2, CAP_YUY2},
		{12, fccIYUV, MEDIASUBTYPE_IYUV, CAP_IYUV},
		{16, fccUYVY, MEDIASUBTYPE_UYVY, CAP_UYVY},
		{12, fccYV12, MEDIASUBTYPE_YV12, CAP_YV12},
		{16, fccYV12, MEDIASUBTYPE_YV12, CAP_YV12},
		{16, fccYVYU, MEDIASUBTYPE_YVYU, CAP_YVYU},
		//{12, fccI420, MEDIASUBTYPE_I420, CAP_I420},
		{0},
	    };

	    m_Caps = CAP_NONE;

	    for (ct* c = check; c->bits; c++)
	    {
		m_sVhdr2->bmiHeader.biBitCount = c->bits;
		m_sVhdr2->bmiHeader.biCompression = c->fcc;
		m_sDestType.subtype = c->subtype;
		result = m_pDS_Filter->m_pOutputPin->vt->QueryAccept(m_pDS_Filter->m_pOutputPin, &m_sDestType);
		if (!result)
		    m_Caps = (CAPS)(m_Caps | c->cap);
	    }
	}

	if (m_Caps != CAP_NONE)
	    cout << "Decoder is capable of YUV output ( flags 0x"<<hex<<(int)m_Caps<<dec<<" )"<<endl;

	m_sVhdr2->bmiHeader.biBitCount = 24;
	m_sVhdr2->bmiHeader.biCompression = 0;
	m_sDestType.subtype = MEDIASUBTYPE_RGB24;

	m_bIsDivX = ((info.dll == string("divxcvki.ax"))
		     || (info.dll == string("divx_c32.ax"))
		     || (info.dll == string("wmvds32.ax"))
		     || (info.dll == string("wmv8ds32.ax")));

	printf("m_bIsDivX=%d\n",m_bIsDivX);
    }
    catch (FatalError& error)
    {
	delete[] m_sVhdr2;
        delete m_pDS_Filter;
	throw;
    }
}

DS_VideoDecoder::~DS_VideoDecoder()
{
    Stop();
    delete[] m_sVhdr2;
    delete m_pDS_Filter;
}

void DS_VideoDecoder::StartInternal()
{
    //cout << "DSSTART" << endl;
    m_pDS_Filter->Start();
    ALLOCATOR_PROPERTIES props, props1;
    props.cBuffers = 1;
    props.cbBuffer = m_sDestType.lSampleSize;
    //don't know how to do this correctly
    props.cbAlign = props.cbPrefix = 0;
    m_pDS_Filter->m_pAll->vt->SetProperties(m_pDS_Filter->m_pAll, &props, &props1);
    m_pDS_Filter->m_pAll->vt->Commit(m_pDS_Filter->m_pAll);
}

void DS_VideoDecoder::StopInternal()
{
    //cout << "DSSTOP" << endl;
    m_pDS_Filter->Stop();
    //??? why was this here ??? m_pOurOutput->SetFramePointer(0);
}

int DS_VideoDecoder::DecodeInternal(void* src, size_t size, int is_keyframe, CImage* pImage)
{
    IMediaSample* sample = 0;

    m_pDS_Filter->m_pAll->vt->GetBuffer(m_pDS_Filter->m_pAll, &sample, 0, 0, 0);

    if (!sample)
    {
	Debug cerr << "ERROR: null sample" << endl;
	return -1;
    }

    //cout << "DECODE " << (void*) pImage << "   d: " << (void*) pImage->Data() << endl;
    if (pImage)
    {
	if (!(pImage->Data()))
	{
	    Debug cout << "no m_outFrame??" << endl;
	}
	else
	    m_pDS_Filter->m_pOurOutput->SetPointer2((char*)pImage->Data());
    }

    char* ptr;
    sample->vt->GetPointer(sample, (BYTE **)&ptr);
    memcpy(ptr, src, size);
    sample->vt->SetActualDataLength(sample, size);
    sample->vt->SetSyncPoint(sample, is_keyframe);
    sample->vt->SetPreroll(sample, pImage ? 0 : 1);
    // sample->vt->SetMediaType(sample, &m_sOurType);

    // FIXME: - crashing with YV12 at this place decoder will crash
    //          while doing this call
    // %FS register was not setup for calling into win32 dll. Are all
    // crashes inside ...->Receive() fixed now?
    //
    // nope - but this is surely helpfull - I'll try some more experiments
    setup_FS_Segment();
#if 0
    if (!m_pDS_Filter || !m_pDS_Filter->m_pImp
	|| !m_pDS_Filter->m_pImp->vt
	|| !m_pDS_Filter->m_pImp->vt->Receive)
	printf("DecodeInternal ERROR???\n");
#endif
    int result = m_pDS_Filter->m_pImp->vt->Receive(m_pDS_Filter->m_pImp, sample);
    if (result)
    {
	Debug printf("DS_VideoDecoder::DecodeInternal() error putting data into input pin %x\n", result);
    }

    sample->vt->Release((IUnknown*)sample);

    if (m_bIsDivX)
    {
	int q;
	IHidden* hidden=(IHidden*)((int)m_pDS_Filter->m_pFilter + 0xb8);
	// always check for actual value
	// this seems to be the only way to know the actual value
	hidden->vt->GetSmth2(hidden, &m_iLastQuality);
	if (m_iLastQuality > 9)
	    m_iLastQuality -= 10;

	if (m_iLastQuality < 0)
	    m_iLastQuality = 0;
	else if (m_iLastQuality > 4)
	    m_iLastQuality = 4;

	//cout << " Qual: " << m_iLastQuality << endl;
	m_fQuality = m_iLastQuality / 4.0;
    }

    // FIXME: last_quality currently works only for DivX movies
    // more general approach is needed here
    // cout << "Q: " << m_iLastQuality << "  rt: " << m_Mode << " dp: " << decpos << endl;
    // also accesing vbuf doesn't look very nice at this place
    // FIXME later - do it as a virtual method

    if (m_Mode == IVideoDecoder::REALTIME_QUALITY_AUTO)
    {
	// adjust Quality - depends on how many cached frames we have
	int buffered = m_iDecpos - m_iPlaypos;

	if (m_bIsDivX)
	{
	    //cout << "qual " << q << "  " << buffered << endl;
	    if (buffered < (m_iLastQuality * 2 + QMARKLO - 1)
		|| buffered > ((m_iLastQuality + 1) * 2 + QMARKLO))
	    {
		// removed old code which was present here
		// and replaced with this new uptodate one

		int to = (buffered - QMARKLO) / 2;
		if (to < 0)
		    to = 0;
		else if (to > 4)
		    to = 4;
		if (m_iLastQuality != to)
		{
		    IHidden* hidden=(IHidden*)((int)m_pDS_Filter->m_pFilter + 0xb8);
		    hidden->vt->SetSmth(hidden, to, 0);
#ifndef QUIET
		    //cout << "Switching quality " << m_iLastQuality <<  " -> " << to << "  b:" << buffered << endl;
#endif
		}
	    }
	}
    }


    return 0;
}

/*
 * bits == 0   - leave unchanged
 */
int DS_VideoDecoder::SetDestFmt(int bits, fourcc_t csp)
{
    if (!CImage::Supported(csp, bits))
	return -1;

    // BitmapInfo temp = m_obh;
    if (bits != 0)
    {
	bool ok = true;

	switch (bits)
        {
	case 15:
	    m_sDestType.subtype = MEDIASUBTYPE_RGB555;
    	    break;
	case 16:
	    m_sDestType.subtype = MEDIASUBTYPE_RGB565;
	    break;
	case 24:
	    m_sDestType.subtype = MEDIASUBTYPE_RGB24;
	    break;
	case 32:
	    m_sDestType.subtype = MEDIASUBTYPE_RGB32;
	    break;
	default:
            ok = false;
	    break;
	}

        if (ok)
	    m_obh.SetBits(bits);
	//.biSizeImage=abs(temp.biWidth*temp.biHeight*((temp.biBitCount+7)/8));
    }

    if (csp != 0)
    {
        bool ok = true;
	switch (csp)
	{
	case fccYUY2:
	    m_sDestType.subtype = MEDIASUBTYPE_YUY2;
	    break;
	case fccYV12:
	    m_sDestType.subtype = MEDIASUBTYPE_YV12;
	    break;
	case fccIYUV:
	    m_sDestType.subtype = MEDIASUBTYPE_IYUV;
	    break;
	case fccUYVY:
	    m_sDestType.subtype = MEDIASUBTYPE_UYVY;
	    break;
	case fccYVYU:
	    m_sDestType.subtype = MEDIASUBTYPE_YVYU;
	    break;
	default:
	    ok = false;
            break;
	}

        if (ok)
	    m_obh.SetSpace(csp);
    }
    m_sDestType.lSampleSize = m_obh.biSizeImage;
    memcpy(&(m_sVhdr2->bmiHeader), &m_obh, sizeof(m_obh));
    m_sVhdr2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    if (m_sVhdr2->bmiHeader.biCompression == 3)
        m_sDestType.cbFormat = sizeof(VIDEOINFOHEADER) + 12;
    else
        m_sDestType.cbFormat = sizeof(VIDEOINFOHEADER);

    HRESULT result;
    bool should_test=true;
    switch(csp)
    {
    case fccYUY2:
	if(!(m_Caps & CAP_YUY2))
	    should_test=false;
	break;
    case fccYV12:
	if(!(m_Caps & CAP_YV12))
	    should_test=false;
	break;
    case fccIYUV:
	if(!(m_Caps & CAP_IYUV))
	    should_test=false;
	break;
    case fccUYVY:
	if(!(m_Caps & CAP_UYVY))
	    should_test=false;
	break;
    case fccYVYU:
	if(!(m_Caps & CAP_YVYU))
	    should_test=false;
	break;
    }
    if(should_test)
	result = m_pDS_Filter->m_pOutputPin->vt->QueryAccept(m_pDS_Filter->m_pOutputPin, &m_sDestType);
    else
	result = -1;

    if (result != 0)
    {
	if (csp)
	    cerr << "Warning: unsupported color space" << endl;
	else
	    cerr << "Warning: unsupported bit depth" << endl;

	m_sDestType.lSampleSize = m_decoder.biSizeImage;
	memcpy(&(m_sVhdr2->bmiHeader), &m_decoder, sizeof(m_decoder));
	m_sVhdr2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	if(m_sVhdr2->bmiHeader.biCompression == 3)
    	    m_sDestType.cbFormat = sizeof(VIDEOINFOHEADER) + 12;
	else
    	    m_sDestType.cbFormat = sizeof(VIDEOINFOHEADER);

	return 1;
    }

    m_decoder = m_obh;

//    m_obh=temp;
//    if(csp)
//	m_obh.biBitCount=BitmapInfo::BitCount(csp);
    m_bh->biBitCount = bits;

    //Restart();
    bool stoped = false;
    if (m_State == START)
    {
	Stop();
        stoped = true;
    }

    m_pDS_Filter->m_pInputPin->vt->Disconnect(m_pDS_Filter->m_pInputPin);
    m_pDS_Filter->m_pOutputPin->vt->Disconnect(m_pDS_Filter->m_pOutputPin);
    m_pDS_Filter->m_pOurOutput->SetNewFormat(m_sDestType);
    result = m_pDS_Filter->m_pInputPin->vt->ReceiveConnection(m_pDS_Filter->m_pInputPin,
							      m_pDS_Filter->m_pOurInput,
							      &m_sOurType);
    if (result)
    {
	cerr<<"Error reconnecting input pin "<<hex<<result<<dec<<endl;
	return -1;
    }
    result = m_pDS_Filter->m_pOutputPin->vt->ReceiveConnection(m_pDS_Filter->m_pOutputPin,
							       m_pDS_Filter->m_pOurOutput,
							       &m_sDestType);
    if (result)
    {
	cerr<<"Error reconnecting output pin "<<hex<<result<<dec<<endl;
	return -1;
    }

    if (stoped)
	Start();

    return 0;
}

HRESULT DS_VideoDecoder::GetValue(const char* name, int& value)
{
    if (m_bIsDivX)
    {
	if (m_State != START)
	    return VFW_E_NOT_RUNNING;
// brightness 87
// contrast 74
// hue 23
// saturation 20
// post process mode 0
// get1 0x01
// get2 10
// get3=set2 86
// get4=set3 73
// get5=set4 19
// get6=set5 23
	IHidden* hidden=(IHidden*)((int)m_pDS_Filter->m_pFilter+0xb8);
	if (strcmp(name, "Brightness") == 0)
	    return hidden->vt->GetSmth3(hidden, &value);
	if (strcmp(name, "Contrast") == 0)
	    return hidden->vt->GetSmth4(hidden, &value);
	if (strcmp(name, "Hue") == 0)
	    return hidden->vt->GetSmth6(hidden, &value);
	if (strcmp(name, "Saturation") == 0)
	    return hidden->vt->GetSmth5(hidden, &value);
	if (strcmp(name, "Quality") == 0)
	{
#warning NOT SURE
	    int r = hidden->vt->GetSmth2(hidden, &value);
	    if (value >= 10)
		value -= 10;
	    return 0;
	}
    }
    else if (record.dll == string("ir50_32.dll"))
    {
	IHidden2* hidden = 0;
	if (m_pDS_Filter->m_pFilter->vt->QueryInterface((IUnknown*)m_pDS_Filter->m_pFilter, &IID_Iv50Hidden, (void**)&hidden))
	{
	    cerr << "No such interface" << endl;
	    return -1;
	}
#warning FIXME
	int recordpar[30];
	recordpar[0]=0x7c;
	recordpar[1]=fccIV50;
	recordpar[2]=0x10005;
	recordpar[3]=2;
	recordpar[4]=1;
	recordpar[5]=0x80000000;

	if (strcmp(name, "Brightness") == 0)
	    recordpar[5]|=0x20;
	else if (strcmp(name, "Saturation") == 0)
	    recordpar[5]|=0x40;
	else if (strcmp(name, "Contrast") == 0)
	    recordpar[5]|=0x80;
	if (!recordpar[5])
	{
	    hidden->vt->Release((IUnknown*)hidden);
	    return -1;
	}
	if (hidden->vt->DecodeSet(hidden, recordpar))
	    return -1;

	if (strcmp(name, "Brightness") == 0)
	    value = recordpar[18];
	else if (strcmp(name, "Saturation") == 0)
	    value = recordpar[19];
	else if (strcmp(name, "Contrast") == 0)
	    value = recordpar[20];

	hidden->vt->Release((IUnknown*)hidden);
    }

    return 0;
}

HRESULT DS_VideoDecoder::SetValue(const char* name, int value)
{
    if (m_bIsDivX)
    {

	if (m_State != START)
	    return VFW_E_NOT_RUNNING;

    	/* This printf is annoying with autoquality, *
     	 * should be moved into players code - atmos */
	//printf("DS_VideoDecoder::SetValue(%s,%d)\n",name,value);

	//cout << "set value " << name << "  " << value << endl;
// brightness 87
// contrast 74
// hue 23
// saturation 20
// post process mode 0
// get1 0x01
// get2 10
// get3=set2 86
// get4=set3 73
// get5=set4 19
// get6=set5 23
    	IHidden* hidden = (IHidden*)((int)m_pDS_Filter->m_pFilter + 0xb8);
	if (strcmp(name, "Quality") == 0)
	{
            m_iLastQuality = value;
	    return hidden->vt->SetSmth(hidden, value, 0);
	}
	if (strcmp(name, "Brightness") == 0)
	    return hidden->vt->SetSmth2(hidden, value, 0);
	if (strcmp(name, "Contrast") == 0)
	    return hidden->vt->SetSmth3(hidden, value, 0);
	if (strcmp(name, "Saturation") == 0)
	    return hidden->vt->SetSmth4(hidden, value, 0);
	if (strcmp(name, "Hue") == 0)
	    return hidden->vt->SetSmth5(hidden, value, 0);
    }
    else if (record.dll == string("ir50_32.dll"))
    {
	IHidden2* hidden = 0;
	if (m_pDS_Filter->m_pFilter->vt->QueryInterface((IUnknown*)m_pDS_Filter->m_pFilter, &IID_Iv50Hidden, (void**)&hidden))
	{
	    Debug cerr << "No such interface" << endl;
	    return -1;
	}
	int recordpar[30];
	recordpar[0]=0x7c;
	recordpar[1]=fccIV50;
	recordpar[2]=0x10005;
	recordpar[3]=2;
	recordpar[4]=1;
	recordpar[5]=0x80000000;
	if (strcmp(name, "Brightness") == 0)
	{
	    recordpar[5]|=0x20;
	    recordpar[18]=value;
	}
	else if (strcmp(name, "Saturation") == 0)
	{
	    recordpar[5]|=0x40;
	    recordpar[19]=value;
	}
	else if (strcmp(name, "Contrast") == 0)
	{
	    recordpar[5]|=0x80;
	    recordpar[20]=value;
	}
	if(!recordpar[5])
	{
	    hidden->vt->Release((IUnknown*)hidden);
    	    return -1;
	}
	HRESULT result = hidden->vt->DecodeSet(hidden, recordpar);
	hidden->vt->Release((IUnknown*)hidden);

	return result;
    }
    return 0;
}
/*
vim: tabstop=8
*/
