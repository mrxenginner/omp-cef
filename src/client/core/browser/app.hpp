#pragma once

#include "include/cef_app.h"

class DefaultCefApp : public CefApp {
public:
    DefaultCefApp() = default;

    void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override 
    {
        command_line->AppendSwitchWithValue("force-device-scale-factor", "1.0");
        command_line->AppendSwitch("high-dpi-support");

        command_line->AppendSwitch("use-gl=swiftshader-webgl");
        command_line->AppendSwitch("enable-unsafe-swiftshader");
        command_line->AppendSwitch("disable-gpu-vsync");
        
        command_line->AppendSwitch("ignore-gpu-blocklist");
        
        command_line->AppendSwitch("enable-font-antialiasing");
        command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");

        command_line->AppendSwitch("disable-web-security");
        command_line->AppendSwitch("disable-site-isolation-trials");
        
        command_line->AppendSwitch("disable-features=IsolateOrigins,site-per-process,CrossOriginOpenerPolicy");
    }

private:
    IMPLEMENT_REFCOUNTING(DefaultCefApp);
};