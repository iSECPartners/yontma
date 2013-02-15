#include "stdafx.h"

HRESULT LoadWmi()
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

void CleanupWmi()
{
    CoUninitialize();
}

HRESULT GetNamespace(__in PCWSTR pObjectPath,
                     __out IWbemServices** ppNamespace)
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