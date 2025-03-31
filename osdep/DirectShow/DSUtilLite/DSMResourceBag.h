#pragma once

#include "IDSMResourceBag.h"

#include <string>
#include <vector>
#include <map>

class CDSMResource
{
  public:
    CDSMResource();
    CDSMResource(LPCWSTR name, LPCWSTR desc, LPCWSTR mime, BYTE *pData, int len, DWORD_PTR tag = 0);

    CDSMResource &operator=(const CDSMResource &r);

  public:
    DWORD_PTR tag;
    std::wstring name, desc, mime;
    std::vector<BYTE> data;
};

class CDSMResourceBag : public IDSMResourceBag
{
  public:
    CDSMResourceBag();
    virtual ~CDSMResourceBag();

    // IDSMResourceBag
    STDMETHODIMP_(DWORD) ResGetCount();
    STDMETHODIMP ResGet(DWORD iIndex, BSTR *ppName, BSTR *ppDesc, BSTR *ppMime, BYTE **ppData, DWORD *pDataLen,
                        DWORD_PTR *pTag = nullptr);
    STDMETHODIMP ResSet(DWORD iIndex, LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime, const BYTE *pData, DWORD len,
                        DWORD_PTR tag = 0);
    STDMETHODIMP ResAppend(LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime, BYTE *pData, DWORD len, DWORD_PTR tag = 0);
    STDMETHODIMP ResRemoveAt(DWORD iIndex);
    STDMETHODIMP ResRemoveAll(DWORD_PTR tag = 0);

  private:
    CCritSec m_csResources;
    std::vector<CDSMResource> m_resources;
};
