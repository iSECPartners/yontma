#include "stdafx.h"

HRESULT GetInstallDirectory(__out TCHAR* pszInstallDirectory, __in size_t cchInstallDirectory)
{
    HRESULT hr;
    const TCHAR szYontmaInstallDirectory[] = TEXT("%PROGRAMFILES%\\yontma");

    if(!ExpandEnvironmentStrings(szYontmaInstallDirectory,
                                 pszInstallDirectory,
                                 cchInstallDirectory)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:
    return hr;
}

HRESULT GetInstallPath(__out TCHAR* pszInstallPath, __in size_t cchInstallPath)
{
    HRESULT hr;
    const TCHAR szYontmaInstallFilename[] = TEXT("yontma.exe");
    TCHAR szInstallDirectory[MAX_PATH];

    hr = GetInstallDirectory(szInstallDirectory, ARRAYSIZE(szInstallDirectory));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }
    
    hr = StringCchPrintf(pszInstallPath,
                         cchInstallPath,
                         TEXT("%s\\%s"),
                         szInstallDirectory,
                         szYontmaInstallFilename);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:
    return hr;
}

HRESULT CopyYontmaBinaryToInstallLocation()
{
    HRESULT hr;
    DWORD rc;
    TCHAR szInstallDestinationDirectory[MAX_PATH];
    TCHAR szInstallDestinationPath[MAX_PATH];
    TCHAR szInstallerBinaryPath[MAX_PATH];

    hr = GetInstallDirectory(szInstallDestinationDirectory, ARRAYSIZE(szInstallDestinationDirectory));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    if(!CreateDirectory(szInstallDestinationDirectory, NULL)) {
        rc = GetLastError();
        if(rc != ERROR_ALREADY_EXISTS) {
            hr = HRESULT_FROM_WIN32(rc);
            goto cleanexit;
        }
    }

    if(!GetModuleFileName(NULL, szInstallerBinaryPath, ARRAYSIZE(szInstallerBinaryPath))) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    hr = GetInstallPath(szInstallDestinationPath,
                        ARRAYSIZE(szInstallDestinationPath));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    //
    // If the installing binary is already in the installed location, no need to copy.
    //

    if(_tcscmp(szInstallerBinaryPath, szInstallDestinationPath) == 0) {
        hr = S_OK;
        goto cleanexit;
    }

    if(!CopyFile(szInstallerBinaryPath, szInstallDestinationPath, FALSE)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:
    return hr;
}

void RemoveYontmaBinaryFromInstallLocation()
{
    HRESULT hr;
    TCHAR szInstallDestinationDirectory[MAX_PATH];
    TCHAR szInstallDestinationPath[MAX_PATH];

    hr = GetInstallDirectory(szInstallDestinationDirectory, ARRAYSIZE(szInstallDestinationDirectory));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = GetInstallPath(szInstallDestinationPath,
                        ARRAYSIZE(szInstallDestinationPath));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    DeleteFile(szInstallDestinationPath);
    RemoveDirectory(szInstallDestinationDirectory);

cleanexit:
    ;
}