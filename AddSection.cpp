#include "stdafx.h"
#include "AddSection.h"
#include "puPEinfoData.h"

static DWORD AlignUpDword(const DWORD value, const DWORD alignment)
{
	if (alignment == 0) {
		return value;
	}
	const DWORD remainder = value % alignment;
	if (remainder == 0) {
		return value;
	}
	return value + (alignment - remainder);
}

AddSection::AddSection()
{
}

AddSection::~AddSection()
{
	if (pFileBaseData) {
		free(pFileBaseData);
		pFileBaseData = nullptr;
	}
	if (FileHandle) {
		CloseHandle(FileHandle);
		FileHandle = nullptr;
	}
}

BOOL AddSection::Init() {
	Free();

	SinglePuPEInfo::instance()->puOpenFileLoadEx(m_FilePath);
	pFileBaseData = SinglePuPEInfo::instance()->puGetImageBase();
	pNtHeadre = SinglePuPEInfo::instance()->puGetNtHeadre();
	pSectionHeadre = SinglePuPEInfo::instance()->puGetSection();
	FileSize = SinglePuPEInfo::instance()->puFileSize();
	FileHandle = SinglePuPEInfo::instance()->puFileHandle();
	OldOep = SinglePuPEInfo::instance()->puGetOEP();
	return true;
}

BOOL AddSection::Free() {
	SinglePuPEInfo::instance()->puClearPeData();
	if (pFileBaseData) {
		pFileBaseData = nullptr;
	}
	if (FileHandle) {
		FileHandle = nullptr;
	}
	if (m_newlpBase) {
		free(m_newlpBase);
		m_newlpBase = nullptr;
	}
	pNtHeadre = nullptr;
	pSectionHeadre = nullptr;
	SectionSizeof = 0;
	FileSize = 0;
	return true;
}

BOOL AddSection::ModifySectionNumber()
{
	PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)this->pNtHeadre;
	if (pNtHeaders) {
		DWORD temp = pNtHeaders->FileHeader.NumberOfSections;
		SectionSizeof = temp * 0x28;
		pNtHeaders->FileHeader.NumberOfSections += 0x1;
		return TRUE;
	}
	return false;
}

BOOL AddSection::ModifySectionInfo(const BYTE* Name, const DWORD & size)
{
	if (!pNtHeadre || !pSectionHeadre || !Name) {
		return FALSE;
	}

	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)pNtHeadre;
	const DWORD sectionAlignment = pNt->OptionalHeader.SectionAlignment ? pNt->OptionalHeader.SectionAlignment : 0x1000;
	const DWORD fileAlignment = pNt->OptionalHeader.FileAlignment ? pNt->OptionalHeader.FileAlignment : 0x200;

#ifdef _WIN64
	DWORD64 pSectionAddress = (DWORD64)pSectionHeadre;
#else
	DWORD pSectionAddress = (DWORD)pSectionHeadre;
#endif
	pSectionAddress = pSectionAddress + SectionSizeof - 0x28;
	PIMAGE_SECTION_HEADER PtrpSection = (PIMAGE_SECTION_HEADER)pSectionAddress;
	if (!PtrpSection)
		return false;

	pSectionAddress += 0x28;
	NewpSection = (PIMAGE_SECTION_HEADER)pSectionAddress;
	memset(NewpSection, 0, sizeof(IMAGE_SECTION_HEADER));
	const size_t sectionNameLen = strlen((const char*)Name);
	const size_t copyLen = sectionNameLen < IMAGE_SIZEOF_SHORT_NAME ? sectionNameLen : IMAGE_SIZEOF_SHORT_NAME;
	memcpy(NewpSection->Name, Name, copyLen);

	DWORD prevVirtualSize = PtrpSection->Misc.VirtualSize;
	if (prevVirtualSize == 0) {
		prevVirtualSize = PtrpSection->SizeOfRawData;
	}
	DWORD dwtemps = PtrpSection->VirtualAddress + prevVirtualSize;
	if (!dwtemps)
		return false;

	DWORD Temp = 0;
#ifdef _WIN64
	dwtemps = AlignUpDword(dwtemps, sectionAlignment);
	NewpSection->VirtualAddress = dwtemps;
	Temp = AlignUpDword(PtrpSection->SizeOfRawData + PtrpSection->PointerToRawData, fileAlignment);
	// check arg
	if (!dwtemps || !Temp)
		return 0;

#else
	__asm{
		pushad;
		mov		esi, dwtemps;
		mov		eax, dwtemps;
		mov		edx, 0x1;
		mov		cx, 0x1000;
		div		cx;
		test	dx, dx;
		jz		MemSucess
		shr		dx, 12;
		inc		dx;
		shl		dx, 12;
		add		esi, edx;
		shr		esi, 12;
		shl		esi, 12;
		mov		dwtemps, esi;
	MemSucess:
		popad
	}
	NewpSection->VirtualAddress = dwtemps;

	Temp = PtrpSection->SizeOfRawData + PtrpSection->PointerToRawData;

	__asm{
		pushad;
		mov		esi, Temp;
		mov		edx, 0x1;
		mov		eax, Temp;
		mov		ecx, 0x200;
		div		cx;
		test	dx, dx;
		jz		FileSucess
		xor		eax, eax
		mov		ax, 0x200;
		sub		ax, dx;
		add		esi, eax;
		mov		Temp, esi;
	FileSucess:
		popad
	}

#endif // _WIN64
	NewpSection->PointerToRawData = Temp;
	NewpSection->SizeOfRawData = AlignUpDword(size, fileAlignment);
	NewpSection->Misc.VirtualSize = size;
	NewpSection->Characteristics = 0xE00000E0;
	return TRUE;
}

BOOL AddSection::ModifyProgramEntryPoint()
{
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)pNtHeadre;
	if (pNt) {
		pNt->OptionalHeader.AddressOfEntryPoint = NewpSection->VirtualAddress;
		return TRUE;
	}
	return false;
}

BOOL AddSection::ModifySizeofImage()
{
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)pNtHeadre;
	if (pNt) {
		const DWORD sectionAlignment = pNt->OptionalHeader.SectionAlignment ? pNt->OptionalHeader.SectionAlignment : 0x1000;
		const DWORD imageEnd = NewpSection->VirtualAddress + max(NewpSection->Misc.VirtualSize, NewpSection->SizeOfRawData);
		pNt->OptionalHeader.SizeOfImage = AlignUpDword(imageEnd, sectionAlignment);
		// Preserve original DllCharacteristics but clear flags that require
		// data directories zeroed in the packed PE:
		// - GUARD_CF (0x4000): LoadConfig directory is zeroed
		// - DYNAMIC_BASE (0x0040): .reloc section data is zeroed, no relocation possible
		// - HIGH_ENTROPY_VA (0x0020): meaningless without DYNAMIC_BASE
		// Keep NX_COMPAT, TERMINAL_SERVER_AWARE, etc.
		pNt->OptionalHeader.DllCharacteristics &= ~(WORD)(0x4000 | 0x0040 | 0x0020);
		return TRUE;
	}
	return FALSE;
}

BOOL AddSection::AddNewSectionByteData(const DWORD & size)
{
	if (!FileHandle || !pFileBaseData || !NewpSection) {
		return FALSE;
	}

	const int newFileSize = FileSize + NewpSection->SizeOfRawData;
	m_newlpBase = (char *)malloc(newFileSize);
	if (!m_newlpBase || (nullptr == m_newlpBase))
		return false;
	memset(m_newlpBase, 0, newFileSize);

	memcpy(m_newlpBase, pFileBaseData, FileSize);
	SetFilePointer(FileHandle, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(FileHandle);

	DWORD dWriteSize = 0;
	int nRetCode = WriteFile(FileHandle, m_newlpBase, newFileSize, &dWriteSize, NULL);
	if (m_newlpBase) {
		free(m_newlpBase);
		m_newlpBase = nullptr;
	}
	if (!nRetCode || dWriteSize != (DWORD)newFileSize){ 
		AfxMessageBox(L"CreateSection WriteFIle faliuer"); 
		return FALSE; 
	}
	return TRUE;
}

