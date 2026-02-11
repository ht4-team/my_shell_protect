#include "stdafx.h"

#include "AddSection.h"
#include "CompressionData.h"
#include "UnShell.h"
#include "studData.h"

#include <stdio.h>

CString UnShllerProcPath;
char g_CombatShellDataLocalFile[MAX_PATH] = { 0 };

namespace {
constexpr const char* kNewSectionName = ".VMP";

void PrintUsage() {
	wprintf(
		L"Usage:\n"
		L"  CombatShellCli.exe pack <target.exe>\n"
		L"  CombatShellCli.exe unpack <target.exe>\n"
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
		GENERIC_READ | GENERIC_WRITE,
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

bool RunPack(const CString& inputPath) {
	if (!BuildCombatDataFilePath(inputPath)) {
		return false;
	}

	CString fileName = inputPath;
	const int slashPos = fileName.ReverseFind('\\') + 1;
	const CString targetDirectory = fileName.Left(slashPos);
	fileName = fileName.Mid(slashPos);

	// Backup original executable before patching sections.
	CopyFile(inputPath, targetDirectory + L"old_" + fileName, FALSE);

	DWORD oldOep = 0;
	if (!AddNewSectionAndUpdateOep(inputPath, oldOep)) {
		fprintf(stderr, "add section failed\n");
		return false;
	}

	CompressionData compressor;
	compressor.puInit(inputPath);
	if (!compressor.puCompressSection()) {
		fprintf(stderr, "compress section failed\n");
		return false;
	}

	CString compressionMask = targetDirectory + L"CompressionMask.exe";
	if (!SingleStudData::instance()->puInit(compressionMask, oldOep)) {
		fprintf(stderr, "stud init failed\n");
		return false;
	}
	SingleStudData::instance()->puLoadLibraryStud();
	SingleStudData::instance()->puRepairReloCationStud();
	const bool copyOk = SingleStudData::instance()->puCopyStud() == TRUE;
	SingleStudData::instance()->puClearStuData();
	if (!copyOk) {
		fprintf(stderr, "copy shell payload failed\n");
		return false;
	}

	DeleteFile(inputPath);
	if (!CopyFile(compressionMask, inputPath, FALSE)) {
		fprintf(stderr, "replace target failed\n");
		return false;
	}
	DeleteFile(compressionMask);
	return true;
}

bool RunUnpack(const CString& inputPath) {
	if (!BuildCombatDataFilePath(inputPath)) {
		return false;
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
} // namespace

int wmain(int argc, wchar_t* argv[]) {
	if (argc < 3) {
		PrintUsage();
		return 2;
	}

	const wchar_t* command = argv[1];
	const wchar_t* target = argv[2];
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
