#ifndef  _IO_H
#define _IO_H

#include <Windows.h>

using namespace std;

#define SYMBOL_NAME "\\??\\zty_19970121"

// 发送给驱动一段信息
#define  CWK_DVC_SEND_STR \
	(ULONG)CTL_CODE( \
	FILE_DEVICE_UNKNOWN, \
	0x911,METHOD_BUFFERED, \
	FILE_WRITE_DATA)

// 从驱动读取一段信息
#define  CWK_DVC_RECV_STR \
	(ULONG)CTL_CODE( \
	FILE_DEVICE_UNKNOWN, \
	0x912,METHOD_BUFFERED, \
	FILE_READ_DATA)

enum DEAL_VALUE
{
	MESSAGE_INSERT = 1,
	MESSAGE_DELETE = 2
};

typedef struct _FILE_MESSAGE
{
	enum DEAL_VALUE Flag;					//1表示增加，2表示删除
	WCHAR FilePath[MAX_PATH];
}FILE_MESSAGE, *PFILE_MESSAGE;

HANDLE DeviceHandle;

BOOL CreateDevice()
{
	DeviceHandle = CreateFileA(SYMBOL_NAME, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, 0);
	if (DeviceHandle == INVALID_HANDLE_VALUE)
		return FALSE;

	return TRUE;
}

VOID TurnDosPathToDevicePath(WCHAR *FilterFilePath)
{
	WCHAR DosName[3];
	DosName[0] = FilterFilePath[0];
	DosName[1] = FilterFilePath[1];
	DosName[2] = L'\0';

	WCHAR DeviceName[MAX_PATH];
	QueryDosDeviceW(DosName, DeviceName, MAX_PATH);
	wcscat(DeviceName, L"\\");
	wcscat(DeviceName, FilterFilePath + 3);
	wcscpy(FilterFilePath, DeviceName);
}

VOID TurnDevicePathToDosPath(WCHAR *FilterFilePath)
{
	WCHAR DosName[3] = L"C:";

	WCHAR DeviceName[MAX_PATH];
	while (DosName[0] <= L'Z')
	{
		ZeroMemory(DeviceName, sizeof(DeviceName));
		QueryDosDeviceW(DosName, DeviceName, MAX_PATH);
		if (DeviceName[22] == FilterFilePath[22])
			break;

		++DosName[0];
	}

	wcscpy(DeviceName, DosName);
	wcscat(DeviceName, FilterFilePath + 23);
	wcscpy(FilterFilePath, DeviceName);
}

VOID InsertFileterFile(WCHAR *FilterFilePath)
{
	FILE_MESSAGE FileMsg;
	ZeroMemory(&FileMsg, sizeof(FileMsg));
	FileMsg.Flag = MESSAGE_INSERT;
	wcscpy(FileMsg.FilePath, FilterFilePath);

	ULONG RetLength;
	DeviceIoControl(DeviceHandle, CWK_DVC_SEND_STR, &FileMsg, sizeof(FileMsg), NULL, 0, &RetLength, 0);
}

VOID DeleteFileterFile(WCHAR *FilterFilePath)
{
	FILE_MESSAGE FileMsg;
	ZeroMemory(&FileMsg, sizeof(FileMsg));
	FileMsg.Flag = MESSAGE_DELETE;
	wcscpy(FileMsg.FilePath, FilterFilePath);

	ULONG RetLength;
	DeviceIoControl(DeviceHandle, CWK_DVC_SEND_STR, &FileMsg, sizeof(FileMsg), NULL, 0, &RetLength, 0);
}

#endif