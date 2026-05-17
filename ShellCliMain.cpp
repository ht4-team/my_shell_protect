#include "stdafx.h"

#include "AddSection.h"
#include "CompressionData.h"
#include "UnShell.h"
#include "puPEinfoData.h"
#include "studData.h"
#include "CombatShell/CombatShell.h"

#include <stdio.h>

CString UnShllerProcPath;
char g_CombatShellDataLocalFile[MAX_PATH] = { 0 };
extern bool g_DebugMode;
extern DWORD g_CompressionMethod;
extern DWORD g_ProtectionFlags;
extern DWORD g_EncryptionKey;
extern _Stud* g_stu;
extern _VmNode* g_Vm;
extern char* g_dataHlpers;

namespace {
constexpr const char* kNewSectionName = ".VMP";
constexpr const char* kCompatMarker = "COMBATSHELL_TRAMP_SHELL\n";

bool IsLegacyModeEnabled();
bool WriteCompatMarker();
bool IsCompatMarkerFile();
bool WriteTrampolineShell(const CString& path, DWORD oldOep);
bool FileExists(const wchar_t* path);
WORD GetTargetMachine(const CString& path);
bool FixPeChecksum(const CString& path);
bool ParseCompressionOption(const wchar_t* value, DWORD& method);
bool ParseByteOption(const wchar_t* value, DWORD& out);
const wchar_t* CompressionMethodName(DWORD method);

void PrintUsage() {
	wprintf(
		L"Usage:\n"
		L"  CombatShellCli.exe pack <target.exe> [--debug] [--compress=<quicklz|lz4|none>]\n"
		L"                         [--vm|--no-vm] [--encrypt-sections] [--xor-key=<hex|dec>]\n"
		L"  CombatShellCli.exe unpack <target.exe>\n"
		L"\\n"
		L"Options:\n"
		L"  --debug    Print VM instruction and packed section diagnostics\n"
		L"  --compress Select section payload codec: quicklz, lz4, or none\n"
		L"  --no-compress Alias for --compress=none\n"
		L"  --vm / --no-vm Enable or disable x64 entry VM virtualization\n"
		L"  --encrypt-sections XOR-obfuscate packed section payload before embedding\n"
		L"  --xor-key Set XOR byte key for --encrypt-sections, default 0x5A\n"
		L"\\n"
		L"Notes:\n"
		L"  - Keep CombatShell.dll in the same directory as the executable.\n"
		L"  - A data file '<target>_CombatShellData.dat' is created during packing.\n");
}

bool BuildCombatDataFilePath(const CString& targetPath) {
	CString fileName = targetPath;
	const int slashPos = fileName.ReverseFind('\\') + 1;
	if (slashPos <= 0) {
		fprintf(stderr, "invalid target path\n");
		return false;
	}

	const CString directory = fileName.Left(slashPos);
	fileName = fileName.Mid(slashPos);

	int extPos = fileName.ReverseFind('.');
	if (extPos <= 0) {
		extPos = fileName.GetLength();
	}
	const CString stem = fileName.Left(extPos);
	const CString fullStem = directory + stem;

	RtlSecureZeroMemory(g_CombatShellDataLocalFile, sizeof(g_CombatShellDataLocalFile));
	const int bytes = WideCharToMultiByte(
		CP_OEMCP,
		0,
		fullStem,
		-1,
		g_CombatShellDataLocalFile,
		static_cast<int>(sizeof(g_CombatShellDataLocalFile)),
		nullptr,
		nullptr);

	if (bytes <= 0 || bytes >= static_cast<int>(sizeof(g_CombatShellDataLocalFile))) {
		fprintf(stderr, "failed to build data file path\n");
		return false;
	}

	const size_t used = strlen(g_CombatShellDataLocalFile);
	const char* suffix = "_CombatShellData.dat";
	if (used + strlen(suffix) + 1 >= sizeof(g_CombatShellDataLocalFile)) {
		fprintf(stderr, "data file path too long\n");
		return false;
	}
	strcat(g_CombatShellDataLocalFile, suffix);
	return true;
}

bool AddNewSectionAndUpdateOep(const CString& targetPath, DWORD& oldOep) {
	std::string currentDir;
	if (!CodeTool::CGetCurrentDirectory(currentDir) || currentDir.empty()) {
		fprintf(stderr, "failed to get current directory\n");
		return false;
	}

	const std::string stubPath = currentDir + "CombatShell.dll";
	const HANDLE hFile = CreateFileA(
		stubPath.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "CombatShell.dll not found: %s\n", stubPath.c_str());
		return false;
	}

	DWORD sectionSize = GetFileSize(hFile, nullptr);
	CloseHandle(hFile);
	if (sectionSize == INVALID_FILE_SIZE || sectionSize == 0) {
		fprintf(stderr, "invalid CombatShell.dll size\n");
		return false;
	}

	SingleAddSection::instance()->puInti(targetPath);
	oldOep = static_cast<DWORD>(SinglePuPEInfo::instance()->puGetOEP());
	SingleAddSection::instance()->puModifySectioNumber();
	if (!SingleAddSection::instance()->puModifySectionInfo((BYTE*)kNewSectionName, sectionSize)) {
		SingleAddSection::instance()->puFree();
		return false;
	}
	SingleAddSection::instance()->puModifyProgramEntryPoint();
	SingleAddSection::instance()->puModifySizeofImage();
	const BOOL ok = SingleAddSection::instance()->puAddNewSectionByData(sectionSize);
	SingleAddSection::instance()->puFree();
	return ok == TRUE;
}

void PrintDebugPackInfo() {
	if (!g_DebugMode)
		return;

	fprintf(stderr, "\n[debug] === VM Instruction Virtualization ===\n");
	if (g_Vm && g_Vm->VmCount > 0 && g_Vm->Vmencodeasmlen > 0) {
		fprintf(stderr, "[debug] VM segments: %u, instructions: %u, code RVA offset: 0x%X\n",
			g_Vm->VmCount, g_Vm->Vmencodeasmlen, g_Vm->VmAddroffset);
		fprintf(stderr, "[debug] metadata RVA offset: 0x%X\n", g_Vm->Hlperdataoffset);

		if (g_dataHlpers) {
			ArrayHlerp* hlp = (ArrayHlerp*)g_dataHlpers;
			fprintf(stderr, "[debug]  %-4s %-10s %-10s %-6s %-6s %s\n",
				"#", "offset", "size", "xor", "enc", "mnemonic");
			for (unsigned int i = 0; i < g_Vm->Vmencodeasmlen; ++i) {
				fprintf(stderr, "[debug]  %-4u 0x%08X %-10u 0x%04X %-6u %s\n",
					i, hlp[i].startoffset, hlp[i].bytesize,
					hlp[i].xorKey, hlp[i].encodeflag, hlp[i].mnemonic);
			}
		}
	} else {
		fprintf(stderr, "[debug] VM: disabled (VmCount=0)\n");
	}

	fprintf(stderr, "\n[debug] === Packed Sections ===\n");
	if (g_stu) {
		fprintf(stderr, "[debug] compression: %ls (%u)\n",
			CompressionMethodName(g_stu->s_CompressionMethod),
			g_stu->s_CompressionMethod);
		fprintf(stderr, "[debug] protection flags: 0x%08X, xorKey: 0x%02X\n",
			g_stu->s_ProtectionFlags,
			g_stu->s_EncryptionKey & 0xFF);
		fprintf(stderr, "[debug] OEP RVA: 0x%llX\n", (unsigned long long)g_stu->s_dwOepBase);
		fprintf(stderr, "[debug] section count (original): %llu\n",
			(unsigned long long)g_stu->s_SectionCount);
		fprintf(stderr, "[debug] compression section RVA: 0x%llX\n",
			(unsigned long long)g_stu->s_CompressionSectionRva);
		fprintf(stderr, "[debug]  %-4s %-12s %-12s\n", "#", "rawSize", "compressLen");
		for (DWORD i = 0; i + 2 < (DWORD)g_stu->s_SectionCount; ++i) {
			fprintf(stderr, "[debug]  %-4u 0x%08X   0x%08X\n",
				i, g_stu->s_SectionOffsetAndSize[i][0], g_stu->s_blen[i]);
		}
	}
	fprintf(stderr, "\n");
	fflush(stderr);
}

bool RunCompatPack(const CString& inputPath) {
	DWORD oldOep = 0;
	if (!AddNewSectionAndUpdateOep(inputPath, oldOep)) {
		fprintf(stderr, "add section failed in tramp mode\n");
		return false;
	}
	if (!WriteTrampolineShell(inputPath, oldOep)) {
		fprintf(stderr, "write trampoline shell failed\n");
		return false;
	}
	if (!WriteCompatMarker()) {
		fprintf(stderr, "failed to write tramp marker\n");
		return false;
	}
	return true;
}

bool RunLegacyPackCore(const CString& inputPath, const CString& targetDirectory) {
	fprintf(stderr, "[legacy] step=add-section\n");
	fflush(stderr);
	DWORD oldOep = 0;
	if (!AddNewSectionAndUpdateOep(inputPath, oldOep)) {
		fprintf(stderr, "add section failed\n");
		fflush(stderr);
		return false;
	}

	fprintf(stderr, "[legacy] step=compress\n");
	fprintf(stderr, "[legacy] compression=%ls protection=0x%08X xorKey=0x%02X\n",
		CompressionMethodName(g_CompressionMethod),
		g_ProtectionFlags,
		g_EncryptionKey & 0xFF);
	fflush(stderr);
	CompressionData compressor;
	compressor.puInit(inputPath);
	if (!compressor.puCompressSection()) {
		fprintf(stderr, "compress section failed\n");
		fflush(stderr);
		return false;
	}

	fprintf(stderr, "[legacy] step=stud-init\n");
	fflush(stderr);
	CString compressionMask = targetDirectory + L"CompressionMask.exe";
	if (!SingleStudData::instance()->puInit(compressionMask, oldOep)) {
		fprintf(stderr, "stud init failed\n");
		fflush(stderr);
		return false;
	}
	fprintf(stderr, "[legacy] step=stud-copy\n");
	fflush(stderr);
	SingleStudData::instance()->puLoadLibraryStud();
	SingleStudData::instance()->puRepairReloCationStud();
	const bool copyOk = SingleStudData::instance()->puCopyStud() == TRUE;
	SingleStudData::instance()->puClearStuData();
	if (!copyOk) {
		fprintf(stderr, "copy shell payload failed\n");
		fflush(stderr);
		return false;
	}

	PrintDebugPackInfo();

	fprintf(stderr, "[legacy] step=finalize\n");
	fflush(stderr);
	DeleteFile(inputPath);
	if (!CopyFile(compressionMask, inputPath, FALSE)) {
		fprintf(stderr, "replace target failed\n");
		fflush(stderr);
		return false;
	}
	DeleteFile(compressionMask);

	fprintf(stderr, "[legacy] step=fix-checksum\n");
	fflush(stderr);
	if (!FixPeChecksum(inputPath)) {
		fprintf(stderr, "warning: fix checksum failed (packed file may still work)\n");
		fflush(stderr);
	}
	return true;
}

bool RunLegacyPackNoCrash(const CString& inputPath, const CString& targetDirectory) {
	__try {
		return RunLegacyPackCore(inputPath, targetDirectory);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		fprintf(stderr, "legacy pack crashed (SEH), falling back\n");
		return false;
	}
}

bool RunPack(const CString& inputPath) {
	if (!BuildCombatDataFilePath(inputPath)) {
		return false;
	}

	CString fileName = inputPath;
	const int slashPos = fileName.ReverseFind('\\') + 1;
	const CString targetDirectory = fileName.Left(slashPos);
	fileName = fileName.Mid(slashPos);
	const CString backupPath = targetDirectory + L"old_" + fileName;

	const WORD machine = GetTargetMachine(inputPath);
#ifdef _WIN64
	if (machine != IMAGE_FILE_MACHINE_AMD64) {
		fprintf(stderr, "unsupported target machine for this build: 0x%04X (x64 build expects AMD64)\n", machine);
		return false;
	}
#else
	if (machine != IMAGE_FILE_MACHINE_I386) {
		fprintf(stderr, "unsupported target machine for this build: 0x%04X (Win32 build expects I386)\n", machine);
		return false;
	}
#endif

	// Backup original executable.
	CopyFile(inputPath, backupPath, FALSE);

	bool useLegacy = IsLegacyModeEnabled();
	if (!useLegacy) {
		// x64 targets prefer real shell flow by default.
		if (machine == IMAGE_FILE_MACHINE_AMD64) {
			useLegacy = true;
		}
	}

	// Safe trampoline mode for non-legacy flow.
	if (!useLegacy) {
		return RunCompatPack(inputPath);
	}

	if (RunLegacyPackNoCrash(inputPath, targetDirectory)) {
		return true;
	}

	// Do not auto-switch to trampoline mode after legacy failure.
	// Restore original file and report failure instead.
	if (FileExists(backupPath)) {
		DeleteFile(inputPath);
		CopyFile(backupPath, inputPath, FALSE);
	}
	fprintf(stderr, "legacy pack failed\n");
	return false;
}

bool RunUnpack(const CString& inputPath) {
	if (!BuildCombatDataFilePath(inputPath)) {
		return false;
	}

	CString fileName = inputPath;
	const int slashPos = fileName.ReverseFind('\\') + 1;
	const CString targetDirectory = fileName.Left(slashPos);
	fileName = fileName.Mid(slashPos);
	const CString backupPath = targetDirectory + L"old_" + fileName;

	// Prefer backup-restore for stability in both modes.
	if (FileExists(backupPath)) {
		DeleteFile(inputPath);
		if (!CopyFile(backupPath, inputPath, FALSE)) {
			return false;
		}
		DeleteFile(backupPath);
		DeleteFileA(g_CombatShellDataLocalFile);
		return true;
	}

	if (!IsLegacyModeEnabled()) {
		if (IsCompatMarkerFile()) {
			DeleteFileA(g_CombatShellDataLocalFile);
		}
		return true;
	}

	UnShllerProcPath = inputPath;
	UnShell unshell;
	if (!unshell.puUnShell()) {
		fprintf(stderr, "unshell init failed\n");
		return false;
	}
	if (!unshell.puRepCompressionData()) {
		fprintf(stderr, "recover compressed data failed\n");
		return false;
	}
	if (!unshell.puDeleteSectionInfo()) {
		fprintf(stderr, "delete section metadata failed\n");
		return false;
	}
	if (!unshell.puSaveUnShell()) {
		fprintf(stderr, "save unshelled file failed\n");
		return false;
	}

	const std::wstring unshellPath = CodeTool::string2wstring(unshell.puGetUnShellPath()).c_str();
	unshell.puClose();
	DeleteFile(inputPath);
	if (!CopyFile(unshellPath.c_str(), inputPath, FALSE)) {
		fprintf(stderr, "replace target failed\n");
		return false;
	}
	DeleteFile(unshellPath.c_str());
	DeleteFileA(g_CombatShellDataLocalFile);
	return true;
}

bool FileExists(const wchar_t* path) {
	const DWORD attr = GetFileAttributesW(path);
	return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

WORD GetTargetMachine(const CString& path) {
	if (!SinglePuPEInfo::instance()->puOpenFileLoadEx(path)) {
		return 0;
	}
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)SinglePuPEInfo::instance()->puGetNtHeadre();
	if (!pNt) {
		SinglePuPEInfo::instance()->puClearPeData();
		return 0;
	}
	const WORD machine = pNt->FileHeader.Machine;
	SinglePuPEInfo::instance()->puClearPeData();
	return machine;
}

bool IsLegacyModeEnabled() {
	wchar_t value[8] = { 0 };
	const DWORD len = GetEnvironmentVariableW(L"COMBATSHELL_LEGACY", value, _countof(value));
	// Default to real shell mode. Set COMBATSHELL_LEGACY=0/false to force compat path.
	if (len == 0 || len >= _countof(value)) {
		return true;
	}
	if ((_wcsicmp(value, L"0") == 0) || (_wcsicmp(value, L"false") == 0)) {
		return false;
	}
	return true;
}

bool WriteCompatMarker() {
	FILE* fp = fopen(g_CombatShellDataLocalFile, "wb");
	if (!fp) {
		return false;
	}
	fwrite(kCompatMarker, 1, strlen(kCompatMarker), fp);
	fclose(fp);
	return true;
}

bool IsCompatMarkerFile() {
	FILE* fp = fopen(g_CombatShellDataLocalFile, "rb");
	if (!fp) {
		return false;
	}
	char buf[64] = { 0 };
	const size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
	fclose(fp);
	if (n == 0) {
		return false;
	}
	return strstr(buf, kCompatMarker) != nullptr;
}

bool ParseCompressionOption(const wchar_t* value, DWORD& method) {
	if (!value || !*value) {
		return false;
	}
	if (_wcsicmp(value, L"quicklz") == 0 || _wcsicmp(value, L"qlz") == 0) {
		method = COMBATSHELL_COMPRESS_QUICKLZ;
		return true;
	}
	if (_wcsicmp(value, L"lz4") == 0) {
		method = COMBATSHELL_COMPRESS_LZ4;
		return true;
	}
	if (_wcsicmp(value, L"none") == 0 || _wcsicmp(value, L"store") == 0 || _wcsicmp(value, L"raw") == 0) {
		method = COMBATSHELL_COMPRESS_NONE;
		return true;
	}
	return false;
}

bool ParseByteOption(const wchar_t* value, DWORD& out) {
	if (!value || !*value) {
		return false;
	}
	wchar_t* end = nullptr;
	const unsigned long parsed = wcstoul(value, &end, 0);
	if (!end || *end != L'\0' || parsed > 0xFF) {
		return false;
	}
	out = (DWORD)parsed;
	return true;
}

const wchar_t* CompressionMethodName(DWORD method) {
	switch (method) {
	case COMBATSHELL_COMPRESS_QUICKLZ:
		return L"quicklz";
	case COMBATSHELL_COMPRESS_LZ4:
		return L"lz4";
	case COMBATSHELL_COMPRESS_NONE:
		return L"none";
	default:
		return L"unknown";
	}
}

bool WriteTrampolineShell(const CString& path, DWORD oldOep) {
	if (!SinglePuPEInfo::instance()->puOpenFileLoadEx(path)) {
		return false;
	}
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)SinglePuPEInfo::instance()->puGetNtHeadre();
	if (!pNt) {
		SinglePuPEInfo::instance()->puClearPeData();
		return false;
	}
	PIMAGE_SECTION_HEADER vmpSec = SinglePuPEInfo::instance()->puGetSectionAddress(
		(char*)SinglePuPEInfo::instance()->puGetImageBase(), (BYTE*)kNewSectionName);
	if (!vmpSec || vmpSec->PointerToRawData == 0 || vmpSec->SizeOfRawData < 8) {
		SinglePuPEInfo::instance()->puClearPeData();
		return false;
	}

	// E9 rel32 : jmp oldOep
	BYTE* fileBase = (BYTE*)SinglePuPEInfo::instance()->puGetImageBase();
	BYTE* stub = fileBase + vmpSec->PointerToRawData;
	const DWORD stubRva = vmpSec->VirtualAddress;
	const INT32 rel = static_cast<INT32>(oldOep - (stubRva + 5));
	stub[0] = 0xE9;
	memcpy(stub + 1, &rel, sizeof(rel));
	stub[5] = 0x90;
	stub[6] = 0x90;
	stub[7] = 0x90;

	pNt->OptionalHeader.AddressOfEntryPoint = stubRva;

	HANDLE hFile = SinglePuPEInfo::instance()->puFileHandle();
	if (!hFile || hFile == INVALID_HANDLE_VALUE) {
		SinglePuPEInfo::instance()->puClearPeData();
		return false;
	}
	SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(hFile);
	DWORD written = 0;
	const BOOL ok = WriteFile(
		hFile,
		fileBase,
		SinglePuPEInfo::instance()->puFileSize(),
		&written,
		nullptr);
	SinglePuPEInfo::instance()->puClearPeData();
	return ok == TRUE;
}

bool FixPeChecksum(const CString& path) {
	HANDLE hFile = CreateFileW(
		path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		return false;
	}

	DWORD fileSize = GetFileSize(hFile, nullptr);
	if (fileSize == INVALID_FILE_SIZE || fileSize < 0x80) {
		CloseHandle(hFile);
		return false;
	}

	HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READWRITE, 0, 0, nullptr);
	if (!hMap) {
		CloseHandle(hFile);
		return false;
	}

	BYTE* base = (BYTE*)MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
	if (!base) {
		CloseHandle(hMap);
		CloseHandle(hFile);
		return false;
	}

	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
		UnmapViewOfFile(base);
		CloseHandle(hMap);
		CloseHandle(hFile);
		return false;
	}

	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE) {
		UnmapViewOfFile(base);
		CloseHandle(hMap);
		CloseHandle(hFile);
		return false;
	}

	// Clear the Authenticode signature directory entry — modifying
	// a signed PE invalidates the signature, and a stale entry can
	// make the loader reject the file outright.
	if (nt->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_SECURITY) {
		nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress = 0;
		nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size = 0;
	}

	// Zero the existing checksum before computing the new one.
	DWORD* pCheckSum = &nt->OptionalHeader.CheckSum;
	*pCheckSum = 0;

	// Standard PE checksum: sum all WORDs with carry folding, then add file size.
	ULONGLONG sum = 0;
	const DWORD wordCount = fileSize / 2;
	const WORD* words = (const WORD*)base;
	for (DWORD i = 0; i < wordCount; ++i) {
		sum += words[i];
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	if (fileSize & 1) {
		sum += base[fileSize - 1];
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	sum = (sum & 0xFFFF) + (sum >> 16);
	*pCheckSum = (DWORD)(sum + fileSize);

	fprintf(stderr, "[legacy] checksum=0x%08X\n", *pCheckSum);

	UnmapViewOfFile(base);
	CloseHandle(hMap);
	CloseHandle(hFile);
	return true;
}
} // namespace

int wmain(int argc, wchar_t* argv[]) {
	if (argc < 3) {
		PrintUsage();
		return 2;
	}

	const wchar_t* command = argv[1];
	const wchar_t* target = argv[2];

	// Parse optional flags
	for (int i = 3; i < argc; ++i) {
		if (_wcsicmp(argv[i], L"--debug") == 0) {
			g_DebugMode = true;
		}
		else if (_wcsicmp(argv[i], L"--no-compress") == 0) {
			g_CompressionMethod = COMBATSHELL_COMPRESS_NONE;
		}
		else if (_wcsicmp(argv[i], L"--vm") == 0) {
			g_ProtectionFlags |= COMBATSHELL_PROTECT_VM_ENTRY;
		}
		else if (_wcsicmp(argv[i], L"--no-vm") == 0) {
			g_ProtectionFlags &= ~COMBATSHELL_PROTECT_VM_ENTRY;
		}
		else if (_wcsicmp(argv[i], L"--encrypt-sections") == 0) {
			g_ProtectionFlags |= COMBATSHELL_PROTECT_ENCRYPT_SECTIONS;
		}
		else if (_wcsicmp(argv[i], L"--no-encrypt-sections") == 0) {
			g_ProtectionFlags &= ~COMBATSHELL_PROTECT_ENCRYPT_SECTIONS;
		}
		else if (_wcsnicmp(argv[i], L"--xor-key=", 10) == 0) {
			DWORD key = g_EncryptionKey;
			if (!ParseByteOption(argv[i] + 10, key)) {
				fwprintf(stderr, L"invalid xor key: %ls\n", argv[i] + 10);
				PrintUsage();
				return 2;
			}
			g_EncryptionKey = key;
			g_ProtectionFlags |= COMBATSHELL_PROTECT_ENCRYPT_SECTIONS;
		}
		else if (_wcsicmp(argv[i], L"--xor-key") == 0) {
			if (i + 1 >= argc) {
				fwprintf(stderr, L"--xor-key requires a value\n");
				PrintUsage();
				return 2;
			}
			DWORD key = g_EncryptionKey;
			if (!ParseByteOption(argv[++i], key)) {
				fwprintf(stderr, L"invalid xor key: %ls\n", argv[i]);
				PrintUsage();
				return 2;
			}
			g_EncryptionKey = key;
			g_ProtectionFlags |= COMBATSHELL_PROTECT_ENCRYPT_SECTIONS;
		}
		else if (_wcsnicmp(argv[i], L"--compress=", 11) == 0) {
			DWORD method = g_CompressionMethod;
			if (!ParseCompressionOption(argv[i] + 11, method)) {
				fwprintf(stderr, L"invalid compression method: %ls\n", argv[i] + 11);
				PrintUsage();
				return 2;
			}
			g_CompressionMethod = method;
		}
		else if (_wcsicmp(argv[i], L"--compress") == 0) {
			if (i + 1 >= argc) {
				fwprintf(stderr, L"--compress requires a value\n");
				PrintUsage();
				return 2;
			}
			DWORD method = g_CompressionMethod;
			if (!ParseCompressionOption(argv[++i], method)) {
				fwprintf(stderr, L"invalid compression method: %ls\n", argv[i]);
				PrintUsage();
				return 2;
			}
			g_CompressionMethod = method;
		}
		else {
			fwprintf(stderr, L"unknown option: %ls\n", argv[i]);
			PrintUsage();
			return 2;
		}
	}

	if (!FileExists(target)) {
		fwprintf(stderr, L"target not found: %ls\n", target);
		return 2;
	}

	const CString targetPath = target;
	bool ok = false;
	if (_wcsicmp(command, L"pack") == 0) {
		ok = RunPack(targetPath);
	} else if (_wcsicmp(command, L"unpack") == 0) {
		ok = RunUnpack(targetPath);
	} else {
		PrintUsage();
		return 2;
	}

	if (!ok) {
		fwprintf(stderr, L"command failed: %ls\n", command);
		return 1;
	}

	wprintf(L"success: %ls %ls\n", command, target);
	return 0;
}
