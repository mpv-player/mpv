#ifndef DS_FILTER_H
#define DS_FILTER_H

#include "interfaces.h"
#include "inputpin.h"
#include "outputpin.h"

/**
   User will allocate and fill format structures, call Create(),
   and then set up m_pAll.
 **/

struct DS_Filter
{
    DS_Filter();
    virtual ~DS_Filter();
    void Start();
    void Stop();

    int m_iHandle;
    IBaseFilter* m_pFilter;
    IPin* m_pInputPin;
    IPin* m_pOutputPin;
    
    CBaseFilter* m_pSrcFilter;
    CBaseFilter2* m_pParentFilter;
    IPin* m_pOurInput;
    COutputPin* m_pOurOutput;
    
    AM_MEDIA_TYPE *m_pOurType, *m_pDestType;
    IMemAllocator* m_pAll;
    IMemInputPin* m_pImp;
    int m_iState;

    void Create(const char* dllname, const GUID* id, AM_MEDIA_TYPE* in_fmt, AM_MEDIA_TYPE* out_fmt);
    void SetPointer(char* pointer);

    void destroy();
};

#endif /* DS_FILTER_H */
