/********************************************************

	DirectShow Video decoder implementation
	Copyright 2000 Eugene Kuznetsov  (divx@euro.ru)

*********************************************************/
#include "config.h"
#include "dshow/guids.h"
#include "dshow/interfaces.h"
#include "registry.h"
#ifdef WIN32_LOADER
#include "ldt_keeper.h"
#endif

#include "dshow/libwin32.h"
#include "DMO_Filter.h"

#include "DMO_VideoDecoder.h"

struct DMO_VideoDecoder
{
    IVideoDecoder iv;
    
    DMO_Filter* m_pDMO_Filter;
    AM_MEDIA_TYPE m_sOurType, m_sDestType;
    VIDEOINFOHEADER* m_sVhdr;
    VIDEOINFOHEADER* m_sVhdr2;
    int m_Caps;//CAPS m_Caps;                // capabilities of DirectShow decoder
    int m_iLastQuality;         // remember last quality as integer
    int m_iMinBuffers;
    int m_iMaxAuto;
};

//#include "DMO_VideoDecoder.h"

#include "wine/winerror.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <stdio.h>
#include <stdlib.h>  // labs

// strcmp((const char*)info.dll,...)  is used instead of  (... == ...)
// so Arpi could use char* pointer in his simplified DMO_VideoDecoder class

#define false 0
#define true 1


//int DMO_VideoDecoder_GetCapabilities(DMO_VideoDecoder *this){return this->m_Caps;}

typedef struct ct ct;

struct ct {
    fourcc_t fcc;
    unsigned int bits;
    const GUID* subtype;
    int cap;
    char *name;
	    };
            
static ct check[] = {
    { fccI420, 12, &MEDIASUBTYPE_I420,   CAP_I420, NULL     },
    { fccYV12, 12, &MEDIASUBTYPE_YV12,   CAP_YV12, NULL     },
    { fccYUY2, 16, &MEDIASUBTYPE_YUY2,   CAP_YUY2, NULL     },
    { fccUYVY, 16, &MEDIASUBTYPE_UYVY,   CAP_UYVY, NULL     },
    { fccYVYU, 16, &MEDIASUBTYPE_YVYU,   CAP_YVYU, NULL     },
    { fccIYUV, 24, &MEDIASUBTYPE_IYUV,   CAP_IYUV, NULL     },

    {       8,  8, &MEDIASUBTYPE_RGB8,   CAP_NONE, "RGB8"   },
    {      15, 16, &MEDIASUBTYPE_RGB555, CAP_NONE, "RGB555" },
    {      16, 16, &MEDIASUBTYPE_RGB565, CAP_NONE, "RGB565" },
    {      24, 24, &MEDIASUBTYPE_RGB24,  CAP_NONE, "RGB24"  },
    {      32, 32, &MEDIASUBTYPE_RGB32,  CAP_NONE, "RGB32"  },

    {0,0,NULL,0},
};

DMO_VideoDecoder * DMO_VideoDecoder_Open(char* dllname, GUID* guid, BITMAPINFOHEADER * format, int flip, int maxauto)
{
    DMO_VideoDecoder *this;
    HRESULT result;
    ct* c;
                        
    this = malloc(sizeof(DMO_VideoDecoder));
    memset( this, 0, sizeof(DMO_VideoDecoder));
    
    this->m_sVhdr2 = 0;
    this->m_iLastQuality = -1;
    this->m_iMaxAuto = maxauto;

#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
#endif

    //memset(&m_obh, 0, sizeof(m_obh));
    //m_obh.biSize = sizeof(m_obh);
    /*try*/
    {
        unsigned int bihs;
        
	bihs = (format->biSize < (int) sizeof(BITMAPINFOHEADER)) ?
	    sizeof(BITMAPINFOHEADER) : format->biSize;
     
        this->iv.m_bh = malloc(bihs);
        memcpy(this->iv.m_bh, format, bihs);
        this->iv.m_bh->biSize = bihs;

        this->iv.m_State = STOP;
        //this->iv.m_pFrame = 0;
        this->iv.m_Mode = DIRECT;
        this->iv.m_iDecpos = 0;
        this->iv.m_iPlaypos = -1;
        this->iv.m_fQuality = 0.0f;
        this->iv.m_bCapable16b = true;
                
        bihs += sizeof(VIDEOINFOHEADER) - sizeof(BITMAPINFOHEADER);
	this->m_sVhdr = malloc(bihs);
	memset(this->m_sVhdr, 0, bihs);
	memcpy(&this->m_sVhdr->bmiHeader, this->iv.m_bh, this->iv.m_bh->biSize);
	this->m_sVhdr->rcSource.left = this->m_sVhdr->rcSource.top = 0;
	this->m_sVhdr->rcSource.right = this->m_sVhdr->bmiHeader.biWidth;
	this->m_sVhdr->rcSource.bottom = this->m_sVhdr->bmiHeader.biHeight;
	//this->m_sVhdr->rcSource.right = 0;
	//this->m_sVhdr->rcSource.bottom = 0;
	this->m_sVhdr->rcTarget = this->m_sVhdr->rcSource;

	this->m_sOurType.majortype = MEDIATYPE_Video;
	this->m_sOurType.subtype = MEDIATYPE_Video;
        this->m_sOurType.subtype.f1 = this->m_sVhdr->bmiHeader.biCompression;
	this->m_sOurType.formattype = FORMAT_VideoInfo;
        this->m_sOurType.bFixedSizeSamples = false;
	this->m_sOurType.bTemporalCompression = true;
	this->m_sOurType.pUnk = 0;
        this->m_sOurType.cbFormat = bihs;
        this->m_sOurType.pbFormat = (char*)this->m_sVhdr;

	this->m_sVhdr2 = (VIDEOINFOHEADER*)(malloc(sizeof(VIDEOINFOHEADER)+12));
	memcpy(this->m_sVhdr2, this->m_sVhdr, sizeof(VIDEOINFOHEADER));
	memset((char*)this->m_sVhdr2 + sizeof(VIDEOINFOHEADER), 0, 12);
	this->m_sVhdr2->bmiHeader.biCompression = 0;
	this->m_sVhdr2->bmiHeader.biBitCount = 24;

//	memset((char*)this->m_sVhdr2, 0, sizeof(VIDEOINFOHEADER)+12);
	this->m_sVhdr2->rcTarget = this->m_sVhdr->rcTarget;
//	this->m_sVhdr2->rcSource = this->m_sVhdr->rcSource;

	memset(&this->m_sDestType, 0, sizeof(this->m_sDestType));
	this->m_sDestType.majortype = MEDIATYPE_Video;
	this->m_sDestType.subtype = MEDIASUBTYPE_RGB24;
	this->m_sDestType.formattype = FORMAT_VideoInfo;
	this->m_sDestType.bFixedSizeSamples = true;
	this->m_sDestType.bTemporalCompression = false;
	this->m_sDestType.lSampleSize = labs(this->m_sVhdr2->bmiHeader.biWidth*this->m_sVhdr2->bmiHeader.biHeight
				       * ((this->m_sVhdr2->bmiHeader.biBitCount + 7) / 8));
	this->m_sVhdr2->bmiHeader.biSizeImage = this->m_sDestType.lSampleSize;
	this->m_sDestType.pUnk = 0;
	this->m_sDestType.cbFormat = sizeof(VIDEOINFOHEADER);
	this->m_sDestType.pbFormat = (char*)this->m_sVhdr2;
        
        memset(&this->iv.m_obh, 0, sizeof(this->iv.m_obh));
	memcpy(&this->iv.m_obh, this->iv.m_bh, sizeof(this->iv.m_obh) < (unsigned) this->iv.m_bh->biSize
	       ? sizeof(this->iv.m_obh) : (unsigned) this->iv.m_bh->biSize);
	this->iv.m_obh.biBitCount=24;
        this->iv.m_obh.biSize = sizeof(BITMAPINFOHEADER);
        this->iv.m_obh.biCompression = 0;	//BI_RGB
        //this->iv.m_obh.biHeight = labs(this->iv.m_obh.biHeight);
        this->iv.m_obh.biSizeImage = labs(this->iv.m_obh.biWidth * this->iv.m_obh.biHeight)
                              * ((this->iv.m_obh.biBitCount + 7) / 8);


	this->m_pDMO_Filter = DMO_FilterCreate(dllname, guid, &this->m_sOurType, &this->m_sDestType);
	
	if (!this->m_pDMO_Filter)
	{
	    printf("Failed to create DMO filter\n");
	    return 0;
	}

	if (!flip)
	{
	    this->iv.m_obh.biHeight *= -1;
	    this->m_sVhdr2->bmiHeader.biHeight = this->iv.m_obh.biHeight;
//	    result = this->m_pDMO_Filter->m_pOutputPin->vt->QueryAccept(this->m_pDMO_Filter->m_pOutputPin, &this->m_sDestType);
	    result = this->m_pDMO_Filter->m_pMedia->vt->SetOutputType(this->m_pDMO_Filter->m_pMedia, 0, &this->m_sDestType, DMO_SET_TYPEF_TEST_ONLY);
	    if (result)
	    {
		printf("Decoder does not support upside-down RGB frames\n");
		this->iv.m_obh.biHeight *= -1;
		this->m_sVhdr2->bmiHeader.biHeight = this->iv.m_obh.biHeight;
	    }
	}

        memcpy( &this->iv.m_decoder, &this->iv.m_obh, sizeof(this->iv.m_obh) );

	switch (this->iv.m_bh->biCompression)
	{
#if 0
	case fccDIV3:
	case fccDIV4:
	case fccDIV5:
	case fccDIV6:
	case fccMP42:
	case fccWMV2:
	    //YV12 seems to be broken for DivX :-) codec
//	case fccIV50:
	    //produces incorrect picture
	    //m_Caps = (CAPS) (m_Caps & ~CAP_YV12);
	    //m_Caps = CAP_UYVY;//CAP_YUY2; // | CAP_I420;
	    //m_Caps = CAP_I420;
	    this->m_Caps = (CAP_YUY2 | CAP_UYVY);
	    break;
#endif
	default:
              
	    this->m_Caps = CAP_NONE;

	    printf("Decoder supports the following formats: ");
	    for (c = check; c->bits; c++)
	    {
		this->m_sVhdr2->bmiHeader.biBitCount = c->bits;
		this->m_sVhdr2->bmiHeader.biCompression = c->fcc;
		this->m_sDestType.subtype = *c->subtype;
		//result = this->m_pDMO_Filter->m_pOutputPin->vt->QueryAccept(this->m_pDMO_Filter->m_pOutputPin, &this->m_sDestType);
		result = this->m_pDMO_Filter->m_pMedia->vt->SetOutputType(this->m_pDMO_Filter->m_pMedia, 0, &this->m_sDestType, DMO_SET_TYPEF_TEST_ONLY);
		if (!result)
		{
		    this->m_Caps = (this->m_Caps | c->cap);
		    if (c->name)
			printf("%s ", c->name);
		    else
			printf("%.4s ", (char*) &c->fcc);
		}
	    }
	    printf("\n");
	}

	if (this->m_Caps != CAP_NONE)
	    printf("Decoder is capable of YUV output (flags 0x%x)\n", (int)this->m_Caps);

	this->m_sVhdr2->bmiHeader.biBitCount = 24;
	this->m_sVhdr2->bmiHeader.biCompression = 0;
	this->m_sDestType.subtype = MEDIASUBTYPE_RGB24;

	this->m_iMinBuffers = this->iv.VBUFSIZE;
    }
    /*catch (FatalError& error)
    {
        delete[] m_sVhdr;
	delete[] m_sVhdr2;
        delete m_pDMO_Filter;
	throw;
    }*/
    return this;
}

void DMO_VideoDecoder_Destroy(DMO_VideoDecoder *this)
{
    DMO_VideoDecoder_StopInternal(this);
    this->iv.m_State = STOP;
    free(this->m_sVhdr);
    free(this->m_sVhdr2);
    DMO_Filter_Destroy(this->m_pDMO_Filter);
}

void DMO_VideoDecoder_StartInternal(DMO_VideoDecoder *this)
{
#if 0
    ALLOCATOR_PROPERTIES props, props1;
    Debug printf("DMO_VideoDecoder_StartInternal\n");
    //cout << "DSSTART" << endl;
    this->m_pDMO_Filter->Start(this->m_pDMO_Filter);
    
    props.cBuffers = 1;
    props.cbBuffer = this->m_sDestType.lSampleSize;

    props.cbAlign = 1;
    props.cbPrefix = 0;
    this->m_pDMO_Filter->m_pAll->vt->SetProperties(this->m_pDMO_Filter->m_pAll, &props, &props1);
    this->m_pDMO_Filter->m_pAll->vt->Commit(this->m_pDMO_Filter->m_pAll);
#endif    
    this->iv.m_State = START;
}

void DMO_VideoDecoder_StopInternal(DMO_VideoDecoder *this)
{
    // this->m_pDMO_Filter->Stop(this->m_pDMO_Filter);
    //??? why was this here ??? m_pOurOutput->SetFramePointer(0);
}

int DMO_VideoDecoder_DecodeInternal(DMO_VideoDecoder *this, const void* src, int size, int is_keyframe, char* imdata)
{
//    IMediaSample* sample = 0;
    int result;
    unsigned long status; // to be ignored by M$ specs
    DMO_OUTPUT_DATA_BUFFER db;
    CMediaBuffer* bufferin;
//+    uint8_t* imdata = dest ? dest->Data() : 0;
    
    Debug printf("DMO_VideoDecoder_DecodeInternal(%p,%p,%d,%d,%p)\n",this,src,size,is_keyframe,imdata);

//    this->m_pDMO_Filter->m_pAll->vt->GetBuffer(this->m_pDMO_Filter->m_pAll, &sample, 0, 0, 0);
//    if (!sample)
//    {
//	Debug printf("ERROR: null sample\n");
//	return -1;
//    }

#ifdef WIN32_LOADER
    Setup_FS_Segment();
#endif

    bufferin = CMediaBufferCreate(size, (void*)src, size, 0);
    result = this->m_pDMO_Filter->m_pMedia->vt->ProcessInput(this->m_pDMO_Filter->m_pMedia, 0,
						      (IMediaBuffer*)bufferin,
						      DMO_INPUT_DATA_BUFFERF_SYNCPOINT,
						      0, 0);
    ((IMediaBuffer*)bufferin)->vt->Release((IUnknown*)bufferin);

    if (result != S_OK)
    {
        /* something for process */
	if (result != S_FALSE)
	    printf("ProcessInputError  r:0x%x=%d (keyframe: %d)\n", result, result, is_keyframe);
	else
	    printf("ProcessInputError  FALSE ?? (keyframe: %d)\n", is_keyframe);
	return size;
    }

    db.rtTimestamp = 0;
    db.rtTimelength = 0;
    db.dwStatus = 0;
    db.pBuffer = (IMediaBuffer*) CMediaBufferCreate(this->m_sDestType.lSampleSize,
						    imdata, 0, 0);
    result = this->m_pDMO_Filter->m_pMedia->vt->ProcessOutput(this->m_pDMO_Filter->m_pMedia,
						   (imdata) ? 0 : DMO_PROCESS_OUTPUT_DISCARD_WHEN_NO_BUFFER,
						   1, &db, &status);
    //m_pDMO_Filter->m_pMedia->vt->Lock(m_pDMO_Filter->m_pMedia, 0);
    if ((unsigned)result == DMO_E_NOTACCEPTING)
	printf("ProcessOutputError: Not accepting\n");
    else if (result)
	printf("ProcessOutputError: r:0x%x=%d  %ld  stat:%ld\n", result, result, status, db.dwStatus);

    ((IMediaBuffer*)db.pBuffer)->vt->Release((IUnknown*)db.pBuffer);

    //int r = m_pDMO_Filter->m_pMedia->vt->Flush(m_pDMO_Filter->m_pMedia);
    //printf("FLUSH %d\n", r);

    return 0;
}

/*
 * bits == 0   - leave unchanged
 */
//int SetDestFmt(DMO_VideoDecoder * this, int bits = 24, fourcc_t csp = 0);
int DMO_VideoDecoder_SetDestFmt(DMO_VideoDecoder *this, int bits, unsigned int csp)
{
    HRESULT result;
    int should_test=1;
    
    Debug printf("DMO_VideoDecoder_SetDestFmt (%p, %d, %d)\n",this,bits,(int)csp);
        
       /* if (!CImage::Supported(csp, bits))
	return -1;
*/
    // BitmapInfo temp = m_obh;
    
    if (!csp)	// RGB
    {
	int ok = true;

	switch (bits)
        {
	case 15:
	    this->m_sDestType.subtype = MEDIASUBTYPE_RGB555;
    	    break;
	case 16:
	    this->m_sDestType.subtype = MEDIASUBTYPE_RGB565;
	    break;
	case 24:
	    this->m_sDestType.subtype = MEDIASUBTYPE_RGB24;
	    break;
	case 32:
	    this->m_sDestType.subtype = MEDIASUBTYPE_RGB32;
	    break;
	default:
            ok = false;
	    break;
	}

        if (ok) {
	    this->iv.m_obh.biBitCount=bits;
            if( bits == 15 || bits == 16 ) {
	      this->iv.m_obh.biSize=sizeof(BITMAPINFOHEADER)+12;
	      this->iv.m_obh.biCompression=3;//BI_BITFIELDS
	      this->iv.m_obh.biSizeImage=abs((int)(2*this->iv.m_obh.biWidth*this->iv.m_obh.biHeight));
	    }
            
            if( bits == 16 ) {
	      this->iv.m_obh.colors[0]=0xF800;
	      this->iv.m_obh.colors[1]=0x07E0;
	      this->iv.m_obh.colors[2]=0x001F;
            } else if ( bits == 15 ) {
	      this->iv.m_obh.colors[0]=0x7C00;
	      this->iv.m_obh.colors[1]=0x03E0;
	      this->iv.m_obh.colors[2]=0x001F;
            } else {
	      this->iv.m_obh.biSize = sizeof(BITMAPINFOHEADER);
	      this->iv.m_obh.biCompression = 0;	//BI_RGB
	      //this->iv.m_obh.biHeight = labs(this->iv.m_obh.biHeight);
	      this->iv.m_obh.biSizeImage = labs(this->iv.m_obh.biWidth * this->iv.m_obh.biHeight)
                              * ((this->iv.m_obh.biBitCount + 7) / 8);
            }
        }
	//.biSizeImage=abs(temp.biWidth*temp.biHeight*((temp.biBitCount+7)/8));
    } else
    {	// YUV
        int ok = true;
	switch (csp)
	{
	case fccYUY2:
	    this->m_sDestType.subtype = MEDIASUBTYPE_YUY2;
	    break;
	case fccYV12:
	    this->m_sDestType.subtype = MEDIASUBTYPE_YV12;
	    break;
	case fccIYUV:
	    this->m_sDestType.subtype = MEDIASUBTYPE_IYUV;
	    break;
	case fccI420:
	    this->m_sDestType.subtype = MEDIASUBTYPE_I420;
	    break;
	case fccUYVY:
	    this->m_sDestType.subtype = MEDIASUBTYPE_UYVY;
	    break;
	case fccYVYU:
	    this->m_sDestType.subtype = MEDIASUBTYPE_YVYU;
	    break;
	case fccYVU9:
	    this->m_sDestType.subtype = MEDIASUBTYPE_YVU9;
	default:
	    ok = false;
            break;
	}

        if (ok) {
	  if (csp != 0 && csp != 3 && this->iv.m_obh.biHeight > 0)
    	    this->iv.m_obh.biHeight *= -1; // YUV formats uses should have height < 0
	  this->iv.m_obh.biSize = sizeof(BITMAPINFOHEADER);
	  this->iv.m_obh.biCompression=csp;
	  this->iv.m_obh.biBitCount=bits;

	  this->iv.m_obh.biSizeImage = labs(this->iv.m_obh.biWidth * this->iv.m_obh.biHeight)
                                       * ((this->iv.m_obh.biBitCount + 7) / 8);
        }
    }
    this->m_sDestType.lSampleSize = this->iv.m_obh.biSizeImage;
    memcpy(&(this->m_sVhdr2->bmiHeader), &this->iv.m_obh, sizeof(this->iv.m_obh));
    this->m_sVhdr2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    if (this->m_sVhdr2->bmiHeader.biCompression == 3)
        this->m_sDestType.cbFormat = sizeof(VIDEOINFOHEADER) + 12;
    else
        this->m_sDestType.cbFormat = sizeof(VIDEOINFOHEADER);


    switch(csp)
    {
    case fccYUY2:
	if(!(this->m_Caps & CAP_YUY2))
	    should_test=false;
	break;
    case fccYV12:
	if(!(this->m_Caps & CAP_YV12))
	    should_test=false;
	break;
    case fccIYUV:
	if(!(this->m_Caps & CAP_IYUV))
	    should_test=false;
	break;
    case fccI420:
	if(!(this->m_Caps & CAP_I420))
	    should_test=false;
	break;
    case fccUYVY:
	if(!(this->m_Caps & CAP_UYVY))
	    should_test=false;
	break;
    case fccYVYU:
	if(!(this->m_Caps & CAP_YVYU))
	    should_test=false;
	break;
    case fccYVU9:
	if(!(this->m_Caps & CAP_YVU9))
	    should_test=false;
	break;
    }

#ifdef WIN32_LOADER
    Setup_FS_Segment();
#endif

//    if(should_test)
//	result = this->m_pDMO_Filter->m_pOutputPin->vt->QueryAccept(this->m_pDMO_Filter->m_pOutputPin, &this->m_sDestType);
//    else
//	result = -1;

    // test accept
    if(!this->m_pDMO_Filter) return 0;
    result = this->m_pDMO_Filter->m_pMedia->vt->SetOutputType(this->m_pDMO_Filter->m_pMedia, 0, &this->m_sDestType, DMO_SET_TYPEF_TEST_ONLY);

    if (result != 0)
    {
	if (csp)
	    printf("Warning: unsupported color space\n");
	else
	    printf("Warning: unsupported bit depth\n");

	this->m_sDestType.lSampleSize = this->iv.m_decoder.biSizeImage;
	memcpy(&(this->m_sVhdr2->bmiHeader), &this->iv.m_decoder, sizeof(this->iv.m_decoder));
	this->m_sVhdr2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	if (this->m_sVhdr2->bmiHeader.biCompression == 3)
    	    this->m_sDestType.cbFormat = sizeof(VIDEOINFOHEADER) + 12;
	else
    	    this->m_sDestType.cbFormat = sizeof(VIDEOINFOHEADER);

	return -1;
    }

    memcpy( &this->iv.m_decoder, &this->iv.m_obh, sizeof(this->iv.m_obh));

//    m_obh=temp;
//    if(csp)
//	m_obh.biBitCount=BitmapInfo::BitCount(csp);
    this->iv.m_bh->biBitCount = bits;

    //DMO_VideoDecoder_Restart(this);

    this->m_pDMO_Filter->m_pMedia->vt->SetOutputType(this->m_pDMO_Filter->m_pMedia, 0, &this->m_sDestType, 0);

    return 0;
}


int DMO_VideoDecoder_SetDirection(DMO_VideoDecoder *this, int d)
{
    this->iv.m_obh.biHeight = (d) ? this->iv.m_bh->biHeight : -this->iv.m_bh->biHeight;
    this->m_sVhdr2->bmiHeader.biHeight = this->iv.m_obh.biHeight;
    return 0;
}

