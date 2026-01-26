#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>

#include "client.hpp"
#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "rendering/view.hpp"
#include "rendering/world_renderer.hpp"

class AudioManager;
class FocusManager;
class ResourceManager;
class NetworkManager;
class Gta;
class CEntity;

// Status of a browser creation
enum class BrowserCreateStatus : int
{
    Success = 0,
    Error_Generic = 1,
    Error_IdAlreadyInUse = 2
};

// Defines how a browser is rendered in the game
enum class RenderMode
{
    Overlay2D,
    WorldObject3D
};

// Holds all data and state related to a single browser instance
struct BrowserInstance
{
    int id;
    std::string url;
    CefRefPtr<BrowserClient> client;
    CefRefPtr<CefBrowser> browser;
    View view;

    RenderMode mode = RenderMode::Overlay2D;
    std::string textureName; // Used only for WorldObject3D mode

    bool visible = true;
    bool controls_chat_input = true;

    explicit BrowserInstance(int id) : id(id), view(id) {}

    void OpenDevTools();
    void Emit(int browserId, const std::string& eventName, const std::vector<Argument>& args);
};

class BrowserManager
{
public:
    BrowserManager(AudioManager& audio, Gta& gta, ResourceManager& resource_manager, NetworkManager& network) : audio_(audio), gta_(gta), resource_manager_(resource_manager), network_(network) {}
    ~BrowserManager()
    {
        Shutdown();
    }

    BrowserManager(const BrowserManager&) = delete;
    BrowserManager& operator=(const BrowserManager&) = delete;
    BrowserManager(BrowserManager&&) = delete;
    BrowserManager& operator=(BrowserManager&&) = delete;

    void SetFocusManager(FocusManager* f)
    {
        focus_ = f;
    }

    void SetEntityResolver(std::function<CEntity*(int)> resolver)
    {
        entity_resolver_ = std::move(resolver);
    }

    bool Initialize();
    void Shutdown();

    // Browser management
    void CreateBrowser(int id, const std::string& url, bool focused, bool controls_chat, float width, float height);
    void CreateWorldBrowser(int id, const std::string& url, const std::string& textureName, float width, float height);
    void DestroyBrowser(int id);
    void ReloadBrowser(int id, bool ignoreCache);

    // 3D World interaction
    void AttachBrowserToObject(int browserId, int objectId);
    void DetachBrowserFromObject(int browserId, int objectId);
    void OnBeforeEntityRender(CEntity* entity);
    void OnAfterEntityRender(CEntity* entity);
    void UpdateAudioSpatialization();

    // Callbacks from BrowserClient
    void OnBrowserCreated(int id, CefRefPtr<CefBrowser> browser);
    void OnBrowserClosed(int id);
    void OnPaint(int id, const void* buffer, int w, int h);

    bool RenderAll();
    LRESULT OnWndProcMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    BrowserInstance* GetBrowserInstance(int id);
    BrowserInstance* GetFocusedBrowser();
    const std::unordered_map<int, std::unique_ptr<BrowserInstance>>& GetAllBrowsers() const
    {
        return browsers_;
    }
    bool IsAnyBrowserVisible() const;
    bool IsAnyBrowserFocused() const;
    cef_cursor_type_t GetCursorType() const
    {
        return cursor_type_;
    }

    void FocusBrowser(int id, bool focus);
    void SetDrawEnabled(bool enabled)
    {
        draw_enabled_ = enabled;
    }
    void SetCursorType(cef_cursor_type_t type)
    {
        cursor_type_ = type;
    }

private:
    void CreateBrowserInternal(
        int id, const std::string& url, bool focused, bool controls_chat, float width, float height);
    void CreateWorldBrowserInternal(int id, const std::string& url, std::string textureName, float width, float height);
    CEntity* GetEntityFromObjectId(int objectId);

private:
    

    bool initialized_ = false;
    DWORD uiThreadId_ = 0;
    std::atomic<bool> is_shutting_down_{false};

    // The single source for which browser has focus. -1 means none.
    int focusedBrowserId_ = -1;

    std::unordered_map<int, std::unique_ptr<BrowserInstance>> browsers_;
    std::unordered_map<int, std::unique_ptr<WorldRenderer>> worldRenderers_;
    std::unordered_map<CEntity*, int> entityToBrowserId_;

    bool draw_enabled_ = true;
    cef_cursor_type_t cursor_type_ = CT_POINTER;

    AudioManager& audio_;
    Gta& gta_;
    ResourceManager& resource_manager_;
    NetworkManager& network_;
    FocusManager* focus_ = nullptr;

    std::function<CEntity*(int)> entity_resolver_{};
};