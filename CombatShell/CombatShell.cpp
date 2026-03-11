#include "pch.h"
#include <corecrt_wstdio.h>
#include "../lz4/include/lz4.h"
#include "../quick/quicklz.h"
#include "CombatShell.h"

#include <stdio.h>
#include <CommCtrl.h>

#pragma comment(linker, "/merge:.data=.text")
#pragma comment(linker, "/merge:.rdata=.text")
#pragma comment(linker, "/section:.text,RWE")

// Windows
HINSTANCE g_hInstance = nullptr;
static TCHAR szWindowClass[] = TEXT("CombatShellWnd");

// IAT Encode Key
#define XORKEY 0x13973575

// DLL_ImageBase
#ifdef _WIN64
DWORD64 m_Dlllpbase = 0x140000000;
#else
DWORD m_Dlllpbase = 0x400000;
#endif

/*
* export gloable struct
	g_dataHlper: 淇濆瓨鍔犲３鏃跺€欑殑鏁版嵁, 闇€瑕佹牴鎹姞瀵嗙殑澶у皬鏉ョ敵璇?
*/
#define DllExport __declspec( dllexport )
extern "C" {
	DllExport Stud g_stud = { 0, };
	DllExport VmNode g_VmNode = { 0, };
	DllExport char g_dataHlper[0x2048] = { 0, };
#ifdef _WIN64
	DWORD64 g_EntryArg1 = 0;
	DWORD64 g_EntryArg2 = 0;
	DWORD64 g_EntryArg3 = 0;
	DWORD64 g_EntryArg4 = 0;
	DWORD64 g_VmOepResult = 0;
	int g_VmActive = 0;
	DllExport void WINAPI CombatShellEntry(void* entryArg1, void* entryArg2, void* entryArg3, void* entryArg4);
	DllExport void WINAPI CombatShellEntry_Vm();
	DllExport void WINAPI VmEntry();
#else
	DllExport void WINAPI CombatShellEntry();
#endif
}

Fn_strnicmp My_strnicmp = nullptr; //38C92E5F
FnMy_stricmp My_stricmp = nullptr;
Fnmemmove Mymemmove = nullptr;
Fnmemcpy Mymemcpy = nullptr;
Fnmemset Mymemset = nullptr;
Fnfree Myfree = nullptr;
Fnfread Myfread = nullptr;
Fnfopen Myfopen = nullptr;
FnGetLastError MyGetLastError = nullptr;
FnPostMessage MyPostMessageW = nullptr;
FnFindWindowW MyFindWindowW = nullptr;
FnGetDlgCtrlID MyGetDlgCtrlID = nullptr;
FnVirtualAlloc MyVirtualAlloc = nullptr;
FnSleep MySleep = nullptr;
FnCreateThread MyCreateThread = nullptr;
FnSendMessageW MySendMessageW = nullptr;
FnFindWindowExW MyFindWindowExW = nullptr;
FnVirtualFree MyVirtualFree = nullptr;
FnVirtualProtect MyVirtualProtect = nullptr;
FnGetDlgItem MyGetDlgItem = nullptr;
FnExitProcess MyExitProcess = nullptr;
FnLoadIconW MyLoadIconW = nullptr;
FnLoadCursorW MyLoadCursorW = nullptr;
FnDefWindowProcW MyDefWindowProcA = nullptr;
FnDefWindowProcW MyDefWindowProcW = nullptr;
FnPostQuitMessage MyPostQuitMessage = nullptr;
FnMessageBoxA MyMessageBoxA = nullptr;
FnlstrcmpW MylstrcmpW = nullptr;
FnGetWindowTextW MyGetWindowTextW = nullptr;
FnDispatchMessageW MyDispatchMessageW = nullptr;
FnTranslateMessage	MyTranslateMessage = nullptr;
FnGetMessageW MyGetMessageW = nullptr;
FnUpdateWindow MyUpdateWindow = nullptr;
FnShowWindow MyShowWindow = nullptr;
FnCreateWindowExW MyCreateWindowExW = nullptr;
Fnmalloc Mymalloc = nullptr;
FnRegisterClassExW MyRegisterClassExW = nullptr;
FnRegisterClassW MyRegisterClassW = nullptr;
FnCreateSolidBrush MyCreateSolidBrush = nullptr;
FnGetModuleHandleW MyGetModuleHandleW = nullptr;
FnLoadLibraryExA MyLoadLibraryExA = nullptr;
FnGetProcAddress MyGetProcAddress = nullptr;

// x32 resolver
#ifndef _WIN64
namespace {
	typedef struct _UNICODE_STRING_X {
		USHORT Length;
		USHORT MaximumLength;
		PWSTR Buffer;
	} UNICODE_STRING_X;

	typedef struct _LDR_DATA_TABLE_ENTRY_X {
		LIST_ENTRY InLoadOrderLinks;
		LIST_ENTRY InMemoryOrderLinks;
		LIST_ENTRY InInitializationOrderLinks;
		PVOID DllBase;
		PVOID EntryPoint;
		ULONG SizeOfImage;
		UNICODE_STRING_X FullDllName;
		UNICODE_STRING_X BaseDllName;
	} LDR_DATA_TABLE_ENTRY_X;

	typedef struct _PEB_LDR_DATA_X {
		ULONG Length;
		BOOLEAN Initialized;
		PVOID SsHandle;
		LIST_ENTRY InLoadOrderModuleList;
	} PEB_LDR_DATA_X;

	typedef struct _PEB_X {
		BYTE Reserved1[0x0C];
		PEB_LDR_DATA_X* Ldr;
	} PEB_X;

	static DWORD RotHashStep(DWORD current, unsigned char ch) {
		return ((current << 25) | (current >> 7)) + ch;
	}

	static DWORD HashUnicodeModuleNameLower(const UNICODE_STRING_X* name) {
		if (!name || !name->Buffer || name->Length == 0) {
			return 0;
		}

		DWORD hash = 0;
		const USHORT count = static_cast<USHORT>(name->Length / sizeof(WCHAR));
		for (USHORT i = 0; i < count; ++i) {
			unsigned char ch = static_cast<unsigned char>(name->Buffer[i] & 0xFF);
			if (ch == 0) {
				break;
			}
			if (ch >= 'A' && ch <= 'Z') {
				ch = static_cast<unsigned char>(ch + ('a' - 'A'));
			}
			hash = RotHashStep(hash, ch);
		}
		return hash;
	}

	static DWORD HashAnsiName(const char* name) {
		if (!name) {
			return 0;
		}
		DWORD hash = 0;
		for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); *p; ++p) {
			hash = RotHashStep(hash, *p);
		}
		return hash;
	}
} // namespace

DWORD puGetModule(const DWORD Hash)
{
	PEB_X* peb = reinterpret_cast<PEB_X*>(__readfsdword(0x30));
	if (!peb || !peb->Ldr) {
		return 0;
	}

	LIST_ENTRY* head = &peb->Ldr->InLoadOrderModuleList;
	for (LIST_ENTRY* node = head->Flink; node && node != head; node = node->Flink) {
		LDR_DATA_TABLE_ENTRY_X* entry =
			CONTAINING_RECORD(node, LDR_DATA_TABLE_ENTRY_X, InLoadOrderLinks);
		if (!entry || !entry->DllBase) {
			continue;
		}
		if (HashUnicodeModuleNameLower(&entry->BaseDllName) == Hash) {
			return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(entry->DllBase));
		}
	}
	return 0;
}

DWORD puGetProcAddress(const DWORD dllvalues, const DWORD Hash)
{
	if (!dllvalues) {
		return 0;
	}

	const BYTE* base = reinterpret_cast<const BYTE*>(dllvalues);
	const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) {
		return 0;
	}

	const IMAGE_NT_HEADERS* nt =
		reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
	if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) {
		return 0;
	}

	const IMAGE_DATA_DIRECTORY& expDirEntry =
		nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	if (expDirEntry.VirtualAddress == 0 || expDirEntry.Size == 0) {
		return 0;
	}

	const IMAGE_EXPORT_DIRECTORY* expDir =
		reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + expDirEntry.VirtualAddress);
	const DWORD* names = reinterpret_cast<const DWORD*>(base + expDir->AddressOfNames);
	const WORD* ords = reinterpret_cast<const WORD*>(base + expDir->AddressOfNameOrdinals);
	const DWORD* funcs = reinterpret_cast<const DWORD*>(base + expDir->AddressOfFunctions);

	for (DWORD i = 0; i < expDir->NumberOfNames; ++i) {
		const char* name = reinterpret_cast<const char*>(base + names[i]);
		if (HashAnsiName(name) != Hash) {
			continue;
		}

		const WORD ordinalIndex = ords[i];
		const DWORD fnRva = funcs[ordinalIndex];
		return static_cast<DWORD>(dllvalues + fnRva);
	}

	return 0;
}
#endif // _WIN32

#ifdef _WIN64
namespace {
	typedef struct _UNICODE_STRING_X64 {
		USHORT Length;
		USHORT MaximumLength;
		PWSTR Buffer;
	} UNICODE_STRING_X64;

	typedef struct _LDR_DATA_TABLE_ENTRY_X64 {
		LIST_ENTRY InLoadOrderLinks;
		LIST_ENTRY InMemoryOrderLinks;
		LIST_ENTRY InInitializationOrderLinks;
		PVOID DllBase;
		PVOID EntryPoint;
		ULONG SizeOfImage;
		UNICODE_STRING_X64 FullDllName;
		UNICODE_STRING_X64 BaseDllName;
	} LDR_DATA_TABLE_ENTRY_X64;

	typedef struct _PEB_LDR_DATA_X64 {
		ULONG Length;
		BOOLEAN Initialized;
		BYTE Reserved1[3];
		PVOID SsHandle;
		LIST_ENTRY InLoadOrderModuleList;
		LIST_ENTRY InMemoryOrderModuleList;
		LIST_ENTRY InInitializationOrderModuleList;
	} PEB_LDR_DATA_X64;

	typedef struct _PEB_X64 {
		BYTE Reserved1[0x18];
		PEB_LDR_DATA_X64* Ldr;
	} PEB_X64;

	static bool EqualsKernel32Lower(const UNICODE_STRING_X64* name) {
		static const char kKernel32[] = "kernel32.dll";
		if (!name || !name->Buffer) {
			return false;
		}
		const USHORT count = (USHORT)(name->Length / sizeof(WCHAR));
		if (count != (USHORT)(sizeof(kKernel32) - 1)) {
			return false;
		}
		for (USHORT i = 0; i < count; ++i) {
			char ch = (char)(name->Buffer[i] & 0xFF);
			if (ch >= 'A' && ch <= 'Z') {
				ch = (char)(ch - 'A' + 'a');
			}
			if (ch != kKernel32[i]) {
				return false;
			}
		}
		return true;
	}

	static bool EqualsNtdllLower(const UNICODE_STRING_X64* name) {
		static const char kNtdll[] = "ntdll.dll";
		if (!name || !name->Buffer) {
			return false;
		}
		const USHORT count = (USHORT)(name->Length / sizeof(WCHAR));
		if (count != (USHORT)(sizeof(kNtdll) - 1)) {
			return false;
		}
		for (USHORT i = 0; i < count; ++i) {
			char ch = (char)(name->Buffer[i] & 0xFF);
			if (ch >= 'A' && ch <= 'Z') {
				ch = (char)(ch - 'A' + 'a');
			}
			if (ch != kNtdll[i]) {
				return false;
			}
		}
		return true;
	}

	static DWORD64 ResolveKernel32ByPebX64() {
		PEB_X64* peb = (PEB_X64*)__readgsqword(0x60);
		if (!peb || !peb->Ldr) {
			return 0;
		}
		DWORD64 ntdllBase = 0;
		LIST_ENTRY* head = &peb->Ldr->InMemoryOrderModuleList;
		for (LIST_ENTRY* node = head->Flink; node && node != head; node = node->Flink) {
			LDR_DATA_TABLE_ENTRY_X64* entry = (LDR_DATA_TABLE_ENTRY_X64*)((BYTE*)node - offsetof(LDR_DATA_TABLE_ENTRY_X64, InMemoryOrderLinks));
			if (EqualsKernel32Lower(&entry->BaseDllName)) {
				return (DWORD64)entry->DllBase;
			}
			if (EqualsNtdllLower(&entry->BaseDllName)) {
				ntdllBase = (DWORD64)entry->DllBase;
			}
		}

		// kernel32.dll not in PEB (packed PE has no imports).
		// Load it via ntdll.dll's LdrLoadDll.
		if (ntdllBase) {
			typedef LONG(NTAPI* FnLdrLoadDll)(PWCHAR, ULONG*, UNICODE_STRING_X64*, PVOID*);
			FnLdrLoadDll pLdrLoadDll = (FnLdrLoadDll)puGetProcAddress(ntdllBase, 0xCC4C8B22);
			if (pLdrLoadDll) {
				WCHAR k32Name[] = { 'k','e','r','n','e','l','3','2','.','d','l','l',0 };
				UNICODE_STRING_X64 us;
				us.Length = sizeof(k32Name) - sizeof(WCHAR);
				us.MaximumLength = sizeof(k32Name);
				us.Buffer = k32Name;
				PVOID hModule = nullptr;
				pLdrLoadDll(nullptr, 0, &us, &hModule);
				return (DWORD64)hModule;
			}
		}

		return 0;
	}
} // namespace
#endif

void SetString(HWND hWnd)
{
	MyPostQuitMessage = (FnPostQuitMessage)puGetProcAddress(g_stud.s_User32, 0xCAA94781);
	MyDefWindowProcW = (FnDefWindowProcW)puGetProcAddress(g_stud.s_User32, 0x22E85CBA);
	MyCreateWindowExW(0L, WC_BUTTON, TEXT("Shell"), WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 98, 300, 200, hWnd, NULL, 0, NULL);
	MyCreateWindowExW(0L, WC_STATIC, TEXT("Account:"), WS_CHILD | WS_VISIBLE, 30, 145, 80, 20, hWnd, NULL, 0, NULL);
	MyCreateWindowExW(0L, WC_STATIC, TEXT("Passwd :"), WS_CHILD | WS_VISIBLE, 30, 175, 80, 20, hWnd, NULL, 0, NULL);
	MyCreateWindowExW(WS_EX_CLIENTEDGE, WC_EDIT, TEXT(""), WS_CHILD | WS_VISIBLE, 120, 145, 160, 20, hWnd, (HMENU)0x1001, 0, NULL);
	MyCreateWindowExW(WS_EX_CLIENTEDGE, WC_EDIT, TEXT(""), WS_CHILD | WS_VISIBLE, 120, 175, 160, 20, hWnd, (HMENU)0x1002, 0, NULL);
	MyCreateWindowExW(0L, WC_BUTTON, TEXT("login:"), WS_CHILD | WS_VISIBLE, 120, 220, 70, 25, hWnd, (HMENU)0x1003, 0, NULL);
}

static BOOL IsApiSetLikeName(const char* name)
{
	if (!name) {
		return FALSE;
	}
	const char apiPrefix[] = "api-";
	const char extPrefix[] = "ext-";
	for (int i = 0; apiPrefix[i]; ++i) {
		char c = name[i];
		if (c >= 'A' && c <= 'Z') {
			c = (char)(c - 'A' + 'a');
		}
		if (c != apiPrefix[i]) {
			goto check_ext;
		}
	}
	return TRUE;

check_ext:
	for (int i = 0; extPrefix[i]; ++i) {
		char c = name[i];
		if (c >= 'A' && c <= 'Z') {
			c = (char)(c - 'A' + 'a');
		}
		if (c != extPrefix[i]) {
			return FALSE;
		}
	}
	return TRUE;
}

void UnCompression()
{
	MyVirtualAlloc = (FnVirtualAlloc)puGetProcAddress(g_stud.s_Krenel32, 0x1EDE5967);
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(((PIMAGE_DOS_HEADER)m_Dlllpbase)->e_lfanew + (DWORD64)m_Dlllpbase);
	PIMAGE_DATA_DIRECTORY pDataDirectory = (PIMAGE_DATA_DIRECTORY)pNt->OptionalHeader.DataDirectory;
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	DWORD Att_old = 0;

	for (DWORD i = 0; i < 16; ++i)
	{
		MyVirtualProtect(pDataDirectory, 0x8, PAGE_READWRITE, &Att_old);
		if (0 != g_stud.s_DataDirectory[i][0])
			pDataDirectory->VirtualAddress = g_stud.s_DataDirectory[i][0];
		if (0 != g_stud.s_DataDirectory[i][1])
			pDataDirectory->Size = g_stud.s_DataDirectory[i][1];
		MyVirtualProtect(pDataDirectory, 0x8, Att_old, &Att_old);
		++pDataDirectory;
	}
	for (DWORD i = 0; i < g_stud.s_SectionCount - 2; ++i)
	{
		MyVirtualProtect(pSection, 0x8, PAGE_READWRITE, &Att_old);
		if (0 != g_stud.s_SectionOffsetAndSize[i][0])
			pSection->SizeOfRawData = g_stud.s_SectionOffsetAndSize[i][0];
		if (0 != g_stud.s_SectionOffsetAndSize[i][1])
			pSection->PointerToRawData = g_stud.s_SectionOffsetAndSize[i][1];
		MyVirtualProtect(pSection, 0x8, Att_old, &Att_old);
		++pSection;
	}
	PIMAGE_SECTION_HEADER pSections = IMAGE_FIRST_SECTION(pNt);

	DWORD Att_olds = 0;
	DWORD64 SectionAddress = g_stud.s_CompressionSectionRva;
	for (DWORD i = 0; i < g_stud.s_SectionCount - 2; ++i)
	{
		if ((g_stud.s_blen[i] == 0) || (pSections->SizeOfRawData == 0)) {
			++pSections;
			SectionAddress += g_stud.s_blen[i];
			continue;
		}

		BYTE* Address = (BYTE*)(pSections->VirtualAddress + m_Dlllpbase);
		BYTE* CompressAddress = (BYTE*)(SectionAddress + m_Dlllpbase);

		MyVirtualProtect(Address, g_stud.s_SectionOffsetAndSize[i][0], PAGE_EXECUTE_READWRITE, &Att_old);
		MyVirtualProtect(CompressAddress, g_stud.s_blen[i], PAGE_EXECUTE_READWRITE, &Att_olds);

#ifdef _WIN64
		qlz_state_decompress* state_decompress = (qlz_state_decompress*)MyVirtualAlloc(
			NULL,
			sizeof(qlz_state_decompress),
			MEM_RESERVE | MEM_COMMIT,
			PAGE_READWRITE);
		if (!state_decompress) {
			MyVirtualProtect(Address, g_stud.s_SectionOffsetAndSize[i][0], Att_old, &Att_old);
			MyVirtualProtect(CompressAddress, g_stud.s_blen[i], Att_olds, &Att_olds);
			return;
		}
		int nRet = (int)qlz_decompress(
			(char*)CompressAddress,
			(char*)(pSections->VirtualAddress + m_Dlllpbase),
			state_decompress);
#else
		// 缂撳啿鍖? RVA+鍔犺浇鍩哄潃  缂撳啿鍖哄ぇ灏? 鍘嬬缉杩囧幓鐨勫ぇ灏?
		int nRet = LZ4_decompress_safe((char*)CompressAddress, (char*)(pSections->VirtualAddress + m_Dlllpbase), g_stud.s_blen[i], pSections->SizeOfRawData);
#endif
		if (nRet <= 0) {
			MyVirtualProtect(Address, g_stud.s_SectionOffsetAndSize[i][0], Att_old, &Att_old);
			MyVirtualProtect(CompressAddress, g_stud.s_blen[i], Att_olds, &Att_olds);
			return;
		}
		MyVirtualProtect(Address, g_stud.s_SectionOffsetAndSize[i][0], Att_old, &Att_old);
		MyVirtualProtect(CompressAddress, g_stud.s_blen[i], Att_olds, &Att_olds);
		++pSections;
		SectionAddress += g_stud.s_blen[i];
	}
}

void RepairTheIAT()
{
#ifdef _WIN64
	DWORD64 dwMoudle = 0, ImportTabVA = 0, FunAddress = 0;
#else
	DWORD dwMoudle = 0, ImportTabVA = 0, FunAddress = 0;
#endif
	// Win32_4byte_鍗充娇_寮鸿浆_DWORD64涔熸槸4byte
	dwMoudle = (DWORD64)MyGetModuleHandleW(NULL);
	ImportTabVA = g_stud.s_DataDirectory[1][0] + dwMoudle;
	PIMAGE_IMPORT_DESCRIPTOR pImport = (PIMAGE_IMPORT_DESCRIPTOR)ImportTabVA;
	DWORD Att_old = 0;
	while (pImport->Name)
	{
		char* Name = (char*)(pImport->Name + dwMoudle);
		HMODULE hModuledll = MyLoadLibraryExA(Name, NULL, NULL);
		if (!hModuledll && IsApiSetLikeName(Name)) {
			hModuledll = (HMODULE)g_stud.s_Krenel32;
		}
		if (!hModuledll) {
			++pImport;
			continue;
		}
		DWORD thunkRva = pImport->OriginalFirstThunk ? pImport->OriginalFirstThunk : pImport->FirstThunk;
		PIMAGE_THUNK_DATA pThunkINT = (PIMAGE_THUNK_DATA)(thunkRva + dwMoudle);
		PIMAGE_THUNK_DATA pThunkIAT = (PIMAGE_THUNK_DATA)(pImport->FirstThunk + dwMoudle);
		while (pThunkINT->u1.AddressOfData)
		{
			MyVirtualProtect((PVOID64)pThunkIAT, sizeof(ULONG_PTR), PAGE_READWRITE, &Att_old);
			if (!IMAGE_SNAP_BY_ORDINAL(pThunkINT->u1.Ordinal))
			{
				PIMAGE_IMPORT_BY_NAME pName = (PIMAGE_IMPORT_BY_NAME)(pThunkINT->u1.AddressOfData + dwMoudle);
				FunAddress = (DWORD64)MyGetProcAddress(hModuledll, pName->Name);
				if (!FunAddress && IsApiSetLikeName(Name)) {
					FunAddress = (DWORD64)MyGetProcAddress((HMODULE)g_stud.s_Krenel32, pName->Name);
				}
			}
			else
			{
				DWORD64 dwFunOrdinal = IMAGE_ORDINAL((pThunkINT->u1.Ordinal));
				FunAddress = (DWORD64)MyGetProcAddress(hModuledll, (char*)dwFunOrdinal);
			}

			pThunkIAT->u1.Function = (ULONG_PTR)FunAddress;
			MyVirtualProtect((PVOID64)pThunkIAT, sizeof(ULONG_PTR), Att_old, &Att_old);
			++pThunkINT;
			++pThunkIAT;
		}
		++pImport;
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		SetString(hWnd);
	}
	break;
	case WM_COMMAND:
	{
		if (0x1003 == LOWORD(wParam)) {
			WCHAR User[20] = { 0 };
			WCHAR Pass[20] = { 0 };
			MyGetWindowTextW(MyGetDlgItem(hWnd, 0x1001), User, 20);
			MyGetWindowTextW(MyGetDlgItem(hWnd, 0x1002), Pass, 20);
			if ((0 == MylstrcmpW(User, L"admin") && (0 == MylstrcmpW(Pass, L"admin"))))
			{
				int i = 10;
				int b = 20;
				int c = 30;
				int q = 10;
				int w = 20;
				int e = 30;
				int r = 10;
				int bt = 20;
				int cy = 30;
				int iu = 10;
				int bi = 20;
				int co = 30;
			}
			else
			{
				if (1 == LOWORD(lParam))
				{
					UnCompression();
					RepairTheIAT();
#ifdef _WIN64
					CodeExecEntry(g_stud.s_dwOepBase + m_Dlllpbase, g_EntryArg1, g_EntryArg2, g_EntryArg3, g_EntryArg4);
#else
					__asm {
						mov	 esi, g_stud.s_dwOepBase;
						add	 esi, m_Dlllpbase;
						jmp	 esi;
					}
#endif
				}
			}
		}
	}
	break;
	case WM_DESTROY:
	{
		MyPostQuitMessage(0);
	}
	break;
	}
#ifdef _WIN64
	return 0;
#else
	return MyDefWindowProcW(hWnd, uMsg, wParam, lParam);
#endif
}

DWORD ProcessCallBack(LPVOID lpThreadParameter)
{

	MySleep = (FnSleep)puGetProcAddress(g_stud.s_Krenel32, 0xCB9765A0);
	MySendMessageW = (FnSendMessageW)puGetProcAddress(g_stud.s_User32, 0xDB9DF473);
	MyGetDlgCtrlID = (FnGetDlgCtrlID)puGetProcAddress(g_stud.s_User32, 0xA3E1DC76);
	MyFindWindowW = (FnFindWindowW)puGetProcAddress(g_stud.s_User32, 0x3DB19618);
	MyFindWindowExW = (FnFindWindowExW)puGetProcAddress(g_stud.s_User32, 0x4818F71E);
	MyPostMessageW = (FnPostMessage)puGetProcAddress(g_stud.s_User32, 0x386047E);

	HWND hCalc = nullptr, hbutton = nullptr;
	HWND* hWnd = (HWND*)lpThreadParameter;
	static int i = 10;
	MySleep(10000);
	hCalc = MyFindWindowW(L"PasswdWind", NULL);
	if (hCalc)
	{
		hbutton = MyFindWindowExW(hCalc, 0, L"Button", L"login:");
	}

	while (true)
	{
		MySleep(1000);
		if (hCalc && hbutton)
		{
			MyPostMessageW(hCalc, WM_COMMAND, MAKEWPARAM(MyGetDlgCtrlID(hbutton), BN_CLICKED), i--);
		}
		else
		{
			MySleep(1000);
			// 濡傛灉鑾峰彇澹崇獥鍙ｅけ璐?灏嗕笉鍐嶈繘琛岄槦鍒楃瓑寰呰Е鍙慜EP瑙ｅ瘑锛岀洿鎺ヨВ瀵嗘墽琛?
			UnCompression();
			MySleep(1000);
			RepairTheIAT();
#ifdef  _WIN64
			MySleep(2000);
			CodeExecEntry(g_stud.s_dwOepBase + m_Dlllpbase, g_EntryArg1, g_EntryArg2, g_EntryArg3, g_EntryArg4);
#else
			__asm {
				mov	 esi, g_stud.s_dwOepBase;
				add	 esi, m_Dlllpbase;
				jmp	 esi;
			}
#endif
			break;
		}
		if (i == 0)
			break;
	}
	return 0;
}

int CreateWind()
{
	MyCreateThread = (FnCreateThread)puGetProcAddress(g_stud.s_Krenel32, 0x2729F8BB);
	MyGetLastError = (FnGetLastError)puGetProcAddress(g_stud.s_Krenel32, 0x12F461BB);
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = g_hInstance;
	wcex.hIcon = MyLoadIconW(g_hInstance, IDI_APPLICATION);
	wcex.hCursor = MyLoadCursorW(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = MyLoadIconW(wcex.hInstance, IDI_APPLICATION);

	if (!MyRegisterClassExW(&wcex))
	{
		DWORD nError = MyGetLastError();
		MyMessageBoxA(NULL, "Resgiter Windows Error", "warning", MB_OK | MB_ICONERROR);
		MyExitProcess(0);
	}

	HWND hWnd = MyCreateWindowExW(WS_EX_CLIENTEDGE, szWindowClass, TEXT("鐧诲綍杈撳叆"), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 357, 500, NULL, NULL, g_hInstance, NULL);
	DWORD nError = MyGetLastError();
	MyShowWindow(hWnd, SW_HIDE);
	MyUpdateWindow(hWnd);

	HANDLE handle = MyCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ProcessCallBack, (LPVOID)&hWnd, 0, NULL);

	MSG msg = { 0 };
	while (MyGetMessageW(&msg, NULL, 0, 0))
	{
		MyTranslateMessage(&msg);
		MyDispatchMessageW(&msg);
	}
	return 0;
}

// ShellCode Main
#ifdef _WIN64
extern "C" DWORD64 WINAPI CombatShellEntryImpl(void* entryArg1, void* entryArg2, void* entryArg3, void* entryArg4)
#else
static DWORD WINAPI CombatShellEntryImpl()
#endif
{
#ifdef _WIN64
	if (!g_VmActive) {
		g_EntryArg1 = (DWORD64)entryArg1;
		g_EntryArg2 = (DWORD64)entryArg2;
		g_EntryArg3 = (DWORD64)entryArg3;
		g_EntryArg4 = (DWORD64)entryArg4;
	}
	g_stud.s_Krenel32 = ResolveKernel32ByPebX64();
	if (!g_stud.s_Krenel32)
		return 0;
#else
	g_stud.s_Krenel32 = puGetModule(0xEC1C6278);
	if (!g_stud.s_Krenel32)
		return 0;
	MyLoadLibraryExA = (FnLoadLibraryExA)puGetProcAddress(g_stud.s_Krenel32, 0xC0D83287);
	g_stud.s_User32 = (DWORD64)MyLoadLibraryExA("user32.dll", NULL, NULL);
#endif
	// VM_Start_start
	// GetLoadlibraryExA
	MyLoadLibraryExA = (FnLoadLibraryExA)puGetProcAddress(g_stud.s_Krenel32, 0xC0D83287);
	if (!MyLoadLibraryExA)
#ifdef _WIN64
		return 0;
#else
		return 0;
#endif
#ifdef _WIN64
	g_stud.s_User32 = (DWORD64)MyLoadLibraryExA("user32.dll", NULL, NULL);
	if (!g_stud.s_User32)
		return 0;
#endif
	// Load GDI32.lib
	//g_stud.s_Gdi32 = (DWORD64)MyLoadLibraryExA("gdi32.dll", NULL, NULL);
	// GetExitProcW
	MyExitProcess = (FnExitProcess)puGetProcAddress(g_stud.s_Krenel32, 0x4FD18963);
	// GetGetModuleW
	MyGetModuleHandleW = (FnGetModuleHandleW)puGetProcAddress(g_stud.s_Krenel32, 0xF4E2F2C8);
	if (MyGetModuleHandleW)
	{
		m_Dlllpbase = (DWORD64)MyGetModuleHandleW(NULL);
	}
#ifndef _WIN64
	else
	{
		return 0;
	}
#endif
	// GetCreateSolidBrush
	//MyCreateSolidBrush = (FnCreateSolidBrush)puGetProcAddress(g_stud.s_Gdi32, 0xBB7420F9);
	// GetUpdateData
	MyUpdateWindow = (FnUpdateWindow)puGetProcAddress(g_stud.s_User32, 0x9BB5D8DC);
	// GetGetMessageW
	MyGetMessageW = (FnGetMessageW)puGetProcAddress(g_stud.s_User32, 0x61060461);
	// GetTranslateMessage
	MyTranslateMessage = (FnTranslateMessage)puGetProcAddress(g_stud.s_User32, 0xE09980A2);
	// GetDispatchMessageW
	MyDispatchMessageW = (FnDispatchMessageW)puGetProcAddress(g_stud.s_User32, 0x7A1506D8);
	// GetShowWindow
	MyShowWindow = (FnShowWindow)puGetProcAddress(g_stud.s_User32, 0xDD8B5FB8);
	// GetLoadCursorW
	MyLoadCursorW = (FnLoadCursorW)puGetProcAddress(g_stud.s_User32, 0xC6B20165);
	// GetLoadIconW
	MyLoadIconW = (FnLoadIconW)puGetProcAddress(g_stud.s_User32, 0x7636E8F4);
	// GetRegisterClassExW
	MyRegisterClassW = (FnRegisterClassW)puGetProcAddress(g_stud.s_User32, 0xBC05E48);
	MyRegisterClassExW = (FnRegisterClassExW)puGetProcAddress(g_stud.s_User32, 0x68D82F59);
	MyMessageBoxA = (FnMessageBoxA)puGetProcAddress(g_stud.s_User32, 0x1E380A6A);
	// GetCreateWindowExW
	MyCreateWindowExW = (FnCreateWindowExW)puGetProcAddress(g_stud.s_User32, 0x1FDAF571);
	// GetGetWindowTextW
	MyGetWindowTextW = (FnGetWindowTextW)puGetProcAddress(g_stud.s_User32, 0x457BF55A);
	// GetlstrcmpW
	MylstrcmpW = (FnlstrcmpW)puGetProcAddress(g_stud.s_Krenel32, 0x7EAD1F86);
	// GetDefWindowProcW
	MyDefWindowProcW = (FnDefWindowProcW)puGetProcAddress(g_stud.s_User32, 0x22E85CBA);
	// GetGetDlgItem
	MyGetDlgItem = (FnGetDlgItem)puGetProcAddress(g_stud.s_User32, 0x5D0CB479);
	// Mymemcpy = (Fnmemcpy)puGetProcAddress(g_stud.s_MSVCRT, 0x818F6ED7);
	MyVirtualProtect = (FnVirtualProtect)puGetProcAddress(g_stud.s_Krenel32, 0xEF64A41E);
	// GetMyGetProcessAddress
	MyGetProcAddress = (FnGetProcAddress)puGetProcAddress(g_stud.s_Krenel32, 0xBBAFDF85);

#ifndef _WIN64
	// x86 direct path: avoid unstable UI flow, run shell restoration and jump back to OEP.
	__try {
		UnCompression();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		MyExitProcess(0xE1);
		return 0;
	}
	__try {
		RepairTheIAT();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		MyExitProcess(0xE2);
		return 0;
	}
	return g_stud.s_dwOepBase + m_Dlllpbase;
#else
	// x64 direct path: restore sections/IAT and transfer control to original entry point.
	UnCompression();
	RepairTheIAT();
	return g_stud.s_dwOepBase + m_Dlllpbase;
#endif

	CreateWind();
#ifndef _WIN64
	return 0;
#endif
}

#ifndef _WIN64
extern "C" __declspec(dllexport) __declspec(naked) void WINAPI CombatShellEntry()
{
	__asm {
		call CombatShellEntryImpl
		mov	 esi, eax
		jmp	 esi
	}
}
#endif

// VM Module
#ifdef _WIN64


// Helper: read a 64-bit register from the VM register file by index (0=rax..15=r15)
static unsigned __int64 VmReadReg(x86regeditNode* r, int idx)
{
	switch (idx) {
	case 0:  return r->rax; case 1:  return r->rcx; case 2:  return r->rdx; case 3:  return r->rbx;
	case 4:  return r->rsp; case 5:  return r->rbp; case 6:  return r->rsi; case 7:  return r->rdi;
	case 8:  return r->r8;  case 9:  return r->r9;  case 10: return r->r10; case 11: return r->r11;
	case 12: return r->r12; case 13: return r->r13; case 14: return r->r14; case 15: return r->r15;
	default: return 0;
	}
}

// Helper: write a 64-bit register in the VM register file by index
static void VmWriteReg(x86regeditNode* r, int idx, unsigned __int64 val)
{
	switch (idx) {
	case 0:  r->rax = val; break; case 1:  r->rcx = val; break;
	case 2:  r->rdx = val; break; case 3:  r->rbx = val; break;
	case 4:  r->rsp = val; break; case 5:  r->rbp = val; break;
	case 6:  r->rsi = val; break; case 7:  r->rdi = val; break;
	case 8:  r->r8  = val; break; case 9:  r->r9  = val; break;
	case 10: r->r10 = val; break; case 11: r->r11 = val; break;
	case 12: r->r12 = val; break; case 13: r->r13 = val; break;
	case 14: r->r14 = val; break; case 15: r->r15 = val; break;
	}
}

// Helper: decode x64 register index from REX + ModRM.reg field
static int VmDecodeReg(unsigned char rex, unsigned char modrm)
{
	int reg = (modrm >> 3) & 7;
	if (rex & 0x04) reg |= 8;	// REX.R
	return reg;
}

// Helper: decode x64 register index from REX + ModRM.rm field
static int VmDecodeRm(unsigned char rex, unsigned char modrm)
{
	int rm = modrm & 7;
	if (rex & 0x01) rm |= 8;	// REX.B
	return rm;
}

// Helper: update ZF and SF in rflags based on a 64-bit result
static void VmUpdateFlags(x86regeditNode* r, unsigned __int64 result)
{
	if (result == 0)
		r->rflags |= (1ULL << 6);	// set ZF
	else
		r->rflags &= ~(1ULL << 6);	// clear ZF
	if (result & (1ULL << 63))
		r->rflags |= (1ULL << 7);	// set SF
	else
		r->rflags &= ~(1ULL << 7);	// clear SF
}

/*
	VM instruction dispatcher
	Handler IDs:
	  1   = nop
	  2   = ret (free VM stack, set rbp=0)
	  20  = xor reg, reg
	  50  = add rsp, imm8
	  51  = sub rsp, imm8
	  100 = call (E8 rel32 / FF 15 [rip+disp32])
	  101 = jmp (FF E0 = jmp rax / EB rel8 / E9 rel32)
	  102 = lea reg, [rip+disp32]
	  103 = mov (multiple variants)
	  105 = push reg
	  106 = test reg, reg
	  107 = jz / je
	  108 = jnz / jne
	  109 = pop reg
	  110 = cmp
*/
void VmCodetoExecDispath(int handlerid, unsigned char* pOpCode, int codelen, unsigned __int64 vmstarbaseaddr, x86regeditNode* vmcurrentstackstatus)
{
	unsigned char rex = 0;
	unsigned char modrm = 0;

	switch (handlerid)
	{
	case 1:		// nop
		break;

	case 2:		// ret
	{
		MyVirtualFree((LPVOID)vmcurrentstackstatus->rbp, 0x100000, MEM_RELEASE);
		vmcurrentstackstatus->rbp = 0;
	}
	break;

	case 20:	// xor reg, reg (register zeroing)
	{
		int prefix_len = 0;
		rex = 0;
		if (codelen >= 3 && (*pOpCode & 0xF0) == 0x40) {
			rex = *pOpCode;
			prefix_len = 1;
		}
		modrm = *(pOpCode + prefix_len + 1);
		int dst = VmDecodeRm(rex, modrm);
		VmWriteReg(vmcurrentstackstatus, dst, 0);
		VmUpdateFlags(vmcurrentstackstatus, 0);
	}
	break;

	case 50:	// add rsp, imm8
	{
		if (*(pOpCode + 2) == (unsigned char)'\xC4') {
			VmAdd_RspHandle(pOpCode, (unsigned __int64)(&vmcurrentstackstatus->rsp));
		}
	}
	break;

	case 51:	// sub rsp, imm8
	{
		if (*(pOpCode + 2) == (unsigned char)'\xEC') {
			VmSub_RSPHandle(pOpCode, (unsigned __int64)(&vmcurrentstackstatus->rsp));
		}
	}
	break;

	case 100:	// call
	{
		if (*pOpCode == (unsigned char)'\xE8') {
			VmCallE8_Handle(pOpCode, vmstarbaseaddr + 5,
				(unsigned long long)(&vmcurrentstackstatus),
				(unsigned long long)(&vmcurrentstackstatus->rax));
		}
		else if (*pOpCode == (unsigned char)'\xFF' && *(pOpCode + 1) == (unsigned char)'\x15') {
			VmCallFF15_Handle(pOpCode, vmstarbaseaddr + 6,
				(unsigned long long)(&vmcurrentstackstatus),
				(unsigned long long)(&vmcurrentstackstatus->rax));
		}
	}
	break;

	case 101:	// jmp
	{
		if (*pOpCode == (unsigned char)'\xFF' && *(pOpCode + 1) == (unsigned char)'\xE0') {
			vmcurrentstackstatus->rbp = 0;	// jmp rax: VM exit
		}
		else if (*pOpCode == (unsigned char)'\xEB') {
			signed char rel = (signed char)*(pOpCode + 1);
			vmcurrentstackstatus->vm_ip_byte_delta = 2 + (signed __int64)rel;
		}
		else if (*pOpCode == (unsigned char)'\xE9') {
			signed int rel = *(signed int*)(pOpCode + 1);
			vmcurrentstackstatus->vm_ip_byte_delta = 5 + (signed __int64)rel;
		}
	}
	break;

	case 102:	// lea reg, [rip+disp32]
	{
		if (*(pOpCode + 2) == (unsigned char)'\x15') {
			VmLea_RDXHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rdx));
		}
		else if (*(pOpCode + 2) == (unsigned char)'\x0D') {
			VmLea_RCXHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rcx));
		}
	}
	break;
	case 103:	// mov (multiple variants)
	{
		// mov [rip+disp32], rax: 48 89 05 xx xx xx xx
		if (*(pOpCode + 2) == (unsigned char)'\x05') {
			VmMov_MemHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rax));
		}
		// mov ecx, imm32: B9 xx xx xx xx
		else if (*pOpCode == (unsigned char)'\xB9') {
			VmMov_ECXHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rcx));
		}
		// mov rcx, [rip+disp32]: 48 8B 0D xx xx xx xx
		else if (*(pOpCode + 2) == (unsigned char)'\x0D') {
			VmMov_RCXHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rcx));
		}
		// mov edx, imm32: BA xx xx xx xx
		else if (*pOpCode == (unsigned char)'\xBA') {
			VmMov_EDXHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rdx));
		}
		// mov reg, [rsp+disp]: (48|4C) 8B xx 24 dd
		else if (*(pOpCode + 1) == (unsigned char)'\x8B' && codelen >= 4 && *(pOpCode + 3) == (unsigned char)'\x24') {
			rex = *pOpCode;
			modrm = *(pOpCode + 2);
			int reg = VmDecodeReg(rex, modrm);
			unsigned __int64 disp = 0;
			if ((modrm & 0xC0) == 0x40) disp = *(pOpCode + 4);
			else if ((modrm & 0xC0) == 0x80) disp = *(unsigned int*)(pOpCode + 4);
			unsigned __int64 val = *(unsigned __int64*)(vmcurrentstackstatus->rsp + disp);
			VmWriteReg(vmcurrentstackstatus, reg, val);
		}
		// mov [rsp+disp], reg: (48|4C) 89 xx 24 dd
		else if (*(pOpCode + 1) == (unsigned char)'\x89' && codelen >= 4 && *(pOpCode + 3) == (unsigned char)'\x24') {
			rex = *pOpCode;
			modrm = *(pOpCode + 2);
			int reg = VmDecodeReg(rex, modrm);
			unsigned __int64 disp = 0;
			if ((modrm & 0xC0) == 0x40) disp = *(pOpCode + 4);
			else if ((modrm & 0xC0) == 0x80) disp = *(unsigned int*)(pOpCode + 4);
			unsigned __int64 val = VmReadReg(vmcurrentstackstatus, reg);
			*(unsigned __int64*)(vmcurrentstackstatus->rsp + disp) = val;
		}
		// mov reg, reg: (4x) 89/8B C0-FF (mod=11)
		else if ((*(pOpCode + 1) == (unsigned char)'\x89' || *(pOpCode + 1) == (unsigned char)'\x8B')
			&& codelen == 3 && (*(pOpCode + 2) & 0xC0) == 0xC0) {
			rex = *pOpCode;
			modrm = *(pOpCode + 2);
			if (*(pOpCode + 1) == (unsigned char)'\x89') {
				int src = VmDecodeReg(rex, modrm);
				int dst = VmDecodeRm(rex, modrm);
				VmWriteReg(vmcurrentstackstatus, dst, VmReadReg(vmcurrentstackstatus, src));
			} else {
				int dst = VmDecodeReg(rex, modrm);
				int src = VmDecodeRm(rex, modrm);
				VmWriteReg(vmcurrentstackstatus, dst, VmReadReg(vmcurrentstackstatus, src));
			}
		}
	}
	break;

	case 105:	// push reg
	{
		unsigned __int64 value = 0;
		if (codelen == 1) {
			int reg = (*pOpCode) - 0x50;
			if (reg >= 0 && reg <= 7)
				value = VmReadReg(vmcurrentstackstatus, reg);
		}
		else if (codelen == 2 && (*pOpCode & 0xF0) == 0x40) {
			int reg = (*(pOpCode + 1)) - 0x50;
			if (*pOpCode & 0x01) reg |= 8;
			value = VmReadReg(vmcurrentstackstatus, reg);
		}
		vmcurrentstackstatus->rsp -= 8;
		*(unsigned __int64*)(vmcurrentstackstatus->rsp) = value;
	}
	break;

	case 106:	// test reg, reg
	{
		rex = 0;
		int prefix_len = 0;
		if ((*pOpCode & 0xF0) == 0x40) { rex = *pOpCode; prefix_len = 1; }
		if (*(pOpCode + prefix_len) == (unsigned char)'\x85') {
			modrm = *(pOpCode + prefix_len + 1);
			int reg1 = VmDecodeReg(rex, modrm);
			int reg2 = VmDecodeRm(rex, modrm);
			unsigned __int64 result = VmReadReg(vmcurrentstackstatus, reg1) & VmReadReg(vmcurrentstackstatus, reg2);
			VmUpdateFlags(vmcurrentstackstatus, result);
		}
	}
	break;

	case 107:	// jz / je
	{
		if (vmcurrentstackstatus->rflags & (1ULL << 6)) {
			if (*pOpCode == (unsigned char)'\x74') {
				signed char rel = (signed char)*(pOpCode + 1);
				vmcurrentstackstatus->vm_ip_byte_delta = 2 + (signed __int64)rel;
			}
			else if (*pOpCode == (unsigned char)'\x0F' && *(pOpCode + 1) == (unsigned char)'\x84') {
				signed int rel = *(signed int*)(pOpCode + 2);
				vmcurrentstackstatus->vm_ip_byte_delta = 6 + (signed __int64)rel;
			}
		}
	}
	break;

	case 108:	// jnz / jne
	{
		if (!(vmcurrentstackstatus->rflags & (1ULL << 6))) {
			if (*pOpCode == (unsigned char)'\x75') {
				signed char rel = (signed char)*(pOpCode + 1);
				vmcurrentstackstatus->vm_ip_byte_delta = 2 + (signed __int64)rel;
			}
			else if (*pOpCode == (unsigned char)'\x0F' && *(pOpCode + 1) == (unsigned char)'\x85') {
				signed int rel = *(signed int*)(pOpCode + 2);
				vmcurrentstackstatus->vm_ip_byte_delta = 6 + (signed __int64)rel;
			}
		}
	}
	break;

	case 109:	// pop reg
	{
		unsigned __int64 value = *(unsigned __int64*)(vmcurrentstackstatus->rsp);
		vmcurrentstackstatus->rsp += 8;
		if (codelen == 1) {
			int reg = (*pOpCode) - 0x58;
			if (reg >= 0 && reg <= 7)
				VmWriteReg(vmcurrentstackstatus, reg, value);
		}
		else if (codelen == 2 && (*pOpCode & 0xF0) == 0x40) {
			int reg = (*(pOpCode + 1)) - 0x58;
			if (*pOpCode & 0x01) reg |= 8;
			VmWriteReg(vmcurrentstackstatus, reg, value);
		}
	}
	break;

	case 110:	// cmp
	{
		rex = 0;
		int prefix_len = 0;
		if ((*pOpCode & 0xF0) == 0x40) { rex = *pOpCode; prefix_len = 1; }
		unsigned char op = *(pOpCode + prefix_len);
		if (op == 0x39) {
			modrm = *(pOpCode + prefix_len + 1);
			int reg = VmDecodeReg(rex, modrm);
			int rm = VmDecodeRm(rex, modrm);
			unsigned __int64 a = VmReadReg(vmcurrentstackstatus, rm);
			unsigned __int64 b = VmReadReg(vmcurrentstackstatus, reg);
			VmUpdateFlags(vmcurrentstackstatus, a - b);
			if (a < b) vmcurrentstackstatus->rflags |= 1; else vmcurrentstackstatus->rflags &= ~1ULL;
		}
		else if (op == 0x3B) {
			modrm = *(pOpCode + prefix_len + 1);
			int reg = VmDecodeReg(rex, modrm);
			int rm = VmDecodeRm(rex, modrm);
			unsigned __int64 a = VmReadReg(vmcurrentstackstatus, reg);
			unsigned __int64 b = VmReadReg(vmcurrentstackstatus, rm);
			VmUpdateFlags(vmcurrentstackstatus, a - b);
			if (a < b) vmcurrentstackstatus->rflags |= 1; else vmcurrentstackstatus->rflags &= ~1ULL;
		}
		else if (op == 0x83) {
			modrm = *(pOpCode + prefix_len + 1);
			int rm = VmDecodeRm(rex, modrm);
			unsigned __int64 a = VmReadReg(vmcurrentstackstatus, rm);
			signed char imm = (signed char)*(pOpCode + prefix_len + 2);
			unsigned __int64 b = (unsigned __int64)(signed __int64)imm;
			VmUpdateFlags(vmcurrentstackstatus, a - b);
			if (a < b) vmcurrentstackstatus->rflags |= 1; else vmcurrentstackstatus->rflags &= ~1ULL;
		}
		else if (op == 0x3D) {
			unsigned __int64 a = vmcurrentstackstatus->rax;
			signed int imm = *(signed int*)(pOpCode + prefix_len + 1);
			unsigned __int64 b = (unsigned __int64)(signed __int64)imm;
			VmUpdateFlags(vmcurrentstackstatus, a - b);
			if (a < b) vmcurrentstackstatus->rflags |= 1; else vmcurrentstackstatus->rflags &= ~1ULL;
		}
	}
	break;
	default:
		break;
	}
}


int  VmOpcodeAnalHlper(PVOID64 Vmcodeaddr, unsigned char* pOpCode, unsigned int size) 
{
	ArrayHlerp* Hlerp = (ArrayHlerp*)Vmcodeaddr;
	if (Hlerp == nullptr || (!Hlerp))
		return 0;

	int XorKey = Hlerp->xorKey;
	int bytesize = Hlerp->bytesize;
	/*
		绗竴娆＄瓫閫?opcode: x32 x64涓嶅悓 鍦板潃闀垮害涓嶅悓
			1涓瓧鑺傛眹缂栨寚浠? nop int 3 ret
			2涓瓧鑺傛眹缂栨寚浠? EB xx
			4涓瓧鑺傦細
			涓嶅畾闀垮瓧鑺傛眹缂?: eb e8 e9 ff25 ff15
	*/
	// VMopcode decode to opcode

	// 瑙ｅ瘑VmCode
	for (int i = 0; i < bytesize; ++i)
	{
		//if (*pOpCode == (unsigned char)('\x00'))
		//	continue;
		*pOpCode ^= Hlerp->xorKey;
		pOpCode++;
	}

	// int count = cs_disasm(Handle, (uint8_t*)pOpCode, 16, (uint64_t)VmStartCodeAddr, 0, &ins);

	// if (count != 1 && !ins)	// 杩欓噷蹇呴』鍙嶆眹缂栨槸涓€鏉★紝鍥犱负鍙細鏈変竴鏉＄殑opcode
	//	return 0;

	switch (size)	// bytes
	{
	case 1:
	{
		if (0 == My_stricmp("nop", Hlerp->mnemonic))
		{
			return 1;
		}
		else if (0 == My_stricmp("ret", Hlerp->mnemonic))
		{
			return 2;
		}
		else if (0 == My_stricmp("push", Hlerp->mnemonic))
		{
			return 105;
		}
		else if (0 == My_stricmp("pop", Hlerp->mnemonic))
		{
			return 109;
		}
	}
	break;
	case 2:
	{
		if (0 == My_stricmp("xor", Hlerp->mnemonic))
		{
			return 20;
		}
		else if (0 == My_stricmp("push", Hlerp->mnemonic))
		{
			return 105;
		}
		else if (0 == My_stricmp("pop", Hlerp->mnemonic))
		{
			return 109;
		}
		else if (0 == My_stricmp("je", Hlerp->mnemonic))
		{
			return 107;
		}
		else if (0 == My_stricmp("jne", Hlerp->mnemonic))
		{
			return 108;
		}
		else if (0 == My_stricmp("jmp", Hlerp->mnemonic))
		{
			return 101;
		}
	}
	break;
	case 3:
	{
		if (0 == My_stricmp("xor", Hlerp->mnemonic))
		{
			return 20;
		}
		else if (0 == My_stricmp("test", Hlerp->mnemonic))
		{
			return 106;
		}
		else if (0 == My_stricmp("cmp", Hlerp->mnemonic))
		{
			return 110;
		}
	}
	break;
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	{
		if (0 == My_stricmp("add", Hlerp->mnemonic))
		{
			return 50;
		}
		if (0 == My_stricmp("sub", Hlerp->mnemonic))
		{
			return 51;
		}
		if (0 == My_stricmp("call", Hlerp->mnemonic))
		{
			return 100;
		}
		else if (0 == My_stricmp("jmp", Hlerp->mnemonic))
		{
			return 101;
		}
		else if (0 == My_stricmp("lea", Hlerp->mnemonic))
		{
			return 102;
		}
		else if (0 == My_stricmp("mov", Hlerp->mnemonic))
		{
			return 103;
		}
		else if (0 == My_stricmp("je", Hlerp->mnemonic))
		{
			return 107;
		}
		else if (0 == My_stricmp("jne", Hlerp->mnemonic))
		{
			return 108;
		}
		else if (0 == My_stricmp("cmp", Hlerp->mnemonic))
		{
			return 110;
		}
		else if (0 == My_stricmp("test", Hlerp->mnemonic))
		{
			return 106;
		}
		else if (0 == My_stricmp("xor", Hlerp->mnemonic))
		{
			return 20;
		}
	}
	break;
	default:
		break;
	}
	return 0;
}

DWORD64 VmStart(PVOID64 Vmcodeaddr)
{
	if (!Vmcodeaddr)
		return 0;

	// 鑾峰彇鍔犲瘑浠ｇ爜娈佃捣濮嬪湴鍧€
	VmNode* pVmNode = nullptr;
	ArrayHlerp* Hlerp = nullptr;
	pVmNode = (VmNode *)Vmcodeaddr;

	// VmCode鍔犲瘑璧峰鍦板潃
	DWORD64 VmcodeStartaddr = pVmNode->VmAddroffset + m_Dlllpbase;
	BYTE* pOpCode = (BYTE *)Mymalloc(16);
	DWORD old_attr = 0;

	// 鍒濆鍖朇urrent_stack_regedit_status
	x86regeditNode x86regNode = { 0, };

	// 鍒濆鍖栬繍琛屼唬鐮佺殑鏍堢┖闂?鐢宠褰撳墠鍫嗗湴鍧€,澶у皬涓嶉檺,闅忕潃鎵ц瀹屾瘯涔嬪悗ret閿€姣併€?
	// 鐢宠鐨勬槸鏍堝簳  +0x1024鏍堥《
	PVOID64 stack = MyVirtualAlloc(NULL, 0x100000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	Mymemset(stack, 0, 0x100000);
	if (!stack)
		return 0;

	// 鍒濆鍖朇urrent_Vmstack
	x86regNode.rsp = (unsigned __int64)stack + 0x100000 - 8;
	x86regNode.rbp = (unsigned __int64)stack + 0x100000;

	// Initialize VM registers with original entry arguments
	x86regNode.rcx = g_EntryArg1;
	x86regNode.rdx = g_EntryArg2;
	x86regNode.r8  = g_EntryArg3;
	x86regNode.r9  = g_EntryArg4;

	// Data鍔犲３杩囩▼涓繚瀛樼殑鏄亸绉籵ffset
	Hlerp = (ArrayHlerp*)(pVmNode->Hlperdataoffset + m_Dlllpbase);

	// Vm鎵ц浠ｇ爜
	for (size_t i = 0; i < pVmNode->Vmencodeasmlen; ++i)
	{
		if (!Hlerp)
			break;

		// 鍒ゆ柇鏄惁琚玍mCode鍔犲瘑
		if (!Hlerp->encodeflag)
		{
			// payload鎵ц

			// 鎵ц涓嬩竴鏉℃眹缂栨寚浠?
			VmcodeStartaddr += Hlerp->bytesize;
			Hlerp++;

			continue;
		}

		// 鍔犺浇VMCode锛屽垽鏂槸浠€涔圴M鎸囦护
		Mymemset(pOpCode, 0, (sizeof(BYTE) * 16));
		Mymemmove(pOpCode, (void *)VmcodeStartaddr, Hlerp->bytesize);

		// 甯姪璇嗗埆姹囩紪鍜岃В瀵唒Opcode浠ｇ爜
		int handler_id = VmOpcodeAnalHlper(Hlerp, pOpCode, Hlerp->bytesize);
		// Handler澶勭悊
		if (handler_id)
		{
			VmCodetoExecDispath(handler_id, pOpCode, Hlerp->bytesize, m_Dlllpbase + pVmNode->VmAddroffset + Hlerp->startoffset, &x86regNode);

			// 鎰忓懗鐫€鎵ц杩噐et.灏嗕笉鍐嶇嚎鎬ф墽琛?
			if (x86regNode.rbp == 0)
				break;

			// Jump resolution: search for target instruction by startoffset
			if (x86regNode.vm_ip_byte_delta != 0)
			{
				unsigned int target_offset = (unsigned int)((signed __int64)Hlerp->startoffset + x86regNode.vm_ip_byte_delta);
				x86regNode.vm_ip_byte_delta = 0;
				ArrayHlerp* baseHlerp = (ArrayHlerp*)(pVmNode->Hlperdataoffset + m_Dlllpbase);
				for (size_t j = 0; j < pVmNode->Vmencodeasmlen; ++j)
				{
					if (baseHlerp[j].startoffset == target_offset)
					{
						i = j - 1;		// for-loop will ++i
						Hlerp = &baseHlerp[j];
						VmcodeStartaddr = m_Dlllpbase + pVmNode->VmAddroffset + target_offset;
						break;
					}
				}
				continue;
			}
		}
		else
		{
			// payload鎵ц
		}

		// 鎵ц涓嬩竴鏉℃眹缂栨寚浠?
		VmcodeStartaddr += Hlerp->bytesize;
		Hlerp++;
	}

	// 閿€姣佹爤
	if (stack)
		MyVirtualFree((LPVOID)stack, 0, MEM_RELEASE);
	return x86regNode.rax;
}

// VM entry point implementation — called from asm VmEntry stub.
// Resolves runtime APIs needed by VmStart, then runs the VM loop.
extern "C" DWORD64 WINAPI VmEntryImpl()
{
	// Resolve kernel32 via PEB walk
	g_stud.s_Krenel32 = ResolveKernel32ByPebX64();
	if (!g_stud.s_Krenel32)
		return 0;

	// Get LoadLibraryExA
	MyLoadLibraryExA = (FnLoadLibraryExA)puGetProcAddress(g_stud.s_Krenel32, 0xC0D83287);
	if (!MyLoadLibraryExA)
		return 0;

	// Load msvcrt.dll for VM runtime dependencies
	g_stud.s_msvcr100 = (DWORD64)MyLoadLibraryExA("msvcrt.dll", NULL, NULL);
	if (!g_stud.s_msvcr100)
		return 0;

	// Load user32.dll (needed by CombatShellEntryImpl via VM call)
	g_stud.s_User32 = (DWORD64)MyLoadLibraryExA("user32.dll", NULL, NULL);

	// Resolve VM runtime dependencies from msvcrt
	Myfree = (Fnfree)puGetProcAddress(g_stud.s_msvcr100, 0xCBCB3065);
	Mymalloc = (Fnmalloc)puGetProcAddress(g_stud.s_msvcr100, 0x7FB36681);
	Mymemset = (Fnmemset)puGetProcAddress(g_stud.s_msvcr100, 0x6BCF6ED2);
	My_stricmp = (FnMy_stricmp)puGetProcAddress(g_stud.s_msvcr100, 0x787ECF9F);
	Mymemmove = (Fnmemmove)puGetProcAddress(g_stud.s_msvcr100, 0xA8FF6F42);

	// Resolve VirtualAlloc/VirtualFree from kernel32
	MyVirtualAlloc = (FnVirtualAlloc)puGetProcAddress(g_stud.s_Krenel32, 0x1EDE5967);
	MyVirtualFree = (FnVirtualFree)puGetProcAddress(g_stud.s_Krenel32, 0x6144AA05);

	// Set DLL base
	MyGetModuleHandleW = (FnGetModuleHandleW)puGetProcAddress(g_stud.s_Krenel32, 0xF4E2F2C8);
	if (MyGetModuleHandleW)
		m_Dlllpbase = (DWORD64)MyGetModuleHandleW(NULL);

	// Set VM active flag — protects g_EntryArg1-4 in CombatShellEntryImpl
	g_VmActive = 1;

	// Run VM: executes encrypted CombatShellEntry_Vm instructions
	DWORD64 oep = VmStart(&g_VmNode);

	// Store and return OEP
	g_VmOepResult = oep;
	return oep;
}

#endif
