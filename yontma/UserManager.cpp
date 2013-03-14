
#include "stdafx.h"

#define YONTMA_SERVICE_ACCOUNT_COMMENT L"Service account for YoNTMA (You'll Never Take Me Alive!) service."
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

//HRESULT CheckIfServiceUserExists(PBOOL pbUserExists);
HRESULT AdjustYontmaAccountPrivileges(void);
HRESULT RemoveServiceUserFromGroups(void);
bool InitLsaString(PLSA_UNICODE_STRING pLsaString,LPCWSTR pwszString);
HRESULT GenerateRandomPassword(__out PWSTR pszPassword, __in size_t cchPassword);
HRESULT PasswordFromBytes(__in PBYTE pBytes, __in size_t cbBytes, __out PWSTR pszPassword, __in size_t cbPassword);

//
// Description:
//  Creates a new, limited-privilege user account under which the YoNTMA service will run.
//
// Parameters:
//  ppszAccountPassword - On success, contains the randomly generated password of the new account.
//      Caller must free with HB_SECURE_FREE.
//
//  cbAccountPassword - On success, is set to the size (in bytes) of ppszAccountPassword.
//
HRESULT CreateServiceUserAccount(__out PWSTR* ppszAccountPassword, __out size_t* cbAccountPassword)
{
    HRESULT hr;
    BOOL bServiceUserExists;
    PWSTR pszAccountPasswordLocal = NULL;
    DWORD badParameterIndex,dwResult;
	USER_INFO_1 *pUserInfo1;
    USER_INFO_1 userInfo = {
      YONTMA_SERVICE_ACCOUNT_NAME,
      NULL,
      0,
      USER_PRIV_USER,
      NULL,
      YONTMA_SERVICE_ACCOUNT_COMMENT,
      UF_DONT_EXPIRE_PASSWD,
      NULL
    };
    //
    // This value is chosen arbitrarily as a long password. Could increase or decrease if there are
    // compatibility issues.
    //

    const size_t cchPasswordLength = 40;
    size_t cbAccountPasswordLocal;
	
    hr = SizeTMult(cchPasswordLength, sizeof(WCHAR), &cbAccountPasswordLocal);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    pszAccountPasswordLocal = (PWSTR)malloc(cbAccountPasswordLocal);
    if(!pszAccountPasswordLocal) {
        hr = E_OUTOFMEMORY;
        goto cleanexit;
    }

	hr = GenerateRandomPassword(pszAccountPasswordLocal,cchPasswordLength);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

	if(NetUserGetInfo(NULL,YONTMA_SERVICE_ACCOUNT_NAME,1,(LPBYTE*)&pUserInfo1) == NERR_Success) {
		pUserInfo1->usri1_flags = pUserInfo1->usri1_flags & (~UF_ACCOUNTDISABLE); //enable
		pUserInfo1->usri1_password = pszAccountPasswordLocal; //set new password
		dwResult = NetUserSetInfo(NULL,YONTMA_SERVICE_ACCOUNT_NAME,1,(LPBYTE)pUserInfo1,NULL);
		NetApiBufferFree(pUserInfo1);
		if(dwResult != NERR_Success) hr = E_FAIL;
		hr = S_OK;
	}
	else {
		userInfo.usri1_password = pszAccountPasswordLocal;
    
		if(NetUserAdd(NULL,
					  1,
					  (LPBYTE)&userInfo,
					  &badParameterIndex) != NERR_Success) {
			hr = E_FAIL;
			goto cleanexit;
		}
    
		hr = AdjustYontmaAccountPrivileges();
		if(HB_FAILED(hr)) {
			goto cleanexit;
		}
    
		//
		// We ignore failures on group removal, since this user will still be more
		// lower-privileged than SYSTEM
		//

		RemoveServiceUserFromGroups();

		*ppszAccountPassword = pszAccountPasswordLocal;
		pszAccountPasswordLocal = NULL;

		*cbAccountPassword = cbAccountPasswordLocal;

		hr = S_OK;
	}

cleanexit:
    HB_SECURE_FREE(pszAccountPasswordLocal, cbAccountPasswordLocal);

    return hr;
}

HRESULT DisableServiceUserAccount(void)
{
	DWORD dwResult; 
	USER_INFO_1 *pUserInfo1;
	HRESULT hr;

	if(NetUserGetInfo(NULL,YONTMA_SERVICE_ACCOUNT_NAME,1,(LPBYTE*)&pUserInfo1) != NERR_Success) {
        hr = E_FAIL;
        goto cleanexit;
    }
	
	if(NetUserSetInfo(NULL,YONTMA_SERVICE_ACCOUNT_NAME,1,(LPBYTE)pUserInfo1,NULL) != NERR_Success) hr = E_FAIL;
	NetApiBufferFree(pUserInfo1);

    hr = S_OK;

cleanexit:

    return hr;
}

/*HRESULT CheckIfServiceUserExists(PBOOL pbUserExists)
{
    HRESULT hr;
    NET_API_STATUS nerr;
    PUSER_INFO_0 pUserInfo = NULL;
    BOOL bUserExistsLocal;

    nerr = NetUserGetInfo(NULL,
                          YONTMA_SERVICE_ACCOUNT_NAME,
                          0,
                          (LPBYTE*)&pUserInfo);
    if(nerr == NERR_Success) {
        bUserExistsLocal = TRUE;
    }
    else if(nerr == NERR_UserNotFound) {
        bUserExistsLocal = FALSE;
    }
    else {
        hr = E_FAIL;
        goto cleanexit;
    }

    *pbUserExists = bUserExistsLocal;
    hr = S_OK;

cleanexit:
    HB_SAFE_NETAPI_FREE(pUserInfo);

    return hr;
}*/

HRESULT AdjustYontmaAccountPrivileges(void)
{
    HRESULT hr;
    NTSTATUS ntReturn;
    PBYTE SidBuffer[128];
    PSID pSid = (PSID)SidBuffer;
    DWORD dwSid = sizeof(SidBuffer);
    WCHAR wcRefDomain[128];
    DWORD dwRefDomain = sizeof(wcRefDomain) / sizeof(WCHAR);
    SID_NAME_USE SidNameUse;
    LSA_OBJECT_ATTRIBUTES ObjectAttributes = {0};
    LSA_HANDLE lsahPolicyHandle = NULL;
    LSA_UNICODE_STRING lucStr;
    
    if(!LookupAccountName(NULL,
                          YONTMA_SERVICE_ACCOUNT_NAME,
                          &SidBuffer,
                          &dwSid,
                          wcRefDomain,
                          &dwRefDomain,
                          &SidNameUse)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    ntReturn = LsaOpenPolicy(NULL,
                             &ObjectAttributes,
                             POLICY_ALL_ACCESS,
                             &lsahPolicyHandle);
    if(ntReturn != STATUS_SUCCESS) {
        hr = HRESULT_FROM_WIN32(LsaNtStatusToWinError(ntReturn));
        goto cleanexit;
    }

    if(!InitLsaString(&lucStr,SE_SERVICE_LOGON_NAME)) {
        hr = E_FAIL;
        goto cleanexit;
    }

    ntReturn = LsaAddAccountRights(lsahPolicyHandle,pSid,&lucStr,1);
    if(ntReturn != STATUS_SUCCESS) {
        hr = HRESULT_FROM_WIN32(LsaNtStatusToWinError(ntReturn));
        goto cleanexit;
    }

    //
    // When we remove privileges, we don't care if we fail.
    //

    InitLsaString(&lucStr,SE_BATCH_LOGON_NAME);
    LsaRemoveAccountRights(lsahPolicyHandle,pSid,FALSE,&lucStr,1);
    InitLsaString(&lucStr,SE_INTERACTIVE_LOGON_NAME);
    LsaRemoveAccountRights(lsahPolicyHandle,pSid,FALSE,&lucStr,1);
    InitLsaString(&lucStr,SE_NETWORK_LOGON_NAME);
    LsaRemoveAccountRights(lsahPolicyHandle,pSid,FALSE,&lucStr,1);
    InitLsaString(&lucStr,SE_REMOTE_INTERACTIVE_LOGON_NAME);
    LsaRemoveAccountRights(lsahPolicyHandle,pSid,FALSE,&lucStr,1);

    hr = S_OK;

cleanexit:
    LsaClose(lsahPolicyHandle);

    return hr;
}

HRESULT RemoveServiceUserFromGroups(void)
{
    HRESULT hr;
    PGROUP_USERS_INFO_0 pGroupInfo = NULL;
    LOCALGROUP_MEMBERS_INFO_3 LocalGroupMembersInfo = {0};
    DWORD dwEntries;
    DWORD dwTotalEntries;

    //
    // Removes user fom all groups.
    //

    if(NetUserGetGroups(NULL,
                        YONTMA_SERVICE_ACCOUNT_NAME,
                        0,
                        (LPBYTE*)&pGroupInfo,
                        MAX_PREFERRED_LENGTH,
                        &dwEntries,
                        &dwTotalEntries) != NERR_Success) {
        hr = E_FAIL;
        goto cleanexit;
    }

    for (DWORD i = 0; i < dwEntries; i++) {
        LocalGroupMembersInfo.lgrmi3_domainandname = YONTMA_SERVICE_ACCOUNT_NAME;
        NetLocalGroupDelMembers(NULL,
                                pGroupInfo[i].grui0_name,
                                3,
                                (LPBYTE)&LocalGroupMembersInfo,
                                1);
    }

    hr = S_OK;

cleanexit:
    HB_SAFE_NETAPI_FREE(pGroupInfo);

    return hr;
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

HRESULT GenerateRandomPassword(__out PWSTR pszPassword, __in size_t cchPassword)
{
    HRESULT hr;
    HCRYPTPROV hCryptProvider = NULL;
    size_t cbRandomBuffer = 0;
    PBYTE pbRandomBuffer = NULL;
    size_t cbPassword;
    
    if(!CryptAcquireContext(&hCryptProvider,
                            NULL,
                            NULL,
                            PROV_RSA_FULL,
                            CRYPT_VERIFYCONTEXT)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    //
    // The size of the random buffer will be the number of characters in the
    // password minus one, as the last character is NULL.
    //

    hr = SizeTSub(cchPassword, 1, &cbRandomBuffer);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    pbRandomBuffer = (PBYTE)malloc(cbRandomBuffer);
    if(!pbRandomBuffer) {
        hr = E_OUTOFMEMORY;
        goto cleanexit;
    }

    if(!CryptGenRandom(hCryptProvider,
                       cbRandomBuffer,
                       pbRandomBuffer)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    hr = SizeTMult(cchPassword, sizeof(WCHAR), &cbPassword);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = PasswordFromBytes(pbRandomBuffer, cbRandomBuffer, pszPassword, cbPassword);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

cleanexit:
    CryptReleaseContext(hCryptProvider,0);
    HB_SECURE_FREE(pbRandomBuffer, cbRandomBuffer);

    return hr;
}

HRESULT PasswordFromBytes(__in PBYTE pBytes, __in size_t cbBytes, __out PWSTR pszPassword, __in size_t cbPassword)
{
    HRESULT hr;
    size_t cbAsciiPassword = 0;
    PSTR pszAsciiPassword = NULL;
    size_t cchPassword;
    size_t cchConverted;

    cbAsciiPassword = cbPassword / sizeof(WCHAR);
    pszAsciiPassword = (PSTR)malloc(cbAsciiPassword);
    if(!pszAsciiPassword) {
        hr = E_OUTOFMEMORY;
        goto cleanexit;
    }

    //
    // Make sure all bytes are within printable range. We do lose some entropy,
    // but we still have enough.
    //

    for (size_t i = 0; i < cbBytes; i++) 
    {
        pszAsciiPassword[i] = (pBytes[i] % 0x4d) + 0x21;
    }
    pszAsciiPassword[cbAsciiPassword - 1] = '\0';

    //
    // Convert ASCII password to Unicode
    //

    cchPassword = cbPassword / sizeof(WCHAR);
    if (mbstowcs_s(&cchConverted,
                   pszPassword,
                   cbPassword / sizeof(WORD),
                   pszAsciiPassword,
                   cchPassword)) {
        hr = E_FAIL;
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:
    HB_SECURE_FREE(pszAsciiPassword, cbAsciiPassword);

    return hr;
}
