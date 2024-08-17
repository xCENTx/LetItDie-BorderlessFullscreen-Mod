#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>

struct ProcInfo
{
	DWORD m_PID;
	HANDLE m_hProc;
	HWND m_hWnd;
	__int64 m_base;
	int m_szScreen[2];
};

struct EnumWindowData
{
	unsigned int procId;
	HWND hwnd;
};

BOOL CALLBACK GetProcWindows(HWND window, LPARAM lParam)
{
	auto data = reinterpret_cast<EnumWindowData*>(lParam);

	DWORD windowPID;
	GetWindowThreadProcessId(window, &windowPID);

	bool isMainWindow = GetWindow(window, GW_OWNER) == (HWND)0 && IsWindowVisible(window);
	if (windowPID != data->procId || !isMainWindow)
		return true;

	data->hwnd = window;

	return true;
}

bool ResolveProcess(const wchar_t* pName, ProcInfo* pInfo)
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
		return false;

	PROCESSENTRY32 procEntry;
	procEntry.dwSize = sizeof(procEntry);
	if (!Process32Next(hSnap, &procEntry))
	{
		CloseHandle(hSnap);
		return false;
	}

	ProcInfo proc;
	do
	{
		if (_wcsicmp(procEntry.szExeFile, pName))
			continue;

		proc.m_PID = procEntry.th32ProcessID;
		proc.m_hProc = OpenProcess(PROCESS_ALL_ACCESS, false, procEntry.th32ProcessID);

		HANDLE modSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, proc.m_PID);
		if (modSnap == INVALID_HANDLE_VALUE)
		{
			CloseHandle(hSnap);
			return false;
		}

		MODULEENTRY32 me32;
		me32.dwSize = sizeof(me32);
		if (!Module32First(modSnap, &me32))
		{
			CloseHandle(modSnap);
			CloseHandle(hSnap);
			return false;
		}

		proc.m_base = (long long)me32.modBaseAddr;

		CloseHandle(modSnap);
		CloseHandle(hSnap);

		EnumWindowData eDat;
		eDat.procId = proc.m_PID;
		if (!EnumWindows(GetProcWindows, reinterpret_cast<LPARAM>(&eDat)))
			return false;

		proc.m_hWnd = eDat.hwnd;

		proc.m_szScreen[0] = GetSystemMetrics(SM_CXSCREEN);
		proc.m_szScreen[1] = GetSystemMetrics(SM_CYSCREEN);

		*pInfo = proc;

		return true;

	} while (Process32Next(hSnap, &procEntry));

	CloseHandle(hSnap);

	return false;
}

int main()
{
	ProcInfo pInfo;
	if (!ResolveProcess(L"BrgGame-Steam.exe", &pInfo))
	{
		MessageBoxA(0, "failed to resolve let it die process.\nplease ensure that the game is running and try again.", "ERROR", MB_ICONERROR | MB_OK);
		return EXIT_FAILURE;
	}

	HWND hwnd = pInfo.m_hWnd;
	SetWindowLongPtr(hwnd, GWL_STYLE, WS_BORDER | WS_MAXIMIZE);
	SetWindowPos(hwnd, 0, 0, 0, pInfo.m_szScreen[0], pInfo.m_szScreen[1], SWP_SHOWWINDOW);

	return EXIT_SUCCESS;
}
