#include "render_manager.hpp"

#include <windows.h>
#include <new>

#include "system/logger.hpp"
#include "hooks/hook_manager.hpp"
#include "hooks/d3d9.hpp"
#include "hooks/d3ddevice9.hpp"

namespace
{
    void* ResolveDirect3DCreate9()
    {
        HMODULE mod = ::GetModuleHandleA("d3d9.dll");
        if (!mod)
            mod = ::LoadLibraryA("d3d9.dll");

        if (!mod)
            return nullptr;

        return reinterpret_cast<void*>(::GetProcAddress(mod, "Direct3DCreate9"));
    }
}

bool RenderManager::Initialize()
{
    if (initialized_)
        return true;

    if (!hooks_) {
        LOG_FATAL("[RenderManager] Initialize called without HookManager set. Call SetHookManager() first.");
        return false;
    }

    s_self_ = this;

    if (void* target = ResolveDirect3DCreate9())
    {
        LOG_INFO("[D3D9] Installing Direct3DCreate9 hook at {}", target);

        if (!hooks_->Install(kHookName, target, reinterpret_cast<void*>(&RenderManager::Direct3DCreate9Hook)))
        {
            LOG_ERROR("[D3D9] Failed to install Direct3DCreate9 hook");
        }
        else
        {
            LOG_DEBUG("[D3D9] Direct3DCreate9 hook installed successfully");
        }
    }
    else
    {
        LOG_ERROR("[D3D9] Unable to resolve d3d9.dll!Direct3DCreate9 (GetProcAddress failed)");
    }

    if (g_pD3DDeviceHook == nullptr)
    {
        auto* game_device = const_cast<IDirect3DDevice9*>(gGameDevice);
        if (game_device)
        {
            LOG_WARN("[D3D9] CreateDevice already happened; wrapping existing gGameDevice={}", (void*)game_device);

            D3DPRESENT_PARAMETERS pp{};
            if (IDirect3DSwapChain9* sc = nullptr; SUCCEEDED(game_device->GetSwapChain(0, &sc)) && sc)
            {
                sc->GetPresentParameters(&pp);
                sc->Release();
            }

            IDirect3D9* d3d = nullptr;
            game_device->GetDirect3D(&d3d);

            if (direct_ != nullptr && OnDeviceDestroy)
                OnDeviceDestroy();

            {
                std::scoped_lock lock(device_lock_);
                direct_ = d3d;
                device_ = game_device;
                device_parameters_ = pp;
            }

            if (OnDeviceInitialize)
                OnDeviceInitialize(direct_, device_, device_parameters_);

            auto* hook_game_device = new (std::nothrow) IDirect3DDevice9Hook(game_device);
            if (hook_game_device)
            {
                g_pD3DDeviceHook = hook_game_device;
                gGameDevice = hook_game_device;

                LOG_INFO("[D3D9] Existing device hooked successfully: orig={}, hook={}", (void*)game_device, (void*)hook_game_device);
            }
            else
            {
                LOG_ERROR("[D3D9] Failed to allocate IDirect3DDevice9Hook for existing device.");
            }
        }
    }

    initialized_ = true;
    return true;
}

void RenderManager::Shutdown()
{
    if (hooks_)
        hooks_->Uninstall(kHookName);

    std::scoped_lock lock(device_lock_);

    direct_ = nullptr;
    device_ = nullptr;
    device_parameters_ = {};
}

IDirect3DDevice9* RenderManager::GetDevice() const noexcept
{
    std::scoped_lock lock(device_lock_);
    return device_;
}

bool RenderManager::GetWindowHandle(HWND& handle) const noexcept
{
    std::scoped_lock lock(device_lock_);

    if (device_) {
        handle = device_parameters_.hDeviceWindow;
        return true;
    }

    return false;
}

bool RenderManager::GetScreenSize(float& width, float& height) const noexcept
{
    std::scoped_lock lock(device_lock_);
    
    if (device_ && device_parameters_.BackBufferWidth > 0 && device_parameters_.BackBufferHeight > 0) {
        width = static_cast<float>(device_parameters_.BackBufferWidth);
        height = static_cast<float>(device_parameters_.BackBufferHeight);
        return true;
    }

    return false;
}

bool RenderManager::ConvertBaseXValueToScreenXValue(float base, float& screen) const noexcept
{
    std::scoped_lock lock(device_lock_);
    const auto w = device_parameters_.BackBufferWidth;
    if (device_ && w > 0) { screen = (static_cast<float>(w) / kBaseWidth) * base; return true; }
    return false;
}

bool RenderManager::ConvertBaseYValueToScreenYValue(float base, float& screen) const noexcept
{
    std::scoped_lock lock(device_lock_);
    const auto h = device_parameters_.BackBufferHeight;
    if (device_ && h > 0) { screen = (static_cast<float>(h) / kBaseHeight) * base; return true; }
    return false;
}

bool RenderManager::ConvertScreenXValueToBaseXValue(float screen, float& base) const noexcept
{
    std::scoped_lock lock(device_lock_);
    const auto w = device_parameters_.BackBufferWidth;
    if (device_ && w > 0) { base = (kBaseWidth / static_cast<float>(w)) * screen; return true; }
    return false;
}

bool RenderManager::ConvertScreenYValueToBaseYValue(float screen, float& base) const noexcept
{
    std::scoped_lock lock(device_lock_);
    const auto h = device_parameters_.BackBufferHeight;
    if (device_ && h > 0) { base = (kBaseHeight / static_cast<float>(h)) * screen; return true; }
    return false;
}

IDirect3D9* WINAPI RenderManager::Direct3DCreate9Hook(UINT sdk_version)
{
    // Call trampoline MinHook
    using Fn = IDirect3D9* (WINAPI*)(UINT);

    auto* self = s_self_;
    auto* orig = self ? reinterpret_cast<Fn>(self->hooks_->GetOriginal(kHookName)) : nullptr;
    if (!orig)
        return nullptr;

    IDirect3D9* d3d = orig(sdk_version);
    if (!d3d)
        return nullptr;

    // Wrap IDirect3D9
    if (auto* hookDirect = new (std::nothrow) IDirect3D9Hook(d3d))
    {
        gGameDirect = hookDirect;
        return hookDirect;
    }

    return d3d;
}











































