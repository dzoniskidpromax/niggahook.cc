#include <windows.h>
#include <tlhelp32.h>
#include <lmcons.h>
#include <string>
#include <fstream>
#pragma comment(lib, "user32.lib")

#include "injector.h"
#include "xorstr.hpp"

std::string GetUserTempPath() {
    char username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    GetUserNameA(username, &username_len);
    return std::string(xorstr_("C:\\Users\\")) + username + xorstr_("\\AppData\\Local\\Temp\\");
}

DWORD GetProcessIdByName(const char* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (_stricmp(pe32.szExeFile, processName) == 0) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return 0;
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

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::string tempPath = GetUserTempPath();
    std::string qdPath = tempPath + xorstr_("qd.dll");
    std::string win64Path = tempPath + xorstr_("win64dialog.exe");
    std::string selfPath = tempPath + xorstr_("example.exe");

    std::ifstream File(qdPath, std::ios::binary | std::ios::ate);
    auto FileSize = File.tellg();
    BYTE* pSrcData = new BYTE[(UINT_PTR)FileSize];
    File.seekg(0, std::ios::beg);
    File.read((char*)(pSrcData), FileSize);
    File.close();

    DWORD processID = 0;
    while (processID == 0) {
        processID = GetProcessIdByName(xorstr_("cs2.exe"));
        Sleep(1000);
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (hProcess) {
        if (ManualMapDll(hProcess, pSrcData, FileSize)) {
            CloseHandle(hProcess);
            delete[] pSrcData;

            DeleteFileA(win64Path.c_str());
            DeleteFileA(qdPath.c_str());
            SelfDelete(selfPath.c_str());
        }
    }

    return 0;
}