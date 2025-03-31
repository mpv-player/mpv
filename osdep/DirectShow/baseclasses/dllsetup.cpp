//------------------------------------------------------------------------------
// File: DllSetup.cpp
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#include <strsafe.h>

//---------------------------------------------------------------------------
// defines

#define MAX_KEY_LEN 260

//---------------------------------------------------------------------------
// externally defined functions/variable

extern int g_cTemplates;
extern CFactoryTemplate g_Templates[];

//---------------------------------------------------------------------------
//
// EliminateSubKey
//
// Try to enumerate all keys under this one.
// if we find anything, delete it completely.
// Otherwise just delete it.
//
// note - this was pinched/duplicated from
// Filgraph\Mapper.cpp - so should it be in
// a lib somewhere?
//
//---------------------------------------------------------------------------

STDAPI
EliminateSubKey(HKEY hkey, LPCTSTR strSubKey)
{
    HKEY hk;
    if (0 == lstrlen(strSubKey))
    {
        // defensive approach
        return E_FAIL;
    }

    LONG lreturn = RegOpenKeyEx(hkey, strSubKey, 0, MAXIMUM_ALLOWED, &hk);

    ASSERT(lreturn == ERROR_SUCCESS || lreturn == ERROR_FILE_NOT_FOUND || lreturn == ERROR_INVALID_HANDLE);

    if (ERROR_SUCCESS == lreturn)
    {
        // Keep on enumerating the first (zero-th)
        // key and deleting that

        for (;;)
        {
            TCHAR Buffer[MAX_KEY_LEN];
            DWORD dw = MAX_KEY_LEN;
            FILETIME ft;

            lreturn = RegEnumKeyEx(hk, 0, Buffer, &dw, NULL, NULL, NULL, &ft);

            ASSERT(lreturn == ERROR_SUCCESS || lreturn == ERROR_NO_MORE_ITEMS);

            if (ERROR_SUCCESS == lreturn)
            {
                EliminateSubKey(hk, Buffer);
            }
            else
            {
                break;
            }
        }

        RegCloseKey(hk);
        RegDeleteKey(hkey, strSubKey);
    }

    return NOERROR;
}

//---------------------------------------------------------------------------
//
// AMovieSetupRegisterServer()
//
// registers specfied file "szFileName" as server for
// CLSID "clsServer".  A description is also required.
// The ThreadingModel and ServerType are optional, as
// they default to InprocServer32 (i.e. dll) and Both.
//
//---------------------------------------------------------------------------

STDAPI
AMovieSetupRegisterServer(CLSID clsServer, LPCWSTR szDescription, LPCWSTR szFileName,
                          LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType = L"InprocServer32")
{
    // temp buffer
    //
    TCHAR achTemp[MAX_PATH];

    // convert CLSID uuid to string and write
    // out subkey as string - CLSID\{}
    //
    OLECHAR szCLSID[CHARS_IN_GUID];
    HRESULT hr = StringFromGUID2(clsServer, szCLSID, CHARS_IN_GUID);
    ASSERT(SUCCEEDED(hr));

    // create key
    //
    HKEY hkey;
    (void)StringCchPrintf(achTemp, NUMELMS(achTemp), TEXT("CLSID\\%ls"), szCLSID);
    LONG lreturn = RegCreateKey(HKEY_CLASSES_ROOT, (LPCTSTR)achTemp, &hkey);
    if (ERROR_SUCCESS != lreturn)
    {
        return AmHresultFromWin32(lreturn);
    }

    // set description string
    //

    (void)StringCchPrintf(achTemp, NUMELMS(achTemp), TEXT("%ls"), szDescription);
    lreturn = RegSetValue(hkey, (LPCTSTR)NULL, REG_SZ, achTemp, sizeof(achTemp));
    if (ERROR_SUCCESS != lreturn)
    {
        RegCloseKey(hkey);
        return AmHresultFromWin32(lreturn);
    }

    // create CLSID\\{"CLSID"}\\"ServerType" key,
    // using key to CLSID\\{"CLSID"} passed back by
    // last call to RegCreateKey().
    //
    HKEY hsubkey;

    (void)StringCchPrintf(achTemp, NUMELMS(achTemp), TEXT("%ls"), szServerType);
    lreturn = RegCreateKey(hkey, achTemp, &hsubkey);
    if (ERROR_SUCCESS != lreturn)
    {
        RegCloseKey(hkey);
        return AmHresultFromWin32(lreturn);
    }

    // set Server string
    //
    (void)StringCchPrintf(achTemp, NUMELMS(achTemp), TEXT("%ls"), szFileName);
    lreturn = RegSetValue(hsubkey, (LPCTSTR)NULL, REG_SZ, (LPCTSTR)achTemp, sizeof(TCHAR) * (lstrlen(achTemp) + 1));
    if (ERROR_SUCCESS != lreturn)
    {
        RegCloseKey(hkey);
        RegCloseKey(hsubkey);
        return AmHresultFromWin32(lreturn);
    }

    (void)StringCchPrintf(achTemp, NUMELMS(achTemp), TEXT("%ls"), szThreadingModel);
    lreturn = RegSetValueEx(hsubkey, TEXT("ThreadingModel"), 0L, REG_SZ, (CONST BYTE *)achTemp,
                            sizeof(TCHAR) * (lstrlen(achTemp) + 1));

    // close hkeys
    //
    RegCloseKey(hkey);
    RegCloseKey(hsubkey);

    // and return
    //
    return HRESULT_FROM_WIN32(lreturn);
}

//---------------------------------------------------------------------------
//
// AMovieSetupUnregisterServer()
//
// default ActiveMovie dll setup function
// - to use must be called from an exported
//   function named DllRegisterServer()
//
//---------------------------------------------------------------------------

STDAPI
AMovieSetupUnregisterServer(CLSID clsServer)
{
    // convert CLSID uuid to string and write
    // out subkey CLSID\{}
    //
    OLECHAR szCLSID[CHARS_IN_GUID];
    HRESULT hr = StringFromGUID2(clsServer, szCLSID, CHARS_IN_GUID);
    ASSERT(SUCCEEDED(hr));

    TCHAR achBuffer[MAX_KEY_LEN];
    (void)StringCchPrintf(achBuffer, NUMELMS(achBuffer), TEXT("CLSID\\%ls"), szCLSID);

    // delete subkey
    //

    hr = EliminateSubKey(HKEY_CLASSES_ROOT, achBuffer);
    ASSERT(SUCCEEDED(hr));

    // return
    //
    return NOERROR;
}

//---------------------------------------------------------------------------
//
// AMovieSetupRegisterFilter through IFilterMapper2
//
//---------------------------------------------------------------------------

STDAPI
AMovieSetupRegisterFilter2(const AMOVIESETUP_FILTER *const psetupdata, IFilterMapper2 *pIFM2, BOOL bRegister)
{
    DbgLog((LOG_TRACE, 3, TEXT("= AMovieSetupRegisterFilter")));

    // check we've got data
    //
    if (NULL == psetupdata)
        return S_FALSE;

    // unregister filter
    // (as pins are subkeys of filter's CLSID key
    // they do not need to be removed separately).
    //
    DbgLog((LOG_TRACE, 3, TEXT("= = unregister filter")));
    HRESULT hr = pIFM2->UnregisterFilter(0, // default category
                                         0, // default instance name
                                         *psetupdata->clsID);

    if (bRegister)
    {
        REGFILTER2 rf2;
        rf2.dwVersion = 1;
        rf2.dwMerit = psetupdata->dwMerit;
        rf2.cPins = psetupdata->nPins;
        rf2.rgPins = psetupdata->lpPin;

        // register filter
        //
        DbgLog((LOG_TRACE, 3, TEXT("= = register filter")));
        hr = pIFM2->RegisterFilter(*psetupdata->clsID, psetupdata->strName, 0 // moniker
                                   ,
                                   &psetupdata->filterCategory // category
                                   ,
                                   NULL // instance
                                   ,
                                   &rf2);
    }

    // handle one acceptable "error" - that
    // of filter not being registered!
    // (couldn't find a suitable #define'd
    // name for the error!)
    //
    if (0x80070002 == hr)
        return NOERROR;
    else
        return hr;
}

//---------------------------------------------------------------------------
//
// RegisterAllServers()
//
//---------------------------------------------------------------------------

STDAPI
RegisterAllServers(LPCWSTR szFileName, BOOL bRegister)
{
    HRESULT hr = NOERROR;

    for (int i = 0; i < g_cTemplates; i++)
    {
        // get i'th template
        //
        const CFactoryTemplate *pT = &g_Templates[i];

        DbgLog((LOG_TRACE, 2, TEXT("- - register %ls"), (LPCWSTR)pT->m_Name));

        // register CLSID and InprocServer32
        //
        if (bRegister)
        {
            hr = AMovieSetupRegisterServer(*(pT->m_ClsID), (LPCWSTR)pT->m_Name, szFileName);
        }
        else
        {
            hr = AMovieSetupUnregisterServer(*(pT->m_ClsID));
        }

        // check final error for this pass
        // and break loop if we failed
        //
        if (FAILED(hr))
            break;
    }

    return hr;
}

//---------------------------------------------------------------------------
//
// AMovieDllRegisterServer2()
//
// default ActiveMovie dll setup function
// - to use must be called from an exported
//   function named DllRegisterServer()
//
// this function is table driven using the
// static members of the CFactoryTemplate
// class defined in the dll.
//
// it registers the Dll as the InprocServer32
// and then calls the IAMovieSetup.Register
// method.
//
//---------------------------------------------------------------------------

STDAPI
AMovieDllRegisterServer2(BOOL bRegister)
{
    HRESULT hr = NOERROR;

    DbgLog((LOG_TRACE, 2, TEXT("AMovieDllRegisterServer2()")));

    // get file name (where g_hInst is the
    // instance handle of the filter dll)
    //
    WCHAR achFileName[MAX_PATH];

    // WIN95 doesn't support GetModuleFileNameW
    //
    {
        char achTemp[MAX_PATH];

        DbgLog((LOG_TRACE, 2, TEXT("- get module file name")));

        // g_hInst handle is set in our dll entry point. Make sure
        // DllEntryPoint in dllentry.cpp is called
        ASSERT(g_hInst != 0);

        if (0 == GetModuleFileNameA(g_hInst, achTemp, sizeof(achTemp)))
        {
            // we've failed!
            DWORD dwerr = GetLastError();
            return AmHresultFromWin32(dwerr);
        }

        MultiByteToWideChar(CP_ACP, 0L, achTemp, lstrlenA(achTemp) + 1, achFileName, NUMELMS(achFileName));
    }

    //
    // first registering, register all OLE servers
    //
    if (bRegister)
    {
        DbgLog((LOG_TRACE, 2, TEXT("- register OLE Servers")));
        hr = RegisterAllServers(achFileName, TRUE);
    }

    //
    // next, register/unregister all filters
    //

    if (SUCCEEDED(hr))
    {
        // init is ref counted so call just in case
        // we're being called cold.
        //
        DbgLog((LOG_TRACE, 2, TEXT("- CoInitialize")));
        hr = CoInitialize((LPVOID)NULL);
        ASSERT(SUCCEEDED(hr));

        // get hold of IFilterMapper2
        //
        DbgLog((LOG_TRACE, 2, TEXT("- obtain IFilterMapper2")));
        IFilterMapper2 *pIFM2 = 0;
        IFilterMapper *pIFM = 0;
        hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper2, (void **)&pIFM2);
        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 2, TEXT("- trying IFilterMapper instead")));

            hr = CoCreateInstance(CLSID_FilterMapper, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper, (void **)&pIFM);
        }
        if (SUCCEEDED(hr))
        {
            // scan through array of CFactoryTemplates
            // registering servers and filters.
            //
            DbgLog((LOG_TRACE, 2, TEXT("- register Filters")));
            for (int i = 0; i < g_cTemplates; i++)
            {
                // get i'th template
                //
                const CFactoryTemplate *pT = &g_Templates[i];

                if (NULL != pT->m_pAMovieSetup_Filter)
                {
                    DbgLog((LOG_TRACE, 2, TEXT("- - register %ls"), (LPCWSTR)pT->m_Name));

                    if (pIFM2)
                    {
                        hr = AMovieSetupRegisterFilter2(pT->m_pAMovieSetup_Filter, pIFM2, bRegister);
                    }
                    else
                    {
                        hr = AMovieSetupRegisterFilter(pT->m_pAMovieSetup_Filter, pIFM, bRegister);
                    }
                }

                // check final error for this pass
                // and break loop if we failed
                //
                if (FAILED(hr))
                    break;
            }

            // release interface
            //
            if (pIFM2)
                pIFM2->Release();
            else
                pIFM->Release();
        }

        // and clear up
        //
        CoFreeUnusedLibraries();
        CoUninitialize();
    }

    //
    // if unregistering, unregister all OLE servers
    //
    if (SUCCEEDED(hr) && !bRegister)
    {
        DbgLog((LOG_TRACE, 2, TEXT("- register OLE Servers")));
        hr = RegisterAllServers(achFileName, FALSE);
    }

    DbgLog((LOG_TRACE, 2, TEXT("- return %0x"), hr));
    return hr;
}

//---------------------------------------------------------------------------
//
// AMovieDllRegisterServer()
//
// default ActiveMovie dll setup function
// - to use must be called from an exported
//   function named DllRegisterServer()
//
// this function is table driven using the
// static members of the CFactoryTemplate
// class defined in the dll.
//
// it registers the Dll as the InprocServer32
// and then calls the IAMovieSetup.Register
// method.
//
//---------------------------------------------------------------------------

STDAPI
AMovieDllRegisterServer(void)
{
    HRESULT hr = NOERROR;

    // get file name (where g_hInst is the
    // instance handle of the filter dll)
    //
    WCHAR achFileName[MAX_PATH];

    {
        // WIN95 doesn't support GetModuleFileNameW
        //
        char achTemp[MAX_PATH];

        if (0 == GetModuleFileNameA(g_hInst, achTemp, sizeof(achTemp)))
        {
            // we've failed!
            DWORD dwerr = GetLastError();
            return AmHresultFromWin32(dwerr);
        }

        MultiByteToWideChar(CP_ACP, 0L, achTemp, lstrlenA(achTemp) + 1, achFileName, NUMELMS(achFileName));
    }

    // scan through array of CFactoryTemplates
    // registering servers and filters.
    //
    for (int i = 0; i < g_cTemplates; i++)
    {
        // get i'th template
        //
        const CFactoryTemplate *pT = &g_Templates[i];

        // register CLSID and InprocServer32
        //
        hr = AMovieSetupRegisterServer(*(pT->m_ClsID), (LPCWSTR)pT->m_Name, achFileName);

        // instantiate all servers and get hold of
        // IAMovieSetup, if implemented, and call
        // IAMovieSetup.Register() method
        //
        if (SUCCEEDED(hr) && (NULL != pT->m_lpfnNew))
        {
            // instantiate object
            //
            PAMOVIESETUP psetup;
            hr = CoCreateInstance(*(pT->m_ClsID), 0, CLSCTX_INPROC_SERVER, IID_IAMovieSetup,
                                  reinterpret_cast<void **>(&psetup));
            if (SUCCEEDED(hr))
            {
                hr = psetup->Unregister();
                if (SUCCEEDED(hr))
                    hr = psetup->Register();
                psetup->Release();
            }
            else
            {
                if ((E_NOINTERFACE == hr) || (VFW_E_NEED_OWNER == hr))
                    hr = NOERROR;
            }
        }

        // check final error for this pass
        // and break loop if we failed
        //
        if (FAILED(hr))
            break;

    } // end-for

    return hr;
}

//---------------------------------------------------------------------------
//
// AMovieDllUnregisterServer()
//
// default ActiveMovie dll uninstall function
// - to use must be called from an exported
//   function named DllRegisterServer()
//
// this function is table driven using the
// static members of the CFactoryTemplate
// class defined in the dll.
//
// it calls the IAMovieSetup.Unregister
// method and then unregisters the Dll
// as the InprocServer32
//
//---------------------------------------------------------------------------

STDAPI
AMovieDllUnregisterServer()
{
    // initialize return code
    //
    HRESULT hr = NOERROR;

    // scan through CFactory template and unregister
    // all OLE servers and filters.
    //
    for (int i = g_cTemplates; i--;)
    {
        // get i'th template
        //
        const CFactoryTemplate *pT = &g_Templates[i];

        // check method exists
        //
        if (NULL != pT->m_lpfnNew)
        {
            // instantiate object
            //
            PAMOVIESETUP psetup;
            hr = CoCreateInstance(*(pT->m_ClsID), 0, CLSCTX_INPROC_SERVER, IID_IAMovieSetup,
                                  reinterpret_cast<void **>(&psetup));
            if (SUCCEEDED(hr))
            {
                hr = psetup->Unregister();
                psetup->Release();
            }
            else
            {
                if ((E_NOINTERFACE == hr) || (VFW_E_NEED_OWNER == hr))
                    hr = NOERROR;
            }
        }

        // unregister CLSID and InprocServer32
        //
        if (SUCCEEDED(hr))
        {
            hr = AMovieSetupUnregisterServer(*(pT->m_ClsID));
        }

        // check final error for this pass
        // and break loop if we failed
        //
        if (FAILED(hr))
            break;
    }

    return hr;
}
