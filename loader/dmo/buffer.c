#include "DMO_Filter.h"

#include "loader/wine/winerror.h"
#include "loader/wine/windef.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct CMediaBuffer
{
    IMediaBuffer_vt* vt;
    DECLARE_IUNKNOWN();
    GUID interfaces[2];
    void* mem;
    unsigned long len;
    unsigned long maxlen;
    int freemem;
};

static HRESULT STDCALL CMediaBuffer_SetLength(IMediaBuffer* This,
					      unsigned long cbLength)
{
    CMediaBuffer* cmb = (CMediaBuffer*) This;
    Debug printf("CMediaBuffer_SetLength(%p) called (%ld, %ld)\n", This, cbLength, cmb->maxlen);
    if (cbLength > cmb->maxlen)
        return E_INVALIDARG;
    cmb->len = cbLength;
    return S_OK;
}

static HRESULT STDCALL CMediaBuffer_GetMaxLength(IMediaBuffer* This,
						 /* [out] */ unsigned long *pcbMaxLength)
{
    CMediaBuffer* cmb = (CMediaBuffer*) This;
    Debug printf("CMediaBuffer_GetMaxLength(%p) called -> %ld\n", This, cmb->maxlen);
    if (!pcbMaxLength)
	return E_POINTER;
    *pcbMaxLength = cmb->maxlen;
    return S_OK;
}

static HRESULT STDCALL CMediaBuffer_GetBufferAndLength(IMediaBuffer* This,
						       /* [out] */ char** ppBuffer,
						       /* [out] */ unsigned long* pcbLength)
{
    CMediaBuffer* cmb = (CMediaBuffer*) This;
    Debug printf("CMediaBuffer_GetBufferAndLength(%p) called -> %p %ld\n", This, cmb->mem, cmb->len);
    if (!ppBuffer && !pcbLength)
	return E_POINTER;
    if (ppBuffer)
	*ppBuffer = cmb->mem;
    if (pcbLength)
	*pcbLength = cmb->len;
    return S_OK;
}

static void CMediaBuffer_Destroy(CMediaBuffer* This)
{
    Debug printf("CMediaBuffer_Destroy(%p) called\n", This);
    if (This->freemem)
        free(This->mem);
    free(This->vt);
    free(This);
}

IMPLEMENT_IUNKNOWN(CMediaBuffer)

CMediaBuffer* CMediaBufferCreate(unsigned long maxlen, void* mem,
				 unsigned long len, int copy)
{
    CMediaBuffer* This = malloc(sizeof(CMediaBuffer));

    if (!This)
        return NULL;

    This->vt = malloc(sizeof(IMediaBuffer_vt));
    if (!This->vt)
    {
        CMediaBuffer_Destroy(This);
	return NULL;
    }

    This->refcount = 1;
    This->len = len;
    This->maxlen = maxlen;
    This->freemem = 0;
    This->mem = mem;
    if (copy)
	/* make a private copy of data */
        This->mem = 0;
    if (This->mem == NULL)
    {
	if (This->maxlen)
	{
	    This->mem = malloc(This->maxlen);
	    if (!This->mem)
	    {
		CMediaBuffer_Destroy(This);
		return NULL;
	    }
	    This->freemem = 1;
	    if (copy)
		memcpy(This->mem, mem, This->len);
	}
    }
    This->vt->QueryInterface = CMediaBuffer_QueryInterface;
    This->vt->AddRef = CMediaBuffer_AddRef;
    This->vt->Release = CMediaBuffer_Release;

    This->vt->SetLength = CMediaBuffer_SetLength;
    This->vt->GetMaxLength = CMediaBuffer_GetMaxLength;
    This->vt->GetBufferAndLength = CMediaBuffer_GetBufferAndLength;

    This->interfaces[0] = IID_IUnknown;
    This->interfaces[1] = IID_IMediaBuffer;

    return This;
}
