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
    void RegisterEvent(const std::string& name, const std::string& callback, const std::vector<ArgumentType>& signature);
    void EmitEvent(int playerid, int browserid, const std::string& name, const std::vector<Argument>& args);
    void ReloadBrowser(int playerid, int browserid, bool ignoreCache);
    void FocusBrowser(int playerid, int id, bool focused);
    void EnableDevTools(int playerid, int browserid, bool enabled);

    void AttachBrowserToObject(int playerid, int browserid, int objectid);
    void DetachBrowserFromObject(int playerid, int browserid, int objectid);

    void SetBrowserMuted(int playerid, int browserid, bool muted);
    void SetBrowserAudioMode(int playerid, int browserid, int mode);
    void SetBrowserAudioSettings(int playerid, int browserid, float maxDistance, float referenceDistance);

    void ToggleHudComponent(int playerid, int componentid, bool toggle);
    void ToggleSpawnScreen(int playerid, bool toggle);

private:
    CefPlugin& plugin_;

    static CefApi* instance_;
};