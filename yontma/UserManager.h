
#define YONTMA_SERVICE_ACCOUNT_NAME L"yontmauser"
#define USERNAME L"yontma"
#define USERNAME_W_DOMAIN L".\\yontma"

HRESULT CreateServiceUserAccount(__out PWSTR* ppszAccountPassword, __out size_t* cbPassword);
HRESULT RemoveServiceUserAccount(void);
BOOL CreateYontmaUser(WCHAR *wcPassword,DWORD dwPwdSize);
BOOL DeleteYontmaUser(void);