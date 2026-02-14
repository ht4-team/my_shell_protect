#include <windows.h>
#include <wchar.h>

static int RunSmokeMode() {
    LPCWSTR outPath = L"mini_target_smoke.txt";

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
    LPCWSTR cmd = GetCommandLineW();
    if (cmd && wcsstr(cmd, L"--smoke")) {
        return RunSmokeMode();
    }
    return 0;
}
