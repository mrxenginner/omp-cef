#include "render_manager.hpp"
#include "system/logger.hpp"
#include "hooks/d3d9.hpp"
#include "hooks/hook_manager.hpp"

bool RenderManager::Initialize()
{
    if (initialized_) return true;

    if (!hooks_) {
        LOG_FATAL("RenderManager::Initialize called without HookManager set. Call SetHookManager() first.");
        return false;
    }

    s_self_ = this;

    if (!hooks_->Install(
            kHookName,
            reinterpret_cast<void*>(GameDirect3DCreate9),
            reinterpret_cast<void*>(&RenderManager::Direct3DCreate9Hook)))
    {
        LOG_ERROR("Failed to install Direct3DCreate9 hook");
        return false;
    }

    initialized_ = true;
    LOG_DEBUG("Direct3DCreate9 hook installed successfully");
    return true;
}

void RenderManager::Shutdown()
{
    if (hooks_) hooks_->Uninstall(kHookName);

    std::scoped_lock lock(device_lock_);
    direct_ = nullptr;
    device_ = nullptr;
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
    const auto buffer_width = device_parameters_.BackBufferWidth;
    if (device_ && buffer_width > 0) { screen = (static_cast<float>(buffer_width) / kBaseWidth) * base; return true; }
    return false;
}

bool RenderManager::ConvertBaseYValueToScreenYValue(float base, float& screen) const noexcept
{
    std::scoped_lock lock(device_lock_);
    const auto buffer_height = device_parameters_.BackBufferHeight;
    if (device_ && buffer_height > 0) { screen = (static_cast<float>(buffer_height) / kBaseHeight) * base; return true; }
    return false;
}

bool RenderManager::ConvertScreenXValueToBaseXValue(float screen, float& base) const noexcept
{
    std::scoped_lock lock(device_lock_);
    const auto buffer_width = device_parameters_.BackBufferWidth;
    if (device_ && buffer_width > 0) { base = (kBaseWidth / static_cast<float>(buffer_width)) * screen; return true; }
    return false;
}

bool RenderManager::ConvertScreenYValueToBaseYValue(float screen, float& base) const noexcept
{
    std::scoped_lock lock(device_lock_);
    const auto buffer_height = device_parameters_.BackBufferHeight;
    if (device_ && buffer_height > 0) { base = (kBaseHeight / static_cast<float>(buffer_height)) * screen; return true; }
    return false;
}

IDirect3D9* CALLBACK RenderManager::Direct3DCreate9Hook(UINT SDKVersion)
{
    if (s_self_ && s_self_->hooks_) s_self_->hooks_->Disable(kHookName);
    auto* origDirect = GameDirect3DCreate9(SDKVersion);
    if (s_self_ && s_self_->hooks_) s_self_->hooks_->Enable(kHookName);

    if (origDirect != nullptr)
    {
        if (const auto hookDirect = new (std::nothrow) IDirect3D9Hook(origDirect); hookDirect != nullptr)
        {
            gGameDirect = hookDirect;
            origDirect = hookDirect;
        }
    }

    return origDirect;
}