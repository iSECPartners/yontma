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
#define CMD_PARAM_STARTED_FROM_SS        TEXT("started_from_ss")

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

    if(!IsUserAdmin()) {
        printf("Please run yontma as an administrator\n");
        return E_FAIL;
    }

#ifdef NDEBUG
    hr = CheckYontmaRequirements();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }
#endif

    hr = InstallYontma();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

cleanexit:

    return hr;
}

HRESULT PerformUninstall(void)
{
    if(!IsUserAdmin()) {
        printf("Please run yontma as an administrator\n");
        return E_FAIL;
    }

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

void __stdcall ServiceMain(int argc, TCHAR* argv[])
{
    HRESULT hr;
    static const int DisconnectACEventIndex = 0;
    static const int DisconnectWiredEthernetEventIndex = 1;
    static const int ServiceEndEventIndex = 2;
    static const int TotalEvents = ServiceEndEventIndex + 1;
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
                ResetEvent(HandleArray[DisconnectACEventIndex]);
                HibernateMachine();
                break;
            case WAIT_OBJECT_0 + DisconnectWiredEthernetEventIndex: //wired ether was disconnected
                WriteLineToLog("ServiceMain: Wired ether disconnected -> suspend");
                ResetEvent(HandleArray[DisconnectWiredEthernetEventIndex]);
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
    PWSTR pszAccountPassword = NULL;
    size_t cbAccountPassword;
    TCHAR szServiceLaunchCommand[ARRAYSIZE(szYontmaInstalledPath) + ARRAYSIZE(CMD_PARAM_RUN_AS_SERVICE)];
    LPCTSTR cstrSSArgument[32] = {CMD_PARAM_STARTED_FROM_SS};

    hr = CopyYontmaBinaryToInstallLocation();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = GetInstallPath(szYontmaInstalledPath, ARRAYSIZE(szYontmaInstalledPath));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = CreateServiceUserAccount(&pszAccountPassword, &cbAccountPassword);
    
    //
    // Don't fail if we're unable to create the low-privileged YoNTMA user.
    //

    if(HB_FAILED(hr)) {
        hr = S_OK;
        HB_SECURE_FREE(pszAccountPassword, cbAccountPassword);
        printf("Failed to create yontma user account: %d\r\n", GetLastError());
        printf("Installing YoNTMA to run as LocalService.\r\n");
    }

    hr = StringCchPrintf(szServiceLaunchCommand,
                         ARRAYSIZE(szServiceLaunchCommand),
                         TEXT("\"%s\" %s"),
                         szYontmaInstalledPath,
                         CMD_PARAM_RUN_AS_SERVICE);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = CreateYontmaService(szServiceLaunchCommand, pszAccountPassword, &hService);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    iStatus = StartService(hService, 1, cstrSSArgument);
    if(iStatus == 0) {
        printf("StartService error: %d\r\n", GetLastError());
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

cleanexit:
    HB_SECURE_FREE(pszAccountPassword, cbAccountPassword);
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
    TCHAR szInstalledPath[MAX_PATH] = {0};
    
    hr = GetServiceInstalledPath(szInstalledPath,
                                 ARRAYSIZE(szInstalledPath));
    if(HB_FAILED(hr)) {
        _tprintf(TEXT("Failed to remove YoNTMA files. Could not determine install location. Error 0x%x\r\n"), hr);
        szInstalledPath[0] = '\0';
        hr = S_OK;
    }

    //
    // Failing to delete the service is a fatal error and we should abort here.
    //

    hr = DeleteYontmaService();
    if(HB_FAILED(hr)) {
        _tprintf(TEXT("Failed to remove YoNTMA service. Error 0x%x"), hr);
        goto cleanexit;
    }

    if(szInstalledPath[0] != '\0') {
        hr = RemoveYontmaBinaryFromInstallLocation(szInstalledPath);
        if (HB_FAILED(hr)) {
            _tprintf(TEXT("Failed to remove YoNTMA files from [%s]. Error 0x%x\r\n"), szInstalledPath, hr);
            hr = S_OK;
        }
    }

    hr = RemoveServiceUserAccount();
    if(HB_FAILED(hr)) {
        _tprintf(TEXT("Failed to remove YoNTMA service limited user account. Error 0x%x\r\n"), hr);
        hr = S_OK;
    }

cleanexit:

    return hr;
}

void HibernateMachine(void)
{
    SetSuspendState(TRUE, TRUE, TRUE);
}

BOOL IsUserAdmin(void)
{
    BOOL b;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;

    b = AllocateAndInitializeSid(&NtAuthority,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&AdministratorsGroup);
    if(b) {
        if(!CheckTokenMembership(NULL,AdministratorsGroup,&b)) b = FALSE;
        FreeSid(AdministratorsGroup); 
    }


    return b;
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
TEXT("(c)2013 andreas at isecpartners.com and mlynch at isecpartners.com\r\n")
TEXT("--------------------------------------------------------------------------------\r\n")
TEXT("Usage:\r\n")
TEXT("  yontma <option>\r\n")
TEXT("Options:\r\n")
TEXT("  ") CMD_PARAM_INSTALL      TEXT("             Installs and starts yontma\r\n")
TEXT("  ") CMD_PARAM_UNINSTALL    TEXT("             Stops and removes yontma\r\n");
    _tprintf(usage);
}