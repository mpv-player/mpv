/********************************************************

	DirectShow Video decoder implementation
	Copyright 2000 Eugene Kuznetsov  (divx@euro.ru)
        Converted  C++ --> C  :) by A'rpi/ESP-team

*********************************************************/

//#include <config.h>

//#include "DS_VideoDecoder.h"
//#include <string.h>
using namespace std;
#include <stdlib.h>
#include <except.h>
#define __MODULE__ "DirectShow_VideoDecoder"


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

#include "DS_VideoDec.h"


extern "C" char* def_path;

    static char** m_destptr=0;

    static DS_Filter* dsf=0;

    static AM_MEDIA_TYPE m_sOurType, m_sDestType;
    static VIDEOINFOHEADER m_sVhdr;
    static VIDEOINFOHEADER *m_sVhdr2;
    static void* m_pCust;

    static BITMAPINFOHEADER m_bh;//format of input data
    static BitmapInfo m_decoder;//format of decoder output
    static BitmapInfo m_obh;  //format of returned frames
//    CImage* m_outFrame;

//    int m_iState=0;

extern "C" int DS_VideoDecoder_Open(char* dllname, GUID* guid, BITMAPINFOHEADER* format, int flip,char** d_ptr)
//    :IVideoDecoder(info), m_sVhdr2(0)
{

    m_destptr=d_ptr;
    
    //m_outFrame=0;
    //decpos = 0;
    //playpos = 0;
    //realtime = 0;

    try
    {
	m_bh=*format;
	memset(&m_obh, 0, sizeof(m_obh));
	m_obh.biSize=sizeof(m_obh);

	memset(&m_sVhdr, 0, sizeof(m_sVhdr));
	m_sVhdr.bmiHeader=m_bh;
	m_sVhdr.rcSource.left=m_sVhdr.rcSource.top=0;
	m_sVhdr.rcSource.right=m_sVhdr.bmiHeader.biWidth;
        m_sVhdr.rcSource.bottom=m_sVhdr.bmiHeader.biHeight;
	m_sVhdr.rcTarget=m_sVhdr.rcSource;
        m_sOurType.majortype=MEDIATYPE_Video;

	m_sOurType.subtype=MEDIATYPE_Video;
        m_sOurType.subtype.f1=m_sVhdr.bmiHeader.biCompression;
	m_sOurType.formattype=FORMAT_VideoInfo;
        m_sOurType.bFixedSizeSamples=false;
	m_sOurType.bTemporalCompression=true;
	m_sOurType.pUnk=0;
        m_sOurType.cbFormat=sizeof(m_sVhdr);
        m_sOurType.pbFormat=(char*)&m_sVhdr;

	m_sVhdr2=(VIDEOINFOHEADER*)(new char[sizeof(VIDEOINFOHEADER)+12]);
	*m_sVhdr2=m_sVhdr;
	m_sVhdr2->bmiHeader.biCompression=0;
	m_sVhdr2->bmiHeader.biBitCount=24;

	memset(&m_sDestType, 0, sizeof(m_sDestType));
	m_sDestType.majortype=MEDIATYPE_Video;
	m_sDestType.subtype=MEDIASUBTYPE_RGB24;
	m_sDestType.formattype=FORMAT_VideoInfo;
	m_sDestType.bFixedSizeSamples=true;
	m_sDestType.bTemporalCompression=false;
	m_sDestType.lSampleSize=abs(m_sVhdr2->bmiHeader.biWidth*m_sVhdr2->bmiHeader.biHeight*
	((m_sVhdr2->bmiHeader.biBitCount+7)/8));
	m_sVhdr2->bmiHeader.biSizeImage=m_sDestType.lSampleSize;
	m_sDestType.pUnk=0;
	m_sDestType.cbFormat=sizeof(VIDEOINFOHEADER);
        m_sDestType.pbFormat=(char*)m_sVhdr2;

	m_obh=m_bh;
	m_obh.setBits(24);	
        
	HRESULT result;

        dsf=new DS_Filter();
	dsf->Create(dllname, guid, &m_sOurType, &m_sDestType);

	if(!flip)
	{
	    m_sVhdr2->bmiHeader.biHeight*=-1;
	    m_obh.biHeight*=-1;
	    result=dsf->m_pOutputPin->vt->QueryAccept(dsf->m_pOutputPin, &m_sDestType);
	    if(result){
                printf("DShow: Decoder does not support upside-down frames");
                m_obh.biHeight*=-1;
            }
	}	

#if 0
	m_sVhdr2->bmiHeader.biBitCount=16;
	m_sVhdr2->bmiHeader.biCompression=fccYUY2;
	m_sDestType.subtype=MEDIASUBTYPE_YUY2;
	result=dsf->m_pOutputPin->vt->QueryAccept(dsf->m_pOutputPin, &m_sDestType);
//	if(!result) caps=(CAPS)(caps | CAP_YUY2);
#endif

	m_sVhdr2->bmiHeader.biBitCount=24;
	m_sVhdr2->bmiHeader.biCompression=0;
	m_sDestType.subtype=MEDIASUBTYPE_RGB24;
	m_decoder=m_obh;	
	//qual = 0-1;
    }
    catch(FatalError& error)
    {
	delete[] m_sVhdr2;
        return 1;
    }	    
    return 0;
}

extern "C" void DS_VideoDecoder_Start(){
    if(dsf->m_iState!=1) return;
    dsf->Start();

    ALLOCATOR_PROPERTIES props, props1;
    props.cBuffers=1;
    props.cbBuffer=1024*1024; //m_sDestType.lSampleSize;//don't know how to do this correctly
    props.cbAlign=props.cbPrefix=0;
    dsf->m_pAll->vt->SetProperties(dsf->m_pAll, &props, &props1);
    dsf->m_pAll->vt->Commit(dsf->m_pAll);

//    m_outFrame=new CImage(&m_decoder,(unsigned char *)malloc(m_sDestType.lSampleSize),false);
    //m_outFrame=new CImage(&m_decoder, 0, false);
//    printf("Datap %x\n",m_outFrame->getaddr());


//    dsf->m_pOurOutput->SetFramePointer((char **)m_outFrame->getaddr()); //!FIXME!
    dsf->m_pOurOutput->SetFramePointer(m_destptr); //!FIXME!

//    filling = realtime;
    
    dsf->m_iState=2;
    return;
}

extern "C" void DS_VideoDecoder_Stop(){
    if(dsf->m_iState!=2) return;
    dsf->Stop();
//    dsf->m_pOurOutput->SetFramePointer(0);
//    free(m_outFrame->data());
    //m_outFrame->release();//just in case
    //m_outFrame=0;
//    FlushCache();
    dsf->m_iState=1;
    return;
}

extern "C" void DS_VideoDecoder_Restart(){
    if(dsf->m_iState!=2) return;

    dsf->Stop();
    dsf->Start();

    ALLOCATOR_PROPERTIES props, props1;
    props.cBuffers=1;
    props.cbBuffer=m_sDestType.lSampleSize;//don't know how to do this correctly
    props.cbAlign=props.cbPrefix=0;
    dsf->m_pAll->vt->SetProperties(dsf->m_pAll, &props, &props1);
    dsf->m_pAll->vt->Commit(dsf->m_pAll);
}

extern "C" void DS_VideoDecoder_Close(){
    if(dsf->m_iState==0) return;
    if(dsf->m_iState==2) DS_VideoDecoder_Stop();
    delete[] m_sVhdr2;
//    delete m_outFrame;
}

extern "C" int DS_VideoDecoder_DecodeFrame(char* src, int size, int is_keyframe, int render){

    if(!size)return 0;

        m_bh.biSizeImage=size;

        IMediaSample* sample=0;
        //printf("GetBuffer... (m_pAll=%X) ",dsf->m_pAll);fflush(stdout);
	dsf->m_pAll->vt->GetBuffer(dsf->m_pAll, &sample, 0, 0, 0);
        //printf("OK!\n");
	if(!sample)
	{
	    Debug cerr<<"ERROR: null sample"<<endl;
	    return -1;
	}
        char* ptr;
        //printf("GetPtr...");fflush(stdout);
	sample->vt->GetPointer(sample, (BYTE **)&ptr);
        //printf("OK!\n");
	memcpy(ptr, src, size);
        //printf("memcpy OK!\n");
	sample->vt->SetActualDataLength(sample, size);
        //printf("SetActualDataLength OK!\n");
        sample->vt->SetSyncPoint(sample, is_keyframe);
        //printf("SetSyncPoint OK!\n");
	sample->vt->SetPreroll(sample, !render);
//    sample->vt->SetMediaType(sample, &m_sOurType);
        int result=dsf->m_pImp->vt->Receive(dsf->m_pImp, sample);
	if(result)
	    printf("Error putting data into input pin %x\n", result);

        sample->vt->Release((IUnknown*)sample);

        return 0;
}

extern "C" int DS_VideoDecoder_SetDestFmt(int bits, int csp){
    if(dsf->m_iState==0) return -1;
//    if(!CImage::supported(csp, bits)) return -1;
    HRESULT result;
//    BitmapInfo temp=m_obh;
    if(csp==0)
    {
	switch(bits)
        {
	case 15:
	    m_sDestType.subtype=MEDIASUBTYPE_RGB555;
    	    break;	    
	case 16:
	    m_sDestType.subtype=MEDIASUBTYPE_RGB565;
	    break;	    
	case 24:
	    m_sDestType.subtype=MEDIASUBTYPE_RGB24;
	    break;
	case 32:
	    m_sDestType.subtype=MEDIASUBTYPE_RGB32;
	    break;
        default:
	    break;
        }
	m_obh.setBits(bits);
//        .biSizeImage=abs(temp.biWidth*temp.biHeight*((temp.biBitCount+7)/8));
    }	
    else
    {
	m_obh.setSpace(csp,bits);
	switch(csp)
	{
	    case fccYUY2:
		m_sDestType.subtype=MEDIASUBTYPE_YUY2;
                printf("DShow: using YUY2 colorspace\n");
		break;
	    case fccYV12:
		m_sDestType.subtype=MEDIASUBTYPE_YV12;
                printf("DShow: using YV12 colorspace\n");
		break;
	    case fccIYUV:
		m_sDestType.subtype=MEDIASUBTYPE_IYUV;
                printf("DShow: using IYUV colorspace\n");
		break;
	    case fccUYVY:
		m_sDestType.subtype=MEDIASUBTYPE_UYVY;
                printf("DShow: using UYVY colorspace\n");
		break;
	    case fccYVYU:
		m_sDestType.subtype=MEDIASUBTYPE_YVYU;
                printf("DShow: using YVYU colorspace\n");
		break;
	}
    }

    m_sDestType.lSampleSize=m_obh.biSizeImage;
    memcpy(&(m_sVhdr2->bmiHeader), &m_obh, sizeof(m_obh));
    m_sVhdr2->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    if(m_sVhdr2->bmiHeader.biCompression==3)
        m_sDestType.cbFormat=sizeof(VIDEOINFOHEADER)+12;
    else
        m_sDestType.cbFormat=sizeof(VIDEOINFOHEADER);
    
    result=dsf->m_pOutputPin->vt->QueryAccept(dsf->m_pOutputPin, &m_sDestType);

    if(result!=0)
    {
	if(csp)
	    cerr<<"Warning: unsupported color space"<<endl;
	else
	    cerr<<"Warning: unsupported bit depth"<<endl;

	m_sDestType.lSampleSize=m_decoder.biSizeImage;
	memcpy(&(m_sVhdr2->bmiHeader), &m_decoder, sizeof(m_decoder));
	m_sVhdr2->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	if(m_sVhdr2->bmiHeader.biCompression==3)
    	    m_sDestType.cbFormat=sizeof(VIDEOINFOHEADER)+12;
	else
    	    m_sDestType.cbFormat=sizeof(VIDEOINFOHEADER);
	return 1;
    }   
    
    m_decoder=m_obh; 
//    m_obh=temp;
//    if(csp)
//	m_obh.biBitCount=BitmapInfo::BitCount(csp);
    m_bh.biBitCount=bits;
    if(dsf->m_iState>0)
    {
	int old_state=dsf->m_iState;
	if(dsf->m_iState==2) DS_VideoDecoder_Stop();
	dsf->m_pInputPin->vt->Disconnect(dsf->m_pInputPin);
	dsf->m_pOutputPin->vt->Disconnect(dsf->m_pOutputPin);
	dsf->m_pOurOutput->SetNewFormat(m_sDestType);
    	result=dsf->m_pInputPin->vt->ReceiveConnection(dsf->m_pInputPin, dsf->m_pOurInput, &m_sOurType);
	if(result)
	{
	    cerr<<"Error reconnecting input pin "<<hex<<result<<dec<<endl;
	    return -1;
	}	
        result=dsf->m_pOutputPin->vt->ReceiveConnection(dsf->m_pOutputPin,
	     dsf->m_pOurOutput, &m_sDestType);
	if(result)
	{
	    cerr<<"Error reconnecting output pin "<<hex<<result<<dec<<endl;
	    return -1;
	}		
	if(old_state==2) DS_VideoDecoder_Start();
    }
    return 0; 
}


extern "C" int DS_SetValue_DivX(char* name, int value){
	int temp;
	if(dsf->m_iState!=2) return VFW_E_NOT_RUNNING;
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
//        printf("DivX setting: %s = %d\n",name,value);

    	IHidden* hidden=(IHidden*)((int)dsf->m_pFilter+0xb8);
	if(strcmp(name, "Brightness")==0)
	    return hidden->vt->SetSmth2(hidden, value, 0);
	if(strcmp(name, "Contrast")==0)
	    return hidden->vt->SetSmth3(hidden, value, 0);
	if(strcmp(name, "Hue")==0)
	    return hidden->vt->SetSmth5(hidden, value, 0);
	if(strcmp(name, "Saturation")==0)
	    return hidden->vt->SetSmth4(hidden, value, 0);
	if(strcmp(name, "Quality")==0)
	    return hidden->vt->SetSmth(hidden, value, 0);
            
        printf("Invalid setting!\n");
        return -200;
}

extern "C" int DS_SetAttr_DivX(char* attribute, int value){
    int result, status, newkey, count;
        if(strcmp(attribute, "Quality")==0){
	    char* keyname="SOFTWARE\\Microsoft\\Scrunch";
    	    result=RegCreateKeyExA(HKEY_CURRENT_USER, keyname, 0, 0, 0, 0, 0,	   		&newkey, &status);
            if(result!=0)
	    {
	        printf("VideoDecoder::SetExtAttr: registry failure\n");
	        return -1;
	    }    
	    result=RegSetValueExA(newkey, "Current Post Process Mode", 0, REG_DWORD, &value, 4);
            if(result!=0)
	    {
	        printf("VideoDecoder::SetExtAttr: error writing value\n");
	        return -1;
	    }    
	    value=-1;
	    result=RegSetValueExA(newkey, "Force Post Process Mode", 0, REG_DWORD, &value, 4);
            if(result!=0)
	    {
		printf("VideoDecoder::SetExtAttr: error writing value\n");
	    	return -1;
	    }    
   	    RegCloseKey(newkey);
   	    return 0;
	}   	

        if(
	(strcmp(attribute, "Saturation")==0) ||
	(strcmp(attribute, "Hue")==0) ||
	(strcmp(attribute, "Contrast")==0) ||
	(strcmp(attribute, "Brightness")==0)
	)
        {
	    char* keyname="SOFTWARE\\Microsoft\\Scrunch\\Video";
    	    result=RegCreateKeyExA(HKEY_CURRENT_USER, keyname, 0, 0, 0, 0, 0,	   		&newkey, &status);
            if(result!=0)
	    {
	        printf("VideoDecoder::SetExtAttr: registry failure\n");
	        return -1;
	    }    
	    result=RegSetValueExA(newkey, attribute, 0, REG_DWORD, &value, 4);
            if(result!=0)
	    {
	        printf("VideoDecoder::SetExtAttr: error writing value\n");
	        return -1;
	    }    
   	    RegCloseKey(newkey);
   	    return 0;
	}   	

        printf("Unknown attribute!\n");
        return -200;
}

