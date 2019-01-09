#include <fltKernel.h>
#include <ntddk.h>
#include "Struct.h"
#include "IO.h"

PFLT_FILTER m_Filter;

FLT_PREOP_CALLBACK_STATUS FltPreCreate(__inout PFLT_CALLBACK_DATA Data, __in PCFLT_RELATED_OBJECTS FltObjects, __deref_out_opt PVOID *CompletionContext);
FLT_PREOP_CALLBACK_STATUS FltDirCtrlPreOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext);
FLT_POSTOP_CALLBACK_STATUS FltDirCtrlPostOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags);

NTSTATUS Unload(__in FLT_FILTER_UNLOAD_FLAGS Flags);

CONST FLT_OPERATION_REGISTRATION CallBack[] = {
	{ IRP_MJ_CREATE, 0, FltPreCreate, NULL },
	{ IRP_MJ_DIRECTORY_CONTROL, 0, FltDirCtrlPreOperation, FltDirCtrlPostOperation },
	{ IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION FilterRegistration = {
	sizeof(FLT_REGISTRATION),
	FLT_REGISTRATION_VERSION,
	0,
	NULL,
	CallBack,
	Unload,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

BOOLEAN CheckHideFile(PUNICODE_STRING DirectoryPath, PUNICODE_STRING FileName)
{
	PFILTER_STRUCT TempItem;
	USHORT i, j;

	BOOLEAN Ret = TRUE;

	ExAcquireFastMutex(&FilterFastMutex);
	TempItem = (PFILTER_STRUCT)FilterList.Blink;
	while ((PLIST_ENTRY)TempItem != &FilterList)
	{
		for (i = 0; i < DirectoryPath->Length / 2; ++i)
		{
			if (DirectoryPath->Buffer[i] != TempItem->FilePath[i])
				goto NEXT;
		}

		if (FileName == NULL)
		{
			if (TempItem->FilePath[i] == L'\0')
				break;
			else
				goto NEXT;
		}

		if (DirectoryPath->Buffer[i - 1] != L'\\')
			i += 1;										
		//这里发现一个讨厌的属性，如果是文件夹，那么在IRP_MJ_DIRECTORY_CONTROL消息中
		//在DirectoryPath中不带\符号，因此这里主动的跳过
		//举例，如果我想隐藏c:\a\b
		//在IRP_MJ_DIRECTORY_CONTROL消息中有时传入的就是c:\a和b
		//所以这里主动的跳过一下，用来规避

		for (j = 0; j < FileName->Length / 2; ++j)
		{
			if (FileName->Buffer[j] != TempItem->FilePath[i + j])
				goto NEXT;
		}

		if(TempItem->FilePath[i + j] == L'\0')
			break;															//到这里已经说明上面的goto都没有走，所以肯定是符合过滤的规范的，所以直接跳出
	NEXT:
		TempItem = (PFILTER_STRUCT)TempItem->ListEntry.Blink;
	}

	if ((PLIST_ENTRY)TempItem == &FilterList)
		Ret = FALSE;

	ExReleaseFastMutex(&FilterFastMutex);

	return Ret;
}

NTSTATUS CleanFileFullDirectoryInformation(PFILE_FULL_DIR_INFORMATION info, PFLT_FILE_NAME_INFORMATION fltName)
{
	PFILE_FULL_DIR_INFORMATION prevInfo = NULL;
	UNICODE_STRING fileName;
	NTSTATUS status = STATUS_SUCCESS;

	while (TRUE)
	{
		fileName.Buffer = info->FileName;
		fileName.Length = (USHORT)info->FileNameLength;
		fileName.MaximumLength = (USHORT)info->FileNameLength;

		if (CheckHideFile(&fltName->Name, &fileName))
		{
			KdPrint(("FileIdBothDirectoryInformation Remove:%wZ%wZ\n", &fltName->Name, &fileName));

			if (prevInfo)
			{
				if (info->NextEntryOffset)
				{
					prevInfo->NextEntryOffset += info->NextEntryOffset;			//如果Info的下一个存在，那么跳过Info
					info = (PFILE_FULL_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
					continue;
				}
				else
					prevInfo->NextEntryOffset = 0;								//如果下一个不存在，那就让PreInfo作为最后一个
			}
			else
			{
				if (info->NextEntryOffset == 0)
				{
					RtlZeroMemory(info, sizeof(FILE_FULL_DIR_INFORMATION));
					status = STATUS_NO_MORE_ENTRIES;
				}
				else
				{
					while (TRUE)
					{
						prevInfo = info;
						info = (PFILE_FULL_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
						RtlMoveMemory(prevInfo, info, sizeof(FILE_FULL_DIR_INFORMATION));

						if (info->NextEntryOffset == 0)
						{
							prevInfo->NextEntryOffset = 0;
							RtlZeroMemory(info, sizeof(FILE_FULL_DIR_INFORMATION));
							break;
						}
					}
				}
			}
		}

		if (info->NextEntryOffset == 0)
			break;

		prevInfo = info;
		info = (PFILE_FULL_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
	}

	return status;
}

NTSTATUS CleanFileBothDirectoryInformation(PFILE_BOTH_DIR_INFORMATION info, PFLT_FILE_NAME_INFORMATION fltName)
{
	PFILE_BOTH_DIR_INFORMATION prevInfo = NULL;
	UNICODE_STRING fileName;
	NTSTATUS status = STATUS_SUCCESS;

	while (TRUE)
	{
		fileName.Buffer = info->FileName;
		fileName.Length = (USHORT)info->FileNameLength;
		fileName.MaximumLength = (USHORT)info->FileNameLength;

		if (CheckHideFile(&fltName->Name, &fileName))
		{
			KdPrint(("FileIdBothDirectoryInformation Remove:%wZ%wZ\n", &fltName->Name, &fileName));

			if (prevInfo)
			{
				if (info->NextEntryOffset)
				{
					prevInfo->NextEntryOffset += info->NextEntryOffset;			//如果Info的下一个存在，那么跳过Info
					info = (PFILE_BOTH_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
					continue;
				}
				else
					prevInfo->NextEntryOffset = 0;								//如果下一个不存在，那就让PreInfo作为最后一个
			}
			else
			{
				if (info->NextEntryOffset == 0)
				{
					RtlZeroMemory(info, sizeof(FILE_BOTH_DIR_INFORMATION));
					status = STATUS_NO_MORE_ENTRIES;
				}
				else
				{
					while (TRUE)
					{
						prevInfo = info;
						info = (PFILE_BOTH_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
						RtlMoveMemory(prevInfo, info, sizeof(FILE_BOTH_DIR_INFORMATION));

						if (info->NextEntryOffset == 0)
						{
							prevInfo->NextEntryOffset = 0;
							RtlZeroMemory(info, sizeof(FILE_BOTH_DIR_INFORMATION));
							break;
						}
					}
				}
			}
		}

		if (info->NextEntryOffset == 0)
			break;

		prevInfo = info;
		info = (PFILE_BOTH_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
	}

	return status;
}

NTSTATUS CleanFileIdFullDirectoryInformation(PFILE_ID_FULL_DIR_INFORMATION info, PFLT_FILE_NAME_INFORMATION fltName)
{
	PFILE_ID_FULL_DIR_INFORMATION prevInfo = NULL;
	UNICODE_STRING fileName;
	NTSTATUS status = STATUS_SUCCESS;

	while (TRUE)
	{
		fileName.Buffer = info->FileName;
		fileName.Length = (USHORT)info->FileNameLength;
		fileName.MaximumLength = (USHORT)info->FileNameLength;

		if (CheckHideFile(&fltName->Name, &fileName))
		{
			KdPrint(("FileIdBothDirectoryInformation Remove:%wZ%wZ\n", &fltName->Name, &fileName));

			if (prevInfo)
			{
				if (info->NextEntryOffset)
				{
					prevInfo->NextEntryOffset += info->NextEntryOffset;			//如果Info的下一个存在，那么跳过Info
					info = (PFILE_ID_FULL_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
					continue;
				}
				else
					prevInfo->NextEntryOffset = 0;								//如果下一个不存在，那就让PreInfo作为最后一个
			}
			else
			{
				if (info->NextEntryOffset == 0)
				{
					RtlZeroMemory(info, sizeof(FILE_ID_FULL_DIR_INFORMATION));
					status = STATUS_NO_MORE_ENTRIES;
				}
				else
				{
					while (TRUE)
					{
						prevInfo = info;
						info = (PFILE_ID_FULL_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
						RtlMoveMemory(prevInfo, info, sizeof(FILE_ID_FULL_DIR_INFORMATION));

						if (info->NextEntryOffset == 0)
						{
							prevInfo->NextEntryOffset = 0;
							RtlZeroMemory(info, sizeof(FILE_ID_FULL_DIR_INFORMATION));
							break;
						}
					}
				}
			}
		}

		if (info->NextEntryOffset == 0)
			break;

		prevInfo = info;
		info = (PFILE_ID_FULL_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
	}

	return status;
}

NTSTATUS CleanFileIdBothDirectoryInformation(PFILE_ID_BOTH_DIR_INFORMATION info, PFLT_FILE_NAME_INFORMATION fltName)
{
	PFILE_ID_BOTH_DIR_INFORMATION prevInfo = NULL;
	UNICODE_STRING fileName;
	NTSTATUS status = STATUS_SUCCESS;

	while (TRUE)
	{
		fileName.Buffer = info->FileName;
		fileName.Length = (USHORT)info->FileNameLength;
		fileName.MaximumLength = (USHORT)info->FileNameLength;

		if (CheckHideFile(&fltName->Name, &fileName))
		{
			KdPrint(("FileIdBothDirectoryInformation Remove:%wZ%wZ\n", &fltName->Name, &fileName));

			if (prevInfo)
			{
				if (info->NextEntryOffset)
				{
					prevInfo->NextEntryOffset += info->NextEntryOffset;			//如果Info的下一个存在，那么跳过Info
					info = (PFILE_ID_BOTH_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
					continue;
				}
				else
					prevInfo->NextEntryOffset = 0;								//如果下一个不存在，那就让PreInfo作为最后一个
			}
			else
			{
				if (info->NextEntryOffset == 0)
				{
					RtlZeroMemory(info, sizeof(FILE_ID_BOTH_DIR_INFORMATION));
					status = STATUS_NO_MORE_ENTRIES;
				}
				else
				{
					while (TRUE)
					{
						prevInfo = info;
						info = (PFILE_ID_BOTH_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
						RtlMoveMemory(prevInfo, info, sizeof(FILE_ID_BOTH_DIR_INFORMATION));

						if (info->NextEntryOffset == 0)
						{
							prevInfo->NextEntryOffset = 0;
							RtlZeroMemory(info, sizeof(FILE_ID_BOTH_DIR_INFORMATION));
							break;
						}
					}
				}
			}
		}

		if (info->NextEntryOffset == 0)
			break;

		prevInfo = info;
		info = (PFILE_ID_BOTH_DIR_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
	}

	return status;
}

NTSTATUS CleanFileDirectoryInformation(PFILE_DIRECTORY_INFORMATION info, PFLT_FILE_NAME_INFORMATION fltName)
{
	PFILE_DIRECTORY_INFORMATION prevInfo = NULL;
	UNICODE_STRING fileName;
	NTSTATUS status = STATUS_SUCCESS;

	while (TRUE)
	{
		fileName.Buffer = info->FileName;
		fileName.Length = (USHORT)info->FileNameLength;
		fileName.MaximumLength = (USHORT)info->FileNameLength;

		if (CheckHideFile(&fltName->Name, &fileName))
		{
			KdPrint(("FileIdBothDirectoryInformation Remove:%wZ%wZ\n", &fltName->Name, &fileName));

			if (prevInfo)
			{
				if (info->NextEntryOffset)
				{
					prevInfo->NextEntryOffset += info->NextEntryOffset;			//如果Info的下一个存在，那么跳过Info
					info = (PFILE_DIRECTORY_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
					continue;
				}
				else
					prevInfo->NextEntryOffset = 0;								//如果下一个不存在，那就让PreInfo作为最后一个
			}
			else
			{
				if (info->NextEntryOffset == 0)
				{
					RtlZeroMemory(info, sizeof(FILE_DIRECTORY_INFORMATION));
					status = STATUS_NO_MORE_ENTRIES;
				}
				else
				{
					while (TRUE)
					{
						prevInfo = info;
						info = (PFILE_DIRECTORY_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
						RtlMoveMemory(prevInfo, info, sizeof(FILE_DIRECTORY_INFORMATION));

						if (info->NextEntryOffset == 0)
						{
							prevInfo->NextEntryOffset = 0;
							RtlZeroMemory(info, sizeof(FILE_DIRECTORY_INFORMATION));
							break;
						}
					}
				}
			}
		}

		if (info->NextEntryOffset == 0)
			break;

		prevInfo = info;
		info = (PFILE_DIRECTORY_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
	}

	return status;
}

NTSTATUS CleanFileNamesInformation(PFILE_NAMES_INFORMATION info, PFLT_FILE_NAME_INFORMATION fltName)
{
	PFILE_NAMES_INFORMATION prevInfo = NULL;
	UNICODE_STRING fileName;
	NTSTATUS status = STATUS_SUCCESS;

	while (TRUE)
	{
		fileName.Buffer = info->FileName;
		fileName.Length = (USHORT)info->FileNameLength;
		fileName.MaximumLength = (USHORT)info->FileNameLength;

		if (CheckHideFile(&fltName->Name, &fileName))
		{
			KdPrint(("FileIdBothDirectoryInformation Remove:%wZ%wZ\n", &fltName->Name, &fileName));

			if (prevInfo)
			{
				if (info->NextEntryOffset)
				{
					prevInfo->NextEntryOffset += info->NextEntryOffset;			//如果Info的下一个存在，那么跳过Info
					info = (PFILE_NAMES_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
					continue;
				}
				else
					prevInfo->NextEntryOffset = 0;								//如果下一个不存在，那就让PreInfo作为最后一个
			}
			else
			{
				if (info->NextEntryOffset == 0)
				{
					RtlZeroMemory(info, sizeof(FILE_NAMES_INFORMATION));
					status = STATUS_NO_MORE_ENTRIES;
				}
				else
				{
					while (TRUE)
					{
						prevInfo = info;
						info = (PFILE_NAMES_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
						RtlMoveMemory(prevInfo, info, sizeof(FILE_NAMES_INFORMATION));

						if (info->NextEntryOffset == 0)
						{
							prevInfo->NextEntryOffset = 0;
							RtlZeroMemory(info, sizeof(FILE_NAMES_INFORMATION));
							break;
						}
					}
				}
			}
		}

		if (info->NextEntryOffset == 0)
			break;

		prevInfo = info;
		info = (PFILE_NAMES_INFORMATION)((UCHAR*)info + info->NextEntryOffset);
	}

	return status;
}

FLT_PREOP_CALLBACK_STATUS FltPreCreate(__inout PFLT_CALLBACK_DATA Data, __in PCFLT_RELATED_OBJECTS FltObjects, __deref_out_opt PVOID *CompletionContext)
{
	PFLT_FILE_NAME_INFORMATION fltName;
	NTSTATUS status;

	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &fltName);
	if (!NT_SUCCESS(status))
	{
		if (status != STATUS_OBJECT_PATH_NOT_FOUND)
			DbgPrint("FsFilter1!" __FUNCTION__ ": FltGetFileNameInformation failed with code:%08x\n", status);

		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	status = FltParseFileNameInformation(fltName);
	if (!NT_SUCCESS(status))
	{
		if (status != STATUS_OBJECT_PATH_NOT_FOUND)
			DbgPrint("FsFilter1!" __FUNCTION__ ": FltParseFileNameInformation failed with code:%08x\n", status);

		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	if (CheckHideFile(&fltName->Name, NULL))
	{
		KdPrint(("创建文件：%wZ 失败！\n", &fltName->Name));
		Data->IoStatus.Status = STATUS_NO_SUCH_FILE;

		FltReleaseFileNameInformation(fltName);

		return FLT_PREOP_COMPLETE;
	}

	FltReleaseFileNameInformation(fltName);

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS FltDirCtrlPreOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	if (Data->Iopb->MinorFunction != IRP_MN_QUERY_DIRECTORY)
		return FLT_PREOP_SUCCESS_NO_CALLBACK;

	switch (Data->Iopb->Parameters.DirectoryControl.QueryDirectory.FileInformationClass)
	{
	case FileIdFullDirectoryInformation:
	case FileIdBothDirectoryInformation:
	case FileBothDirectoryInformation:
	case FileDirectoryInformation:
	case FileFullDirectoryInformation:
	case FileNamesInformation:
		break;
	default:
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS FltDirCtrlPostOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)
{
	PFLT_PARAMETERS params = &Data->Iopb->Parameters;
	PFLT_FILE_NAME_INFORMATION fltName;
	NTSTATUS status;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);


	if (!NT_SUCCESS(Data->IoStatus.Status))
		return FLT_POSTOP_FINISHED_PROCESSING;

	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &fltName);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("FsFilter1!" __FUNCTION__ ": FltGetFileNameInformation failed with code:%08x\n", status);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	__try
	{
		status = STATUS_SUCCESS;

		switch (params->DirectoryControl.QueryDirectory.FileInformationClass)
		{
		case FileFullDirectoryInformation:
			status = CleanFileFullDirectoryInformation((PFILE_FULL_DIR_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer, fltName);
			break;
		case FileBothDirectoryInformation:
			status = CleanFileBothDirectoryInformation((PFILE_BOTH_DIR_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer, fltName);
			break;
		case FileDirectoryInformation:
			status = CleanFileDirectoryInformation((PFILE_DIRECTORY_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer, fltName);
			break;
		case FileIdFullDirectoryInformation:
			status = CleanFileIdFullDirectoryInformation((PFILE_ID_FULL_DIR_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer, fltName);
			break;
		case FileIdBothDirectoryInformation:
			status = CleanFileIdBothDirectoryInformation((PFILE_ID_BOTH_DIR_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer, fltName);
			break;
		case FileNamesInformation:
			status = CleanFileNamesInformation((PFILE_NAMES_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer, fltName);
			break;
		}

		Data->IoStatus.Status = status;
	}
	__finally
	{
		FltReleaseFileNameInformation(fltName);
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS Unload(__in FLT_FILTER_UNLOAD_FLAGS Flags)
{
	UNICODE_STRING LinkName;

	RtlInitUnicodeString(&LinkName, L"\\??\\zty_19970121");
	IoDeleteSymbolicLink(&LinkName);
	IoDeleteDevice(DeviceObject);

	if (m_Filter)
		FltUnregisterFilter(m_Filter);

	if (KeyHandle)
		ZwClose(KeyHandle);

	EmptyList();

	KdPrint(("Unload Success!\n"));
	return STATUS_SUCCESS;
}

NTSTATUS InitFltFilter(PDRIVER_OBJECT DriverObject)
{
	NTSTATUS status;

	status = FltRegisterFilter(DriverObject, &FilterRegistration, &m_Filter);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("Register Filter UnSuccess!\n"));
		return STATUS_UNSUCCESSFUL;
	}

	status = FltStartFiltering(m_Filter);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("Start Filter UnSuccess!\n"));
		FltUnregisterFilter(m_Filter);
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegString)
{
	InitFilterList();
	//InsertItemIntoFilterList(L"\\Device\\HarddiskVolume1\\Program Files\\TY_HIDEFILE");

	if (!NT_SUCCESS(CreateRegPath()))
		return STATUS_UNSUCCESSFUL;

	if (!NT_SUCCESS(InitFltFilter(DriverObject)))
		return STATUS_UNSUCCESSFUL;

	if (!NT_SUCCESS(CreateDevice(DriverObject)))
		return STATUS_UNSUCCESSFUL;

	GetAllHideFileFromReg();

	KdPrint(("成功加载驱动！\n"));
	return STATUS_SUCCESS;
}