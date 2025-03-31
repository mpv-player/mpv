#include "stdafx.h"
#include "DSMResourceBag.h"

#include <algorithm>

CDSMResource::CDSMResource()
    : mime(L"application/octet-stream")
{
}

CDSMResource::CDSMResource(LPCWSTR name, LPCWSTR desc, LPCWSTR mime, BYTE *pData, int len, DWORD_PTR tag)
{
    this->name = name;
    this->desc = desc;
    this->mime = mime;
    data.resize(len);
    memcpy(data.data(), pData, data.size());
    this->tag = tag;
}

CDSMResource &CDSMResource::operator=(const CDSMResource &r)
{
    if (this != &r)
    {
        name = r.name;
        desc = r.desc;
        mime = r.mime;
        data = r.data;
        tag = r.tag;
    }
    return *this;
}

CDSMResourceBag::CDSMResourceBag()
{
}

CDSMResourceBag::~CDSMResourceBag()
{
    m_resources.clear();
}

STDMETHODIMP_(DWORD) CDSMResourceBag::ResGetCount()
{
    CAutoLock lock(&m_csResources);
    return (DWORD)m_resources.size();
}

STDMETHODIMP CDSMResourceBag::ResGet(DWORD iIndex, BSTR *ppName, BSTR *ppDesc, BSTR *ppMime, BYTE **ppData,
                                     DWORD *pDataLen, DWORD_PTR *pTag)
{
    CAutoLock lock(&m_csResources);

    if (ppData && !pDataLen)
        return E_INVALIDARG;

    if (iIndex >= m_resources.size())
        return E_INVALIDARG;

    CDSMResource &r = m_resources[iIndex];

    if (ppName)
    {
        *ppName = SysAllocString(r.name.data());
        if (*ppName == NULL)
            return E_OUTOFMEMORY;
    }
    if (ppDesc)
    {
        *ppDesc = SysAllocString(r.desc.data());
        if (*ppDesc == NULL)
            return E_OUTOFMEMORY;
    }
    if (ppMime)
    {
        *ppMime = SysAllocString(r.mime.data());
        if (*ppMime == NULL)
            return E_OUTOFMEMORY;
    }
    if (ppData)
    {
        *pDataLen = (DWORD)r.data.size();
        memcpy(*ppData = (BYTE *)CoTaskMemAlloc(*pDataLen), r.data.data(), *pDataLen);
    }
    if (pTag)
    {
        *pTag = r.tag;
    }

    return S_OK;
}

STDMETHODIMP CDSMResourceBag::ResSet(DWORD iIndex, LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime, const BYTE *pData,
                                     DWORD len, DWORD_PTR tag)
{
    CAutoLock lock(&m_csResources);

    if (iIndex >= m_resources.size())
        return E_INVALIDARG;

    CDSMResource &r = m_resources[iIndex];

    if (pName)
        r.name = pName;

    if (pDesc)
        r.desc = pDesc;

    if (pMime)
        r.mime = pMime;

    if (pData || len == 0)
    {
        r.data.resize(len);
        if (pData)
        {
            memcpy(r.data.data(), pData, r.data.size());
        }
    }

    r.tag = tag;

    return S_OK;
}

STDMETHODIMP CDSMResourceBag::ResAppend(LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime, BYTE *pData, DWORD len,
                                        DWORD_PTR tag)
{
    CAutoLock lock(&m_csResources);
    m_resources.push_back(CDSMResource());
    return ResSet((DWORD)m_resources.size() - 1, pName, pDesc, pMime, pData, len, tag);
}

STDMETHODIMP CDSMResourceBag::ResRemoveAt(DWORD iIndex)
{
    CAutoLock lock(&m_csResources);

    if (iIndex >= m_resources.size())
        return E_INVALIDARG;

    m_resources.erase(m_resources.cbegin() + iIndex);

    return S_OK;
}

STDMETHODIMP CDSMResourceBag::ResRemoveAll(DWORD_PTR tag)
{
    CAutoLock lock(&m_csResources);

    if (tag)
    {
        m_resources.erase(
            std::remove_if(m_resources.begin(), m_resources.end(), [&](const CDSMResource &r) { return r.tag == tag; }),
            m_resources.end());
    }
    else
    {
        m_resources.clear();
    }

    return S_OK;
}
