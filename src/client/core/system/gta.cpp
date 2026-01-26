#include "gta.hpp"
#include "system/logger.hpp"
#include <shlobj.h>
#include <filesystem>
#include <thread>

void Gta::Initialize()
{
    HWND hwnd = nullptr;
    int attempts = 0;

    while ((hwnd = *reinterpret_cast<HWND*>(HwndAddress)) == nullptr && attempts++ < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!hwnd) { LOG_WARN("GTA HWND not found after 100 attempts."); return; }
    hwnd_ = hwnd;
    LOG_DEBUG("GTA HWND found at address {}", static_cast<const void*>(hwnd_));
}

std::string Gta::GetUserFilesPath()
{
    PWSTR pwszPath = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &pwszPath);
    if (SUCCEEDED(hr)) {
        int bufferSize = WideCharToMultiByte(CP_UTF8, 0, pwszPath, -1, nullptr, 0, nullptr, nullptr);
        if (bufferSize > 0) {
            std::string documentsPath(bufferSize, '\0');
            WideCharToMultiByte(CP_UTF8, 0, pwszPath, -1, &documentsPath[0], bufferSize, nullptr, nullptr);
            CoTaskMemFree(pwszPath);
            try {
                if (!documentsPath.empty() && documentsPath.back() == '\0') documentsPath.pop_back();
                std::filesystem::path finalPath = documentsPath; finalPath /= "GTA San Andreas User Files";
                return finalPath.string();
            } catch (const std::filesystem::filesystem_error& e) {
                LOG_ERROR("[GTA] Filesystem error while building user path: {}", e.what());
                return "";
            }
        } else { CoTaskMemFree(pwszPath); }
    }
    LOG_ERROR("[GTA] Failed to get user Documents path.");
    return "";
}