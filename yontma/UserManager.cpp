
#include "stdafx.h"

#define YONTMA_SERVICE_ACCOUNT_COMMENT L"Service account for YoNTMA (You'll Never Take Me Alive!) service."

//
// Description:
//  Creates a new user account under which the YoNTMA service will run.
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
    PWSTR pszAccountPasswordLocal;
    DWORD badParameterIndex;

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

    //TODO: Replace with a secure random password
    hr = StringCbCopy(pszAccountPasswordLocal, cbAccountPasswordLocal, L"fakefornow");
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    USER_INFO_1 userInfo = {
      YONTMA_SERVICE_ACCOUNT_NAME,
      pszAccountPasswordLocal,
      0,
      USER_PRIV_USER,
      NULL,
      YONTMA_SERVICE_ACCOUNT_COMMENT,
      UF_DONT_EXPIRE_PASSWD,
      NULL
    };
    
    if(NetUserAdd(NULL,
                  1,
                  (LPBYTE)&userInfo,
                  &badParameterIndex) != NERR_Success) {
        hr = E_FAIL;
        goto cleanexit;
    }

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

    if(NetUserDel(NULL, YONTMA_SERVICE_ACCOUNT_NAME) != NERR_Success) {
        hr = E_FAIL;
        goto cleanexit;
    }

    hr = S_OK;

cleanexit:

    return hr;
}

