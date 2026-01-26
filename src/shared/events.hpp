#pragma once

namespace CefEvent
{
    namespace Server
    {
        inline constexpr const char* CreateBrowser = "CreateBrowser";
        inline constexpr const char* CreateWorldBrowser = "CreateWorldBrowser";
        inline constexpr const char* DestroyBrowser = "DestroyBrowser";

        inline constexpr const char* AttachBrowserToObject = "AttachBrowserToObject";
        inline constexpr const char* DetachBrowserFromObject = "DetachBrowserFromObject";
    }

    namespace Client 
    {
        inline constexpr const char* BrowserCreateResult = "BrowserCreateResult";
    }
}