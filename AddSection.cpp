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
	memcpy(NewpSection->Name, Name, sizeof(Name));
	DWORD dwtemps = PtrpSection->VirtualAddress + PtrpSection->SizeOfRawData;
	if (!dwtemps)
		return false;

	DWORD Temp = 0;
	dwtemps = AlignUpDword(dwtemps, 0x1000);
	NewpSection->VirtualAddress = dwtemps;
	Temp = AlignUpDword(PtrpSection->SizeOfRawData + PtrpSection->PointerToRawData, 0x200);
	if (!dwtemps || !Temp)
		return 0;
	NewpSection->PointerToRawData = Temp;
	NewpSection->SizeOfRawData = size;
	NewpSection->Misc.VirtualSize = NewpSection->SizeOfRawData;
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
		pNt->OptionalHeader.SizeOfImage = NewpSection->VirtualAddress + NewpSection->SizeOfRawData;
		pNt->OptionalHeader.DllCharacteristics = 0x8000;
		return TRUE;
	}
	return FALSE;
}

BOOL AddSection::AddNewSectionByteData(const DWORD & size)
{
	const int newFileSize = FileSize + size;
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
	int nRetCode = WriteFile(FileHandle, m_newlpBase, (FileSize + size), &dWriteSize, &OverLapped);
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

