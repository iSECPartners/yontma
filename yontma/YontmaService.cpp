#include "stdafx.h"

void HibernateMachine(void);
void ResetMonitoringState(__in size_t cMonitors, __in PHANDLE pMonitoringHandles, __in LONG volatile * pMonitorsCompleted);

#ifdef _DEBUG
const TCHAR LOGGING_MUTEX_NAME[] = TEXT("YontmaLoggingMutex");

//
// To faciliitate debugging, uncomment the following line to make yontma skip actual
// hibernation.
//

//#define DO_NOT_REALLY_HIBERNATE

#ifdef DO_NOT_REALLY_HIBERNATE
#define SetSuspendState(__x, __y, __z)
#endif

#endif

void __stdcall ServiceMain(int argc, TCHAR* argv[])
{
    HRESULT hr;
    static const int DisconnectACEventIndex = 0;
    static const int DisconnectWiredEthernetEventIndex = 1;
    static const int ServiceEndEventIndex = 2;
    static const int TotalEvents = ServiceEndEventIndex + 1;
    static const int MonitorCount = 2;
    HANDLE HandleArray[TotalEvents];
    SERVICE_HANDLER_PARAMS serviceHandlerParams = {0};
    BOOL bExitService = FALSE;
    BOOL bNeverLoggedIn = FALSE;

    WriteLineToLog("In ServiceMain");

    //ServiceMain will only be called when the service is started. When it is started by StartService, we set an argument which can be checked for. If
    //this argument is not present, we know we are coming out of a OS start and the machine is locked.
    if((argc == 2) && (!_tcscmp(argv[1],CMD_PARAM_STARTED_FROM_SS))) {
        bNeverLoggedIn = FALSE;
        WriteLineToLog("bNeverLoggedIn = FALSE");
    }
    else {
        bNeverLoggedIn = TRUE;
        WriteLineToLog("bNeverLoggedIn = TRUE");
    }

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

    //make sure to tell the Callback that the screen is locked if we came out of a OS boot
    if(bNeverLoggedIn) ServiceHandlerEx(SERVICE_CONTROL_SESSIONCHANGE,
                                        WTS_SESSION_LOCK,
                                        NULL,
                                        &serviceHandlerParams);

    while(!bExitService) {
        switch(WaitForMultipleObjects(TotalEvents,
                                      HandleArray,
                                      FALSE,
                                      DEFAULT_SLEEP_TIME)) {
            case WAIT_OBJECT_0 + DisconnectACEventIndex: //AC was disconnected
                WriteLineToLog("ServiceMain: AC disconnected -> suspend");
                HibernateMachine();
                ResetMonitoringState(MonitorCount, HandleArray, &serviceHandlerParams.MonitorsCompleted);
                break;
            case WAIT_OBJECT_0 + DisconnectWiredEthernetEventIndex: //wired ether was disconnected
                WriteLineToLog("ServiceMain: Wired ether disconnected -> suspend");
                HibernateMachine();
                ResetMonitoringState(MonitorCount, HandleArray, &serviceHandlerParams.MonitorsCompleted);
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
        switch (dwEventType) {
        case WTS_SESSION_LOCK:
        case WTS_SESSION_LOGOFF:

            //
            // Ignore locks/logoffs if machine is in suspend/hibernate state.
            //

            if(pServiceHandlerParams->bMachineSuspended) {
                break;
            }

            ResetEvent(pServiceHandlerParams->hMonitorStopEvent);

            pACThreadParams = (PMONITOR_THREAD_PARAMS)malloc(sizeof(MONITOR_THREAD_PARAMS));
            if(!pACThreadParams) {
                return NO_ERROR;
            }
            pACThreadParams->hMonitorStopEvent = pServiceHandlerParams->hMonitorStopEvent;
            pACThreadParams->hMonitorEvent = pServiceHandlerParams->hACDisconnectedEvent;
            pACThreadParams->pMonitorsCompleted = &pServiceHandlerParams->MonitorsCompleted;
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
            pEthernetThreadParams->pMonitorsCompleted = &pServiceHandlerParams->MonitorsCompleted;
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
        if(dwEventType == PBT_APMSUSPEND) {
            WriteLineToLog("ServiceHandlerEx: Going into suspended state");
            pServiceHandlerParams->bMachineSuspended = TRUE;
        }
        else if(dwEventType == PBT_APMRESUMESUSPEND) {
            WriteLineToLog("ServiceHandlerEx: Resuming from suspended state");
            pServiceHandlerParams->bMachineSuspended = FALSE;
        }
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

void HibernateMachine(void)
{
    SetSuspendState(TRUE, TRUE, TRUE);
}

void ResetMonitoringState(__in size_t cMonitors, __in PHANDLE pMonitoringHandles, __in LONG volatile * pMonitorsCompleted)
{
    WriteLineToLog("In ResetMonitoringState");

    while(cMonitors < *pMonitorsCompleted) {
        Sleep(250);
    }

    WriteLineToLog("ResetMonitoringState: Resetting handles and monitor complete count");

    for(size_t handleIndex = 0; handleIndex < cMonitors; handleIndex++) {
        ResetEvent(pMonitoringHandles[handleIndex]);
    }

    *pMonitorsCompleted = 0;

    WriteLineToLog("ResetMonitoringState: Exiting");
}

#ifdef _DEBUG
void InitLogging(void)
{
    CreateMutex(NULL, FALSE, LOGGING_MUTEX_NAME);
}

//
// Description:
//  Writes a message to the debug log. Path is:
//   Under LocalService: C:\Windows\Temp\yontmalog.txt
//   Under yontmauser:   C:\Users\yontmauser\AppData\Local\Temp\yontmalog.txt
//
// Parameters:
//  pStr - Debug message to write to the log.
//
void WriteLineToLog(char *pStr)
{
    HRESULT hr;
    HANDLE fh;
    DWORD dwBytes;
    HANDLE hLoggingMutex;
    WCHAR szTempPath[MAX_PATH];
    WCHAR szLogFilePath[MAX_PATH];
    SYSTEMTIME systemTime = {0};
    CHAR szTimestamp[] = "[12/12/2012 12:12:12.123] ";
    static const char CRLF[] = "\r\n";

    hLoggingMutex = OpenMutex(SYNCHRONIZE, FALSE, LOGGING_MUTEX_NAME);
    if(!hLoggingMutex) {
        return;
    }

    WaitForSingleObject(hLoggingMutex, INFINITE);

    if(!GetTempPath(ARRAYSIZE(szTempPath), szTempPath)) {
        return;
    }

    hr = StringCchPrintf(szLogFilePath,
                         ARRAYSIZE(szLogFilePath),
                         TEXT("%s%s"),
                         szTempPath,
                         TEXT("yontmalog.txt"));
    if(HB_FAILED(hr)) {
        return;
    }

    fh = CreateFile(szLogFilePath,
                    FILE_APPEND_DATA,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
    if(fh != INVALID_HANDLE_VALUE) {
        GetSystemTime(&systemTime);

        hr = StringCchPrintfA(szTimestamp,
                              ARRAYSIZE(szTimestamp),
                              "[%d/%d/%d %02d:%02d:%02d.%03d] ",
                              systemTime.wMonth,
                              systemTime.wDay,
                              systemTime.wYear,
                              systemTime.wHour,
                              systemTime.wMinute,
                              systemTime.wSecond,
                              systemTime.wMilliseconds);
        if(HB_FAILED(hr)) {
            return;
        }
        WriteFile(fh,
                  szTimestamp,
                  strlen(szTimestamp),
                  &dwBytes,
                  NULL);
        WriteFile(fh,
                  pStr,
                  strlen(pStr),
                  &dwBytes,
                  NULL);
        WriteFile(fh,
                  CRLF,
                  strlen(CRLF),
                  &dwBytes,
                  NULL);
        CloseHandle(fh);
    }

    ReleaseMutex(hLoggingMutex);
}
#endif
