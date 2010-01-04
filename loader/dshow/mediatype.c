/*
 *     AM_MEDIA_TYPE service functions implementations
 *     Code is based on quartz/enummedia.c file from wine project.
 *     Modified by Vladimir Voroshilov
 *
 *     Original code: Copyright 2003 Robert Shearman
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */
#include "mp_msg.h"
#include "libmpcodecs/img_format.h"
#include "loader/wine/winerror.h"
#include "loader/com.h"
#include "mediatype.h"
#include "libwin32.h"

void DisplayMediaType(const char * label,const AM_MEDIA_TYPE* pmt){
    WAVEFORMATEX* pWF;
    VIDEOINFOHEADER* Vhdr;
    int i;
    GUID* iid;


    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"=======================\n");
    if(label){
        Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"AM_MEDIA_TYPE: %s\n",label);
    }else
        Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"AM_MEDIA_TYPE:\n");
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"-(Ptr:%p)--------\n",pmt);
    for(i=0;i<sizeof(AM_MEDIA_TYPE);i++){
        Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"%02x ",(BYTE)((BYTE*)pmt)[i]);
        if((i+1)%8==0) Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"\n");
    }
    if((i)%8!=0) Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"\n");
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"-(Ptr:%p)--(%lu)--\n",pmt->pbFormat,pmt->cbFormat);
    for(i=0;i<pmt->cbFormat;i++){
        Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"%02x ",(BYTE)pmt->pbFormat[i]);
        if((i+1)%8==0) Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"\n");
    }
    if((i)%8!=0) Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"\n");
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"-----------------------\n");
    iid=(GUID*)&(pmt->subtype);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"Subtype:     %08x-%04x-%04x-%02x%02x-"
		 "%02x%02x%02x%02x%02x%02x\n",
		 iid->f1,  iid->f2,  iid->f3,
		 (unsigned char)iid->f4[1], (unsigned char)iid->f4[0],
		 (unsigned char)iid->f4[2], (unsigned char)iid->f4[3],
		 (unsigned char)iid->f4[4], (unsigned char)iid->f4[5],
		 (unsigned char)iid->f4[6], (unsigned char)iid->f4[7]);

    iid=(GUID*)&(pmt->formattype);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"Format type: %08x-%04x-%04x-%02x%02x-"
		 "%02x%02x%02x%02x%02x%02x\n",
		 iid->f1,  iid->f2,  iid->f3,
		 (unsigned char)iid->f4[1], (unsigned char)iid->f4[0],
		 (unsigned char)iid->f4[2], (unsigned char)iid->f4[3],
		 (unsigned char)iid->f4[4], (unsigned char)iid->f4[5],
		 (unsigned char)iid->f4[6], (unsigned char)iid->f4[7]);
    if(pmt && memcmp(&pmt->formattype,&FORMAT_WaveFormatEx,16)==0 && pmt->pbFormat){
    pWF=(WAVEFORMATEX*)pmt->pbFormat;
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"PMT: nChannels %d\n",pWF->nChannels);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"PMT: nSamplesPerSec %ld\n",pWF->nSamplesPerSec);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"PMT: wBitsPerSample %d\n",pWF->wBitsPerSample);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"PMT: nBlockAlign %d\n",pWF->nBlockAlign);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"PMT: nAvgBytesPerSec %ld\n",pWF->nAvgBytesPerSec);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"PMT: SampleSize %ld\n",pmt->lSampleSize);
    }
    if(pmt && memcmp(&pmt->formattype,&FORMAT_VideoInfo,16)==0 && pmt->pbFormat){
    Vhdr=(VIDEOINFOHEADER*)pmt->pbFormat;
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"Vhdr: dwBitRate %ld\n",Vhdr->dwBitRate);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"Vhdr: biWidth %ld\n",Vhdr->bmiHeader.biWidth);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"Vhdr: biHeight %ld\n",Vhdr->bmiHeader.biHeight);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"Vhdr: biSizeImage %ld\n",Vhdr->bmiHeader.biSizeImage);
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"Vhdr: biBitCount %d\n",Vhdr->bmiHeader.biBitCount);
    if(Vhdr->bmiHeader.biCompression){
        Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"Vhdr: biComression 0x%08lx (%s)\n",Vhdr->bmiHeader.biCompression,vo_format_name(Vhdr->bmiHeader.biCompression));
    }else
        Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"Vhdr: biComression 0x00000000\n");

    }
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"=======================\n");
}

HRESULT CopyMediaType(AM_MEDIA_TYPE * pDest, const AM_MEDIA_TYPE *pSrc)
{
    Debug mp_msg(MSGT_LOADER,MSGL_DBG4,"%s(%p) called\n", "CopyMediaType",pSrc);

    if(!pSrc || !pDest) return E_POINTER;

    if(pSrc == pDest) return E_INVALIDARG;

    if(!pSrc->pbFormat && pSrc->cbFormat) return E_POINTER;

    memcpy(pDest, pSrc, sizeof(AM_MEDIA_TYPE));
    if (!pSrc->pbFormat) return S_OK;
    if (!(pDest->pbFormat = CoTaskMemAlloc(pSrc->cbFormat)))
        return E_OUTOFMEMORY;
    memcpy(pDest->pbFormat, pSrc->pbFormat, pSrc->cbFormat);
    if (pDest->pUnk)
        pDest->pUnk->vt->AddRef(pDest->pUnk);
    return S_OK;
}

void FreeMediaType(AM_MEDIA_TYPE * pMediaType)
{
    if (!pMediaType) return;
    if (pMediaType->pbFormat)
    {
        CoTaskMemFree(pMediaType->pbFormat);
        pMediaType->pbFormat = NULL;
    }
    if (pMediaType->pUnk)
    {
        pMediaType->pUnk->vt->Release(pMediaType->pUnk);
        pMediaType->pUnk = NULL;
    }
}

AM_MEDIA_TYPE * CreateMediaType(AM_MEDIA_TYPE const * pSrc)
{
    AM_MEDIA_TYPE * pDest;
    if (!pSrc) return NULL;
    pDest = CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    if (!pDest)
        return NULL;

    if (FAILED(CopyMediaType(pDest, pSrc)))
    {
        CoTaskMemFree(pDest);
        return NULL;
    }

    return pDest;
}

void DeleteMediaType(AM_MEDIA_TYPE * pMediaType)
{
    if (!pMediaType) return;
    FreeMediaType(pMediaType);
    CoTaskMemFree(pMediaType);
}

#define IsEqualGUID(a,b) (memcmp(a,b,16)==0)
int CompareMediaTypes(const AM_MEDIA_TYPE * pmt1, const AM_MEDIA_TYPE * pmt2, int bWildcards)
{
    return  ((bWildcards && (IsEqualGUID(&pmt1->majortype, &GUID_NULL) || IsEqualGUID(&pmt2->majortype, &GUID_NULL))) || IsEqualGUID(&pmt1->majortype, &pmt2->majortype)) &&
            ((bWildcards && (IsEqualGUID(&pmt1->subtype, &GUID_NULL)   || IsEqualGUID(&pmt2->subtype, &GUID_NULL)))   || IsEqualGUID(&pmt1->subtype, &pmt2->subtype));
}
