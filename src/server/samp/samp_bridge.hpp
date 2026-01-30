#pragma once

#include "common/bridge.hpp"
#include <memory>

std::unique_ptr<IPlatformBridge> CreateSampPlatformBridge();

class SampPlatformBridge final : public IPlatformBridge
{
public:
    SampPlatformBridge() = default;

    void LogInfo(const std::string& message) override;
    void LogWarn(const std::string& message) override;
    void LogError(const std::string& message) override;
    void LogDebug(const std::string& message) override;

    void CallPawnPublic(const std::string& name, const std::vector<Argument>& args) override;
    void CallOnBrowserCreated(int playerid, int browserid, bool success, int code, const std::string& reason) override;

    std::string GetPlayerAddressIp(int playerid) override;
    void KickPlayer(int playerid) override;
};