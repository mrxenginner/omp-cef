#pragma once

namespace CefEvent
{
    namespace Server
    {
        inline constexpr const char* CreateBrowser = "CreateBrowser";
        inline constexpr const char* CreateWorldBrowser = "CreateWorldBrowser";
        inline constexpr const char* DestroyBrowser = "DestroyBrowser";

        inline constexpr const char* ReloadBrowser = "ReloadBrowser";
        inline constexpr const char* FocusBrowser = "FocusBrowser";

        inline constexpr const char* AttachBrowserToObject = "AttachBrowserToObject";
        inline constexpr const char* DetachBrowserFromObject = "DetachBrowserFromObject";

        inline constexpr const char* MuteBrowser = "MuteBrowser";
        inline constexpr const char* SetAudioMode = "SetAudioMode";
        inline constexpr const char* SetAudioSettings = "SetAudioSettings";

        inline constexpr const char* ToggleHudComponent = "ToggleHudComponent";
        inline constexpr const char* ToggleSpawnScreen = "ToggleSpawnScreen";
    }

    namespace Client 
    {
        inline constexpr const char* BrowserCreateResult = "BrowserCreateResult";
    }
}