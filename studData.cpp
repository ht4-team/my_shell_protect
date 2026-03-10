#include "stdafx.h"
#include "studData.h"
#include "CombatShell/CombatShell.h"
#include "puPEinfoData.h"
#include "CompressionData.h"
#include <io.h>

#define NEWSECITONNAME ".VMP"

extern _Stud*	g_stu;
extern _VmNode* g_Vm;
extern char*	g_dataHlpers;
extern char		g_CombatShellDataLocalFile[MAX_PATH];

studData::studData()
{
}

studData::~studData()
{
} 

BOOL studData::InitStuData(const DWORD dwOldOEP) {
	try
	{
		if (!SinglePuPEInfo::instance()->puOpenFileLoadEx(m_MasterFilePath))
			return false;

		m_lpBase = SinglePuPEInfo::instance()->puGetImageBase();
#ifdef _WIN64
		m_dwNewSectionAddress64 = (DWORD64)SinglePuPEInfo::instance()->puGetSectionAddress((char*)m_lpBase, (BYTE*)NEWSECITONNAME);
#else
		m_dwNewSectionAddress = (DWORD)SinglePuPEInfo::instance()->puGetSectionAddress((char*)m_lpBase, (BYTE*)NEWSECITONNAME);
#endif
		m_OldOEP = dwOldOEP;
		FILE* fpFile = nullptr;
		if ((fpFile = fopen(g_CombatShellDataLocalFile, "ab+")) == NULL) {

			AfxMessageBox(L"文件打开失败");
			return false;
		}
		fwrite(&m_OldOEP, sizeof(DWORD), 1, fpFile);
		fclose(fpFile);
		g_stu->s_dwOepBase = m_OldOEP;
	}
	catch (const std::exception&)
	{
		return false;
	}
	return true;
}

// Stud热身
BOOL studData::LoadLibraryStud()
{
	std::string sDriectory = "";
	std::string sCombatShellPath = "";
	CodeTool::CGetCurrentDirectory(sDriectory);
	if (!sDriectory.empty()) {
		sCombatShellPath = (sDriectory + "CombatShell.dll").c_str();
	}
	if (sCombatShellPath.empty())
		sCombatShellPath = "CombatShell.dll";
	std::wstring wsCombatShellPath = CodeTool::string2wstring(sCombatShellPath.c_str()).c_str();
	if (_access(sCombatShellPath.c_str(), 0) != 0) {
		AfxMessageBox((L"CombatShell文件缺失. " + wsCombatShellPath).c_str());
		return 0;
	}
#ifdef _WIN64
	m_studBase = LoadLibraryEx(wsCombatShellPath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
#else
	m_studBase = LoadLibraryEx(wsCombatShellPath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
#endif
	if (!m_studBase || (nullptr == m_studBase)) {
		AfxMessageBox((L"CombatShell LoadLibraryEx Error. " + wsCombatShellPath).c_str());
		return false;
	}
	_Stud* studSrc = g_stu;
	_VmNode* vmSrc = g_Vm;
	char* helperSrc = g_dataHlpers;

	_Stud* studDst = (_Stud*)GetProcAddress((HMODULE)m_studBase, "g_stud");
	_VmNode* vmDst = (VmNode*)GetProcAddress((HMODULE)m_studBase, "g_VmNode");
	char* helperDst = (char*)GetProcAddress((HMODULE)m_studBase, "g_dataHlper");
	if (studSrc && studDst) {
		memcpy(studDst, studSrc, sizeof(_Stud));
	}
	if (vmSrc && vmDst) {
		memcpy(vmDst, vmSrc, sizeof(_VmNode));
	}
	if (helperSrc && helperDst) {
		memcpy(helperDst, helperSrc, 0x2048);
	}
	if (studDst) {
		g_stu = studDst;
	}
	if (vmDst) {
		g_Vm = vmDst;
	}
	if (helperDst) {
		g_dataHlpers = helperDst;
	}
	// 获取dll的导出函数
#ifdef _WIN64
	dexportAddress = GetProcAddress((HMODULE)m_studBase, "CombatShellEntry");
	if (!dexportAddress) {
		dexportAddress = GetProcAddress((HMODULE)m_studBase, "VmEntry");
	}
#else
	dexportAddress = GetProcAddress((HMODULE)m_studBase, "CombatShellEntry");
	if (!dexportAddress) {
		dexportAddress = GetProcAddress((HMODULE)m_studBase, "_CombatShellEntry@0");
	}
#endif
	// ImageBase
#ifdef _WIN64
	m_dwStudSectionAddress64 = (DWORD64)SinglePuPEInfo::instance()->puGetSectionAddress((char *)m_studBase, (BYTE *)".text");
	m_dwNewSectionAddress64 = (DWORD64)SinglePuPEInfo::instance()->puGetSectionAddress((char *)m_lpBase, (BYTE *)NEWSECITONNAME);
	m_ImageBase64 = ((PIMAGE_NT_HEADERS)SinglePuPEInfo::instance()->puGetNtHeadre())->OptionalHeader.ImageBase;
#else
	m_dwStudSectionAddress = (DWORD)SinglePuPEInfo::instance()->puGetSectionAddress((char *)m_studBase, (BYTE *)".text");
	m_dwNewSectionAddress = (DWORD)SinglePuPEInfo::instance()->puGetSectionAddress((char *)m_lpBase, (BYTE *)NEWSECITONNAME);
	m_ImageBase = ((PIMAGE_NT_HEADERS)SinglePuPEInfo::instance()->puGetNtHeadre())->OptionalHeader.ImageBase;
#endif // _WIN64

	return TRUE;
}

// 修复重定位
BOOL studData::RepairReloCationStud()
{
	PIMAGE_DOS_HEADER pStuDos = (PIMAGE_DOS_HEADER)m_studBase;
#ifdef _WIN64
	PIMAGE_NT_HEADERS pStuNt = (PIMAGE_NT_HEADERS)(pStuDos->e_lfanew + (DWORD64)m_studBase);
	PIMAGE_BASE_RELOCATION pStuRelocation = (PIMAGE_BASE_RELOCATION)(pStuNt->OptionalHeader.DataDirectory[5].VirtualAddress + (DWORD64)m_studBase);
#else
	PIMAGE_NT_HEADERS pStuNt = (PIMAGE_NT_HEADERS)(pStuDos->e_lfanew + (DWORD)m_studBase);
	PIMAGE_BASE_RELOCATION pStuRelocation = (PIMAGE_BASE_RELOCATION)(pStuNt->OptionalHeader.DataDirectory[5].VirtualAddress + (DWORD)m_studBase);
#endif // _WIN64

	typedef struct _Node
	{
		WORD offset : 12;
		WORD type : 4;
	}Node, *PNode;

	DWORD OldAttribute = 0;
	while (pStuRelocation->SizeOfBlock)
	{
		DWORD nStuRelocationBlockCount = (pStuRelocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;

		_Node* RelType = (PNode)(pStuRelocation + 1);

		for (DWORD i = 0; i < nStuRelocationBlockCount; ++i)
		{
			if (RelType[i].type == 3) {
				DWORD* pRel = (DWORD *)(pStuRelocation->VirtualAddress + RelType[i].offset + (DWORD)m_studBase);

				VirtualProtect(pRel, 8, PAGE_READWRITE, &OldAttribute);

				*pRel = *pRel - (DWORD)m_studBase - ((PIMAGE_SECTION_HEADER)m_dwStudSectionAddress)->VirtualAddress + m_ImageBase + ((PIMAGE_SECTION_HEADER)m_dwNewSectionAddress)->VirtualAddress;

				VirtualProtect(pRel, 8, OldAttribute, &OldAttribute);
			}
#ifdef _WIN64
			if (RelType[i].type == 10) {
				PULONGLONG pAddress = (PULONGLONG)((DWORD64)m_studBase + pStuRelocation->VirtualAddress + RelType[i].offset);
				VirtualProtect(pAddress, 8, PAGE_READWRITE, &OldAttribute);
				*pAddress = *pAddress
					- (DWORD64)m_studBase
					- ((PIMAGE_SECTION_HEADER)m_dwStudSectionAddress64)->VirtualAddress
					+ ((PIMAGE_SECTION_HEADER)m_dwNewSectionAddress64)->VirtualAddress
					+ m_ImageBase64;
				VirtualProtect(pAddress, 8, OldAttribute, &OldAttribute);
			}

#endif // _WIN64
		}
#ifdef _WIN64
		pStuRelocation = (PIMAGE_BASE_RELOCATION)((DWORD64)pStuRelocation + pStuRelocation->SizeOfBlock);
#else
		pStuRelocation = (PIMAGE_BASE_RELOCATION)((DWORD)pStuRelocation + pStuRelocation->SizeOfBlock);
#endif
	}
	return TRUE;
}

// 拷贝stud数据到新增区段
BOOL studData::CopyStud()
{
	PIMAGE_SECTION_HEADER studSection = SinglePuPEInfo::instance()->puGetSectionAddress((char *)m_studBase, (BYTE *)".text");
	PIMAGE_SECTION_HEADER SurceBase = SinglePuPEInfo::instance()->puGetSectionAddress((char *)m_lpBase, (BYTE *)NEWSECITONNAME);
	if (!studSection || (!SurceBase))
		return false;
#ifdef _WIN64
	memcpy(
		(void *)(SurceBase->PointerToRawData + (DWORD64)m_lpBase),
		(void *)(studSection->VirtualAddress + (DWORD64)m_studBase),
		studSection->Misc.VirtualSize
	);
#else
	memcpy(
		(void *)(SurceBase->PointerToRawData + (DWORD)m_lpBase),
		(void *)(studSection->VirtualAddress + (DWORD)m_studBase),
		studSection->Misc.VirtualSize
	);
#endif

	DWORD dwRiteFile = 0;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)SinglePuPEInfo::instance()->puGetNtHeadre();
	if (!pNt)
		return false;

	// OEP
#ifdef _WIN64
	pNt->OptionalHeader.AddressOfEntryPoint = (DWORD64)dexportAddress - (DWORD64)m_studBase - studSection->VirtualAddress + SurceBase->VirtualAddress;
#else
	pNt->OptionalHeader.AddressOfEntryPoint = (DWORD)dexportAddress - (DWORD)m_studBase - studSection->VirtualAddress + SurceBase->VirtualAddress;
#endif

	// The packer creates sections with PointerToRawData aligned to 0x200, but
	// the original PE might declare FileAlignment = 0x1000.  The PE loader
	// rejects sections whose PTRD is not a multiple of FileAlignment, which
	// silently breaks import resolution and section mapping.  Force 0x200 so
	// the packed layout is valid.
	if (pNt->OptionalHeader.FileAlignment > 0x200) {
		pNt->OptionalHeader.FileAlignment = 0x200;
	}

	// Also fix SizeOfHeaders: the original value (e.g. 0x1000) can overlap
	// with the first section data (PTRD=0x400).  Re-derive from the actual
	// header + section-table size, aligned to the new FileAlignment.
	{
		DWORD hdrsEnd = (DWORD)(
			((PIMAGE_DOS_HEADER)m_lpBase)->e_lfanew + 24 +
			pNt->FileHeader.SizeOfOptionalHeader +
			pNt->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER));
		DWORD fa = pNt->OptionalHeader.FileAlignment;
		DWORD newSoh = (hdrsEnd + fa - 1) & ~(fa - 1);
		if (newSoh < pNt->OptionalHeader.SizeOfHeaders) {
			pNt->OptionalHeader.SizeOfHeaders = newSoh;
		}
	}

	// Write a minimal Import Table into spare .VMP space so the Windows
	// loader loads kernel32.dll.  The IAT must be in a writable section
	// (the loader writes resolved addresses into it), so we place the
	// entire mini-import-table inside .VMP which has RWX characteristics.
	{
		DWORD shellSize = studSection->Misc.VirtualSize;
		DWORD importOff = (shellSize + 15) & ~15;  // align to 16
		DWORD vmpVA  = SurceBase->VirtualAddress;
		DWORD importRva = vmpVA + importOff;

#ifdef _WIN64
		const DWORD ptrSize = 8;
#else
		const DWORD ptrSize = 4;
#endif
		const DWORD totalNeeded = 56 + 4 * ptrSize + 14;

		if (importOff + totalNeeded <= SurceBase->SizeOfRawData) {
			BYTE* base = (BYTE*)m_lpBase + SurceBase->PointerToRawData + importOff;
			memset(base, 0, totalNeeded);

			const DWORD nameRva = importRva + 40;
			const DWORD intRva  = importRva + 56;
			const DWORD iatRva  = importRva + 56 + 2 * ptrSize;
			const DWORD hintRva = importRva + 56 + 4 * ptrSize;

			PIMAGE_IMPORT_DESCRIPTOR pDesc = (PIMAGE_IMPORT_DESCRIPTOR)base;
			pDesc->OriginalFirstThunk = intRva;
			pDesc->TimeDateStamp      = 0;
			pDesc->ForwarderChain     = (DWORD)-1;
			pDesc->Name               = nameRva;
			pDesc->FirstThunk         = iatRva;

			memcpy(base + 40, "kernel32.dll", 13);

#ifdef _WIN64
			*(ULONGLONG*)(base + 56)                = (ULONGLONG)hintRva;
			*(ULONGLONG*)(base + 56 + 2 * ptrSize)  = (ULONGLONG)hintRva;
#else
			*(DWORD*)(base + 56)                    = hintRva;
			*(DWORD*)(base + 56 + 2 * ptrSize)      = hintRva;
#endif
			*(WORD*)(base + 56 + 4 * ptrSize) = 0;
			memcpy(base + 56 + 4 * ptrSize + 2, "ExitProcess", 12);

			pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = importRva;
			pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = 40;
			pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress = iatRva;
			pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size = 2 * ptrSize;
		}
	}

	HANDLE hFile = SinglePuPEInfo::instance()->puFileHandle();
	if (!hFile || hFile == INVALID_HANDLE_VALUE)
		return FALSE;
	SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(hFile);

	int nRet = WriteFile(hFile, SinglePuPEInfo::instance()->puGetImageBase(), SinglePuPEInfo::instance()->puFileSize(), &dwRiteFile, NULL);
	if (!nRet || dwRiteFile != SinglePuPEInfo::instance()->puFileSize())
		return FALSE;

	// Ensure the file covers the full .VMP section (SizeOfRawData may be
	// larger than the data actually present due to FileAlignment rounding).
	// Without this, the PE loader may reject the import table we placed
	// at the end of .VMP because the file is truncated.
	DWORD expectedEnd = SurceBase->PointerToRawData + SurceBase->SizeOfRawData;
	if (expectedEnd > dwRiteFile) {
		SetFilePointer(hFile, expectedEnd, nullptr, FILE_BEGIN);
		SetEndOfFile(hFile);
	}

	return TRUE;
}

// Clear
void studData::puClearStuData()
{
	SinglePuPEInfo::instance()->puClearPeData();
	if (m_lpBase)
		m_lpBase = nullptr;
	if (m_studBase)
		m_studBase = nullptr;
	m_OldOEP = 0;
	m_ImageBase = 0;
}
