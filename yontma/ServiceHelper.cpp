#include "stdafx.h"

HRESULT ChangeYontmaServiceStatus(DWORD dwServiceStatus);

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

HRESULT OpenYontmaService(__in SC_HANDLE hSCManager,
                          __out SC_HANDLE* phService)
{
    HRESULT hr;
    SC_HANDLE hServiceLocal = NULL;

    hServiceLocal = OpenService(hSCManager,
                                SERVICE_NAME,
                                SERVICE_ALL_ACCESS);
    if(hServiceLocal == NULL) {
        printf("OpenService error: %d\r\n", GetLastError());
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }
    
    *phService = hServiceLocal;
    hServiceLocal = NULL;

    hr = S_OK;

cleanexit:
    HB_SAFE_CLOSE_SERVICE_HANDLE(hServiceLocal);

    return hr;
}

HRESULT CreateYontmaService(__in PCTSTR pServicePath,
                            __out SC_HANDLE* phService)
{
    HRESULT hr;
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hServiceLocal = NULL;

    hr = OpenServiceManager(&hSCManager);
    if (HB_FAILED(hr)) {
        goto cleanexit;
    }

    hServiceLocal = CreateService(hSCManager,
                                  SERVICE_NAME,
                                  SERVICE_NAME,
                                  SERVICE_ALL_ACCESS,
                                  SERVICE_WIN32_OWN_PROCESS,
                                  SERVICE_AUTO_START,
                                  SERVICE_ERROR_IGNORE,
                                  pServicePath,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL);
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
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    SERVICE_STATUS ServiceStatus;

    hr = OpenServiceManager(&hSCManager);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = OpenYontmaService(hSCManager, &hService);
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
    HB_SAFE_CLOSE_SERVICE_HANDLE(hSCManager);
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
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE | SERVICE_CONTROL_POWEREVENT;
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