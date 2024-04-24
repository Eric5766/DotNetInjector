#include <metahost.h>
#include <string>
#include <iostream>
#include <mscoree.h>

#include "coreclrhost.h"

#pragma comment(lib, "mscoree.lib")

#import "mscorlib.tlb" raw_interfaces_only				\
    high_property_prefixes("_get","_put","_putref")		\
    rename("ReportEvent", "InteropServices_ReportEvent")

typedef HRESULT(STDAPICALLTYPE* FnGetNETCoreCLRRuntimeHost)(REFIID riid, IUnknown** pUnk);

using namespace mscorlib;
using namespace std;

struct ClrArgs
{
	static const LPCWSTR DELIM;

	ClrArgs(LPCWSTR command)
	{
		int i = 0;
		wstring s(command);
		wstring* ptrs[] = { &pwzAssemblyPath, &pwzTypeName, &pwzMethodName, &pwzNetPath };
		while (s.find(DELIM) != wstring::npos && i < 4)
		{
			*ptrs[i++] = s.substr(0, s.find(DELIM));
			s.erase(0, s.find(DELIM) + 1);
		}

		if (s.length() > 0)
			pwzArgument = s;
		/*
		wcout << "Assembly Path:" << pwzAssemblyPath << endl;
		wcout << "Type Name:" << pwzTypeName << endl;
		wcout << "Method Name:" << pwzMethodName << endl;
		wcout << "Net Path:" << pwzNetPath << endl;
		wcout << "Arguments:" << pwzArgument << endl;*/
	}

	wstring pwzAssemblyPath;
	wstring pwzTypeName;
	wstring pwzMethodName;
	wstring pwzArgument;
	wstring pwzNetPath;
};

HMODULE InjectCoreCLR(char clrDirectoryPath[], char managedDLL[])
{
	std::string coreCLRDLL = std::string(clrDirectoryPath) + "\\coreclr.dll";
	std::wstring stemp = std::wstring(coreCLRDLL.begin(), coreCLRDLL.end());
	LPCWSTR lpcCoreCLRDLL = stemp.c_str();
	HINSTANCE hGetProcIDDLL = LoadLibrary(lpcCoreCLRDLL);
	if (!hGetProcIDDLL) {
		return NULL;
	}
	coreclr_initialize_ptr coreclr_init = (coreclr_initialize_ptr)GetProcAddress(hGetProcIDDLL, "coreclr_initialize");
	if (!coreclr_init) {
		return NULL;
	}

	std::string s1(managedDLL);
	s1 = s1.substr(0, s1.find_last_of("\\/"));
	auto appPath = s1.c_str();

	char appPath2[256];
	strcpy_s(appPath2, appPath);

	const char* property_keys[] = { "APP_PATHS","APP_CONTEXT_BASE_DIRECTORY" };
	const char* property_values[] = {
									 clrDirectoryPath,
									 appPath2
	};

	void* coreclr_handle;
	unsigned int domain_id;
	int ret =
		coreclr_init(clrDirectoryPath, // exePath
			"host",   // appDomainFriendlyName
			sizeof(property_values) / sizeof(char*), // propertyCount
			property_keys,                            // propertyKeys
			property_values,                          // propertyValues
			&coreclr_handle,                          // hostHandle
			&domain_id                                // domainId
		);

	if (ret < 0) {
		return NULL;
	}

	return  ::GetModuleHandle(L"coreclr.dll");
}

ICLRRuntimeHost* GetNETCoreCLRRuntimeHost(char clrDirectoryPath[], char managedDLL[])
{
	HMODULE coreCLRModule = ::GetModuleHandle(L"coreclr.dll");

	if (!coreCLRModule)
	{
		coreCLRModule = InjectCoreCLR(clrDirectoryPath, managedDLL);
		if (!coreCLRModule)
		{
			return nullptr;
		}
	}
	const auto pfnGetCLRRuntimeHost = reinterpret_cast<FnGetNETCoreCLRRuntimeHost>(::GetProcAddress(coreCLRModule, "GetCLRRuntimeHost"));

	if (!pfnGetCLRRuntimeHost)
	{

		return nullptr;
	}

	ICLRRuntimeHost* clrRuntimeHost = nullptr;
	const auto hr = pfnGetCLRRuntimeHost(IID_ICLRRuntimeHost, reinterpret_cast<IUnknown**>(&clrRuntimeHost));
	clrRuntimeHost->Start();
	if (FAILED(hr))
	{
		return nullptr;
	}

	return clrRuntimeHost;
}


HRESULT StartNetCore(ClrArgs args)
{
	std::string clrDirectoryPath = std::string(args.pwzNetPath.begin(), args.pwzNetPath.end());
	std::string managedDll = std::string(args.pwzAssemblyPath.begin(), args.pwzAssemblyPath.end());

	std::wstring lManagedDll = std::wstring(args.pwzAssemblyPath.begin(), args.pwzAssemblyPath.end());
	std::wstring lmanagedNamespace = std::wstring(args.pwzTypeName.begin(), args.pwzTypeName.end());
	std::wstring lmanagedMethod = std::wstring(args.pwzMethodName.begin(), args.pwzMethodName.end());
	std::wstring lmanagedArgs = std::wstring(args.pwzArgument.begin(), args.pwzArgument.end());

	ICLRRuntimeHost* pClrRuntimeHost = GetNETCoreCLRRuntimeHost(const_cast<char*>(clrDirectoryPath.c_str()), const_cast<char*>(managedDll.c_str()));

	DWORD dwRet = 0;

	HRESULT hr = pClrRuntimeHost->ExecuteInDefaultAppDomain(lManagedDll.c_str(), lmanagedNamespace.c_str(), lmanagedMethod.c_str(), lmanagedArgs.c_str(), &dwRet);

	pClrRuntimeHost->Release();

	return hr;
}

HRESULT StartNetFramework(ClrArgs args) {
	HRESULT hr;
	ICLRMetaHost* pMetaHost = NULL;
	ICLRRuntimeInfo* pRuntimeInfo = NULL;
	ICLRRuntimeHost* pClrRuntimeHost = NULL;

	hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_PPV_ARGS(&pMetaHost));
	hr = pMetaHost->GetRuntime(L"v4.0.30319", IID_PPV_ARGS(&pRuntimeInfo));
	hr = pRuntimeInfo->GetInterface(CLSID_CLRRuntimeHost, IID_PPV_ARGS(&pClrRuntimeHost));

	hr = pClrRuntimeHost->Start();

	DWORD pReturnValue;
	hr = pClrRuntimeHost->ExecuteInDefaultAppDomain(
		args.pwzAssemblyPath.c_str(),
		args.pwzTypeName.c_str(),
		args.pwzMethodName.c_str(),
		args.pwzArgument.c_str(),
		&pReturnValue);

	pMetaHost->Release();
	pRuntimeInfo->Release();
	pClrRuntimeHost->Release();

	return hr;
}

const LPCWSTR ClrArgs::DELIM = L"\t";

__declspec(dllexport) HRESULT ImplantDotNetAssembly(_In_ LPCTSTR lpCommand)
{
	//AllocConsole();
	//freopen("CONOUT$", "w", stdout);

	ClrArgs args(lpCommand);

	if (args.pwzNetPath.length() == 0)
	{
		return StartNetFramework(args);
	}
	else
	{
		return StartNetCore(args);
	}

}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
