#include "stdafx.h"

#define HB_VOLUME_ID_LEN ARRAYSIZE(TEXT("\\\\?\\Volume{00000000-0000-0000-0000-000000000000}"))

static const UINT BDE_PROTECTION_STATUS_OFF         = 0;
static const UINT BDE_PROTECTION_STATUS_ON          = 1;

HRESULT GetBootVolumeId(__out PTSTR pVolumeId,
                        __in size_t cchVolumeId);

HRESULT GetProtectionStatus(__in PCTSTR pVolumeId,
                            __out PUINT pProtectionStatus);

//
// Description:
//  Indicates whether the OS volume is actively being protected by BitLocker
//  Drive Encryption.
//
// Parameters:
//  pbIsProtected - On success, is set to TRUE if the OS volume is fully
//  encrypted and protection is not suspended and FALSE otherwise.
//
HRESULT IsOsVolumeProtectedByBitLocker(__out PBOOL pbIsProtected)
{
    HRESULT hr;
    WCHAR BootVolumeId[HB_VOLUME_ID_LEN] = {0};
    UINT uProtectionStatus;

    hr = GetBootVolumeId(BootVolumeId, ARRAYSIZE(BootVolumeId));
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }
    
    hr = GetProtectionStatus(BootVolumeId, &uProtectionStatus);
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
HRESULT GetBootVolumeId(__out PWSTR pVolumeId,
                        __in size_t cchVolumeId)
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
HRESULT GetProtectionStatus(__in PCWSTR pVolumeId,
                            __out PUINT pProtectionStatus)
{
    HRESULT hr;
    IWbemServices* pNamespace = NULL;
    IWbemClassObject* pOutParams = NULL;
    VARIANT vtProtectionStatus = {0};
    const WCHAR ObjectPathFormat[] = L"Win32_EncryptableVolume.DeviceID='%s\\'";
    WCHAR ObjectPath[ARRAYSIZE(ObjectPathFormat) + HB_VOLUME_ID_LEN - 2 - 1];
    
    hr = GetNamespace(L"ROOT\\CIMV2\\Security\\MicrosoftVolumeEncryption", &pNamespace);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = StringCchPrintf(ObjectPath,
                         ARRAYSIZE(ObjectPath),
                         ObjectPathFormat,
                         pVolumeId);
    if(HB_FAILED(hr)) {
        goto cleanexit;
    }

    hr = pNamespace->ExecMethod(bstr_t(ObjectPath),
                                bstr_t(L"GetProtectionStatus"),
                                0,
                                NULL,
                                NULL,
                                &pOutParams,
                                NULL );
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
    HB_SAFE_RELEASE(pNamespace);
    HB_SAFE_RELEASE(pOutParams);

    VariantClear(&vtProtectionStatus);

    return hr;
}