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
	memset(NewpSection->Name, 0, IMAGE_SIZEOF_SHORT_NAME);
	if (Name) {
		size_t nameLen = 0;
		while (nameLen < IMAGE_SIZEOF_SHORT_NAME && Name[nameLen] != '\0') {
			++nameLen;
		}
		memcpy(NewpSection->Name, Name, nameLen);
	}

	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)pNtHeadre;
	if (!pNt) {
		return false;
	}
	const DWORD sectionAlignment = pNt->OptionalHeader.SectionAlignment ? pNt->OptionalHeader.SectionAlignment : 0x1000;
	const DWORD fileAlignment = pNt->OptionalHeader.FileAlignment ? pNt->OptionalHeader.FileAlignment : 0x200;
	const DWORD prevVirtualSize = PtrpSection->Misc.VirtualSize ? PtrpSection->Misc.VirtualSize : PtrpSection->SizeOfRawData;
	DWORD dwtemps = PtrpSection->VirtualAddress + AlignUpDword(prevVirtualSize, sectionAlignment);
	if (!dwtemps)
		return false;

	DWORD Temp = 0;
	dwtemps = AlignUpDword(dwtemps, sectionAlignment);
	NewpSection->VirtualAddress = dwtemps;
	Temp = AlignUpDword(PtrpSection->SizeOfRawData + PtrpSection->PointerToRawData, fileAlignment);
	if (!dwtemps || !Temp)
		return 0;
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
		pNt->OptionalHeader.SizeOfImage = AlignUpDword(NewpSection->VirtualAddress + NewpSection->Misc.VirtualSize, sectionAlignment);
		return TRUE;
	}
	return FALSE;
}

BOOL AddSection::AddNewSectionByteData(const DWORD & size)
{
	// Use the file-aligned section size so the physical file matches what the
	// section header claims.  Prevents buffer over-reads when the file is
	// reloaded and code trusts SizeOfRawData to determine readable extent.
	const DWORD alignedSize = NewpSection ? NewpSection->SizeOfRawData : size;
	const DWORD newFileSize = FileSize + alignedSize;
	m_newlpBase = (char *)malloc(newFileSize);
	if (!m_newlpBase || (nullptr == m_newlpBase))
		return false;
	memset(m_newlpBase, 0, newFileSize);

	if (pFileBaseData) {
		memcpy(m_newlpBase, pFileBaseData, FileSize);
	}
	else
		return false;

	DWORD dWriteSize = 0; OVERLAPPED OverLapped = { 0 };
	int nRetCode = WriteFile(FileHandle, m_newlpBase, newFileSize, &dWriteSize, &OverLapped);
	if (m_newlpBase) {
		free(m_newlpBase);
		m_newlpBase = nullptr;
	}
	if (dWriteSize == 0){ 
		AfxMessageBox(L"CreateSection WriteFIle faliuer"); 
		return FALSE; 
	}
	return TRUE;
}

