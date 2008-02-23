/*
-------------------------------------------------------------------
    AM_MEDIA_TYPE service functions declarations
-------------------------------------------------------------------
*/

#ifndef MPLAYER_MEDIATYPE_H
#define MPLAYER_MEDIATYPE_H

#include "guids.h"
                  
typedef struct __attribute__((__packed__)) MediaType
{
    GUID	majortype;		//0x0
    GUID	subtype;		//0x10
    int		bFixedSizeSamples;	//0x20
    int		bTemporalCompression;	//0x24
    unsigned long lSampleSize;		//0x28
    GUID	formattype;		//0x2c
    IUnknown*	pUnk;			//0x3c
    unsigned long cbFormat;		//0x40
    char*	pbFormat;		//0x44
} AM_MEDIA_TYPE;

/**
 * \brief print info from AM_MEDIA_TYPE structure
 * =param[in] label short lable for media type
 * \param[in] pmt pointer to AM_MEDIA_TYPE
 * 
 * routine used for debug purposes
 *
 */
void DisplayMediaType(const char * label,const AM_MEDIA_TYPE* pmt);
/**
 * \brief frees memory, pointed by pbFormat and pUnk members of AM_MEDIA_TYPE structure
 *
 * \param[in] pmt pointer to structure
 *
 * \note
 * routine does not frees memory allocated for AM_MEDIA_TYPE, so given pointer will be
 * valid after this routine call.
 *
 */
void FreeMediaType(AM_MEDIA_TYPE* pmt);
/**
 * \brief frees memory allocated for AM_MEDIA_TYPE structure, including pbFormat and pUnk
 *        members
 *
 * \param[in] pmt pointer to structure
 *
 * \note
 * after call to this routine, pointer to AM_MEDIA_TYPE will not be valid anymore
 *
 */
void DeleteMediaType(AM_MEDIA_TYPE* pmt);
/**
 * \brief copyies info from source to destination AM_MEDIA_TYPE structures
 *
 * \param[in] pSrc pointer to AM_MEDIA_TYPE structure to copy data from
 * \param[out] pDst pointer to AM_MEDIA_TYPE structure to copy data to
 *
 * \return S_OK - success
 * \return E_POINTER - pSrc or pDst is NULL or (pSrc->cbFormat && !pSrc->pbFormat)
 * \return E_INVALIDARG - (pSrc == pDst) 
 * \return E_OUTOFMEMORY - Insufficient memory
 *
 * \note
 * - pDst must point to existing AM_MEDIA_TYPE structure (all data will be overwritten)
 * - if pDst->pbFormat!=NULL this will cause memory leak (as described in Directshow SDK)!
 *
 */
HRESULT CopyMediaType(AM_MEDIA_TYPE* pDst,const AM_MEDIA_TYPE* pSrc);
/**
 * \brief allocates new AM_MEDIA_TYPE structure and fills it with info from given one
 *
 * \param[in] pSrc pointer to AM_MEDIA_TYPE structure to copy data from
 *
 * \return result code, returned from CopyMediaType
 *
 */
AM_MEDIA_TYPE* CreateMediaType(const AM_MEDIA_TYPE* pSrc);

/**
 * \brief compares two AM_MEDIA_TYPE structures for compatibility
 *
 * \param[in] pmt1 first  AM_MEDIA_TYPE structure for compare
 * \param[in] pmt2 second AM_MEDIA_TYPE structure for compare
 * \param[in] bWildcards 1 means that GUID_NULL of one structure will be compatible with any value of another structure
 *
 * \return 1 if structures are compatible
 * \return 0 if structures are not compatible
 *
 */
int CompareMediaTypes(const AM_MEDIA_TYPE * pmt1, const AM_MEDIA_TYPE * pmt2, int bWildcards);

#endif /* MPLAYER_MEDIA_TYPE_H */
