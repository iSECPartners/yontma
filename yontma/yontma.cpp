#include "stdafx.h"

HRESULT VerifyRunningAsAdministrator(void);
HRESULT VerifyDriveProtectedByBitLocker(void);
void PrintError(HRESULT hrError);

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
    BOOL bForceInstall;
    
    if((argc < 2) || (argc > 3)) {
        hr = E_YONTMA_INVALID_COMMAND_LINE;
        goto cleanexit;
    }

    if(_tcscmp(argv[1], CMD_PARAM_INSTALL) == 0) {
        if(argc == 3) {
            if ((_tcscmp(argv[2], CMD_PARAM_FORCE_INSTALL_SHORT) == 0) ||
                (_tcscmp(argv[2], CMD_PARAM_FORCE_INSTALL_LONG) == 0)) {
                bForceInstall = TRUE;
            }
            else {
                hr = E_YONTMA_INVALID_COMMAND_LINE;
                goto cleanexit;
            }
        }
        else {
            bForceInstall = FALSE;
        }

        hr = PerformInstall(bForceInstall);
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
        hr = E_YONTMA_INVALID_COMMAND_LINE;
        goto cleanexit;
    }

cleanexit:
    if(hr == E_YONTMA_INVALID_COMMAND_LINE) {
        PrintUsage();
    }
    else if(HB_FAILED(hr)) {
        PrintError(hr);
    }

    return hr;
}

HRESULT PerformInstall(__in BOOL bSkipEncryptionCheck)
{
    HRESULT hr;

    hr = VerifyRunningAsAdministrator();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = CheckYontmaRequirements(bSkipEncryptionCheck);
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
    HRESULT hr;

    hr = VerifyRunningAsAdministrator();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = RemoveYontma();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

cleanexit:

    return hr;
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

HRESULT CheckYontmaRequirements(__in BOOL bSkipEncryptionCheck)
{
    HRESULT hr;
    SYSTEM_POWER_CAPABILITIES SystemPwrCap;

    //
    // Check if machine can hibernate.
    //

    if(!GetPwrCapabilities(&SystemPwrCap)) {
        printf("Unable to get hibernation information, exiting\r\n");
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }
    if(!SystemPwrCap.HiberFilePresent) {
        hr = E_YONTMA_HIBERNATE_NOT_ENABLED;
        goto cleanexit;
    }

    if(!bSkipEncryptionCheck) {
        hr = VerifyDriveProtectedByBitLocker();
        if(HB_FAILED(hr)) {
            goto cleanexit;
        }
    }

    hr = S_OK;

cleanexit:

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
    if(hr == E_YONTMA_SERVICE_NOT_INSTALLED) {
        PrintError(hr);
    }
    else if(HB_FAILED(hr)) {
        _tprintf(TEXT("Failed to remove YoNTMA files. Could not determine install location. Error 0x%x\r\n"), hr);
        szInstalledPath[0] = '\0';
    }

    if(hr != E_YONTMA_SERVICE_NOT_INSTALLED) {

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
    }

    hr = RemoveServiceUserAccount();
    if(HB_FAILED(hr)) {
        _tprintf(TEXT("Failed to remove YoNTMA service limited user account. Error 0x%x\r\n"), hr);
        hr = S_OK;
    }

cleanexit:

    return hr;
}

HRESULT VerifyDriveProtectedByBitLocker(void)
{
    HRESULT hr;
    BOOL bLoadedWmi = FALSE;
    BOOL bIsOsVolumeProtectedByBitLocker;
    
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
        hr = E_YONTMA_OS_DRIVE_NOT_ENCRYPTED;
        goto cleanexit;
    }

cleanexit:
    if(bLoadedWmi) {
        CleanupWmi();
    }

    return hr;
}

HRESULT VerifyRunningAsAdministrator(void)
{
    HRESULT hr;
    BOOL bIsAdmin;

    hr = IsUserAdmin(&bIsAdmin);
    if(HB_FAILED(hr)) {
        printf("Error occurred when checking administrator status: 0x%x\n", hr);
        goto cleanexit;
    }

    if(!bIsAdmin) {
        printf("Please run yontma as an administrator.\n");
        hr = E_FAIL;
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:

    return hr;
}

HRESULT IsUserAdmin(__out PBOOL pbIsAdmin)
{
    HRESULT hr;
    BOOL bIsAdminLocal;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup = NULL;

    if(!AllocateAndInitializeSid(&NtAuthority,
                                  2,
                                  SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  &AdministratorsGroup)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    if(!CheckTokenMembership(NULL,AdministratorsGroup,&bIsAdminLocal)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }
    
    *pbIsAdmin = bIsAdminLocal;
    hr = S_OK;

cleanexit:
    FreeSid(AdministratorsGroup);

    return hr;
}

HRESULT GetCurrentModule(__out HMODULE* hCurrentModule)
{
    HRESULT hr;
    HMODULE hModule = NULL;

    if(!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                          (LPCTSTR)GetCurrentModule,
                          &hModule)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    *hCurrentModule = hModule;
    hr = S_OK;

cleanexit:

    return hr;
}

void PrintError(HRESULT hrError)
{
    HRESULT hr;
    HMODULE hCurrentModule;
    PTSTR pszErrorMessage = NULL;

    hr = GetCurrentModule(&hCurrentModule);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    if(!FormatMessage(FORMAT_MESSAGE_FROM_HMODULE |
                        FORMAT_MESSAGE_ALLOCATE_BUFFER,
                      hCurrentModule,
                      (DWORD)hrError,
                      0,
                      (LPTSTR)&pszErrorMessage,
                      0,
                      NULL)) {
        goto cleanexit;
    }

    _tprintf(pszErrorMessage);

cleanexit:
    HB_SAFE_LOCAL_FREE(pszErrorMessage);
}

void PrintUsage(void)
{
    static const TCHAR usage[] =
TEXT("YoNTMA v1.1 (Build date: ") TEXT(__DATE__) TEXT(")\r\n")
TEXT("\r\n")
TEXT("YoNTMA (You'll Never Take Me Alive!) is a service that helps protect a\r\n")
TEXT("laptop.\r\n")
TEXT("\r\n")
TEXT("If BitLocker is enabled, it will hibernate a locked laptop if power or wired\r\n")
TEXT("Ethernet is disconnected.\r\n")
TEXT("\r\n")
TEXT("(c)2013 andreas at isecpartners.com and mlynch at isecpartners.com\r\n")
TEXT("--------------------------------------------------------------------------------\r\n")
TEXT("Usage:\r\n")
TEXT("  yontma <option> [arguments]\r\n")
TEXT("Options:\r\n")
TEXT("  ") CMD_PARAM_INSTALL      TEXT("             Installs and starts YoNTMA\r\n")
TEXT("  ") CMD_PARAM_UNINSTALL    TEXT("             Stops and removes YoNTMA\r\n")
TEXT("Arguments:\r\n")
TEXT("  ") CMD_PARAM_FORCE_INSTALL_SHORT TEXT("/") CMD_PARAM_FORCE_INSTALL_LONG
                                          TEXT("     Force installation (skip encryption check)\r\n")
                              TEXT("                 Use this argument only if your OS drive is encrypted with a\r\n")
                              TEXT("                 technology that YoNTMA does not recognize.\r\n");

    _tprintf(usage);
}