#include "stdafx.h"

#define HB_VOLUME_ID_LEN ARRAYSIZE(TEXT("\\\\?\\Volume{00000000-0000-0000-0000-000000000000}"))

static const UINT BDE_PROTECTION_STATUS_OFF         = 0;
static const UINT BDE_PROTECTION_STATUS_ON          = 1;

static const UINT BDE_PROTECTOR_TYPE_ANY                = 0;
static const UINT BDE_PROTECTOR_TYPE_TPM_ONLY           = 1;
static const UINT BDE_PROTECTOR_TYPE_EXTERNAL           = 2;
static const UINT BDE_PROTECTOR_TYPE_RECOVERY_PASSWORD  = 3;
static const UINT BDE_PROTECTOR_TYPE_TPM_PIN            = 4;
static const UINT BDE_PROTECTOR_TYPE_TPM_KEY            = 5;
static const UINT BDE_PROTECTOR_TYPE_TPM_PIN_KEY        = 6;
static const UINT BDE_PROTECTOR_TYPE_PUBLIC_KEY         = 7;
static const UINT BDE_PROTECTOR_TYPE_PASSPHRASE         = 8;
static const UINT BDE_PROTECTOR_TYPE_TPM_CERT           = 9;
static const UINT BDE_PROTECTOR_TYPE_SID                = 10;

HRESULT GetBitLockerWmiService(__out IWbemServices** ppSvc);
HRESULT GetBootVolumeId(__out PTSTR pVolumeId, __in size_t cchVolumeId);
HRESULT IsVolumeProtectedByBitLocker(__in IWbemServices* pBdeSvc, __in PCWSTR pVolumeId, PBOOL pbIsProtected);;
HRESULT HasTpmOnlyProtector(__in IWbemServices* pBdeSvc, __in PCWSTR pVolumeId, __out PBOOL pbHasTpmOnlyProtector);
HRESULT GetProtectionStatus(__in IWbemServices* pBdeSvc, __in PCWSTR pVolumeId, __out PUINT pProtectionStatus);

HRESULT VerifyBitLockerRequirements(void)
{
    HRESULT hr;
    BOOL bLoadedWmi = FALSE;
    IWbemServices* pBdeSvc = NULL;
    WCHAR BootVolumeId[HB_VOLUME_ID_LEN] = {0};
    BOOL bIsOsVolumeProtectedByBitLocker;
    BOOL bHasTpmOnlyProtector;

    hr = LoadWmi();
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }
    bLoadedWmi = TRUE;

    hr = GetBitLockerWmiService(&pBdeSvc);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = GetBootVolumeId(BootVolumeId, ARRAYSIZE(BootVolumeId));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = IsVolumeProtectedByBitLocker(pBdeSvc, BootVolumeId, &bIsOsVolumeProtectedByBitLocker);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    if(!bIsOsVolumeProtectedByBitLocker) {
        hr = E_YONTMA_OS_DRIVE_NOT_ENCRYPTED;
        goto cleanexit;
    }

    hr = HasTpmOnlyProtector(pBdeSvc, BootVolumeId, &bHasTpmOnlyProtector);
    if(bHasTpmOnlyProtector) {
        hr = E_YONTMA_BDE_TPM_ONLY_PROTECTOR;
        goto cleanexit;
    }

cleanexit:
    HB_SAFE_RELEASE(pBdeSvc);

    if(bLoadedWmi) {
        CleanupWmi();
    }

    return hr;
}

//
// Description:
//  Indicates whether the specified volume is actively being protected by BitLocker
//  Drive Encryption.
//
// Parameters:
//  pbIsProtected - On success, is set to TRUE if the volume is fully encrypted
//  and protection is not suspended and FALSE otherwise.
//
HRESULT IsVolumeProtectedByBitLocker(__in IWbemServices* pBdeSvc, __in PCWSTR pVolumeId, __out PBOOL pbIsProtected)
{
    HRESULT hr;
    UINT uProtectionStatus;

    hr = GetProtectionStatus(pBdeSvc, pVolumeId, &uProtectionStatus);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    *pbIsProtected = (uProtectionStatus == BDE_PROTECTION_STATUS_ON);

cleanexit:

    return hr;
}

//
// Description:
//  Retrieves the ID of the boot volume on the system (the volume where the
//  running OS is installed).
//
// Parameters:
//  pVolumeId - On success, contains the volume ID of the boot volume
//      (e.g. "\\?\Volume{fea9044e-0340-11e2-a5c6-806e6f6e6963}")
//
//  pVolumeIdLen - Size of the pVolumeId buffer (in characters)
//
HRESULT GetBootVolumeId(__out PWSTR pVolumeId, __in size_t cchVolumeId)
{
    HRESULT hr;
    IWbemServices* pNamespace = NULL;
    IEnumWbemClassObject* pEnumerator = NULL;
    IWbemClassObject *pBootVolume = NULL;
    ULONG uReturn = 0;
    VARIANT vtDeviceId = {0};

    hr = GetNamespace(L"ROOT\\CIMV2", &pNamespace);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = pNamespace->ExecQuery(bstr_t("WQL"),
                               bstr_t("SELECT * FROM Win32_Volume WHERE BootVolume = True"),
                               WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                               NULL,
                               &pEnumerator);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = pEnumerator->Next(WBEM_INFINITE,
                           1,
                           &pBootVolume,
                           &uReturn);
    if(uReturn == 0) {
        hr = S_OK;
        goto cleanexit;
    }

    hr = pBootVolume->Get(_bstr_t(L"DeviceID"),
                          0,
                          &vtDeviceId,
                          0,
                          0);

    wcsncpy_s(pVolumeId,
              cchVolumeId,
              vtDeviceId.bstrVal,
              HB_VOLUME_ID_LEN - 1);

cleanexit:
    HB_SAFE_RELEASE(pNamespace);

    return hr;
}

HRESULT GetBitLockerWmiService(__out IWbemServices** ppSvc)
{
    HRESULT hr;
    IWbemServices* pSvc = NULL;

    hr = GetNamespace(L"ROOT\\CIMV2\\Security\\MicrosoftVolumeEncryption", &pSvc);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    *ppSvc = pSvc;
    pSvc = NULL;

cleanexit:
    HB_SAFE_RELEASE(pSvc);

    return hr;
}

HRESULT ExecuteBitLockerMethod(__in IWbemServices* pBdeSvc, __in PCWSTR pVolumeId, __in PCWSTR pMethodName, __in IWbemClassObject* pInParams, __out IWbemClassObject** ppOutParams)
{
    HRESULT hr;
    const WCHAR ObjectPathFormat[] = L"Win32_EncryptableVolume.DeviceID='%s\\'";
    WCHAR ObjectPath[ARRAYSIZE(ObjectPathFormat) + HB_VOLUME_ID_LEN - 2 - 1];

    hr = StringCchPrintf(ObjectPath,
                         ARRAYSIZE(ObjectPath),
                         ObjectPathFormat,
                         pVolumeId);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = pBdeSvc->ExecMethod(bstr_t(ObjectPath),
                             bstr_t(pMethodName),
                             0,
                             NULL,
                             pInParams,
                             ppOutParams,
                             NULL );
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

cleanexit:

    return hr;
}

HRESULT GetBdeInputParameters(__in IWbemServices* pBdeSvc, __in PCWSTR pMethodName, __out IWbemClassObject** ppInParams)
{
    return GetInputParameters(pBdeSvc, L"Win32_EncryptableVolume", pMethodName, ppInParams);
}

//
// Description:
//  Wrapper for the Win32_EncryptableVolume::GetProtectionStatus method.
//
// Parameters:
//  pVolumeId - Specifies the device id of the volume to check.
//
//  pProtectionStatus - On success, receives the ProtectionStatus output
//      parameter of GetProtectionStatus.
//
HRESULT GetProtectionStatus(__in IWbemServices* pBdeSvc, __in PCWSTR pVolumeId, __out PUINT pProtectionStatus)
{
    HRESULT hr;
    IWbemClassObject* pOutParams = NULL;
    VARIANT vtProtectionStatus = {0};

    hr = ExecuteBitLockerMethod(pBdeSvc, pVolumeId, L"GetProtectionStatus", NULL, &pOutParams);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = pOutParams->Get(_bstr_t(L"ProtectionStatus"),
                         0,
                         &vtProtectionStatus,
                         0,
                         0);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    *pProtectionStatus = vtProtectionStatus.uintVal;

cleanexit:
    HB_SAFE_RELEASE(pOutParams);

    VariantClear(&vtProtectionStatus);

    return hr;
}

HRESULT GetKeyProtectors(__in IWbemServices* pBdeSvc, __in PCWSTR pVolumeId, __in UINT protectorType, __out PWSTR** pppProtectorIds, __out size_t* pcProtectorIds)
{
    HRESULT hr;
    const WCHAR MethodName[] = L"GetKeyProtectors";
    IWbemClassObject* pInParams = NULL;
    VARIANT vtKeyProtectorType = {0};
    IWbemClassObject* pOutParams = NULL;
    VARIANT vtProtectorIds = {0};

    hr = GetBdeInputParameters(pBdeSvc, MethodName, &pInParams);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    //
    // I would think that the type should be VT_UINT as it's a uint32, but
    // using VT_UINT causes Put() to return WBEM_E_TYPE_MISMATCH, while VT_UI1
    // works.
    //

    vtKeyProtectorType.vt = VT_UI1;
    vtKeyProtectorType.uintVal = protectorType;
    hr = pInParams->Put(L"KeyProtectorType", 0, &vtKeyProtectorType, 0);
    if(HB_FAILED(hr)) {
        goto cleanexit;;
    }

    hr = ExecuteBitLockerMethod(pBdeSvc, pVolumeId, MethodName, pInParams, &pOutParams);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = pOutParams->Get(_bstr_t(L"VolumeKeyProtectorID"),
                         0,
                         &vtProtectorIds,
                         0,
                         0);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = VariantStringArrayToWstrArray(vtProtectorIds, pppProtectorIds, pcProtectorIds);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

cleanexit:
    HB_SAFE_RELEASE(pInParams);
    HB_SAFE_RELEASE(pOutParams);

    VariantClear(&vtKeyProtectorType);
    VariantClear(&vtProtectorIds);

    return hr;
}

HRESULT HasTpmOnlyProtector(__in IWbemServices* pBdeSvc, __in PCWSTR pVolumeId, __out PBOOL pbHasTpmOnlyProtector)
{
    HRESULT hr;
    PWSTR* ppProtectorIds = NULL;
    size_t cProtectorIds = 0;
    BOOL bHasTpmOnlyProtector;

    hr = GetKeyProtectors(pBdeSvc,
                          pVolumeId,
                          BDE_PROTECTOR_TYPE_TPM_ONLY,
                          &ppProtectorIds,
                          &cProtectorIds);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    if(cProtectorIds > 0) {
        bHasTpmOnlyProtector = TRUE;
    }
    else {
        bHasTpmOnlyProtector = FALSE;
    }

    *pbHasTpmOnlyProtector = bHasTpmOnlyProtector;

cleanexit:
    HB_SAFE_FREE_ARRAY(ppProtectorIds, cProtectorIds);

    return hr;
}