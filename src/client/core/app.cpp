#include "app.hpp"

#include <windows.h>
#include <algorithm>

#include "shared/events.hpp"
#include "shared/packet.hpp"

#include "browser/manager.hpp"
#include "browser/audio.hpp"
#include "browser/focus.hpp"
#include "system/resource_manager.hpp"
#include "network/network_manager.hpp"
#include "ui/hud_manager.hpp"

#include "system/logger.hpp"
#include <samp/components/netgame.hpp>

static void SendEmitToBrowser(BrowserManager& browserManager, int browserId, const std::string& name, const std::vector<Argument>& args)
{
    auto* instance = browserManager.GetBrowserInstance(browserId);
    if (!instance || !instance->browser)
        return;

    CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("emit_event");
    CefRefPtr<CefListValue> list = msg->GetArgumentList();

    list->SetString(0, name);

    for (size_t i = 0; i < args.size(); ++i)
    {
        const auto& arg = args[i];
        const size_t idx = i + 1;

        switch (arg.type)
        {
            case ArgumentType::Integer: 
                list->SetInt(idx, arg.intValue); 
                break;
            case ArgumentType::Float:   
                list->SetDouble(idx, arg.floatValue); 
                break;
            case ArgumentType::Bool:    
                list->SetBool(idx, arg.boolValue); 
                break;
            case ArgumentType::String:  
                list->SetString(idx, arg.stringValue); 
                break;
            default:                    
                list->SetNull(idx); 
                break;
        }
    }

    instance->browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, msg);
}

void App::Initialize()
{
    LOG_INFO("Init omp-cef client app ...");

    network_.SetPacketHandler([this](const NetworkPacket& p) {
        OnPacketReceived(p);
    });

    resources_.Initialize();
}

bool App::ResourcesReady() const
{
    return resources_.GetState() == DownloadState::COMPLETED;
}

void App::FlushPendingIfReady()
{
    if (!ResourcesReady())
        return;

    if (!flushed_once_)
    {
        flushed_once_ = true;
        LOG_INFO("[CEF] Resources completed -> flushing pending creates={}, emits={}.",
            (int)pending_creates_.size(), (int)pending_emits_.size());
    }

    if (!pending_creates_.empty())
    {
        std::vector<PendingCreate> creates;
        creates.swap(pending_creates_);

        for (const auto& c : creates)
        {
            if (c.kind == PendingCreate::Kind::Overlay)
            {
                browser_.CreateBrowser(c.id, c.url, c.focused, c.controls_chat, c.width, c.height);
            }
            else
            {
                browser_.CreateWorldBrowser(c.id, c.url, c.textureName, c.width, c.height);
            }
        }
    }

    if (!pending_emits_.empty())
    {
        std::vector<PendingEmit> remaining;
        remaining.reserve(pending_emits_.size());

        for (const auto& e : pending_emits_)
        {
            auto* browser = browser_.GetBrowserInstance(e.browserId);
            if (browser)
            {
				SendEmitToBrowser(browser_, e.browserId, e.eventName, e.args);  
            }
            else
            {
                remaining.push_back(e);
            }
        }

        pending_emits_ = std::move(remaining);
    }
}

void App::Tick()
{
    if (network_.GetState() == ConnectionState::DISCONNECTED)
    {
        const auto now = ::GetTickCount64();
        const bool can_attempt_connect = (now >= next_connect_attempt_ms_);

        auto* netGame = GetComponent<NetGameComponent>();
        if (can_attempt_connect && netGame)
        {
            const int localPlayerId = netGame->GetLocalPlayerId();
            if (localPlayerId >= 0)
            {
                const std::string host = netGame->GetIp();
                const int gamePort = netGame->GetPort();

                if (!host.empty() && gamePort > 0)
                {
                    const int cefPortInt = gamePort + cef_port_offset_;

                    if (cefPortInt <= 0 || cefPortInt > 65535)
                    {
                        LOG_ERROR("[CEF] Invalid computed CEF port: gamePort={} offset={} => {}", gamePort, cef_port_offset_, cefPortInt);
                        next_connect_attempt_ms_ = now + 2000ULL;
                    }
                    else
                    {
                        const unsigned short cefPort = static_cast<unsigned short>(cefPortInt);

                        const bool endpoint_changed =
                            (!net_endpoint_ready_ || host != net_host_ || cefPort != net_port_);

                        if (endpoint_changed)
                        {
                            net_endpoint_ready_ = false;

                            if (!network_.Initialize(host, cefPort))
                            {
                                LOG_ERROR("[CEF] Failed to init endpoint {}:{} (game {} + {}) - will retry", host.c_str(), (int)cefPort, gamePort, cef_port_offset_);
                                next_connect_attempt_ms_ = now + 2000ULL;
                            }
                            else
                            {
                                net_endpoint_ready_ = true;
                                net_host_ = host;
                                net_port_ = cefPort;

                                LOG_INFO("[CEF] Endpoint {}:{} (derived from {}:{})", net_host_.c_str(), (int)net_port_, host.c_str(), gamePort);
                            }
                        }

                        if (net_endpoint_ready_)
                            network_.Connect(localPlayerId);
                    }
                }
            }
        }
    }

    FlushPendingIfReady();

    focus_.Update();
    browser_.RenderAll();
}

void App::RemovePendingCreate(int id)
{
    pending_creates_.erase(
        std::remove_if(pending_creates_.begin(), pending_creates_.end(),
            [id](const PendingCreate& pending_create) { 
                return pending_create.id == id; 
            }), pending_creates_.end());
}

void App::QueueOrCreateOverlay(int id, const std::string& url, bool focused, bool controls_chat, float width, float height)
{
    if (!ResourcesReady())
    {
        PendingCreate pending_create;
        pending_create.kind = PendingCreate::Kind::Overlay;
        pending_create.id = id;
        pending_create.url = url;
        pending_create.focused = focused;
        pending_create.controls_chat = controls_chat;
        pending_create.width = width;
        pending_create.height = height;

        pending_creates_.push_back(std::move(pending_create));
        LOG_INFO("[CEF] CreateBrowser queued (id={}, url={}) - waiting resources...", id, url.c_str());
        return;
    }

    browser_.CreateBrowser(id, url, focused, controls_chat, width, height);
}

void App::QueueOrCreateWorld(int id, const std::string& url, const std::string& textureName, float width, float height)
{
    if (!ResourcesReady())
    {
        PendingCreate pending_create;
        pending_create.kind = PendingCreate::Kind::World;
        pending_create.id = id;
        pending_create.url = url;
        pending_create.textureName = textureName;
        pending_create.width = width;
        pending_create.height = height;

        pending_creates_.push_back(std::move(pending_create));
        LOG_INFO("[CEF] CreateWorldBrowser queued (id={}, url={}) - waiting resources...", id, url.c_str());
        return;
    }

    browser_.CreateWorldBrowser(id, url, textureName, width, height);
}

void App::OnPacketReceived(const NetworkPacket& packet)
{
    switch (packet.type)
    {
        case PacketType::ServerConfig:
        {
            const auto& cfg = std::get<ServerConfigPacket>(packet.payload);
            resources_.SetMasterKey(cfg.master_resource_key);
            resources_.MarkAsReadyToDownload();
            break;
        }
        case PacketType::FileData:
        {
            resources_.OnFileData(std::get<FileDataPacket>(packet.payload));
            break;
        }
        case PacketType::EmitEvent:
        {
            const auto& event = std::get<EmitEventPacket>(packet.payload);

            if (event.name == CefEvent::Server::CreateBrowser && event.args.size() >= 4) {
                int id = event.args[0].intValue;
                const std::string& url = event.args[1].stringValue;
                bool focused = event.args[2].boolValue;
                bool controls_chat = event.args[3].boolValue;

                QueueOrCreateOverlay(id, url, focused, controls_chat, -1.f, -1.f);
            }
            else if (event.name == CefEvent::Server::CreateWorldBrowser && event.args.size() >= 5) {
                int id = event.args[0].intValue;
                const std::string& url = event.args[1].stringValue;
                const std::string& textureName = event.args[2].stringValue;
                float width = event.args[3].floatValue;
                float height = event.args[4].floatValue;

                QueueOrCreateWorld(id, url, textureName, width, height);
            }
            else if (event.name == CefEvent::Server::DestroyBrowser && event.args.size() >= 1) {
                const int id = event.args[0].intValue;

                RemovePendingCreate(id);

                browser_.DestroyBrowser(id);
            }
            else if (event.name == CefEvent::Server::ReloadBrowser && event.args.size() >= 2)
            {
                int browserId = event.args[0].intValue;
                bool ignoreCache = event.args[1].boolValue;

                browser_.ReloadBrowser(browserId, ignoreCache);
            }
            else if (event.name == CefEvent::Server::FocusBrowser && event.args.size() >= 2) {
                int browserId = event.args[0].intValue;
                bool toggle = event.args[1].boolValue;

                browser_.FocusBrowser(browserId, toggle);
            }
            else if (event.name == CefEvent::Server::AttachBrowserToObject && event.args.size() >= 2)
            {
                int browserId = event.args[0].intValue;
                int objectId = event.args[1].intValue;

                browser_.AttachBrowserToObject(browserId, objectId);
            }
            else if (event.name == CefEvent::Server::DetachBrowserFromObject && event.args.size() >= 2)
            {
                int browserId = event.args[0].intValue;
                int objectId = event.args[1].intValue;

                browser_.DetachBrowserFromObject(browserId, objectId);
            }
            else if (event.name == CefEvent::Server::MuteBrowser && event.args.size() >= 2)
            {
                int browserId = event.args[0].intValue;
                bool muted = event.args[1].boolValue;

                audio_.SetStreamMuted(browserId, muted);
            }
            else if (event.name == CefEvent::Server::SetAudioMode && event.args.size() >= 2)
            {
                int browserId = event.args[0].intValue;
                int modeValue = event.args[1].intValue;

                AudioMode mode = static_cast<AudioMode>(modeValue);
                audio_.SetStreamAudioMode(browserId, mode);
            }
            else if (event.name == CefEvent::Server::SetAudioSettings && event.args.size() >= 3)
            {
                int browserId = event.args[0].intValue;
                float maxDistance = event.args[1].floatValue;
                float refDistance = event.args[2].floatValue;

                audio_.SetStreamAudioSettings(browserId, maxDistance, refDistance);
            }
            else if (event.name == CefEvent::Server::ToggleHudComponent && event.args.size() >= 2)
            {
                int componentId = event.args[0].intValue;
                bool toggle = event.args[1].boolValue;

                hud_.ToggleComponent(static_cast<EHudComponent>(componentId), toggle);
            }
            else if (event.name == CefEvent::Server::ToggleSpawnScreen && event.args.size() >= 1)
            {
                bool toggle = event.args[0].boolValue;

                hud_.SetClassSelectionVisible(toggle);
            }
            break;
        }
        case PacketType::EmitBrowserEvent:
        {
            const auto& event = std::get<EmitEventPacket>(packet.payload);

            const int browserId = event.browserId;
            const std::string& eventName = event.name;

            auto* browser = browser_.GetBrowserInstance(browserId);
            if (browser)
            {
				SendEmitToBrowser(browser_, browserId, eventName, event.args);
                LOG_DEBUG("[CEF] EmitEvent {} sent to browser {} with {} args", eventName.c_str(), browserId, event.args.size());
            }
            else
            {
                PendingEmit pending_emit;
                pending_emit.browserId = browserId;
                pending_emit.eventName = eventName;
                pending_emit.args = event.args;
                pending_emits_.push_back(std::move(pending_emit));

                LOG_INFO("[CEF] EmitEvent '{}' queued for browser {} (not created yet).", eventName.c_str(), browserId);
            }

            break;
        }
        default:
            break;
    }
}

void App::Shutdown()
{
    LOG_INFO("Shutdown omp-cef client app ...");

    browser_.Shutdown();
    audio_.Shutdown();

    LOG_INFO("Good bye! See you soon.");
}