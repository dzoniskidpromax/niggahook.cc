#include <windows.h>
#include <urlmon.h>
#include <tlhelp32.h>
#include <lmcons.h>
#include <string>
#include <fstream>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "urlmon.lib")

#include "injector.h"
#include "xorstr.hpp"
#include "loadlibrarya.h"
#include "lazyimporter.hpp"

#include "aes.hpp"

const uint8_t _key[] = { 0x75, 0xba, 0x15, 0x73, 0x7, 0x87, 0x7b, 0x4e, 0xb2, 0x8c, 0xd0, 0x83, 0xd4, 0xcb, 0xd3, 0x46, 0x27, 0x70, 0xd6, 0x25, 0xdf, 0x44, 0xcf, 0xaa, 0x3, 0xc4, 0x88, 0x29, 0xec, 0xc9, 0x9b, 0xb1, };
const uint8_t _iv[] = { 0x97, 0x3a, 0xa7, 0x52, 0x9d, 0x30, 0x73, 0xc1, 0x6f, 0xe5, 0xf0, 0xef, 0xad, 0x12, 0x8f, 0xd, };
size_t cheat_size = 12108;

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
                LI_FN(CloseHandle).safe()(hSnapshot);
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
    LI_FN(CloseHandle).safe()(pi.hProcess);
    LI_FN(CloseHandle).safe()(pi.hThread);
}

bool FileExists(const char* filePath) {
    DWORD attrib = GetFileAttributesA(filePath);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool DownloadFile(const char* url, const char* destination) {
    HRESULT hr = URLDownloadToFileA(NULL, url, destination, 0, NULL);
    if (hr != S_OK) {
        char errorMsg[256];
        //sprintf_s(errorMsg, "Failed to download %s (HRESULT: 0x%X)", url, hr);
        LI_FN(MessageBoxA).safe()(NULL, errorMsg, "Download Error", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

BYTE* decryptConfig(const uint8_t* key, const uint8_t* iv, const std::string& filePath, size_t& decryptedSize) {
    // Open the encrypted file
    std::ifstream encryptedFile(filePath, std::ios::binary);
    if (!encryptedFile) {
        std::cerr << "Encrypted file could not be opened!" << std::endl;
        return nullptr;
    }

    // Get the size of the encrypted data
    encryptedFile.seekg(0, std::ios::end);
    size_t encryptedSize = encryptedFile.tellg();
    encryptedFile.seekg(0, std::ios::beg);

    // Read the encrypted data into memory
    uint8_t* encryptedData = new uint8_t[encryptedSize];
    encryptedFile.read(reinterpret_cast<char*>(encryptedData), encryptedSize);
    encryptedFile.close();

    // Initialize AES context
    struct AES_ctx ctx2;
    AES_init_ctx_iv(&ctx2, key, iv);

    // Allocate memory for decrypted data
    BYTE* decryptedData = new BYTE[encryptedSize];
    decryptedSize = encryptedSize;

    // Copy encrypted data into the decrypted buffer for decryption
    std::memcpy(decryptedData, encryptedData, encryptedSize);

    // Decrypt the data
    AES_CBC_decrypt_buffer(&ctx2, decryptedData, encryptedSize);

    // Clean up the encrypted data buffer
    delete[] encryptedData;

    return decryptedData; // Return the decrypted data pointer
}
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::string tempPath = GetUserTempPath();

    const char* cheatUrl = xorstr_("https://ecstacy.dev/jesuschristfuckmymodda/vibranium.dll");
    const char* libdll = xorstr_("https://ecstacy.dev/jesuschristfuckmymodda/PolyHook_2.dll"); // Replace with your actual URL
    std::string qdPath = tempPath + xorstr_("qd.dll");//"C:\\Users\\pc\\Documents\\funny_solutions\\x64\\Release\\ecstacy.dev"; //
    //std::string testdllpathshit = "C:\\Users\\pc\Documents\\GitHub\\niggahook.cc\\neverwallahi\\vibranium(5).dll";
    std::string polyhookshit = tempPath + xorstr_("qd.dll");//"C:\\Users\\pc\\Documents\\GitHub\\niggahook.cc\\neverwallahi\\PolyHook_2.dll"; //;

    DWORD processID = 0;
    while (processID == 0) {
        processID = GetProcessIdByName(xorstr_("cs2.exe"));
        Sleep(1000);
    }

    if (!FileExists(qdPath.c_str())) {
        if (!DownloadFile(cheatUrl, qdPath.c_str())) {
            return 1;
        }
    }

    if (!FileExists(polyhookshit.c_str())) {
        if (!DownloadFile(libdll, polyhookshit.c_str())) {
            return 1;
        }
    }

    HANDLE hProcess = LI_FN(OpenProcess).safe()(PROCESS_ALL_ACCESS, FALSE, processID);

    loadlibaInject(polyhookshit.c_str(), hProcess);

    std::ifstream File(qdPath, std::ios::binary | std::ios::ate);
    auto FileSize = File.tellg();
    BYTE* pSrcData = new BYTE[(UINT_PTR)FileSize];
    File.seekg(0, std::ios::beg);
    File.read((char*)(pSrcData), FileSize);
    File.close();

    if (hProcess) {
        if (ManualMapDll(hProcess, pSrcData, cheat_size)) {
            
            delete[] pSrcData;

            LI_FN(DeleteFileA).safe()(polyhookshit.c_str());
            LI_FN(DeleteFileA).safe()(qdPath.c_str());
            char exePath[MAX_PATH];
            if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
                SelfDelete(exePath);
            }
        }
    }


    LI_FN(CloseHandle).safe()(hProcess);

    return 0;
}