#include "d3d9.hpp"
#include "d3ddevice9.hpp"
#include "rendering/render_manager.hpp"

volatile IDirect3D9*&       gGameDirect = *reinterpret_cast<volatile IDirect3D9**>(0xC97C20);
volatile IDirect3DDevice9*& gGameDevice = *reinterpret_cast<volatile IDirect3DDevice9**>(0xC97C28);

HRESULT __stdcall IDirect3D9Hook::CreateDevice(
    UINT adapter, D3DDEVTYPE deviceType, HWND hFocusWindow, DWORD behaviorFlags,
    D3DPRESENT_PARAMETERS* pPresentationParameters,
    IDirect3DDevice9** ppReturnedDeviceInterface)
{

    LOG_DEBUG("[D3D9] CreateDevice called: this={}, gGameDirect={}, ppArg={}, &gGameDevice={}.",
        (void*)this, (void*)gGameDirect, (void*)ppReturnedDeviceInterface, (void*)&gGameDevice);

    const HRESULT result = _orig->CreateDevice(adapter, deviceType, hFocusWindow,
        behaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

    if (FAILED(result) || !ppReturnedDeviceInterface || !*ppReturnedDeviceInterface)
        return result;

    auto* render_manager = &RenderManager::Instance();

    if (g_pD3DDeviceHook != nullptr)
        return result;

    const auto hook_device = new (std::nothrow) IDirect3DDevice9Hook{ *ppReturnedDeviceInterface };
    if (!hook_device)
        return result;

    g_pD3DDeviceHook = hook_device;

    if (render_manager->direct_ != nullptr)
    {
        if (render_manager->OnDeviceDestroy)
            render_manager->OnDeviceDestroy();
    }

    {
        std::scoped_lock lock(render_manager->device_lock_);

        render_manager->direct_ = _orig;
        render_manager->device_ = *ppReturnedDeviceInterface;
        render_manager->device_parameters_ = *pPresentationParameters;
    }

    if (render_manager->OnDeviceInitialize)
        render_manager->OnDeviceInitialize(render_manager->direct_, render_manager->device_, render_manager->device_parameters_);

    *ppReturnedDeviceInterface = hook_device;

    if (gGameDevice == render_manager->device_ || gGameDevice == nullptr)
        gGameDevice = hook_device;

    LOG_DEBUG("[D3D9] Device hooked: orig={}, hook={}.", (void*)render_manager->device_, (void*)hook_device);

    return result;
}

HRESULT __stdcall IDirect3D9Hook::QueryInterface(REFIID riid, void** ppvObj) { return _orig->QueryInterface(riid, ppvObj); }
ULONG   __stdcall IDirect3D9Hook::AddRef() { return _orig->AddRef(); }
ULONG   __stdcall IDirect3D9Hook::Release() { ULONG c = _orig->Release(); if (c == 0) delete this; return c; }
HRESULT __stdcall IDirect3D9Hook::RegisterSoftwareDevice(void* pInitializeFunction) { return _orig->RegisterSoftwareDevice(pInitializeFunction); }
UINT    __stdcall IDirect3D9Hook::GetAdapterCount() { return _orig->GetAdapterCount(); }
HRESULT __stdcall IDirect3D9Hook::GetAdapterIdentifier(UINT a, DWORD f, D3DADAPTER_IDENTIFIER9* p) { return _orig->GetAdapterIdentifier(a, f, p); }
UINT    __stdcall IDirect3D9Hook::GetAdapterModeCount(UINT a, D3DFORMAT fmt) { return _orig->GetAdapterModeCount(a, fmt); }
HRESULT __stdcall IDirect3D9Hook::EnumAdapterModes(UINT a, D3DFORMAT fmt, UINT m, D3DDISPLAYMODE* p) { return _orig->EnumAdapterModes(a, fmt, m, p); }
HRESULT __stdcall IDirect3D9Hook::GetAdapterDisplayMode(UINT a, D3DDISPLAYMODE* p) { return _orig->GetAdapterDisplayMode(a, p); }
HRESULT __stdcall IDirect3D9Hook::CheckDeviceType(UINT a, D3DDEVTYPE t, D3DFORMAT df, D3DFORMAT bf, BOOL w) { return _orig->CheckDeviceType(a, t, df, bf, w); }
HRESULT __stdcall IDirect3D9Hook::CheckDeviceFormat(UINT a, D3DDEVTYPE t, D3DFORMAT af, DWORD u, D3DRESOURCETYPE r, D3DFORMAT cf) { return _orig->CheckDeviceFormat(a, t, af, u, r, cf); }
HRESULT __stdcall IDirect3D9Hook::CheckDeviceMultiSampleType(UINT a, D3DDEVTYPE t, D3DFORMAT sf, BOOL w, D3DMULTISAMPLE_TYPE ms, DWORD* q) { return _orig->CheckDeviceMultiSampleType(a, t, sf, w, ms, q); }
HRESULT __stdcall IDirect3D9Hook::CheckDepthStencilMatch(UINT a, D3DDEVTYPE t, D3DFORMAT af, D3DFORMAT rtf, D3DFORMAT dsf) { return _orig->CheckDepthStencilMatch(a, t, af, rtf, dsf); }
HRESULT __stdcall IDirect3D9Hook::CheckDeviceFormatConversion(UINT a, D3DDEVTYPE t, D3DFORMAT sf, D3DFORMAT tf) { return _orig->CheckDeviceFormatConversion(a, t, sf, tf); }
HRESULT __stdcall IDirect3D9Hook::GetDeviceCaps(UINT a, D3DDEVTYPE t, D3DCAPS9* p) { return _orig->GetDeviceCaps(a, t, p); }
HMONITOR __stdcall IDirect3D9Hook::GetAdapterMonitor(UINT a) { return _orig->GetAdapterMonitor(a); }