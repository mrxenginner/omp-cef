#include "d3ddevice9.hpp"
#include "rendering/render_manager.hpp"

IDirect3DDevice9Hook* g_pD3DDeviceHook = nullptr;

HRESULT __stdcall IDirect3DDevice9Hook::Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
{
    if (RenderManager::Instance().OnPresent)
        RenderManager::Instance().OnPresent();
    return _orig->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT __stdcall IDirect3DDevice9Hook::BeginScene()
{
    auto& render = RenderManager::Instance();
    const HRESULT result = _orig->BeginScene();
    if (_orig == render.device_ && SUCCEEDED(result) && !render.reset_status_)
    {
        if (!render.scene_status_) {
            if (render.OnAfterBeginScene) render.OnAfterBeginScene();
        }
        render.scene_status_ = true;
    }
    return result;
}

HRESULT __stdcall IDirect3DDevice9Hook::EndScene()
{
    auto& render = RenderManager::Instance();
    const HRESULT result = _orig->EndScene();
    if (_orig == render.device_ && SUCCEEDED(result) && !render.reset_status_)
    {
        if (render.scene_status_) {
            if (render.OnAfterEndScene) render.OnAfterEndScene();
        }
        render.scene_status_ = false;
    }
    return result;
}

HRESULT __stdcall IDirect3DDevice9Hook::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    auto& render = RenderManager::Instance();
    render.reset_status_ = true;
    if (render.OnBeforeReset) render.OnBeforeReset();

    const HRESULT result = _orig->Reset(pPresentationParameters);

    if (SUCCEEDED(result)) {
        std::scoped_lock lock(render.device_lock_);
        render.device_parameters_ = *pPresentationParameters;
        if (render.OnAfterReset) render.OnAfterReset(render.device_, *pPresentationParameters);
    }

    render.reset_status_ = false;
    return result;
}

HRESULT __stdcall IDirect3DDevice9Hook::QueryInterface(REFIID riid, void** ppvObject) { return _orig->QueryInterface(riid, ppvObject); }
ULONG   __stdcall IDirect3DDevice9Hook::AddRef() { return _orig->AddRef(); }
ULONG   __stdcall IDirect3DDevice9Hook::Release()
{
    ULONG count = _orig->Release();
    if (count == 0)
    {
        if (RenderManager::Instance().OnDeviceDestroy) RenderManager::Instance().OnDeviceDestroy();
        std::scoped_lock lock(RenderManager::Instance().device_lock_);
        RenderManager::Instance().direct_ = nullptr;
        RenderManager::Instance().device_ = nullptr;
        delete this;
    }
    return count;
}

HRESULT __stdcall IDirect3DDevice9Hook::TestCooperativeLevel() { return _orig->TestCooperativeLevel(); }
UINT __stdcall IDirect3DDevice9Hook::GetAvailableTextureMem() { return _orig->GetAvailableTextureMem(); }
HRESULT __stdcall IDirect3DDevice9Hook::EvictManagedResources() { return _orig->EvictManagedResources(); }
HRESULT __stdcall IDirect3DDevice9Hook::GetDirect3D(IDirect3D9** pp) { return _orig->GetDirect3D(pp); }
HRESULT __stdcall IDirect3DDevice9Hook::GetDeviceCaps(D3DCAPS9* p) { return _orig->GetDeviceCaps(p); }
HRESULT __stdcall IDirect3DDevice9Hook::GetDisplayMode(UINT a, D3DDISPLAYMODE* p) { return _orig->GetDisplayMode(a, p); }
HRESULT __stdcall IDirect3DDevice9Hook::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* p) { return _orig->GetCreationParameters(p); }

HRESULT __stdcall IDirect3DDevice9Hook::SetCursorProperties(UINT x, UINT y, IDirect3DSurface9* pSurface) 
{
    if (m_bForceHideCursor) {
        return S_OK;
    }
    return _orig->SetCursorProperties(x, y, pSurface);
}

void __stdcall IDirect3DDevice9Hook::SetCursorPosition(int x, int y, DWORD f) { return _orig->SetCursorPosition(x, y, f); }

BOOL __stdcall IDirect3DDevice9Hook::ShowCursor(BOOL b) 
{ 
    if (m_bForceHideCursor) {
        return S_OK;
    }

    return _orig->ShowCursor(b); 
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* p, IDirect3DSwapChain9** s) { return _orig->CreateAdditionalSwapChain(p, s); }
HRESULT __stdcall IDirect3DDevice9Hook::GetSwapChain(UINT a, IDirect3DSwapChain9** s) { return _orig->GetSwapChain(a, s); }
UINT __stdcall IDirect3DDevice9Hook::GetNumberOfSwapChains() { return _orig->GetNumberOfSwapChains(); }
HRESULT __stdcall IDirect3DDevice9Hook::GetBackBuffer(UINT a, UINT b, D3DBACKBUFFER_TYPE t, IDirect3DSurface9** s) { return _orig->GetBackBuffer(a, b, t, s); }
HRESULT __stdcall IDirect3DDevice9Hook::GetRasterStatus(UINT a, D3DRASTER_STATUS* r) { return _orig->GetRasterStatus(a, r); }
HRESULT __stdcall IDirect3DDevice9Hook::SetDialogBoxMode(BOOL b) { return _orig->SetDialogBoxMode(b); }
void __stdcall IDirect3DDevice9Hook::SetGammaRamp(UINT a, DWORD b, const D3DGAMMARAMP* r) { _orig->SetGammaRamp(a, b, r); }
void __stdcall IDirect3DDevice9Hook::GetGammaRamp(UINT a, D3DGAMMARAMP* r) { _orig->GetGammaRamp(a, r); }

HRESULT __stdcall IDirect3DDevice9Hook::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {
    return _orig->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) {
    return _orig->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) {
    return _orig->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) {
    return _orig->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) {
    return _orig->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable,
    IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    return _orig->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard,
    IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    return _orig->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9Hook::UpdateSurface(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect,
    IDirect3DSurface9* pDestinationSurface, const POINT* pDestPoint) {
    return _orig->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
}

HRESULT __stdcall IDirect3DDevice9Hook::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture,
    IDirect3DBaseTexture9* pDestinationTexture) {
    return _orig->UpdateTexture(pSourceTexture, pDestinationTexture);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetRenderTargetData(IDirect3DSurface9* pRenderTarget,
    IDirect3DSurface9* pDestSurface) {
    return _orig->GetRenderTargetData(pRenderTarget, pDestSurface);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
    return _orig->GetFrontBufferData(iSwapChain, pDestSurface);
}

HRESULT __stdcall IDirect3DDevice9Hook::StretchRect(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect,
    IDirect3DSurface9* pDestSurface, const RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) {
    return _orig->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
}

HRESULT __stdcall IDirect3DDevice9Hook::ColorFill(IDirect3DSurface9* pSurface, const RECT* pRect, D3DCOLOR color) {
    return _orig->ColorFill(pSurface, pRect, color);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format,
    D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    return _orig->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) {
    return _orig->SetRenderTarget(RenderTargetIndex, pRenderTarget);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) {
    return _orig->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    return _orig->SetDepthStencilSurface(pNewZStencil);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    return _orig->GetDepthStencilSurface(ppZStencilSurface);
}

HRESULT __stdcall IDirect3DDevice9Hook::Clear(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
    return _orig->Clear(Count, pRects, Flags, Color, Z, Stencil);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    return _orig->SetTransform(State, pMatrix);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    return _orig->GetTransform(State, pMatrix);
}

HRESULT __stdcall IDirect3DDevice9Hook::MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    return _orig->MultiplyTransform(State, pMatrix);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetViewport(const D3DVIEWPORT9* pViewport) {
    return _orig->SetViewport(pViewport);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetViewport(D3DVIEWPORT9* pViewport) {
    return _orig->GetViewport(pViewport);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetMaterial(const D3DMATERIAL9* pMaterial) {
    return _orig->SetMaterial(pMaterial);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetMaterial(D3DMATERIAL9* pMaterial) {
    return _orig->GetMaterial(pMaterial);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetLight(DWORD Index, const D3DLIGHT9* pLight) {
    return _orig->SetLight(Index, pLight);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetLight(DWORD Index, D3DLIGHT9* pLight) {
    return _orig->GetLight(Index, pLight);
}

HRESULT __stdcall IDirect3DDevice9Hook::LightEnable(DWORD Index, BOOL Enable) {
    return _orig->LightEnable(Index, Enable);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetLightEnable(DWORD Index, BOOL* pEnable) {
    return _orig->GetLightEnable(Index, pEnable);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetClipPlane(DWORD Index, const float* pPlane) {
    return _orig->SetClipPlane(Index, pPlane);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetClipPlane(DWORD Index, float* pPlane) {
    return _orig->GetClipPlane(Index, pPlane);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    return _orig->SetRenderState(State, Value);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    return _orig->GetRenderState(State, pValue);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) {
    return _orig->CreateStateBlock(Type, ppSB);
}

HRESULT __stdcall IDirect3DDevice9Hook::BeginStateBlock() {
    return _orig->BeginStateBlock();
}

HRESULT __stdcall IDirect3DDevice9Hook::EndStateBlock(IDirect3DStateBlock9** ppSB) {
    return _orig->EndStateBlock(ppSB);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetClipStatus(const D3DCLIPSTATUS9* pClipStatus) {
    return _orig->SetClipStatus(pClipStatus);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetClipStatus(D3DCLIPSTATUS9* pClipStatus) {
    return _orig->GetClipStatus(pClipStatus);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) {
    return _orig->GetTexture(Stage, ppTexture);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    return _orig->SetTexture(Stage, pTexture);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) {
    return _orig->GetTextureStageState(Stage, Type, pValue);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
    return _orig->SetTextureStageState(Stage, Type, Value);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) {
    return _orig->GetSamplerState(Sampler, Type, pValue);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
    return _orig->SetSamplerState(Sampler, Type, Value);
}

HRESULT __stdcall IDirect3DDevice9Hook::ValidateDevice(DWORD* pNumPasses) {
    return _orig->ValidateDevice(pNumPasses);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries) {
    return _orig->SetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) {
    return _orig->GetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetCurrentTexturePalette(UINT PaletteNumber) {
    return _orig->SetCurrentTexturePalette(PaletteNumber);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetCurrentTexturePalette(UINT* PaletteNumber) {
    return _orig->GetCurrentTexturePalette(PaletteNumber);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetScissorRect(const RECT* pRect) {
    return _orig->SetScissorRect(pRect);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetScissorRect(RECT* pRect) {
    return _orig->GetScissorRect(pRect);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetSoftwareVertexProcessing(BOOL bSoftware) {
    return _orig->SetSoftwareVertexProcessing(bSoftware);
}

BOOL __stdcall IDirect3DDevice9Hook::GetSoftwareVertexProcessing() {
    return _orig->GetSoftwareVertexProcessing();
}

HRESULT __stdcall IDirect3DDevice9Hook::SetNPatchMode(float nSegments) {
    return _orig->SetNPatchMode(nSegments);
}

float __stdcall IDirect3DDevice9Hook::GetNPatchMode() {
    return _orig->GetNPatchMode();
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
    return _orig->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) {
    return _orig->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    return _orig->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    return _orig->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT __stdcall IDirect3DDevice9Hook::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) {
    return _orig->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateVertexDeclaration(const D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) {
    return _orig->CreateVertexDeclaration(pVertexElements, ppDecl);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) {
    return _orig->SetVertexDeclaration(pDecl);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
    return _orig->GetVertexDeclaration(ppDecl);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetFVF(DWORD FVF) {
    return _orig->SetFVF(FVF);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetFVF(DWORD* pFVF) {
    return _orig->GetFVF(pFVF);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateVertexShader(const DWORD* pFunction, IDirect3DVertexShader9** ppShader) {
    return _orig->CreateVertexShader(pFunction, ppShader);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetVertexShader(IDirect3DVertexShader9* pShader) {
    return _orig->SetVertexShader(pShader);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    return _orig->GetVertexShader(ppShader);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetVertexShaderConstantF(UINT StartRegister, const float* pConstantData, UINT Vector4fCount) {
    return _orig->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
    return _orig->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetVertexShaderConstantI(UINT StartRegister, const int* pConstantData, UINT Vector4iCount) {
    return _orig->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
    return _orig->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetVertexShaderConstantB(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount) {
    return _orig->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
    return _orig->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) {
    return _orig->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride) {
    return _orig->GetStreamSource(StreamNumber, ppStreamData, pOffsetInBytes, pStride);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetStreamSourceFreq(UINT StreamNumber, UINT Setting) {
    return _orig->SetStreamSourceFreq(StreamNumber, Setting);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) {
    return _orig->GetStreamSourceFreq(StreamNumber, pSetting);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetIndices(IDirect3DIndexBuffer9* pIndexData) {
    return _orig->SetIndices(pIndexData);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
    return _orig->GetIndices(ppIndexData);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreatePixelShader(const DWORD* pFunction, IDirect3DPixelShader9** ppShader) {
    return _orig->CreatePixelShader(pFunction, ppShader);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetPixelShader(IDirect3DPixelShader9* pShader) {
    return _orig->SetPixelShader(pShader);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    return _orig->GetPixelShader(ppShader);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetPixelShaderConstantF(UINT StartRegister, const float* pConstantData, UINT Vector4fCount) {
    return _orig->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
    return _orig->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetPixelShaderConstantI(UINT StartRegister, const int* pConstantData, UINT Vector4iCount) {
    return _orig->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
    return _orig->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::SetPixelShaderConstantB(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount) {
    return _orig->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
    return _orig->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawRectPatch(UINT Handle, const float* pNumSegs, const D3DRECTPATCH_INFO* pRectPatchInfo) {
    return _orig->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}

HRESULT __stdcall IDirect3DDevice9Hook::DrawTriPatch(UINT Handle, const float* pNumSegs, const D3DTRIPATCH_INFO* pTriPatchInfo) {
    return _orig->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}

HRESULT __stdcall IDirect3DDevice9Hook::DeletePatch(UINT Handle) {
    return _orig->DeletePatch(Handle);
}

HRESULT __stdcall IDirect3DDevice9Hook::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
    return _orig->CreateQuery(Type, ppQuery);
}