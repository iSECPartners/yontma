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

//Creates a user for the yontma service
//returns TRUE if user is created, FALSE otherwise
BOOL CreateYontmaUser(WCHAR *wcPassword,DWORD dwPwdSize)
{
	USER_INFO_0 *pUserInfo0;
	USER_INFO_1 UserInfo1 = {0};
	DWORD dwResult,dwReturn,dwEntries,dwTotalEntries,i;
	GROUP_USERS_INFO_0 *pGroupInfo0;
	LOCALGROUP_MEMBERS_INFO_3 LocalGroupMembersInfo3 = {0};

	//Check if the user already exist. Delete if it does
	dwResult = NetUserGetInfo(NULL,USERNAME,0,(LPBYTE*)&pUserInfo0);
	NetApiBufferFree(pUserInfo0);
	if(dwResult == NERR_Success)
	{
		if(NetUserDel(NULL,USERNAME) != NERR_Success) return FALSE;
	}

	//generate a new, random passwords
	if(!GenerateRandomPassword(wcPassword,dwPwdSize)) return FALSE;

	//create a new user
	UserInfo1.usri1_name = USERNAME;
	UserInfo1.usri1_password = wcPassword;
	UserInfo1.usri1_priv = USER_PRIV_USER;
	dwResult = NetUserAdd(NULL,1,(LPBYTE)&UserInfo1,NULL);
	if(dwResult != NERR_Success) {
		return FALSE;
	}

	//we need this to succeed to be able to use our new user
	if(!AdjustYontmaAccountPrivileges()) return FALSE;
	else return TRUE;

	//remove user fom all groups
	//we return true even if this fails since this user will still be more low-priv than SYSTEM
	if(NetUserGetGroups(NULL,USERNAME,0,(LPBYTE*)&pGroupInfo0,MAX_PREFERRED_LENGTH,&dwEntries,&dwTotalEntries) == NERR_Success) {
		for(i = 0; i < dwEntries; i++) {
			LocalGroupMembersInfo3.lgrmi3_domainandname = USERNAME;
			NetLocalGroupDelMembers(NULL,pGroupInfo0[i].grui0_name,3,(LPBYTE)&LocalGroupMembersInfo3,1);
		}
		NetApiBufferFree(pGroupInfo0);
	}
		
	return TRUE;
}

BOOL AdjustYontmaAccountPrivileges(void)
{
	PBYTE SidBuffer[128];
	PSID pSid = (PSID)SidBuffer;
	DWORD dwSid = sizeof(SidBuffer);
	WCHAR wcRefDomain[128];
	DWORD dwRefDomain = sizeof(wcRefDomain) / sizeof(WCHAR);
	SID_NAME_USE SidNameUse;
	LSA_OBJECT_ATTRIBUTES ObjectAttributes = {0};
	LSA_HANDLE lsahPolicyHandle;
	LSA_UNICODE_STRING lucStr;
	
	if(!LookupAccountName(NULL,USERNAME,&SidBuffer,&dwSid,wcRefDomain,&dwRefDomain,&SidNameUse)) {
		return FALSE;
	}

	if(LsaOpenPolicy(NULL,&ObjectAttributes,POLICY_ALL_ACCESS,&lsahPolicyHandle) != STATUS_SUCCESS) return FALSE;

	InitLsaString(&lucStr,SE_SERVICE_LOGON_NAME);
	if(LsaAddAccountRights(lsahPolicyHandle,pSid,&lucStr,1) != STATUS_SUCCESS) {
		LsaClose(lsahPolicyHandle);
		return FALSE;
	}

	//when we remove privs, we don't care if we fail
	InitLsaString(&lucStr,SE_BATCH_LOGON_NAME);
	LsaRemoveAccountRights(lsahPolicyHandle,pSid,FALSE,&lucStr,1);
	InitLsaString(&lucStr,SE_INTERACTIVE_LOGON_NAME);
	LsaRemoveAccountRights(lsahPolicyHandle,pSid,FALSE,&lucStr,1);
	InitLsaString(&lucStr,SE_NETWORK_LOGON_NAME);
	LsaRemoveAccountRights(lsahPolicyHandle,pSid,FALSE,&lucStr,1);
	InitLsaString(&lucStr,SE_REMOTE_INTERACTIVE_LOGON_NAME);
	LsaRemoveAccountRights(lsahPolicyHandle,pSid,FALSE,&lucStr,1);

	LsaClose(lsahPolicyHandle);
	return TRUE;
}

bool InitLsaString(PLSA_UNICODE_STRING pLsaString,LPCWSTR pwszString)
{
	DWORD dwLen = 0;

	if (NULL == pLsaString) return FALSE;

	if (NULL != pwszString) {
		dwLen = wcslen(pwszString);
		if (dwLen > 0x7ffe) return FALSE;
	}

	// Store the string.
	pLsaString->Buffer = (WCHAR *)pwszString;
	pLsaString->Length =  (USHORT)dwLen * sizeof(WCHAR);
	pLsaString->MaximumLength= (USHORT)(dwLen+1) * sizeof(WCHAR);

	return TRUE;
}

//dwSize is the size of pBuffer in WCHAR
BOOL GenerateRandomPassword(WCHAR *pBuffer,DWORD dwSize)
{
	HCRYPTPROV hCryptProvider;
	BYTE bRandomBuffer[256];
	DWORD i;

	if(dwSize > sizeof(bRandomBuffer) * sizeof(WCHAR)) dwSize = sizeof(bRandomBuffer) * sizeof(WCHAR);
	
	if(!CryptAcquireContext(&hCryptProvider,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT)) {
		printf("CryptAcquireContext error: 0x%08x\n",GetLastError());
		return FALSE;
	}

	if(!CryptGenRandom(hCryptProvider,sizeof(bRandomBuffer),bRandomBuffer)) {
		CryptReleaseContext(hCryptProvider,0);
		return FALSE;
	}

	//Make sure all byte are within printable range. We do lose some entropy, but we still have enough
	//Also convert into WCHAR
	for(i = 0; i < dwSize - 1; i++) pBuffer[i] = (bRandomBuffer[i] % 0x4d) + 0x21;
	pBuffer[i] = '\0';

	CryptReleaseContext(hCryptProvider,0);
	return TRUE;
}

BOOL DeleteYontmaUser(void)
{
	if(NetUserDel(NULL,USERNAME) != NERR_Success) return FALSE;
	else return TRUE;
}