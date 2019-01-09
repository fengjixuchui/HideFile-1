#include <iostream>

#include "CreateService.h"
#include "resource.h"
#include "IO.h"

#define DirectoryPath "C:\\Program Files\\TY_HIDEFILE"
#define SysFilePath "C:\\Program Files\\TY_HIDEFILE\\HideFile.sys"
#define DriverName "HideFile"
#define RegPath "SOFTWARE\\TY_HIDEFILE"

using namespace std;

//��ѯSYS�ļ��Ƿ��ͷų�����
BOOL FindSysFile()
{
	OFSTRUCT t;

	return OpenFile(SysFilePath, &t, OF_EXIST) == -1 ? FALSE : TRUE;
}

BOOL CreateResource()
{
	HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(IDR_SYS1), "SYS");
	if (hRes == NULL)
		return FALSE;

	ULONG SizeRes = SizeofResource(NULL, hRes);
	if (SizeRes == 0)
		return FALSE;

	HGLOBAL GlobalRes = LoadResource(NULL, hRes);
	if (GlobalRes == NULL)
		return FALSE;

	LPVOID BaseRes = LockResource(GlobalRes);
	if (BaseRes == NULL)
		return FALSE;

	CreateDirectoryA(DirectoryPath, NULL);
	
	HANDLE FileHandle = CreateFileA(SysFilePath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (FileHandle == NULL || FileHandle == INVALID_HANDLE_VALUE)
		return FALSE;
	
	ULONG RetWrietNumbers;
	WriteFile(FileHandle, BaseRes, SizeRes, &RetWrietNumbers, NULL);

	CloseHandle(FileHandle);

	return TRUE;
}

BOOL EnumAllFilterFile()
{
	HKEY KeyHandle;
	//KEY_WOW64_64KEY��Ϊ��ǰ�ĳ�����32λ�ģ����������ӣ�����ᱻ�ض�λ�Ҳ�����Ӧ�ĵ�ַ
	LSTATUS Status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, RegPath, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &KeyHandle);
	if (Status != ERROR_SUCCESS || KeyHandle == NULL)
		return FALSE;

	ULONG NumberValues = 0;
	Status = RegQueryInfoKeyA(KeyHandle,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		&NumberValues,
		NULL,
		NULL,
		NULL,
		NULL);

	if (Status != ERROR_SUCCESS)
	{
		CloseHandle(KeyHandle);
		return FALSE;
	}

	if (NumberValues != 0)
		cout << "--------------------------------------------------------------------------" << endl;
	for (int Index = 0; Index < NumberValues; ++Index)
	{
		WCHAR FilterFile[MAX_PATH];
		ULONG ValueNameSize = sizeof(FilterFile);

		ZeroMemory(FilterFile, ValueNameSize);

		Status = RegEnumValueW(KeyHandle, Index, FilterFile, &ValueNameSize, NULL, NULL, NULL, NULL);
		
		if (Status != ERROR_SUCCESS)
		{
			CloseHandle(KeyHandle);
			return FALSE;
		}

		TurnDevicePathToDosPath(FilterFile);
		cout << "���صĵ�" << Index + 1 << "���ļ��ǣ�";
		wcout << FilterFile << endl;
	}
	if (NumberValues != 0)
		cout << "--------------------------------------------------------------------------" << endl;

	RegCloseKey(KeyHandle);
	return TRUE;
}

int main()
{
	std::wcout.imbue(std::locale("chs"));
	std::wcin.imbue(std::locale("chs"));

	ULONG DeleteFlag;
	cout << "������׼���������ܻ���ɾ�����ã�����1��ɾ��2����";
	cin >> DeleteFlag;
	if (DeleteFlag == 1)
	{
		if (!FindSysFile())				//����ļ��������򴴽��ļ�
		{
			if (!CreateResource())		//��������ļ�ʧ��
			{
				cout << "�����ļ�ʧ�ܣ�" << endl;
				system("pause");
			}

			//��Ϊ���ɵ������������������������ֻ��Ҫ�ڴ����ļ���ʱ��д��һ�μ���
			if (!InstallDriver(DriverName, SysFilePath))
			{
				cout << "��������ʧ�ܣ�" << "�������ǣ�" << GetLastError() << endl;
				system("pause");
				return -1;
			}

			if (!StartDriver(DriverName))
			{
				cout << "��������ʧ�ܣ�" << "�������ǣ�" << GetLastError() << endl;
				system("pause");
				return -1;
			}
		}

		if (!EnumAllFilterFile())
		{
			cout << "��ȡ�ļ����������ʧ�ܣ�" << "�������ǣ�" << GetLastError() << endl;
			system("pause");
			return -1;
		}

		if (!CreateDevice())
		{
			cout << "���豸ʧ�ܣ�" << "�������ǣ�" << GetLastError() << endl;
			system("pause");
			return -1;
		}

		ULONG Flag;						//��Ӧ��������ɾ��
		WCHAR FilePath[MAX_PATH];

		while (TRUE)
		{
			cout << "��ѡ�������ļ�����ȡ�����أ�1 / 2����";
			cin >> Flag;
			getchar();

			ZeroMemory(FilePath, sizeof(FilePath));
			if (Flag == MESSAGE_INSERT)
			{
				cout << "�����ļ�·����";
				wcin.getline(FilePath, MAX_PATH);
				//wcin >> FilePath;
				TurnDosPathToDevicePath(FilePath);
				InsertFileterFile(FilePath);
				EnumAllFilterFile();
			}
			else if (Flag == MESSAGE_DELETE)
			{
				cout << "�����ļ�·����";
				wcin.getline(FilePath, MAX_PATH);
				//wcin >> FilePath;
				TurnDosPathToDevicePath(FilePath);
				DeleteFileterFile(FilePath);
				EnumAllFilterFile();
			}
			else
			{
				cout << "��������ټ����׳գ�" << endl;
				break;
			}
		}

		CloseHandle(DeviceHandle);
	}
	else if (DeleteFlag == 2)
	{
		StopDriver(DriverName);
		DeleteDriver(DriverName);
		DeleteFileA(SysFilePath);

		HKEY KeyHandle;
		LSTATUS Status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SoftWare", 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &KeyHandle);
		if (Status != ERROR_SUCCESS || KeyHandle == NULL)
			return FALSE;

		RegDeleteKeyA(KeyHandle, "TY_HIDEFILE");
		RegCloseKey(KeyHandle);
	}

	system("pause");
	return 0;
}