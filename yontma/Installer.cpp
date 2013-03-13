#include "stdafx.h"

HRESULT GetPathParentDirectory(__in PTSTR pszPath, __out PTSTR pszParentDirectory, __in size_t cchParentDirectory);
HRESULT GetPathFilename(__in PTSTR pszPath, __out PTSTR* ppszFilename);

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
    TCHAR szModuleFilename[MAX_PATH];
    PTSTR pszYontmaInstallFilename;
    TCHAR szInstallDirectory[MAX_PATH];

    if(!GetModuleFileName(NULL, szModuleFilename, ARRAYSIZE(szModuleFilename))) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    hr = GetPathFilename(szModuleFilename, &pszYontmaInstallFilename);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = GetInstallDirectory(szInstallDirectory, ARRAYSIZE(szInstallDirectory));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }
    
    hr = StringCchPrintf(pszInstallPath,
                         cchInstallPath,
                         TEXT("%s\\%s"),
                         szInstallDirectory,
                         pszYontmaInstallFilename);
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

//
// Description:
//  Removes the YoNTMA executable from its installed location and deletes the
//  YoNTMA directory if it is empty.
//
//  N.B.: This function cannot remove the YoNTMA binaries from the installed
//      location if YoNTMA is being run from the installed location, as a
//      running program cannot delete its own image. This function assumes
//      that there is a separate copy of YoNTMA outside the installed location
//      that will perform the deletion of installed files.
//
// Parameters:
//  pszInstalledPath - Full path to where the YoNTMA binary was installed.
//
void RemoveYontmaBinaryFromInstallLocation(__in PTSTR pszInstalledPath)
{
    HRESULT hr;
    TCHAR szInstalledDirectory[MAX_PATH] = {0};

    if(!DeleteFile(pszInstalledPath)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        _tprintf(TEXT("Failed to delete file: %s\r\n"), pszInstalledPath);
        _tprintf(TEXT("Error: 0x%x\r\n"), hr);
        goto cleanexit;
    }

    hr = GetPathParentDirectory(pszInstalledPath,
                                szInstalledDirectory,
                                ARRAYSIZE(szInstalledDirectory));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    //
    // If the binary was the only file in the directory, remove the directory.
    // This call will fail if the directory is non-empty.
    //

    RemoveDirectory(szInstalledDirectory);

cleanexit:
    ;
}

HRESULT GetPathParentDirectory(__in PTSTR pszPath, __out PTSTR pszParentDirectory, __in size_t cchParentDirectory)
{
    HRESULT hr;
    size_t cchPath;
    PTSTR pszDirectoryEnd;
    size_t cchToCopy;

    cchPath = _tcsclen(pszPath);
    if(cchPath < 2) {
        hr = E_INVALIDARG;
        goto cleanexit;
    }

    //
    // Walk backwards in the string until we encounter a path separator
    // character. We start from cchPath - 2 to skip the NULL terminaor and to
    // ignore a possible trailing slash.
    //

    pszDirectoryEnd = &pszPath[cchPath - 2];
    while(pszDirectoryEnd > pszPath)  {
        if(*pszDirectoryEnd == '\\') {
            break;
        }
        pszDirectoryEnd--;
    }

    cchToCopy = pszDirectoryEnd - pszPath + 1;
    hr = StringCchCopyN(pszParentDirectory, cchParentDirectory, pszPath, cchToCopy);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

cleanexit:
    return hr;
}

//
// Description:
//  Retrieves the location in a filesystem path of the filename portion of the
//  path (i.e. the non-directory portion).
//
// Parameters:
//  pszPath - Specifies a filesystem path
//
//  ppszFilename - On success, is set to the location in pszPath where the
//      filename (non-directory) portion of the path begins.
//
HRESULT GetPathFilename(__in PTSTR pszPath, __out PTSTR* ppszFilename)
{
    HRESULT hr;
    PTSTR pszLastSlash;

    pszLastSlash = _tcsrchr(pszPath, '\\');
    if(!pszLastSlash) {
        hr = E_INVALIDARG;
        goto cleanexit;
    }

    if(*(pszLastSlash + 1) == '\0') {
        hr = E_INVALIDARG;
        goto cleanexit;
    }

    *ppszFilename = (pszLastSlash + 1);

    hr = S_OK;

cleanexit:
    return hr;
}
