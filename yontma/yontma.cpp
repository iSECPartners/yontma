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
    int rc;
    HRESULT hr;
    SC_HANDLE hSCManager = NULL;
    SERVICE_TABLE_ENTRY stbl[] = {
                                    {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
                                    {NULL, NULL}
                                 };
    SYSTEM_POWER_CAPABILITIES SystemPwrCap;
    BOOL bLoadedWmi = FALSE;
    BOOL bIsOsVolumeProtectedByBitLocker;

    InitLogging();

    //
    // Check if yontma was launched as a service or manually by the user.
    //

    if ((argc == 2) && (_tcscmp(argv[1], CMD_PARAM_RUN_AS_SERVICE) == 0)) {
        StartServiceCtrlDispatcher(stbl);
    }
    else {
        //check if machine can hibernate
        if(!GetPwrCapabilities(&SystemPwrCap)) {
            printf("Unable to get hibernation information, exiting\r\n");
            rc = 1;
            goto cleanexit;
        }
        if(!SystemPwrCap.HiberFilePresent) {
            printf("Hibernation is not enabled on this system, exiting\r\n");
            rc = 1;
            goto cleanexit;
        }

        hr = LoadWmi();
        if(HB_FAILED(hr)) {
            rc = 1;
            goto cleanexit;
        }

        //check for bitlocker
        hr = IsOsVolumeProtectedByBitLocker(&bIsOsVolumeProtectedByBitLocker);
        if(HB_FAILED(hr)) {
            printf("Error checking BitLocker status. Error=0x%x\r\n", hr);
            printf("Please make sure yontma was executed as administrator\r\n");
            rc = 1;
            goto cleanexit;
        }
        if(!bIsOsVolumeProtectedByBitLocker) {
            printf("BitLocker is not enabled on the OS drive of the current system, exiting.\r\n");
            rc = 1;
            goto cleanexit;
        }
        if(argc != 2) {
            usage();
            rc = 1;
            goto cleanexit;
        }

        if(_tcscmp(argv[1], CMD_PARAM_INSTALL) == 0) {
            rc = InstallYontma();
            goto cleanexit;
        }
        else if(_tcscmp(argv[1], CMD_PARAM_UNINSTALL) == 0) {
            rc = RemoveYontma();
            goto cleanexit;
        }
        else {
            usage();
            rc = 1;
            goto cleanexit;
        }
    }
    rc = 0;

cleanexit:
    if(bLoadedWmi) {
        CleanupWmi();
    }

    return rc;
}

void __stdcall ServiceMain(int argc, char* argv[])
{
    HRESULT hr;
    static const int DisconnectACEventIndex = 0;
    static const int DisconnectWiredEthernetEventIndex = 1;
    static const int ServiceEndEventIndex = 2;
    static const int TotalEvents = ServiceEndEventIndex + 1;
    HANDLE HandleArray[TotalEvents];
    MONITOR_THREAD_PARAMS acEventParams = {0};
    MONITOR_THREAD_PARAMS ethernetEventParams = {0};
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

DWORD InstallYontma(void)
{
    HRESULT hr;
    DWORD dwReturn = 1;
    int iStatus;
    SC_HANDLE hService = NULL;
    TCHAR szFullPathName[MAX_PATH];
    TCHAR szServiceLaunchCommand[ARRAYSIZE(szFullPathName) + ARRAYSIZE(CMD_PARAM_RUN_AS_SERVICE)];

    if(!GetModuleFileName(NULL, szFullPathName, ARRAYSIZE(szFullPathName))) {
        printf("GetModuleFileName error: %d\r\n", GetLastError());
        goto cleanexit;
    }

    hr = StringCchPrintf(szServiceLaunchCommand,
                         ARRAYSIZE(szServiceLaunchCommand),
                         TEXT("%s %s"),
                         szFullPathName,
                         CMD_PARAM_RUN_AS_SERVICE);
    if (HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = CreateYontmaService(szServiceLaunchCommand, &hService);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    iStatus = StartService(hService, 0, NULL);
    if(iStatus == 0) {
        printf("StartService error: %d\r\n", GetLastError());
        goto cleanexit;
    }

    dwReturn = 0;

cleanexit:
    HB_SAFE_CLOSE_SERVICE_HANDLE(hService);

    return dwReturn;
}

DWORD RemoveYontma(void)
{
    HRESULT hr;
    DWORD dwReturn = 1;

    hr = DeleteYontmaService();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    dwReturn = 0;

cleanexit:
    return dwReturn;
}

void HibernateMachine(void)
{
    SetSuspendState(TRUE, TRUE, TRUE);
}

DWORD WINAPI PowerMonitorThread(LPVOID lpParams)
{
    SYSTEM_POWER_STATUS PowerStatus;
    PMONITOR_THREAD_PARAMS pMonitorThreadParams = (PMONITOR_THREAD_PARAMS)lpParams;
    
    WriteLineToLog("PowerMonitorThread: Started");

    while(1) {
        GetSystemPowerStatus(&PowerStatus);
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
    PMONITOR_THREAD_PARAMS pMonitorThreadParams = (PMONITOR_THREAD_PARAMS)lpParams;
    PIP_ADAPTER_ADDRESSES pNewAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pOldAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrNewAddress = NULL;
    PIP_ADAPTER_ADDRESSES pCurrOldAddress = NULL;
    ULONG outBufLen = sizeof(IP_ADAPTER_ADDRESSES);

    WriteLineToLog("WiredEtherMonitorThread: Started");

    pOldAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
    if(!pOldAddresses) {
        goto cleanexit;
    }

    if(GetAdaptersAddresses(AF_UNSPEC,
                            GAA_FLAG_INCLUDE_PREFIX/*|GAA_FLAG_INCLUDE_ALL_INTERFACES*/,
                            NULL,
                            pOldAddresses,
                            &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        HB_SAFE_FREE(pOldAddresses);
        outBufLen *= 2; //int overflow...
        pOldAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if(!pOldAddresses) {
            goto cleanexit;
        }

        pNewAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if(!pNewAddresses) {
            goto cleanexit;
        }
    }

    if(GetAdaptersAddresses(AF_UNSPEC,
                            GAA_FLAG_INCLUDE_PREFIX/*|GAA_FLAG_INCLUDE_ALL_INTERFACES*/,
                            NULL,
                            pOldAddresses,
                            &outBufLen) != NO_ERROR)
        goto cleanexit;

    while(1) {
        if(GetAdaptersAddresses(AF_UNSPEC,
                                GAA_FLAG_INCLUDE_PREFIX/*|GAA_FLAG_INCLUDE_ALL_INTERFACES*/,
                                NULL,
                                pNewAddresses,
                                &outBufLen) != NO_ERROR)
            goto cleanexit;

        pCurrNewAddress = pNewAddresses;
        pCurrOldAddress = pOldAddresses;
        while(pCurrNewAddress && pCurrOldAddress) {
            if(pCurrNewAddress->IfType == IF_TYPE_ETHERNET_CSMACD) {
                if((pCurrNewAddress->OperStatus == IfOperStatusDown) && (pCurrOldAddress->OperStatus == IfOperStatusUp)) {
                    WriteLineToLog("WiredEtherMonitorThread: Firing monitor event");
                    SetEvent(pMonitorThreadParams->hMonitorEvent);
                    break;
                }
                else {
                    ResetEvent(pMonitorThreadParams->hMonitorEvent);
                }
            }
            pCurrNewAddress = pCurrNewAddress->Next;
            pCurrOldAddress = pCurrOldAddress->Next;
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

    HB_SAFE_FREE(pOldAddresses);
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

void usage(void)
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