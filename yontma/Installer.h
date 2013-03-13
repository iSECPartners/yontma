#include <LM.h>
#include <Ntsecapi.h>

#define USERNAME L"yontma"
#define USERNAME_W_DOMAIN L".\\yontma"
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

HRESULT CopyYontmaBinaryToInstallLocation();
void RemoveYontmaBinaryFromInstallLocation(__in PTSTR pszInstalledPath);
HRESULT GetInstallPath(__out TCHAR* pszInstallPath, __in size_t cchInstallPath);
BOOL CreateYontmaUser(WCHAR *wcPassword,DWORD dwPwdSize);
BOOL AdjustYontmaAccountPrivileges(void);
bool InitLsaString(PLSA_UNICODE_STRING pLsaString,LPCWSTR pwszString);
BOOL GenerateRandomPassword(WCHAR *pBuffer,DWORD dwSize);
BOOL DeleteYontmaUser(void);