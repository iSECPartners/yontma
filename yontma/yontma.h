
#define SERVICE_NAME TEXT("YoNTMA")
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

void __stdcall ServiceMain(int argc, char* argv[]);
DWORD WINAPI ServiceHandlerEx(DWORD dwControl,
                              DWORD dwEventType,
                              LPVOID lpEventData,
                              LPVOID lpContext);

DWORD InstallYontma(void);
DWORD RemoveYontma(void);

void HibernateMachine(void);

//Monitor functions
DWORD WINAPI PowerMonitorThread(LPVOID lpParams);
DWORD WINAPI WiredEthernetMonitorThread(LPVOID lpParams);

//DEBUG

#ifdef _DEBUG
const TCHAR LOGGING_MUTEX_NAME[] = TEXT("YontmaLoggingMutex");

void InitLogging(void);
void WriteLineToLog(char *pStr);
#else
#define InitLogging()
#define WriteLineToLog(__x)
#endif

void usage(void);