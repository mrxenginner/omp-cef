#include "chat.hpp"

#include <cstdint>

#include "browser/focus.hpp"
#include "hooks/hook_manager.hpp"
#include "samp/addresses.hpp"
#include "system/logger.hpp"

bool ChatHook::Initialize()
{
    s_self_ = this;

    const auto version = addrs_.Version();
    auto* base = addrs_.Base();

    void* addrOpenChat = nullptr;
    switch (version)
    {
        case SampVersion::V037:
            addrOpenChat = reinterpret_cast<void*>(base + 0x657E0);
            break;
        case SampVersion::V037R3:
            addrOpenChat = reinterpret_cast<void*>(base + 0x68D10);
            break;
        case SampVersion::V037R5:
            addrOpenChat = reinterpret_cast<void*>(base + 0x69480);
            break;
        case SampVersion::V03DLR1:
            addrOpenChat = reinterpret_cast<void*>(base + 0x68EC0);
            break;
        default:
            break;
    }

    if (!addrOpenChat)
    {
        LOG_FATAL("[ChatHook] Unsupported SA:MP version for OpenChatInput.");
        return false;
    }

    if (!hooks_.Install("ChatHook::OpenChatInput", addrOpenChat, reinterpret_cast<void*>(&Hook_OpenChatInput)))
    {
        LOG_ERROR("[ChatHook] Failed to install OpenChatInput hook.");
        return false;
    }

    s_orig_ = reinterpret_cast<FnOpenChatInput>(hooks_.GetOriginal("ChatHook::OpenChatInput"));
    if (!s_orig_)
    {
        LOG_FATAL("[ChatHook] Failed to get original function pointer.");
        hooks_.Uninstall("ChatHook::OpenChatInput");
        return false;
    }

    LOG_DEBUG("[ChatHook] OpenChatInput hook installed.");
    return true;
}

void ChatHook::Shutdown()
{
    hooks_.Uninstall("ChatHook::OpenChatInput");
    s_orig_ = nullptr;
    s_self_ = nullptr;
}

void __fastcall ChatHook::Hook_OpenChatInput(void* pThis, void* _edx)
{
    auto* self = s_self_;
    if (self && self->focus_.ShouldBlockChat())
    {
        return;
    }

    if (s_orig_)
        s_orig_(pThis, _edx);
}