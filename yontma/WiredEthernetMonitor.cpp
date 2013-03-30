#include "stdafx.h"

HRESULT GetInternetAdapterAddresses(__inout PIP_ADAPTER_ADDRESSES* ppAdapterAddresses,__inout PULONG pAdapterAddressesSize);

HRESULT GetInternetAdapterAddresses(__inout PIP_ADAPTER_ADDRESSES* ppAdapterAddresses,__inout PULONG pAdapterAddressesSize)
{
    HRESULT hr;
    ULONG rc = 0;

    rc = GetAdaptersAddresses(AF_UNSPEC,
                              GAA_FLAG_INCLUDE_PREFIX,
                              NULL,
                              *ppAdapterAddresses,
                              pAdapterAddressesSize);
    if(rc == ERROR_SUCCESS) {

        //
        // Our original buffer was large enough to store the result, so we're done.
        //

        hr = S_OK;
        goto cleanexit;
    }
    else if(rc != ERROR_BUFFER_OVERFLOW) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    //
    // Our original buffer couldn't store the result, so we need to allocate a larger buffer.
    //

    HB_SAFE_FREE(*ppAdapterAddresses);
    *ppAdapterAddresses = (PIP_ADAPTER_ADDRESSES)malloc(*pAdapterAddressesSize);
    if(*ppAdapterAddresses == NULL) {
        hr = E_OUTOFMEMORY;
        goto cleanexit;
    }

    rc = GetAdaptersAddresses(AF_UNSPEC,
                              GAA_FLAG_INCLUDE_PREFIX,
                              NULL,
                              *ppAdapterAddresses,
                              pAdapterAddressesSize);
    if(rc != ERROR_SUCCESS) {
        hr = HRESULT_FROM_WIN32(rc);
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:

    return hr;
}

DWORD WINAPI WiredEthernetMonitorThread(LPVOID lpParams)
{
    HRESULT hr;
    PMONITOR_THREAD_PARAMS pMonitorThreadParams = (PMONITOR_THREAD_PARAMS)lpParams;
    PIP_ADAPTER_ADDRESSES pOriginalAddresses = NULL;
    ULONG originalAddressesSize = 0;
    PIP_ADAPTER_ADDRESSES pNewAddresses = NULL;
    ULONG newAddressesSize = 0;
    PIP_ADAPTER_ADDRESSES pCurrOriginalAddress = NULL;
    PIP_ADAPTER_ADDRESSES pCurrNewAddress = NULL;

    WriteLineToLog("WiredEtherMonitorThread: Started");

    hr = GetInternetAdapterAddresses(&pOriginalAddresses, &originalAddressesSize);
    if(HB_FAILED(hr)) {
        WriteLineToLog("WiredEtherMonitorThread: Failed to get original adapter addresses");
        goto cleanexit;
    }

    while(1) {
        hr = GetInternetAdapterAddresses(&pNewAddresses, &newAddressesSize);
        if(HB_FAILED(hr)) {
            WriteLineToLog("WiredEtherMonitorThread: Failed to get new adapter addresses");
            goto cleanexit;
        }
        
        pCurrOriginalAddress = pOriginalAddresses;
        pCurrNewAddress = pNewAddresses;
        while(pCurrOriginalAddress && pCurrNewAddress) {
            if(pCurrNewAddress->IfType == IF_TYPE_ETHERNET_CSMACD) {
                if((pCurrNewAddress->OperStatus == IfOperStatusDown) && (pCurrOriginalAddress->OperStatus == IfOperStatusUp)) {
                    WriteLineToLog("WiredEtherMonitorThread: Firing monitor event");
                    SetEvent(pMonitorThreadParams->hMonitorEvent);
                    goto cleanexit;
                }
                else {
                    ResetEvent(pMonitorThreadParams->hMonitorEvent);
                }
            }
            pCurrOriginalAddress = pCurrOriginalAddress->Next;
            pCurrNewAddress = pCurrNewAddress->Next;
        }
        
        switch (WaitForSingleObject(pMonitorThreadParams->hMonitorStopEvent, DEFAULT_SLEEP_TIME)) {
        case WAIT_OBJECT_0:
            goto cleanexit;
        case WAIT_TIMEOUT:
            continue;
        }
    }

cleanexit:
    WriteLineToLog("WiredEtherMonitorThread: Exiting");
    InterlockedIncrement(pMonitorThreadParams->pMonitorsCompleted);

    HB_SAFE_FREE(pOriginalAddresses);
    HB_SAFE_FREE(pNewAddresses);
    HB_SAFE_FREE(pMonitorThreadParams);

    return 0;
}
