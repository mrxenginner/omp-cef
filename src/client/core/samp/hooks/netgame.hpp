#pragma once

class HookManager;
class ResourceManager;

class NetGameHook
{
public:
    NetGameHook(HookManager& hooks, ResourceManager& resources) : hooks_(hooks), resources_(resources) {}

    bool Initialize();
    void Shutdown();

private:
    using FnProcessGameStuff = void(__thiscall*)(void*);
    static void __fastcall Hook_ProcessGameStuff(void* pThis, void* _edx);

    HookManager& hooks_;
    ResourceManager& resources_;

    bool cef_download_triggered_ = false;

    static inline NetGameHook* s_self_ = nullptr;
    static inline FnProcessGameStuff s_orig_ = nullptr;
};