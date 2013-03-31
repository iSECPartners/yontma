#include "stdafx.h"

HRESULT OpenServiceManager(__out SC_HANDLE* hSCManager);
HRESULT ChangeYontmaServiceStatus(DWORD dwServiceStatus);
HRESULT GetServiceExecutionString(__out PTSTR* ppszServiceExecutionString);
HRESULT ServiceExecutionStringToInstalledPath(__in PTSTR pszServiceExecutionString, __out PTSTR pszInstalledPath, __in size_t cchInstalledPath);

HRESULT OpenServiceManager(__out SC_HANDLE* phSCManager)
{
    HRESULT hr;
    SC_HANDLE hSCManagerLocal = NULL;

    hSCManagerLocal = OpenSCManager(NULL,
                                    NULL,
                                    SC_MANAGER_ALL_ACCESS);
    if(hSCManagerLocal == NULL) {
        printf("OpenSCManager error: %d\r\n", GetLastError());
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    *phSCManager = hSCManagerLocal;
    hSCManagerLocal = NULL;

    hr = S_OK;

cleanexit:
    HB_SAFE_CLOSE_SERVICE_HANDLE(hSCManagerLocal);

    return hr;
}

HRESULT OpenYontmaService(__out SC_HANDLE* phService)
{
    HRESULT hr;
    SC_HANDLE hSCManager = NULL;

    hr = OpenServiceManager(&hSCManager);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = OpenYontmaService(hSCManager, phService);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:
    HB_SAFE_CLOSE_SERVICE_HANDLE(hSCManager);

    return hr;
}

HRESULT OpenYontmaService(__in SC_HANDLE hSCManager, __out SC_HANDLE* phService)
{
    HRESULT hr;
    SC_HANDLE hServiceLocal = NULL;
    DWORD dwError;

    hServiceLocal = OpenService(hSCManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
    if(hServiceLocal == NULL) {
        dwError = GetLastError();
        if(dwError == ERROR_SERVICE_DOES_NOT_EXIST) {
            printf("YoNTMA is not installed.\r\n");
        }
        else {
            printf("OpenService error: %d\r\n", dwError);
        }
        hr = HRESULT_FROM_WIN32(dwError);
        goto cleanexit;
    }
    
    *phService = hServiceLocal;
    hServiceLocal = NULL;

    hr = S_OK;

cleanexit:
    HB_SAFE_CLOSE_SERVICE_HANDLE(hServiceLocal);

    return hr;
}

HRESULT CreateYontmaService(__in PCTSTR pServicePath, __in_opt PCWSTR pszServiceAccountPassword, __out SC_HANDLE* phService)
{
    HRESULT hr;
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hServiceLocal = NULL;
    SERVICE_DESCRIPTION serviceDescription = { SERVICE_FRIENDLY_DESCRIPTION };
    PCTSTR pszServiceAccountName;
    WCHAR szServiceAccountName[] = YONTMA_SERVICE_ACCOUNT_NAME_WITH_DOMAIN;

    hr = OpenServiceManager(&hSCManager);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    //
    // If a password is provided, run the service under the yontma use
    // account. Otherwise, leave as NULL to run the service under LocalService.
    //

    if(pszServiceAccountPassword) {
        pszServiceAccountName = szServiceAccountName;
    }
    else {
        pszServiceAccountName = NULL;
    }

    hServiceLocal = CreateService(hSCManager,
                                  SERVICE_NAME,
                                  SERVICE_DISPLAY_NAME,
                                  SERVICE_ALL_ACCESS,
                                  SERVICE_WIN32_OWN_PROCESS,
                                  SERVICE_AUTO_START,
                                  SERVICE_ERROR_IGNORE,
                                  pServicePath,
                                  NULL,
                                  NULL,
                                  NULL,
                                  pszServiceAccountName,
                                  pszServiceAccountPassword);
    if(hServiceLocal == NULL) {
        if(GetLastError() != ERROR_SERVICE_EXISTS) {
            printf("CreateService error: %d\r\n", GetLastError());
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto cleanexit;
        }
        else {
            hr = OpenYontmaService(hSCManager, &hServiceLocal);
            if(HB_FAILED(hr)) {
                goto cleanexit;
            }
        }
    }

    ChangeServiceConfig2(hServiceLocal, SERVICE_CONFIG_DESCRIPTION, &serviceDescription);

    *phService = hServiceLocal;
    hServiceLocal = NULL;

    hr = S_OK;

cleanexit:
    HB_SAFE_CLOSE_SERVICE_HANDLE(hSCManager);
    HB_SAFE_CLOSE_SERVICE_HANDLE(hServiceLocal);

    return hr;
}

HRESULT DeleteYontmaService(void)
{
    HRESULT hr;
    SC_HANDLE hService = NULL;
    SERVICE_STATUS ServiceStatus;

    hr = OpenYontmaService(&hService);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    if(!ControlService(hService, SERVICE_CONTROL_STOP, &ServiceStatus)) {
        printf("ControlService error: %d\r\n", GetLastError());
        //goto cleanexit;
    }

    //give the service some time to die!
    Sleep(2 * DEFAULT_SLEEP_TIME);

    if(!DeleteService(hService)) {
        printf("DeleteService error: %d\r\n", GetLastError());
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:
    HB_SAFE_CLOSE_SERVICE_HANDLE(hService);

    return hr;
}

HRESULT ChangeYontmaServiceStatus(DWORD dwServiceStatus, LPVOID lpContext)
{
    SERVICE_STATUS status;
    SERVICE_STATUS_HANDLE hSrv;

    hSrv = RegisterServiceCtrlHandlerEx(SERVICE_NAME,
                                        (LPHANDLER_FUNCTION_EX)ServiceHandlerEx,
                                        lpContext);
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = dwServiceStatus;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE | SERVICE_ACCEPT_POWEREVENT;
    status.dwWin32ExitCode = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint = 0;
    status.dwWaitHint = 3000;
    
    if(!SetServiceStatus(hSrv, &status)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

HRESULT RunYontmaService(__in PSERVICE_HANDLER_PARAMS pServiceHandlerParams)
{
    return ChangeYontmaServiceStatus(SERVICE_RUNNING, pServiceHandlerParams);
}

void StopYontmaService(void)
{
    ChangeYontmaServiceStatus(SERVICE_STOPPED, NULL);
}

HRESULT GetServiceInstalledPath(__out PTSTR pszServiceInstalledPath, __in size_t cchServiceInstalledPath)
{
    HRESULT hr;
    PTSTR pszServiceExecutionString = NULL;

    hr = GetServiceExecutionString(&pszServiceExecutionString);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = ServiceExecutionStringToInstalledPath(pszServiceExecutionString,
                                               pszServiceInstalledPath,
                                               cchServiceInstalledPath);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

cleanexit:
    return hr;
}

//
// Description:
//  Retrieves that the service manage uses to launch the YoNTMA binary as a
//  service.
//
// Parameters:
//  ppszServiceExecutionString - On success, contains the command line string
//      used to launch YoNTMA (including arguments). Caller must free with
//      HB_SAFE_FREE.
//
HRESULT GetServiceExecutionString(__out PTSTR* ppszServiceExecutionString)
{
    HRESULT hr;
    DWORD rc;
    SC_HANDLE hService = NULL;
    LPQUERY_SERVICE_CONFIG pQueryServiceConfig = NULL;
    DWORD cbQueryServiceConfig;
    DWORD cbQueryServiceConfigRequired;
    PTSTR pszServiceExecutionStringLocal = NULL;
    size_t cchServiceExecutionStringLocal;
    size_t cbServiceExecutionStringLocal;

    hr = OpenYontmaService(&hService);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    QueryServiceConfig(hService,
                       NULL,
                       0,
                       &cbQueryServiceConfigRequired);

    rc = GetLastError();

    //
    // QueryServiceConfig succeeded with a zero-sized buffer? Something is wrong.
    //

    if(rc == ERROR_SUCCESS) {
        hr = E_UNEXPECTED;
        goto cleanexit;
    }
    else if(rc != ERROR_INSUFFICIENT_BUFFER) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    cbQueryServiceConfig = cbQueryServiceConfigRequired;
    pQueryServiceConfig = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LMEM_FIXED, cbQueryServiceConfig);
    if(!pQueryServiceConfig) {
        hr = E_OUTOFMEMORY;
        goto cleanexit;
    }

    if(!QueryServiceConfig(hService,
                           pQueryServiceConfig,
                           cbQueryServiceConfig,
                           &cbQueryServiceConfigRequired)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    cchServiceExecutionStringLocal = _tcslen(pQueryServiceConfig->lpBinaryPathName);
    hr = SizeTAdd(cchServiceExecutionStringLocal,
                  1,
                  &cchServiceExecutionStringLocal);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = SizeTMult(cchServiceExecutionStringLocal, sizeof(TCHAR), &cbServiceExecutionStringLocal);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    pszServiceExecutionStringLocal = (PTSTR)malloc(cbServiceExecutionStringLocal);
    if(!pszServiceExecutionStringLocal) {
        hr = E_OUTOFMEMORY;
        goto cleanexit;
    }

    hr = StringCchCopy(pszServiceExecutionStringLocal,
                       cchServiceExecutionStringLocal,
                       pQueryServiceConfig->lpBinaryPathName);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    *ppszServiceExecutionString = pszServiceExecutionStringLocal;
    pszServiceExecutionStringLocal = NULL;

cleanexit:
    HB_SAFE_CLOSE_SERVICE_HANDLE(hService);
    HB_SAFE_LOCAL_FREE(pQueryServiceConfig);
    HB_SAFE_FREE(pszServiceExecutionStringLocal);

    return hr;
}

//
// Description:
//  Given the service execution string, returns the portion of the string that
//  only contains the path to the YoNTMA executables (no arguments).
//
// Parameters:
//  pszServiceExecutionString - Contains the string used by the service to
//      execute the YoNTMA binary (includes binary path + command-line
//      arguments).
//
//  pszInstalledPath - On success, contains the path to where the YoNTMA binary
//      is installed on the system.
//
//  cchInstalledPath - The size of the pszInstalledPath buffer (in characters).
//
HRESULT ServiceExecutionStringToInstalledPath(__in PTSTR pszServiceExecutionString, __out PTSTR pszInstalledPath, __in size_t cchInstalledPath)
{
    HRESULT hr;
    PTSTR pszInstalledPathEnd;
    size_t cchToCopy;

    //
    // The path to the YoNTMA binary appears first in the service execution
    // string and is enclosed in quotes. Retrieve the string between the
    // two quotation marks.
    //

    if(pszServiceExecutionString[0] != '\"') {
        hr = E_INVALIDARG;
        goto cleanexit;
    }

    pszInstalledPathEnd = _tcschr(pszServiceExecutionString + 1, '\"');
    if(!pszInstalledPathEnd) {
        hr = E_INVALIDARG;
        goto cleanexit;
    }

    cchToCopy = (pszInstalledPathEnd - (pszServiceExecutionString + 1));
    
    hr = StringCchCopyN(pszInstalledPath,
                        cchInstalledPath,
                        pszServiceExecutionString + 1,
                        cchToCopy);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:

    return hr;
}