#include <string>
#include "windows.h"
#include "Shlwapi.h"
#include "TlHelp32.h"
#include <stdexcept>
#include <iostream>
#pragma comment(lib, "shlwapi")

using namespace std;

#pragma region constants

static const wchar_t* BOOTSTRAP_DLL = L"Bootstrap.dll";

static const wchar_t* PARAMETERS[] = { L"-m", L"-i", L"-l",  L"-n", L"-c", L"-a" };

static const wstring DELIM = L"\t";

#pragma endregion

DWORD_PTR Inject(const HANDLE hProcess, const LPVOID function, const wstring& argument);
DWORD_PTR GetFunctionOffset(const wstring& library, const char* functionName);
DWORD GetProcessIdByName(LPCTSTR processName);
DWORD_PTR GetRemoteModuleHandle(const int processId, const wchar_t* moduleName);
void EnablePrivilege(const wchar_t* lpPrivilegeName, const bool bEnable);
inline size_t GetStringAllocSize(const wstring& str);
wstring GetBootstrapPath();
bool ParseArgs(int argc, wchar_t* argv[]);

LPCTSTR g_processName = NULL;
int g_processId = 0;		// id of process to inject
wchar_t* g_netPath = NULL;	// Path of .net runtime
wchar_t* g_moduleName = NULL;	// full path of managed assembly (exe or dll) to inject
wchar_t* g_typeName = NULL;	// the assembly and type of the managed assembly 
wchar_t* g_methodName = NULL;	// method to execute
wchar_t* g_Argument = NULL;	// optional argument to pass into the method to execute


int wmain(int argc, wchar_t* argv[])
{
	AllocConsole();
	Sleep(5000);
	if (!ParseArgs(argc, argv))
	{
		return -1;
	}

	wcout << "Modulename = " << g_moduleName << endl;
	wcout << "Typename = " << g_typeName << endl;
	wcout << "Methodname = " << g_methodName << endl;
	wcout << "Processname = " << g_processName << endl;
	wcout << "Netpath = " << g_netPath << endl;
	wcout << "Arguments = " << g_Argument << endl;


	while (g_processId == 0)
	{
		cout << "Waiting for process to start...";
		g_processId = GetProcessIdByName(g_processName);
		Sleep(1000);
	}
	/*
	g_moduleName = (wchar_t*)L"C:\\Users\\Eric\\source\\repos\\Finj\\Release\\CoreTest.dll";
	g_typeName = (wchar_t*)L"CoreTest.Program";
	g_methodName = (wchar_t*)L"EntryPoint";
	g_Argument = (wchar_t*)L"";*/

	EnablePrivilege(SE_DEBUG_NAME, TRUE);

	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_processId);

	FARPROC fnLoadLibrary = GetProcAddress(GetModuleHandle(L"Kernel32"), "LoadLibraryW");
	Inject(hProcess, fnLoadLibrary, GetBootstrapPath());

	DWORD_PTR hBootstrap = GetRemoteModuleHandle(g_processId, BOOTSTRAP_DLL);
	DWORD_PTR offset = GetFunctionOffset(GetBootstrapPath(), "ImplantDotNetAssembly");
	DWORD_PTR fnImplant = hBootstrap + offset;

	wstring argument = g_moduleName + DELIM + g_typeName + DELIM + g_methodName + DELIM + g_netPath + DELIM + g_Argument;

	Inject(hProcess, (LPVOID)fnImplant, argument);

	FARPROC fnFreeLibrary = GetProcAddress(GetModuleHandle(L"Kernel32"), "FreeLibrary");
	CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)fnFreeLibrary, (LPVOID)hBootstrap, NULL, 0);

	CloseHandle(hProcess);

	return 0;
}

DWORD_PTR Inject(const HANDLE hProcess, const LPVOID function, const wstring& argument)
{
	LPVOID baseAddress = VirtualAllocEx(hProcess, NULL, GetStringAllocSize(argument), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	BOOL isSucceeded = WriteProcessMemory(hProcess, baseAddress, argument.c_str(), GetStringAllocSize(argument), NULL);

	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)function, baseAddress, NULL, 0);

	WaitForSingleObject(hThread, INFINITE);

	VirtualFreeEx(hProcess, baseAddress, 0, MEM_RELEASE);

	DWORD exitCode = 0;
	GetExitCodeThread(hThread, &exitCode);

	CloseHandle(hThread);

	return exitCode;
}

DWORD_PTR GetFunctionOffset(const wstring& library, const char* functionName)
{
	HMODULE hLoaded = LoadLibrary(library.c_str());

	void* lpInject = GetProcAddress(hLoaded, functionName);

	DWORD_PTR offset = (DWORD_PTR)lpInject - (DWORD_PTR)hLoaded;

	FreeLibrary(hLoaded);

	return offset;
}

DWORD GetProcessIdByName(LPCTSTR ProcessName)
{
	PROCESSENTRY32 pt;
	HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	pt.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hsnap, &pt)) {
		do
		{
			if (!lstrcmpi(pt.szExeFile, ProcessName)) {
				CloseHandle(hsnap);
				return pt.th32ProcessID;
			}
		} while (Process32Next(hsnap, &pt));
	}
	CloseHandle(hsnap);
	return 0;
}

DWORD_PTR GetRemoteModuleHandle(const int processId, const wchar_t* moduleName)
{
	MODULEENTRY32 me32;
	HANDLE hSnapshot = INVALID_HANDLE_VALUE;

	me32.dwSize = sizeof(MODULEENTRY32);
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);

	if (!Module32First(hSnapshot, &me32))
	{
		CloseHandle(hSnapshot);
		return 0;
	}

	while (wcscmp(me32.szModule, moduleName) != 0 && Module32Next(hSnapshot, &me32));

	CloseHandle(hSnapshot);

	if (wcscmp(me32.szModule, moduleName) == 0)
		return (DWORD_PTR)me32.modBaseAddr;

	return 0;
}


void EnablePrivilege(const wchar_t* lpPrivilegeName, const bool enable)
{
	HANDLE token;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
		return;

	TOKEN_PRIVILEGES privileges;
	ZeroMemory(&privileges, sizeof(privileges));
	privileges.PrivilegeCount = 1;
	privileges.Privileges[0].Attributes = (enable) ? SE_PRIVILEGE_ENABLED : 0;
	if (!LookupPrivilegeValue(NULL, lpPrivilegeName, &privileges.Privileges[0].Luid))
	{
		CloseHandle(token);
		return;
	}

	BOOL result = AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), NULL, NULL);

	CloseHandle(token);
}

inline size_t GetStringAllocSize(const wstring& str)
{
	return (wcsnlen(str.c_str(), 65536) * sizeof(wchar_t)) + sizeof(wchar_t);
}

wstring GetBootstrapPath()
{
	wchar_t buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);

	PathRemoveFileSpec(buffer);

	size_t size = wcslen(buffer) + wcslen(BOOTSTRAP_DLL);
	if (size <= MAX_PATH)
		PathAppend(buffer, BOOTSTRAP_DLL);
	else
		throw runtime_error("Module name cannot exceed MAX_PATH");

	return buffer;
}

bool ParseArgs(int argc, wchar_t* argv[])
{
	for (int i = 1; (i < argc) && ((i + 1) < argc); i += 2)
	{
		if (wcscmp(argv[i], L"-m") == 0)
			g_methodName = argv[i + 1];
		else if (wcscmp(argv[i], L"-i") == 0)
			g_moduleName = argv[i + 1];
		else if (wcscmp(argv[i], L"-l") == 0)
			g_typeName = argv[i + 1];
		else if (wcscmp(argv[i], L"-n") == 0)
			g_processName = (LPCTSTR)argv[i + 1];
		else if (wcscmp(argv[i], L"-c") == 0)
			g_netPath = argv[i + 1];
		else if (wcscmp(argv[i], L"-a") == 0)
			g_Argument = argv[i + 1];
	}

	// basic validation
	if (g_moduleName == NULL || g_typeName == NULL || g_methodName == NULL || g_processName == NULL)
		return false;
	else if (wcslen(g_moduleName) > MAX_PATH)
		throw runtime_error("Module name cannot exceed MAX_PATH");

	return true;
}