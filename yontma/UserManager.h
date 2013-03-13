
#define YONTMA_SERVICE_ACCOUNT_NAME L"yontmauser"

HRESULT CreateServiceUserAccount(__out PWSTR* ppszAccountPassword, __out size_t* cbPassword);
HRESULT RemoveServiceUserAccount(void);