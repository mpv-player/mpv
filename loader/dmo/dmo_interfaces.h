#ifndef DMO_INTERFACE_H
#define DMO_INTERFACE_H

#include "dmo.h"

/*
 * IMediaBuffer interface
 */
typedef struct _IMediaBuffer IMediaBuffer;
typedef struct IMediaBuffer_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *SetLength )(IMediaBuffer* This,
				   unsigned long cbLength);
    HRESULT STDCALL ( *GetMaxLength )(IMediaBuffer* This,
				      /* [out] */ unsigned long *pcbMaxLength);
    HRESULT STDCALL ( *GetBufferAndLength )(IMediaBuffer* This,
					    /* [out] */ char** ppBuffer,
					    /* [out] */ unsigned long* pcbLength);
} IMediaBuffer_vt;
struct _IMediaBuffer { IMediaBuffer_vt* vt; };


typedef struct _DMO_OUTPUT_DATA_BUFFER
{
    IMediaBuffer *pBuffer;
    unsigned long dwStatus;
    REFERENCE_TIME rtTimestamp;
    REFERENCE_TIME rtTimelength;
} DMO_OUTPUT_DATA_BUFFER;


/*
 * IMediaObject interface
 */
typedef struct _IMediaObject IMediaObject;
typedef struct IMediaObject_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *GetStreamCount )(IMediaObject * This,
					/* [out] */ unsigned long *pcInputStreams,
					/* [out] */ unsigned long *pcOutputStreams);
    HRESULT STDCALL ( *GetInputStreamInfo )(IMediaObject * This,
					    unsigned long dwInputStreamIndex,
					    /* [out] */ unsigned long *pdwFlags);
    HRESULT STDCALL ( *GetOutputStreamInfo )(IMediaObject * This,
					     unsigned long dwOutputStreamIndex,
					     /* [out] */ unsigned long *pdwFlags);
    HRESULT STDCALL ( *GetInputType )(IMediaObject * This,
				      unsigned long dwInputStreamIndex,
				      unsigned long dwTypeIndex,
				      /* [out] */ DMO_MEDIA_TYPE *pmt);
    HRESULT STDCALL ( *GetOutputType )(IMediaObject * This,
				       unsigned long dwOutputStreamIndex,
				       unsigned long dwTypeIndex,
				       /* [out] */ DMO_MEDIA_TYPE *pmt);
    HRESULT STDCALL ( *SetInputType )(IMediaObject * This,
				      unsigned long dwInputStreamIndex,
				      /* [in] */ const DMO_MEDIA_TYPE *pmt,
				      unsigned long dwFlags);
    HRESULT STDCALL ( *SetOutputType )(IMediaObject * This,
				       unsigned long dwOutputStreamIndex,
				       /* [in] */ const DMO_MEDIA_TYPE *pmt,
				       unsigned long dwFlags);
    HRESULT STDCALL ( *GetInputCurrentType )(IMediaObject * This,
					     unsigned long dwInputStreamIndex,
					     /* [out] */ DMO_MEDIA_TYPE *pmt);
    HRESULT STDCALL ( *GetOutputCurrentType )(IMediaObject * This,
					      unsigned long dwOutputStreamIndex,
					      /* [out] */ DMO_MEDIA_TYPE *pmt);
    HRESULT STDCALL ( *GetInputSizeInfo )(IMediaObject * This,
					  unsigned long dwInputStreamIndex,
					  /* [out] */ unsigned long *pcbSize,
					  /* [out] */ unsigned long *pcbMaxLookahead,
					  /* [out] */ unsigned long *pcbAlignment);
    HRESULT STDCALL ( *GetOutputSizeInfo )(IMediaObject * This,
					   unsigned long dwOutputStreamIndex,
					   /* [out] */ unsigned long *pcbSize,
					   /* [out] */ unsigned long *pcbAlignment);
    HRESULT STDCALL ( *GetInputMaxLatency )(IMediaObject * This,
					    unsigned long dwInputStreamIndex,
					    /* [out] */ REFERENCE_TIME *prtMaxLatency);
    HRESULT STDCALL ( *SetInputMaxLatency )(IMediaObject * This,
					    unsigned long dwInputStreamIndex,
					    REFERENCE_TIME rtMaxLatency);
    HRESULT STDCALL ( *Flush )(IMediaObject * This);
    HRESULT STDCALL ( *Discontinuity )(IMediaObject * This,
				       unsigned long dwInputStreamIndex);
    HRESULT STDCALL ( *AllocateStreamingResources )(IMediaObject * This);
    HRESULT STDCALL ( *FreeStreamingResources )(IMediaObject * This);
    HRESULT STDCALL ( *GetInputStatus )(IMediaObject * This,
					unsigned long dwInputStreamIndex,
					/* [out] */ unsigned long *dwFlags);
    HRESULT STDCALL ( *ProcessInput )(IMediaObject * This,
				      unsigned long dwInputStreamIndex,
				      IMediaBuffer *pBuffer,
				      unsigned long dwFlags,
				      REFERENCE_TIME rtTimestamp,
				      REFERENCE_TIME rtTimelength);
    HRESULT STDCALL ( *ProcessOutput )(IMediaObject * This,
				       unsigned long dwFlags,
				       unsigned long cOutputBufferCount,
				       /* [size_is][out][in] */ DMO_OUTPUT_DATA_BUFFER *pOutputBuffers,
				       /* [out] */ unsigned long *pdwStatus);
    HRESULT STDCALL ( *Lock )(IMediaObject * This, long bLock);
} IMediaObject_vt;
struct _IMediaObject { IMediaObject_vt* vt; };

/*
 * IEnumDMO interface
 */
typedef struct _IEnumDMO IEnumDMO;
typedef struct IEnumDMO_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *Next )(IEnumDMO * This,
			      unsigned long cItemsToFetch,
			      /* [length_is][size_is][out] */ CLSID *pCLSID,
			      /* [string][length_is][size_is][out] */ WCHAR **Names,
			      /* [out] */ unsigned long *pcItemsFetched);
    HRESULT STDCALL ( *Skip )(IEnumDMO * This,
			      unsigned long cItemsToSkip);
    HRESULT STDCALL ( *Reset )(IEnumDMO * This);
    HRESULT STDCALL ( *Clone )(IEnumDMO * This,
			       /* [out] */ IEnumDMO **ppEnum);
} IEnumDMO_vt;
struct _IEnumDMO { IEnumDMO_vt* vt; };

/*
 * IMediaObjectInPlace interface
 */
typedef struct _IMediaObjectInPlace IMediaObjectInPlace;
typedef struct IMediaObjectInPlace_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *Process )(IMediaObjectInPlace * This,
				 /* [in] */ unsigned long ulSize,
				 /* [size_is][out][in] */ BYTE *pData,
				 /* [in] */ REFERENCE_TIME refTimeStart,
				 /* [in] */ unsigned long dwFlags);
    HRESULT STDCALL ( *Clone )(IMediaObjectInPlace * This,
			       /* [out] */ IMediaObjectInPlace **ppMediaObject);
    HRESULT STDCALL ( *GetLatency )(IMediaObjectInPlace * This,
				    /* [out] */ REFERENCE_TIME *pLatencyTime);

} IMediaObjectInPlace_vt;
struct _IMediaObjectInPlace { IMediaObjectInPlace_vt* vt; };


/*
 * IDMOQualityControl interface
 */
typedef struct _IDMOQualityControl IDMOQualityControl;
typedef struct IDMOQualityControl_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *SetNow )(IDMOQualityControl * This,
				/* [in] */ REFERENCE_TIME rtNow);
    HRESULT STDCALL ( *SetStatus )(IDMOQualityControl * This,
				   /* [in] */ unsigned long dwFlags);
    HRESULT STDCALL ( *GetStatus )(IDMOQualityControl * This,
				   /* [out] */ unsigned long *pdwFlags);
} IDMOQualityControl_vt;
struct _IDMOQualityControl { IDMOQualityControl_vt* vt; };

/*
 * IDMOVideoOutputOptimizations  interface
 */
typedef struct _IDMOVideoOutputOptimizations  IDMOVideoOutputOptimizations;
typedef struct IDMOVideoOutputOptimizations_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *QueryOperationModePreferences )(IDMOVideoOutputOptimizations * This,
						       unsigned long ulOutputStreamIndex,
						       unsigned long *pdwRequestedCapabilities);
    HRESULT STDCALL ( *SetOperationMode )(IDMOVideoOutputOptimizations * This,
					  unsigned long ulOutputStreamIndex,
					  unsigned long dwEnabledFeatures);
    HRESULT STDCALL ( *GetCurrentOperationMode )(IDMOVideoOutputOptimizations * This,
						 unsigned long ulOutputStreamIndex,
						 unsigned long *pdwEnabledFeatures);
    HRESULT STDCALL ( *GetCurrentSampleRequirements )(IDMOVideoOutputOptimizations * This,
						      unsigned long ulOutputStreamIndex,
						      unsigned long *pdwRequestedFeatures);
} IDMOVideoOutputOptimizations_vt;
struct _IDMOVideoOutputOptimizations { IDMOVideoOutputOptimizations_vt* vt; };

#endif /* DMO_INTERFACE_H */
