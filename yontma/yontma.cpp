#include "stdafx.h"

#ifdef _DEBUG

//
// To faciliitate debugging, uncomment the following line to make yontma skip actual
// hibernation.
//

//#define DO_NOT_REALLY_HIBERNATE

#ifdef DO_NOT_REALLY_HIBERNATE
#define SetSuspendState(__x, __y, __z)
#endif

#endif

#define CMD_PARAM_INSTALL               TEXT("-i")
#define CMD_PARAM_UNINSTALL             TEXT("-u")
#define CMD_PARAM_RUN_AS_SERVICE        TEXT("as_svc")

int _tmain(int argc, _TCHAR* argv[])
{
    HRESULT hr;

    hr = ProcessCommandLine(argc, argv);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

cleanexit:
    if(HB_FAILED(hr)) {
        return 1;
    }

    return 0;
}

HRESULT ProcessCommandLine(int argc, _TCHAR* argv[])
{
    HRESULT hr;
    
    if(argc != 2) {
        PrintUsage();
        hr = E_FAIL;
        goto cleanexit;
    }

    if(_tcscmp(argv[1], CMD_PARAM_INSTALL) == 0) {
        hr = PerformInstall();
        goto cleanexit;
    }
    else if(_tcscmp(argv[1], CMD_PARAM_UNINSTALL) == 0) {
        hr = PerformUninstall();
        goto cleanexit;
    }
    else if(_tcscmp(argv[1], CMD_PARAM_RUN_AS_SERVICE) == 0) {
        PerformRunAsService();
        hr = S_OK;
    }
    else {
        PrintUsage();
        hr = E_FAIL;
        goto cleanexit;
    }

cleanexit:

    return hr;
}

HRESULT PerformInstall(void)
{
    HRESULT hr;

    hr = CheckYontmaRequirements();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = InstallYontma();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

cleanexit:

    return hr;
}

HRESULT PerformUninstall(void)
{
    return RemoveYontma();
}

void PerformRunAsService(void)
{
    SERVICE_TABLE_ENTRY stbl[] = {
                                    {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
                                    {NULL, NULL}
                                 };

    InitLogging();

    StartServiceCtrlDispatcher(stbl);
}

void __stdcall ServiceMain(int argc, char* argv[])
{
    HRESULT hr;
    static const int DisconnectACEventIndex = 0;
    static const int DisconnectWiredEthernetEventIndex = 1;
    static const int ServiceEndEventIndex = 2;
    static const int TotalEvents = ServiceEndEventIndex + 1;
    HANDLE HandleArray[TotalEvents];
    SERVICE_HANDLER_PARAMS serviceHandlerParams = {0};
    BOOL bExitService = FALSE;

    WriteLineToLog("In ServiceMain");

    HandleArray[ServiceEndEventIndex] = CreateEvent(NULL,
                                                    TRUE,
                                                    FALSE,
                                                    NULL);
    if(HandleArray[ServiceEndEventIndex] == NULL) {
        WriteLineToLog("ServiceMain: Couldn't create service end event");
        return;
    }

    HandleArray[DisconnectACEventIndex] = CreateEvent(NULL,
                                                      TRUE,
                                                      FALSE,
                                                      NULL);
    HandleArray[DisconnectWiredEthernetEventIndex] = CreateEvent(NULL,
                                                                 TRUE,
                                                                 FALSE,
                                                                 NULL);

    if((!HandleArray[DisconnectACEventIndex]) || (!HandleArray[DisconnectWiredEthernetEventIndex])) {
        bExitService = TRUE;
    }

    serviceHandlerParams.hServiceEndEvent = HandleArray[ServiceEndEventIndex];
    serviceHandlerParams.hMonitorStopEvent = CreateEvent(NULL,
                                                         TRUE,
                                                         FALSE,
                                                         NULL);
    serviceHandlerParams.hACDisconnectedEvent = HandleArray[DisconnectACEventIndex];
    serviceHandlerParams.hWiredEthernetDisconnectedEvent = HandleArray[DisconnectWiredEthernetEventIndex];

    hr = RunYontmaService(&serviceHandlerParams);
    if(HB_FAILED(hr)) {
        WriteLineToLog("ServiceMain: RunYontmaService error");
        return;
    }

    WriteLineToLog("ServiceMain: Going into main loop");

    while(!bExitService) {
        switch(WaitForMultipleObjects(TotalEvents,
                                      HandleArray,
                                      FALSE,
                                      DEFAULT_SLEEP_TIME)) {
            case WAIT_OBJECT_0 + DisconnectACEventIndex: //AC was disconnected
                RemoveYontma();
                WriteLineToLog("ServiceMain: AC disconnected -> suspend");
                HibernateMachine();
                break;
            case WAIT_OBJECT_0 + DisconnectWiredEthernetEventIndex: //wired ether was disconnected
                RemoveYontma();
                WriteLineToLog("ServiceMain: Wired ether disconnected -> suspend");
                HibernateMachine();
                break;
            case WAIT_OBJECT_0 + ServiceEndEventIndex:
                WriteLineToLog("ServiceMain: Service stopped");
                bExitService = TRUE;
                break;
            case WAIT_TIMEOUT: //wait time was satisfied
                break;
            default: //some other error, bail
                WriteLineToLog("ServiceMain: Unknown error");
                bExitService = TRUE;
                break;
        }
    }

    WriteLineToLog("ServiceMain: Exiting");

    StopYontmaService();
}

DWORD WINAPI ServiceHandlerEx(DWORD dwControl,
                              DWORD dwEventType,
                              LPVOID lpEventData,
                              LPVOID lpContext)
{
    PSERVICE_HANDLER_PARAMS pServiceHandlerParams = (PSERVICE_HANDLER_PARAMS)lpContext;
    PMONITOR_THREAD_PARAMS pACThreadParams;
    PMONITOR_THREAD_PARAMS pEthernetThreadParams;

    WriteLineToLog("In ServiceHandlerEx");

    if(dwControl == SERVICE_CONTROL_INTERROGATE) return NO_ERROR;
    else if(dwControl == SERVICE_CONTROL_SESSIONCHANGE) {
        if(!lpEventData) return NO_ERROR;

        switch (dwEventType) {
        case WTS_SESSION_LOCK:
        case WTS_SESSION_LOGOFF:
            ResetEvent(pServiceHandlerParams->hMonitorStopEvent);

            pACThreadParams = (PMONITOR_THREAD_PARAMS)malloc(sizeof(MONITOR_THREAD_PARAMS));
            if(!pACThreadParams) {
                return NO_ERROR;
            }
            pACThreadParams->hMonitorStopEvent = pServiceHandlerParams->hMonitorStopEvent;
            pACThreadParams->hMonitorEvent = pServiceHandlerParams->hACDisconnectedEvent;
            CreateThread(NULL,
                         0,
                         PowerMonitorThread,
                         pACThreadParams,
                         0,
                         NULL);

            pEthernetThreadParams = (PMONITOR_THREAD_PARAMS)malloc(sizeof(MONITOR_THREAD_PARAMS));
            if(!pEthernetThreadParams) {
                return NO_ERROR;
            }
            pEthernetThreadParams->hMonitorStopEvent = pServiceHandlerParams->hMonitorStopEvent;
            pEthernetThreadParams->hMonitorEvent = pServiceHandlerParams->hWiredEthernetDisconnectedEvent;
            CreateThread(NULL,
                         0,
                         WiredEthernetMonitorThread,
                         pEthernetThreadParams,
                         0,
                         NULL);

            break;
        case WTS_SESSION_UNLOCK:
        case WTS_SESSION_LOGON:
            SetEvent(pServiceHandlerParams->hMonitorStopEvent);
            break;
        }

        return NO_ERROR;
    }
    else if(dwControl == SERVICE_CONTROL_POWEREVENT) {
        return NO_ERROR;
    }
    else if((dwControl == SERVICE_CONTROL_STOP) || (dwControl == SERVICE_CONTROL_SHUTDOWN)) {
        WriteLineToLog("ServiceHandlerEx: Firing service end event");
        SetEvent(pServiceHandlerParams->hMonitorStopEvent);
        SetEvent(pServiceHandlerParams->hServiceEndEvent);
        return NO_ERROR;
    }
    else return ERROR_CALL_NOT_IMPLEMENTED;
}

HRESULT CheckYontmaRequirements()
{
    HRESULT hr;
    SYSTEM_POWER_CAPABILITIES SystemPwrCap;
    BOOL bLoadedWmi = FALSE;
    BOOL bIsOsVolumeProtectedByBitLocker;

    //
    // Check if machine can hibernate.
    //

    if(!GetPwrCapabilities(&SystemPwrCap)) {
        printf("Unable to get hibernation information, exiting\r\n");
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }
    if(!SystemPwrCap.HiberFilePresent) {
        printf("Hibernation is not enabled on this system, exiting\r\n");
        hr = E_FAIL;
        goto cleanexit;
    }

    hr = LoadWmi();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }
    bLoadedWmi = TRUE;

    //
    // Check for BitLocker.
    //

    hr = IsOsVolumeProtectedByBitLocker(&bIsOsVolumeProtectedByBitLocker);
    if(HB_FAILED(hr)) {
        printf("Error checking BitLocker status. Error=0x%x\r\n", hr);
        printf("Please make sure yontma was executed as administrator\r\n");
        goto cleanexit;
    }
    if(!bIsOsVolumeProtectedByBitLocker) {
        printf("BitLocker is not enabled on the OS drive of the current system, exiting.\r\n");
        hr = E_FAIL;
        goto cleanexit;
    }

cleanexit:
    if(bLoadedWmi) {
        CleanupWmi();
    }

    return hr;
}

HRESULT InstallYontma(void)
{
    HRESULT hr;
    int iStatus;
    SC_HANDLE hService = NULL;
    TCHAR szYontmaInstalledPath[MAX_PATH];
    TCHAR szServiceLaunchCommand[ARRAYSIZE(szYontmaInstalledPath) + ARRAYSIZE(CMD_PARAM_RUN_AS_SERVICE)];

    hr = CopyYontmaBinaryToInstallLocation();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = GetInstallPath(szYontmaInstalledPath, ARRAYSIZE(szYontmaInstalledPath));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = StringCchPrintf(szServiceLaunchCommand,
                         ARRAYSIZE(szServiceLaunchCommand),
                         TEXT("\"%s\" %s"),
                         szYontmaInstalledPath,
                         CMD_PARAM_RUN_AS_SERVICE);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = CreateYontmaService(szServiceLaunchCommand, &hService);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    iStatus = StartService(hService, 0, NULL);
    if(iStatus == 0) {
        printf("StartService error: %d\r\n", GetLastError());
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

cleanexit:
    HB_SAFE_CLOSE_SERVICE_HANDLE(hService);

    return hr;
}

//
// Description:
//  Removes the YoNTMA service from the service manager and deletes the
//  binaries from the install location.
//
HRESULT RemoveYontma(void)
{
    HRESULT hr;
    PTSTR pszServiceExecutionString = NULL;
    TCHAR szInstalledPath[MAX_PATH] = {0};
    
    hr = GetServiceInstalledPath(szInstalledPath,
                                 ARRAYSIZE(szInstalledPath));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = DeleteYontmaService();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    RemoveYontmaBinaryFromInstallLocation(szInstalledPath);

cleanexit:
    HB_SAFE_FREE(pszServiceExecutionString);

    return hr;
}

void HibernateMachine(void)
{
    SetSuspendState(TRUE, TRUE, TRUE);
}

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

DWORD WINAPI PowerMonitorThread(LPVOID lpParams)
{
    SYSTEM_POWER_STATUS PowerStatus;
    PMONITOR_THREAD_PARAMS pMonitorThreadParams = (PMONITOR_THREAD_PARAMS)lpParams;
    
    WriteLineToLog("PowerMonitorThread: Started");

    if(!GetSystemPowerStatus(&PowerStatus)) {
        WriteLineToLog("PowerMonitorThread: Error detecting initial power state.");
        goto cleanexit;
    }
    if(!PowerStatus.ACLineStatus) {
        WriteLineToLog("PowerMonitorThread: Machine was not connected to AC power at lock time.");
        goto cleanexit;
    }

    while(1) {
        if(!GetSystemPowerStatus(&PowerStatus)) {
            continue;
        }
        if(!PowerStatus.ACLineStatus) {
            WriteLineToLog("PowerMonitorThread: Firing monitor event");
            SetEvent(pMonitorThreadParams->hMonitorEvent);
        }
        else {
            ResetEvent(pMonitorThreadParams->hMonitorEvent);
        }
        switch (WaitForSingleObject(pMonitorThreadParams->hMonitorStopEvent, DEFAULT_SLEEP_TIME)) {
        case WAIT_OBJECT_0:
            goto cleanexit;
        case WAIT_TIMEOUT:
            continue;
        }
    }

cleanexit:
    WriteLineToLog("PowerMonitorThread: Exiting");
    HB_SAFE_FREE(pMonitorThreadParams);

    return 0;
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
                    break;
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

    HB_SAFE_FREE(pOriginalAddresses);
    HB_SAFE_FREE(pNewAddresses);
    HB_SAFE_FREE(pMonitorThreadParams);
    
    return 0;
}

#ifdef _DEBUG
void InitLogging(void)
{
    CreateMutex(NULL, FALSE, LOGGING_MUTEX_NAME);
}

void WriteLineToLog(char *pStr)
{
    HANDLE fh;
    DWORD dwBytes;
    HANDLE hLoggingMutex;
    static const char CRLF[] = "\r\n";
    
    hLoggingMutex = OpenMutex(SYNCHRONIZE, FALSE, LOGGING_MUTEX_NAME);
    if(!hLoggingMutex) {
        return;
    }

    WaitForSingleObject(hLoggingMutex, INFINITE);

    fh = CreateFile(TEXT("c:\\yontmalog.txt"),
                    FILE_APPEND_DATA,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
    if(fh) {
        WriteFile(fh,
                  pStr,
                  strlen(pStr),
                  &dwBytes,
                  NULL);
        WriteFile(fh,
                  CRLF,
                  sizeof(CRLF) - sizeof(CRLF[2]),
                  &dwBytes,
                  NULL);
        CloseHandle(fh);
    }

    ReleaseMutex(hLoggingMutex);
}
#endif

void PrintUsage(void)
{
    static const TCHAR usage[] =
TEXT("yontma v0.2\r\n")
TEXT("yontma (You'll Never Take Me Alive!) is a service that helps protect a\r\n")
TEXT("laptop.\r\n")
TEXT("If BitLocker is enabled, it will hibernate a locked laptop if power or wired\r\n")
TEXT("ethernet is disconnected.\r\n")
TEXT("(c)2013 andreas@isecpartners.com\r\n")
TEXT("--------------------------------------------------------------------------------\r\n")
TEXT("Usage:\r\n")
TEXT("  yontma <option>\r\n")
TEXT("Options:\r\n")
TEXT("  ") CMD_PARAM_INSTALL      TEXT("             Installs and starts yontma\r\n")
TEXT("  ") CMD_PARAM_UNINSTALL    TEXT("             Stops and removes yontma\r\n");
    _tprintf(usage);
}