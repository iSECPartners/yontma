
#define SERVICE_NAME TEXT("yontma")
#define SERVICE_DISPLAY_NAME TEXT("You'll Never Take Me Alive! (YoNTMA)")
#define SERVICE_FRIENDLY_DESCRIPTION TEXT("Protects the data on your laptop by automatically hibernating the machine when the screen is locked and wired Ethernet or AC power is disconnected.")
#define DEFAULT_SLEEP_TIME 500

typedef struct _PARTITION_TABLE {
    BYTE Status;
    BYTE Ignore1[3];
    BYTE Type;
    BYTE Ignore2[3];
    DWORD FirstSector;
    DWORD NumberOfSectors;
} PARTITION_TABLE, *PPARTITION_TABLE;

typedef struct _MONITOR_THREAD_PARAMS {
    HANDLE hMonitorStopEvent;
    HANDLE hMonitorEvent;
} MONITOR_THREAD_PARAMS, *PMONITOR_THREAD_PARAMS;

void __stdcall ServiceMain(int argc, TCHAR* argv[]);
DWORD WINAPI ServiceHandlerEx(DWORD dwControl,
                              DWORD dwEventType,
                              LPVOID lpEventData,
                              LPVOID lpContext);

HRESULT CheckYontmaRequirements();

HRESULT InstallYontma(void);
HRESULT RemoveYontma(void);

void HibernateMachine(void);

//DEBUG

#ifdef _DEBUG
const TCHAR LOGGING_MUTEX_NAME[] = TEXT("YontmaLoggingMutex");

void InitLogging(void);
void WriteLineToLog(char *pStr);
#else
#define InitLogging()
#define WriteLineToLog(__x)
#endif

HRESULT ProcessCommandLine(int argc, _TCHAR* argv[]);
HRESULT PerformInstall(void);
HRESULT PerformUninstall(void);
void PerformRunAsService(void);
void PrintUsage(void);