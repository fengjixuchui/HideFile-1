#pragma once
#ifndef _STRUCT_H
#define _STRUCT_H

#define MAX_PATH 260

#define sfExAllocateMemory(SIZE) ExAllocatePoolWithTag(NonPagedPool,SIZE,'ytz')
#define sfExFreeMemory(Pointer) if(Pointer != NULL){ExFreePoolWithTag(Pointer,'ytz');Pointer = NULL;}

#define REG_PATH L"\\REGISTRY\\MACHINE\\SOFTWARE\\TY_HIDEFILE"

LIST_ENTRY FilterList;
FAST_MUTEX FilterFastMutex;

HANDLE KeyHandle;

typedef struct _FILTER_STRUCT
{
	LIST_ENTRY ListEntry;
	WCHAR FilePath[MAX_PATH];
}FILTER_STRUCT, *PFILTER_STRUCT;


VOID InitFilterList()
{
	InitializeListHead(&FilterList);
	ExInitializeFastMutex(&FilterFastMutex);
}

PFILTER_STRUCT GetItemFilterList()
{
	PFILTER_STRUCT TempItem;

	ExAcquireFastMutex(&FilterFastMutex);
	TempItem = (PFILTER_STRUCT)RemoveHeadList(&FilterList);
	ExReleaseFastMutex(&FilterFastMutex);

	return TempItem;
}

VOID InsertItemIntoFilterList(WCHAR *FilterFilePth)
{
	PFILTER_STRUCT TempItem;

	TempItem = sfExAllocateMemory(sizeof(FILTER_STRUCT));
	if (TempItem == NULL)
	{
		KdPrint(("AllocateMemory Fail!\n"));
		return;
	}

	RtlZeroMemory(TempItem, sizeof(FILTER_STRUCT));
	wcscpy(TempItem->FilePath, FilterFilePth);

	ExAcquireFastMutex(&FilterFastMutex);
	InsertHeadList(&FilterList, (PLIST_ENTRY)TempItem);
	ExReleaseFastMutex(&FilterFastMutex);
}

VOID RemoveItemFromFilterList(WCHAR *FilterFilePth)
{
	PFILTER_STRUCT TempItem;

	ExAcquireFastMutex(&FilterFastMutex);

	TempItem = (PFILTER_STRUCT)FilterList.Blink;
	while ((PLIST_ENTRY)TempItem != &FilterList)
	{
		if (wcsstr(TempItem->FilePath, FilterFilePth))
		{
			RemoveEntryList(&TempItem->ListEntry);
			sfExFreeMemory(TempItem);
			break;
		}
		TempItem = (PFILTER_STRUCT)TempItem->ListEntry.Blink;
	}

	ExReleaseFastMutex(&FilterFastMutex);
}

VOID EmptyList()
{
	PFILTER_STRUCT TempItem;

	ExAcquireFastMutex(&FilterFastMutex);

	TempItem = (PFILTER_STRUCT)RemoveHeadList(&FilterList);
	while ((PLIST_ENTRY)TempItem != &FilterList)
	{
		sfExFreeMemory(TempItem);
		TempItem = (PFILTER_STRUCT)RemoveHeadList(&FilterList);
	}

	ExReleaseFastMutex(&FilterFastMutex);
}

//创建保存着隐藏的路径的项注册表项
NTSTATUS CreateRegPath()
{
	OBJECT_ATTRIBUTES ObjectAttributes;
	UNICODE_STRING RegPath;
	NTSTATUS Status;
	ULONG Disposition;

	RtlInitUnicodeString(&RegPath, REG_PATH);
	RtlZeroMemory(&ObjectAttributes, sizeof(ObjectAttributes));
	ObjectAttributes.Length = sizeof(ObjectAttributes);
	ObjectAttributes.Attributes = OBJ_KERNEL_HANDLE;
	ObjectAttributes.ObjectName = &RegPath;

	Status = STATUS_SUCCESS;
	Status = ZwCreateKey(&KeyHandle, KEY_ALL_ACCESS, &ObjectAttributes, 0, NULL, REG_OPTION_NON_VOLATILE, &Disposition);
	if (!NT_SUCCESS(Status))
		KdPrint(("创建注册表项失败！错误码是：%x\n", Status));

	return Status;
}

//创建子键，保存着隐藏着文件路径的代码
VOID RegSetFilter(WCHAR *FilterFilePth)
{
	UNICODE_STRING ValueName;
	NTSTATUS Status;
	WCHAR Data[] = L"ZTY";

	RtlInitUnicodeString(&ValueName, FilterFilePth);
	Status = ZwSetValueKey(KeyHandle, &ValueName, 0, REG_SZ, Data, sizeof(Data));

	if (!NT_SUCCESS(Status))
		KdPrint(("设置：%ws 失败！错误码是：%x\n", FilterFilePth, Status));
}

//删除对应隐藏文件的项
VOID RegDeleteFilter(WCHAR *FilterFilePth)
{
	UNICODE_STRING DeleteValueName;
	RtlInitUnicodeString(&DeleteValueName, FilterFilePth);
	ZwDeleteValueKey(KeyHandle, &DeleteValueName);
}

//从注册表中读取当前所有保存的隐藏文件
VOID GetAllHideFileFromReg()
{
	PKEY_VALUE_BASIC_INFORMATION BasicInformation;
	KEY_FULL_INFORMATION FullInformation;
	WCHAR TempFilePath[MAX_PATH];

	NTSTATUS Status;
	ULONG Index = 0, TempIndex;
	ULONG Size;

	Status = ZwQueryKey(KeyHandle, KeyFullInformation, &FullInformation, sizeof(FullInformation), &Size);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("调用ZwQueryKey失败！错误码是：%x\n", Status));
		return;
	}

	while (Index < FullInformation.Values)
	{
		Status = ZwEnumerateValueKey(KeyHandle, Index, KeyValueBasicInformation, NULL, 0, &Size);
		if (Status != STATUS_BUFFER_TOO_SMALL)
		{
			KdPrint(("调用ZwEnumerateValueKey失败！错误码是：%x\n", Status));
			return;
		}

		BasicInformation = (PKEY_VALUE_BASIC_INFORMATION)sfExAllocateMemory(Size);
		Status = ZwEnumerateValueKey(KeyHandle, Index, KeyValueBasicInformation, BasicInformation, Size, &Size);
		if (!NT_SUCCESS(Status))
		{
			KdPrint(("调用ZwEnumerateValueKey失败！错误码是：%x\n", Status));
			sfExFreeMemory(BasicInformation);
			return;
		}

		RtlZeroMemory(TempFilePath, sizeof(TempFilePath));
		for (TempIndex = 0; TempIndex < BasicInformation->NameLength / 2; ++TempIndex)
			TempFilePath[TempIndex] = BasicInformation->Name[TempIndex];

		InsertItemIntoFilterList(TempFilePath);			//这里不确定BasicInformation到底是不是以\0结尾的字符串，所以自己生成一个拷贝过来

		sfExFreeMemory(BasicInformation);
		++Index;
	}
}

#endif // _STRUCT_H