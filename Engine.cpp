/*
A Microsoft Windows memory page delta tool

Released as open source by NCC Group Plc - http://www.nccgroup.com/

Developed by Ollie Whitehouse, ollie dot whitehouse at nccgroup dot com

https://github.com/nccgroup/WindowsMemPageDelta

Released under AGPL see LICENSE for more information
*/

// Includes
#include "stdafx.h"

bool	bFirstRun = true;
bool	bVerbose = false;
TCHAR	strErrMsg[1024];
DWORD	dwModuleRelocs = 0;
HANDLE	event_log = NULL;

//
#pragma pack(push, 1)
struct procNfoStuct {
	DWORD PID;
	TCHAR Name[MAX_PATH];
	unsigned long long TotalExecMem = 0;
};
#pragma pack(pop)

//
#pragma pack(push, 1)
struct procStuct
{
	DWORD PID;
	unsigned long long address;
	unsigned long allocprotection;
	unsigned long long size;
	unsigned long protection;
	unsigned long state;
	unsigned long type;
};
#pragma pack(pop)

procStuct megaStruc[1000000];
procStuct megaStruc2[1000000];
unsigned long long lastEntry = 0;
unsigned long long lastEntry2 = 0;

procNfoStuct Procs[4098];
DWORD NumOfProcs = 0;

// Manual imports
_NtQueryInformationProcess __NtQueryInformationProcess = (_NtQueryInformationProcess)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "NtQueryInformationProcess");
typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process = fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");


//
BOOL putinMegaStruc(DWORD dwPID, TCHAR* cProcess, ULONG64 BaseAddress, DWORD AllocationProtect, SIZE_T RegionSize, DWORD Protect, DWORD State, DWORD Type) {

	megaStruc[lastEntry].PID = dwPID;
	megaStruc[lastEntry].address = BaseAddress;
	megaStruc[lastEntry].allocprotection = AllocationProtect;
	megaStruc[lastEntry].size = RegionSize;
	megaStruc[lastEntry].protection = Protect;
	megaStruc[lastEntry].state = State;
	megaStruc[lastEntry].type = Type;
	lastEntry++;
	return TRUE;
}

//
BOOL putinMegaStrucAlt(DWORD dwPID, TCHAR* cProcess, ULONG64 BaseAddress, DWORD AllocationProtect, SIZE_T RegionSize, DWORD Protect, DWORD State, DWORD Type) {

	megaStruc2[lastEntry2].PID = dwPID;
	megaStruc2[lastEntry2].address = BaseAddress;
	megaStruc2[lastEntry2].allocprotection = AllocationProtect;
	megaStruc2[lastEntry2].size = RegionSize;
	megaStruc2[lastEntry2].protection = Protect;
	megaStruc2[lastEntry2].state = State;
	megaStruc2[lastEntry2].type = Type;
	lastEntry2++;

	return TRUE;
}

CONST TCHAR* Protection(DWORD Protection)
{
	switch (Protection) {
	case PAGE_EXECUTE:
		return L"X.....";

	case PAGE_EXECUTE_READ:
		return L"XR....";


	case PAGE_EXECUTE_READWRITE:
		return L"XRW....";

	case PAGE_NOACCESS:
		return L".......";

	case PAGE_READONLY:
		return L".R.....";

	case PAGE_READWRITE:
		return L".RW....";

	case PAGE_WRITECOPY:
		return L"..WC...";

	case PAGE_GUARD:
		return L"....G..";

	case PAGE_NOCACHE:
		return L".....c.";

	case PAGE_WRITECOMBINE:
		return L".....Cw";

	default:
		return L"UKNOWN.";

	}



}

//
// This calculates the total executable memory for a process
// and then logs it
//
void calcTotals() {

	DWORD dwCount = 0;
	TCHAR logLine[4098];

	while (Procs[dwCount].PID != 0) {

		for (DWORD dwCountInner = 0; dwCountInner < lastEntry2; dwCountInner++) {

			if (megaStruc2[dwCountInner].PID == Procs[dwCount].PID) {
				Procs[dwCount].TotalExecMem += megaStruc2[dwCountInner].size;
			}
			else if(megaStruc2[dwCountInner].PID == 0)
			{
				continue;
			} 

		}

		_stprintf_s(logLine, TEXT("Total,%u,%s,%llu\n"), Procs[dwCount].PID, Procs[dwCount].Name, Procs[dwCount].TotalExecMem);
		if (bConsole == true) {
			_ftprintf(stdout, logLine);
		}
		WriteTotal(logLine);
		

		dwCount++;
	}

}

//
// This diffs the new exec pages
// and then logs it
//
BOOL diffMegaStrucAlt() {

	bool bBoing = false;
	TCHAR logLine[4098];


	for (int count = 0; count < lastEntry2; count++) {

		bBoing = false;

		unsigned int dwPIDMatch = 0;
		for (dwPIDMatch = 0; dwPIDMatch < 4098; dwPIDMatch++) {
			if (Procs[dwPIDMatch].PID == megaStruc2[count].PID) { // this code assumes likelihood of a process being created and assuming a previous PID within 30 seconds is low
				break;
			}
		}

		// noise reduction
		if (dwPIDMatch == 0 || dwPIDMatch == 4098) { // New process not seen before so don't report
			continue;
		}

		// optimization
		if (megaStruc2[count].protection != PAGE_EXECUTE && megaStruc2[count].protection != PAGE_EXECUTE_READ && megaStruc2[count].protection != PAGE_EXECUTE_READWRITE) {
			continue;
		}

		// then fall back methods
		if (count < lastEntry2 && count < lastEntry && megaStruc2[count].PID == megaStruc[count].PID && megaStruc2[count].address == megaStruc[count].address && megaStruc2[count].allocprotection == megaStruc[count].allocprotection && megaStruc2[count].type == megaStruc[count].type && megaStruc2[count].state == megaStruc[count].state && megaStruc2[count].protection == megaStruc[count].protection)
		{

			continue;
		}
		else // then slow way
		{

			for (int count2 = 0; count2 < lastEntry; count2++) {

				if (megaStruc2[count].PID == megaStruc[count2].PID && megaStruc2[count].address == megaStruc[count2].address && megaStruc2[count].allocprotection == megaStruc[count2].allocprotection && megaStruc2[count].type == megaStruc[count2].type && megaStruc2[count].state == megaStruc[count2].state && megaStruc2[count].protection == megaStruc[count2].protection) {
					bBoing = true;
					break; // present 
				}
				else if (megaStruc2[count].PID == megaStruc[count2].PID && megaStruc2[count].address == megaStruc[count2].address && megaStruc2[count].size == megaStruc[count2].size)
				{
					bBoing = true;

					_stprintf_s(logLine, TEXT("Changed,%u,%s,%I64x,%llu,%s,%s\n"), megaStruc2[count].PID, Procs[dwPIDMatch].Name, megaStruc2[count].address, megaStruc2[count].size, Protection(megaStruc2[count].protection), Protection(megaStruc[count2].protection));

					if (bConsole == true) {
						_ftprintf(stdout, logLine);
					}

					WriteEvent(logLine);

					break;
				}
			}

			if (bBoing == false) {
				
				_stprintf_s(logLine, TEXT("New,%u,%s,%I64x,%llu,%s\n"), megaStruc2[count].PID, Procs[dwPIDMatch].Name, megaStruc2[count].address, megaStruc2[count].size, Protection(megaStruc2[count].protection));

				if (bConsole == true) {
					_ftprintf(stdout, logLine);
				}

				WriteEvent(logLine);
			}
		}
	}

	// Copy across
	memcpy(megaStruc, megaStruc2, sizeof(megaStruc));
	lastEntry = lastEntry2;

	// Clean up
	memset(megaStruc2, 0x00, sizeof(megaStruc2));
	lastEntry2 = 0;

	return TRUE;
}

//
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms684139(v=vs.85).aspx
//
BOOL IsWow64()
{
	BOOL bIsWow64 = FALSE;

	if (NULL != fnIsWow64Process)
	{
		if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		{
			return false;
		}
	}
	return bIsWow64;
}

/// <summary>
/// Analyze the process and its memory regions
/// </summary>
/// <param name="dwPID">Process ID</param>
void AnalyzeProc(DWORD dwPID)
{
	DWORD dwRet, dwMods;
	HANDLE hProcess;
	HMODULE hModule[4096];
	TCHAR cProcess[MAX_PATH]; // Process name
	SYSTEM_INFO sysnfoSysNFO;
	BOOL bIsWow64 = FALSE;
	BOOL bIsWow64Other = FALSE;
	DWORD dwRES = 0;



	hProcess = OpenProcess(PROCESS_ALL_ACCESS | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwPID);
	if (hProcess == NULL)
	{
		if (GetLastError() == 5) {
			hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwPID);
			if (hProcess == NULL) {
				//fprintf(stderr, "Failed to OpenProcess(%d),%d\n", dwPID, GetLastError());
				return;
			}
		}
		else {
			//fprintf(stderr, "Failed to OpenProcess(%d),%d\n", dwPID, GetLastError());
			return;
		}
	}


	if (EnumProcessModules(hProcess, hModule, 4096 * sizeof(HMODULE), &dwRet) == 0)
	{
		if (GetLastError() == 299) {
			//fprintf(stderr, "64bit process and we're 32bit - sad panda! skipping PID %d\n", dwPID);
		}
		else {
			//fprintf(stderr, "Error in EnumProcessModules(%d),%d\n", dwPID, GetLastError());
		}
		return;
	}
	dwMods = dwRet / sizeof(HMODULE);

	GetModuleBaseName(hProcess, hModule[0], cProcess, MAX_PATH);
	Procs[NumOfProcs].PID = dwPID;
	_tcscpy_s(Procs[NumOfProcs].Name, MAX_PATH, cProcess);
	NumOfProcs++;

	//PrintProcessInfo(cProcess, dwPID);

	if (IsWow64Process(GetCurrentProcess(), &bIsWow64)) {
		GetNativeSystemInfo(&sysnfoSysNFO);

		if (bIsWow64)
		{
			//fwprintf(stdout,L"[i] Running under WOW64 - Page Size %d\n",sysnfoSysNFO.dwPageSize);
		}
		else
		{
			//fwprintf(stdout,L"[i] Not running under WOW64 - Page Size %d\n",sysnfoSysNFO.dwPageSize);	
		}
	}
	else {
		//fwprintf(stderr, L"Error in IsWow64Process(%d),%d\n", dwPID, GetLastError());
		return;
	}

	//
	// Walk the processes address space
	//
	unsigned char* pString = NULL;

	ULONG_PTR addrCurrent = 0;
	ULONG_PTR lastBase = (-1);

	unsigned int iInserted = 0;

	for (;;)
	{
#ifdef WIN64
		MEMORY_BASIC_INFORMATION64 memMeminfo;
#endif
#ifdef WIN32
		MEMORY_BASIC_INFORMATION memMeminfo;
#endif
		VirtualQueryEx(hProcess, reinterpret_cast<LPVOID>(addrCurrent), reinterpret_cast<PMEMORY_BASIC_INFORMATION>(&memMeminfo), sizeof(memMeminfo));

		if (lastBase == (ULONG_PTR)memMeminfo.BaseAddress) {
			break;
		}

		lastBase = (ULONG_PTR)memMeminfo.BaseAddress;

		//_ftprintf(stdout, TEXT("%u,%s,%p,%lld,%u,%u\n"), dwPID, cProcess, memMeminfo.BaseAddress, memMeminfo.RegionSize, memMeminfo.Protect, memMeminfo.State);

		if (bFirstRun == true && memMeminfo.State == MEM_COMMIT) {
			putinMegaStruc(dwPID, cProcess, (ULONG64)memMeminfo.BaseAddress, memMeminfo.AllocationProtect, memMeminfo.RegionSize, memMeminfo.Protect, memMeminfo.State, memMeminfo.Type);
		}
		else if (memMeminfo.State == MEM_COMMIT)
		{
			putinMegaStrucAlt(dwPID, cProcess, (ULONG64)memMeminfo.BaseAddress, memMeminfo.AllocationProtect, memMeminfo.RegionSize, memMeminfo.Protect, memMeminfo.State, memMeminfo.Type);
		}
		else
		{
			// Skip
		}

		addrCurrent += memMeminfo.RegionSize;
	}

	CloseHandle(hProcess);
}

/// <summary>
/// Enumerate all the processes on the system and
/// pass off to the analysis function
/// </summary>
void EnumerateProcesses()
{
	DWORD dwPIDArray[4096], dwRet, dwPIDS, intCount;
	NumOfProcs = 0;

	// Be clean to ensure no stale data
	memset(Procs, 0x00, sizeof(Procs));

	//
	// Enumerate
	//
	if (EnumProcesses(dwPIDArray, 4096 * sizeof(DWORD), &dwRet) == 0)
	{
		DWORD dwRet = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, strErrMsg, 1023, NULL);
		if (dwRet != 0) {
			_ftprintf(stderr, TEXT("[!] EnumProcesses() failed - %s"), strErrMsg);
		}
		else
		{
			_ftprintf(stderr, TEXT("[!] EnumProcesses() - Error: %d\n"), GetLastError());
		}
		return;
	}

	dwPIDS = dwRet / sizeof(DWORD);

	//
	// Analyze
	//
	for (intCount = 0; intCount < dwPIDS; intCount++)
	{
		AnalyzeProc(dwPIDArray[intCount]);
	}

	// Do the diff calc the totals
	// and log
	if (bFirstRun == false)
	{
		calcTotals();
		diffMegaStrucAlt();
	}

}