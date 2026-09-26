#include "pch.h"
#include "path_utils.h"

namespace DL2HT {

// Static address used to identify our module
static void DummyAddress() {}

std::string GetModuleDirectory() {
    char modulePath[MAX_PATH];
    HMODULE hModule = nullptr;

    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&DummyAddress),
        &hModule
    );

    if (hModule == nullptr) {
        return "";
    }

    if (GetModuleFileNameA(hModule, modulePath, MAX_PATH) == 0) {
        return "";
    }

    std::string path(modulePath);
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        return path.substr(0, lastSlash + 1);
    }

    return "";
}

std::string GetModulePath(const char* filename) {
    std::string dir = GetModuleDirectory();
    if (dir.empty()) {
        return filename;  // Fall back to just the filename
    }
    return dir + filename;
}

std::wstring GetModuleDirectoryW() {
    HMODULE hModule = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&DummyAddress),
            &hModule) || hModule == nullptr) {
        return L"";
    }

    DWORD bufSize = MAX_PATH;
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::vector<wchar_t> buf(bufSize);
        SetLastError(0);
        const DWORD ret = GetModuleFileNameW(hModule, buf.data(), bufSize);
        if (ret == 0) return L"";
        if (ret < bufSize && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            const std::wstring path(buf.data(), ret);
            const size_t lastSlash = path.find_last_of(L"\\/");
            if (lastSlash == std::wstring::npos) return L"";
            return path.substr(0, lastSlash + 1);
        }
        bufSize *= 2;
    }
    return L"";
}

} // namespace DL2HT
