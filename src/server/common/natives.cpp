#include "natives.hpp"
#include "api.hpp"

PAWN_NATIVE(Natives, CEF_PlayerHasPlugin, bool(int playerid))
{
    return CefApi::Instance()->PlayerHasPlugin(playerid);
}

PAWN_NATIVE(Natives, CEF_AddResource, void(const std::string& resourceName))
{
    CefApi::Instance()->AddResource(resourceName);
}

PAWN_NATIVE(Natives, CEF_CreateBrowser, void(int playerid, int browserid, const std::string& url, bool focused, bool controls_chat))
{
    CefApi::Instance()->CreateBrowser(playerid, browserid, url, focused, controls_chat);
}

PAWN_NATIVE(Natives, CEF_CreateWorldBrowser, void(int playerid, int browserid, const std::string& url, const std::string& textureName, float width, float height))
{
    CefApi::Instance()->CreateWorldBrowser(playerid, browserid, url, textureName, width, height);
}

PAWN_NATIVE(Natives, CEF_DestroyBrowser, void(int playerid, int browserid))
{
    CefApi::Instance()->DestroyBrowser(playerid, browserid);
}

PAWN_NATIVE(Natives, CEF_RegisterEvent, void(const std::string& eventName, const std::string& callback, EventSignature signature))
{
    CefApi::Instance()->RegisterEvent(eventName, callback, signature.types);
}

PAWN_NATIVE(Natives, CEF_EmitEvent, void(int playerid, int browserid, const std::string& eventName, DynamicArguments arguments))
{
    CefApi::Instance()->EmitEvent(playerid, browserid, eventName, arguments.args);
}

PAWN_NATIVE(Natives, CEF_ReloadBrowser, void(int playerid, int browserid, bool ignore_cache))
{
    CefApi::Instance()->ReloadBrowser(playerid, browserid, ignore_cache);
}

PAWN_NATIVE(Natives, CEF_FocusBrowser, void(int playerid, int id, bool focused))
{
     CefApi::Instance()->FocusBrowser(playerid, id, focused);
}

PAWN_NATIVE(Natives, CEF_EnableDevTools, void(int playerid, int browserid, bool enabled))
{
     CefApi::Instance()->EnableDevTools(playerid, browserid, enabled);
}

PAWN_NATIVE(Natives, CEF_AttachBrowserToObject, void(int playerid, int browserid, int objectId))
{
    CefApi::Instance()->AttachBrowserToObject(playerid, browserid, objectId);
}

PAWN_NATIVE(Natives, CEF_DetachBrowserFromObject, void(int playerid, int browserid, int objectId))
{
    CefApi::Instance()->DetachBrowserFromObject(playerid, browserid, objectId);
}

PAWN_NATIVE(Natives, CEF_SetBrowserMuted, void(int playerid, int browserid, bool muted))
{
    CefApi::Instance()->SetBrowserMuted(playerid, browserid, muted);
}

PAWN_NATIVE(Natives, CEF_SetBrowserAudioMode, void(int playerid, int browserid, int mode))
{
    CefApi::Instance()->SetBrowserAudioMode(playerid, browserid, mode);
}

PAWN_NATIVE(Natives, CEF_SetBrowserAudioSettings, void(int playerid, int browserid, float maxDistance, float referenceDistance))
{
    CefApi::Instance()->SetBrowserAudioSettings(playerid, browserid, maxDistance, referenceDistance);
}

PAWN_NATIVE(Natives, CEF_ToggleHudComponent, void(int playerid, int componentid, bool toggle))
{
    CefApi::Instance()->ToggleHudComponent(playerid, componentid, toggle);
}

PAWN_NATIVE(Natives, CEF_ToggleSpawnScreen, void(int playerid, bool visible))
{
    CefApi::Instance()->ToggleSpawnScreen(playerid, visible);
}