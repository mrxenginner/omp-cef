#pragma once

class HookManager;
class FocusManager;

class ChatHook
{
public:
    ChatHook(HookManager& hooks, FocusManager& focus)
        : hooks_(hooks), focus_(focus)
    {
    }

    bool Initialize();
    void Shutdown();

private:
    using FnOpenChatInput = void(__fastcall*)(void* pThis, void* _edx);
    static void __fastcall Hook_OpenChatInput(void* pThis, void* _edx);

    HookManager& hooks_;
    FocusManager& focus_;

    static inline ChatHook* s_self_ = nullptr;
    static inline FnOpenChatInput s_orig_ = nullptr;
};