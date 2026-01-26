#include "wndproc.hpp"
#include "system/logger.hpp"

bool WndProcHook::Initialize()
{
    if (!hwnd_) { LOG_FATAL("WndProcHook: invalid window handle."); return false; }
    s_self_ = this;

    originalProc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&StaticWndProc))
    );

    if (!originalProc_) {
        LOG_FATAL("Failed to install WndProc hook.");
        s_self_ = nullptr;
        return false;
    }

    LOG_DEBUG("WndProc hook installed successfully.");
    return true;
}

void WndProcHook::Shutdown()
{
    if (hwnd_ && originalProc_) {
        SetWindowLongPtr(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalProc_));
        LOG_DEBUG("WndProc hook uninstalled successfully.");
    }
    hwnd_ = nullptr; originalProc_ = nullptr; s_self_ = nullptr;
}

LRESULT CALLBACK WndProcHook::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = s_self_;
    if (self && self->OnMessage) {
        const LRESULT res = self->OnMessage(hwnd, msg, wParam, lParam);
        if (res != 0) return res;
    }
    return CallWindowProc(self ? self->originalProc_ : DefWindowProc, hwnd, msg, wParam, lParam);
}