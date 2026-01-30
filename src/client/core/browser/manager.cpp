#include "manager.hpp"

#include <algorithm>
#include <filesystem>

#include <game_sa/CPlayerPed.h>
#include <game_sa/common.h>
#include <include/base/cef_bind.h>
#include <include/wrapper/cef_closure_task.h>
#include <include/wrapper/cef_helpers.h>
#include <windowsx.h>

#include "app.hpp"
#include "audio.hpp"
#include "client.hpp"
#include "rendering/render_manager.hpp"
#include "network/network_manager.hpp"
#include "scheme_handler.hpp"
#include "system/gta.hpp"
#include <samp/components/netgame.hpp>

static void ConfigureBrowserSettings(CefBrowserSettings& settings)
{
    settings.windowless_frame_rate = 60;
    settings.javascript = STATE_ENABLED;
    settings.javascript_access_clipboard = STATE_ENABLED;
    settings.javascript_dom_paste = STATE_ENABLED;
    settings.remote_fonts = STATE_ENABLED;
    settings.webgl = STATE_ENABLED;
    settings.tab_to_links = STATE_DISABLED;
}

static uint32_t GetCefEventFlags()
{
    uint32_t flags = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        flags |= EVENTFLAG_SHIFT_DOWN;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        flags |= EVENTFLAG_CONTROL_DOWN;
    if (GetKeyState(VK_MENU) & 0x8000)
        flags |= EVENTFLAG_ALT_DOWN;
    if (GetKeyState(VK_LBUTTON) & 0x8000)
        flags |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    if (GetKeyState(VK_RBUTTON) & 0x8000)
        flags |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    if (GetKeyState(VK_MBUTTON) & 0x8000)
        flags |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    if (GetKeyState(VK_CAPITAL) & 1)
        flags |= EVENTFLAG_CAPS_LOCK_ON;
    if (GetKeyState(VK_NUMLOCK) & 1)
        flags |= EVENTFLAG_NUM_LOCK_ON;
    return flags;
}

bool BrowserManager::Initialize()
{
    if (initialized_)
        return true;

    LOG_INFO("Initialize CEF Browser manager ...");

    CefMainArgs main_args(GetModuleHandle(nullptr));
    CefRefPtr<DefaultCefApp> app = new DefaultCefApp();
    CefSettings settings;

    wchar_t exe_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);

    auto base = std::filesystem::path(exe_path).parent_path();
    auto cef_dir = base / "cef";
    auto cache_dir = cef_dir / "cache";
    auto profile_dir = cache_dir / "profile";
    auto log_file = cef_dir / "debug.log";

    std::filesystem::create_directories(profile_dir);

    LOG_DEBUG("[CEF] Base dir: {}", base.string().c_str());
    LOG_DEBUG("[CEF] CEF dir: {}", cef_dir.string().c_str());
    LOG_DEBUG("[CEF] Cache dir: {}", cache_dir.string().c_str());
    LOG_DEBUG("[CEF] Profile dir: {}", profile_dir.string().c_str());
    LOG_DEBUG("[CEF] Log file: {}", log_file.string().c_str());

    CefString(&settings.log_file) = log_file.wstring();
    CefString(&settings.root_cache_path) = cache_dir.wstring();
    CefString(&settings.cache_path) = profile_dir.wstring(); 
    CefString(&settings.browser_subprocess_path) = (cef_dir / "renderer.exe").wstring();

    settings.no_sandbox = true;
    settings.log_severity = LOGSEVERITY_DEBUG;
    settings.multi_threaded_message_loop = true;
    settings.windowless_rendering_enabled = true;
    settings.persist_session_cookies = true;

    int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
    if (exit_code >= 0)
    {
        LOG_FATAL("[CEF] CefExecuteProcess exited with code: {}.", exit_code);
        return false;
    }

    if (!CefInitialize(main_args, settings, app.get(), nullptr))
    {
        LOG_FATAL("[CEF] Initialization failed !");
        return false;
    }

    // Register a custom scheme handler for "cef://"
    CefRegisterSchemeHandlerFactory("http", "cef", new LocalSchemeHandlerFactory(resource_manager_));

    initialized_ = true;
    uiThreadId_ = GetCurrentThreadId();

    LOG_INFO("[CEF] Browser manager initialized on UI thread id: {}.", uiThreadId_);
    return true;
}

static bool StartsWithI(const std::string& s, const std::string& prefix)
{
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i)
    {
        if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)prefix[i]))
            return false;
    }
    return true;
}

static std::string UrlDecode(const std::string& in)
{
    std::string out;
    out.reserve(in.size());

    for (size_t i = 0; i < in.size(); ++i)
    {
        const char c = in[i];
        if (c == '%' && i + 2 < in.size())
        {
            auto hex = [](char h) -> int {
                if (h >= '0' && h <= '9') 
                    return h - '0';
                if (h >= 'a' && h <= 'f') 
                    return 10 + (h - 'a');
                if (h >= 'A' && h <= 'F') 
                    return 10 + (h - 'A');

                return -1;
            };

            const int hi = hex(in[i+1]);
            const int lo = hex(in[i+2]);

            if (hi >= 0 && lo >= 0)
            {
                out.push_back((char)((hi << 4) | lo));
                i += 2;
                continue;
            }
        }

        if (c == '+') { 
            out.push_back(' '); 
            continue; 
        }

        out.push_back(c);
    }

    return out;
}

static std::string GetQueryParam(const std::string& url, const std::string& key)
{
    const auto qpos = url.find('?');
    if (qpos == std::string::npos) 
        return {};

    const auto frag = url.find('#', qpos + 1);
    const std::string qs = url.substr(qpos + 1, (frag == std::string::npos ? url.size() : frag) - (qpos + 1));

    size_t start = 0;
    while (start < qs.size())
    {
        const size_t amp = qs.find('&', start);
        const size_t end = (amp == std::string::npos) ? qs.size() : amp;
        const size_t eq = qs.find('=', start);

        std::string k, v;
        if (eq != std::string::npos && eq < end)
        {
            k = qs.substr(start, eq - start);
            v = qs.substr(eq + 1, end - (eq + 1));
        }
        else
        {
            k = qs.substr(start, end - start);
        }

        if (UrlDecode(k) == key)
            return UrlDecode(v);

        start = end + 1;
    }

    return {};
}

static std::string ExtractYouTubeId(const std::string& url)
{
    // Accept:
    // - https://www.youtube.com/embed/VIDEO_ID
    // - https://youtu.be/VIDEO_ID
    // - https://www.youtube.com/watch?v=VIDEO_ID
    // - https://www.youtube.com/shorts/VIDEO_ID
    const std::string u = url;

    auto extractAfter = [&](const std::string& needle) -> std::string {
        auto p = u.find(needle);
        if (p == std::string::npos) 
            return {};

        p += needle.size();
        if (p >= u.size()) 
            return {};

        size_t e = p;
        while (e < u.size())
        {
            const char c = u[e];
            if (c == '?' || c == '&' || c == '#' || c == '/') 
                break;

            ++e;
        }

        return u.substr(p, e - p);
    };

    if (auto id = extractAfter("youtube.com/embed/"); !id.empty()) return id;
    if (auto id = extractAfter("youtube-nocookie.com/embed/"); !id.empty()) return id;
    if (auto id = extractAfter("youtu.be/"); !id.empty()) return id;
    if (auto id = extractAfter("youtube.com/shorts/"); !id.empty()) return id;

    if (StartsWithI(u, "http") && u.find("youtube.com/") != std::string::npos)
    {
        auto id = GetQueryParam(u, "v");
        if (!id.empty()) return id;
    }

    return {};
}

static std::string NormalizeUrlForEmbeds(const std::string& url)
{
    // YouTube -> internal wrapper
    if (url.find("youtube.com") != std::string::npos || url.find("youtu.be") != std::string::npos || url.find("youtube-nocookie.com") != std::string::npos)
    {
        const std::string id = ExtractYouTubeId(url);
        if (!id.empty())
        {
            const std::string autoplay = GetQueryParam(url, "autoplay").empty() ? "1" : GetQueryParam(url, "autoplay");
            const std::string mute = GetQueryParam(url, "mute").empty() ? "1" : GetQueryParam(url, "mute");
            const std::string controls = GetQueryParam(url, "controls").empty() ? "0" : GetQueryParam(url, "controls");
            const std::string rel = GetQueryParam(url, "rel").empty() ? "0" : GetQueryParam(url, "rel");
            const std::string start = GetQueryParam(url, "start");
            const std::string end = GetQueryParam(url, "end");

            std::string internal = "http://cef/__internal/youtube.html?v=" + id +
                "&autoplay=" + autoplay +
                "&mute=" + mute +
                "&controls=" + controls +
                "&rel=" + rel;

            if (!start.empty()) internal += "&start=" + start;
            if (!end.empty()) internal += "&end=" + end;

            return internal;
        }
    }

    // Twitch -> internal wrapper
    if (url.find("twitch.tv") != std::string::npos)
    {
        // If already a player.twitch.tv URL, we keep query but strip "parent".
        const auto qpos = url.find('?');
        if (qpos != std::string::npos)
        {
            std::string qs = url.substr(qpos + 1);

            std::string out;
            size_t start = 0;
            while (start < qs.size())
            {
                size_t amp = qs.find('&', start);
                size_t end = (amp == std::string::npos) ? qs.size() : amp;
                std::string kv = qs.substr(start, end - start);

                std::string k = kv;
                auto eq = kv.find('=');
                if (eq != std::string::npos) k = kv.substr(0, eq);
                k = UrlDecode(k);

                if (k != "parent")
                {
                    if (!out.empty()) out.push_back('&');
                    out += kv;
                }

                start = end + 1;
            }

            if (out.find("channel=") != std::string::npos || out.find("video=") != std::string::npos || out.find("clip=") != std::string::npos)
            {
                return std::string("http://cef/__internal/twitch.html?") + out;
            }
        }

        // https://www.twitch.tv/<channel> -> internal wrapper
        const auto slash = url.rfind('/');
        if (slash != std::string::npos && slash + 1 < url.size())
        {
            std::string tail = url.substr(slash + 1);
            const auto q = tail.find_first_of("?#");
            if (q != std::string::npos) tail = tail.substr(0, q);

            if (!tail.empty() && tail != "videos" && tail != "clips")
            {
                return std::string("http://cef/__internal/twitch.html?channel=") + tail;
            }
        }
    }

    return url;
}

void BrowserManager::Shutdown()
{
    if (!initialized_ || is_shutting_down_.exchange(true))
        return;

    LOG_DEBUG("Shutting down CEF Browser manager...");

    browsers_.clear();
    worldRenderers_.clear();
    entityToBrowserId_.clear();

    CefShutdown();
    initialized_ = false;

    LOG_INFO("CEF Browser manager shut down successfully.");
}

void BrowserManager::CreateBrowser(
    int id, const std::string& url, bool focused, bool controls_chat, float width, float height)
{
    LOG_DEBUG("[CEF] CreateBrowser called with ID={}, url={}", id, url);
    
    if (browsers_.count(id))
    {
        LOG_ERROR("[CEF] CreateBrowser failed: Browser with ID {} already exists.", id);
        LOG_ERROR("[CEF] Existing browser mode: {}", (int)browsers_[id]->mode);
        return;
    }
    CreateBrowserInternal(id, url, focused, controls_chat, width, height);
}

void BrowserManager::CreateWorldBrowser(
    int id, const std::string& url, const std::string& textureName, float width, float height)
{
    LOG_DEBUG("[CEF] CreateWorldBrowser called with ID={}, url={}", id, url);
    
    const std::string normalized_url = NormalizeUrlForEmbeds(url);
    if (normalized_url != url)
        LOG_DEBUG("[CEF] Normalized URL -> {}", normalized_url);

    if (browsers_.count(id))
    {
        LOG_ERROR("[CEF] CreateWorldBrowser failed: Browser with ID {} already exists.", id);
        LOG_ERROR("[CEF] Existing browser mode: {}", (int)browsers_[id]->mode);
        return;
    }

    CreateWorldBrowserInternal(id, normalized_url, textureName, width, height);
}

void BrowserManager::CreateBrowserInternal(
    int id, const std::string& url, bool focused, bool controls_chat, float width, float height)
{
    if (CefCurrentlyOn(TID_UI) == false)
    {
        CefPostTask(TID_UI,
                    base::BindOnce(&BrowserManager::CreateBrowserInternal,
                                   base::Unretained(this),
                                   id,
                                   url,
                                   focused,
                                   controls_chat,
                                   width,
                                   height));
        return;
    }

    if (browsers_.count(id))
    {
        LOG_ERROR("[CEF] CreateBrowserInternal: Browser with ID {} already exists (race condition?).", id);
        return;
    }

    auto instance = std::make_unique<BrowserInstance>(id);
    instance->mode = RenderMode::Overlay2D;
    instance->url = url;
    instance->client = BrowserClient::Create(id, *this, audio_, focus_, network_);
    instance->controls_chat_input = controls_chat;

    if (auto* device = RenderManager::Instance().GetDevice())
    {
        instance->view.Initialize(device);

        int browser_width = (int)width;
        int browser_height = (int)height;
        if (browser_width <= 0 || browser_height <= 0)
        {
            float screen_w, screen_h;
            if (RenderManager::Instance().GetScreenSize(screen_w, screen_h))
            {
                browser_width = (int)screen_w;
                browser_height = (int)screen_h;
            }
            else
            {
                browser_width = 1280;
                browser_height = 720;
            }
        }
        browser_width = std::clamp(browser_width, 1, 2560);
        browser_height = std::clamp(browser_height, 1, 1440);
        instance->view.Create(browser_width, browser_height);
    }

    browsers_[id] = std::move(instance);
    if (focused)
    {
        FocusBrowser(id, true);
    }

    CefWindowInfo windowInfo;
    windowInfo.SetAsWindowless(gta_.GetHwnd());
    CefBrowserSettings bs;
    ConfigureBrowserSettings(bs);
    CefBrowserHost::CreateBrowser(
        windowInfo, browsers_[id]->client, url, bs, nullptr, CefRequestContext::GetGlobalContext());
}

void BrowserManager::CreateWorldBrowserInternal(
    int id, const std::string& url, std::string textureName, float width, float height)
{
    if (CefCurrentlyOn(TID_UI) == false)
    {
        CefPostTask(TID_UI,
                    base::BindOnce(&BrowserManager::CreateWorldBrowserInternal,
                                   base::Unretained(this),
                                   id,
                                   url,
                                   textureName,
                                   width,
                                   height));
        return;
    }

    if (browsers_.count(id))
    {
        LOG_ERROR("[CEF] CreateWorldBrowserInternal: Browser with ID {} already exists (race condition?).", id);
        return;
    }

    auto instance = std::make_unique<BrowserInstance>(id);
    instance->mode = RenderMode::WorldObject3D;
    instance->url = url;
    instance->textureName = textureName;
    instance->client = BrowserClient::Create(id, *this, audio_, focus_, network_);

    const int browser_width = std::clamp((int)width, 1, 1024);
    const int browser_height = std::clamp((int)height, 1, 1024);

    worldRenderers_[id] = std::make_unique<WorldRenderer>(textureName, (float)browser_width, (float)browser_height);

    if (auto* device = RenderManager::Instance().GetDevice())
    {
        instance->view.Initialize(device);
        instance->view.Create(browser_width, browser_height);
    }

    browsers_[id] = std::move(instance);

    // Prepare audio stream but keep it muted until attached to a world entity
    audio_.EnsureStream(id);
    audio_.SetStreamMuted(id, true);

    CefWindowInfo windowInfo;
    windowInfo.SetAsWindowless(gta_.GetHwnd());
    CefBrowserSettings bs;
    ConfigureBrowserSettings(bs);
    CefBrowserHost::CreateBrowser(
        windowInfo, browsers_[id]->client, url, bs, nullptr, CefRequestContext::GetGlobalContext());
}

void BrowserManager::DestroyBrowser(int id)
{
    auto it = browsers_.find(id);
    if (it == browsers_.end())
        return;

    auto& instance = it->second;
    if (instance->browser && instance->browser->GetHost())
    {
        instance->view.SetFocused(false);
        instance->browser->GetHost()->CloseBrowser(true);
        instance->browser = nullptr;
    }

    worldRenderers_.erase(id);

    for (auto eit = entityToBrowserId_.begin(); eit != entityToBrowserId_.end();)
    {
        if (eit->second == id)
        {
            OnAfterEntityRender(eit->first);
            eit = entityToBrowserId_.erase(eit);
        }
        else
        {
            ++eit;
        }
    }

    browsers_.erase(it);
    LOG_DEBUG("[CEF] Browser ID {} destroyed and removed from map.", id);
}

void BrowserManager::ReloadBrowser(int id, bool ignoreCache)
{
    if (CefCurrentlyOn(TID_UI) == false)
    {
        CefPostTask(TID_UI, base::BindOnce(&BrowserManager::ReloadBrowser, base::Unretained(this), id, ignoreCache));
        return;
    }

    if (auto* inst = GetBrowserInstance(id))
    {
        if (inst->browser)
        {
            ignoreCache ? inst->browser->ReloadIgnoreCache() : inst->browser->Reload();
            LOG_DEBUG("[CEF] Reloading browser with ID {}.", id);
        }
    }
}

void BrowserManager::AttachBrowserToObject(int browserId, int objectId)
{
    if (!browsers_.count(browserId))
    {
        LOG_WARN("[CEF] AttachBrowserToObject: Browser ID {} does not exist.", browserId);
        return;
    }
    CEntity* nativeEntity = GetEntityFromObjectId(objectId);
    if (nativeEntity)
    {
        entityToBrowserId_[nativeEntity] = browserId;
        audio_.SetStreamMuted(browserId, false); // unmute when attached

        LOG_DEBUG(
            "[CEF] Browser {} attached to object {} (Entity: {})", browserId, objectId, (const void*)nativeEntity);
    }
    else
    {
        LOG_WARN("[CEF] AttachBrowserToObject: Could not find entity for object ID {}.", objectId);
    }
}

void BrowserManager::DetachBrowserFromObject(int browserId, int objectId)
{
    CEntity* nativeEntity = GetEntityFromObjectId(objectId);
    if (nativeEntity)
    {
        auto it = entityToBrowserId_.find(nativeEntity);
        if (it != entityToBrowserId_.end() && it->second == browserId)
        {
            OnAfterEntityRender(nativeEntity); // ensure texture restored
            entityToBrowserId_.erase(it);
            audio_.SetStreamMuted(browserId, true); // mute when detached
            LOG_DEBUG("[CEF] Browser {} detached from object {} (Entity: {})",
                      browserId,
                      objectId,
                      (const void*)nativeEntity);
        }
    }
    else
    {
        LOG_WARN("[CEF] DetachBrowserFromObject: Could not find entity for object ID {}.", objectId);
    }
}

CEntity* BrowserManager::GetEntityFromObjectId(int objectId)
{
    auto* netGame = GetComponent<NetGameComponent>();
    if (!netGame)
        return nullptr;

    return netGame->GetEntityFromObjectId(objectId);
}

void BrowserManager::OnBrowserCreated(int id, CefRefPtr<CefBrowser> browser)
{
    auto it = browsers_.find(id);
    if (it != browsers_.end())
    {
        it->second->browser = browser;
        if (auto host = browser->GetHost())
        {
            host->WasResized();
            host->Invalidate(PET_VIEW);
            if (focusedBrowserId_ == id)
            {
                it->second->view.SetFocused(true);
                browser->GetHost()->SetFocus(true);
            }
        }

        network_.SendBrowserCreateResult(id, true, static_cast<int>(BrowserCreateStatus::Success), "Successfully created");
    }
}

void BrowserManager::OnBrowserClosed(int id)
{
    browsers_.erase(id);
}

void BrowserManager::OnPaint(int id, const void* buffer, int w, int h)
{
    auto* instance = GetBrowserInstance(id);
    if (!instance || !buffer)
        return;

    if (instance->mode == RenderMode::WorldObject3D)
    {
        auto it = worldRenderers_.find(id);
        if (it != worldRenderers_.end())
            it->second->OnPaint(buffer, w, h);
    }
    else
    {
        cef_rect_t rect{0, 0, w, h};
        instance->view.UpdateTexture((const uint8_t*)buffer, &rect, 1);
    }
}

bool BrowserManager::RenderAll()
{
    if (!draw_enabled_)
        return false;

    UpdateAudioSpatialization();

    bool any_visible = false;
    for (auto& [id, browser] : browsers_)
    {
        if (browser->visible && browser->mode == RenderMode::Overlay2D)
        {
            browser->view.Draw();
            any_visible = true;
        }
    }
    return any_visible;
}

void BrowserManager::OnBeforeEntityRender(CEntity* entity)
{
    auto it = entityToBrowserId_.find(entity);
    if (it == entityToBrowserId_.end())
        return;
    int browserId = it->second;
    auto wrIt = worldRenderers_.find(browserId);
    if (wrIt != worldRenderers_.end())
    {
        wrIt->second->SwapTexture(entity);
    }
}

void BrowserManager::OnAfterEntityRender(CEntity* entity)
{
    auto it = entityToBrowserId_.find(entity);
    if (it == entityToBrowserId_.end())
        return;
    int browserId = it->second;
    auto wrIt = worldRenderers_.find(browserId);
    if (wrIt != worldRenderers_.end())
    {
        wrIt->second->RestoreTexture();
    }
}

BrowserInstance* BrowserManager::GetBrowserInstance(int id)
{
    auto it = browsers_.find(id);
    return it != browsers_.end() ? it->second.get() : nullptr;
}

bool BrowserManager::IsAnyBrowserVisible() const
{
    for (const auto& [id, browser] : browsers_)
    {
        if (browser && browser->visible)
            return true;
    }
    return false;
}

bool BrowserManager::IsAnyBrowserFocused() const
{
    for (const auto& [id, instance] : browsers_)
    {
        if (instance && instance->view.IsFocused())
            return true;
    }
    return false;
}

void BrowserManager::FocusBrowser(int browserId, bool focus)
{
    if (CefCurrentlyOn(TID_UI) == false)
    {
        CefPostTask(TID_UI, base::BindOnce(&BrowserManager::FocusBrowser, base::Unretained(this), browserId, focus));
        return;
    }

    auto* instance_to_change = GetBrowserInstance(browserId);
    if (!instance_to_change)
    {
        LOG_WARN("[CEF] FocusBrowser: Could not find browser with ID {}.", browserId);
        return;
    }

    if (focus)
    {
        if (focusedBrowserId_ == browserId)
            return;
        if (focusedBrowserId_ != -1)
        {
            if (auto* old = GetBrowserInstance(focusedBrowserId_))
            {
                old->view.SetFocused(false);
                if (old->browser && old->browser->GetHost())
                    old->browser->GetHost()->SetFocus(false);
            }
        }
        focusedBrowserId_ = browserId;
        instance_to_change->view.SetFocused(true);
        if (instance_to_change->browser && instance_to_change->browser->GetHost())
            instance_to_change->browser->GetHost()->SetFocus(true);
        LOG_DEBUG("[CEF] Browser {} gained focus.", browserId);
    }
    else
    {
        if (focusedBrowserId_ != browserId)
            return;
        focusedBrowserId_ = -1;
        instance_to_change->view.SetFocused(false);
        if (instance_to_change->browser && instance_to_change->browser->GetHost())
            instance_to_change->browser->GetHost()->SetFocus(false);
        LOG_DEBUG("[CEF] Browser {} lost focus.", browserId);
    }
}

BrowserInstance* BrowserManager::GetFocusedBrowser()
{
    return focusedBrowserId_ == -1 ? nullptr : GetBrowserInstance(focusedBrowserId_);
}

void BrowserManager::UpdateAudioSpatialization()
{
    // Update listener from player camera
    if (CPlayerPed* player = FindPlayerPed(-1))
    {
        if (player->m_matrix)
        {
            const auto& pos = player->m_matrix->pos;
            const auto& at = player->m_matrix->at;
            const auto& up = player->m_matrix->up;
            audio_.UpdateListenerPosition(pos.x, pos.y, pos.z, at.x, at.y, at.z, up.x, up.y, up.z);
        }
    }

    // Update 3D positions for world browsers bound to entities
    for (const auto& [entity, browserId] : entityToBrowserId_)
    {
        if (entity && entity->m_matrix)
        {
            const auto& pos = entity->m_matrix->pos;
            audio_.UpdateSourcePosition(browserId, pos.x, pos.y, pos.z);
        }
    }
}

void BrowserInstance::OpenDevTools()
{
    if (!browser)
    {
        LOG_WARN("[CEF] OpenDevTools: browser is null.");
        return;
    }

    CefWindowInfo windowInfo;
    windowInfo.SetAsPopup(nullptr, "DevTools");

    CefBrowserSettings settings;
    browser->GetHost()->ShowDevTools(windowInfo, client, settings, CefPoint());

    LOG_DEBUG("[CEF] DevTools opened for browser ID: %d", id);
}

LRESULT BrowserManager::OnWndProcMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* focused_inst = GetFocusedBrowser();
    if (!focused_inst || !focused_inst->browser)
        return false;

    if (msg == WM_KEYDOWN && (GetAsyncKeyState(VK_CONTROL) & 0x8000) && wParam == 'L')
    {
        focused_inst->OpenDevTools();
        return true;
    }

    auto host = focused_inst->browser->GetHost();
    if (!host)
    {
        return false;
    }

    switch (msg)
    {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MOUSEWHEEL:
        {
            CefMouseEvent evt;
            evt.x = GET_X_LPARAM(lParam);
            evt.y = GET_Y_LPARAM(lParam);
            evt.modifiers = GetCefEventFlags();

            if (msg == WM_MOUSEMOVE)
                host->SendMouseMoveEvent(evt, false);
            else if (msg == WM_LBUTTONDOWN)
                host->SendMouseClickEvent(evt, MBT_LEFT, false, 1);
            else if (msg == WM_LBUTTONUP)
                host->SendMouseClickEvent(evt, MBT_LEFT, true, 1);
            else if (msg == WM_RBUTTONDOWN)
                host->SendMouseClickEvent(evt, MBT_RIGHT, false, 1);
            else if (msg == WM_RBUTTONUP)
                host->SendMouseClickEvent(evt, MBT_RIGHT, true, 1);
            else if (msg == WM_MOUSEWHEEL)
                host->SendMouseWheelEvent(evt, 0, GET_WHEEL_DELTA_WPARAM(wParam));

            return true;
        }
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR:
        {
            CefKeyEvent evt;
            evt.windows_key_code = static_cast<int>(wParam);
            evt.native_key_code = static_cast<int>(lParam);
            evt.modifiers = GetCefEventFlags();

            if (msg == WM_KEYDOWN)
                evt.type = KEYEVENT_RAWKEYDOWN;
            else if (msg == WM_KEYUP)
                evt.type = KEYEVENT_KEYUP;
            else
                evt.type = KEYEVENT_CHAR;

            host->SendKeyEvent(evt);

            if (msg != WM_KEYUP)
            {
                if (wParam == 'T' || wParam == 'F' || wParam == VK_RETURN || wParam == VK_ESCAPE ||
                    (wParam >= 'A' && wParam <= 'Z') || (wParam >= VK_LEFT && wParam <= VK_DOWN))
                {
                    return true;
                }
            }

            return true;
        }
    }

    return false;
}