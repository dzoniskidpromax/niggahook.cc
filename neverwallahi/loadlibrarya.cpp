#include "loadlibrarya.h"
#include "xorstr.hpp"
#include "lazyimporter.hpp"

bool loadlibaInject(const char* path, HANDLE hTargetProcess) {
    const LPVOID lpPathAddress = LI_FN(VirtualAllocEx).safe()(hTargetProcess, nullptr, lstrlenA(path) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (lpPathAddress == nullptr)
    {
        return false;
    }

    const DWORD dwWriteResult = WriteProcessMemory(hTargetProcess, lpPathAddress, path, lstrlenA(path) + 1, nullptr);
    if (dwWriteResult == 0)
    {
        return false;
    }


    const HMODULE hModule = LI_FN(GetModuleHandleA).safe()(xorstr_("kernel32.dll"));
    if (hModule == INVALID_HANDLE_VALUE || hModule == nullptr)
        return false;

    const FARPROC lpFunctionAddress = LI_FN(GetProcAddress).safe()(hModule, xorstr_("LoadLibraryA"));
    if (lpFunctionAddress == nullptr)
    {
        return false;
    }


    const HANDLE hThreadCreationResult = LI_FN(CreateRemoteThread).safe()(hTargetProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)lpFunctionAddress, lpPathAddress, 0, nullptr);
    if (hThreadCreationResult == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    return true;
}