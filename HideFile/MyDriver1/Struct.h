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

//�������������ص�·������ע�����
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
		KdPrint(("����ע�����ʧ�ܣ��������ǣ�%x\n", Status));

	return Status;
}

//�����Ӽ����������������ļ�·���Ĵ���
VOID RegSetFilter(WCHAR *FilterFilePth)
{
	UNICODE_STRING ValueName;
	NTSTATUS Status;
	WCHAR Data[] = L"ZTY";

	RtlInitUnicodeString(&ValueName, FilterFilePth);
	Status = ZwSetValueKey(KeyHandle, &ValueName, 0, REG_SZ, Data, sizeof(Data));

	if (!NT_SUCCESS(Status))
		KdPrint(("���ã�%ws ʧ�ܣ��������ǣ�%x\n", FilterFilePth, Status));
}

//ɾ����Ӧ�����ļ�����
VOID RegDeleteFilter(WCHAR *FilterFilePth)
{
	UNICODE_STRING DeleteValueName;
	RtlInitUnicodeString(&DeleteValueName, FilterFilePth);
	ZwDeleteValueKey(KeyHandle, &DeleteValueName);
}

//��ע����ж�ȡ��ǰ���б���������ļ�
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
		KdPrint(("����ZwQueryKeyʧ�ܣ��������ǣ�%x\n", Status));
		return;
	}

	while (Index < FullInformation.Values)
	{
		Status = ZwEnumerateValueKey(KeyHandle, Index, KeyValueBasicInformation, NULL, 0, &Size);
		if (Status != STATUS_BUFFER_TOO_SMALL)
		{
			KdPrint(("����ZwEnumerateValueKeyʧ�ܣ��������ǣ�%x\n", Status));
			return;
		}

		BasicInformation = (PKEY_VALUE_BASIC_INFORMATION)sfExAllocateMemory(Size);
		Status = ZwEnumerateValueKey(KeyHandle, Index, KeyValueBasicInformation, BasicInformation, Size, &Size);
		if (!NT_SUCCESS(Status))
		{
			KdPrint(("����ZwEnumerateValueKeyʧ�ܣ��������ǣ�%x\n", Status));
			sfExFreeMemory(BasicInformation);
			return;
		}

		RtlZeroMemory(TempFilePath, sizeof(TempFilePath));
		for (TempIndex = 0; TempIndex < BasicInformation->NameLength / 2; ++TempIndex)
			TempFilePath[TempIndex] = BasicInformation->Name[TempIndex];

		InsertItemIntoFilterList(TempFilePath);			//���ﲻȷ��BasicInformation�����ǲ�����\0��β���ַ����������Լ�����һ����������

		sfExFreeMemory(BasicInformation);
		++Index;
	}
}

#endif // _STRUCT_H