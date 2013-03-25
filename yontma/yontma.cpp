#include "stdafx.h"

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

void PrintUsage(void)
{
    static const TCHAR usage[] =
TEXT("YoNTMA v1.1 (Build date: ") TEXT(__DATE__) TEXT(")\r\n")
TEXT("\r\n")
TEXT("YoNTMA (You'll Never Take Me Alive!) is a service that helps protect a\r\n")
TEXT("laptop.\r\n")
TEXT("\r\n")
TEXT("If BitLocker is enabled, it will hibernate a locked laptop if power or wired\r\n")
TEXT("ethernet is disconnected.\r\n")
TEXT("\r\n")
TEXT("(c)2013 andreas at isecpartners.com and mlynch at isecpartners.com\r\n")
TEXT("--------------------------------------------------------------------------------\r\n")
TEXT("Usage:\r\n")
TEXT("  yontma <option>\r\n")
TEXT("Options:\r\n")
TEXT("  ") CMD_PARAM_INSTALL      TEXT("             Installs and starts yontma\r\n")
TEXT("  ") CMD_PARAM_UNINSTALL    TEXT("             Stops and removes yontma\r\n");
    _tprintf(usage);
}