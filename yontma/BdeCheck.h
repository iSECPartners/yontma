
//
// Description:
//  Indicates whether the OS volume is actively being protected by BitLocker
//  Drive Encryption.
//
// Parameters:
//  pbIsProtected - On success, is set to TRUE if the OS volume is fully
//  encrypted and protection is not suspended and FALSE otherwise.
//
HRESULT IsOsVolumeProtectedByBitLocker(PBOOL pbIsProtected);