#pragma once

#include <string>
#include <shared/packet.hpp>

class IPlatformBridge
{
public:
    virtual ~IPlatformBridge() = default;

    virtual void LogInfo(const std::string& message) = 0;
    virtual void LogWarn(const std::string& message) = 0;
    virtual void LogError(const std::string& message) = 0;
    virtual void LogDebug(const std::string& message) = 0;

    virtual void CallPawnPublic(const std::string& name, const std::vector<Argument>& args) = 0;
    virtual void CallOnBrowserCreated(int playerid, int browserId, bool success, int code, const std::string& reason) = 0;

    virtual std::string GetPlayerAddressIp(int playerid) = 0;
    virtual void KickPlayer(int playerid) = 0;
};