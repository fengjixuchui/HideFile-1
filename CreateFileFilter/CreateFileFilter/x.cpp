#include <iostream>

#include "CreateService.h"
#include "resource.h"
#include "IO.h"

#define DirectoryPath "C:\\Program Files\\TY_HIDEFILE"
#define SysFilePath "C:\\Program Files\\TY_HIDEFILE\\HideFile.sys"
#define DriverName "HideFile"
#define RegPath "SOFTWARE\\TY_HIDEFILE"

using namespace std;

//查询SYS文件是否被释放出来了
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
	//KEY_WOW64_64KEY因为当前的程序是32位的，这个必须添加，否则会被重定位找不到对应的地址
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
		cout << "隐藏的第" << Index + 1 << "个文件是：";
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
	cout << "请输入准备开启功能还是删除配置（开启1，删除2）：";
	cin >> DeleteFlag;
	if (DeleteFlag == 1)
	{
		if (!FindSysFile())				//如果文件不存在则创建文件
		{
			if (!CreateResource())		//如果创建文件失败
			{
				cout << "创建文件失败！" << endl;
				system("pause");
			}

			//因为生成的是自启动的驱动，因此这里只需要在创建文件的时候写入一次即可
			if (!InstallDriver(DriverName, SysFilePath))
			{
				cout << "创建服务失败！" << "错误码是：" << GetLastError() << endl;
				system("pause");
				return -1;
			}

			if (!StartDriver(DriverName))
			{
				cout << "开启服务失败！" << "错误码是：" << GetLastError() << endl;
				system("pause");
				return -1;
			}
		}

		if (!EnumAllFilterFile())
		{
			cout << "获取文件保存的内容失败！" << "错误码是：" << GetLastError() << endl;
			system("pause");
			return -1;
		}

		if (!CreateDevice())
		{
			cout << "打开设备失败！" << "错误码是：" << GetLastError() << endl;
			system("pause");
			return -1;
		}

		ULONG Flag;						//对应创建还是删除
		WCHAR FilePath[MAX_PATH];

		while (TRUE)
		{
			cout << "请选择隐藏文件还是取消隐藏（1 / 2）：";
			cin >> Flag;
			getchar();

			ZeroMemory(FilePath, sizeof(FilePath));
			if (Flag == MESSAGE_INSERT)
			{
				cout << "输入文件路径：";
				wcin.getline(FilePath, MAX_PATH);
				//wcin >> FilePath;
				TurnDosPathToDevicePath(FilePath);
				InsertFileterFile(FilePath);
				EnumAllFilterFile();
			}
			else if (Flag == MESSAGE_DELETE)
			{
				cout << "输入文件路径：";
				wcin.getline(FilePath, MAX_PATH);
				//wcin >> FilePath;
				TurnDosPathToDevicePath(FilePath);
				DeleteFileterFile(FilePath);
				EnumAllFilterFile();
			}
			else
			{
				cout << "输入错误，再见，白痴！" << endl;
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