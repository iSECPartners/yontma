// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <windows.h>
#include <PowrProf.h>
#include <winsock2.h>
#include <Iphlpapi.h>
#include <IPTypes.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <strsafe.h>

#include "BdeCheck.h"
#include "WmiHelper.h"
#include "ServiceHelper.h"
#include "yontma.h"

#define HB_FAILED(__x) ((__x) != S_OK)
#define HB_SAFE_RELEASE(__x) { if((__x)) { (__x)->Release(); (__x) = NULL; } }
#define HB_SAFE_CLOSE_SERVICE_HANDLE(__x) { if((__x)) { CloseServiceHandle((__x)); (__x) = NULL; } }
#define HB_SAFE_FREE(__x) { if((__x)) { free((__x)); (__x) = NULL; } }
