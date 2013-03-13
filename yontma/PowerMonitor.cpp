
#include "stdafx.h"

DWORD WINAPI PowerMonitorThread(LPVOID lpParams)
{
    SYSTEM_POWER_STATUS PowerStatus;
    PMONITOR_THREAD_PARAMS pMonitorThreadParams = (PMONITOR_THREAD_PARAMS)lpParams;
    
    WriteLineToLog("PowerMonitorThread: Started");

    if(!GetSystemPowerStatus(&PowerStatus)) {
        WriteLineToLog("PowerMonitorThread: Error detecting initial power state.");
        goto cleanexit;
    }
    if(!PowerStatus.ACLineStatus) {
        WriteLineToLog("PowerMonitorThread: Machine was not connected to AC power at lock time.");
        goto cleanexit;
    }

    while(1) {
        if(!GetSystemPowerStatus(&PowerStatus)) {
            continue;
        }
        if(!PowerStatus.ACLineStatus) {
            WriteLineToLog("PowerMonitorThread: Firing monitor event");
            SetEvent(pMonitorThreadParams->hMonitorEvent);
        }
        else {
            ResetEvent(pMonitorThreadParams->hMonitorEvent);
        }
        switch (WaitForSingleObject(pMonitorThreadParams->hMonitorStopEvent, DEFAULT_SLEEP_TIME)) {
        case WAIT_OBJECT_0:
            goto cleanexit;
        case WAIT_TIMEOUT:
            continue;
        }
    }

cleanexit:
    WriteLineToLog("PowerMonitorThread: Exiting");
    HB_SAFE_FREE(pMonitorThreadParams);

    return 0;
}