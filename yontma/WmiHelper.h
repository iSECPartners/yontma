HRESULT LoadWmi();
VOID CleanupWmi();
HRESULT GetNamespace(__in PCWSTR pObjectPath,
                     __out IWbemServices** ppNamespace);