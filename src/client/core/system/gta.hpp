#pragma once

#include <windows.h>
#include <string>

class Gta
{
public:
    Gta() = default;
    ~Gta() = default;

    Gta(const Gta&) = delete;
    Gta& operator=(const Gta&) = delete;

    void Initialize();
    std::string GetUserFilesPath();

    HWND GetHwnd() const { return hwnd_; }

private:
    static constexpr uintptr_t HwndAddress = 0xC97C1C;
    HWND hwnd_ = nullptr;
};