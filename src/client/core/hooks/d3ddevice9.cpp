#include "d3ddevice9.hpp"
#include "rendering/render_manager.hpp"
#include "system/logger.hpp"

IDirect3DDevice9Hook* g_pD3DDeviceHook = nullptr;

IDirect3DDevice9Hook::IDirect3DDevice9Hook(IDirect3DDevice9* orig) noexcept
    : orig_(orig)
{
    if (orig) {
        orig->AddRef();
    }
}

IDirect3DDevice9Hook::~IDirect3DDevice9Hook() noexcept
{
    if (IDirect3DDevice9* device = orig_.load(std::memory_order_acquire)) {
        device->Release();
        orig_.store(nullptr, std::memory_order_release);
    }
}

void IDirect3DDevice9Hook::Rebind(IDirect3DDevice9* newOrig) noexcept
{
    orig_.store(newOrig, std::memory_order_release);
}

HRESULT __stdcall IDirect3DDevice9Hook::Present(const RECT* pSourceRect, const RECT* pDestRect, 
                                                 HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
{
    IDirect3DDevice9* device = orig();
    if (!device) 
        return D3DERR_INVALIDCALL;

    HRESULT hr = device->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

    auto& render_manager = RenderManager::Instance();
    if (SUCCEEDED(hr) && !render_manager.reset_status_)
    {
        if (render_manager.OnPresent) 
            render_manager.OnPresent();
    }

    return hr;
}

HRESULT __stdcall IDirect3DDevice9Hook::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    auto* render_manager = &RenderManager::Instance();
    
    // Set reset status BEFORE calling OnBeforeReset to prevent rendering
    render_manager->reset_status_ = true;
    
    // Notify that device is about to be reset - release D3DPOOL_MANAGED resources
    if (render_manager->OnBeforeReset)
        render_manager->OnBeforeReset();
    
    // Perform the actual reset
    IDirect3DDevice9* device = orig();
    if (!device)
        return D3DERR_INVALIDCALL;
        
    HRESULT hr = device->Reset(pPresentationParameters);
    if (SUCCEEDED(hr)) 
    {
        LOG_DEBUG("[D3DDevice9Hook] Reset succeeded, updating parameters and recreating resources");
        
        // Update parameters
        {
            std::scoped_lock lock(render_manager->device_lock_);
            render_manager->device_parameters_ = *pPresentationParameters;
        }
        
        // Notify that device has been reset - recreate resources
        if (render_manager->OnAfterReset) 
        {
            render_manager->OnAfterReset(device, *pPresentationParameters);
        }
    } 
    else 
    {
        LOG_ERROR("[D3DDevice9Hook] Reset failed with HRESULT: 0x{:08X}", static_cast<unsigned>(hr));
    }
    
    // Clear reset status to allow rendering again
    render_manager->reset_status_ = false;
    
    return hr;
}

HRESULT __stdcall IDirect3DDevice9Hook::BeginScene()
{
    IDirect3DDevice9* device = orig();
    if (!device)
        return D3DERR_INVALIDCALL;

    auto& render = RenderManager::Instance();

    if (!render.reset_status_ && render.OnBeforeBeginScene)
        render.OnBeforeBeginScene();

    HRESULT hr = device->BeginScene();

    if (SUCCEEDED(hr) && !render.reset_status_)
    {
        if (!render.scene_status_)
        {
            if (render.OnAfterBeginScene)
                render.OnAfterBeginScene();

            render.scene_status_ = true;
        }
    }

    return hr;
}

HRESULT __stdcall IDirect3DDevice9Hook::EndScene()
{
    IDirect3DDevice9* device = orig();
    if (!device)
        return D3DERR_INVALIDCALL;

    auto& render = RenderManager::Instance();

    if (!render.reset_status_ && render.OnBeforeEndScene)
        render.OnBeforeEndScene();

    HRESULT hr = device->EndScene();

    if (SUCCEEDED(hr) && !render.reset_status_)
    {
        if (render.scene_status_)
        {
            if (render.OnAfterEndScene)
                render.OnAfterEndScene();

            render.scene_status_ = false;
        }
    }

    return hr;
}

HRESULT __stdcall IDirect3DDevice9Hook::QueryInterface(REFIID riid, void** ppvObject)
{
    if (!ppvObject) 
        return E_POINTER;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDirect3DDevice9))
    {
        *ppvObject = static_cast<IDirect3DDevice9*>(this);
        AddRef();
        return S_OK;
    }

    IDirect3DDevice9* device = orig();
    return device ? device->QueryInterface(riid, ppvObject) : E_NOINTERFACE;
}

ULONG __stdcall IDirect3DDevice9Hook::AddRef()
{
    IDirect3DDevice9* device = orig();
    return device ? device->AddRef() : 0;
}

ULONG __stdcall IDirect3DDevice9Hook::Release()
{
    IDirect3DDevice9* device = orig();
    if (!device) 
        return 0;

    ULONG count = device->Release();

    if (count == 0)
    {
        auto& render = RenderManager::Instance();

        // Stop any render/tick work immediately
        render.reset_status_ = true;

        // Notify that device is being destroyed
        if (render.OnDeviceDestroy) 
            render.OnDeviceDestroy();

        // Clear render manager's references
        {
            std::scoped_lock lock(render.device_lock_);
            render.direct_ = nullptr;
            render.device_ = nullptr;
        }

        // Detach orig to avoid accidental calls
        orig_.store(nullptr, std::memory_order_release);

        // Clear global hook pointer
        if (g_pD3DDeviceHook == this) {
            g_pD3DDeviceHook = nullptr;
        }

        // Self-destruct
        delete this;
    }

    return count;
}

HRESULT __stdcall IDirect3DDevice9Hook::TestCooperativeLevel() {
    IDirect3DDevice9* device = orig();
    return device ? device->TestCooperativeLevel() : D3DERR_INVALIDCALL;
}

UINT __stdcall IDirect3DDevice9Hook::GetAvailableTextureMem() {
    IDirect3DDevice9* device = orig();
    return device ? device->GetAvailableTextureMem() : 0;
}

HRESULT __stdcall IDirect3DDevice9Hook::EvictManagedResources() {
    IDirect3DDevice9* device = orig();
    return device ? device->EvictManagedResources() : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetDirect3D(IDirect3D9** ppD3D9) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetDirect3D(ppD3D9) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetDeviceCaps(D3DCAPS9* pCaps) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetDeviceCaps(pCaps) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetDisplayMode(iSwapChain, pMode) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetCreationParameters(pParameters) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap) : D3DERR_INVALIDCALL;
}

void __stdcall IDirect3DDevice9Hook::SetCursorPosition(int X, int Y, DWORD Flags) {
    IDirect3DDevice9* device = orig();
    if (device) device->SetCursorPosition(X, Y, Flags);
}

BOOL __stdcall IDirect3DDevice9Hook::ShowCursor(BOOL bShow) {
    if (m_bForceHideCursor)
        bShow = FALSE;
    IDirect3DDevice9* device = orig();
    return device ? device->ShowCursor(bShow) : FALSE;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** ppSwapChain) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateAdditionalSwapChain(pPresentationParameters, ppSwapChain) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** ppSwapChain) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetSwapChain(iSwapChain, ppSwapChain) : D3DERR_INVALIDCALL;
}

UINT __stdcall IDirect3DDevice9Hook::GetNumberOfSwapChains() {
    IDirect3DDevice9* device = orig();
    return device ? device->GetNumberOfSwapChains() : 0;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetRasterStatus(iSwapChain, pRasterStatus) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetDialogBoxMode(BOOL bEnableDialogs) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetDialogBoxMode(bEnableDialogs) : D3DERR_INVALIDCALL;
}

void __stdcall IDirect3DDevice9Hook::SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP* pRamp) {
    IDirect3DDevice9* device = orig();
    if (device) device->SetGammaRamp(iSwapChain, Flags, pRamp);
}

void __stdcall IDirect3DDevice9Hook::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) {
    IDirect3DDevice9* device = orig();
    if (device) device->GetGammaRamp(iSwapChain, pRamp);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::UpdateSurface(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, const POINT* pDestPoint) {
    IDirect3DDevice9* device = orig();
    return device ? device->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) {
    IDirect3DDevice9* device = orig();
    return device ? device->UpdateTexture(pSourceTexture, pDestinationTexture) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetRenderTargetData(pRenderTarget, pDestSurface) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetFrontBufferData(iSwapChain, pDestSurface) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::StretchRect(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect, IDirect3DSurface9* pDestSurface, const RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) {
    IDirect3DDevice9* device = orig();
    return device ? device->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::ColorFill(IDirect3DSurface9* pSurface, const RECT* pRect, D3DCOLOR color) {
    IDirect3DDevice9* device = orig();
    return device ? device->ColorFill(pSurface, pRect, color) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetRenderTarget(RenderTargetIndex, pRenderTarget) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetRenderTarget(RenderTargetIndex, ppRenderTarget) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetDepthStencilSurface(pNewZStencil) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetDepthStencilSurface(ppZStencilSurface) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::Clear(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
    IDirect3DDevice9* device = orig();
    return device ? device->Clear(Count, pRects, Flags, Color, Z, Stencil) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetTransform(State, pMatrix) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetTransform(State, pMatrix) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    IDirect3DDevice9* device = orig();
    return device ? device->MultiplyTransform(State, pMatrix) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetViewport(const D3DVIEWPORT9* pViewport) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetViewport(pViewport) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetViewport(D3DVIEWPORT9* pViewport) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetViewport(pViewport) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetMaterial(const D3DMATERIAL9* pMaterial) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetMaterial(pMaterial) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetMaterial(D3DMATERIAL9* pMaterial) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetMaterial(pMaterial) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetLight(DWORD Index, const D3DLIGHT9* pLight) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetLight(Index, pLight) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetLight(DWORD Index, D3DLIGHT9* pLight) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetLight(Index, pLight) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::LightEnable(DWORD Index, BOOL Enable) {
    IDirect3DDevice9* device = orig();
    return device ? device->LightEnable(Index, Enable) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetLightEnable(DWORD Index, BOOL* pEnable) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetLightEnable(Index, pEnable) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetClipPlane(DWORD Index, const float* pPlane) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetClipPlane(Index, pPlane) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetClipPlane(DWORD Index, float* pPlane) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetClipPlane(Index, pPlane) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetRenderState(State, Value) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetRenderState(State, pValue) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateStateBlock(Type, ppSB) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::BeginStateBlock() {
    IDirect3DDevice9* device = orig();
    return device ? device->BeginStateBlock() : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::EndStateBlock(IDirect3DStateBlock9** ppSB) {
    IDirect3DDevice9* device = orig();
    return device ? device->EndStateBlock(ppSB) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetClipStatus(const D3DCLIPSTATUS9* pClipStatus) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetClipStatus(pClipStatus) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetClipStatus(D3DCLIPSTATUS9* pClipStatus) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetClipStatus(pClipStatus) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetTexture(Stage, ppTexture) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetTexture(Stage, pTexture) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetTextureStageState(Stage, Type, pValue) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetTextureStageState(Stage, Type, Value) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetSamplerState(Sampler, Type, pValue) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetSamplerState(Sampler, Type, Value) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::ValidateDevice(DWORD* pNumPasses) {
    IDirect3DDevice9* device = orig();
    return device ? device->ValidateDevice(pNumPasses) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetPaletteEntries(PaletteNumber, pEntries) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetPaletteEntries(PaletteNumber, pEntries) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetCurrentTexturePalette(UINT PaletteNumber) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetCurrentTexturePalette(PaletteNumber) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetCurrentTexturePalette(UINT* PaletteNumber) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetCurrentTexturePalette(PaletteNumber) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetScissorRect(const RECT* pRect) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetScissorRect(pRect) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetScissorRect(RECT* pRect) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetScissorRect(pRect) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetSoftwareVertexProcessing(BOOL bSoftware) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetSoftwareVertexProcessing(bSoftware) : D3DERR_INVALIDCALL;
}

BOOL __stdcall IDirect3DDevice9Hook::GetSoftwareVertexProcessing() {
    IDirect3DDevice9* device = orig();
    return device ? device->GetSoftwareVertexProcessing() : FALSE;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetNPatchMode(float nSegments) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetNPatchMode(nSegments) : D3DERR_INVALIDCALL;
}

float __stdcall IDirect3DDevice9Hook::GetNPatchMode() {
    IDirect3DDevice9* device = orig();
    return device ? device->GetNPatchMode() : 0.0f;
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    IDirect3DDevice9* device = orig();
    return device ? device->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    IDirect3DDevice9* device = orig();
    return device ? device->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) {
    IDirect3DDevice9* device = orig();
    return device ? device->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateVertexDeclaration(const D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateVertexDeclaration(pVertexElements, ppDecl) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetVertexDeclaration(pDecl) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetVertexDeclaration(ppDecl) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetFVF(DWORD FVF) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetFVF(FVF) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetFVF(DWORD* pFVF) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetFVF(pFVF) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateVertexShader(const DWORD* pFunction, IDirect3DVertexShader9** ppShader) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateVertexShader(pFunction, ppShader) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetVertexShader(IDirect3DVertexShader9* pShader) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetVertexShader(pShader) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetVertexShader(ppShader) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetVertexShaderConstantF(UINT StartRegister, const float* pConstantData, UINT Vector4fCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetVertexShaderConstantI(UINT StartRegister, const int* pConstantData, UINT Vector4iCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetVertexShaderConstantB(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetStreamSource(StreamNumber, ppStreamData, pOffsetInBytes, pStride) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetStreamSourceFreq(UINT StreamNumber, UINT Setting) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetStreamSourceFreq(StreamNumber, Setting) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetStreamSourceFreq(StreamNumber, pSetting) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetIndices(IDirect3DIndexBuffer9* pIndexData) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetIndices(pIndexData) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetIndices(ppIndexData) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreatePixelShader(const DWORD* pFunction, IDirect3DPixelShader9** ppShader) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreatePixelShader(pFunction, ppShader) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetPixelShader(IDirect3DPixelShader9* pShader) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetPixelShader(pShader) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetPixelShader(ppShader) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetPixelShaderConstantF(UINT StartRegister, const float* pConstantData, UINT Vector4fCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetPixelShaderConstantI(UINT StartRegister, const int* pConstantData, UINT Vector4iCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::SetPixelShaderConstantB(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
    IDirect3DDevice9* device = orig();
    return device ? device->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawRectPatch(UINT Handle, const float* pNumSegs, const D3DRECTPATCH_INFO* pRectPatchInfo) {
    IDirect3DDevice9* device = orig();
    return device ? device->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawTriPatch(UINT Handle, const float* pNumSegs, const D3DTRIPATCH_INFO* pTriPatchInfo) {
    IDirect3DDevice9* device = orig();
    return device ? device->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::DeletePatch(UINT Handle) {
    IDirect3DDevice9* device = orig();
    return device ? device->DeletePatch(Handle) : D3DERR_INVALIDCALL;
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
    IDirect3DDevice9* device = orig();
    return device ? device->CreateQuery(Type, ppQuery) : D3DERR_INVALIDCALL;
}