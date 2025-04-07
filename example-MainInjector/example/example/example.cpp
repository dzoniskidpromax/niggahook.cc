#include <windows.h>
#include <tlhelp32.h>
#include <lmcons.h>
#include <string>
#include <fstream>
#pragma comment(lib, "user32.lib")

std::string GetUserTempPath() {
    char username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    GetUserNameA(username, &username_len);
    return std::string("C:\\Users\\") + username + "\\AppData\\Local\\Temp\\";
}

DWORD GetProcessIdByName(const char* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, L"cs2.exe") == 0) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return 0;
}

bool ManualMapDLL(HANDLE hProcess, const char* dllPath) {
    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file) return false;

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    char* dllData = new char[fileSize];
    file.read(dllData, fileSize);
    file.close();

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)dllData;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        delete[] dllData;
        return false;
    }

    if (dosHeader->e_lfanew >= fileSize || dosHeader->e_lfanew < sizeof(IMAGE_DOS_HEADER)) {
        delete[] dllData;
        return false;
    }

    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(dllData + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        delete[] dllData;
        return false;
    }

    LPVOID remoteMemory = VirtualAllocEx(hProcess, NULL, ntHeaders->OptionalHeader.SizeOfImage,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteMemory) {
        delete[] dllData;
        return false;
    }

    IMAGE_SECTION_HEADER* sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        LPVOID sectionDest = (LPVOID)((DWORD_PTR)remoteMemory + sectionHeader[i].VirtualAddress);
        LPVOID sectionSrc = dllData + sectionHeader[i].PointerToRawData;
        WriteProcessMemory(hProcess, sectionDest, sectionSrc, sectionHeader[i].SizeOfRawData, NULL);
    }

    LPVOID imageBase = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!imageBase) {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        delete[] dllData;
        return false;
    }

    WriteProcessMemory(hProcess, imageBase, dllData, 4096, NULL);

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
        delete[] dllData;
        return false;
    }

    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");
    if (!loadLibraryAddr) {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
        delete[] dllData;
        return false;
    }

    unsigned char shellcode[] = {
        0x68, 0x00, 0x00, 0x00, 0x00,
        0xB8, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xD0,
        0xC3
    };

    *(DWORD_PTR*)(shellcode + 1) = (DWORD_PTR)remoteMemory;
    *(DWORD_PTR*)(shellcode + 6) = (DWORD_PTR)loadLibraryAddr;

    LPVOID shellcodeMemory = VirtualAllocEx(hProcess, NULL, sizeof(shellcode),
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!shellcodeMemory) {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
        delete[] dllData;
        return false;
    }

    WriteProcessMemory(hProcess, shellcodeMemory, shellcode, sizeof(shellcode), NULL);

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)shellcodeMemory, NULL, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, shellcodeMemory, 0, MEM_RELEASE);
        delete[] dllData;
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, shellcodeMemory, 0, MEM_RELEASE);
    CloseHandle(hThread);
    delete[] dllData;

    return true;
}

void SelfDelete(const char* exePath) {
    char cmd[MAX_PATH + 20];
    sprintf_s(cmd, "cmd.exe /C del /F /Q \"%s\"", exePath);
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

#pragma warning(suppress: 28251)
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::string tempPath = GetUserTempPath();
    std::string qdPath = tempPath + "qd.dll";
    std::string win64Path = tempPath + "win64dialog.exe";
    std::string selfPath = tempPath + "example.exe";

    DWORD processID = 0;
    while (processID == 0) {
        processID = GetProcessIdByName("cs2.exe");
        Sleep(1000);
    }

    Sleep(30000);

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (hProcess) {
        if (ManualMapDLL(hProcess, qdPath.c_str())) {
            DeleteFileA(win64Path.c_str());
            DeleteFileA(qdPath.c_str());
            SelfDelete(selfPath.c_str());
        }
        CloseHandle(hProcess);
    }

    return 0;
}