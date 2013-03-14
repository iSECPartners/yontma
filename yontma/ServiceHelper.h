
typedef struct _SERVICE_HANDLER_PARAMS {
    HANDLE hServiceEndEvent;
    HANDLE hMonitorStopEvent;
    HANDLE hACDisconnectedEvent;
    HANDLE hWiredEthernetDisconnectedEvent;
} SERVICE_HANDLER_PARAMS, *PSERVICE_HANDLER_PARAMS;

HRESULT OpenYontmaService(__in SC_HANDLE hSCManager, __out SC_HANDLE* phService);
HRESULT CreateYontmaService(__in PCTSTR pServicePath, __in_opt PCWSTR pszServiceAccountPassword, __out SC_HANDLE* phService);
HRESULT DeleteYontmaService(void);
HRESULT RunYontmaService(__in PSERVICE_HANDLER_PARAMS pServiceHandlerParams);
void StopYontmaService(void);
HRESULT GetServiceInstalledPath(__out PTSTR pszServiceInstalledPath, __in size_t cchServiceInstalledPath);