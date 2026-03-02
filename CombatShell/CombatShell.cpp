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
	DllExport void WINAPI CombatShellEntry();
#ifdef _WIN64
	DllExport void WINAPI VmEntry();
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
#pragma pack(push, 1)
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
#pragma pack(pop)

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
	qlz_state_decompress *state_decompress = (qlz_state_decompress *)MyVirtualAlloc(NULL, sizeof(qlz_state_decompress), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	for (DWORD i = 0; i < g_stud.s_SectionCount - 2; ++i)
	{
		BYTE* Address = (BYTE*)(pSections->VirtualAddress + m_Dlllpbase);

		MyVirtualProtect(Address, g_stud.s_SectionOffsetAndSize[i][0], PAGE_EXECUTE_READWRITE, &Att_old);
		MyVirtualProtect((void*)SectionAddress, g_stud.s_blen[i], PAGE_EXECUTE_READWRITE, &Att_olds);

#ifdef _WIN64
		int nRet = qlz_decompress((char*)(SectionAddress + m_Dlllpbase), (char*)(pSections->VirtualAddress + m_Dlllpbase), state_decompress);
#else
		// 缂撳啿鍖? RVA+鍔犺浇鍩哄潃  缂撳啿鍖哄ぇ灏? 鍘嬬缉杩囧幓鐨勫ぇ灏?
		int nRet = LZ4_decompress_safe((char*)(SectionAddress + m_Dlllpbase), (char*)(pSections->VirtualAddress + m_Dlllpbase), g_stud.s_blen[i], pSections->SizeOfRawData);
#endif
		MyVirtualProtect(Address, g_stud.s_SectionOffsetAndSize[i][0], Att_old, &Att_old);
		MyVirtualProtect((void*)SectionAddress, g_stud.s_blen[i], Att_olds, &Att_olds);
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

#ifdef _WIN64

#else
	// IAT
	BYTE OpCode[] = { 0xe8, 0x01, 0x00, 0x00,
					  0x00, 0xe9, 0x58, 0xeb,
					  0x01, 0xe8, 0xb8, 0x8d,
					  0xe4, 0xd8, 0x62, 0xeb,
					  0x01, 0x15, 0x35, 0x75,
					  0x35, 0x97, 0x13, 0xeb,
					  0x01, 0xff, 0x50, 0xeb,
					  0x02, 0xff, 0x15, 0xc3
	};
#endif
	DWORD Att_old = 0;
	while (pImport->Name)
	{
		char* Name = (char*)(pImport->Name + dwMoudle);
		HMODULE hModuledll = MyLoadLibraryExA(Name, NULL, NULL);
		PIMAGE_THUNK_DATA pThunkINT = (PIMAGE_THUNK_DATA)(pImport->OriginalFirstThunk + dwMoudle);
		PIMAGE_THUNK_DATA pThunkIAT = (PIMAGE_THUNK_DATA)(pImport->FirstThunk + dwMoudle);
		while (pThunkINT->u1.AddressOfData)
		{
			MyVirtualProtect((PVOID64)pThunkIAT, 0x16, PAGE_READWRITE, &Att_old);
			if (!IMAGE_SNAP_BY_ORDINAL(pThunkIAT->u1.Ordinal))
			{
				PIMAGE_IMPORT_BY_NAME pName = (PIMAGE_IMPORT_BY_NAME)(pThunkINT->u1.AddressOfData + dwMoudle);
				FunAddress = (DWORD64)MyGetProcAddress(hModuledll, pName->Name);
			}
			else
			{
				DWORD64 dwFunOrdinal = IMAGE_ORDINAL((pThunkIAT->u1.Ordinal));
				FunAddress = (DWORD64)MyGetProcAddress(hModuledll, (char*)dwFunOrdinal);
			}

#ifdef _WIN64
			pThunkIAT->u1.Function = (ULONGLONG)FunAddress;
#else
			LPVOID AllocMem = NULL;
			FunAddress ^= XORKEY;
			AllocMem = (PDWORD)MyVirtualAlloc(NULL, 0x20, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			//address offset
			OpCode[11] = FunAddress;
			OpCode[12] = FunAddress >> 0x8;
			OpCode[13] = FunAddress >> 0x10;
			OpCode[14] = FunAddress >> 0x18;
			memcpy(AllocMem, OpCode, 0x20);
			pThunkIAT->u1.Function = (ULONGLONG)AllocMem;
#endif
			MyVirtualProtect((PVOID64)pThunkIAT, 0x16, Att_old, &Att_old);
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
					CodeExecEntry(g_stud.s_dwOepBase);
#else
					__asm {
						push esi;
						push eax;
						mov	 esi, g_stud.s_dwOepBase;
						xor	 eax, eax;
						add  eax, 0x200000;
						add	 eax, 0x200000;
						add	 eax, 0x200000;
						sub  eax, 0x200000;
						add  esi, eax;
						jmp	 esi;
						pop eax;
						pop esi;
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
			CodeExecEntry(g_stud.s_dwOepBase);
#else
			__asm {
				push esi;
				push eax;
				mov	 esi, g_stud.s_dwOepBase;
				xor	 eax, eax;
				add  eax, 0x200000;
				add	 eax, 0x200000;
				add	 eax, 0x200000;
				sub  eax, 0x200000
					add  esi, eax;
				jmp	 esi;
				pop eax;
				pop esi;
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
void WINAPI CombatShellEntry()
{
#ifndef _WIN64
	g_stud.s_Krenel32 = puGetModule(0xEC1C6278);
	MyLoadLibraryExA = (FnLoadLibraryExA)puGetProcAddress(g_stud.s_Krenel32, 0xC0D83287);
	g_stud.s_User32 = (DWORD64)MyLoadLibraryExA("user32.dll", NULL, NULL);
#endif
	// VM_Start_start
	// GetLoadlibraryExA
	MyLoadLibraryExA = (FnLoadLibraryExA)puGetProcAddress(g_stud.s_Krenel32, 0xC0D83287);
	// Load GDI32.lib
	//g_stud.s_Gdi32 = (DWORD64)MyLoadLibraryExA("gdi32.dll", NULL, NULL);
	// GetExitProcW
	MyExitProcess = (FnExitProcess)puGetProcAddress(g_stud.s_Krenel32, 0x4FD18963);
	// GetGetModuleW
	MyGetModuleHandleW = (FnGetModuleHandleW)puGetProcAddress(g_stud.s_Krenel32, 0xF4E2F2C8);
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
	UnCompression();
	RepairTheIAT();
	__asm {
		push esi;
		push eax;
		mov	 esi, g_stud.s_dwOepBase;
		xor	 eax, eax;
		add  eax, 0x200000;
		add	 eax, 0x200000;
		add	 eax, 0x200000;
		sub  eax, 0x200000;
		add  esi, eax;
		jmp	 esi;
		pop eax;
		pop esi;
	}
	return;
#endif

	CreateWind();
}

// VM Module
#ifdef _WIN64

void VmCodetoExecDispath(int handlerid, unsigned char* pOpCode, int codelen, unsigned __int64 vmstarbaseaddr, x86regeditNode* vmcurrentstackstatus)
/*
	@1 锛?鎸囦护id
	@2 锛?pOpcode宸茶В瀵?
	@3 锛?鐩稿Cuurent_Vmstart鍋忕Щoffset
	@4 锛?imagebase + Vmstartoffset + asmoffset(鐩稿浜巚mstart)
	@5 锛?Vm_CurrentRegeditstatus 淇濆瓨handler澶勭悊鍚庡睘浜庤嚜宸变唬鐮佺殑瀵勫瓨鍣ㄧ姸鎬?
*/
{

	switch (handlerid)
	{
	case 1:
		break;
	case 2:
	{
		// ret 閿€姣佹爤
		MyVirtualFree((LPVOID)vmcurrentstackstatus->rbp, 0x100000, MEM_RELEASE);
		vmcurrentstackstatus->rbp = 0;
	}
	break;
	case 3:
		break;
	case 20:	// xor
	{
		if (codelen == 3)
		{
			// 45:33C0 xor r8d,r8d
			if (*(pOpCode + 2) == (unsigned char)'\xc0')
			{
				VmXor_r8dHandle(pOpCode, (unsigned __int64)(&vmcurrentstackstatus->r8));
			}
		}
		else
		{
			// 33D2 xor edx,edx 
			if (*(pOpCode + 1) == (unsigned char)'\xd2')
			{
				VmXor_EdxHandle(pOpCode, (unsigned __int64)(&vmcurrentstackstatus->rdx));
			}
			// 33C9 xor ecx,ecx 
			if (*(pOpCode + 1) == (unsigned char)'\xc9')
			{
				VmXor_EcxHandle(pOpCode, (unsigned __int64)(&vmcurrentstackstatus->rcx));
			}
		}
	}
	break;
	case 50:
	{
		// 48:83C4 28 add rsp
		if (*(pOpCode + 2) == (unsigned char)('\xC4'))
		{
			VmAdd_RspHandle(pOpCode, (unsigned __int64)(&vmcurrentstackstatus->rsp));
		}
	}
	break;
	case 51:	// sub
	{
		if (*(pOpCode + 2) == (unsigned char)('\xEC'))
		{
			// sub rsp, 28(byte) 
			VmSub_RSPHandle(pOpCode, (unsigned __int64)(&vmcurrentstackstatus->rsp));
		}
	}
	break;
	case 100:	// call  姣忔鎭㈠ebp
	{
		// E8 8B020000(offset)
		if (*pOpCode == (unsigned char)'\xE8')
		{
			// @2锛歰ffset + 5 + currentaddr = call_addr
			// @4锛歳ax = ret
			VmCallE8_Handle(pOpCode, vmstarbaseaddr + 5, (unsigned long long)(&vmcurrentstackstatus), (unsigned long long)(&vmcurrentstackstatus->rax));
		}
		if ((*pOpCode == (unsigned char)'\xff') && ((*(pOpCode + 1)) == (unsigned char)'\x15'))
		{
			VmCallFF15_Handle(pOpCode, vmstarbaseaddr + 6, (unsigned long long)(&vmcurrentstackstatus), (unsigned long long)(&vmcurrentstackstatus->rax));
		}
		vmcurrentstackstatus->rsp = vmcurrentstackstatus->rbp;
	}
	break;
	case 102:	// lea
	{
		if (*(pOpCode + 2) == (unsigned char)'\x15')
		{
			VmLea_RDXHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rdx));
		}
		else if (*(pOpCode + 2) == (unsigned char)'\x0D')
		{
			// rcx = dll_base + vmstartoffset + imm + 7
			VmLea_RCXHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rcx));
		}
	}
	break;
	case 103:	// mov
	{
		// mov qword ptr ds:[xx], rax 8905
		if (*(pOpCode + 2) == (unsigned char)'\x05')
		{
			VmMov_MemHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rax));
		}
		// B9 18428C22 | mov ecx, 228C4218
		else if (*pOpCode == (unsigned char)'\xB9')
		{
			VmMov_ECXHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rcx));
		}
		// 48:8B0D mov rcx
		else if (*(pOpCode + 2) == (unsigned char)'\x0D')
		{
			VmMov_RCXHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rcx));
		}
		// BA 8732D8C0 | mov edx, C0D83287
		else if (*pOpCode == (unsigned char)'\xBA')
		{
			VmMov_EDXHandle(pOpCode, vmstarbaseaddr, (unsigned long long)(&vmcurrentstackstatus->rdx));
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
		// 鎻愬彇鎿嶄綔绗?鍜?瀵勫瓨鍣?
		if (0 == My_stricmp("nop", Hlerp->mnemonic))
		{
			return 1;
		}
		else if (0 == My_stricmp("ret", Hlerp->mnemonic)) // 鎰忓懗鏀瑰嚱鏁扮粨鏉?
		{
			return 2;
		}
	}
	break;
	case 2:
	case 3:
		if (0 == My_stricmp("xor", Hlerp->mnemonic))
		{
			return 20;
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
	}
	break;
	default:
		break;
	}
	return 0;
}

int VmStart(PVOID64 Vmcodeaddr)
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
	x86regNode.rsp = (unsigned __int64)stack + 0x100000;
	x86regNode.rbp = (unsigned __int64)stack + 0x100000;

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
		Myfree(stack);
	return 1;
}

// Unit Test.
void WINAPI VmEntry()
{
	/*
		1. 浣跨敤鍏ㄥ眬鍙橀噺淇濆瓨鍔犲瘑鍦板潃鍒楄〃,鍦板潃琚鍙?铏氭嫙鏈烘墽琛?
		2. 姝ｅ父铏氭嫙鏈轰細鏈変竴濂楃被浼间簬鏂偣 eip == VmcodeAddr锛屾帶鍒秂ip杞崲鍒拌櫄鎷熸満鎵ц.
		3. 绀轰緥鏄竴娆℃€ц櫄鎷熸満,涔熷氨鏄澹砿ain鍑芥暟鍏╒Mcode鍔犲瘑.
	*/

	puGetModule(0x228C4218, &g_stud.s_Krenel32);
	MyLoadLibraryExA = (FnLoadLibraryExA)puGetProcAddress(g_stud.s_Krenel32, 0xC0D83287);
	// msvcrt.dll
	g_stud.s_msvcr100 = (DWORD64)MyLoadLibraryExA("msvcrt.dll", NULL, NULL);
	g_stud.s_User32 = (DWORD64)MyLoadLibraryExA("user32.dll", NULL, NULL);
	Myfopen = (Fnfopen)puGetProcAddress(g_stud.s_msvcr100, 0xCBC37ECE);
	Myfread = (Fnfread)puGetProcAddress(g_stud.s_msvcr100, 0xC39796C4);
	Myfree = (Fnfree)puGetProcAddress(g_stud.s_msvcr100, 0xCBCB3065);
	Mymalloc = (Fnmalloc)puGetProcAddress(g_stud.s_msvcr100, 0x7FB36681);
	Mymemset = (Fnmemset)puGetProcAddress(g_stud.s_msvcr100, 0x6BCF6ED2);
	Mymemcpy = (Fnmemcpy)puGetProcAddress(g_stud.s_msvcr100, 0x818F6ED7);
	My_stricmp = (FnMy_stricmp)puGetProcAddress(g_stud.s_msvcr100, 0x787ECF9F);
	Mymemmove = (Fnmemmove)puGetProcAddress(g_stud.s_msvcr100, 0xA8FF6F42);
	My_strnicmp = (Fn_strnicmp)puGetProcAddress(g_stud.s_msvcr100, 0x38C92E5F);
	MyVirtualAlloc = (FnVirtualAlloc)puGetProcAddress(g_stud.s_Krenel32, 0x1EDE5967);
	MyVirtualFree = (FnVirtualFree)puGetProcAddress(g_stud.s_Krenel32, 0x6144AA05);
	MyGetModuleHandleW = (FnGetModuleHandleW)puGetProcAddress(g_stud.s_Krenel32, 0xF4E2F2C8);
	// g_stud.s_User32 = (DWORD64)MyGetModuleHandleW(L"user32.dll");
	g_hInstance = (HINSTANCE)MyGetModuleHandleW(NULL);

	// 1. 鏂规涓€浣跨敤鏂囦欢淇濆瓨VmCodeList鏁版嵁-缂虹偣涓嶇伒娲?涓嶆牸澶栧鍔犲３浣撶Н銆?寮€濮嬩娇鐢ㄨ鏂规
	// 2. 鏂规浜屼娇鐢ㄦ坊鍔犳柊鍖烘淇濆瓨,绋冲Ε銆?
	// 3. dll涓叏灞€鍙橀噺淇濆瓨,鏂逛究蹇嵎,浠庢敞閲婄▼搴﹀彲浠ユ瘮杈冧笌鏂规涓€宸窛銆?鏈€缁堥噴鏀炬柟妗?鈭?
	// VmNode Vmnode;
	// FILE *fpFile = NULL;
	// int VmCount = 0, offsetaddr = 0, VmasmLen = 0;
	// if ((fpFile = Myfopen("VmCodeList.txt", "rb+")) != NULL)
	{
		// Myfread(&VmCount, sizeof(int), 1, fpFile);
		// 鏈繘琛孷M鍔犲瘑,鎵ц澹充唬鐮?
		if (!g_VmNode.VmCount)
		{
			CombatShellEntry();
			return;
		}
		for (size_t index = 0; index < g_VmNode.VmCount; ++index)
		{
			// Vmnode = { 0, };
			// 鏂囦欢涓褰曠殑鏄亸绉籵ffset + m_Dlllpbase = RVA
			// Myfread(&Vmnode.VmAddroffset, sizeof(DWORD64), 1, fpFile);
			// Myfread(&Vmnode.Vmencodeasmlen, sizeof(int), 1, fpFile);

			if (g_VmNode.Vmencodeasmlen)
			{
				// 缁撴瀯浣撶洰鍓?3*4 = 12
				// char* VmStackCode = (char *)Mymalloc(g_VmNode.Vmencodeasmlen * sizeof(ArrayHlerp));
				// Mymemset(VmStackCode, 0, g_VmNode.Vmencodeasmlen * 16);
				// g_VmNode.data = (ArrayHlerp *)VmStackCode;
				// 璇诲彇鍔犲瘑List {鍔犲瘑澶у皬 | 鍔犲瘑xor | vmflag}
				// for (int i = 0; i < g_VmNode.Vmencodeasmlen; ++i)
				// {
					// Myfread(&Vmnode.data->xorKey, sizeof(int), 1, fpFile);
					// Myfread(&Vmnode.data->bytesize, sizeof(unsigned short), 1, fpFile);
					// Myfread(&Vmnode.data->encodeflag, sizeof(int), 1, fpFile);
					// Myfread(Vmnode.data->mnemonic, 32, 1, fpFile);
					// Vmnode.data++;
				// }
				// 娉ㄦ剰杩欓噷瑕佸啀绛夊洖鏉ワ紝鍚﹀垯data鏄唴瀛樻渶鍚庯紝鍥犱负寰幆涓€鐩?+
				// Vmnode.data = (ArrayHlerp *)VmStackCode;
				// 杩涘叆铏氭嫙鏈?-->  鎵ц --> oep
				VmStart(&g_VmNode);
				// Myfree(VmStackCode);
				// VmStackCode = NULL;
			}
			// Next 璇诲彇涓嬩竴涓姞瀵嗙殑瀵嗙爜娈?鎵ц
		}
	}
}

#endif
