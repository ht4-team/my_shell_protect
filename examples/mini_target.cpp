#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

static int RunSmokeMode(int argc, LPWSTR* argv) {
    LPCWSTR outPath = L"mini_target_smoke.txt";
    for (int i = 1; i < argc; ++i) {
        if (lstrcmpiW(argv[i], L"--out") == 0 && i + 1 < argc) {
            outPath = argv[++i];
        }
    }

    HANDLE hFile = CreateFileW(
        outPath,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        return 2;
    }

    const char payload[] = "mini-target-smoke-ok\n";
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, payload, (DWORD)(sizeof(payload) - 1), &written, NULL);
    CloseHandle(hFile);
    if (!ok || written != sizeof(payload) - 1) {
        return 3;
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc <= 1) {
        if (argv) {
            LocalFree(argv);
        }
        return 0;
    }

    int ret = 0;
    bool smoke = false;
    for (int i = 1; i < argc; ++i) {
        if (lstrcmpiW(argv[i], L"--smoke") == 0) {
            smoke = true;
            break;
        }
    }
    if (smoke) {
        ret = RunSmokeMode(argc, argv);
    }

    LocalFree(argv);
    return ret;
}
