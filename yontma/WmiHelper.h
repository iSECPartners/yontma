HRESULT LoadWmi(void);
void CleanupWmi(void);
HRESULT GetNamespace(__in PCWSTR pObjectPath, __out IWbemServices** ppNamespace);
HRESULT GetInputParameters(__in IWbemServices* pSvc, __in PCWSTR pClassName,  __in PCWSTR pMethodName, __out IWbemClassObject** ppInParams);
HRESULT BStrToPWchar(__in BSTR* pBstr, __out PWSTR* ppWStr);
HRESULT VariantStringArrayToWstrArray(__in VARIANT vtStringArray, __out PWSTR** pppWstrArray, __out size_t* pcWstrArray);
