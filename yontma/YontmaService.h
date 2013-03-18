
void __stdcall ServiceMain(int argc, TCHAR* argv[]);
DWORD WINAPI ServiceHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);

#ifdef _DEBUG
void InitLogging(void);
void WriteLineToLog(char *pStr);
#else
#define InitLogging()
#define WriteLineToLog(__x)
#endif