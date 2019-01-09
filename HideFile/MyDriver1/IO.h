#pragma once
#ifndef  _IO_H
#define _IO_H

#include "Struct.h"

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

PDEVICE_OBJECT DeviceObject;
HANDLE m_Process;

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

NTSTATUS DispatchFunction(IN PDEVICE_OBJECT c_DeviceObject, IN PIRP Irp)
{
	PIO_STACK_LOCATION IoStack = IoGetCurrentIrpStackLocation(Irp);
	PFILE_MESSAGE TempUserData;

	NTSTATUS status = STATUS_SUCCESS;
	ULONG Ret = 0;

	PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG InputLength = IoStack->Parameters.DeviceIoControl.InputBufferLength;
	ULONG OutputLength = IoStack->Parameters.DeviceIoControl.OutputBufferLength;

	if (IoStack->MajorFunction == IRP_MJ_DEVICE_CONTROL)
	{
		if (IoStack->Parameters.DeviceIoControl.IoControlCode == CWK_DVC_SEND_STR)
		{
			if (InputLength == sizeof(FILE_MESSAGE))
			{
				TempUserData = (PFILE_MESSAGE)Buffer;
				if (TempUserData->Flag == MESSAGE_INSERT)
				{
					InsertItemIntoFilterList(TempUserData->FilePath);
					RegSetFilter(TempUserData->FilePath);
				}
				else if (TempUserData->Flag == MESSAGE_DELETE)
				{
					RemoveItemFromFilterList(TempUserData->FilePath);
					RegDeleteFilter(TempUserData->FilePath);
				}
				else
				{
					Ret = 0;
					status = STATUS_UNSUCCESSFUL;
				}
			}
			else
			{
				Ret = 0;
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}

	Irp->IoStatus.Information = Ret;
	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS CreateDevice(IN PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING	DeviceName;
	UNICODE_STRING	SymName;
	PDEVICE_OBJECT	Device;
	NTSTATUS		Status;
	ULONG			Index;

	Status = STATUS_SUCCESS;

	RtlInitUnicodeString(&DeviceName, L"\\Device\\zty_1997021");
	RtlInitUnicodeString(&SymName, L"\\??\\zty_19970121");

	Status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, FILE_OPEN, TRUE, &Device);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("创建设备失败！\n"));
		return Status;
	}

	Status = IoCreateSymbolicLink(&SymName, &DeviceName);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("创建符号链接失败！\n"));
		IoDeleteDevice(Device);
		return Status;
	}

	for (Index = 0; Index < IRP_MJ_MAXIMUM_FUNCTION; ++Index)
		DriverObject->MajorFunction[Index] = DispatchFunction;

	DeviceObject = Device;

	return Status;
}

#endif // ! _IO_H


