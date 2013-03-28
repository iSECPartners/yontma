
#include "stdafx.h"

#define YONTMA_SERVICE_ACCOUNT_COMMENT L"Service account for YoNTMA (You'll Never Take Me Alive!) service."
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

HRESULT AdjustYontmaAccountPrivileges(void);
HRESULT RemoveServiceUserFromGroups(void);
HRESULT DeleteServiceUserProfile(void);
HRESULT CheckIfServiceUserExists(PBOOL pbUserExists);
bool InitLsaString(PLSA_UNICODE_STRING pLsaString,LPCWSTR pwszString);
HRESULT AddPrivilegeToAccount(__in LSA_HANDLE lsahPolicyHandle,__in PSID pSid, __in PTSTR pszPrivilegeName);
HRESULT RemovePrivilegeFromAccount(__in LSA_HANDLE lsahPolicyHandle,__in PSID pSid, __in PTSTR pszPrivilegeName);
HRESULT GetAccountSid(__in PCWSTR pszAccountName, __out PSID* ppSid);
HRESULT GenerateRandomPassword(__out PWSTR pszPassword, __in size_t cchPassword);
HRESULT PasswordFromBytes(__in PBYTE pBytes, __in size_t cbBytes, __out PWSTR pszPassword, __in size_t cbPassword);

//
// Description:
//  Creates a new, limited-privilege user account under which the YoNTMA service will run or
//  re-enables a previously created YoNTMA account (if YoNTMA was installed previously).
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
    DWORD badParameterIndex;
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
    size_t cbAccountPasswordLocal = 0;

    hr = CheckIfServiceUserExists(&bServiceUserExists);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    if(bServiceUserExists) {
        hr = RemoveServiceUserAccount();
        if(HB_FAILED(hr)) {
            goto cleanexit;
        }
    }

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

cleanexit:
    HB_SECURE_FREE(pszAccountPasswordLocal, cbAccountPasswordLocal);

    return hr;
}

HRESULT RemoveServiceUserAccount(void)
{
    HRESULT hr;
    HRESULT hrDeleteProfile;

    //
    // If we fail to delete the profile, we should delete the user, but save
    // the error.
    //

    hrDeleteProfile = DeleteServiceUserProfile();

    if(NetUserDel(NULL, YONTMA_SERVICE_ACCOUNT_NAME) != NERR_Success) {
        hr = E_FAIL;
        goto cleanexit;
    }

    hr = hrDeleteProfile;

cleanexit:

    return hr;
}

HRESULT DeleteServiceUserProfile(void)
{
    HRESULT hr;
    PSID pSid = NULL;
    PWSTR pszSidString = NULL;

    hr = GetAccountSid(YONTMA_SERVICE_ACCOUNT_NAME, &pSid);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    if(!ConvertSidToStringSid(pSid,&pszSidString)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    if(!DeleteProfile(pszSidString, NULL, NULL)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:
    HB_SAFE_FREE(pSid);
    HB_SAFE_LOCAL_FREE(pszSidString);

    return hr;
}

HRESULT CheckIfServiceUserExists(PBOOL pbUserExists)
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
}

HRESULT AdjustYontmaAccountPrivileges(void)
{
    HRESULT hr;
    PSID pSid = NULL;
    NTSTATUS ntReturn;
    LSA_OBJECT_ATTRIBUTES ObjectAttributes = {0};
    LSA_HANDLE lsahPolicyHandle = NULL;
    
    hr = GetAccountSid(YONTMA_SERVICE_ACCOUNT_NAME, &pSid);
    if(HB_FAILED(hr)) {
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

    hr = AddPrivilegeToAccount(lsahPolicyHandle, pSid, SE_SERVICE_LOGON_NAME);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    //
    // When we remove privileges, we don't care if we fail.
    //
    RemovePrivilegeFromAccount(lsahPolicyHandle, pSid, SE_BATCH_LOGON_NAME);
    RemovePrivilegeFromAccount(lsahPolicyHandle, pSid, SE_INTERACTIVE_LOGON_NAME);
    RemovePrivilegeFromAccount(lsahPolicyHandle, pSid, SE_NETWORK_LOGON_NAME);
    RemovePrivilegeFromAccount(lsahPolicyHandle, pSid, SE_REMOTE_INTERACTIVE_LOGON_NAME);

    hr = S_OK;

cleanexit:
    HB_SAFE_FREE(pSid);
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

bool InitLsaString(PLSA_UNICODE_STRING pLsaString, LPCWSTR pszString)
{
    size_t cchString = 0;

    if(NULL == pLsaString) {
        return FALSE;
    }

    if(NULL != pszString) {
        cchString = wcslen(pszString);
        if(cchString > 0x7ffe) {
            return FALSE;
        }
    }

    // Store the string.
    pLsaString->Buffer = (WCHAR*)pszString;
    pLsaString->Length =  (USHORT)cchString * sizeof(WCHAR);
    pLsaString->MaximumLength= (USHORT)(cchString + 1) * sizeof(WCHAR);

    return TRUE;
}

HRESULT AddPrivilegeToAccount(__in LSA_HANDLE lsahPolicyHandle,__in PSID pSid, __in PTSTR pszPrivilegeName)
{
    HRESULT hr;
    LSA_UNICODE_STRING lucStr;
    NTSTATUS ntReturn;

    if(!InitLsaString(&lucStr, pszPrivilegeName)) {
        hr = E_FAIL;
        goto cleanexit;
    }

    ntReturn = LsaAddAccountRights(lsahPolicyHandle, pSid, &lucStr, 1);
    if(ntReturn != STATUS_SUCCESS) {
        hr = HRESULT_FROM_WIN32(LsaNtStatusToWinError(ntReturn));
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:

    return hr;
}

HRESULT RemovePrivilegeFromAccount(__in LSA_HANDLE lsahPolicyHandle,__in PSID pSid, __in PTSTR pszPrivilegeName)
{
    HRESULT hr;
    LSA_UNICODE_STRING lucStr;
    NTSTATUS ntReturn;

    if(!InitLsaString(&lucStr, pszPrivilegeName)) {
        hr = E_FAIL;
        goto cleanexit;
    }

    ntReturn = LsaRemoveAccountRights(lsahPolicyHandle, pSid, FALSE, &lucStr, 1);
    if(ntReturn != STATUS_SUCCESS) {
        hr = HRESULT_FROM_WIN32(LsaNtStatusToWinError(ntReturn));
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:

    return hr;
}

//
// Description:
//  Retrieves the SID of the specified account.
//
// Parameters:
//  pszAccountName - The name of the account for which to retrieve the SID.
//
//  ppSid - On success, is set to the SID of the specified account name. Caller
//      must free with HB_SAFE_FREE.
//
HRESULT GetAccountSid(__in PCWSTR pszAccountName, __out PSID* ppSid)
{
    HRESULT hr;
    DWORD dwErr;
    PSID pSidLocal = NULL;
    DWORD cbSidLocal = 0;
    PWSTR pszReferencedDomain = NULL;
    DWORD cchReferencedDomain = 0;
    DWORD cbReferencedDomain;
    SID_NAME_USE SidNameUse;
    
    //
    // If we succeed with a zero-length buffer, something is wrong.
    //

    if(LookupAccountName(NULL,
                         pszAccountName,
                         &pSidLocal,
                         &cbSidLocal,
                         pszReferencedDomain,
                         &cchReferencedDomain,
                         &SidNameUse)) {
        hr = E_UNEXPECTED;
        goto cleanexit;
    }

    dwErr = GetLastError();
    if(dwErr != ERROR_INSUFFICIENT_BUFFER) {
        hr = HRESULT_FROM_WIN32(dwErr);
        goto cleanexit;
    }

    pSidLocal = (PSID)malloc(cbSidLocal);
    if(!pSidLocal) {
        hr = E_OUTOFMEMORY;
        goto cleanexit;
    }

    hr = DWordMult(cchReferencedDomain, sizeof(WCHAR), &cbReferencedDomain);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    pszReferencedDomain = (PWSTR)malloc(cbReferencedDomain);
    if(!pszReferencedDomain) {
        hr = E_OUTOFMEMORY;
        goto cleanexit;
    }

    if(!LookupAccountName(NULL,
                          pszAccountName,
                          pSidLocal,
                          &cbSidLocal,
                          pszReferencedDomain,
                          &cchReferencedDomain,
                          &SidNameUse)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto cleanexit;
    }

    *ppSid = pSidLocal;
    pSidLocal = NULL;

    hr = S_OK;

cleanexit:
    HB_SAFE_FREE(pSidLocal);
    HB_SAFE_FREE(pszReferencedDomain);

    return hr;
}

HRESULT GenerateRandomPassword(__out PWSTR pszPassword, __in size_t cchPassword)
{
    HRESULT hr;
    HCRYPTPROV hCryptProvider = NULL;
    DWORD cbRandomBuffer = 0;
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

    hr = SizeTToDWord(cchPassword, &cbRandomBuffer);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = DWordSub(cbRandomBuffer, 1, &cbRandomBuffer);
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
