#include "samp_bridge.hpp"
#include <common/encoding.hpp>

#define SAMPGDK_STATIC
#include <sampgdk.h>

extern std::vector<AMX*> g_AmxList;

std::unique_ptr<IPlatformBridge> CreateSampPlatformBridge()
{
    return std::make_unique<SampPlatformBridge>();
}

void SampPlatformBridge::LogInfo(const std::string& message)
{
    sampgdk::logprintf("[CEF] [INFO] %s", message.c_str());
}

void SampPlatformBridge::LogWarn(const std::string& message)
{
    sampgdk::logprintf("[CEF] [WARN] %s", message.c_str());
}

void SampPlatformBridge::LogError(const std::string& message)
{
    sampgdk::logprintf("[CEF] [ERROR] %s", message.c_str());
}

void SampPlatformBridge::LogDebug(const std::string& message)
{
    sampgdk::logprintf("[CEF] [DEBUG] %s", message.c_str());
}

void SampPlatformBridge::CallPawnPublic(const std::string& name, const std::vector<Argument>& args)
{
    for (AMX* amx : g_AmxList)
    {
        int idx = 0;
        if (amx_FindPublic(amx, name.c_str(), &idx) != AMX_ERR_NONE)
            continue;

        cell heap_addr_before_push = 0;
        bool string_pushed = false;

        for (auto it = args.rbegin(); it != args.rend(); ++it)
        {
            const auto& arg = *it;
            switch (arg.type)
            {
                case ArgumentType::String:
                {
                    cell amx_addr = 0;
                    
                    std::string ansi_string = Utf8ToAnsi(arg.stringValue);
                    amx_PushString(amx, &amx_addr, NULL, ansi_string.c_str(), NULL, NULL);

                    if (!string_pushed) {
                        heap_addr_before_push = amx_addr;
                        string_pushed = true;
                    }
                    break;
                }
                case ArgumentType::Integer:
                    amx_Push(amx, arg.intValue);
                    break;
                case ArgumentType::Float:
                    amx_Push(amx, amx_ftoc(arg.floatValue));
                    break;
                case ArgumentType::Bool:
                    amx_Push(amx, arg.boolValue);
                    break;
            }
        }

        cell ret;
        int error = amx_Exec(amx, &ret, idx);
        if (error != AMX_ERR_NONE)
        {
            // TODO
        }

        if (string_pushed)
        {
            amx_Release(amx, heap_addr_before_push);
        }
    }
}

void SampPlatformBridge::CallOnBrowserCreated(int playerid, int browserId, bool success, int code, const std::string& reason)
{
    for (AMX* amx : g_AmxList)
    {
        int idx;
        if (amx_FindPublic(amx, "OnCefBrowserCreated", &idx) != AMX_ERR_NONE) 
            continue;

        cell reason_addr = 0;

        amx_PushString(amx, &reason_addr, NULL, reason.c_str(), NULL, NULL);
        amx_Push(amx, code);
        amx_Push(amx, success);
        amx_Push(amx, browserId);
        amx_Push(amx, playerid);

        cell retval;
        amx_Exec(amx, &retval, idx);
        amx_Release(amx, reason_addr);
    }
}

std::string SampPlatformBridge::GetPlayerAddressIp(int playerid)
{
    char ip[64] = {};
    if (sampgdk_GetPlayerIp(playerid, ip, sizeof(ip)) == 0)
        return std::string(ip);

    return {};
}

void SampPlatformBridge::KickPlayer(int playerid)
{
    sampgdk_Kick(playerid);
}
