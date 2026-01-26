#pragma once

#include <functional>
#include <mutex>

#include <d3d9.h>

class HookManager;

const auto GameDirect3DCreate9 = reinterpret_cast<IDirect3D9*(CALLBACK*)(UINT)>(0x807C2B);

class RenderManager
{
public:
    static RenderManager& Instance()
    {
        static RenderManager instance;
        return instance;
    }

    RenderManager(const RenderManager&) = delete;
    RenderManager& operator=(const RenderManager&) = delete;
    RenderManager(RenderManager&&) = delete;
    RenderManager& operator=(RenderManager&&) = delete;

    void SetHookManager(HookManager* hooks) noexcept
    {
        hooks_ = hooks;
    }

    bool Initialize();
    void Shutdown();

    // Thread-safe device and window accessors
    IDirect3DDevice9* GetDevice() const noexcept;
    bool GetWindowHandle(HWND& handle) const noexcept;
    bool GetScreenSize(float& width, float& height) const noexcept;

    // Coordinate space conversions (base 640x480 to current resolution)
    bool ConvertBaseXValueToScreenXValue(float base, float& screen) const noexcept;
    bool ConvertBaseYValueToScreenYValue(float base, float& screen) const noexcept;
    bool ConvertScreenXValueToBaseXValue(float screen, float& base) const noexcept;
    bool ConvertScreenYValueToBaseYValue(float screen, float& base) const noexcept;

    static constexpr float kBaseWidth = 640.0f;
    static constexpr float kBaseHeight = 480.0f;

    // Device lifecycle callbacks
    std::function<void(IDirect3D9*, IDirect3DDevice9*, const D3DPRESENT_PARAMETERS&)> OnDeviceInitialize;
    std::function<void()> OnDeviceDestroy;
    std::function<void()> OnBeforeBeginScene;
    std::function<void()> OnAfterBeginScene;
    std::function<void()> OnBeforeEndScene;
    std::function<void()> OnAfterEndScene;
    std::function<void()> OnPresent;
    std::function<void()> OnBeforeReset;
    std::function<void(IDirect3DDevice9*, const D3DPRESENT_PARAMETERS&)> OnAfterReset;

private:
    friend class IDirect3DDevice9Hook;
    friend class IDirect3D9Hook;

    RenderManager() = default;
    ~RenderManager() = default;

    static IDirect3D9* CALLBACK Direct3DCreate9Hook(UINT sdk_version);

    static inline RenderManager* s_self_ = nullptr;

    static constexpr const char* kHookName = "RenderManager::Direct3DCreate9";
    bool initialized_ = false;

    HookManager* hooks_ = nullptr;

    mutable std::mutex device_lock_;
    IDirect3D9* direct_ = nullptr;
    IDirect3DDevice9* device_ = nullptr;
    D3DPRESENT_PARAMETERS device_parameters_{};

    bool reset_status_ = false;
    bool scene_status_ = false;
};
