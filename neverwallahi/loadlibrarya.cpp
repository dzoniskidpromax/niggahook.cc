#include "loadlibrarya.h"
#include "xorstr.hpp"

bool loadlibaInject(const char* path, HANDLE hTargetProcess) {
    const LPVOID lpPathAddress = VirtualAllocEx(hTargetProcess, nullptr, lstrlenA(path) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (lpPathAddress == nullptr)
    {
        return -1;
    }

    const DWORD dwWriteResult = WriteProcessMemory(hTargetProcess, lpPathAddress, path, lstrlenA(path) + 1, nullptr);
    if (dwWriteResult == 0)
    {
        return -1;
    }


    const HMODULE hModule = GetModuleHandleA(xorstr_("kernel32.dll"));
    if (hModule == INVALID_HANDLE_VALUE || hModule == nullptr)
        return -1;

    const FARPROC lpFunctionAddress = GetProcAddress(hModule, xorstr_("LoadLibraryA"));
    if (lpFunctionAddress == nullptr)
    {
        return -1;
    }


    const HANDLE hThreadCreationResult = CreateRemoteThread(hTargetProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)lpFunctionAddress, lpPathAddress, 0, nullptr);
    if (hThreadCreationResult == INVALID_HANDLE_VALUE)
    {
        return -1;
    }
}