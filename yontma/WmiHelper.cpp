#include "stdafx.h"

HRESULT LoadWmi(void)
{
    HRESULT hr;

    hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = CoInitializeSecurity(NULL,                         // Security descriptor
                              -1,                           // COM authentication
                              NULL,                         // Authentication services
                              NULL,                         // Reserved
                              RPC_C_AUTHN_LEVEL_DEFAULT,    // Default authentication
                              RPC_C_IMP_LEVEL_IMPERSONATE,  // Default Impersonation
                              NULL,                         // Authentication info
                              EOAC_NONE,                    // Additional capabilities
                              NULL                          // Reserved
                              );
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

cleanexit:

    return hr;
}

void CleanupWmi(void)
{
    CoUninitialize();
}

HRESULT GetNamespace(__in PCWSTR pObjectPath, __out IWbemServices** ppNamespace)
{
    HRESULT hr;
    IWbemLocator *pLocator = NULL;
    IWbemServices *pNamespaceLocal = NULL;

    hr = CoCreateInstance(CLSID_WbemLocator,
                          0,
                          CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator,
                          (LPVOID *) &pLocator);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = pLocator->ConnectServer(_bstr_t(pObjectPath),      // Object path of WMI namespace
                                 NULL,                      // User name
                                 NULL,                      // User password
                                 0,                         // Locale
                                 NULL,                      // Security flags
                                 0,                         // Authority
                                 0,                         // Context object
                                 &pNamespaceLocal
                                 );

    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = CoSetProxyBlanket(pNamespaceLocal,
                           RPC_C_AUTHN_WINNT,
                           RPC_C_AUTHZ_NONE,
                           NULL,
                           RPC_C_AUTHN_LEVEL_CALL,
                           RPC_C_IMP_LEVEL_IMPERSONATE,
                           NULL,
                           EOAC_NONE
                           );
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    *ppNamespace = pNamespaceLocal;
    pNamespaceLocal = NULL;

cleanexit:
    HB_SAFE_RELEASE(pLocator);
    HB_SAFE_RELEASE(pNamespaceLocal);

    return hr;
}

HRESULT GetInputParameters(__in IWbemServices* pSvc, __in PCWSTR pClassName,  __in PCWSTR pMethodName, __out IWbemClassObject** ppInParams)
{
    HRESULT hr;
    IWbemClassObject* pClass = NULL;
    IWbemClassObject* pInParamsDefinition = NULL;
    IWbemClassObject* pInParamsLocal = NULL;

    hr = pSvc->GetObject(_bstr_t(pClassName),
                         0,
                         NULL,
                         &pClass,
                         NULL);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = pClass->GetMethod(_bstr_t(pMethodName),
                           0,
                           &pInParamsDefinition,
                           NULL);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = pInParamsDefinition->SpawnInstance(0, &pInParamsLocal);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    *ppInParams = pInParamsLocal;
    pInParamsLocal = NULL;

cleanexit:
    HB_SAFE_RELEASE(pClass);
    HB_SAFE_RELEASE(pInParamsDefinition);
    HB_SAFE_RELEASE(pInParamsLocal);

    return hr;
}

//
// Description:
//  Converts a BSTR to an equivalent PWSTR.
//
// Parameters:
//  bstr - A string represented as a BSTR.
//
//  ppWStr - On success, is set to the PWSTR equivalent of bstr. Caller must
//      free with HB_SAFE_FREE.
//
HRESULT BStrToPWchar(__in BSTR bstr, __out PWSTR* ppWStr)
{
    HRESULT hr;
    size_t cbWStr;
    PWSTR pWStrLocal = NULL;

    hr = SizeTMult(SysStringLen(bstr), sizeof(WCHAR), &cbWStr);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    //
    // Correct for additional NULL pointer
    //

    hr = SizeTAdd(cbWStr, sizeof(WCHAR), &cbWStr);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    pWStrLocal = (PWSTR)malloc(cbWStr);
    if(!pWStrLocal) {
        hr = E_OUTOFMEMORY;
        goto cleanexit;
    }

    hr =  StringCbCopy(pWStrLocal, cbWStr, static_cast<wchar_t*>(bstr));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    *ppWStr = pWStrLocal;
    pWStrLocal = NULL;

cleanexit:
    HB_SAFE_FREE(pWStrLocal);

    return hr;
}

//
// Description:
//  Converts a BSTR array in a VARIANT to an equivalent array of PWSTRs.
//
// Parameters:
//  vtStringArray - A string array stored in a VARIANT that wraps a SAFEARRAY.
//
//  pppWstrArray - On success, receives an array of PWSTR* populated with the
//      values from vtStringArray represented as PWSTRs. Caller must free with
//      HB_SAFE_FREE_ARRAY.
//
//  pcWstrArray - On success, is set to the number of elements in pppWstrArray.
//
HRESULT VariantStringArrayToWstrArray(__in VARIANT vtStringArray, __out PWSTR** pppWstrArray, __out size_t* pcWstrArray)
{
    HRESULT hr;
    SAFEARRAY* arrStrings;
    size_t cWstrArrayLocal = 0;
    size_t cbWstrArrayLocal;
    PWSTR* ppWstrArrayLocal = NULL;
    BSTR bstrCurrent;

    arrStrings = vtStringArray.parray;

    cWstrArrayLocal = arrStrings->rgsabound[0].cElements;

    hr = SizeTMult(cWstrArrayLocal, sizeof(PWSTR*), &cbWstrArrayLocal);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    ppWstrArrayLocal = (PWSTR*)malloc(cbWstrArrayLocal);
    if(!ppWstrArrayLocal) {
        hr = E_OUTOFMEMORY;
        goto cleanexit;
    }

    //
    // Zero out the array so that elements can be safely deleted if
    // initialization partway through.
    //

    memset(ppWstrArrayLocal, 0, cbWstrArrayLocal);

    for(size_t stringIndex = 0; stringIndex < cWstrArrayLocal; stringIndex++) {
        bstrCurrent = ((BSTR*)arrStrings->pvData)[stringIndex];
        hr = BStrToPWchar(bstrCurrent, &ppWstrArrayLocal[stringIndex]);
        if(HB_FAILED(hr)) {
            goto cleanexit;
        }
    }

    *pppWstrArray = ppWstrArrayLocal;
    ppWstrArrayLocal = NULL;

    *pcWstrArray = cWstrArrayLocal;
    cWstrArrayLocal = 0;

cleanexit:
    HB_SAFE_FREE_ARRAY(ppWstrArrayLocal, cWstrArrayLocal);

    return hr;
}
