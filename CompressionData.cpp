#include "stdafx.h"
#include "CompressionData.h"
#include "vm.h"
#include "AddSection.h"
#include "puPEinfoData.h"
#include "CombatShell/CombatShell.h"
#include "lz4/include/lz4.h"
#include "quick/quicklz.h"
#include <io.h>

#define NEWSECITONNAME ".VMP"

FILE*			fpVmFile = NULL;
// CombatShell Export
_Stud*			g_stu = nullptr;
_VmNode*		g_Vm = nullptr;
char*			g_dataHlpers = nullptr;
DWORD64			g_dataoffset = 0;
extern char		g_CombatShellDataLocalFile[MAX_PATH];
DWORD			g_CompressionMethod =
#ifdef _WIN64
	COMBATSHELL_COMPRESS_QUICKLZ;
#else
	COMBATSHELL_COMPRESS_LZ4;
#endif
DWORD			g_ProtectionFlags =
#ifdef _WIN64
	COMBATSHELL_PROTECT_VM_ENTRY;
#else
	0;
#endif
DWORD			g_EncryptionKey = 0x5A;

// Preserved LOAD_CONFIG fields for the PE loader (filled by CleanDirectData,
// consumed by CopyStud to write a stub into .VMP spare space).
DWORD   g_LoadConfigOrigSize = 0;
DWORD64 g_LoadConfigSecurityCookie = 0;

static int GetCompressionBound(DWORD method, DWORD inputSize)
{
	switch (method) {
	case COMBATSHELL_COMPRESS_QUICKLZ:
		if (inputSize > (DWORD)((INT_MAX - 1024) / 2))
			return 0;
		return (int)(inputSize * 2) + 1024;
	case COMBATSHELL_COMPRESS_LZ4:
		if (inputSize > (DWORD)INT_MAX)
			return 0;
		return LZ4_compressBound((int)inputSize);
	case COMBATSHELL_COMPRESS_NONE:
		if (inputSize > (DWORD)INT_MAX)
			return 0;
		return (int)inputSize;
	default:
		return 0;
	}
}

static DWORD CompressSectionBuffer(
	DWORD method,
	const char* input,
	DWORD inputSize,
	char* output,
	int outputCapacity)
{
	if (!input || !output || inputSize == 0) {
		return 0;
	}

	switch (method) {
	case COMBATSHELL_COMPRESS_QUICKLZ:
	{
		qlz_state_compress* state_compress = (qlz_state_compress*)malloc(sizeof(qlz_state_compress));
		if (!state_compress) {
			return 0;
		}
		memset(state_compress, 0, sizeof(qlz_state_compress));
		DWORD compressed = (DWORD)qlz_compress(input, output, inputSize, state_compress);
		free(state_compress);
		return compressed;
	}
	case COMBATSHELL_COMPRESS_LZ4:
		return (DWORD)LZ4_compress_default(input, output, (int)inputSize, outputCapacity);
	case COMBATSHELL_COMPRESS_NONE:
		memcpy(output, input, inputSize);
		return inputSize;
	default:
		return 0;
	}
}

static void XorBuffer(char* data, DWORD size, BYTE key)
{
	if (!data || size == 0 || key == 0) {
		return;
	}
	for (DWORD i = 0; i < size; ++i) {
		data[i] ^= key;
	}
}

CompressionData::CompressionData()
{
}

CompressionData::~CompressionData()
{
	if (m_lpBase) {
		m_lpBase = nullptr;
	}
	if (m_hFile) {
		m_hFile = nullptr;
	}
	// clear
	SinglePuPEInfo::instance()->puClearPeData();
}

VOID CompressionData::ReFileInit()
{
	if (m_lpBase) {
		m_lpBase = nullptr;
	}
	if (m_hFile) {
		m_hFile = nullptr;
	}
	SinglePuPEInfo::instance()->puClearPeData();

	SinglePuPEInfo::instance()->puOpenFileLoadEx(m_MasterStaticTextStr);
	m_lpBase = SinglePuPEInfo::instance()->puGetImageBase();
	m_SectionHeadre = SinglePuPEInfo::instance()->puGetSection();
	m_SectionCount = ((PIMAGE_NT_HEADERS)(SinglePuPEInfo::instance()->puGetNtHeadre()))->FileHeader.NumberOfSections;
	m_hFile = SinglePuPEInfo::instance()->puFileHandle();
	m_hFileSize = SinglePuPEInfo::instance()->puFileSize();
}

// 压缩区段之前 Vmencode
void CompressionData::VmcodeEntry(char* TargetCode, _Out_ int &CodeLength)
{
	// 这里写入需要加密多少次,或者代码段Asm
	int vm_len = 1;
	// write: 0. 写入一共VM加密多少代码段
	g_Vm->VmCount = vm_len;

	DWORD64 Offset = 0;
	// 获取VM的起始地址
#ifdef _WIN64
	DWORD64 Vmencodeaddr = (DWORD64)GetProcAddress((HMODULE)m_studBase, "CombatShellEntry");
#else
	DWORD64 Vmencodeaddr = (DWORD64)GetProcAddress((HMODULE)m_studBase, "CombatShellEntry");
#endif
	if (!Vmencodeaddr)
		return;
	PIMAGE_SECTION_HEADER studSection = SinglePuPEInfo::instance()->puGetSectionAddress((char *)m_studBase, (BYTE *)".text");
	PIMAGE_SECTION_HEADER SurceBase = SinglePuPEInfo::instance()->puGetSectionAddress((char *)m_lpBase, (BYTE *)NEWSECITONNAME);
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)SinglePuPEInfo::instance()->puGetNtHeadre();
	if (!studSection || (!SurceBase) || (!pNt))
		return;

#ifdef _WIN64
	Offset = (DWORD64)Vmencodeaddr - (DWORD64)m_studBase - studSection->VirtualAddress + SurceBase->VirtualAddress;
	// 计算保存VM数据偏移
	g_dataoffset = (DWORD64)g_dataHlpers - (DWORD64)m_studBase - studSection->VirtualAddress + SurceBase->VirtualAddress;
#else
	Offset = (DWORD)Vmencodeaddr - (DWORD)m_studBase - studSection->VirtualAddress + SurceBase->VirtualAddress;
#endif
	// write: 1. offset -- 汇编指令
	g_Vm->VmAddroffset = Offset;
#ifdef _WIN64
	vm_len = 21;		// CombatShellEntry has 21 instructions (push/sub/mov/call/add/test/jz/jmp/ret)
#else
	vm_len = 82;		// 固定的需要人工去看反汇编多少行,ida中看一下,不智能
#endif
	g_Vm->Vmencodeasmlen = vm_len;
	VM vmobj;
	vmobj.VmEntry((PVOID64)Vmencodeaddr, vm_len);
}

// 添加区段给压缩后的数据使用
void CompressionData::AddCompreDataSection(const DWORD & size)
{
	BYTE Name[] = ".UPX";
	DWORD Compresdata = size;

	SingleAddSection::instance()->puInti(m_MasterStaticTextStr);
	SingleAddSection::instance()->puModifySectioNumber();
	SingleAddSection::instance()->puModifySectionInfo(Name, Compresdata);
	SingleAddSection::instance()->puModifySizeofImage();
	SingleAddSection::instance()->puAddNewSectionByData(Compresdata);
	SingleAddSection::instance()->puFree();
}

BOOL CompressionData::EncryptionSectionData(
	char* src,
	int srclen,
	char enkey)
{
	for (int i = 0; i < srclen; ++i)
	{
		*src ^= enkey;
		src++;
	}
	return 1;
}

// 压缩PE区段数据
BOOL CompressionData::CompressSectionData()
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
	g_stu = (_Stud*)GetProcAddress((HMODULE)m_studBase, "g_stud");
	g_Vm = (VmNode*)GetProcAddress((HMODULE)m_studBase, "g_VmNode");
	g_dataHlpers = (char *)GetProcAddress((HMODULE)m_studBase, "g_dataHlper");// g_dataHlper
	if (!g_stu || (!g_Vm) || (!g_dataHlpers)) {
		AfxMessageBox(L"Stud.dll GetProcAddress 失败.");
		return 0;
	}
	g_stu->s_OneSectionSizeofData = FALSE;
	g_stu->s_CompressionMethod = g_CompressionMethod;
	g_stu->s_ProtectionFlags = g_ProtectionFlags;
	g_stu->s_EncryptionKey = g_EncryptionKey & 0xFF;

#ifdef _WIN64
	// 压缩前后都可以, 仅壳代码VM
	int nLen = 0;
	if (g_ProtectionFlags & COMBATSHELL_PROTECT_VM_ENTRY) {
		VmcodeEntry(NULL, nLen);
	} else if (g_Vm) {
		memset(g_Vm, 0, sizeof(_VmNode));
	}

	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(((PIMAGE_DOS_HEADER)m_lpBase)->e_lfanew + (DWORD64)m_lpBase);
#else
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(((PIMAGE_DOS_HEADER)m_lpBase)->e_lfanew + (DWORD)m_lpBase);
#endif
	if (!pNt)
		return false;

	// Save LOAD_CONFIG fields from the ORIGINAL PE before compression
	// rearranges the section data.  The SecurityCookie and Size are
	// needed by CopyStud to write a loader-visible stub into .VMP.
#ifdef _WIN64
	{
		extern DWORD   g_LoadConfigOrigSize;
		extern DWORD64 g_LoadConfigSecurityCookie;
		g_LoadConfigOrigSize = 0;
		g_LoadConfigSecurityCookie = 0;
		const IMAGE_DATA_DIRECTORY& lcDir = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
		if (lcDir.VirtualAddress && lcDir.Size >= 0x60) {
			// Convert RVA to file offset using section table.
			PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(pNt);
			for (DWORD si = 0; si < pNt->FileHeader.NumberOfSections; ++si, ++sec) {
				if (sec->SizeOfRawData == 0) continue;
				if (lcDir.VirtualAddress >= sec->VirtualAddress &&
					lcDir.VirtualAddress < sec->VirtualAddress + sec->SizeOfRawData) {
					DWORD fileOff = sec->PointerToRawData + (lcDir.VirtualAddress - sec->VirtualAddress);
					const BYTE* lcData = (const BYTE*)m_lpBase + fileOff;
					g_LoadConfigOrigSize = *(const DWORD*)lcData;
					g_LoadConfigSecurityCookie = *(const DWORD64*)(lcData + 0x58);
					break;
				}
			}
		}
	}
#endif

	DWORD dSectionCount = pNt->FileHeader.NumberOfSections;
	PIMAGE_SECTION_HEADER psection = (PIMAGE_SECTION_HEADER)m_SectionHeadre;
	m_maskAddress = SinglePuPEInfo::instance()->puGetSectionAddress((char *)m_lpBase, (BYTE *)NEWSECITONNAME);
	if (!m_maskAddress) {
		AfxMessageBox(L".VMP 区别识别失败!\n");
		return false;
	}

	// 避免如.textbss无数据
	for (DWORD i = 0; i < dSectionCount; ++i)
	{
		if (psection->PointerToRawData != 0)
			break;
		++psection;
	}

	// pe标准大小对齐后（加载基址 + .text->pointertorawdata的数据）= 大小
	DWORD pStandardHeadersize = psection->PointerToRawData;

	DWORD maxCompressedData = 0;
	{
		PIMAGE_SECTION_HEADER sizeSection = (PIMAGE_SECTION_HEADER)m_SectionHeadre;
		for (DWORD i = 0; i < dSectionCount - 1; ++i, ++sizeSection)
		{
			if (sizeSection->SizeOfRawData == 0)
				continue;
			const int bound = GetCompressionBound(g_CompressionMethod, sizeSection->SizeOfRawData);
			if (bound <= 0 || maxCompressedData > MAXDWORD - (DWORD)bound) {
				AfxMessageBox(L"compressed data size overflow.");
				return false;
			}
			maxCompressedData += (DWORD)bound;
		}
	}
	if (maxCompressedData == 0)
		maxCompressedData = 1;

	char* SaveCompressData = (char*)malloc(maxCompressedData);
	if (!SaveCompressData)
		return false;
	memset(SaveCompressData, 0, maxCompressedData);

	PIMAGE_SECTION_HEADER pSections = (PIMAGE_SECTION_HEADER)m_SectionHeadre;
	DWORD ComressTotalSize = 0;

	// 注意修复-改为程序Name_FileData.txt - 保存本地数据记录，脱壳使用.
	if ((fpFile = fopen(g_CombatShellDataLocalFile, "wb+")) == NULL) 
	{
		free(SaveCompressData);
		AfxMessageBox(L"CombatShell 打开创建失败.");
		return false;
	}
	// 不压缩新增的区段（加壳区段）
	// At this point only .VMP was added (not .UPX yet), so subtract 1.
	for (DWORD i = 0; i < dSectionCount - 1; ++i)
	{
		DWORD DataSize = pSections->SizeOfRawData;
		if (pSections->SizeOfRawData == 0)
		{
			fwrite(&pSections->SizeOfRawData, sizeof(DWORD), 1, fpFile);
			fflush(fpFile);
			++pSections;
			g_stu->s_OneSectionSizeofData = TRUE;
			continue;
		}

		char* buf = NULL;
		void* DataAddress = (void *)(pSections->PointerToRawData + (DWORD64)m_lpBase);
		DWORD dwCompressionSize = 0;
		const int blen = GetCompressionBound(g_CompressionMethod, pSections->SizeOfRawData);
		if (blen <= 0)
		{
			AfxMessageBox(L"invalid compression method.");
			return false;
		}

		if ((buf = (char*)malloc(sizeof(char) * blen)) == NULL)
		{
			AfxMessageBox(L"no enough memory!\n");
			return -1;
		}

		dwCompressionSize = CompressSectionBuffer(
			g_CompressionMethod,
			(char*)DataAddress,
			pSections->SizeOfRawData,
			buf,
			blen);
		if ((g_ProtectionFlags & COMBATSHELL_PROTECT_ENCRYPT_SECTIONS) && dwCompressionSize > 0) {
			XorBuffer(buf, dwCompressionSize, (BYTE)(g_EncryptionKey & 0xFF));
		}
		if (dwCompressionSize == 0) {
			if (buf) {
				free(buf);
				buf = nullptr;
			}
			AfxMessageBox(L"section compression failed.");
			return false;
		}
		fwrite(&dwCompressionSize, sizeof(DWORD), 1, fpFile);
		fflush(fpFile);

		// 区段异或加密
		// EncryptionSectionData(buf, dwCompressionSize, 'B');

		// 计算缓区去后大小
		memcpy(&g_stu->s_blen[i], &dwCompressionSize, sizeof(DWORD));

		if (ComressTotalSize > maxCompressedData - dwCompressionSize) {
			free(buf);
			free(SaveCompressData);
			AfxMessageBox(L"compressed data buffer overflow.");
			return false;
		}

		// 保存压缩后区段数据（拼接每一个压缩区段）
		memcpy(&SaveCompressData[ComressTotalSize], buf, dwCompressionSize);

		// 保存压缩后总大小
		ComressTotalSize += dwCompressionSize;

		if (buf) {
			free(buf);
			buf = nullptr;
		}
		++pSections;
	}
	if (fpFile)
		fclose(fpFile);

	// Align compressed data to 0x200 (the final FileAlignment forced by CopyStud).
	// Use pStandardHeadersize as the base offset (not hardcoded 0x400) so that
	// the header area isn't truncated for PEs with large original headers.
	DWORD alignedCompressed = ComressTotalSize;
	if (alignedCompressed % 0x200 != 0)
		alignedCompressed = ((alignedCompressed / 0x200) + 1) * 0x200;
	DWORD Size = pStandardHeadersize + alignedCompressed;

	// 创建一个新区段
	DWORD ModifySize = Size - pStandardHeadersize;
	AddCompreDataSection(ModifySize);

	// 重载文件 - 修改新区段的信息数据 文件偏移 = pStandardHeadersize 大小 = 压缩后数据对齐大小
	ReFileInit();

	// Re-query m_maskAddress: ReFileInit() freed the old PE allocation and created
	// a new one, so the pointer captured at line 182 is now dangling.
	m_maskAddress = SinglePuPEInfo::instance()->puGetSectionAddress((char*)m_lpBase, (BYTE*)NEWSECITONNAME);
	if (!m_maskAddress) {
		free(SaveCompressData);
		return false;
	}

	BYTE byteName[] = ".UPX";
	SinglePuPEInfo::instance()->puSetFileoffsetAndFileSize(m_lpBase, pStandardHeadersize, ModifySize, byteName);
	BYTE byteNmase[] = ".UPX";
	PIMAGE_SECTION_HEADER compSectionAddress = SinglePuPEInfo::instance()->puGetSectionAddress((char*)m_lpBase, byteNmase);
	if (!compSectionAddress)
		return false;

	// 保存内存地址 用于解压基址
	g_stu->s_CompressionSectionRva = compSectionAddress->VirtualAddress;

	// 拷贝压缩后的数据(对齐) --> 新加的区段
#ifdef  _WIN64
	memcpy((PVOID64)(compSectionAddress->PointerToRawData + (DWORD64)m_lpBase), SaveCompressData, ModifySize);
#else
	memcpy((void*)(compSectionAddress->PointerToRawData + (DWORD)m_lpBase), SaveCompressData, ModifySize);
#endif //  _WIN64
	if (SaveCompressData) {
		free(SaveCompressData);
		SaveCompressData = nullptr;
	}
	// 拼接标准PE头 + 压缩数据的区段 + 自己的区段
	char* ComressNewBase = (char*)malloc(Size + m_maskAddress->SizeOfRawData);
	if (!ComressNewBase)
		return false;
	// 拼接标准PE
	memset(ComressNewBase, 0, (Size + m_maskAddress->SizeOfRawData));
	memcpy(ComressNewBase, m_lpBase, pStandardHeadersize);

	
#ifdef _WIN64
	// 拼接压缩后的全部区段(第一个头信息)
	memcpy(&ComressNewBase[pStandardHeadersize], (PVOID64)(compSectionAddress->PointerToRawData + (DWORD64)m_lpBase), ComressTotalSize);
	memcpy(&ComressNewBase[Size], (PVOID64)(m_maskAddress->PointerToRawData + (DWORD64)m_lpBase), m_maskAddress->SizeOfRawData);
#else
	memcpy(&ComressNewBase[pStandardHeadersize], (void*)(compSectionAddress->PointerToRawData + (DWORD)m_lpBase), ComressTotalSize);
	// 拼接加壳区段数据
	memcpy(&ComressNewBase[Size], (void *)(m_maskAddress->PointerToRawData + (DWORD)m_lpBase), m_maskAddress->SizeOfRawData);
#endif // _WIN64

	// 清空数据目录表(收尾工作)
	CleanDirectData(ComressNewBase, ComressTotalSize, Size);

	// Create File
	std::wstring wsTagetDirectory = L"";
	{
		CString csTmep = m_MasterStaticTextStr;
		int n = csTmep.ReverseFind('\\') + 1;
		int m = csTmep.GetLength() - n;
		wsTagetDirectory = csTmep.Left(n);
		csTmep = csTmep.Right(m);
	}
	const std::wstring wsMaskCompre = (wsTagetDirectory + L"CompressionMask.exe").c_str();
	HANDLE HandComprele = CreateFile(wsMaskCompre.c_str(), GENERIC_READ | GENERIC_WRITE, FALSE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
	// Write 写入压缩
	DWORD dwWrite = 0;
	int nRet = WriteFile(HandComprele, ComressNewBase, (Size + m_maskAddress->SizeOfRawData), &dwWrite, NULL);
	CloseHandle(HandComprele);
	if (ComressNewBase) {
		free(ComressNewBase);
		ComressNewBase = nullptr;
	}
	if (!nRet)
		AfxMessageBox(L"CompressWriteFile failuer");
	return TRUE;
}

// 判断真正的区段数据大小（未对齐）
DWORD CompressionData::IsSectionSize(DWORD MiscVirtualsize, DWORD sizeOfRawData)
{
	if (MiscVirtualsize > sizeOfRawData)
		return sizeOfRawData;
	if (MiscVirtualsize < sizeOfRawData)
		return MiscVirtualsize;
	if (MiscVirtualsize == sizeOfRawData)
		return sizeOfRawData;
	return 0;
}

// 清空数据目录等数据
BOOL CompressionData::CleanDirectData(const char* NewAddress, const DWORD & CompresSize, const DWORD & Size)
{
	if ((fpFile = fopen(g_CombatShellDataLocalFile, "ab+")) == NULL)
	{
		AfxMessageBox(L"文件打开失败");
		return false;
	}
#ifdef _WIN64
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(((PIMAGE_DOS_HEADER)NewAddress)->e_lfanew + (DWORD64)NewAddress);
#else
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(((PIMAGE_DOS_HEADER)NewAddress)->e_lfanew + (DWORD)NewAddress);
#endif

	PIMAGE_DATA_DIRECTORY pDirectory = (PIMAGE_DATA_DIRECTORY)pNt->OptionalHeader.DataDirectory;
	if (!pDirectory)
		return false;

	// Save LOAD_CONFIG fields before zeroing directories.
	// The PE loader on x64 Windows 10+ rejects the image if
	// IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG is absent for binaries
	// compiled with security features.  CopyStud will write a
	// minimal LOAD_CONFIG stub into .VMP spare space.
	// NOTE: already populated from CompressSectionData before
	// the section data was rearranged.

	DWORD dwSectionCount = pNt->FileHeader.NumberOfSections;
	g_stu->s_SectionCount = dwSectionCount;
	int k = 0;
	// 保存\清空数据目录表
	for (DWORD i = 0; i < 16; ++i)
	{
		memcpy(&g_stu->s_DataDirectory[i][0], &pDirectory->VirtualAddress, sizeof(DWORD));
		memcpy(&g_stu->s_DataDirectory[i][1], &pDirectory->Size, sizeof(DWORD));
		//fprintf(fpFile, "%x %x", pDirectory->VirtualAddress, pDirectory->Size);
		fwrite(&pDirectory->VirtualAddress, sizeof(DWORD), 1, fpFile);
		fwrite(&pDirectory->Size, sizeof(DWORD), 1, fpFile);
		fflush(fpFile);
		pDirectory->VirtualAddress = 0;
		pDirectory->Size = 0;
		++pDirectory;
	}

	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	if (!pSection)
		return false;
	// 保存\清空区段文件大小及文件偏移
	for (DWORD i = 0; i < dwSectionCount - 2; ++i)
	{
		memcpy(&g_stu->s_SectionOffsetAndSize[i][0], &pSection->SizeOfRawData, sizeof(DWORD));
		memcpy(&g_stu->s_SectionOffsetAndSize[i][1], &pSection->PointerToRawData, sizeof(DWORD));
		fwrite(&pSection->SizeOfRawData, sizeof(DWORD), 1, fpFile);
		fwrite(&pSection->PointerToRawData, sizeof(DWORD), 1, fpFile);
		fflush(fpFile);
		pSection->SizeOfRawData = 0;
		pSection->PointerToRawData = 0;
		++pSection;
	}

	// 最后一个区段是壳区段 信息不变 修改文件偏移 - 改变文件偏移对齐后文件偏移的地方
	pSection->PointerToRawData = Size;
	if (fpFile)
		fclose(fpFile);
	return 0;
}
