#include "pch.h"
#include <corecrt_wstdio.h>
#include "../lz4/include/lz4.h"
#include "../quick/quicklz.h"
#include "CombatShell.h"

#include <stdio.h>
#include <CommCtrl.h>
#include <stddef.h>

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
	g_dataHlper: 保存加壳时候的数据, 需要根据加密的大小来申请
*/
#define DllExport __declspec( dllexport )
extern "C" {
	DllExport Stud g_stud = { 0, };
	DllExport VmNode g_VmNode = { 0, };
	DllExport char g_dataHlper[0x10000] = { 0, };
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
typedef LPTOP_LEVEL_EXCEPTION_FILTER(WINAPI* FnSetUnhandledExceptionFilter)(LPTOP_LEVEL_EXCEPTION_FILTER);
typedef PVOID(WINAPI* FnAddVectoredExceptionHandler)(ULONG, PVECTORED_EXCEPTION_HANDLER);
FnSetUnhandledExceptionFilter MySetUnhandledExceptionFilter = nullptr;
FnAddVectoredExceptionHandler MyAddVectoredExceptionHandler = nullptr;

static void RefreshRuntimeImageBase()
{
	if (MyGetModuleHandleW) {
		HMODULE moduleBase = MyGetModuleHandleW(NULL);
		if (moduleBase) {
#ifdef _WIN64
			m_Dlllpbase = (DWORD64)moduleBase;
#else
			m_Dlllpbase = (DWORD)moduleBase;
#endif
		}
	}
}

#ifndef _WIN64
static void CallOriginalEntry32()
{
	DWORD target = m_Dlllpbase + (DWORD)g_stud.s_dwOepBase;
	typedef void(WINAPI* FnOepEntry)();
	FnOepEntry entry = (FnOepEntry)(ULONG_PTR)target;
	entry();
}
#endif

#ifdef SHELL_DIAGNOSTIC_ENABLED
static char g_shellDiagTrace[0x3000] = { 0 };
static void ShellDiagTrace(const char* stage)
{
	if (stage == nullptr) {
		return;
	}
	static unsigned int traceOffset = 0;
	static unsigned int helperOffset = 0;
	const unsigned int cap = (unsigned int)sizeof(g_shellDiagTrace);
	const unsigned int helperCap = (unsigned int)sizeof(g_dataHlper);
	if (traceOffset >= cap - 2) {
		return;
	}
	__try {
		if (stage[0] == '\0') {
			return;
		}
		const char* p = stage;
		while (*p && traceOffset < cap - 2) {
			char ch = *p++;
			g_shellDiagTrace[traceOffset++] = ch;
			if (helperOffset < helperCap - 2) {
				g_dataHlper[helperOffset++] = ch;
			}
		}
		g_shellDiagTrace[traceOffset++] = '|';
		g_shellDiagTrace[traceOffset] = '\0';
		if (helperOffset < helperCap - 2) {
			g_dataHlper[helperOffset++] = '|';
			g_dataHlper[helperOffset] = '\0';
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return;
	}
}
static void ShellDiagAppendHex64(const char* key, unsigned long long value)
{
	if (key == nullptr) {
		return;
	}
	char buf[96] = { 0 };
	const char* hex = "0123456789ABCDEF";
	int pos = 0;
	__try {
		if (key[0] == '\0') {
			return;
		}
		while (key[pos] && pos < 40) {
			buf[pos] = key[pos];
			++pos;
		}
		if (pos < (int)sizeof(buf) - 3) {
			buf[pos++] = '=';
			buf[pos++] = '0';
			buf[pos++] = 'x';
		}
		for (int i = 15; i >= 0 && pos < (int)sizeof(buf) - 1; --i) {
			buf[pos++] = hex[(value >> (i * 4)) & 0xF];
		}
		buf[pos] = '\0';
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return;
	}
	ShellDiagTrace(buf);
}
#define SHELL_TRACE(stage) ShellDiagTrace(stage)
#define SHELL_TRACE_HEX(key, value) ShellDiagAppendHex64((key), (unsigned long long)(value))
#else
#define SHELL_TRACE(stage) ((void)0)
#define SHELL_TRACE_HEX(key, value) ((void)0)
#endif

static LONG WINAPI ShellTopLevelExceptionFilter(EXCEPTION_POINTERS* info)
{
	SHELL_TRACE("Crash:uef");
	if (info && info->ExceptionRecord) {
		SHELL_TRACE_HEX("Crash:code", (DWORD64)info->ExceptionRecord->ExceptionCode);
		SHELL_TRACE_HEX("Crash:addr", (DWORD64)info->ExceptionRecord->ExceptionAddress);
	}
	if (info && info->ContextRecord) {
#ifdef _WIN64
		SHELL_TRACE_HEX("Crash:rip", (DWORD64)info->ContextRecord->Rip);
		SHELL_TRACE_HEX("Crash:rsp", (DWORD64)info->ContextRecord->Rsp);
#else
		SHELL_TRACE_HEX("Crash:eip", (DWORD64)info->ContextRecord->Eip);
		SHELL_TRACE_HEX("Crash:esp", (DWORD64)info->ContextRecord->Esp);
#endif
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI ShellVectoredExceptionHandler(PEXCEPTION_POINTERS info)
{
	SHELL_TRACE("Crash:veh");
	if (info && info->ExceptionRecord) {
		SHELL_TRACE_HEX("Crash:veh_code", (DWORD64)info->ExceptionRecord->ExceptionCode);
		SHELL_TRACE_HEX("Crash:veh_addr", (DWORD64)info->ExceptionRecord->ExceptionAddress);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

// x32 asm
#ifndef _WIN64
DWORD puGetModule(const DWORD Hash)
{
	DWORD	nDllBase = 0;
	//__asm {
	//	jmp			start;
	//	/*函数1：遍历PEB_LDR_DATA链表HASH加密*/
	//GetModulVA:
	//	push		ebp;
	//	mov			ebp, esp;
	//	sub			esp, 0x24;
	//	push		edx;
	//	push		ebx;
	//	push		edi;
	//	push		esi;
	//	mov			ecx, 8;
	//	mov			eax, 0CCCCCCCCh;
	//	lea			edi, dword ptr[ebp - 0x20];
	//	rep stos	dword ptr es : [edi] ;
	//	xor edx, edx;
	//	add			edx, 0x10;
	//	add			edx, 0x19;
	//	inc			edx;
	//	inc			edx;
	//	inc			edx;
	//	inc			edx;
	//	inc			edx;
	//	inc			edx;
	//	inc			edx;
	//	mov			esi, dword ptr fs : [edx] ; // edx = 0x30
	//	sub			edx, 0x24;
	//	mov			esi, dword ptr[esi + edx]; // edx = 0xC
	//	add			edx, 0x10;
	//	mov			esi, dword ptr[esi + edx];// edx = 0x1c
	//	add			edx, 0x4;
	//tag_Modul:
	//	mov			dword ptr[ebp - 0x8], esi;	// 保存LDR_DATA_LIST_ENTRY
	//	mov			ebx, dword ptr[esi + edx];	// DLL的名称指针(应该指向一个字符串) edx = 0x20;
	//	mov			eax, dword ptr[ebp + 0x8];
	//	push		eax;
	//	push		ebx;						// +0xC
	//	call		HashModulVA;
	//	test		eax, eax;
	//	jnz			_ModulSucess;
	//	mov			esi, dword ptr[ebp - 0x8];
	//	mov			esi, [esi];					// 遍历下一个
	//	LOOP		tag_Modul;
	//_ModulSucess:
	//	mov			esi, dword ptr[ebp - 0x8];
	//	mov			eax, dword ptr[esi + 0x8];
	//	pop			esi;
	//	pop			edi;
	//	pop			ebx;
	//	pop			edx;
	//	mov			esp, ebp;
	//	pop			ebp;
	//	ret;

	//	/*函数2：HASH解密算法（宽字符解密）*/
	//HashModulVA:
	//	push		ebp;
	//	mov			ebp, esp;
	//	sub			esp, 0x04;
	//	mov			dword ptr[ebp - 0x04], 0x00;
	//	push		ebx;
	//	push		ecx;
	//	push		edx;
	//	push		esi;
	//	// 获取字符串开始计算
	//	mov			esi, [ebp + 0x8];
	//	test		esi, esi;
	//	jz			tag_failuers;
	//	xor ecx, ecx;
	//	xor eax, eax;
	//tag_loops:
	//	mov			al, [esi + ecx];		// 获取字节加密
	//	test		al, al;					// 0则退出
	//	jz			tag_ends;
	//	mov			ebx, [ebp - 0x04];
	//	shl			ebx, 0x19;
	//	mov			edx, [ebp - 0x04];
	//	shr         edx, 0x07;
	//	or ebx, edx;
	//	add			ebx, eax;
	//	mov[ebp - 0x4], ebx;
	//	inc			ecx;
	//	inc			ecx;
	//	jmp			tag_loops;
	//tag_ends:
	//	mov			ebx, [ebp + 0x0C];		// 获取HASH
	//	mov			edx, [ebp - 0x04];
	//	xor eax, eax;
	//	cmp			ebx, edx;
	//	jne			tag_failuers;
	//	mov			eax, 1;
	//	jmp			tag_funends;
	//tag_failuers:
	//	mov			eax, 0;
	//tag_funends:
	//	pop			esi;
	//	pop			edx;
	//	pop			ecx;
	//	pop			ebx;
	//	mov			esp, ebp;
	//	pop			ebp;
	//	ret			0x08;

	//start:
	//	/*主模块*/
	//	pushad;
	//	push		Hash;
	//	call		GetModulVA;
	//	add			esp, 0x4;
	//	mov			nDllBase, eax;
	//	popad;
	//}
	return nDllBase;
}
DWORD puGetProcAddress(const DWORD dllvalues, const DWORD Hash)
{
	DWORD FunctionAddress = 0;
	//__asm {
	//	jmp			start;
	//	// 自定义函数计算Hash且对比返回正确的函数
	//GetHashFunVA:
	//	push		ebp;
	//	mov			ebp, esp;
	//	sub			esp, 0x30;
	//	push		edx;
	//	push		ebx;
	//	push		esi;
	//	push		edi;
	//	lea			edi, dword ptr[ebp - 0x30];
	//	mov			ecx, 12;
	//	mov			eax, 0CCCCCCCCh;
	//	rep	stos	dword ptr es : [edi] ;
	//	// 以上开辟栈帧操作（Debug版本模式）
	//	mov			eax, [ebp + 0x8];				// ☆ kernel32.dll(MZ)
	//	mov			dword ptr[ebp - 0x8], eax;
	//	mov			ebx, [ebp + 0x0c];				// ☆ GetProcAddress Hash值
	//	mov			dword ptr[ebp - 0x0c], ebx;
	//	// 获取PE头与RVA及ENT
	//	mov			edi, [eax + 0x3C];				// e_lfanew
	//	lea			edi, [edi + eax];				// e_lfanew + MZ = PE
	//	mov			dword ptr[ebp - 0x10], edi;		// ☆ 保存PE（VA）
	//	// 获取ENT
	//	mov			edi, dword ptr[edi + 0x78];		// 获取导出表RVA
	//	lea			edi, dword ptr[edi + eax];		// 导出表VA
	//	mov[ebp - 0x14], edi;						// ☆ 保存导出表VA
	//	// 获取函数名称数量
	//	mov			ebx, [edi + 0x18];
	//	mov			dword ptr[ebp - 0x18], ebx;		// ☆ 保存函数名称数量
	//	// 获取ENT
	//	mov			ebx, [edi + 0x20];				// 获取ENT(RVA)
	//	lea			ebx, [eax + ebx];				// 获取ENT(VA)
	//	mov			dword ptr[ebp - 0x20], ebx;		// ☆ 保存ENT(VA)
	//	// 遍历ENT 解密哈希值对比字符串
	//	mov			edi, dword ptr[ebp - 0x18];
	//	mov			ecx, edi;
	//	xor esi, esi;
	//	mov			edi, dword ptr[ebp - 0x8];
	//	jmp			_WHILE;
	//	// 外层大循环
	//_WHILE:
	//	mov			edx, dword ptr[ebp + 0x0c];		// HASH
	//	push		edx;
	//	mov			edx, dword ptr[ebx + esi * 4];	// 获取第一个函数名称的RVA
	//	lea			edx, [edi + edx];				// 获取一个函数名称的VA地址
	//	push		edx;							// ENT表中第一个字符串地址
	//	call		_STRCMP;
	//	cmp			eax, 0;
	//	jnz			_SUCESS;
	//	inc			esi;
	//	LOOP		_WHILE;
	//	jmp			_ProgramEnd;
	//	// 对比成功之后获取循环次数（下标）cx保存下标数
	//_SUCESS:
	//	// 获取EOT导出序号表内容
	//	mov			ecx, esi;
	//	mov			ebx, dword ptr[ebp - 0x14];
	//	mov			esi, dword ptr[ebx + 0x24];
	//	mov			ebx, dword ptr[ebp - 0x8];
	//	lea			esi, [esi + ebx];				// 获取EOT的VA
	//	xor edx, edx;
	//	mov			dx, [esi + ecx * 2];			// 注意双字 获取序号
	//	// 获取EAT地址表RVA
	//	mov			esi, dword ptr[ebp - 0x14];		// Export VA
	//	mov			esi, [esi + 0x1C];
	//	mov			ebx, dword ptr[ebp - 0x8];
	//	lea			esi, [esi + ebx];				// 获取EAT的VA			
	//	mov			eax, [esi + edx * 4];			// 返回值eax（GetProcess地址）
	//	lea			eax, [eax + ebx];
	//	jmp			_ProgramEnd;

	//_ProgramEnd:
	//	pop			edi;
	//	pop			esi;
	//	pop			ebx;
	//	pop			edx;
	//	mov			esp, ebp;
	//	pop			ebp;
	//	ret			0x8;

	//	// 循环对比HASH值
	//_STRCMP:
	//	push		ebp;
	//	mov			ebp, esp;
	//	sub			esp, 0x04;
	//	mov			dword ptr[ebp - 0x04], 0x00;
	//	push		ebx;
	//	push		ecx;
	//	push		edx;
	//	push		esi;
	//	// 获取字符串开始计算
	//	mov			esi, [ebp + 0x8];
	//	xor ecx, ecx;
	//	xor eax, eax;

	//tag_loop:
	//	mov			al, [esi + ecx];		// 获取字节加密
	//	test		al, al;					// 0则退出
	//	jz			tag_end;
	//	mov			ebx, [ebp - 0x04];
	//	shl			ebx, 0x19;
	//	mov			edx, [ebp - 0x04];
	//	shr         edx, 0x07;
	//	or ebx, edx;
	//	add			ebx, eax;
	//	mov[ebp - 0x4], ebx;
	//	inc			ecx;
	//	jmp			tag_loop;

	//tag_end:
	//	mov			ebx, [ebp + 0x0C];		// 获取HASH
	//	mov			edx, [ebp - 0x04];
	//	xor eax, eax;
	//	cmp			ebx, edx;
	//	jne			tag_failuer;
	//	mov			eax, 1;
	//	jmp			tag_funend;

	//tag_failuer:
	//	mov			eax, 0;

	//tag_funend:
	//	pop			esi;
	//	pop			edx;
	//	pop			ecx;
	//	pop			ebx;
	//	mov			esp, ebp;
	//	pop			ebp;
	//	ret			0x08;

	//start:
	//	pushad;
	//	push		Hash;						// Hash加密的函数名称
	//	push		dllvalues;					// 模块基址.dll
	//	call		GetHashFunVA;				// GetProcess
	//	mov			FunctionAddress, eax;		// ☆ 保存地址
	//	popad;
	//}
	return FunctionAddress;
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
	SHELL_TRACE("UnCompression:start");
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
		BYTE* Address = (BYTE*)(pSections->VirtualAddress + m_Dlllpbase);
		SHELL_TRACE_HEX("UnCompression:sec_va", pSections->VirtualAddress);
		SHELL_TRACE_HEX("UnCompression:src_rva", SectionAddress);
		SHELL_TRACE_HEX("UnCompression:src_len", g_stud.s_blen[i]);
		SHELL_TRACE_HEX("UnCompression:dst_len", pSections->SizeOfRawData);
		SHELL_TRACE_HEX("UnCompression:src_va", SectionAddress + m_Dlllpbase);

		BOOL dstVp = MyVirtualProtect(Address, g_stud.s_SectionOffsetAndSize[i][0], PAGE_EXECUTE_READWRITE, &Att_old);
		BOOL srcVp = MyVirtualProtect((void*)(SectionAddress + m_Dlllpbase), g_stud.s_blen[i], PAGE_EXECUTE_READWRITE, &Att_olds);
		SHELL_TRACE_HEX("UnCompression:vp_dst", dstVp);
		SHELL_TRACE_HEX("UnCompression:vp_src", srcVp);
		if (!dstVp || !srcVp) {
			SHELL_TRACE("UnCompression:vp_fail");
			return;
		}

		SHELL_TRACE("UnCompression:before_copy");
		int nRet = -1;
#ifdef _WIN64
		if (g_stud.s_SaveExportTabRVA == 1) {
			BYTE* dst = (BYTE*)(pSections->VirtualAddress + m_Dlllpbase);
			BYTE* src = (BYTE*)(SectionAddress + m_Dlllpbase);
			for (DWORD c = 0; c < g_stud.s_blen[i]; ++c) {
				dst[c] = src[c];
			}
			nRet = (int)g_stud.s_blen[i];
		}
		else {
			__try {
				nRet = LZ4_decompress_safe((char*)(SectionAddress + m_Dlllpbase), (char*)(pSections->VirtualAddress + m_Dlllpbase), g_stud.s_blen[i], pSections->SizeOfRawData);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				SHELL_TRACE_HEX("UnCompression:seh", (DWORD64)GetExceptionCode());
				return;
			}
		}
#else
		if (g_stud.s_SaveExportTabRVA == 1) {
			BYTE* dst = (BYTE*)(pSections->VirtualAddress + m_Dlllpbase);
			BYTE* src = (BYTE*)(SectionAddress + m_Dlllpbase);
			for (DWORD c = 0; c < g_stud.s_blen[i]; ++c) {
				dst[c] = src[c];
			}
			nRet = (int)g_stud.s_blen[i];
		}
		else {
			// 缓冲区  RVA+加载基址  缓冲区大小  压缩过去的大小
			__try {
				nRet = LZ4_decompress_safe((char*)(SectionAddress + m_Dlllpbase), (char*)(pSections->VirtualAddress + m_Dlllpbase), g_stud.s_blen[i], pSections->SizeOfRawData);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				SHELL_TRACE_HEX("UnCompression:seh", (DWORD64)GetExceptionCode());
				return;
			}
		}
#endif
		SHELL_TRACE_HEX("UnCompression:ret", (DWORD64)nRet);
		MyVirtualProtect(Address, g_stud.s_SectionOffsetAndSize[i][0], Att_old, &Att_old);
		MyVirtualProtect((void*)(SectionAddress + m_Dlllpbase), g_stud.s_blen[i], Att_olds, &Att_olds);
		++pSections;
		SectionAddress += g_stud.s_blen[i];
	}
	SHELL_TRACE("UnCompression:end");
}

void ApplyBaseRelocAfterUnpack()
{
	SHELL_TRACE("ApplyReloc:start");
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(((PIMAGE_DOS_HEADER)m_Dlllpbase)->e_lfanew + (DWORD64)m_Dlllpbase);
	if (!pNt) {
		SHELL_TRACE("ApplyReloc:no_nt");
		return;
	}
	DWORD relocRva = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
	DWORD relocSize = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
	if ((relocRva == 0 || relocSize == 0) &&
		g_stud.s_DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC][0] != 0 &&
		g_stud.s_DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC][1] != 0) {
		relocRva = (DWORD)g_stud.s_DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC][0];
		relocSize = (DWORD)g_stud.s_DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC][1];
		SHELL_TRACE("ApplyReloc:use_saved_dir");
	}
	SHELL_TRACE_HEX("ApplyReloc:dir_rva", relocRva);
	SHELL_TRACE_HEX("ApplyReloc:dir_size", relocSize);
	if (relocRva == 0 || relocSize == 0) {
		SHELL_TRACE("ApplyReloc:no_dir");
		return;
	}
	ULONGLONG preferredImageBase = (ULONGLONG)g_stud.s_OriginalImageBase;
	if (preferredImageBase == 0) {
		preferredImageBase = (ULONGLONG)pNt->OptionalHeader.ImageBase;
	}
	SHELL_TRACE_HEX("ApplyReloc:preferred_base", preferredImageBase);
	ULONGLONG delta = (ULONGLONG)m_Dlllpbase - preferredImageBase;
	SHELL_TRACE_HEX("ApplyReloc:delta", delta);
	if (delta == 0) {
		SHELL_TRACE("ApplyReloc:delta_zero");
		return;
	}

	PIMAGE_BASE_RELOCATION pRel = (PIMAGE_BASE_RELOCATION)(m_Dlllpbase + relocRva);
	BYTE* relocEnd = (BYTE*)pRel + relocSize;
	DWORD patched = 0;
	DWORD skipType = 0;
	DWORD skipRange = 0;
	DWORD skipProtect = 0;
	while ((BYTE*)pRel < relocEnd && pRel->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION) && pRel->VirtualAddress != 0)
	{
		DWORD count = (pRel->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
		WORD* entries = (WORD*)(pRel + 1);
		for (DWORD i = 0; i < count; ++i)
		{
			WORD type = (entries[i] >> 12) & 0xF;
			WORD off = entries[i] & 0x0FFF;
#ifdef _WIN64
			if (type != IMAGE_REL_BASED_DIR64) {
				++skipType;
				continue;
			}
#else
			if (type != IMAGE_REL_BASED_HIGHLOW) {
				++skipType;
				continue;
			}
#endif
			DWORD targetRva = pRel->VirtualAddress + off;
			if (targetRva >= pNt->OptionalHeader.SizeOfImage) {
				++skipRange;
				continue;
			}
#ifdef _WIN64
			ULONGLONG* pTarget = (ULONGLONG*)(m_Dlllpbase + targetRva);
			const SIZE_T ptrSize = sizeof(ULONGLONG);
			const ULONGLONG relocDelta = delta;
#else
			DWORD* pTarget = (DWORD*)(m_Dlllpbase + targetRva);
			const SIZE_T ptrSize = sizeof(DWORD);
			const DWORD relocDelta = (DWORD)delta;
#endif
			DWORD old = 0;
			if (!MyVirtualProtect(pTarget, ptrSize, PAGE_READWRITE, &old)) {
				++skipProtect;
				continue;
			}
			*pTarget += relocDelta;
			MyVirtualProtect(pTarget, ptrSize, old, &old);
			++patched;
		}
		pRel = (PIMAGE_BASE_RELOCATION)((BYTE*)pRel + pRel->SizeOfBlock);
	}
	SHELL_TRACE_HEX("ApplyReloc:patched", patched);
	SHELL_TRACE_HEX("ApplyReloc:skip_type", skipType);
	SHELL_TRACE_HEX("ApplyReloc:skip_range", skipRange);
	SHELL_TRACE_HEX("ApplyReloc:skip_protect", skipProtect);
	SHELL_TRACE("ApplyReloc:end");
}

static void RestoreRuntimeDataDirectories()
{
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(((PIMAGE_DOS_HEADER)m_Dlllpbase)->e_lfanew + (DWORD64)m_Dlllpbase);
	if (!pNt) {
		SHELL_TRACE("RestoreDirs:no_nt");
		return;
	}
	SHELL_TRACE("RestoreDirs:start");
	DWORD oldProtect = 0;
	const SIZE_T dirBytes = sizeof(pNt->OptionalHeader.DataDirectory);
	if (!MyVirtualProtect(&pNt->OptionalHeader.DataDirectory[0], dirBytes, PAGE_READWRITE, &oldProtect)) {
		SHELL_TRACE("RestoreDirs:vp_fail");
		return;
	}
	for (int i = 0; i < 16; ++i)
	{
		pNt->OptionalHeader.DataDirectory[i].VirtualAddress = (DWORD)g_stud.s_DataDirectory[i][0];
		pNt->OptionalHeader.DataDirectory[i].Size = (DWORD)g_stud.s_DataDirectory[i][1];
	}
	MyVirtualProtect(&pNt->OptionalHeader.DataDirectory[0], dirBytes, oldProtect, &oldProtect);
	SHELL_TRACE_HEX("RestoreDirs:import_rva", pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	SHELL_TRACE_HEX("RestoreDirs:tls_rva", pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
	SHELL_TRACE("RestoreDirs:end");
}

static void RunTlsCallbacksIfPresent()
{
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(((PIMAGE_DOS_HEADER)m_Dlllpbase)->e_lfanew + (DWORD64)m_Dlllpbase);
	if (!pNt) {
		SHELL_TRACE("TLS:no_nt");
		return;
	}
	const DWORD tlsRva = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress;
	const DWORD tlsSize = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size;
	SHELL_TRACE_HEX("TLS:rva", tlsRva);
	SHELL_TRACE_HEX("TLS:size", tlsSize);
	if (tlsRva == 0 || tlsSize < sizeof(IMAGE_TLS_DIRECTORY)) {
		SHELL_TRACE("TLS:none");
		return;
	}
#ifdef _WIN64
	PIMAGE_TLS_DIRECTORY64 pTls = (PIMAGE_TLS_DIRECTORY64)(m_Dlllpbase + tlsRva);
	PIMAGE_TLS_CALLBACK* pCallbacks = (PIMAGE_TLS_CALLBACK*)pTls->AddressOfCallBacks;
#else
	PIMAGE_TLS_DIRECTORY32 pTls = (PIMAGE_TLS_DIRECTORY32)(m_Dlllpbase + tlsRva);
	PIMAGE_TLS_CALLBACK* pCallbacks = (PIMAGE_TLS_CALLBACK*)pTls->AddressOfCallBacks;
#endif
	if (!pCallbacks) {
		SHELL_TRACE("TLS:no_callbacks");
		return;
	}
	SHELL_TRACE_HEX("TLS:callbacks", (DWORD64)pCallbacks);
	DWORD callbackCount = 0;
	while (*pCallbacks && callbackCount < 64)
	{
		PIMAGE_TLS_CALLBACK cb = *pCallbacks;
		SHELL_TRACE_HEX("TLS:cb", (DWORD64)cb);
		cb((PVOID)m_Dlllpbase, DLL_PROCESS_ATTACH, nullptr);
		++callbackCount;
		++pCallbacks;
	}
	SHELL_TRACE_HEX("TLS:count", callbackCount);
}

static bool IsRvaInsideImage(DWORD rva, DWORD sizeOfImage, DWORD needed = 1)
{
	if (rva == 0 || sizeOfImage == 0 || needed == 0) {
		return false;
	}
	if (rva >= sizeOfImage) {
		return false;
	}
	if (needed > sizeOfImage) {
		return false;
	}
	return rva <= (sizeOfImage - needed);
}

static bool IsStringRvaSafe(DWORD rva, DWORD sizeOfImage, DWORD maxLen)
{
	if (!IsRvaInsideImage(rva, sizeOfImage, 1)) {
		return false;
	}
	const char* p = (const char*)(m_Dlllpbase + rva);
	for (DWORD i = 0; i < maxLen && (rva + i) < sizeOfImage; ++i) {
		if (p[i] == '\0') {
			return true;
		}
	}
	return false;
}

static HMODULE SafeLoadLibraryForIat(char* name)
{
	__try {
		return MyLoadLibraryExA(name, NULL, NULL);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		SHELL_TRACE_HEX("RepairTheIAT:loadlib_seh", (DWORD64)GetExceptionCode());
		return NULL;
	}
}

static DWORD64 SafeGetProcAddressForIat(HMODULE module, const char* procName)
{
	__try {
		return (DWORD64)MyGetProcAddress(module, procName);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		SHELL_TRACE_HEX("RepairTheIAT:getproc_seh", (DWORD64)GetExceptionCode());
		return 0;
	}
}

void RepairTheIAT()
{
	SHELL_TRACE("RepairTheIAT:start");
#ifdef _WIN64
	DWORD64 dwMoudle = 0, ImportTabVA = 0, FunAddress = 0;
#else
	DWORD dwMoudle = 0, ImportTabVA = 0, FunAddress = 0;
#endif
	// Win32_4byte_即使_强转_DWORD64也是4byte
	dwMoudle = (DWORD64)MyGetModuleHandleW(NULL);
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(((PIMAGE_DOS_HEADER)dwMoudle)->e_lfanew + dwMoudle);
	if (!pNt) {
		SHELL_TRACE("RepairTheIAT:no_nt");
		return;
	}
	const DWORD sizeOfImage = pNt->OptionalHeader.SizeOfImage;
	const DWORD importRva = (DWORD)g_stud.s_DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT][0];
	const DWORD importSize = (DWORD)g_stud.s_DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT][1];
	if (!IsRvaInsideImage(importRva, sizeOfImage, sizeof(IMAGE_IMPORT_DESCRIPTOR)) || importSize < sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
		SHELL_TRACE("RepairTheIAT:no_import_dir");
		return;
	}
	ImportTabVA = importRva + dwMoudle;
	PIMAGE_IMPORT_DESCRIPTOR pImport = (PIMAGE_IMPORT_DESCRIPTOR)ImportTabVA;
	const DWORD maxImportDesc = importSize / sizeof(IMAGE_IMPORT_DESCRIPTOR);

	DWORD Att_old = 0;
	DWORD iatTraceCount = 0;
	for (DWORD importIndex = 0; importIndex < maxImportDesc; ++importIndex)
	{
		if (pImport->Name == 0 && pImport->FirstThunk == 0 && pImport->OriginalFirstThunk == 0) {
			break;
		}
		if (!IsStringRvaSafe(pImport->Name, sizeOfImage, 260)) {
			SHELL_TRACE("RepairTheIAT:bad_dll_name");
			++pImport;
			continue;
		}
		if (!IsRvaInsideImage(pImport->FirstThunk, sizeOfImage, sizeof(IMAGE_THUNK_DATA))) {
			SHELL_TRACE("RepairTheIAT:bad_ft");
			++pImport;
			continue;
		}

		char* Name = (char*)(pImport->Name + dwMoudle);
		SHELL_TRACE_HEX("RepairTheIAT:dll_name_rva", pImport->Name);
		SHELL_TRACE_HEX("RepairTheIAT:oft_rva", pImport->OriginalFirstThunk);
		SHELL_TRACE_HEX("RepairTheIAT:ft_rva", pImport->FirstThunk);
		SHELL_TRACE_HEX("RepairTheIAT:desc_idx", importIndex);
		HMODULE hModuledll = SafeLoadLibraryForIat(Name);
		if (!hModuledll) {
			SHELL_TRACE("RepairTheIAT:loadlib_fail");
			++pImport;
			continue;
		}
		DWORD64 thunkRva = pImport->OriginalFirstThunk ? pImport->OriginalFirstThunk : pImport->FirstThunk;
		if (!IsRvaInsideImage((DWORD)thunkRva, sizeOfImage, sizeof(IMAGE_THUNK_DATA))) {
			SHELL_TRACE("RepairTheIAT:bad_oft");
			++pImport;
			continue;
		}
		PIMAGE_THUNK_DATA pThunkINT = (PIMAGE_THUNK_DATA)(thunkRva + dwMoudle);
		PIMAGE_THUNK_DATA pThunkIAT = (PIMAGE_THUNK_DATA)(pImport->FirstThunk + dwMoudle);
		for (DWORD thunkIndex = 0; thunkIndex < 8192; ++thunkIndex)
		{
			const DWORD intRva = (DWORD)((DWORD64)pThunkINT - dwMoudle);
			const DWORD iatRva = (DWORD)((DWORD64)pThunkIAT - dwMoudle);
			if (!IsRvaInsideImage(intRva, sizeOfImage, sizeof(IMAGE_THUNK_DATA)) ||
				!IsRvaInsideImage(iatRva, sizeOfImage, sizeof(IMAGE_THUNK_DATA))) {
				SHELL_TRACE("RepairTheIAT:thunk_oob");
				break;
			}

			const ULONGLONG thunkValue = pThunkINT->u1.AddressOfData;
			if (thunkValue == 0) {
				break;
			}

			MyVirtualProtect((PVOID64)pThunkIAT, 0x16, PAGE_READWRITE, &Att_old);
#ifdef _WIN64
			if (!IMAGE_SNAP_BY_ORDINAL64(thunkValue))
			{
				if (!IsRvaInsideImage((DWORD)thunkValue, sizeOfImage, sizeof(WORD) + 2)) {
					SHELL_TRACE("RepairTheIAT:bad_import_name");
					MyVirtualProtect((PVOID64)pThunkIAT, 0x16, Att_old, &Att_old);
					++pThunkINT;
					++pThunkIAT;
					continue;
				}
				if (!IsStringRvaSafe((DWORD)thunkValue + (DWORD)offsetof(IMAGE_IMPORT_BY_NAME, Name), sizeOfImage, 512)) {
					SHELL_TRACE("RepairTheIAT:bad_import_string");
					MyVirtualProtect((PVOID64)pThunkIAT, 0x16, Att_old, &Att_old);
					++pThunkINT;
					++pThunkIAT;
					continue;
				}
				PIMAGE_IMPORT_BY_NAME pName = (PIMAGE_IMPORT_BY_NAME)(thunkValue + dwMoudle);
				FunAddress = SafeGetProcAddressForIat(hModuledll, pName->Name);
			}
			else
			{
				DWORD64 dwFunOrdinal = IMAGE_ORDINAL64(thunkValue);
				FunAddress = SafeGetProcAddressForIat(hModuledll, (char*)(ULONG_PTR)dwFunOrdinal);
			}
#else
			if (!IMAGE_SNAP_BY_ORDINAL32((DWORD)thunkValue))
			{
				if (!IsRvaInsideImage((DWORD)thunkValue, sizeOfImage, sizeof(WORD) + 2)) {
					SHELL_TRACE("RepairTheIAT:bad_import_name");
					MyVirtualProtect((PVOID64)pThunkIAT, 0x16, Att_old, &Att_old);
					++pThunkINT;
					++pThunkIAT;
					continue;
				}
				if (!IsStringRvaSafe((DWORD)thunkValue + (DWORD)offsetof(IMAGE_IMPORT_BY_NAME, Name), sizeOfImage, 512)) {
					SHELL_TRACE("RepairTheIAT:bad_import_string");
					MyVirtualProtect((PVOID64)pThunkIAT, 0x16, Att_old, &Att_old);
					++pThunkINT;
					++pThunkIAT;
					continue;
				}
				PIMAGE_IMPORT_BY_NAME pName = (PIMAGE_IMPORT_BY_NAME)(thunkValue + dwMoudle);
				FunAddress = SafeGetProcAddressForIat(hModuledll, pName->Name);
			}
			else
			{
				DWORD dwFunOrdinal = IMAGE_ORDINAL32((DWORD)thunkValue);
				FunAddress = SafeGetProcAddressForIat(hModuledll, (char*)(ULONG_PTR)dwFunOrdinal);
			}
#endif

			if (!FunAddress) {
				SHELL_TRACE("RepairTheIAT:getproc_fail");
				MyVirtualProtect((PVOID64)pThunkIAT, 0x16, Att_old, &Att_old);
				++pThunkINT;
				++pThunkIAT;
				continue;
			}

#ifdef _WIN64
			pThunkIAT->u1.Function = (ULONGLONG)FunAddress;
#else
			// Keep x86 path consistent with x64: write resolved function pointer directly.
			// The trampoline-based variant is fragile on modern runtimes and caused crashes.
			pThunkIAT->u1.Function = (ULONGLONG)(DWORD)FunAddress;
#endif
			if (iatTraceCount < 12) {
				SHELL_TRACE_HEX("RepairTheIAT:iat_rva", (DWORD64)((DWORD64)pThunkIAT - dwMoudle));
				SHELL_TRACE_HEX("RepairTheIAT:fun", (DWORD64)FunAddress);
				++iatTraceCount;
			}
			MyVirtualProtect((PVOID64)pThunkIAT, 0x16, Att_old, &Att_old);
			++pThunkINT;
			++pThunkIAT;
		}
		SHELL_TRACE_HEX("RepairTheIAT:desc_done", importIndex);
		++pImport;
	}
	SHELL_TRACE("RepairTheIAT:end");
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
					CodeExecEntry(g_stud.s_dwOepBase + m_Dlllpbase);
					MyExitProcess(0);
#else
					CallOriginalEntry32();
					MyExitProcess(0);
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
			// 如果获取壳窗口失败/将不再进行队列等待触发OEP解密，直接解密执行
			UnCompression();
			MySleep(1000);
			RepairTheIAT();
#ifdef  _WIN64
			MySleep(2000);
			CodeExecEntry(g_stud.s_dwOepBase + m_Dlllpbase);
			MyExitProcess(0);
#else
			CallOriginalEntry32();
			MyExitProcess(0);
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

	HWND hWnd = MyCreateWindowExW(WS_EX_CLIENTEDGE, szWindowClass, TEXT("登录输入"), WS_OVERLAPPEDWINDOW,
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
	SHELL_TRACE("CombatShellEntry:start");
#ifdef _WIN64
	puGetModule(0x228C4218, &g_stud.s_Krenel32);
	MyLoadLibraryExA = (FnLoadLibraryExA)puGetProcAddress(g_stud.s_Krenel32, 0xC0D83287);
	g_stud.s_User32 = (DWORD64)MyLoadLibraryExA("user32.dll", NULL, NULL);
#else
	g_stud.s_Krenel32 = puGetModule(0xEC1C6278);
	MyLoadLibraryExA = (FnLoadLibraryExA)puGetProcAddress(g_stud.s_Krenel32, 0xC0D83287);
	g_stud.s_User32 = (DWORD64)MyLoadLibraryExA("user32.dll", NULL, NULL);
#endif
	SHELL_TRACE("CombatShellEntry:module_resolved");
	// VM_Start_start
	// GetLoadlibraryExA
	MyLoadLibraryExA = (FnLoadLibraryExA)puGetProcAddress(g_stud.s_Krenel32, 0xC0D83287);
	// GetExitProcW
	MyExitProcess = (FnExitProcess)puGetProcAddress(g_stud.s_Krenel32, 0x4FD18963);
	// GetGetModuleW
	MyGetModuleHandleW = (FnGetModuleHandleW)puGetProcAddress(g_stud.s_Krenel32, 0xF4E2F2C8);
	RefreshRuntimeImageBase();
	SHELL_TRACE("CombatShellEntry:imagebase_ready");
	SHELL_TRACE_HEX("CombatShellEntry:imagebase", m_Dlllpbase);
	SHELL_TRACE_HEX("CombatShellEntry:oep", g_stud.s_dwOepBase);
	SHELL_TRACE_HEX("CombatShellEntry:import_rva", g_stud.s_DataDirectory[1][0]);
	MyVirtualProtect = (FnVirtualProtect)puGetProcAddress(g_stud.s_Krenel32, 0xEF64A41E);
	// GetMyGetProcessAddress
	MyGetProcAddress = (FnGetProcAddress)puGetProcAddress(g_stud.s_Krenel32, 0xBBAFDF85);
	MySetUnhandledExceptionFilter = (FnSetUnhandledExceptionFilter)MyGetProcAddress((HMODULE)g_stud.s_Krenel32, "SetUnhandledExceptionFilter");
	MyAddVectoredExceptionHandler = (FnAddVectoredExceptionHandler)MyGetProcAddress((HMODULE)g_stud.s_Krenel32, "AddVectoredExceptionHandler");
	if (MySetUnhandledExceptionFilter) {
		MySetUnhandledExceptionFilter(ShellTopLevelExceptionFilter);
		SHELL_TRACE("Crash:uef_set");
	}
	if (MyAddVectoredExceptionHandler) {
		MyAddVectoredExceptionHandler(1, ShellVectoredExceptionHandler);
		SHELL_TRACE("Crash:veh_set");
	}

	// Stable shell path: unpack, repair IAT, then jump to original entry point.
	SHELL_TRACE("CombatShellEntry:before_unpack");
	UnCompression();
	SHELL_TRACE("CombatShellEntry:before_reloc");
	ApplyBaseRelocAfterUnpack();
	SHELL_TRACE("CombatShellEntry:before_restore_dirs");
	RestoreRuntimeDataDirectories();
	SHELL_TRACE("CombatShellEntry:before_iat");
	RepairTheIAT();
	SHELL_TRACE("CombatShellEntry:before_tls");
	RunTlsCallbacksIfPresent();
	SHELL_TRACE("CombatShellEntry:before_oep");
#ifdef _WIN64
	CodeExecEntry(g_stud.s_dwOepBase + m_Dlllpbase);
	MyExitProcess(0);
#else
	CallOriginalEntry32();
	MyExitProcess(0);
#endif
	SHELL_TRACE("CombatShellEntry:return");
}

// VM Module
#ifdef _WIN64

void VmCodetoExecDispath(int handlerid, unsigned char* pOpCode, int codelen, unsigned __int64 vmstarbaseaddr, x86regeditNode* vmcurrentstackstatus)
/*
	@1 ： 指令id
	@2 ： pOpcode已解密
	@3 ： 相对Cuurent_Vmstart偏移offset
	@4 ： imagebase + Vmstartoffset + asmoffset(相对于vmstart)
	@5 ： Vm_CurrentRegeditstatus 保存handler处理后属于自己代码的寄存器状态
*/
{

	switch (handlerid)
	{
	case 1:
		break;
	case 2:
	{
		// ret 销毁栈
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
	case 100:	// call  每次恢复ebp
	{
		// E8 8B020000(offset)
		if (*pOpCode == (unsigned char)'\xE8')
		{
			// @2：offset + 5 + currentaddr = call_addr
			// @4：rax = ret
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
		第一次筛选 opcode: x32 x64不同 地址长度不同
			1个字节汇编指令: nop int 3 ret
			2个字节汇编指令: EB xx
			4个字节：
			不定长字节汇编 : eb e8 e9 ff25 ff15
	*/
	// VMopcode decode to opcode

	// 解密VmCode
	for (int i = 0; i < bytesize; ++i)
	{
		//if (*pOpCode == (unsigned char)('\x00'))
		//	continue;
		*pOpCode ^= Hlerp->xorKey;
		pOpCode++;
	}

	// int count = cs_disasm(Handle, (uint8_t*)pOpCode, 16, (uint64_t)VmStartCodeAddr, 0, &ins);

	// if (count != 1 && !ins)	// 这里必须反汇编是一条，因为只会有一条的opcode
	//	return 0;

	switch (size)	// bytes
	{
	case 1:
	{
		// 提取操作符 和 寄存器
		if (0 == My_stricmp("nop", Hlerp->mnemonic))
		{
			return 1;
		}
		else if (0 == My_stricmp("ret", Hlerp->mnemonic)) // 意味改函数结束
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

	// 获取加密代码段起始地址
	VmNode* pVmNode = nullptr;
	ArrayHlerp* Hlerp = nullptr;
	pVmNode = (VmNode *)Vmcodeaddr;

	// VmCode加密起始地址
	DWORD64 VmcodeStartaddr = pVmNode->VmAddroffset + m_Dlllpbase;
	BYTE* pOpCode = (BYTE *)Mymalloc(16);
	DWORD old_attr = 0;

	// 初始化Current_stack_regedit_status
	x86regeditNode x86regNode = { 0, };

	// 初始化运行代码的栈空间,申请当前堆地址,大小不限,随着执行完毕之后ret销毁。
	// 申请的是栈底  +0x1024栈顶
	PVOID64 stack = MyVirtualAlloc(NULL, 0x100000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	Mymemset(stack, 0, 0x100000);
	if (!stack)
		return 0;

	// 初始化Current_Vmstack
	x86regNode.rsp = (unsigned __int64)stack + 0x100000;
	x86regNode.rbp = (unsigned __int64)stack + 0x100000;

	// Data加壳过程中保存的是偏移offset
	Hlerp = (ArrayHlerp*)(pVmNode->Hlperdataoffset + m_Dlllpbase);

	// Vm执行代码
	for (size_t i = 0; i < pVmNode->Vmencodeasmlen; ++i)
	{
		if (!Hlerp)
			break;

		// 判断是否被VmCode加密
		if (!Hlerp->encodeflag)
		{
			// payload执行

			// 执行下一条汇编指令
			VmcodeStartaddr += Hlerp->bytesize;
			Hlerp++;

			continue;
		}

		// 加载VMCode，判断是什么VM指令
		Mymemset(pOpCode, 0, (sizeof(BYTE) * 16));
		Mymemmove(pOpCode, (void *)VmcodeStartaddr, Hlerp->bytesize);

		// 帮助识别汇编和解密pOpcode代码
		int handler_id = VmOpcodeAnalHlper(Hlerp, pOpCode, Hlerp->bytesize);
		// Handler处理
		if (handler_id)
		{
			VmCodetoExecDispath(handler_id, pOpCode, Hlerp->bytesize, m_Dlllpbase + pVmNode->VmAddroffset + Hlerp->startoffset, &x86regNode);

			// 意味着执行过ret.将不再线性执行
			if (x86regNode.rbp == 0)
				break;
		}
		else
		{
			// payload执行
		}

		// 执行下一条汇编指令
		VmcodeStartaddr += Hlerp->bytesize;
		Hlerp++;
	}

	// 销毁栈
	if (stack)
		Myfree(stack);
	return 1;
}

// Unit Test.
void WINAPI VmEntry()
{
	SHELL_TRACE("VmEntry:start");
	/*
		1. 使用全局变量保存加密地址列表,地址被读取-虚拟机执行.
		2. 正常虚拟机会有一套类似于断点 eip == VmcodeAddr，控制eip转换到虚拟机执行.
		3. 示例是一次性虚拟机,也就是对壳main函数全VMcode加密.
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
	RefreshRuntimeImageBase();
	SHELL_TRACE("VmEntry:imagebase_ready");
	// g_stud.s_User32 = (DWORD64)MyGetModuleHandleW(L"user32.dll");
	g_hInstance = (HINSTANCE)MyGetModuleHandleW(NULL);
	SHELL_TRACE("VmEntry:api_ready");

	// 1. 方案一使用文件保存VmCodeList数据-缺点不灵活,不格外增加壳体积。 开始使用该方案
	// 2. 方案二使用添加新区段保存,稳妥。
	// 3. dll中全局变量保存,方便快捷,从注释程度可以比较与方案一差距。	最终释放方案 √
	// VmNode Vmnode;
	// FILE *fpFile = NULL;
	// int VmCount = 0, offsetaddr = 0, VmasmLen = 0;
	// if ((fpFile = Myfopen("VmCodeList.txt", "rb+")) != NULL)
	{
		// Myfread(&VmCount, sizeof(int), 1, fpFile);
		// 未进行VM加密,执行壳代码
		if (!g_VmNode.VmCount)
		{
			SHELL_TRACE("VmEntry:fallback_CombatShellEntry");
			CombatShellEntry();
			return;
		}
		for (size_t index = 0; index < g_VmNode.VmCount; ++index)
		{
			// Vmnode = { 0, };
			// 文件中记录的是偏移offset + m_Dlllpbase = RVA
			// Myfread(&Vmnode.VmAddroffset, sizeof(DWORD64), 1, fpFile);
			// Myfread(&Vmnode.Vmencodeasmlen, sizeof(int), 1, fpFile);

			if (g_VmNode.Vmencodeasmlen)
			{
				SHELL_TRACE("VmEntry:before_VmStart");
				// 结构体目前 3*4 = 12
				// char* VmStackCode = (char *)Mymalloc(g_VmNode.Vmencodeasmlen * sizeof(ArrayHlerp));
				// Mymemset(VmStackCode, 0, g_VmNode.Vmencodeasmlen * 16);
				// g_VmNode.data = (ArrayHlerp *)VmStackCode;
				// 读取加密List {加密大小 | 加密xor | vmflag}
				// for (int i = 0; i < g_VmNode.Vmencodeasmlen; ++i)
				// {
					// Myfread(&Vmnode.data->xorKey, sizeof(int), 1, fpFile);
					// Myfread(&Vmnode.data->bytesize, sizeof(unsigned short), 1, fpFile);
					// Myfread(&Vmnode.data->encodeflag, sizeof(int), 1, fpFile);
					// Myfread(Vmnode.data->mnemonic, 32, 1, fpFile);
					// Vmnode.data++;
				// }
				// 注意这里要再等回来，否则data是内存最后，因为循环一直++
				// Vmnode.data = (ArrayHlerp *)VmStackCode;
				// 进入虚拟机 -->  执行 --> oep
				VmStart(&g_VmNode);
				SHELL_TRACE("VmEntry:after_VmStart");
				// Myfree(VmStackCode);
				// VmStackCode = NULL;
			}
			// Next 读取下一个加密的密码段-执行
		}
	}
}

#endif
