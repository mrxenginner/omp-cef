#pragma once

#include <string>
#include <vector>
#include "shared/packet.hpp"

class CefPlugin;

class CefApi
{
public:
    explicit CefApi(CefPlugin& plugin);
    ~CefApi();

    static CefApi* Instance()
    {
        return instance_;
    }

    bool PlayerHasPlugin(int playerid);
    void AddResource(const std::string& resourceName);
    void CreateBrowser(int playerid, int browserid, const std::string& url, bool focused, bool controls_chat);
    void CreateWorldBrowser(int playerid, int browserid, const std::string& url, const std::string& textureName, float width, float height);
    void DestroyBrowser(int playerid, int browserid);
    void RegisterEvent(const std::string& name, const std::vector<ArgumentType>& signature);
    void EmitEvent(int playerid, int browserid, const std::string& name, const std::vector<Argument>& args);

    void AttachBrowserToObject(int playerid, int browserid, int objectid);
    void DetachBrowserFromObject(int playerid, int browserid, int objectid);

private:
    CefPlugin& plugin_;

    static CefApi* instance_;
};