#ifndef _CREATESERVICE_H
#define _CREATESERVICE_H

#include <Windows.h>

#define DRIVER_NAME "HelloDDK"
#define DRIVER_PATH ".\\HelloDDK.sys"

BOOL InstallDriver(char* lpszDriverName, char* lpszDriverPath, char* lpszAltitude = "370090");

BOOL StartDriver(char* lpszDriverName);

BOOL StopDriver(char* lpszDriverName);

BOOL DeleteDriver(char* lpszDriverName);

#endif